#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "RingBuf_public.h"
#include "RingBuf_private.h"

typedef struct _SizeInfo {
    size_t buf_off_s;
    size_t buf_off_e;
    size_t slot_off_s;
    size_t slot_off_e;
    size_t total_size;
    size_t aligned_objSize;  // Object size aligned to cache line to prevent false sharing
} SizeInfo_t;

const char* RingBuf_strerror(int error_code)
{
    switch (error_code) {
        case RINGBUF_SUCCESS:               return "Success";
        case RINGBUF_FULL:                  return "Ring buffer is full";
        case RINGBUF_EMPTY:                 return "Ring buffer is empty";
        case RINGBUF_CONTENTION:            return "High contention, retry suggested";
        case RINGBUF_INVALID_PARAM:         return "Invalid parameters";
        case RINGBUF_NO_MAPPING_TYPE:       return "No mapping type specified";
        case RINGBUF_CAPACITY_WRONG:        return "Capacity must be the power of two and >= 2";
        case RINGBUF_MAPPING_NOT_EXISTS:    return "Use MAP_EXISTS but memory mapping does not exist";
        case RINGBUF_MAPPING_SIZE_ERROR:    return "Mapping size mismatch";
        case RINGBUF_PUSH_SIZE_TOO_LARGE:   return "Push size exceeded base obj size";
        case RINGBUF_SLOT_WRITING_DATA:     return "Producer is writing slot";
        case RINGBUF_SLOT_STAT_UNKNOWN:     return "Ring buffer slot data unknown, probably won't happen";
        default:                            return strerror(errno);
    }
}

__attribute__((always_inline)) inline
static size_t get_aligned_offset(size_t base_offset, size_t alignment)
{
    return (base_offset + alignment - 1) & ~(alignment - 1);
}

__attribute__((always_inline)) inline
static SizeInfo_t get_total_size(const size_t objNum, const size_t objSize, int useSlot)
{
    // Align individual object size to cache line to prevent false sharing in multi-threaded scenarios
    const size_t aligned_objSize = get_aligned_offset(objSize, CACHE_LINE_SIZE);

    size_t total_size = 0;
    size_t buffer_offset_start = 0;
    size_t buffer_offset_end = 0;
    size_t slot_offset_start = 0;
    size_t slot_offset_end = 0;
    buffer_offset_start = get_aligned_offset(sizeof(RingBuf_t), CACHE_LINE_SIZE);
    buffer_offset_end = get_aligned_offset(buffer_offset_start + aligned_objSize * objNum, CACHE_LINE_SIZE);
    if (useSlot & (MPMC_SLOT | MPSC_SLOT)) {
        slot_offset_start = get_aligned_offset(buffer_offset_end, CACHE_LINE_SIZE);
        slot_offset_end = get_aligned_offset(slot_offset_start + sizeof(Slot_t) * objNum, CACHE_LINE_SIZE);
        total_size = slot_offset_end;
    } else if (useSlot & NO_SLOT) {
        total_size = buffer_offset_end;
    }

    return (SizeInfo_t){.buf_off_s  = buffer_offset_start,
                        .buf_off_e  = buffer_offset_end,
                        .slot_off_s = slot_offset_start,
                        .slot_off_e = slot_offset_end,
                        .total_size = total_size,
                        .aligned_objSize = aligned_objSize};
}

static void *get_buf_malloc(size_t totalSz, int *fd)
{
    void *p = NULL;
    p = malloc(totalSz);
    if (!p) {
        errno = ENOMEM;
        return NULL;
    }
    *fd = -9999;

    return p;
}

static void *get_buf_shm(size_t totalSz, int *fd, int prot, int flag, const char *shmPath, bool needNew)
{
    int rc = 0;
    void *p = NULL;

    if (!needNew && !shmPath) {
        errno = RINGBUF_INVALID_PARAM;
        return NULL;
    }
    if (MAP_HUGETLB & flag) {
        totalSz =  ceil((double)totalSz / HUGEPAGE_SIZE) * HUGEPAGE_SIZE;
    }

    if (shmPath) {
        // fprintf(stdout, "RingBuf file backend\n");;
        *fd = open(shmPath, O_CREAT | O_RDWR, 0666);
    } else {
        // fprintf(stdout, "RingBuf file anonymous\n");;
        // MFD_CLOEXEC in #define _GNU_SOURCE or -D_GNU_SOURCE
        *fd = memfd_create("ringBuf", MFD_CLOEXEC);
    }
    if (*fd < 0) {
        return NULL;
    }

    if (needNew) {
        rc = ftruncate(*fd, totalSz);
        if (rc < 0) {
            return NULL;
        }
    } else if (prot & MAP_EXISTS) {
        struct stat st;
        if (fstat(*fd, &st) == -1) {
            return NULL;
        }
        if (st.st_size == 0) {
            errno = RINGBUF_MAPPING_NOT_EXISTS;
            return NULL;
        }
        if ((size_t)st.st_size != totalSz) {
            errno = RINGBUF_MAPPING_SIZE_ERROR;  
            return NULL;
        }
    }

    if (!(flag & MAP_SHARED) && !(flag & MAP_PRIVATE)) {
        flag |= MAP_SHARED;
    }
    p = mmap(NULL, totalSz, PROT_READ | PROT_WRITE, flag, *fd, 0);
    if (p == MAP_FAILED) {
        close(*fd);
        return NULL;
    }

    return p;
}

