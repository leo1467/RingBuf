#pragma once
#include "RingBuf_public.h"

#define CACHE_LINE_SIZE 64

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
    int fd;
    char *buffer_;
    atomic_size_t *slot_;
    char pad3[CACHE_LINE_SIZE - sizeof(size_t) * 3 - sizeof(int) - sizeof(atomic_size_t *) - sizeof(char *)];
} __attribute__((__aligned__(CACHE_LINE_SIZE))) RingBuf_t;

RingBuf_t *get_buf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag, int useSlot);
void del_buf(RingBuf_t *r);