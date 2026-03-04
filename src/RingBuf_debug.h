#pragma once

#include "RingBuf_public.h"

#ifdef __cplusplus
extern "C" {
#endif

// use this when debugging
#if DEBUG
typedef struct Obj_ {
    uint64_t magH;
    char buf[1024];
    uint64_t magT;
    uint64_t seq;
} __attribute__((__aligned__(64))) __attribute__((__packed__)) Obj ;

typedef struct Time_diff_ {
    struct timespec s;
    char pad1[64 - sizeof(struct timespec)];
    struct timespec e;
    char pad2[64 - sizeof(struct timespec)];
} __attribute__((aligned(64))) Time_diff_t;
typedef void (*testFunc)(Time_diff_t *arr, size_t pushed, char buf[], Obj *o);

ssize_t Push_SpscRingBuf(SpscRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o, size_t size);
ssize_t Push_MpscRingBuf(MpscRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o, size_t size);
ssize_t Push_MpmcRingBuf(MpmcRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o, size_t size);
ssize_t Push_BlockRingBuf(BlockRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o, size_t size);
#endif

#ifdef __cplusplus
}
#endif
