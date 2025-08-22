#include <assert.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "RingBuf_public.h"
#include "RingBuf_private.h"

RingBuf_t *get_buf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag)
{
    /* 物件數量只能是2的冪次才能index到正確的位置 */
    assert((objNum >= 2) && ((objNum & (objNum - 1)) == 0));

    const size_t TOTAL_SIZE = objNum * objSize + sizeof(RingBuf_t);

    int rc = 0;
    int fd = -1;
    void *p = NULL;
    if (prot & MAP_MALLOC) {
        p = malloc(TOTAL_SIZE);
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
            rc = ftruncate(fd, TOTAL_SIZE);
            if (rc < 0) {
                perror("ftruncate");
                return NULL;
            }
        }

        p = mmap(NULL, TOTAL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (p == MAP_FAILED) {
            perror("mmap");
            close(fd);
            return NULL;
        }
    }

    RingBuf_t *r = (RingBuf_t *) p;
    if (prot & MAP_NEW) {
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