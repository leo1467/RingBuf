#include <assert.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "RingBuf_public.h"
#include "RingBuf_private.h"

MpscRingBuf_t *Get_MpscRingBuf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag)
{
    return (MpscRingBuf_t *) get_buf(objNum, objSize, shmPath, prot, flag, NO_SLOT);
}

void Del_MpscRingBuf(MpscRingBuf_t *p)
{
    del_buf((RingBuf_t *) p);
}

#if DEBUG
ssize_t Push_MpscRingBuf(MpscRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o)
#else
ssize_t Push_MpscRingBuf(MpscRingBuf_t *p, void *args)
#endif
{
    RingBuf_t *r = (RingBuf_t *) p;
    const size_t curr_head = atomic_fetch_add_explicit(&r->head_, 1, memory_order_relaxed);
    while ((curr_head - (atomic_load_explicit(&r->tail_, memory_order_acquire))) >= r->objNum_) {
        cpu_relax();
    }
#if DEBUG
    cb(arr, curr_head, buf, o);
#endif
    memcpy(&GET_BUFFER(r)[(curr_head & r->mask_) * r->objSize_], args, r->objSize_);
    while (atomic_load_explicit(&r->commit_, memory_order_relaxed) != curr_head) {
        cpu_relax();
    }
    atomic_store_explicit(&r->commit_, curr_head + 1, memory_order_release);
    return curr_head;
}

#if DEBUG
ssize_t Try_push_MpscRingBuf(MpscRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o)
#else
ssize_t Try_push_MpscRingBuf(MpscRingBuf_t *p, void *args)
#endif
{
    RingBuf_t *r = (RingBuf_t *) p;
    unsigned spin = 0;
    size_t expected_head = atomic_load_explicit(&r->head_, memory_order_acquire);

    for (;;) {
        size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_acquire);
        if (expected_head - curr_tail >= r->objNum_) {
            errno = RINGBUF_FULL;
            return errno;
        }
        if (atomic_compare_exchange_weak_explicit(&r->head_, &expected_head, expected_head + 1, memory_order_acq_rel, memory_order_relaxed)) {
            break;
        }
        if (++spin > RETRY_NUM) return -1;
    }
#if DEBUG
    cb(arr, expected_head, buf, o);
#endif
    const size_t idx = expected_head & r->mask_;
    memcpy(&GET_BUFFER(r)[idx * r->objSize_], args, r->objSize_);
    while (atomic_load_explicit(&r->commit_, memory_order_relaxed) != expected_head) {
        cpu_relax();
    }
    atomic_store_explicit(&r->commit_, expected_head + 1, memory_order_release);
    return expected_head;
}

ssize_t Pop_MpscRingBuf(MpscRingBuf_t *p, void *buf)
{
    RingBuf_t *r = (RingBuf_t *) p;
    const size_t curr_commit = atomic_load_explicit(&r->commit_, memory_order_acquire);
    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    if (curr_tail == curr_commit) {
        errno = RINGBUF_EMPTY;
        return errno;
    }
    const size_t idx = curr_tail & r->mask_;
    memcpy(buf, &GET_BUFFER(r)[idx * r->objSize_], r->objSize_);
    atomic_store_explicit(&r->tail_, curr_tail + 1, memory_order_release);
    return curr_tail;
}

int Pop_w_cb_MpscRingBuf(MpscRingBuf_t *p, Pop_cb cb, void *args)
{
    RingBuf_t *r = (RingBuf_t *) p;
    const size_t curr_commit = atomic_load_explicit(&r->commit_, memory_order_acquire);
    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    if (curr_tail == curr_commit) {
        errno = RINGBUF_EMPTY;
        return errno;
    }
    const size_t idx = curr_tail & r->mask_;
    int rc = cb(&GET_BUFFER(r)[idx * r->objSize_], args);
    atomic_store_explicit(&r->tail_, curr_tail + 1, memory_order_release);
    return rc;
}

bool Is_empty_MpscRingBuf(MpscRingBuf_t *p)
{
    RingBuf_t *r = (RingBuf_t *) p;
    size_t curr_commit = atomic_load_explicit(&r->commit_, memory_order_relaxed);
    size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);

    return curr_commit == curr_tail;
}

bool Is_full_MpscRingBuf(MpscRingBuf_t *p)
{
    RingBuf_t *r = (RingBuf_t *) p;
    size_t curr_head = atomic_load_explicit(&r->head_, memory_order_relaxed);
    size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    return (curr_head - curr_tail) >= r->objNum_;
}

size_t Capacity_MpscRingBuf(MpscRingBuf_t *p)
{
    RingBuf_t *r = (RingBuf_t *) p;
    return r->objNum_;
}

size_t Size_MpscRingBuf(MpscRingBuf_t *p)
{
    RingBuf_t *r = (RingBuf_t *) p;
    size_t curr_head = atomic_load_explicit(&r->head_, memory_order_relaxed);
    size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    return curr_head - curr_tail;
}
