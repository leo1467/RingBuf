#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct ShmSpscRingBuf_ ShmSpscRingBuf_t;

typedef struct SpscRingProperty_ {
    int fd;
    void *bufAddr;
} SpscRingProperty;

#ifdef __cplusplus
extern "C" {
#endif

SpscRingProperty Get_shm_ringBuf(const size_t objNum, const size_t objSize, const char *shm_path);
void Del_shm_ringBuf(SpscRingProperty property);

void *begin_push(ShmSpscRingBuf_t *r);
void end_push(ShmSpscRingBuf_t *r);
void *begin_pop(ShmSpscRingBuf_t *r);
void end_pop(ShmSpscRingBuf_t *r);
bool empty(ShmSpscRingBuf_t *r);
bool full(ShmSpscRingBuf_t *r);
size_t capacity(ShmSpscRingBuf_t *r);
size_t size(ShmSpscRingBuf_t *r);

#ifdef __cplusplus
}
#endif
