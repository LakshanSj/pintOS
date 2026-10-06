#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

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
sys_exit(int status)
{
  printf("%s: exit(%d)\n", thread_name(), status);
  thread_exit();
}

static void
check_valid_ptr(const void *vaddr)
{
  if (vaddr == NULL || !is_user_vaddr(vaddr))
  {
    sys_exit(-1);
  }
}

static void
check_valid_buffer(const void *buffer, unsigned size)
{
  for (unsigned i = 0; i < size; i++)
  {
    check_valid_ptr((const char *)buffer + i);
  }
} 

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  check_valid_ptr(f->esp); // validate stack pointer
  int syscall_number = *(int *)(f->esp); // read system call number from stack

  if (syscall_number == SYS_EXIT) 
  {
    check_valid_ptr((int *)(f->esp + 1)); // validate argument pointer
    int status = *(int *)(f->esp + 1); // read exit status from stack
    sys_exit(status);
  }
  else if (syscall_number == SYS_WRITE) 
  {
    check_valid_ptr((int *)(f->esp + 1)); // validate file descriptor pointer
    check_valid_ptr((int *)(f->esp + 2)); // validate buffer pointer
    check_valid_ptr((int *)(f->esp + 3)); // validate size pointer

    int fd = *(int *)(f->esp + 1); // read file descriptor from stack
    const void *buffer = *(const char *)*((int *)f->esp + 2); // read buffer pointer from stack
    unsigned size = *(unsigned *)(f->esp + 3); // read size from stack

    check_valid_buffer(buffer, size); // validate buffer and size

    if (fd == 1) 
    {
      putbuf(buffer, size); // write to standard output
      f->eax = size; // return number of bytes written
    } 
    else 
    {
      f->eax = -1; // invalid file descriptor
    }
  } 
  else 
  {
    sys_exit(-1); // unknown system call number
  }
}