RingBuf_t *get_buf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag, int useSlot)
{
    /* 物件數量只能是2的冪次才能index到正確的位置 */
    // assert((objNum >= 2) && ((objNum & (objNum - 1)) == 0));
    if (objNum < 2 || ((objNum & (objNum - 1)) != 0)) {
        errno = RINGBUF_CAPACITY_WRONG;
        return NULL;
    }

    if (!(useSlot & (MPSC_SLOT | MPMC_SLOT | NO_SLOT))) {
        errno = RINGBUF_USE_SLOT_NA;
        return NULL;
    }
    SizeInfo_t info = get_total_size(objNum, objSize, useSlot);

    int fd = -1;
    void *p = NULL;

    if (!(prot & (MAP_MALLOC | MAP_SHM))) {
        errno = RINGBUF_NO_MAPPING_TYPE;
        return NULL;
    }

    bool useMalloc = !(prot & (MAP_MALLOC | MAP_SHM)) || (prot & MAP_MALLOC);
    bool useSHM = !useMalloc;
    bool needNew = true;

    if (useMalloc) {
        p = get_buf_malloc(info.total_size, &fd);
    } else if (useSHM) {
        needNew = !(prot & MAP_EXISTS);
        p = get_buf_shm(info.total_size, &fd, prot, flag, shmPath, needNew);
    }

    if (!p) {
        return NULL;
    }

    RingBuf_t *r = p;
    if (needNew) {
        atomic_init(&r->head_, 0);
        // atomic_init(&r->commit_, 0);
        atomic_init(&r->tail_, 0);
        r->objSize_ = info.aligned_objSize;  // Use cache-line aligned size to prevent false sharing
        r->objNum_ = objNum;
        r->mask_ = objNum - 1;
        r->totalSize_ = info.total_size;
        r->mapType_ = useMalloc ? MAP_MALLOC : MAP_SHM;
        r->fd = fd;
        r->buffer_offset_ = info.buf_off_s;
        if (useSlot & MPMC_SLOT) {
            r->slot_offset_ = info.slot_off_s;
            for (size_t i = 0; i < r->objNum_; ++i) {
                // 初始化 slot 為 i，因為初始狀態下，slot i 可供寫入
                atomic_store_explicit(&GET_SLOT(r, i), i, memory_order_release);
            }
        } else if (useSlot & MPSC_SLOT) {
            r->slot_offset_ = info.slot_off_s;
            for (size_t i = 0; i < r->objNum_; ++i) {
                // 初始化 slot 為 SLOT_EMPTY
                atomic_store_explicit(&GET_SLOT(r, i), SLOT_EMPTY, memory_order_release);
            }
        }
        // r->slot_offset_ is not used when NO_SLOT;
    }

    return r;
}

void del_buf(RingBuf_t *r)
{
    if (!r) {
        return;
    }

    if (r->mapType_ == MAP_SHM) {
        close(r->fd);
        munmap(r, r->totalSize_);
    } else if (r->mapType_ == MAP_MALLOC) {
        free(r);
    }
}

