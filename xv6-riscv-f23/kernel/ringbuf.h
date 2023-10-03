#ifndef RINGBUF_H
#define RINGBUF_H

#include "types.h"

#define MAX_RINGBUFS 10
#define RINGBUF_SIZE 16

struct ringbuf {
    int refcount; // 0 for empty slot
    char name[16];
    void *buf[RINGBUF_SIZE]; // physical addresses of pages that comprise the ring buffer
    void *book;
};

struct book {
    uint64 read_done, write_done;
};

extern struct spinlock ringbuf_lock;

void lazy_init_ringbufs(void);
int map_ringbuf_to_process(int index, void **addr);
int create_ringbuf(const char *name, void **addr);
int close_ringbuf(const char *name);

#endif // RINGBUF_H
