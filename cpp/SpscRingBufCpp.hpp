#pragma once
#include "SpscRingBuf.h"

#include <string>

namespace ShmRingBufWrapper {

template<class T, size_t ObjNum>
class SpscRingBuf {
public:
    SpscRingBuf_t *Get_RingBuf() const
    {
        return r;
    }

    T* Begin_push() const
    {
        if (unlikely(!r)) {
            return nullptr;
        }
        return reinterpret_cast<T *>(begin_push(r));
    }

    void End_push() const
    {
        if (unlikely(!r)) {
            return;
        }
        end_push(r);
    }

    T* Begin_pop() const
    {
        if (unlikely(!r)) {
            return nullptr;
        }
        return reinterpret_cast<T *>(begin_pop(r));
    }

    void End_pop() const
    {
        if (unlikely(!r)) {
            return;
        }
        end_pop(r);
    }

    explicit SpscRingBuf(const char *shmPath) noexcept : property{-1, nullptr}, r(nullptr)
    {
        property = Get_shm_ringBuf(ObjNum, sizeof(T), shmPath);
        r = reinterpret_cast<SpscRingBuf_t *>(property.bufAddr);
    }

    ~SpscRingBuf()
    {
        Del_shm_ringBuf(property);
    }

    SpscRingBuf(const SpscRingBuf &other) = delete;

    SpscRingBuf &operator=(const SpscRingBuf &other) = delete;

    SpscRingBuf(SpscRingBuf &&other) noexcept
    {
        property = other.property;
        r = other.r;
        other.property.bufAddr = nullptr;
        other.property.fd = -1;
        other.r = nullptr;
    }
    SpscRingBuf &operator=(SpscRingBuf &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        property = other.property;
        r = other.r;
        other.property.bufAddr = nullptr;
        other.property.fd = -1;
        other.r = nullptr;
        return *this;
    }
private:
    SpscRingProperty property;
    SpscRingBuf_t *r;
};
} // ShmRingBufWrapper
