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
#include <linux/mman.h>

#include "RingBuf_public.h"
#include "RingBuf_private.h"

#define LENGTH (2 * 1024 * 1024) // 2MB
#define PROT (PROT_READ | PROT_WRITE)
// #define FLAGS (MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB)
#define FLAGS (MAP_SHARED | MAP_HUGETLB)

#define SHM_PATH "/dev/shm/t"
#define OBJ_NUM 1024

int test()
{
    printf("%lu\n", offsetof(RingBuf_t, head_));
    printf("%lu\n", offsetof(RingBuf_t, tail_));
    printf("%lu\n", offsetof(RingBuf_t, objSize_));
    printf("%lu\n", sizeof(RingBuf_t));

    printf("%lu\n", offsetof(BRingBuf_t, head_));
    printf("%lu\n", offsetof(BRingBuf_t, tail_));
    printf("%lu\n", offsetof(BRingBuf_t, mtx));
    printf("%lu\n", offsetof(BRingBuf_t, writeable));
    printf("%lu\n", offsetof(BRingBuf_t, readable));
    printf("%lu\n", offsetof(BRingBuf_t, objSize_));
    printf("%lu\n", offsetof(BRingBuf_t, objNum_));
    printf("%lu\n", sizeof(BRingBuf_t));

    // void *addr = mmap(NULL, LENGTH, PROT, FLAGS, -1, 0);
    // if (addr == MAP_FAILED) {
    //     perror("mmap");
    //     return 1;
    // }
    // memset(addr, 0, 1024);
    // printf("Hugepage memory mapped at %p\n", addr);
    getchar();
    return 0;
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

    // void *addr = mmap(NULL, LENGTH, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_HUGETLB, fd, 0);
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

void test4()
{

    void *r = Get_SpscRingBuf(OBJ_NUM, sizeof(Obj), SHM_PATH, MAP_NEW | MAP_SHM, 0);
    r = Get_MpscRingBuf(OBJ_NUM, sizeof(Obj), SHM_PATH, MAP_NEW | MAP_SHM, 0);
    r = Get_MpmcRingBuf(OBJ_NUM, sizeof(Obj), SHM_PATH, MAP_NEW | MAP_SHM, 0);
    r = Get_BlockRingBuf(OBJ_NUM, sizeof(Obj), SHM_PATH, MAP_NEW | MAP_SHM, 0);
}

__attribute__((always_inline)) inline
static size_t get_aligned_offset(size_t base_offset, size_t alignment)
{
    return (base_offset + alignment - 1) & ~(alignment - 1);
}

typedef struct _Obj_
{
    char c[16];
} Obj_t;

int main(int argc, char *argv[])
{
    // test();
    // test1();
    // test2();
    // test4();

    MpscRingBuf_t *r = Get_MpscRingBuf(2, sizeof(Obj_t), SHM_PATH, MAP_NEW | MAP_SHM, 0);
    return 0;
};