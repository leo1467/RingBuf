#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

int64_t Push_shmMpscRingBuf(ShmMpscRingBuf_t *r, void *args);
int64_t Pop_shmMpscRingBuf(ShmMpscRingBuf_t *r, void *buf);
bool Is_empty_shmMpscRingBuf(ShmMpscRingBuf_t *r);
bool Is_full_shmMpscRingBuf(ShmMpscRingBuf_t *r);
size_t Capacity_shmMpscRingBuf(ShmMpscRingBuf_t *r);
size_t Size_shmMpscRingBuf(ShmMpscRingBuf_t *r);

#ifdef __cplusplus
}
#endif
