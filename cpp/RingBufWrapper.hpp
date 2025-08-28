#pragma once
#include <functional>
#include <memory>

#include "RingBuf_public.h"

#define my_assert(expr, fmt, ...) \
    do { \
        if (!expr) { \
            fprintf(stderr, \
                        "Assertione failed: (%s)\nFile: %s, Line: %d, Function: %s\n" "Message: " fmt "\n", \
                        #expr, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
            abort(); \
        } \
    } while (0)

using namespace std::placeholders;

namespace RingBufWrapper {

struct RingBufType {
    struct Spsc {};
    struct Commit {};
    struct Slot {};
    struct Blocked {};
};

template<typename RingTypeTag> 
class RingBufTypeTrait {
};

template<> 
class RingBufTypeTrait<RingBufType::Spsc> {
protected:
    using type = SpscRingBuf_t;
    inline static std::function<SpscRingBuf_t *(const size_t, const size_t, const char *, int, int)> GetRing 
        = std::bind(Get_SpscRingBuf, _1, _2, _3, _4, _5);
    inline static std::function<void(SpscRingBuf_t*)> DelRing
        = std::bind(Del_SpscRingBuf, _1);
    inline static std::function<size_t(SpscRingBuf_t*, void *)> Push
        = std::bind(Push_SpscRingBuf, _1, _2);
    inline static std::function<size_t(SpscRingBuf_t*, void *)> Pop
        = std::bind(Pop_SpscRingBuf, _1, _2);
};

template<> 
class RingBufTypeTrait<RingBufType::Commit> {
protected:
    using type = MpscRingBuf_t;
    inline static std::function<MpscRingBuf_t *(const size_t, const size_t, const char *, int, int)> GetRing 
        = std::bind(&Get_MpscRingBuf, _1, _2, _3, _4, _5);
    inline static std::function<void(MpscRingBuf_t*)> DelRing
        = std::bind(Del_MpscRingBuf, _1);
    inline static std::function<size_t(MpscRingBuf_t*, void *)> Push
        = std::bind(Push_MpscRingBuf, _1, _2);
    inline static std::function<size_t(MpscRingBuf_t*, void *)> Pop
        = std::bind(Pop_MpscRingBuf, _1, _2);
    inline static std::function<size_t(MpscRingBuf_t*, void *)> Try_Push
        = std::bind(Try_Push_MpscRingBuf, _1, _2);
};

template<> 
class RingBufTypeTrait<RingBufType::Slot> {
protected:
    using type = SlotRingBuf_t;
    inline static std::function<SlotRingBuf_t *(const size_t, const size_t, const char *, int, int)> GetRing 
        = std::bind(&Get_SlotRingBuf, _1, _2, _3, _4, _5);
    inline static std::function<void(SlotRingBuf_t*)> DelRing
        = std::bind(Del_SlotRingBuf, _1);
    inline static std::function<size_t(SlotRingBuf_t*, void *)> Push
        = std::bind(Try_Push_SlotRingBuf, _1, _2);
    inline static std::function<size_t(SlotRingBuf_t*, void *)> Pop
        = std::bind(Try_Pop_SlotMpscRingBuf, _1, _2);
};

template<>
class RingBufTypeTrait<RingBufType::Blocked> {
protected:
    using type = BlockedRingBuf_t;
};

template<typename RingType, typename Obj, size_t ObjNum>
class RingBuf final : private RingBufTypeTrait<RingType> {
    using Base = RingBufTypeTrait<RingType>;
public:
    typename Base::type *Get_RingBuf() const noexcept
    {
        return r;
    }

    size_t Push(Obj &obj) const noexcept
    {
        return Base::Push(r, reinterpret_cast<void *>(&obj));
    }

    size_t Push(Obj &&obj) const noexcept
    {
        return Base::Push(r, reinterpret_cast<void *>(&obj));
    }

    size_t Pop(Obj &obj) const noexcept
    {
        return Base::Pop(r, reinterpret_cast<void *>(&obj));
    }

    template<typename R = RingType>
    auto Pop_SlotMpscRingBuf(Obj &obj) const noexcept -> std::enable_if_t<std::is_same_v<R, RingBufType::Slot>, size_t>
    {
        return Try_Pop_SlotMpscRingBuf(r, reinterpret_cast<void *>(&obj));
    }

    template<typename R = RingType>
    auto Begin_push() const noexcept -> std::enable_if_t<std::is_same_v<R, RingBufType::Spsc>, Obj*>
    {
        return reinterpret_cast<Obj *>(Begin_push_SpscRingBuf(r));
    }

    template<typename R = RingType>
    auto End_push() const noexcept -> std::enable_if_t<std::is_same_v<R, RingBufType::Spsc>, void>
    {
        End_push_SpscRingBuf(r);
    }

    template<typename R = RingType>
    auto Begin_pop() const noexcept -> std::enable_if_t<std::is_same_v<R, RingBufType::Spsc>, Obj*>
    {
        return reinterpret_cast<Obj *>(Begin_pop_SpscRingBuf(r));
    }

    template<typename R = RingType>
    auto End_pop() const noexcept -> std::enable_if_t<std::is_same_v<R, RingBufType::Spsc>, void>
    {
        End_pop_SpscRingBuf(r);
    }

    explicit RingBuf(const char *shmPath, int prot, int flag) : r(nullptr), p(nullptr)
    {
        static_assert((ObjNum >= 2) && ((ObjNum & (ObjNum - 1)) == 0), "ObjNum need to be power of 2");
        r = Base::GetRing(ObjNum, sizeof(Obj), shmPath, prot, flag);
        my_assert(r, "Ring Buf nullptr, check prot");
        p = std::shared_ptr<typename Base::type>(r, dter());
    }

    ~RingBuf() = default;

    RingBuf(const RingBuf &other)
    {
        r = other.r;
        p = other.p;
    }

    RingBuf &operator=(const RingBuf &other)
    {
        r = other.r;
        p = other.p;
        return *this;
    }

    RingBuf(RingBuf &&other) noexcept
    {
        r = other.r;
        other.r = nullptr;
        p = other.p;
        other.p = nullptr;
    }

    RingBuf &operator=(RingBuf &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        r = other.r;
        other.r = nullptr;
        p = other.p;
        other.p = nullptr;
        return *this;
    }
private:
    typename Base::type *r;
    std::shared_ptr<typename Base::type> p;
    struct dter {
        void operator()(typename Base::type *r)
        {
            Base::DelRing(r);
        }
    };
};
} // RingBufWrapper
