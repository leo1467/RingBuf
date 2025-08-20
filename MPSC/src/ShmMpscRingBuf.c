#include <assert.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ShmMpscRingBuf.h"

#define CACHE_LINE_SIZE 64

struct ShmMpscRingBuf_ {
    atomic_size_t head_ __attribute__((__aligned__(CACHE_LINE_SIZE)));
    atomic_size_t commit_ __attribute__((__aligned__(CACHE_LINE_SIZE)));
    char pad1[CACHE_LINE_SIZE - sizeof(atomic_size_t) * 2];

    atomic_size_t tail_ __attribute__((__aligned__(CACHE_LINE_SIZE)));
    char pad2[CACHE_LINE_SIZE - sizeof(atomic_size_t)];

    // ---- buffer data ----
    size_t objSize_;
    size_t objNum_;
    size_t mask_;
    char pad3[CACHE_LINE_SIZE - sizeof(size_t) * 3];
    char buffer_[] __attribute__((__aligned__(CACHE_LINE_SIZE)));
} __attribute__((__aligned__(CACHE_LINE_SIZE)));

MpscRingProperty_t Get_shmMpscRingBuf(const size_t objNum, const size_t objSize, const char *shmPath)
{
    /* 物件數量只能是2的冪次才能index到正確的位置 */
    assert((objNum >= 2) && ((objNum & (objNum - 1)) == 0));

    const size_t TOTAL_SIZE = objNum * objSize + sizeof(ShmMpscRingBuf_t);

    MpscRingProperty_t property = {.fd = -1, .bufAddr = NULL};
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

    ShmMpscRingBuf_t *r = (ShmMpscRingBuf_t *) p;
    r->objNum_ = objNum;
    r->mask_ = objNum - 1;
    r->objSize_ = objSize;
    atomic_init(&r->head_, 0);
    atomic_init(&r->tail_, 0);
    atomic_init(&r->commit_, 0);

    property.fd = fd;
    property.bufAddr = p;

    return property;
}

void Del_shmMpscRingBuf(MpscRingProperty_t property)
{
    ShmMpscRingBuf_t *r = (ShmMpscRingBuf_t *) property.bufAddr;
    if (r) {
        munmap(r, r->objNum_ * r->objSize_ + sizeof(ShmMpscRingBuf_t));
    }
    if (property.fd >= 0) {
        close(property.fd);
    }
}

#if DEBUG
size_t Push_shmMpscRingBuf(ShmMpscRingBuf_t *r, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o)
{
    const size_t curr_head = atomic_fetch_add_explicit(&r->head_, 1, memory_order_relaxed);
    while ((curr_head - (atomic_load_explicit(&r->tail_, memory_order_acquire))) >= r->objNum_) { cpu_relax(); }
    cb(arr, curr_head, buf, o);
    memcpy(&r->buffer_[(curr_head & r->mask_) * r->objSize_], args, r->objSize_);
    while (atomic_load_explicit(&r->commit_, memory_order_acquire) != curr_head) { cpu_relax(); }
    atomic_store_explicit(&r->commit_, curr_head + 1, memory_order_release);
    return curr_head;
}
#else
size_t Push_shmMpscRingBuf(ShmMpscRingBuf_t *r, void *args)
{
    const size_t curr_head = atomic_fetch_add_explicit(&r->head_, 1, memory_order_relaxed);
    while ((curr_head - (atomic_load_explicit(&r->tail_, memory_order_acquire))) >= r->objNum_) {
        cpu_relax();
    }
    memcpy(&r->buffer_[(curr_head & r->mask_) * r->objSize_], args, r->objSize_);
    while (atomic_load_explicit(&r->commit_, memory_order_acquire) != curr_head) {
        cpu_relax();
    }
    atomic_store_explicit(&r->commit_, curr_head + 1, memory_order_release);
    return curr_head;
}
#endif

int64_t Pop_shmMpscRingBuf(ShmMpscRingBuf_t *r, void *buf)
{
    const size_t curr_commit = atomic_load_explicit(&r->commit_, memory_order_acquire);
    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    if (curr_tail == curr_commit) {
        return -1;
    }
    memcpy(buf, &r->buffer_[(curr_tail & r->mask_) * r->objSize_], r->objSize_);
    atomic_store_explicit(&r->tail_, curr_tail + 1, memory_order_release);
    return curr_tail;
}

bool Is_empty_shmMpscRingBuf(ShmMpscRingBuf_t *r)
{
    size_t curr_commit = atomic_load_explicit(&r->commit_, memory_order_acquire);
    size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_acquire);

    return curr_commit == curr_tail;
}

bool Is_full_shmMpscRingBuf(ShmMpscRingBuf_t *r)
{
    size_t curr_head = atomic_load_explicit(&r->head_, memory_order_acquire);
    size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_acquire);
    return (curr_head - curr_tail) >= r->objNum_;
}

size_t Capacity_shmMpscRingBuf(ShmMpscRingBuf_t *r)
{
    return r->objNum_;
}

size_t Size_shmMpscRingBuf(ShmMpscRingBuf_t *r)
{
    size_t curr_head = atomic_load_explicit(&r->head_, memory_order_acquire);
    size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_acquire);
    return curr_head - curr_tail;
}