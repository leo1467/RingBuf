#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "NuThread.h"
#include "RingBuf_public.h"
#include "common.h"

__attribute__((always_inline)) inline static void chose_test_type_producer(Time_diff_t *arr,
                                                                           size_t pushed,
                                                                           char buf[],
                                                                           Obj *o)
{
#if TIME_TEST == 1
    clock_gettime(CLOCK_MONOTONIC, &(arr[pushed].s));  // 紀錄push時間
    o->seq = pushed;
#else
    o->magH = magichead;
    o->magT = magictail;
    memcpy(o->buf, buf, sizeof(o->buf));
    o->buf[0] = (o->buf[0] + pushed) * PRIME % 97 % 26 + 97;
    o->seq = pushed;
#endif
};

void *producer_thd_work(void *args_)
{
    // 產生要放入Obj::buf的資料
    char buf[sizeof((Obj){}.buf)] = {};
    for (int i = 0; i < sizeof(buf); ++i) {
        buf[i] = 'a' + ((i % 97) % 26);
    }
    buf[sizeof(buf) - 1] = '\0';

    int64_t rcc = 0;
    args_thd *args = (args_thd *) args_;
    atomic_size_t *pushed = args->pushed_got;
    Time_diff_t *arr = args->arr;
#if SPSC == 1
    SpscRingBuf_t *r = args->rbuf;
#elif COMMIT == 1
    MpscRingBuf_t *r = args->rbuf;
#elif SLOT == 1
    MpmcRingBuf_t *r = args->rbuf;
#elif BLOCK == 1
    BlockedRingBuf_t *r = args->rbuf;
#endif
    atomic_int *coreN = args->coreN;

#if BINDCORE == 1
    NuThdBindCore(atomic_fetch_add_explicit(coreN, 2, memory_order_release));
#endif

    Obj o;
    while (atomic_load_explicit(pushed, memory_order_acquire) < N) {
#if SEND_SLEEP == 1
        usleep(1);
#endif

#if SPSC == 1
        rcc = Push_SpscRingBuf(r, &o, chose_test_type_producer, arr, buf, &o, sizeof(Obj));
#elif COMMIT == 1
#if TRY == 1
        rcc = Try_push_MpscRingBuf(r, &o, chose_test_type_producer, arr, buf, &o, sizeof(Obj));
#else
        rcc = Push_MpscRingBuf(r, &o, chose_test_type_producer, arr, buf, &o, sizeof(Obj));
#endif
#elif SLOT == 1
        rcc = Try_push_MpmcRingBuf(r, &o, chose_test_type_producer, arr, buf, &o, sizeof(Obj));
#elif BLOCK == 1
        rcc = Push_BlockedRingBuf(r, &o, chose_test_type_producer, arr, buf, &o, sizeof(Obj));
#endif
        if (rcc >= 0) {
            atomic_fetch_add_explicit(pushed, 1, memory_order_release);
#if PRINT == 1
            char bb[2048] = {};
#if MSG == 1
            snprintf(bb, sizeof(bb), "push=[%ld][%lu][%s]\n", rcc, o.seq, o.buf);
#else
            snprintf(bb, sizeof(bb), "push=[%ld][%lu]\n", rcc, o.seq);
#endif
            write(STDOUT_FILENO, bb, strlen(bb));
#endif
        } else {
#if YIELD == 1
            sched_yield();
#elif RELAX == 1
            cpu_relax();
#endif
        }
    }
    pthread_exit(NULL);
}

int main()
{
#if SPSC == 1
    // SpscRingBuf_t *r = Get_SpscRingBuf(OBJ_NUM, sizeof(Obj), SHM_PATH, MAP_NEW | MAP_SHM, 0);
    SpscRingBuf_t *r = Get_SpscRingBuf(OBJ_NUM, sizeof(Obj), SHM_PATH, MAP_NEW | MAP_SHM,
                                       MAP_HUGETLB | MAP_POPULATE);
#elif COMMIT == 1
    MpscRingBuf_t *r = Get_MpscRingBuf(OBJ_NUM, sizeof(Obj), SHM_PATH, MAP_NEW | MAP_SHM, 0);
#elif SLOT == 1
    MpmcRingBuf_t *r = Get_MpmcRingBuf(OBJ_NUM, sizeof(Obj), SHM_PATH, MAP_NEW | MAP_SHM, 0);
#elif BLOCK == 1
    BlockedRingBuf_t *r = Get_BlockedRingBuf(OBJ_NUM, sizeof(Obj), SHM_PATH, MAP_NEW | MAP_SHM, 0);
#endif
    if (!r) {
        perror("RingBuf constructor");
        return 1;
    }

    int fd = open(SHM_TIME_ARR, O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        perror("memfd_create");
        return 1;
    }

    int rc = 0;
    const size_t TOTAL_SIZE = sizeof(Time_diff_t) * N;
    rc = ftruncate(fd, TOTAL_SIZE);
    if (rc < 0) {
        perror("ftruncate");
        return 1;
    }

    void *p = mmap(NULL, TOTAL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    Time_diff_t *arr = (Time_diff_t *) p;  // 紀錄push pop時間用

    write(STDOUT_FILENO, "count down ", strlen("count donw "));
    char bb[16] = {};
    for (int i = 10; i > 0; --i) {  // 讓comsumer先跑
        snprintf(bb, sizeof(bb) - 1, "%d ", i);
        write(STDOUT_FILENO, bb, strlen(bb));
        sleep(1);
    }
    write(STDOUT_FILENO, "\n", strlen("\n"));

    pthread_t tids[PRO_THD_NUM];
    atomic_int coreN;
    atomic_init(&coreN, 3);
    atomic_size_t pushed;
    atomic_init(&pushed, 0);
    args_thd args = {.coreN = &coreN, .arr = arr, .buf = NULL, .pushed_got = &pushed, .rbuf = r};
    for (int i = 0; i < ((SPSC == 1) ? 1 : PRO_THD_NUM); ++i) {
        pthread_create(&tids[i], NULL, producer_thd_work, (void *) &args);
    }

    for (int i = 0; i < ((SPSC == 1) ? 1 : PRO_THD_NUM); ++i) {
        pthread_join(tids[i], NULL);
    }

#if SPSC == 1
    Del_SpscRingBuf(r);
#elif COMMIT == 1
    Del_MpscRingBuf(r);
#elif SLOT == 1
    Del_MpmcRingBuf(r);
#endif
    close(fd);
    munmap(p, TOTAL_SIZE);
    usleep(50000);
    unlink(SHM_PATH);
    unlink(SHM_TIME_ARR);
    return 0;
}
