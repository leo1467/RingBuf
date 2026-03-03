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
    return (MpmcRingBuf_t *) get_buf(objNum, objSize, shmPath, prot, flag, MPMC_SLOT);
}

void Del_MpmcRingBuf(MpmcRingBuf_t *p)
{
    del_buf((RingBuf_t *) p);
}

#if DEBUG
ssize_t Push_MpmcRingBuf(MpmcRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o, size_t size)
#else
ssize_t Push_MpmcRingBuf(MpmcRingBuf_t *p, void *args, size_t size)
#endif
{
    RingBuf_t *r = (RingBuf_t *) p;
    if (size > r->objSize_) {
        errno = RINGBUF_PUSH_SIZE_TOO_LARGE;
        return errno;
    }

    size_t expected_head = atomic_load_explicit(&r->head_, memory_order_relaxed);  // 取得目前 head
    size_t idx = expected_head & r->mask_;
    size_t seq = atomic_load_explicit(&GET_SLOT(r, idx), memory_order_acquire);  // 讀取 slot seq

    /* consumer 讀完之後會把 slot 內的 seq 設為 consumer 當下拿到的 tail + objNum
     * producer 繞了一圈會回到這格，如果 buf 這格是空的，slot seq 會等於 head */
    intptr_t dif = (intptr_t) seq - (intptr_t) expected_head;

    if (dif > 0) {
        /* 假如 thread a 取了 seq ，但突然停住
         * thread b 也進來取到了 expected_head，且也成功取到了 seq，也成功寫入並更新 slot
         * 此時 thread a 又開始動，繼續取 slot，此時更新後的 slot 會比 expected_head 還大 */
        errno = RINGBUF_CONTENTION;
        return errno;
    } else if (dif < 0) {
        /* 如果 consumer 速度太慢，producer 繞一圈追上來
         * 因為 slot seq 還沒被 consumer 更新成他的 tail + objNum，所以 head 會比較大 */
        errno = RINGBUF_FULL;
        return errno;
    }

    size_t desired = expected_head + 1;
    // 只有成功把 head + 1 的人才有寫入權
    if (!atomic_compare_exchange_strong_explicit(&r->head_, &expected_head, desired, memory_order_acq_rel, memory_order_relaxed)) {
        // CAS 失敗，head 已被其他 producer 更新，重試
        errno = RINGBUF_CONTENTION;
        return errno;
    }
#if DEBUG
    cb(arr, expected_head, buf, (Obj *) args);
#endif
    memcpy(&GET_BUFFER(r)[idx * r->objSize_], args, size);

    // 將 slot 的 seq 設為 當下的 head + 1，表示這格已經好了
    atomic_store_explicit(&GET_SLOT(r, idx), expected_head + 1, memory_order_release);
    return expected_head;
}

ssize_t Pop_MpmcRingBuf(MpmcRingBuf_t *p, void *buf)
{
    RingBuf_t *r = (RingBuf_t *) p;

    size_t expected_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);  /* 取得目前 tail */
    size_t idx = expected_tail & r->mask_;
    size_t seq = atomic_load_explicit(&GET_SLOT(r, idx), memory_order_acquire);  /* 讀取 slot seq */

    /* 如果 seq 比 tail 大 1， 代表這格有資料可讀 */
    intptr_t dif = (intptr_t) seq - (intptr_t) (expected_tail + 1);

    if (dif > 0) {
        /* 假如 thread a 取了 seq ，但突然停住
         * thread b 也進來取到了 expected_tail，且也成功取到了 seq，也成功寫入並更新 slot
         * 此時 thread a 又開始動，繼續取 slot，此時更新後的 slot 會比 expected_tail + 1 還大 */
        errno = RINGBUF_CONTENTION;
        return errno;
    } else if (dif < 0) {
        /* 如果 producer 速度太慢，consumer 追上來
         * 因為 slot seq 還沒被 producer 更新成他的 head + 1，所以 expected_tail + 1 會比較大 */
        errno = RINGBUF_EMPTY;
        return errno;
    }

    size_t desired = expected_tail + 1;
    /* 只有成功把 tail + 1 的人才有寫入權 */
    if (!atomic_compare_exchange_strong_explicit(&r->tail_, &expected_tail, desired, memory_order_acq_rel, memory_order_relaxed)) {
        /* CAS 失敗，tail 已被其他 consumer 更新 */
        errno = RINGBUF_CONTENTION;
        return errno;
    }
    memcpy(buf, &GET_BUFFER(r)[idx * r->objSize_], r->objSize_);

    /* 將 slot 的 seq 設為 當下的 tail + objNum，表示這格是空的 */
    atomic_store_explicit(&GET_SLOT(r, idx), expected_tail + r->objNum_, memory_order_release);
    return expected_tail;
}

int Pop_w_cb_MpmcRingBuf(MpmcRingBuf_t *p, Pop_cb cb, void *args)
{
    RingBuf_t *r = (RingBuf_t *) p;

    size_t expected_tail = atomic_load_explicit(&r->tail_, memory_order_relaxed);  /* 取得目前 tail */
    size_t idx = expected_tail & r->mask_;
    size_t seq = atomic_load_explicit(&GET_SLOT(r, idx), memory_order_acquire);  /* 讀取 slot seq */

    /* 如果 seq 比 tail 大 1， 代表這格有資料可讀 */
    intptr_t dif = (intptr_t) seq - (intptr_t) (expected_tail + 1);

    if (dif > 0) {
        /* 假如 thread a 取了 seq ，但突然停住
         * thread b 也進來取到了 expected_tail，且也成功取到了 seq，也成功寫入並更新 slot
         * 此時 thread a 又開始動，繼續取 slot，此時更新後的 slot 會比 expected_tail + 1 還大 */
        errno = RINGBUF_CONTENTION;
        return errno;
    } else if (dif < 0) {
        /* 如果 producer 速度太慢，consumer 追上來
         * 因為 slot seq 還沒被 producer 更新成他的 head + 1，所以 expected_tail + 1 會比較大 */
        errno = RINGBUF_EMPTY;
        return errno;
    }

    size_t desired = expected_tail + 1;
    /* 只有成功把 tail + 1 的人才有寫入權 */
    if (!atomic_compare_exchange_strong_explicit(&r->tail_, &expected_tail, desired, memory_order_acq_rel, memory_order_relaxed)) {
        /* CAS 失敗，tail 已被其他 consumer 更新 */
        errno = RINGBUF_CONTENTION;
        return errno;
    }
    int rc = cb(&GET_BUFFER(r)[idx * r->objSize_], args);

    /* 將 slot 的 seq 設為 當下的 tail + objNum，表示這格是空的 */
    atomic_store_explicit(&GET_SLOT(r, idx), expected_tail + r->objNum_, memory_order_release);
    return rc;
}
