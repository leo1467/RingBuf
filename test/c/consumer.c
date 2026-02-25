#define _GNU_SOURCE

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include "RingBuf_public.h"
#include "common.h"

__attribute__((always_inline)) inline
static bool check(Obj *o, char buf[], int64_t rcc)
{
    char newchar = (buf[0] + o->seq) * PRIME % 97 % 26 + 97;
    char tmp = buf[0];
    bool rc = false;
    uint64_t len = 0;

    buf[0] = newchar;
    if (strncmp(o->buf, buf, sizeof(o->buf)) != 0) {
        char bb[2048] = {};
        len = snprintf(bb, sizeof(bb), "comsumer got   =[%ld][%lu][%s]\n", rcc, o->seq, o->buf);
        write(STDOUT_FILENO, bb, len);
        len = snprintf(bb, sizeof(bb), "buf should be  =[%ld][%lu][%s]\n", rcc, o->seq, buf);
        write(STDOUT_FILENO, bb, len);
        goto EXIT;
    }

    rc = (o->magH == magichead) && (o->magT == magictail);
    if (rc == false) {
        char bb[2048] = {};
        len = snprintf(bb, sizeof(bb), "magichead=[%u], magictail=[%u], o->seq=[%lu], o->magH=[%lu], o->magT=[%lu], rcc=[%ld]\n", magichead, magictail, o->seq, o->magH, o->magT, rcc);
        write(STDOUT_FILENO, bb, len);
    }
EXIT:
    buf[0] = tmp;
    return rc;
}

__attribute__((always_inline)) inline
static void chose_test_type_comsumer(Time_diff_t *arr, char buf[], Obj *o, int64_t rcc)
{
#if TIME_TEST == 1
    clock_gettime(CLOCK_MONOTONIC, &(arr[o->seq].e)); // 紀錄pop時間
#else
#if ASSERT == 1
    assert(check(o, buf, rcc));
#else
    check(o, buf, rcc);
#endif
#endif

#if PRINT == 1
    char bb[2048] = {};
    uint64_t len = 0;
#if MSG == 1
    len = snprintf(bb, sizeof(bb), "pop=[%lu][%lu][%s]\n", rcc, o->seq, o->buf);
#else
    len = snprintf(bb, sizeof(bb), "pop=[%lu][%lu]\n", rcc, o->seq);
#endif
    write(STDOUT_FILENO, bb, len);
#endif
};

void *consumer_thd_work(void *args_)
{
    // 產生要放入Obj::buf的資料
    char buf[sizeof((Obj){}.buf)] = {};
    for (int i = 0; i < sizeof(buf); ++i) {
        buf[i] = 'a' + ((i % 97) % 26);
    }
    buf[sizeof(buf) - 1] = '\0';

    int64_t rcc = 0;
    args_thd *args = (args_thd *) args_;
    atomic_size_t *got = args->pushed_got;
    Time_diff_t *arr = args->arr;
#if SPSC == 1
    SpscRingBuf_t *r = args->rbuf;
#elif MPSC == 1
    MpscRingBuf_t *r = args->rbuf;
#elif MPMC == 1
    MpmcRingBuf_t *r = args->rbuf;
#elif BLOCK == 1
    BlockRingBuf_t *r = args->rbuf;
#endif
    atomic_int *coreN = args->coreN;

#if BINDCORE == 1
    int core = atomic_fetch_add_explicit(coreN, BINDCORE_STEP, memory_order_release);
    bindCore(core);
    // printf("bindcore %d\n", core);
#endif

    Obj o;
    while (atomic_load_explicit(got, memory_order_acquire)< N) {
        spin_sleep_ns(CON_SLEEP);
#if SPSC == 1
        rcc = Pop_SpscRingBuf(r, &o);
#elif MPSC == 1
        rcc = Pop_MpscRingBuf(r, &o);
#elif MPMC == 1
        rcc = Pop_MpmcRingBuf(r, &o);
#elif BLOCK == 1
        rcc = Pop_BlockRingBuf(r, &o);
#endif
        // if (rcc == RINGBUF_EMPTY) {
        //     write(STDOUT_FILENO, "empty\n", 6);
        // }
        if (rcc >= 0) {
            chose_test_type_comsumer(arr, buf, &o, rcc);
            atomic_fetch_add_explicit(got, 1, memory_order_release);
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
    if (check_core_collide()) {
        printf("producer cores and consumer cores collide\n");
        return 1;
    }
#if SPSC == 1
    SpscRingBuf_t *r = Get_SpscRingBuf(OBJ_NUM, sizeof(Obj), SHM_PATH, MAP_EXIST | MAP_SHM, 0);
    // SpscRingBuf_t *r = Get_SpscRingBuf(OBJ_NUM, sizeof(Obj), SHM_PATH, MAP_EXIST | MAP_SHM, MAP_HUGETLB);
#elif MPSC == 1
    // MpscRingBuf_t *r = Get_MpscRingBuf(OBJ_NUM, sizeof(Obj), SHM_PATH, MAP_EXIST | MAP_SHM, 0);
    MpscRingBuf_t *r = Get_MpscRingBuf(OBJ_NUM, sizeof(Obj), SHM_PATH, MAP_EXIST | MAP_SHM, MAP_HUGETLB);
#elif MPMC == 1
    // MpmcRingBuf_t *r = Get_MpmcRingBuf(OBJ_NUM, sizeof(Obj), SHM_PATH, MAP_EXIST | MAP_SHM, 0);
    MpmcRingBuf_t *r = Get_MpmcRingBuf(OBJ_NUM, sizeof(Obj), SHM_PATH, MAP_EXIST | MAP_SHM, MAP_HUGETLB);
#elif BLOCK == 1
    BlockRingBuf_t *r = Get_BlockRingBuf(OBJ_NUM, sizeof(Obj), SHM_PATH, MAP_EXIST | MAP_SHM, 0);
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
    Time_diff_t *arr = (Time_diff_t *) p; // 紀錄push pop時間用

    pthread_t tids[CON_THD_NUM];
    atomic_int coreN;
    atomic_init(&coreN, CON_START_CORE);
    atomic_size_t got;
    atomic_init(&got, 0);
    args_thd args = {.coreN = &coreN, .arr = arr, .buf = NULL, .pushed_got = &got, .rbuf = r};
    for (int i = 0; i < ((SPSC == 1 || MPSC == 1) ? 1 : CON_THD_NUM); ++i) {
        pthread_create(&tids[i], NULL, consumer_thd_work, (void *) &args);
    }

    for (int i = 0; i < ((SPSC == 1 || MPSC == 1) ? 1 : CON_THD_NUM); ++i) {
        pthread_join(tids[i], NULL);
    }

#if TIME_TEST == 1
    for (int i = 0; i < N; ++i) {
        fprintf(stdout, "%lld\n", time_diff(&arr[i].s, &arr[i].e)); // 印出push pop印出時間
    }
#endif
#if SPSC == 1
    Del_SpscRingBuf(r);
#elif MPSC == 1
    Del_MpscRingBuf(r);
#elif MPMC == 1
    Del_MpmcRingBuf(r);
#endif
    close(fd);
    munmap(p, TOTAL_SIZE);
    return 0;
}
