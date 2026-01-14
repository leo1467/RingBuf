#include <assert.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "RingBuf_private.h"
#include "RingBuf_public.h"

MpmcRingBuf_t *Get_MpmcRingBuf(const size_t objNum,
                               const size_t objSize,
                               const char *shmPath,
                               int prot,
                               int flag)
{
    return (MpmcRingBuf_t *) get_buf(objNum, objSize, shmPath, prot, flag, USE_SLOT);
}

void Del_MpmcRingBuf(MpmcRingBuf_t *p)
{
    del_buf((RingBuf_t *) p);
}

#if DEBUG
ssize_t Try_push_MpmcRingBuf(MpmcRingBuf_t *p,
                             void *args,
                             testFunc cb,
                             Time_diff_t *arr,
                             char buf[],
                             Obj *o,
                             size_t size)
#else
ssize_t Try_push_MpmcRingBuf(MpmcRingBuf_t *p, void *args, size_t size)
#endif
{
    RingBuf_t *r = (RingBuf_t *) p;
    if (size > r->objSize_) {
        errno = RINGBUF_PUSH_SIZE_TOO_LARGE;
        return errno;
    }
    unsigned spin = 0;

    for (;;) {
        size_t expected_head = atomic_load_explicit(
            &r->head_, memory_order_relaxed);  // 取得目前 head（生產者預約的序號
        size_t idx = expected_head & r->mask_;
        size_t seq = atomic_load_explicit(&GET_SLOT(r)[idx],
                                          memory_order_acquire);  // 讀取 slot 的 sequence number

        /* seq - pos == 0 代表這格是空的，因為 consumer 讀完之後會把 seq 設為 pos + objNum
         * producer 繞了一圈會回到這格之後，看到 slot 裡面是 pos + objNum，代表可以寫入 */
        intptr_t dif = (intptr_t) seq - (intptr_t) expected_head;

        if (dif == 0) {
            size_t desired = expected_head + 1;  // 嘗試用 CAS 保留這個 slot（head_ 前進）
            if (atomic_compare_exchange_weak_explicit(&r->head_, &expected_head, desired,
                                                      memory_order_acq_rel, memory_order_relaxed)) {
#if DEBUG
                cb(arr, expected_head, buf, (Obj *) args);
#endif
                memcpy(&GET_BUFFER(r)[idx * r->objSize_], args, r->objSize_);
                atomic_store_explicit(
                    &GET_SLOT(r)[idx], expected_head + 1,
                    memory_order_release);  // 發佈：將 slot 的 seq 設為 pos+1，表示這格已經填好可被消費
                return expected_head;
            }
            // CAS 失敗，pos 已被其他 producer 更新，重試
        } else if (dif < 0) {
            // seq < pos，代表 queue 滿了
            errno = RINGBUF_FULL;
            return errno;
        } else {
            // 其他 producer/consumer 尚未完成，稍後重試
            cpu_relax();
            if (++spin > RETRY_NUM) {
                errno = RINGBUF_CONTENTION;
                return errno;
            }
        }
    }
}

ssize_t Try_pop_MpmcRingBuf(MpmcRingBuf_t *p, void *buf)
{
    RingBuf_t *r = (RingBuf_t *) p;
    unsigned spin = 0;

    for (;;) {
        size_t expected_tail = atomic_load_explicit(
            &r->tail_, memory_order_relaxed);  // 取得目前 tail（消費者預約的序號）
        size_t idx = expected_tail & r->mask_;
        size_t seq = atomic_load_explicit(&GET_SLOT(r)[idx],
                                          memory_order_acquire);  // 讀取 slot 的 sequence number
        intptr_t dif = (intptr_t) seq -
                       (intptr_t) (expected_tail + 1);  // seq - (pos+1) == 0 代表這格有資料可讀

        if (dif == 0) {
            size_t desired = expected_tail + 1;  // 嘗試用 CAS 保留這個 slot（tail_ 前進）
            if (atomic_compare_exchange_weak_explicit(&r->tail_, &expected_tail, desired,
                                                      memory_order_acq_rel, memory_order_relaxed)) {
                memcpy(buf, &GET_BUFFER(r)[idx * r->objSize_], r->objSize_);
                atomic_store_explicit(
                    &GET_SLOT(r)[idx], expected_tail + r->objNum_,
                    memory_order_release);  // 標記 slot 為 empty，設 seq = pos + objNum，供下一輪 producer 使用
                return expected_tail;
            }
            // CAS 失敗，pos 已被其他 consumer 更新，重試
        } else if (dif < 0) {
            // seq < pos+1，代表 queue 為空
            errno = RINGBUF_EMPTY;
            return errno;
        } else {
            // 其他 producer 尚未 publish，稍後重試
            cpu_relax();
            if (++spin > RETRY_NUM) {
                errno = RINGBUF_CONTENTION;
                return errno;
            }
        }
    }
}

int Pop_w_cb_MpmcRingBuf(MpmcRingBuf_t *p, Pop_cb cb, void *args)
{
    RingBuf_t *r = (RingBuf_t *) p;
    unsigned spin = 0;

    for (;;) {
        size_t pos = atomic_load_explicit(
            &r->tail_, memory_order_relaxed);  // 取得目前 tail（消費者預約的序號）
        size_t idx = pos & r->mask_;
        size_t seq = atomic_load_explicit(&GET_SLOT(r)[idx],
                                          memory_order_acquire);  // 讀取 slot 的 sequence number
        intptr_t dif =
            (intptr_t) seq - (intptr_t) (pos + 1);  // seq - (pos+1) == 0 代表這格有資料可讀

        if (dif == 0) {
            size_t desired = pos + 1;  // 嘗試用 CAS 保留這個 slot（tail_ 前進）
            if (atomic_compare_exchange_weak_explicit(&r->tail_, &pos, desired,
                                                      memory_order_acq_rel, memory_order_relaxed)) {
                int rc = cb(&GET_BUFFER(r)[idx * r->objSize_], args);
                atomic_store_explicit(
                    &GET_SLOT(r)[idx], pos + r->objNum_,
                    memory_order_release);  // 標記 slot 為 empty，設 seq = pos + objNum，供下一輪 producer 使用
                return rc;
            }
            // CAS 失敗，pos 已被其他 consumer 更新，重試
        } else if (dif < 0) {
            // seq < pos+1，代表 queue 為空
            errno = RINGBUF_EMPTY;
            return errno;
        } else {
            // 其他 producer 尚未 publish，稍後重試
            cpu_relax();
            if (++spin > RETRY_NUM) {
                errno = RINGBUF_CONTENTION;
                return errno;
            }
        }
    }
}

ssize_t Try_pop_MpmcMpscRingBuf(MpmcRingBuf_t *p, void *buf)
{
    RingBuf_t *r = (RingBuf_t *) p;
    const size_t curr_head = atomic_load_explicit(&r->head_, memory_order_acquire);
    const size_t curr_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);
    if (curr_head - curr_tail == 0) {
        errno = RINGBUF_EMPTY;
        return errno;
    }

    const size_t expected_signal = curr_tail + 1;
    const size_t idx = curr_tail & r->mask_;
    if (atomic_load_explicit(&GET_SLOT(r)[idx], memory_order_acquire) != expected_signal) {
        errno = RINGBUF_CONTENTION;
        return errno;
    }

    memcpy(buf, &GET_BUFFER(r)[idx * r->objSize_], r->objSize_);
    atomic_store_explicit(&GET_SLOT(r)[idx], curr_tail + r->objNum_, memory_order_release);
    atomic_store_explicit(&r->tail_, curr_tail + 1, memory_order_relaxed);
    return curr_tail;
}
