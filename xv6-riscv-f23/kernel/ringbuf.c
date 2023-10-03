#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "ringbuf.h"

struct spinlock ringbuf_lock;
struct ringbuf ringbufs[MAX_RINGBUFS];
static int is_initialized = 0;

void lazy_init_ringbufs() {
  if (!is_initialized) {
    initlock(&ringbuf_lock, "ringbuf_lock");
    for(int i = 0; i < MAX_RINGBUFS; i++) {
      ringbufs[i].refcount = 0;
    }
    is_initialized = 1;
  }
}

int map_ringbuf_to_process(int index, void **addr) {
  
  struct proc *p = myproc();  // Get the current process
  uint64 va_start = p->sz;

  // Update p->sz to reserve space for the ring buffer
  p->sz += RINGBUF_SIZE;   // Replace with an actual logic to find a suitable starting virtual address

  // Map the ring buffer pages
  for (int i = 0; i < RINGBUF_SIZE; ++i) {
    if (mappages(p->pagetable, va_start + i * PGSIZE, PGSIZE, (uint64)ringbufs[index].buf[i], PTE_W | PTE_U) == -1) {
      // Error handling: unmap any pages that were mapped successfully
      for (int j = 0; j < i; ++j) {
        uvmunmap(p->pagetable, va_start + j * PGSIZE, 1, 0);
      }
      return -1;
    }
  }

  // Map the bookkeeping page
  if (mappages(p->pagetable, va_start + RINGBUF_SIZE * PGSIZE, PGSIZE, (uint64)ringbufs[index].book, PTE_W | PTE_U) == -1) {
    // Error handling: unmap any pages that were mapped successfully
    for (int i = 0; i < RINGBUF_SIZE; ++i) {
      uvmunmap(p->pagetable, va_start + i * PGSIZE, 1, 0);
    }
    return -1;
  }

  // If we reach here, all pages were successfully mapped
  *addr = (void *)va_start;
  return 0;
}


int create_ringbuf(const char *name, void **addr) {
  // Your logic for creating a new ring buffer
  // Allocate memory, set refcount, etc.
  // Map the ring buffer into the address space of the calling process
  // Return 0 on success, -1 on failure
  // Find an empty slot for the new ring buffer
  int index = -1;
  for (int i = 0; i < MAX_RINGBUFS; ++i) {
    if (ringbufs[i].refcount == 0) {
      index = i;
      break;
    }
  }

  // If no empty slot is found, return an error
  if (index == -1) {
    return -1;
  }

  // Initialize the new ring buffer
  ringbufs[index].refcount = 1;
  strncpy(ringbufs[index].name, name, sizeof(ringbufs[index].name) - 1);

  // Allocate memory for the buffer and the bookkeeping page
  for (int i = 0; i < RINGBUF_SIZE; ++i) {
    ringbufs[index].buf[i] = kalloc();
    if (ringbufs[index].buf[i] == 0) {
      // Handle memory allocation failure
      printf("Memory allocation failed for bookkeeping.\n");
      return -1;
    }
  }
  ringbufs[index].book = kalloc();
  if (ringbufs[index].book == 0) {
    // Handle memory allocation failure
    printf("Memory allocation failed for bookkeeping.\n");
    return -1;
  }

  // Initialize the bookkeeping page
  struct book *b = (struct book *)ringbufs[index].book;
  b->read_done = 0;
  b->write_done = 0;

  // Map the buffer and the bookkeeping page into the calling process's address space
  // Assuming you have a function to do this, called map_ringbuf_to_process
  if (map_ringbuf_to_process(index, addr) != 0) {
    // Handle mapping failure
    release(&ringbuf_lock);
    return -1;
  }

  return 0;
}

int close_ringbuf(const char *name) {
  // Your logic for closing an existing ring buffer
  // Decrement refcount, free memory if refcount reaches 0
  // Return 0 on success, -1 on failure
  
  acquire(&ringbuf_lock);  // Acquire the lock to protect the ringbufs array

  // Search for the ring buffer by name
  int index = -1;
  for (int i = 0; i < MAX_RINGBUFS; i++) {
    if (ringbufs[i].refcount > 0 && strncmp(ringbufs[i].name, name, 16) == 0) {
      index = i;
      break;
    }
  }

  // If the ring buffer is not found, return an error
  if (index == -1) {
    release(&ringbuf_lock);
    return -1;
  }

  // Decrement the reference count
  ringbufs[index].refcount--;

  // If the reference count reaches zero, free the resources
  if (ringbufs[index].refcount == 0) {
    for (int i = 0; i < RINGBUF_SIZE; i++) {
      kfree(ringbufs[index].buf[i]);
    }
    kfree(ringbufs[index].book);
    memset(&ringbufs[index], 0, sizeof(struct ringbuf));  // Reset the ringbuf struct
  }

  release(&ringbuf_lock);  // Release the lock
  return 0;
}