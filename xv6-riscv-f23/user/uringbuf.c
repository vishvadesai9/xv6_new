#include "kernel/types.h"
#include "user/user.h"
#include "kernel/ringbuf.h"
#include "kernel/riscv.h"

struct user_ring_buf
{
    void *buf;
    struct book *book_page;
    int exists;
    char *name;
};

static struct user_ring_buf user_ring_bufs[MAX_RINGBUFS];

void store(uint64 *p, uint64 v)
{
    __atomic_store_8(p, v, __ATOMIC_SEQ_CST);
}
uint64 load(uint64 *p)
{
    return __atomic_load_8(p, __ATOMIC_SEQ_CST);
}

int find_open_ringbuf(char *name)
{
    for (int i = 0; i < MAX_RINGBUFS; i++)
    {
        // printf("checking index %d. userrbname: %s, param name: %s\n", i, user_ring_bufs[i].name, name);
        if (!strcmp(user_ring_bufs[i].name, name))
            return i;
    }
    for (int i = 0; i < MAX_RINGBUFS; i++)
        if (!user_ring_bufs[i].exists)
            return i;
    return -1;
}

int ucreate_ringbuf(char *name)
{
    int open_spot = find_open_ringbuf(name);
    if (open_spot == -1)
    {
        printf("Error, no more available fds\n");
        return -1;
    }

    void *addr;
    if (ringbuf(name, 1, &addr) == -1)
    {
        printf("Error creating ringbuf\n");
        return -1;
    }

    user_ring_bufs[open_spot].buf = (void *)((uint64)addr + PGSIZE);
    user_ring_bufs[open_spot].book_page = (struct book *)addr;
    user_ring_bufs[open_spot].exists = 1;
    user_ring_bufs[open_spot].name = name;
    user_ring_bufs[open_spot].book_page->read_done = (uint64)addr + PGSIZE;
    user_ring_bufs[open_spot].book_page->write_done = (uint64)addr + PGSIZE;

    return open_spot;
}

int free_ringbuf(int fd)
{
    user_ring_bufs[fd].exists = 0;
    void *temp;
    int error = ringbuf(user_ring_bufs[fd].name, 0, &temp);
    if (error == -1)
    {
        printf("Error freeing user ring buf\n");
        return -1;
    }
    return 0;
}

void ringbuf_start_read(int ring_desc, char **addr, int *bytes)
{
    *addr = (char *)user_ring_bufs[ring_desc].buf + (load(&user_ring_bufs[ring_desc].book_page->read_done) % (RINGBUF_SIZE * PGSIZE));
    *bytes = load(&user_ring_bufs[ring_desc].book_page->write_done) - load(&user_ring_bufs[ring_desc].book_page->read_done);
}

void ringbuf_finish_read(int ring_desc, int bytes)
{
    uint64 new_read_done = load(&user_ring_bufs[ring_desc].book_page->read_done) + bytes;
    store(&user_ring_bufs[ring_desc].book_page->read_done, new_read_done);

    // if our current pointer is at or greater than the size of doubly mapped ringbuf
    // if (user_ring_bufs[ring_desc].book_page->read_done >= (uint64)user_ring_bufs[ring_desc].buf + (RINGBUF_SIZE * PGSIZE))
    // {
    //     user_ring_bufs[ring_desc].book_page->read_done -= RINGBUF_SIZE * PGSIZE; // store
    // }
}

void ringbuf_start_write(int ring_desc, char **addr, int *bytes)
{
    *addr = (char *)user_ring_bufs[ring_desc].buf + (load(&user_ring_bufs[ring_desc].book_page->write_done) % (RINGBUF_SIZE * PGSIZE));
    // PGSIZE - (rd - wd)
    *bytes = RINGBUF_SIZE * PGSIZE - (load(&user_ring_bufs[ring_desc].book_page->write_done) - load(&user_ring_bufs[ring_desc].book_page->read_done));
}

void ringbuf_finish_write(int ring_desc, int bytes)
{
    uint64 new_write_done = load(&user_ring_bufs[ring_desc].book_page->write_done) + bytes;
    store(&user_ring_bufs[ring_desc].book_page->write_done, new_write_done);

    // if our current pointer is at or greater than the size of doubly mapped ringbuf
    // if (user_ring_bufs[ring_desc].book_page->write_done >= (uint64)user_ring_bufs[ring_desc].buf + (RINGBUF_SIZE * PGSIZE))
    // {
    //     user_ring_bufs[ring_desc].book_page->write_done -= RINGBUF_SIZE * PGSIZE; // store
    // }
}