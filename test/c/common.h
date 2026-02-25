#pragma once

// #define _GNU_SOURCE

#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>

#include "RingBuf_public.h"

// ringbuf type
#define SPSC 0
#define MPSC 0
#define MPMC 1
#define BLOCK 0

// for test
#define YIELD 0          // 1 for push & pop to yield when got NULL
#define RELAX 1          // 1 for cpu_relax()
#define PRO_SLEEP 50000  // producer sleep ns
#define CON_SLEEP 0      // consumer sleep ns
#define PRO_THD_NUM 3    // producer threads
#define CON_THD_NUM 1    // consumer threads
#define BINDCORE 1

// debug use
#define TIME_TEST 1  // 0 for testing memory integrity
#define PRINT 0      // print every msg after push and pop
#define MSG 1        // print msg in Obj::buf
#define ASSERT 1     // comsumer assert check msg
#define PRIME 3571
// #define PRIME 1

#define ONE_THD_SLEEP_NS 0 //1000000000
#define BINDCORE_STEP 2
#define PRO_START_CORE 3
#define CON_START_CORE 9

#define N       10000  // test loop
#define OBJ_NUM 1024

#define magichead 0xDEADBEEF
#define magictail 0xBEEFDEAD

// #define SHM_PATH NULL
#define SHM_PATH "/dev/shm/ringBuf"
// #define SHM_PATH "/mnt/huge/test"
#define SHM_TIME_ARR "/dev/shm/timeArr"

typedef struct args_
{
    atomic_int *coreN;
    atomic_size_t *pushed_got;
    void *rbuf;
    char *buf;
    Time_diff_t *arr;
} args_thd;

__attribute__((always_inline)) inline 
static int bindCore(int core)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

__attribute__((always_inline)) inline 
static bool check_core_collide()
{
    int pro_start = PRO_START_CORE;
    int pro_end = PRO_START_CORE + BINDCORE_STEP * (PRO_THD_NUM - 1);
    int con_start = CON_START_CORE;
    int con_end = CON_START_CORE + BINDCORE_STEP * (CON_THD_NUM - 1);
    return pro_start <= con_end && con_start <= pro_end;
}

__attribute__((always_inline)) inline 
static void spin_sleep_ns(long tm)
{
    if (tm <= 0) {
        return;
    }

    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (true) {
        clock_gettime(CLOCK_MONOTONIC, &now);

        long long elapsed_ns = (now.tv_sec - start.tv_sec) * 1000000000LL + (now.tv_nsec - start.tv_nsec);

        if (elapsed_ns >= tm) {
            break;
        }
        cpu_relax();
    }
}

__attribute__((always_inline)) inline 
static long long time_diff( const struct timespec *s, const struct timespec *e)
{
    long long start_ns = s->tv_sec * 1000000000LL + s->tv_nsec;
    long long end_ns = e->tv_sec * 1000000000LL + e->tv_nsec;

    return end_ns - start_ns;
}
