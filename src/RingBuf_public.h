#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__x86_64__) || defined(__i386__)
#define cpu_relax() __builtin_ia32_pause()
#else
#define cpu_relax() do {} while (0)
#endif

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
#endif

typedef struct _MpscRingBuf MpscRingBuf_t;
typedef struct _SpscRingBuf SpscRingBuf_t;
enum RingBufType {
    MAP_MALLOC  = 1 << 0,
    MAP_NEW     = 1 << 1, 
    MAP_EXIST   = 1 << 2,
};

#ifdef __cplusplus
extern "C" {
#endif

/* SPSC */
SpscRingBuf_t *Get_SpscRingBuf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag);
void Del_SpscRingBuf(SpscRingBuf_t *p);

void *Begin_push_SpscRingBuf(SpscRingBuf_t *p);
void End_push_SpscRingBuf(SpscRingBuf_t *p);
void *Begin_pop_SpscRingBuf(SpscRingBuf_t *p);
void End_pop_SpscRingBuf(SpscRingBuf_t *p);

#if DEBUG
size_t Push_SpscRingBuf(SpscRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o);
#else
size_t Push_SpscRingBuf(SpscRingBuf_t *p, void *buf);
#endif
size_t Pop_SpscRingBuf(SpscRingBuf_t *p, void *buf);
bool Is_empty_SpscRingBuf(SpscRingBuf_t *p);
bool Is_full_SpscRingBuf(SpscRingBuf_t *p);
size_t Capacity_SpscRingBuf(SpscRingBuf_t *p);
size_t Size_SpscRingBuf(SpscRingBuf_t *p);



/* MPSC */
MpscRingBuf_t *Get_MpscRingBuf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag);
void Del_MpscRingBuf(MpscRingBuf_t *p);

#if DEBUG
size_t Push_MpscRingBuf(MpscRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o);
size_t Try_Push_MpscRingBuf(MpscRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o);
#else
size_t Push_MpscRingBuf(MpscRingBuf_t *p, void *args);
size_t Try_Push_MpscRingBuf(MpscRingBuf_t *p, void *args);
#endif

size_t Pop_MpscRingBuf(MpscRingBuf_t *p, void *buf);
bool Is_empty_MpscRingBuf(MpscRingBuf_t *p);
bool Is_full_MpscRingBuf(MpscRingBuf_t *p);
size_t Capacity_MpscRingBuf(MpscRingBuf_t *p);
size_t Size_MpscRingBuf(MpscRingBuf_t *p);

#ifdef __cplusplus
}
#endif
