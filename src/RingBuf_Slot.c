#include <assert.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "RingBuf_public.h"
#include "RingBuf_private.h"

MpmcRingBuf_t *Get_MpmcRingBuf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag)
{
    return (MpmcRingBuf_t *) get_buf(objNum, objSize, shmPath, prot, flag, USE_SLOT);
}

void Del_MpmcRingBuf(MpmcRingBuf_t *p)
{
    del_buf((RingBuf_t *) p);
}

#if DEBUG
size_t Try_push_MpmcRingBuf(MpmcRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o)
#else
size_t Try_push_MpmcRingBuf(MpmcRingBuf_t *p, void *args)
#endif
{
    RingBuf_t *r = (RingBuf_t *) p;
    size_t curr_head = atomic_load_explicit(&r->head_, memory_order_relaxed);
    size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_acquire);
    if (curr_head - curr_tail >= r->objNum_) {
        return -1;
    }
    if (!atomic_compare_exchange_weak_explicit(&r->head_, &curr_head, curr_head + 1, memory_order_release, memory_order_relaxed)) {
        return -1;
    }
#if DEBUG
    cb(arr, curr_head, buf, o);
#endif
    memcpy(&r->buffer_[(curr_head & r->mask_) * r->objSize_], args, r->objSize_);
    atomic_store_explicit(&r->slot_[curr_head & r->mask_], curr_head + 1, memory_order_release);
    return curr_head;
}

size_t Try_pop_MpmcRingBuf(MpmcRingBuf_t *p, void *buf)
{
    RingBuf_t *r = (RingBuf_t *) p;
    size_t curr_head = atomic_load_explicit(&r->head_, memory_order_acquire);
    size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_acquire);
    if (curr_head == curr_tail) {
        return -1;
    }
    const size_t expected_signal = curr_tail + 1;
    if (atomic_load_explicit(&r->slot_[curr_tail & r->mask_], memory_order_acquire) != expected_signal) {
        return -1;
    }

    if (!atomic_compare_exchange_weak_explicit(&r->tail_, &curr_tail, expected_signal, memory_order_release, memory_order_relaxed)) {
        return -1;
    }
    memcpy(buf, &r->buffer_[(curr_tail & r->mask_) * r->objSize_], r->objSize_);
    return curr_tail;
}

size_t Try_pop_MpmcMpscRingBuf(MpmcRingBuf_t *p, void *buf)
{
    RingBuf_t *r = (RingBuf_t *) p;
    const size_t curr_head = atomic_load_explicit(&r->head_, memory_order_acquire);
    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    if  (curr_head - curr_tail == 0) {
        return -1;
    }

    const size_t expected_signal = curr_tail + 1;
    if (atomic_load_explicit(&r->slot_[curr_tail & r->mask_], memory_order_acquire) != expected_signal) {
        return -1; 
    }

    memcpy(buf, &r->buffer_[(curr_tail & r->mask_) * r->objSize_], r->objSize_);
    atomic_store_explicit(&r->tail_, curr_tail + 1, memory_order_release);
    return curr_tail;
}
