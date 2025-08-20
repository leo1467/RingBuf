#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__x86_64__) || defined(__i386__)
#define cpu_relax() __builtin_ia32_pause()
#else
#define cpu_relax() do {} while (0)
#endif

typedef struct ShmMpscRingBuf_ ShmMpscRingBuf_t;

typedef struct MpscRingProperty_ {
    int fd;
    void *bufAddr;
} MpscRingProperty_t;

#ifdef __cplusplus
extern "C" {
#endif

MpscRingProperty_t Get_shmMpscRingBuf(const size_t objNum, const size_t objSize, const char *shmPath);
void Del_shmMpscRingBuf(MpscRingProperty_t property);

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
size_t Push_shmMpscRingBuf(ShmMpscRingBuf_t *r, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o);
#else
size_t Push_shmMpscRingBuf(ShmMpscRingBuf_t *r, void *args);
#endif

int64_t Pop_shmMpscRingBuf(ShmMpscRingBuf_t *r, void *buf);
bool Is_empty_shmMpscRingBuf(ShmMpscRingBuf_t *r);
bool Is_full_shmMpscRingBuf(ShmMpscRingBuf_t *r);
size_t Capacity_shmMpscRingBuf(ShmMpscRingBuf_t *r);
size_t Size_shmMpscRingBuf(ShmMpscRingBuf_t *r);

#ifdef __cplusplus
}
#endif