BRingBuf_t *get_block_buf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag)
{
    /* 物件數量只能是2的冪次才能index到正確的位置 */
    // assert((objNum >= 2) && ((objNum & (objNum - 1)) == 0));
    if (objNum < 2 || ((objNum & (objNum - 1)) != 0)) {
        errno = RINGBUF_CAPACITY_WRONG;
        return NULL;
    }

    // Align individual object size to cache line to prevent false sharing
    const size_t aligned_objSize = get_aligned_offset(objSize, CACHE_LINE_SIZE);
    size_t totalSize = sizeof(BRingBuf_t) + aligned_objSize * objNum;

    int fd = -1;
    void *p = NULL;

    if (!(prot & (MAP_MALLOC | MAP_SHM))) {
        errno = RINGBUF_NO_MAPPING_TYPE;
        return NULL;
    }

    bool useMalloc = !(prot & (MAP_MALLOC | MAP_SHM)) || (prot & MAP_MALLOC);
    bool useSHM = !useMalloc;
    bool needNew = true;

    if (useMalloc) {
        p = get_buf_malloc(totalSize, &fd);
    } else if (useSHM) {
        needNew = !(prot & MAP_EXISTS);
        p = get_buf_shm(totalSize, &fd, prot, flag, shmPath, needNew);
    }

    if (!p) {
        return NULL;
    }
    BRingBuf_t *r = p;
    if (needNew) {
        int rc = 0;
        if (useSHM) {
            pthread_mutexattr_t mattr;
            pthread_mutexattr_init(&mattr);
            pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
            rc = pthread_mutex_init(&r->mtx, &mattr);
            pthread_mutexattr_destroy(&mattr);
            if (rc != 0) {
                return NULL;
            }

            pthread_condattr_t cattr;
            pthread_condattr_init(&cattr);
            pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
            rc = pthread_cond_init(&r->writeable, &cattr);
            if (rc != 0) {
                pthread_mutex_destroy(&r->mtx);
                pthread_condattr_destroy(&cattr);
                return NULL;
            }
            rc = pthread_cond_init(&r->readable, &cattr);
            pthread_condattr_destroy(&cattr);
            if (rc != 0) {
                pthread_mutex_destroy(&r->mtx);
                pthread_cond_destroy(&r->writeable);
                return NULL;
            }
        } else {
            rc = pthread_mutex_init(&r->mtx, NULL);
            if (rc != 0) {
                return NULL;
            }
            rc = pthread_cond_init(&r->writeable, NULL);
            if (rc != 0) {
                pthread_mutex_destroy(&r->mtx);
                return NULL;
            }
            rc = pthread_cond_init(&r->readable, NULL);
            if (rc != 0) {
                pthread_mutex_destroy(&r->mtx);
                pthread_cond_destroy(&r->writeable);
                return NULL;
            }
        }
        r->head_ = 0;
        r->tail_ = 0;
        r->objSize_ = aligned_objSize;  // Use cache-line aligned size to prevent false sharing
        r->objNum_ = objNum;
        r->mask_ = objNum - 1;
        r->totalSize_ = totalSize;
        r->mapType_ = useMalloc ? MAP_MALLOC : MAP_SHM;
        r->fd = fd;
    }
    return r;
}

void del_block_buf(BRingBuf_t *r)
{
    if (r) {
        pthread_mutex_destroy(&r->mtx);
        pthread_cond_destroy(&r->writeable);
        pthread_cond_destroy(&r->readable);

        if (r->mapType_ == MAP_SHM) {
            close(r->fd);
            munmap(r, r->totalSize_);
        } else if (r->mapType_ == MAP_MALLOC) {
            free(r);
        }
    }
}

/**
 * Explicit error handling wrapper functions
 * These provide a cleaner error handling pattern than relying on errno
 */

int Get_SpscRingBuf_e(SpscRingBuf_t **out, const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag)
{
    if (!out) {
        return RINGBUF_INVALID_PARAM;
    }
    *out = (SpscRingBuf_t *)get_buf(objNum, objSize, shmPath, prot, flag, NO_SLOT);
    if (!*out) {
        return errno;
    }
    return RINGBUF_SUCCESS;
}

int Get_MpscRingBuf_e(MpscRingBuf_t **out, const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag)
{
    if (!out) {
        return RINGBUF_INVALID_PARAM;
    }
    *out = (MpscRingBuf_t *)get_buf(objNum, objSize, shmPath, prot, flag, MPSC_SLOT);
    if (!*out) {
        return errno;
    }
    return RINGBUF_SUCCESS;
}

int Get_MpmcRingBuf_e(MpmcRingBuf_t **out, const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag)
{
    if (!out) {
        return RINGBUF_INVALID_PARAM;
    }
    *out = (MpmcRingBuf_t *)get_buf(objNum, objSize, shmPath, prot, flag, MPMC_SLOT);
    if (!*out) {
        return errno;
    }
    return RINGBUF_SUCCESS;
}

int Get_BlockRingBuf_e(BlockRingBuf_t **out, const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag)
{
    if (!out) {
        return RINGBUF_INVALID_PARAM;
    }
    *out = (BlockRingBuf_t *)get_block_buf(objNum, objSize, shmPath, prot, flag);
    if (!*out) {
        return errno;
    }
    return RINGBUF_SUCCESS;
}