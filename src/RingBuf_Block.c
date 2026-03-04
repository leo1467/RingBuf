#include <string.h>
#include <unistd.h>

#include "RingBuf_private.h"
#include "RingBuf_public.h"

BlockRingBuf_t *Get_BlockRingBuf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag)
{
    return (BlockRingBuf_t *) get_block_buf(objNum, objSize, shmPath, prot, flag);
}

void Del_BlockRingBuf(BlockRingBuf_t *r) { del_block_buf((BRingBuf_t *) r); }

#if DEBUG
ssize_t Push_BlockRingBuf(BlockRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o, size_t size)
#else
ssize_t Push_BlockRingBuf(BlockRingBuf_t *p, void *args, size_t size)
#endif
{
    BRingBuf_t *r = (BRingBuf_t *) p;
    if (size > r->objSize_) {
        errno = RINGBUF_PUSH_SIZE_TOO_LARGE;
        return errno;
    }
    pthread_mutex_lock(&r->mtx);
    while (r->head_ - r->tail_ >= r->objNum_) {
        pthread_cond_wait(&r->writeable, &r->mtx);
    }
#if DEBUG
    cb(arr, r->head_, buf, (Obj *) args);
#endif
    memcpy(&r->buffer[(r->head_ & r->mask_) * r->objSize_], args, size);
    size_t head = r->head_;
    r->head_ += 1;
    pthread_cond_signal(&r->readable);
    pthread_mutex_unlock(&r->mtx);
    return head;
}

ssize_t Pop_BlockRingBuf(BlockRingBuf_t *p, void *buf, size_t size)
{
    BRingBuf_t *r = (BRingBuf_t *) p;
    if (size > r->objSize_) {
        return RINGBUF_POP_SIZE_TOO_LARGE;
    }
    pthread_mutex_lock(&r->mtx);
    while (r->head_ == r->tail_) {
        pthread_cond_wait(&r->readable, &r->mtx);
    }
    memcpy(buf, &r->buffer[(r->tail_ & r->mask_) * r->objSize_], size);
    size_t tail = r->tail_;
    r->tail_ += 1;
    pthread_cond_signal(&r->writeable);
    pthread_mutex_unlock(&r->mtx);
    return tail;
}
