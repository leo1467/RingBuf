#include <assert.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
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
        perror("malloc");
        return NULL;
    }
    *fd = -999;

    return p;
}

static void *get_buf_shm(size_t totalSz, int *fd, int prot, const char *shmPath, bool needNew)
{
    int rc = 0;
    void *p = NULL;

    if (!needNew && !shmPath) {
        fprintf(stderr, "MAP_EXIST need shm path\n");
        return NULL;
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
        perror("open or memfd_create");
        return NULL;
    }

    if (needNew) {
        rc = ftruncate(*fd, totalSz);
        if (rc < 0) {
            perror("ftruncate");
            return NULL;
        }
    } else if (prot & MAP_EXIST) {
        struct stat st;
        if (fstat(*fd, &st) == -1) {
            perror("fstat");
            return NULL;
        }
        if (st.st_size < 0) {
            fprintf(stderr, "struct stat::st_size < 0\n");
            return NULL;
        }
        if ((size_t)st.st_size != totalSz) {
            fprintf(stderr, "producer not start yet\n");
            return NULL;
        }
    }

    p = mmap(NULL, totalSz, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        close(*fd);
        return NULL;
    }

    return p;
}

RingBuf_t *get_buf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag, int useSlot)
{
    /* 物件數量只能是2的冪次才能index到正確的位置 */
    assert((objNum >= 2) && ((objNum & (objNum - 1)) == 0));

    SizeInfo_t info = get_total_size(objNum, objSize, useSlot);

    int fd = -1;
    void *p = NULL;

    if (!(prot & (MAP_MALLOC | MAP_SHM))) {
        fprintf(stderr, "Prot need MAP_SHM or MAP_MALLOC\n");
        return NULL;
    }

    bool useMalloc = !(prot & (MAP_MALLOC | MAP_SHM)) || (prot & MAP_MALLOC);
    bool useSHM = !useMalloc;
    bool needNew = true;

    if (useMalloc) {
        p = get_buf_malloc(info.total_size, &fd);
    } else if (useSHM) {
        needNew = !(prot & MAP_EXIST);
        p = get_buf_shm(info.total_size, &fd, prot, shmPath, needNew);
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
        r->fd = fd;
        r->buffer_ = (char *)p + info.buf_off_s;
        if (useSlot & USE_SLOT) {
            r->slot_ = (atomic_size_t *)((char *)p + info.slot_off_s);
            for (size_t i = 0; i < r->objNum_; ++i) {
                // 初始化 slot 為 i，因為初始狀態下，slot i 可供寫入
                atomic_store_explicit(&r->slot_[i], i, memory_order_release);
            }
        } else if (useSlot & NO_SLOT) {
            r->slot_ = NULL;
        }
    }

    return r;
}

void del_buf(RingBuf_t *r)
{
    if (!r) {
        return;
    }

    if (r->fd >= 0) {
        close(r->fd);
    }

    if (r->fd >= 0 && r) {
        munmap(r, r->totalSize_);
    } else if (r->fd == -999) {
        free(r);
    }
}