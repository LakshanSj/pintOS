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

static void syscall_handler (struct intr_frame *);

/* helper function prototype*/
static void check_valid_ptr(const void *vaddr);
static void check_valid_buffer(const void *buffer, unsigned size);
void sys_exit(int status);

void
syscall_init (void) 
{
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
syscall_handler (struct intr_frame *f) 
{
  check_valid_ptr (f->esp);
  
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
      check_valid_ptr (&args[1]);
      check_valid_ptr (&args[2]);
      check_valid_ptr (&args[3]);

      int fd = args[1];
      const void *buffer = (const void *) args[2];
      unsigned size = (unsigned) args[3];

      check_valid_buffer (buffer, size);

      if (fd == 1) 
        {
          putbuf (buffer, size);
          f->eax = size;
        } 
      else 
        {
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
  else 
    {
      sys_exit (-1);
    }
}
