#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include "RingBuf_public.h"

#define my_assert(expr, fmt, ...)                                                                                \
    do {                                                                                                         \
        if (!(expr)) {                                                                                           \
            std::fprintf(stderr, "Assertion failed: (%s)\nFile: %s, Line: %d, Function: %s\nMessage: " fmt "\n", \
                         #expr, __FILE__, __LINE__, __func__, ##__VA_ARGS__);                                    \
            std::abort();                                                                                        \
        }                                                                                                        \
    } while (0)

namespace RingBufWrapper
{

struct RingBufType
{
    struct Spsc
    {
    };

    struct Mpsc
    {
    };

    struct Mpmc
    {
    };

    struct Block
    {
    };
};

template <typename RingTypeTag>
struct RingBufTypeTrait;

template <>
struct RingBufTypeTrait<RingBufType::Spsc>
{
    using type = SpscRingBuf_t;

    static type *GetRing(const size_t n, const size_t objSize, const char *path, int prot, int flag)
    {
        return Get_SpscRingBuf(n, objSize, path, prot, flag);
    }

    static void DelRing(type *r) { Del_SpscRingBuf(r); }

    static ssize_t Push(type *r, void *data, size_t len) { return Push_SpscRingBuf(r, data, len); }

    static ssize_t Pop(type *r, void *out) { return Pop_SpscRingBuf(r, out); }

    static void *BeginPop(type *r) { return Begin_pop_SpscRingBuf(r); }

    static void EndPop(type *r) { End_pop_SpscRingBuf(r); }
};

template <>
struct RingBufTypeTrait<RingBufType::Mpsc>
{
    using type = MpscRingBuf_t;

    static type *GetRing(const size_t n, const size_t objSize, const char *path, int prot, int flag)
    {
        return Get_MpscRingBuf(n, objSize, path, prot, flag);
    }

    static void DelRing(type *r) { Del_MpscRingBuf(r); }

    static ssize_t Push(type *r, void *data, size_t len) { return Push_MpscRingBuf(r, data, len); }

    static ssize_t Pop(type *r, void *out) { return Pop_MpscRingBuf(r, out); }

    static int Pop_w_cb(type *r, Pop_cb cb, void *args) { return Pop_w_cb_MpscRingBuf(r, cb, args); }
};

template <>
struct RingBufTypeTrait<RingBufType::Mpmc>
{
    using type = MpmcRingBuf_t;

    static type *GetRing(const size_t n, const size_t objSize, const char *path, int prot, int flag)
    {
        return Get_MpmcRingBuf(n, objSize, path, prot, flag);
    }

    static void DelRing(type *r) { Del_MpmcRingBuf(r); }

    static ssize_t Push(type *r, void *data, size_t len) { return Push_MpmcRingBuf(r, data, len); }

    static int Pop_w_cb(type *r, Pop_cb cb, void *args) { return Pop_w_cb_MpmcRingBuf(r, cb, args); }
};

template <>
struct RingBufTypeTrait<RingBufType::Block>
{
    using type = BlockRingBuf_t;

    static type *GetRing(const size_t n, const size_t objSize, const char *path, int prot, int flag)
    {
        return Get_BlockRingBuf(n, objSize, path, prot, flag);
    }

    static void DelRing(type *r) { Del_BlockRingBuf(r); }

    static ssize_t Push(type *r, void *data, size_t len) { return Push_BlockRingBuf(r, data, len); }

    static ssize_t Pop(type *r, void *out) { return Pop_BlockRingBuf(r, out); }
};

template <typename T, typename... Options>
inline constexpr bool is_one_of_v = (std::is_same_v<T, Options> || ...);

template <typename RingType, typename Obj, size_t ObjNum>
class RingBuf final : private RingBufTypeTrait<RingType>
{
    static_assert(is_one_of_v<RingType, RingBufType::Spsc, RingBufType::Mpsc, RingBufType::Mpmc, RingBufType::Block>,
                  "RingType wrong");
    static_assert((ObjNum >= 2) && ((ObjNum & (ObjNum - 1)) == 0), "ObjNum need to be power of 2");

    using Base = RingBufTypeTrait<RingType>;

public:
    typename Base::type *Get_RingBuf() const noexcept { return r_; }

    template <typename T>
    ssize_t Push(T &&obj) noexcept
    {
        static_assert(std::is_same_v<std::decay_t<T>, Obj>, "Push incorrect object type");
        return Base::Push(r_, const_cast<void *>(static_cast<const void *>(&obj)), sizeof(Obj));
    }

    ssize_t Push(void *data, size_t size) { return Base::Push(r_, data, size); }

    template <typename T>
    ssize_t Pop(T &obj) noexcept
    {
        static_assert(std::is_same_v<std::decay_t<T>, Obj>, "Pop incorrect object type");
        return Base::Pop(r_, static_cast<void *>(&obj));
    }

    // Callable-based pop with trampoline (requires synchronous callback invocation by C layer)
    template <typename R = RingType, typename Callable, typename... Args>
    ssize_t Pop_w_cb(Callable &&callback, Args &&...args)
    {
        constexpr auto is_fast_path = (sizeof...(Args) == 1) &&
                                      std::is_convertible_v<std::tuple_element_t<0, std::tuple<Args...>>, void *> &&
                                      std::is_convertible_v<Callable, Pop_cb>;
        /* 只有一個參數，且參數是void *，且cb是int(void *, void *)，才會進來 */
        if constexpr (is_fast_path) {
            auto &first_arg = std::get<0>(std::forward_as_tuple(args...));
            if constexpr (is_one_of_v<R, RingBufType::Mpsc, RingBufType::Mpmc>) {
                return Base::Pop_w_cb(r_, callback, first_arg);
            } else if constexpr (std::is_same_v<R, RingBufType::Spsc>) {
                void *p = Base::BeginPop(r_);
                int rc = callback(p, first_arg);
                Base::EndPop(r_);
                return rc;
            } else if constexpr (std::is_same_v<R, RingBufType::Block>) {
                Obj obj{};
                int rc = Base::Pop(r_, reinterpret_cast<void *>(&obj));
                if (rc < 0) {
                    return rc;
                }
                return callback(reinterpret_cast<void *>(&obj), first_arg);
            } else {
                return -1;
            }
        } else {
            auto user_args_tuple = std::forward_as_tuple(std::forward<Args>(args)...);

            using CbRef = std::add_lvalue_reference_t<Callable>;

            struct TrampolineContext
            {
                CbRef cpp_callback;
                decltype(user_args_tuple) &user_args;
            };

            TrampolineContext context{callback, user_args_tuple};

            auto trampoline = [](void *obj_in_buf, void *user_context_ptr) -> int {
                auto *ctx = static_cast<TrampolineContext *>(user_context_ptr);
                /* 因為除了傳進來的參數會被打包成tuple以外，還有一個obj_in_buf需要傳進去callback，所以要用std::invoke來呼叫才能帶入obj_in_buf
                 * 如果單純用std::apply，參數只能放兩個，第一個callback，第二個tuple，所以callback宣告的第一個一定是void *，後面隨意
                **/
                return std::apply(
                    [&](auto &&...unpacked_args) {
                        return std::invoke(ctx->cpp_callback, static_cast<Obj *>(obj_in_buf), unpacked_args...);
                    },
                    ctx->user_args);
            };

            // 不能丟到thread pool裡面，否則context會dangling reference
            if constexpr (is_one_of_v<R, RingBufType::Mpsc, RingBufType::Mpmc>) {
                return Base::Pop_w_cb(r_, trampoline, &context);
            } else {  // Spsc
                auto *p = static_cast<Obj *>(Base::BeginPop(r_));
                int rc = trampoline(p, &context);
                Base::EndPop(r_);
                return rc;
            }
        }
    }

    const char *Get_RingBuf_strerror(int err) const noexcept { return RingBuf_strerror(err); }

    int Init(const char *shmPath, int prot, int flag)
    {
        r_ = Base::GetRing(ObjNum, sizeof(Obj), shmPath, prot, flag);
        p_ = std::shared_ptr<typename Base::type>(r_, dter{});
        return (r_ ? 0 : -1);
    }

    explicit RingBuf(const char *shmPath, int prot, int flag) { Init(shmPath, prot, flag); }

    RingBuf() = default;
    ~RingBuf() = default;
    RingBuf(const RingBuf &) = default;
    RingBuf &operator=(const RingBuf &) = default;
    RingBuf(RingBuf &&) = default;
    RingBuf &operator=(RingBuf &&) = default;

private:
    typename Base::type *r_ = nullptr;
    std::shared_ptr<typename Base::type> p_ = nullptr;

    struct dter
    {
        void operator()(typename Base::type *r) const noexcept { Base::DelRing(r); }
    };
};

}  // namespace RingBufWrapper
