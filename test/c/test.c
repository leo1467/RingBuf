#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "RingBuf_public.h"
#include "RingBuf_private.h"

#define LENGTH (2 * 1024 * 1024) // 2MB
#define PROT (PROT_READ | PROT_WRITE)
// #define FLAGS (MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB)
#define FLAGS (MAP_SHARED | MAP_HUGETLB)

typedef struct Obj
{
    char c[1024];
} Obj;

int test()
{
    printf("%lu\n", offsetof(RingBuf_t, head_));
    printf("%lu\n", offsetof(RingBuf_t, commit_));
    printf("%lu\n", offsetof(RingBuf_t, tail_));
    printf("%lu\n", offsetof(RingBuf_t, objSize_));
    printf("%lu\n", sizeof(RingBuf_t));

    // void *addr = mmap(NULL, LENGTH, PROT, FLAGS, -1, 0);
    // if (addr == MAP_FAILED) {
    //     perror("mmap");
    //     return 1;
    // }
    // memset(addr, 0, 1024);
    // printf("Hugepage memory mapped at %p\n", addr);
    getchar();
}

int test1()
{
    char path[] = "/mnt/huge/test";
    SpscRingBuf_t *r = Get_SpscRingBuf(1024, sizeof(Obj), path, MAP_NEW | MAP_SHM, MAP_PRIVATE | MAP_HUGETLB);
    if (!r) {
        printf("%s\n", strerror(errno));
    }
    getchar();
    Del_SpscRingBuf(r);
    unlink(path);
    return 0;
}

int test2()
{
    const char *path = "/mnt/huge/myfile";
    int fd = open(path, O_CREAT | O_RDWR, 0755);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (ftruncate(fd, LENGTH) != 0) {
        perror("ftruncate");
        return 1;
    }

    void *addr = mmap(NULL, LENGTH, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_HUGETLB, fd, 0);

    if (addr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    memset(addr, 0, LENGTH);

    printf("Hugepage mapped at %p\n", addr);
    getchar();
    unlink(path);
    return 0;
}

void test3()
{
    size_t size = 7788;
    size_t total = ceil((double)size / HUGEPAGE_SIZE) * HUGEPAGE_SIZE;
    printf("%ld\n", total); 
}

int main(int argc, char *argv[])
{
    test();
    return 0;
};