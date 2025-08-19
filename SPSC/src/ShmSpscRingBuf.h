#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct ShmSpscRingBuf_ ShmSpscRingBuf_t;

typedef struct SpscRingProperty_ {
    int fd;
    void *bufAddr;
} SpscRingProperty_t;

#ifdef __cplusplus
extern "C" {
#endif

SpscRingProperty_t Get_shmSpscRingBuf(const size_t objNum, const size_t objSize, const char *shmPath);
void Del_shmSpscRingBuf(SpscRingProperty_t property);

void *Begin_push_shmSpscRingBuf(ShmSpscRingBuf_t *r);
void End_push_shmSpscRingBuf(ShmSpscRingBuf_t *r);
void *Begin_pop_shmSpscRingBuf(ShmSpscRingBuf_t *r);
void End_pop_shmSpscRingBuf(ShmSpscRingBuf_t *r);
bool Is_empty_shmSpscRingBuf(ShmSpscRingBuf_t *r);
bool Is_full_shmSpscRingBuf(ShmSpscRingBuf_t *r);
size_t Capacity_shmSpscRingBuf(ShmSpscRingBuf_t *r);
size_t Size_shmSpscRingBuf(ShmSpscRingBuf_t *r);

#ifdef __cplusplus
}
#endif
