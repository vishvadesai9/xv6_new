#include "kernel/types.h"
#include "user/user.h"
#include "kernel/ringbuf.h"
#include "kernel/riscv.h"

/*
*   Bandwidth of original xv6 pipe:   .111 MB/cycle 
*   Bandwidth of fastest xv6 pipe:    .588 MB/cycle
*   Bandwidth of magic ring buffer:   2.22 MB/cycle
*/

/*
*   Original:  10MB/90 cycles
*   Fastest:   10MB/17 cycles
*   Magic:     100MB/45 cycles
*/
const int TEST_SIZE = 1024 * 1024 * 100; // 100MB
int SEND_SEED = 555;
int DATA_SEED = 12;

int data_rand()
{
    DATA_SEED = (214013 * DATA_SEED + 2531011);
    return (DATA_SEED >> 16) & 0x7FFF;
}
int send_amount_rand()
{
    SEND_SEED = (214013 * SEND_SEED + 2531011);
    return (SEND_SEED >> 16) & 0x7FFF;
}

int main(int argc, char *argv[])
{
    if (fork() == 0)
    { // Child
        int fd = ucreate_ringbuf("magic!");
        int have_sent = 0;
        int num_write = 0;

        while (have_sent < TEST_SIZE) // 1mb
        {
            int can_write;
            char *addr;
            ringbuf_start_write(fd, &addr, &can_write);
            if (can_write)
            {
                int going_to_write = can_write - (send_amount_rand() % can_write);
                // printf("Can write %d bytes, going to write %d bytes\n", can_write, going_to_write);
                for (int i = 0; i < going_to_write; i++)
                {
                    char val = 'a';
                    val += data_rand() % 26;
                    memmove(addr + i, (void *)&val, 1);
                }
                ringbuf_finish_write(fd, going_to_write);
                have_sent += going_to_write;
                num_write++;
            }
        }
        printf("Done writing all data, completed %d writes\n", num_write);
        free_ringbuf(fd);
        exit(0);
    }
    else
    { // Parent
        int fd = ucreate_ringbuf("magic!");
        int have_read = 0;
        int start = uptime();

        while (have_read < TEST_SIZE)
        {
            int can_read;
            char *addr;
            ringbuf_start_read(fd, &addr, &can_read);

            if (can_read)
            {
                for(int i = 0; i < can_read; i++)
                {
                    char expected = 'a' + data_rand() % 26;
                    if(*(addr + i) != expected)
                    {
                        printf("Error! Expected %d, got %d\n", expected, *(addr + i));
                        exit(-1);
                    }
                }
                ringbuf_finish_read(fd, can_read);
                have_read += can_read;
            }
        }
        wait(0);
        int end = uptime();
        printf("Done reading, took %d\n", end-start);

        free_ringbuf(fd);
    }

    return 0;
}