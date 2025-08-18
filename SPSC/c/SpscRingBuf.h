#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct SpscRingBuf_ SpscRingBuf_t;

typedef struct SpscRingProperty_ {
    int fd;
    void *bufAddr;
} SpscRingProperty;

#ifdef __cplusplus
extern "C" {
#endif

SpscRingProperty Get_shm_ringBuf(const size_t objNum, const size_t objSize, const char *shm_path);
void Del_shm_ringBuf(SpscRingProperty property);

void *begin_push(SpscRingBuf_t *r);
void end_push(SpscRingBuf_t *r);
void *begin_pop(SpscRingBuf_t *r);
void end_pop(SpscRingBuf_t *r);
bool empty(SpscRingBuf_t *r);
bool full(SpscRingBuf_t *r);
size_t capacity(SpscRingBuf_t *r);
size_t size(SpscRingBuf_t *r);

#ifdef __cplusplus
}
#endif
