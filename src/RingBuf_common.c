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
        case RINGBUF_MAPPING_NOT_EXITS:     return "Use MAP_EXIST but memory mapping does not exist";
        case RINGBUF_MAPPING_SIZE_ERROR:    return "Mapping size mismatch";
        case RINGBUF_PUSH_SIZE_TOO_LARGE:   return "Push size exceeded base obj size";
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
    size_t total_size = 0;
    size_t buffer_offset_start = 0;
    size_t buffer_offset_end = 0;
    size_t slot_offset_start = 0;
    size_t slot_offset_end = 0;
    buffer_offset_start = get_aligned_offset(sizeof(RingBuf_t), CACHE_LINE_SIZE);
    buffer_offset_end = get_aligned_offset(buffer_offset_start + objSize * objNum, CACHE_LINE_SIZE);
    if (useSlot & USE_SLOT) {
        slot_offset_start = get_aligned_offset(buffer_offset_end, CACHE_LINE_SIZE);
        slot_offset_end = get_aligned_offset(slot_offset_start + sizeof(atomic_size_t) * objNum, CACHE_LINE_SIZE);
        total_size = slot_offset_end;
    } else if (useSlot & NO_SLOT) {
        total_size = buffer_offset_end;;
    }

    return (SizeInfo_t){.buf_off_s  = buffer_offset_start, 
                        .buf_off_e  = buffer_offset_end,
                        .slot_off_s = slot_offset_start,
                        .slot_off_e = slot_offset_end,
                        .total_size = total_size};
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
    } else if (prot & MAP_EXIST) {
        struct stat st;
        if (fstat(*fd, &st) == -1) {
            return NULL;
        }
        if (st.st_size == 0) {
            errno = RINGBUF_MAPPING_NOT_EXITS;
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
    if (objNum < 2 && ((objNum & (objNum - 1)) != 0)) {
        errno = RINGBUF_CAPACITY_WRONG;
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
        needNew = !(prot & MAP_EXIST);
        p = get_buf_shm(info.total_size, &fd, prot, flag, shmPath, needNew);
    }

    if (!p) {
        return NULL;
    }

    RingBuf_t *r = p;
    if (needNew) {
        atomic_init(&r->head_, 0);
        atomic_init(&r->commit_, 0);
        atomic_init(&r->tail_, 0);
        r->objSize_ = objSize;
        r->objNum_ = objNum;
        r->mask_ = objNum - 1;
        r->totalSize_ = info.total_size;
        r->mapType_ = useMalloc ? MAP_MALLOC : MAP_SHM;
        r->fd = fd;
        r->buffer_offset_ = info.buf_off_s;
        if (useSlot & USE_SLOT) {
            r->slot_offset_ = info.slot_off_s;
            for (size_t i = 0; i < r->objNum_; ++i) {
                // 初始化 slot 為 i，因為初始狀態下，slot i 可供寫入
                atomic_store_explicit(&GET_SLOT(r)[i], i, memory_order_release);
            }
        } else if (useSlot & NO_SLOT) {
            // r->slot_offset_ is not used when NO_SLOT;
        }
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

BRingBuf_t *get_blocked_buf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag)
{
    /* 物件數量只能是2的冪次才能index到正確的位置 */
    // assert((objNum >= 2) && ((objNum & (objNum - 1)) == 0));
    if (objNum < 2 && ((objNum & (objNum - 1)) != 0)) {
        errno = RINGBUF_CAPACITY_WRONG;
        return NULL;
    }

    size_t totalSize = sizeof(BRingBuf_t) + objNum * objSize;

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
        needNew = !(prot & MAP_EXIST);
        p = get_buf_shm(totalSize, &fd, prot, flag, shmPath, needNew);
    }

    if (!p) {
        return NULL;
    }
    BRingBuf_t *r = p;
    if (needNew) {
        if (useSHM) {
            pthread_mutexattr_t mattr;
            pthread_mutexattr_init(&mattr);
            pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
            pthread_mutex_init(&r->mtx, &mattr);
            pthread_mutexattr_destroy(&mattr);

            pthread_condattr_t cattr;
            pthread_condattr_init(&cattr);
            pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
            pthread_cond_init(&r->writeable, &cattr);
            pthread_cond_init(&r->readable, &cattr);
            pthread_condattr_destroy(&cattr);
        } else {
            pthread_mutex_init(&r->mtx, NULL);
            pthread_cond_init(&r->writeable, NULL);
            pthread_cond_init(&r->readable, NULL);
        }
        r->head_ = 0;
        r->tail_ = 0;
        r->objSize_ = objSize;
        r->objNum_ = objNum;
        r->mask_ = objNum - 1;
        r->totalSize_ = totalSize;
        r->mapType_ = useMalloc ? MAP_MALLOC : MAP_SHM;
        r->fd = fd;
    }
    return r;
}

void del_blocked_buf(BRingBuf_t *r)
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