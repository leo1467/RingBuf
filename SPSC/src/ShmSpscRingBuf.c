#include <assert.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ShmSpscRingBuf.h"

#define CACHE_LINE_SIZE 64

struct ShmSpscRingBuf_ {
    atomic_size_t head_ __attribute__((__aligned__(CACHE_LINE_SIZE)));
    char pad1[CACHE_LINE_SIZE - sizeof(atomic_size_t)];

    atomic_size_t tail_ __attribute__((__aligned__(CACHE_LINE_SIZE)));
    char pad2[CACHE_LINE_SIZE - sizeof(atomic_size_t)];

    // ---- buffer data ----
    size_t objSize_;
    size_t objNum_;
    size_t mask_;
    char pad3[CACHE_LINE_SIZE - sizeof(size_t) * 3];
    char buffer_[] __attribute__((__aligned__(CACHE_LINE_SIZE)));
} __attribute__((__aligned__(CACHE_LINE_SIZE)));

SpscRingProperty_t Get_shmSpscRingBuf(const size_t objNum, const size_t objSize, const char *shmPath)
{
    /* 物件數量只能是2的冪次才能index到正確的位置 */
    assert((objNum >= 2) && ((objNum & (objNum - 1)) == 0));

    const size_t TOTAL_SIZE = objNum * objSize + sizeof(ShmSpscRingBuf_t);

    SpscRingProperty_t property = {.fd = -1, .bufAddr = NULL};
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

    ShmSpscRingBuf_t *r = (ShmSpscRingBuf_t *) p;
    r->objNum_ = objNum;
    r->mask_ = objNum - 1;
    r->objSize_ = objSize;
    atomic_init(&r->head_, 0);
    atomic_init(&r->tail_, 0);

    property.fd = fd;
    property.bufAddr = p;

    return property;
}

void Del_shmSpscRingBuf(SpscRingProperty_t property)
{
    ShmSpscRingBuf_t *r = (ShmSpscRingBuf_t *) property.bufAddr;
    if (r) {
        munmap(r, r->objNum_ * r->objSize_ + sizeof(ShmSpscRingBuf_t));
    }
    if (property.fd >= 0) {
        close(property.fd);
    }
}

void *Begin_push_shmSpscRingBuf(ShmSpscRingBuf_t *r)
{
    const size_t curr_head = atomic_load_explicit(&r->head_, memory_order_relaxed);
    const size_t next_head = curr_head + 1;
    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_acquire);
    if ((next_head - curr_tail) > r->objNum_) {
        return NULL;
    }
    return &r->buffer_[(curr_head & r->mask_) * r->objSize_];
}

void End_push_shmSpscRingBuf(ShmSpscRingBuf_t *r)
{
    const size_t curr_head = atomic_load_explicit(&r->head_, memory_order_relaxed);
    atomic_store_explicit(&r->head_, curr_head + 1, memory_order_release);
}

void *Begin_pop_shmSpscRingBuf(ShmSpscRingBuf_t *r)
{
    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    const size_t curr_head = atomic_load_explicit(&r->head_, memory_order_acquire);
    if ((curr_head - curr_tail) == 0) {
        return NULL;
    }
    return &r->buffer_[(curr_tail & r->mask_) * r->objSize_];
}

void End_pop_shmSpscRingBuf(ShmSpscRingBuf_t *r)
{
    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    atomic_store_explicit(&r->tail_, curr_tail + 1, memory_order_release);
}

bool Is_empty_shmSpscRingBuf(ShmSpscRingBuf_t *r)
{
    size_t curr_head = atomic_load_explicit(&r->head_, memory_order_acquire);
    size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_acquire);
    return curr_head == curr_tail;
}

bool Is_full_shmSpscRingBuf(ShmSpscRingBuf_t *r)
{
    const size_t curr_head = atomic_load_explicit(&r->head_, memory_order_acquire);
    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_acquire);
    return ((curr_head + 1 - curr_tail) > r->objNum_);
}

size_t Capacity_shmSpscRingBuf(ShmSpscRingBuf_t *r)
{
    return r->objNum_;
}

size_t Size_shmSpscRingBuf(ShmSpscRingBuf_t *r)
{
    const size_t curr_head = atomic_load_explicit(&r->head_, memory_order_acquire);
    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_acquire);
    return curr_head - curr_tail;
}
