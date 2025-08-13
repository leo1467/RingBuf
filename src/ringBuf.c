#include <assert.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ringBuf.h"

typedef struct SpscRing_ {
    atomic_size_t head_ __attribute__((__aligned__(64)));
    char pad1[64 - sizeof(atomic_size_t)];

    atomic_size_t tail_ __attribute__((__aligned__(64)));
    char pad2[64 - sizeof(atomic_size_t)];

    // ---- buffer data ----
    size_t objSize_;
    size_t objNum_;
    size_t mask_;
    char pad3[64 - sizeof(size_t) * 3];
    char buffer_[] __attribute__((__aligned__(64)));
} SpscRing __attribute__((__aligned__(64)));

SpscRingProperty Get_shm_ringBuf(const size_t objNum, const size_t objSize, const char *shmPath)
{
    /* 物件數量只能是2的冪次才能index到正確的位置 */
    assert((objNum >= 2) && ((objNum & (objNum - 1)) == 0));

    const size_t TOTAL_SIZE = objNum * objSize + sizeof(SpscRing);

    SpscRingProperty property = {.fd = -1, .bufAddr = NULL};
    int rc = 0;
    int fd = -1;
    if (shmPath) {
        fd = open(shmPath, O_CREAT | O_RDWR, 0666);
    } else {
        // MFD_CLOEXEC in #define _GNU_SOURCE or -D_GNU_SOURCE
        fd = memfd_create("ringBuf", MFD_CLOEXEC);
    }
    if (fd < 0) {
        perror("memfd_create");
        return property;
    }

    rc = ftruncate(fd, TOTAL_SIZE);
    if (rc < 0) {
        perror("ftruncate");
        return property;
    }

    void *p = mmap(NULL, TOTAL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return property;
    }

    SpscRing *r = (SpscRing *) p;
    r->objNum_ = objNum;
    r->mask_ = objNum - 1;
    r->objSize_ = objSize;
    atomic_init(&r->head_, 0);
    atomic_init(&r->tail_, 0);

    property.fd = fd;
    property.bufAddr = p;

    return property;
}

void Del_shm_ringBuf(SpscRingProperty property)
{
    SpscRing *r = (SpscRing *) property.bufAddr;
    munmap(r, r->objNum_ * r->objSize_ + sizeof(SpscRing));
    close(property.fd);
}

void *begin_push(SpscRing *r)
{
    const size_t head = atomic_load_explicit(&r->head_, memory_order_relaxed);
    const size_t next_head = head + 1;
    const size_t tail = atomic_load_explicit(&r->tail_, memory_order_acquire);
    if ((next_head - tail) > r->objNum_) {
        return NULL;
    }
    return &r->buffer_[(head & r->mask_) * r->objSize_];
}

void end_push(SpscRing *r)
{
    const size_t head = atomic_load_explicit(&r->head_, memory_order_relaxed);
    atomic_store_explicit(&r->head_, head + 1, memory_order_release);
}

void *pop_begin(SpscRing *r)
{
    const size_t tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    const size_t head = atomic_load_explicit(&r->head_, memory_order_acquire);
    if ((head - tail) == 0) {
        return NULL;
    }
    return &r->buffer_[(tail & r->mask_) * r->objSize_];
}

void pop_end(SpscRing *r)
{
    const size_t tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    atomic_store_explicit(&r->tail_, tail + 1, memory_order_release);
}

bool empty(SpscRing *r)
{
    return (atomic_load_explicit(&r->head_, memory_order_acquire) ==
            atomic_load_explicit(&r->tail_, memory_order_acquire));
}

bool full(SpscRing *r)
{
    const size_t head = atomic_load_explicit(&r->head_, memory_order_acquire);
    const size_t tail = atomic_load_explicit(&r->tail_, memory_order_acquire);
    return ((head + 1 - tail) > r->objNum_);
}

size_t capacity(SpscRing *r)
{
    return r->objNum_;
}

size_t size(SpscRing *r)
{
    const size_t head = atomic_load_explicit(&r->head_, memory_order_acquire);
    const size_t tail = atomic_load_explicit(&r->tail_, memory_order_acquire);
    return head - tail;
}
