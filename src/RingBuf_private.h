#pragma once
#include <errno.h>
#include "RingBuf_public.h"

#define CACHE_LINE_SIZE 64
#define RETRY_NUM 64

/**
 * Macro to set errno and return error code
 */
#define RINGBUF_SET_ERROR(err) do { errno = (err); return (err); } while(0)

enum RingBufSlot {
    USE_SLOT = 1 << 0,
    NO_SLOT  = 1 << 1,
};

typedef struct _RingBuf {
    atomic_size_t head_ __attribute__((__aligned__(CACHE_LINE_SIZE)));
    char pad0[CACHE_LINE_SIZE - sizeof(atomic_size_t)];
    atomic_size_t commit_ __attribute__((__aligned__(CACHE_LINE_SIZE)));
    char pad1[CACHE_LINE_SIZE - sizeof(atomic_size_t)];

    atomic_size_t tail_ __attribute__((__aligned__(CACHE_LINE_SIZE)));
    char pad2[CACHE_LINE_SIZE - sizeof(atomic_size_t)];

    // ---- buffer data ----
    size_t objSize_;
    size_t objNum_;
    size_t mask_;
    size_t totalSize_;
    int mapType_;
    int fd;
    size_t buffer_offset_;
    size_t slot_offset_;
    char pad3[CACHE_LINE_SIZE - sizeof(size_t) * 4 - sizeof(int) * 2 - sizeof(size_t) * 2];
} __attribute__((__aligned__(CACHE_LINE_SIZE))) RingBuf_t;

RingBuf_t *get_buf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag, int useSlot);
void del_buf(RingBuf_t *r);

// Helper macros to get actual pointers from offsets
#define GET_BUFFER(r) ((char *)(r) + (r)->buffer_offset_)
#define GET_SLOT(r) ((atomic_size_t *)((char *)(r) + (r)->slot_offset_))
