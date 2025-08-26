#include <assert.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "RingBuf_public.h"
#include "RingBuf_private.h"

SpscRingBuf_t *Get_SpscRingBuf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag)
{
    return (SpscRingBuf_t* ) get_buf(objNum, objSize, shmPath, prot, flag, NO_SLOT);
}

void Del_SpscRingBuf(SpscRingBuf_t *p)
{
    del_buf((RingBuf_t *) p);
}

void *Begin_push_SpscRingBuf(SpscRingBuf_t *p)
{
    RingBuf_t *r = (RingBuf_t *) p;
    const size_t curr_head = atomic_load_explicit(&r->head_, memory_order_relaxed);
    const size_t next_head = curr_head + 1;
    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_acquire);
    if ((next_head - curr_tail) > r->objNum_) {
        return NULL;
    }
    return &r->buffer_[(curr_head & r->mask_) * r->objSize_];
}

void End_push_SpscRingBuf(SpscRingBuf_t *p)
{
    RingBuf_t *r = (RingBuf_t *) p;
    const size_t curr_head = atomic_load_explicit(&r->head_, memory_order_relaxed);
    atomic_store_explicit(&r->head_, curr_head + 1, memory_order_release);
}

void *Begin_pop_SpscRingBuf(SpscRingBuf_t *p)
{
    RingBuf_t *r = (RingBuf_t *) p;
    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    const size_t curr_head = atomic_load_explicit(&r->head_, memory_order_acquire);
    if ((curr_head - curr_tail) == 0) {
        return NULL;
    }
    return &r->buffer_[(curr_tail & r->mask_) * r->objSize_];
}

void End_pop_SpscRingBuf(SpscRingBuf_t *p)
{
    RingBuf_t *r = (RingBuf_t *) p;
    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    atomic_store_explicit(&r->tail_, curr_tail + 1, memory_order_release);
}

#if DEBUG
size_t Push_SpscRingBuf(SpscRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o)
#else
size_t Push_SpscRingBuf(SpscRingBuf_t *p, void *args)
#endif
{
    RingBuf_t *r = (RingBuf_t *) p;
    const size_t curr_head = atomic_load_explicit(&r->head_, memory_order_relaxed);
    while ((curr_head - (atomic_load_explicit(&r->tail_, memory_order_acquire))) >= r->objNum_) {
        cpu_relax();
    }
#if DEBUG
    cb(arr, curr_head, buf, o);
#endif
    memcpy(&r->buffer_[(curr_head & r->mask_) * r->objSize_], args, r->objSize_);
    atomic_store_explicit(&r->head_, curr_head + 1, memory_order_release);
    return curr_head;
}

size_t Pop_SpscRingBuf(SpscRingBuf_t *p, void *buf)
{
    RingBuf_t *r = (RingBuf_t *) p;
    const size_t curr_head = atomic_load_explicit(&r->head_, memory_order_acquire);
    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    if (curr_tail == curr_head) {
        return -1;
    }
    memcpy(buf, &r->buffer_[(curr_tail & r->mask_) * r->objSize_], r->objSize_);
    atomic_store_explicit(&r->tail_, curr_tail + 1, memory_order_release);
    return curr_tail;
}

bool Is_empty_SpscRingBuf(SpscRingBuf_t *p)
{
    RingBuf_t *r = (RingBuf_t *) p;
    size_t curr_head = atomic_load_explicit(&r->head_, memory_order_relaxed);
    size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    return curr_head == curr_tail;
}

bool Is_full_SpscRingBuf(SpscRingBuf_t *p)
{
    RingBuf_t *r = (RingBuf_t *) p;
    const size_t curr_head = atomic_load_explicit(&r->head_, memory_order_relaxed);
    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    return ((curr_head + 1 - curr_tail) > r->objNum_);
}

size_t Capacity_SpscRingBuf(SpscRingBuf_t *p)
{
    RingBuf_t *r = (RingBuf_t *) p;
    return r->objNum_;
}

size_t Size_SpscRingBuf(SpscRingBuf_t *p)
{
    RingBuf_t *r = (RingBuf_t *) p;
    const size_t curr_head = atomic_load_explicit(&r->head_, memory_order_relaxed);
    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    return curr_head - curr_tail;
}
