#pragma once
#include "ShmSpscRingBuf.h"

#include <string>

namespace ShmRingBufWrapper {

template<class T, size_t ObjNum>
class SpscRingBuf {
public:
    ShmSpscRingBuf_t *Get_RingBuf() const
    {
        return r;
    }

    T* Begin_push() const
    {
        if (unlikely(!r)) {
            return nullptr;
        }
        return reinterpret_cast<T *>(Begin_push_shmSpscRingBuf(r));
    }

    void End_push() const
    {
        if (unlikely(!r)) {
            return;
        }
        End_push_shmSpscRingBuf(r);
    }

    T* Begin_pop() const
    {
        if (unlikely(!r)) {
            return nullptr;
        }
        return reinterpret_cast<T *>(Begin_pop_shmSpscRingBuf(r));
    }

    void End_pop() const
    {
        if (unlikely(!r)) {
            return;
        }
        End_pop_shmSpscRingBuf(r);
    }

    explicit SpscRingBuf(const char *shmPath) noexcept : property{-1, nullptr}, r(nullptr)
    {
        property = Get_shmSpscRingBuf(ObjNum, sizeof(T), shmPath);
        r = reinterpret_cast<ShmSpscRingBuf_t *>(property.bufAddr);
    }

    ~SpscRingBuf()
    {
        Del_shmSpscRingBuf(property);
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
    SpscRingProperty_t property;
    ShmSpscRingBuf_t *r;
};
} // ShmRingBufWrapper
