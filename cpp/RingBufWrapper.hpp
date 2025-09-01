#pragma once
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>

#include "RingBuf_public.h"

#define my_assert(expr, fmt, ...) \
    do { \
        if (!expr) { \
            fprintf(stderr, \
                        "Assertion failed: (%s)\nFile: %s, Line: %d, Function: %s\n" "Message: " fmt "\n", \
                        #expr, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
            abort(); \
        } \
    } while (0)

using namespace std::placeholders;

namespace RingBufWrapper {

struct RingBufType {
    struct Spsc {};
    struct Mpsc {};
    struct Mpmc {};
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
class RingBufTypeTrait<RingBufType::Mpsc> {
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
        = std::bind(Try_push_MpscRingBuf, _1, _2);
    inline static std::function<int(MpscRingBuf_t*, Pop_cb, void *args)> Pop_w_cb
        = std::bind(Pop_w_cb_MpscRingBuf, _1, _2, _3);
};

template<> 
class RingBufTypeTrait<RingBufType::Mpmc> {
protected:
    using type = MpmcRingBuf_t;
    inline static std::function<MpmcRingBuf_t *(const size_t, const size_t, const char *, int, int)> GetRing 
        = std::bind(&Get_MpmcRingBuf, _1, _2, _3, _4, _5);
    inline static std::function<void(MpmcRingBuf_t*)> DelRing
        = std::bind(Del_MpmcRingBuf, _1);
    inline static std::function<size_t(MpmcRingBuf_t*, void *)> Push
        = std::bind(Try_push_MpmcRingBuf, _1, _2);
    inline static std::function<size_t(MpmcRingBuf_t*, void *)> Pop
        = std::bind(Try_pop_MpmcMpscRingBuf, _1, _2);
    inline static std::function<int(MpmcRingBuf_t*, Pop_cb, void *args)> Pop_w_cb
        = std::bind(Pop_w_cb_MpmcRingBuf, _1, _2, _3);
};

template<>
class RingBufTypeTrait<RingBufType::Blocked> {
protected:
    using type = BlockedRingBuf_t;
};

template<typename T, typename... Options>
inline constexpr bool is_one_of_v = (std::is_same_v<T, Options> || ...);

template<typename RingType, typename Obj, size_t ObjNum>
class RingBuf final : private RingBufTypeTrait<RingType> {
    using Base = RingBufTypeTrait<RingType>;
public:
    typename Base::type *Get_RingBuf() const noexcept
    {
        return r;
    }

    template<typename T>
    size_t Push(T &&obj) noexcept
    {
        static_assert(std::is_same_v<std::decay_t<T>, Obj>, "Push incorrect object type");
        return Base::Push(r, reinterpret_cast<void *>(&obj));
    }

    template<typename T>
    size_t Pop(T &obj) noexcept
    {
        return Base::Pop(r, reinterpret_cast<void *>(&obj));
    }

    template<typename R = RingType, typename Callable, typename... Args, std::enable_if_t<!std::is_function_v<std::remove_pointer_t<std::decay_t<Callable>>>, int> = 0>
    int Pop_w_cb(Callable &&callback, Args&&... args) noexcept(noexcept(std::invoke(std::forward<Callable>(callback), std::declval<Obj*>(), std::forward<Args>(args)...)))
    {
        auto user_args_tuple = std::make_tuple(std::forward<Args>(args)...);
        struct TrampolineContext {
            Callable &cpp_callback;
            decltype(user_args_tuple) &user_args;
        };

        TrampolineContext context = {callback, user_args_tuple};
        auto trampoline = [](void *obj_in_buf, void *user_context_ptr) -> int {
            TrampolineContext *ctx = static_cast<TrampolineContext *>(user_context_ptr);
            int rc = std::apply([&](auto &&... unpacked_args) { return std::invoke(ctx->cpp_callback, obj_in_buf, unpacked_args...); }, ctx->user_args);
            return rc;
        };

        if constexpr (is_one_of_v<RingType, RingBufType::Mpsc, RingBufType::Mpmc>) {
            return Base::Pop_w_cb(r, trampoline, &context);
        } else {
            Obj *p = reinterpret_cast<Obj *>(Begin_pop_SpscRingBuf(r));
            int rc = trampoline(p, &context);
            End_pop_SpscRingBuf(r); 
            return rc;
        }
    }

    int Pop_w_cb(Pop_cb cb, void *args) noexcept
    {
        if constexpr (is_one_of_v<RingType, RingBufType::Mpsc, RingBufType::Mpmc>) {
            return Base::Pop_w_cb(r, cb, args);
        } else {
            void *p = Begin_pop_SpscRingBuf(r);
            int rc = cb(p, args);
            End_pop_SpscRingBuf(r); 
            return rc;
        }
    }

    template<typename R = RingType>
    auto Pop_MpmcMpscRingBuf(Obj &obj) noexcept -> std::enable_if_t<std::is_same_v<R, RingBufType::Mpmc>, size_t>
    {
        return Try_pop_MpmcMpscRingBuf(r, reinterpret_cast<void *>(&obj));
    }

    int Init(const char *shmPath, int prot, int flag)
    {
        static_assert((ObjNum >= 2) && ((ObjNum & (ObjNum - 1)) == 0), "ObjNum need to be power of 2");
        static_assert(is_one_of_v<RingType, RingBufType::Spsc, RingBufType::Mpsc, RingBufType::Mpmc>, "RingType wrong");
        r = Base::GetRing(ObjNum, sizeof(Obj), shmPath, prot, flag);
        my_assert(r, "Ring Buf nullptr, check prot");
        p = std::shared_ptr<typename Base::type>(r, dter());

        return 0;
    }

    explicit RingBuf(const char *shmPath, int prot, int flag)
    {
        Init(shmPath, prot, flag);
    }

    RingBuf() = default;
    ~RingBuf() = default;
    RingBuf(const RingBuf &other) = default;
    RingBuf &operator=(const RingBuf &other) = default;
    RingBuf(RingBuf &&other) = default;
    RingBuf &operator=(RingBuf &&other) = default;
private:
    typename Base::type *r = nullptr;
    std::shared_ptr<typename Base::type> p = nullptr;
    struct dter {
        void operator()(typename Base::type *r)
        {
            Base::DelRing(r);
        }
    };
};
} // RingBufWrapper
