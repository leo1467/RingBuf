#pragma once
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>

#include "RingBuf_public.h"

#define CACHE_LINE_SIZE 64
#define HUGEPAGE_SIZE (2 * 1024 * 1024)

/**
 * Macro to set errno and return error code
 */
#define RINGBUF_SET_ERROR(err) do { errno = (err); return (err); } while(0)

enum RingBufSlot
{
    NA_SLOT = 0,
    NO_SLOT = 1 << 0,
    MPSC_SLOT = 1 << 1,
    MPMC_SLOT = 1 << 2,
};

enum SlotStat
{
    SLOT_UNKNOWN = 0,
    SLOT_EMPTY = 1 << 0,
    SLOT_VALID = 1 << 1,
};

typedef struct _Slot
{
    atomic_size_t __attribute__((__aligned__((CACHE_LINE_SIZE)))) slot;
} Slot_t;

typedef struct _RingBuf
{
    atomic_size_t head_ __attribute__((__aligned__(CACHE_LINE_SIZE)));
    atomic_size_t tail_ __attribute__((__aligned__(CACHE_LINE_SIZE)));

    // ---- buffer metadata ----
    struct
    {
        size_t objSize_;
        size_t objNum_;
        size_t mask_;
        size_t totalSize_;
        int mapType_;
        int fd;
        size_t buffer_offset_;
        size_t slot_offset_;
    } __attribute__((__aligned__(CACHE_LINE_SIZE)));
} __attribute__((__aligned__(CACHE_LINE_SIZE))) RingBuf_t;

RingBuf_t *get_buf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag, int useSlot);
void del_buf(RingBuf_t *r);

// Helper macros to get actual pointers from offsets
#define GET_BUFFER(r) ((char *)(r) + (r)->buffer_offset_)
#define GET_SLOT(r, i) (((Slot_t *)((char *)(r) + (r)->slot_offset_))[(i)].slot)

typedef struct _BRingBuf
{
    size_t head_ __attribute__((__aligned__(CACHE_LINE_SIZE)));
    size_t tail_ __attribute__((__aligned__(CACHE_LINE_SIZE)));
    pthread_mutex_t mtx __attribute__((__aligned__(CACHE_LINE_SIZE)));
    pthread_cond_t writeable __attribute__((__aligned__(CACHE_LINE_SIZE)));
    pthread_cond_t readable __attribute__((__aligned__(CACHE_LINE_SIZE)));

    // ---- buffer metadata ----
    struct
    {
        size_t objSize_;
        size_t objNum_;
        size_t mask_;
        size_t totalSize_;
        int mapType_;
        int fd;
    } __attribute__((__aligned__(CACHE_LINE_SIZE)));

    char buffer[];
} __attribute__((__aligned__(CACHE_LINE_SIZE))) BRingBuf_t;

BRingBuf_t *get_block_buf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag);
void del_block_buf(BRingBuf_t *r);