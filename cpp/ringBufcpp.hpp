#pragma once
#include "ringBuf.h"

#include <string>

namespace RingBufWrapper {

template<class T, size_t ObjNum>
class RingBuf {
public:
    SpscRing *Get_RingBuf() const
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

    explicit RingBuf(const char *shmPath) noexcept : property{-1, nullptr}, r(nullptr)
    {
        property = Get_shm_ringBuf(ObjNum, sizeof(T), shmPath);
        r = reinterpret_cast<SpscRing *>(property.bufAddr);
    }

    ~RingBuf()
    {
        Del_shm_ringBuf(property);
    }

    RingBuf(const RingBuf &other) = delete;

    RingBuf &operator=(const RingBuf &other) = delete;

    RingBuf(RingBuf &&other) noexcept
    {
        property = other.property;
        r = other.r;
        other.property.bufAddr = nullptr;
        other.property.fd = -1;
        other.r = nullptr;
    }
    RingBuf &operator=(RingBuf &&other) noexcept
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
    SpscRing *r;
};
} // RingBufWrapper
