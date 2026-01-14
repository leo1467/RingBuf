#pragma once
#include <stdatomic.h>
#include "RingBuf_public.h"

// ringbuf type
#define SPSC        1
#define COMMIT      0
#define SLOT        0
#define TRY         0 // for commit try push
#define BLOCK       0

// for test
#define YIELD       0   // 1 for push & pop to yield when got NULL
#define RELAX       1   // 1 for cpu_relax()
#define SEND_SLEEP  1   // 0 for producer to not to sleep 1us after push
#define PRO_THD_NUM 1   // producer threads
#define CON_THD_NUM 1   // consumer threads
#define BINDCORE    1

// debug ues
#define TIME_TEST   1   // 0 for testing memory integrity
#define PRINT       0   // print every msg after push and pop
#define MSG         0   // print msg in Obj::buf
#define ASSERT      1   // comsumer assert check msg
#define PRIME 3571
// #define PRIME 1

#define N           10000 // test loop
#define OBJ_NUM     513

#define magichead 0xDEADBEEF
#define magictail 0xBEEFDEAD

// #define Shm_Path NULL
// #define SHM_PATH "/dev/shm/ringBuf"
#define SHM_PATH "/mnt/huge/test"
#define SHM_TIME_ARR "/dev/shm/timeArr"

typedef struct args_ {
    atomic_int *coreN;
    atomic_size_t *pushed_got;
    void *rbuf;
    char *buf;
    Time_diff_t *arr;
} args_thd;
