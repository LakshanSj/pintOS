#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include <stdint.h>

struct lock filesys_lock;

static void syscall_handler (struct intr_frame *);

/* helper function prototype*/
static void check_valid_ptr(const void *vaddr);
static void check_valid_buffer(const void *buffer, unsigned size);
static void check_valid_string (const char *str);
static void check_writable_buffer (void *buffer, unsigned size);
void sys_exit(int status);

void
syscall_init (void)
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void
sys_exit (int status) 
{
  struct thread *cur = thread_current ();
  printf ("%s: exit(%d)\n", cur->name, status);
  
  if (cur->shared_entry != NULL)
    cur->shared_entry->exit_status = status; // Save for parent
  
  thread_exit ();
}

static void
check_valid_ptr(const void *vaddr)
{
  if (vaddr == NULL || !is_user_vaddr (vaddr) || 
      pagedir_get_page (thread_current ()->pagedir, vaddr) == NULL)
    {
      sys_exit (-1);
    }
}

static void
check_valid_buffer (const void *buffer, unsigned size)
{
  for (unsigned i = 0; i < size; i++)
    {
      check_valid_ptr ((const char *) buffer + i);
    }
}

static void
check_valid_string (const char *str)
{
  for (;;)
    {
      check_valid_ptr (str);

      if (*str == '\0')
        return;

      str++;
    }
}

static void
check_writable_buffer (void *buffer, unsigned size)
{
  if (size == 0)
    return;

  uintptr_t start = (uintptr_t) buffer;
  uintptr_t user_top = (uintptr_t) PHYS_BASE;

  /* Reject NULL, kernel addresses, and ranges crossing PHYS_BASE.
     Subtraction avoids overflow from computing start + size. */
  if (start == 0 || start >= user_top || size > user_top - start)
    sys_exit (-1);

  uintptr_t last = start + size - 1;
  uintptr_t page = (uintptr_t) pg_round_down (buffer);

  /* Check every page touched by the destination buffer. */
  for (;;)
    {
      if (!pagedir_is_writable (thread_current ()->pagedir,
                                (const void *) page))
        sys_exit (-1);

      if (last - page < PGSIZE)
        break;

      page += PGSIZE;
    }
}

static void
syscall_handler (struct intr_frame *f) 
{
  check_valid_buffer (f->esp, sizeof (int));
  
  /* Treat the stack pointer as an array of 4-byte integers */
  int *args = f->esp; 
  int syscall_number = args[0]; 

  if (syscall_number == SYS_EXIT) 
    {
      check_valid_ptr (&args[1]);
      int status = args[1];
      sys_exit (status);
    }
  else if (syscall_number == SYS_WRITE)
    {
      /* Validate all three complete arguments before reading them. */
      check_valid_buffer (&args[1], 3 * sizeof (int));

      int fd = args[1];
      const void *buffer = (const void *) args[2];
      unsigned size = (unsigned) args[3];

      check_valid_buffer (buffer, size);

      struct thread *cur = thread_current ();

      if (fd == 1)
        {
          /* Write to the console. */
          putbuf (buffer, size);
          f->eax = size;
        }
      else if (fd >= 2 && fd < 128 && cur->fd_table[fd] != NULL)
        {
          /* Write to an opened file. */
          lock_acquire (&filesys_lock);
          f->eax = file_write (cur->fd_table[fd], buffer, size);
          lock_release (&filesys_lock);
        }
      else
        {
          /* Invalid descriptor, including standard input. */
          f->eax = -1;
        }
    }
  else if (syscall_number == SYS_EXEC) 
    {
      check_valid_ptr (&args[1]);
      const char *cmd_line = (const char *) args[1];
      
      check_valid_ptr (cmd_line); 
      f->eax = process_execute (cmd_line);
    }
  else if (syscall_number == SYS_WAIT) 
    {
      check_valid_ptr (&args[1]);
      tid_t pid = args[1];
      
      f->eax = process_wait (pid);
    }

  else if (syscall_number == SYS_CREATE)
  {
    /* Validate both complete arguments before reading them. */
    check_valid_buffer (&args[1], 2 * sizeof (int));

    const char *file_name = (const char *) args[1];
    unsigned initial_size = (unsigned) args[2];

    /* Validate before acquiring the file system lock. */
    check_valid_string (file_name);

    lock_acquire (&filesys_lock);
    f->eax = filesys_create (file_name, initial_size);
    lock_release (&filesys_lock);
  }
  else if (syscall_number == SYS_OPEN)
  {
    check_valid_buffer (&args[1], sizeof (int));

    const char *file_name = (const char *) args[1];
    check_valid_string (file_name);

    struct thread *cur = thread_current ();
    int fd;

    /* Find an available descriptor. */
    for (fd = 2; fd < 128; fd++)
      {
        if (cur->fd_table[fd] == NULL)
          break;
      }

    f->eax = -1;

    if (fd < 128)
      {
        lock_acquire (&filesys_lock);
        struct file *opened_file = filesys_open (file_name);
        lock_release (&filesys_lock);

        if (opened_file != NULL)
          {
            cur->fd_table[fd] = opened_file;
            f->eax = fd;
          }
      }
  }
  else if (syscall_number == SYS_CLOSE)
  {
    check_valid_buffer (&args[1], sizeof (int));

    int fd = args[1];
    struct thread *cur = thread_current ();

    if (fd >= 2 && fd < 128 && cur->fd_table[fd] != NULL)
      {
        struct file *opened_file = cur->fd_table[fd];
        cur->fd_table[fd] = NULL;

        lock_acquire (&filesys_lock);
        file_close (opened_file);
        lock_release (&filesys_lock);
      }
  }
  else if (syscall_number == SYS_FILESIZE)
    {
      check_valid_buffer (&args[1], sizeof (int));

      int fd = args[1];
      struct thread *cur = thread_current ();

      if (fd >= 2 && fd < 128 && cur->fd_table[fd] != NULL)
        {
          lock_acquire (&filesys_lock);
          f->eax = file_length (cur->fd_table[fd]);
          lock_release (&filesys_lock);
        }
      else
        {
          f->eax = -1;
        }
    }

  else if (syscall_number == SYS_READ)
    {
      check_valid_buffer (&args[1], 3 * sizeof (int));

      int fd = args[1];
      void *buffer = (void *) args[2];
      unsigned size = (unsigned) args[3];

      check_writable_buffer (buffer, size);

      struct thread *cur = thread_current ();

      if (fd == 0)
        {
          /* Read from standard input. */
          unsigned char *bytes = buffer;

          for (unsigned i = 0; i < size; i++)
            bytes[i] = input_getc ();

          f->eax = size;
        }
      else if (fd >= 2 && fd < 128 && cur->fd_table[fd] != NULL)
        {
          lock_acquire (&filesys_lock);
          f->eax = file_read (cur->fd_table[fd], buffer, size);
          lock_release (&filesys_lock);
        }
      else
        {
          /* Invalid descriptor, including standard output. */
          f->eax = -1;
        }
    }
  else 
    {
      sys_exit (-1);
    }
}

