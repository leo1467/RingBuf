#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct SpscRing_ SpscRing;

typedef struct SpscRingProperty_ {
    int fd;
    void *bufAddr;
} SpscRingProperty;

SpscRingProperty Get_shm_ringBuf(const size_t objNum, const size_t objSize, const char *shm_path);
void Del_shm_ringBuf(SpscRingProperty property);

void *begin_push(SpscRing *r);
void end_push(SpscRing *r);
void *pop_begin(SpscRing *r);
void pop_end(SpscRing *r);
bool empty(SpscRing *r);
bool full(SpscRing *r);
size_t capacity(SpscRing *r);
size_t size(SpscRing *r);
