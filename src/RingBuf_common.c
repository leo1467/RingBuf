#include <assert.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
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

RingBuf_t *get_buf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag, int useSlot)
{
    /* 物件數量只能是2的冪次才能index到正確的位置 */
    assert((objNum >= 2) && ((objNum & (objNum - 1)) == 0));

    SizeInfo_t info = get_total_size(objNum, objSize, useSlot);

    int rc = 0;
    int fd = -1;
    void *p = NULL;
    if (prot & MAP_MALLOC) {
        p = malloc(info.total_size);
        if (!p) {
            perror("malloc");
            return NULL;
        }
        fd = -999;
    }
    else {
        if (shmPath) {
            fd = open(shmPath, O_CREAT | O_RDWR, 0666);
        } else {
            // MFD_CLOEXEC in #define _GNU_SOURCE or -D_GNU_SOURCE
            fd = memfd_create("ringBuf", MFD_CLOEXEC);
        }
        if (fd < 0) {
            perror("memfd_create");
            return NULL;
        }

        if (prot & MAP_NEW) {
            rc = ftruncate(fd, info.total_size);
            if (rc < 0) {
                perror("ftruncate");
                return NULL;
            }
        }

        p = mmap(NULL, info.total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (p == MAP_FAILED) {
            perror("mmap");
            close(fd);
            return NULL;
        }
    }

    RingBuf_t *r = p;
    if (prot & MAP_NEW) {
        r->buffer_ = (char *)p + info.buf_off_s;
        if (useSlot & USE_SLOT) {
            r->slot_ = (atomic_size_t *)((char *)p + info.slot_off_s);
        } else if (useSlot & NO_SLOT) {
            r->slot_ = NULL;
        }

        r->objNum_ = objNum;
        r->mask_ = objNum - 1;
        r->objSize_ = objSize;
        r->fd = fd;
        atomic_init(&r->head_, 0);
        atomic_init(&r->tail_, 0);
        atomic_init(&r->commit_, 0);
    }

    return r;
}

void del_buf(RingBuf_t *r)
{
    if (r->fd >= 0) {
        close(r->fd);
    }
    if (r->fd >= 0 && r) {
        munmap(r, r->objNum_ * r->objSize_ + sizeof(RingBuf_t));
    } else if (r->fd == -999) {
        free(r);
    }
}