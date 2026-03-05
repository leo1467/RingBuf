#include <assert.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "RingBuf_debug.h"
#include "RingBuf_private.h"
#include "RingBuf_public.h"

static __thread size_t local_head = 0;

MpscRingBuf_t *Get_MpscRingBuf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag)
{
    return (MpscRingBuf_t *) get_buf(objNum, objSize, shmPath, prot, flag, MPSC_SLOT);
}

void Del_MpscRingBuf(MpscRingBuf_t *p)
{
    del_buf((RingBuf_t *) p);
}

#if DEBUG
ssize_t Push_MpscRingBuf(MpscRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o, size_t size)
#else
ssize_t Push_MpscRingBuf(MpscRingBuf_t *p, void *args, size_t size)
#endif
{
    RingBuf_t *r = (RingBuf_t *) p;
    if (size > r->objSize_) {
        return RINGBUF_PUSH_SIZE_TOO_LARGE;
    }

    size_t curr_head = atomic_load_explicit(&r->head_, memory_order_acquire);
    size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_acquire);
    if (curr_head - curr_tail >= r->objNum_) {
        return RINGBUF_FULL;
    }
    if (!atomic_compare_exchange_strong_explicit(&r->head_, &curr_head, curr_head + 1, memory_order_acq_rel, memory_order_relaxed)) {
        return RINGBUF_CONTENTION;
    }
#if DEBUG
    cb(arr, curr_head, buf, o);
#endif
    const size_t idx = curr_head & r->mask_;
    memcpy(&GET_BUFFER(r)[idx * r->objSize_], args, size);
    atomic_store_explicit(&GET_SLOT(r, idx), SLOT_VALID, memory_order_release);
    return curr_head;
}

ssize_t Pop_MpscRingBuf(MpscRingBuf_t *p, void *buf, size_t size)
{
    RingBuf_t *r = (RingBuf_t *) p;
    if (size > r->objSize_) {
        return RINGBUF_POP_SIZE_TOO_LARGE;
    }

    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    if (local_head == curr_tail) {
        local_head = atomic_load_explicit(&r->head_, memory_order_acquire);
        if (local_head == curr_tail) {
            return RINGBUF_EMPTY;
        }
    }

    const size_t idx = curr_tail & r->mask_;
    const enum SlotStat slot_stat = atomic_load_explicit(&GET_SLOT(r, idx), memory_order_acquire);
    if (slot_stat & SLOT_EMPTY) {
        return RINGBUF_SLOT_WRITING_DATA;
    } else if (slot_stat & SLOT_UNKNOWN) {
        atomic_store_explicit(&GET_SLOT(r, idx), SLOT_EMPTY, memory_order_relaxed);
        atomic_store_explicit(&r->tail_, curr_tail + 1, memory_order_release);
        return RINGBUF_SLOT_STAT_UNKNOWN;
    }

    memcpy(buf, &GET_BUFFER(r)[idx * r->objSize_], size);
    atomic_store_explicit(&GET_SLOT(r, idx), SLOT_EMPTY, memory_order_relaxed);
    atomic_store_explicit(&r->tail_, curr_tail + 1, memory_order_release);
    return curr_tail;
}

int Pop_w_cb_MpscRingBuf(MpscRingBuf_t *p, Pop_cb cb, void *args)
{
    RingBuf_t *r = (RingBuf_t *) p;
    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    if (local_head == curr_tail) {
        local_head = atomic_load_explicit(&r->head_, memory_order_acquire);
        if (local_head == curr_tail) {
            return RINGBUF_EMPTY;
        }
    }

    const size_t idx = curr_tail & r->mask_;
    const enum SlotStat slot_stat = atomic_load_explicit(&GET_SLOT(r, idx), memory_order_acquire);
    if (slot_stat & SLOT_EMPTY) {
        return RINGBUF_SLOT_WRITING_DATA;
    } else if (slot_stat & SLOT_UNKNOWN) {
        atomic_store_explicit(&GET_SLOT(r, idx), SLOT_EMPTY, memory_order_relaxed);
        atomic_store_explicit(&r->tail_, curr_tail + 1, memory_order_release);
        return RINGBUF_SLOT_STAT_UNKNOWN;
    }

    int rc = cb(&GET_BUFFER(r)[idx * r->objSize_], args);
    atomic_store_explicit(&GET_SLOT(r, idx), SLOT_EMPTY, memory_order_relaxed);
    atomic_store_explicit(&r->tail_, curr_tail + 1, memory_order_release);
    return rc;
}

bool Is_empty_MpscRingBuf(MpscRingBuf_t *p)
{
    RingBuf_t *r = (RingBuf_t *) p;
    size_t curr_head = atomic_load_explicit(&r->head_, memory_order_relaxed);
    size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);

    return curr_head == curr_tail;
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
