#pragma once

/**
 * RingBufWrapper.hpp - C++ RAII wrapper for the RingBuf C library
 *
 * Provides a type-safe, resource-managed C++ interface over the C ring buffer APIs.
 * Supports all four ring buffer types (Spsc, Mpsc, Mpmc, Block) through a
 * unified template class.
 *
 * Overview:
 *   - RingBuf<RingType, Obj, ObjNum> : main class, RAII-managed ring buffer
 *   - RingBufType::Spsc / Mpsc / Mpmc / Block : type tags to select implementation
 *   - Pop_w_cb : flexible callback-based pop, supports any callable signature
 *
 * Basic usage:
 *   RW::RingBuf<RW::RingBufType::Mpsc, MyObj, 64> r;
 *   r.Init("/dev/shm/myring", MAP_SHM | MAP_NEW, 0);
 *
 *   r.Push(myObj);                    // push by value
 *   r.Pop(myObj);                     // pop by value
 *   r.Pop_w_cb(cb, arg);              // pop via callback (zero-copy)
 *
 * Callback rules for Pop_w_cb (see Pop_w_cb for full details):
 *   - fast path  : stateless lambda / function pointer with signature int(const void*, void*)
 *   - trampoline : any other callable (with capture, or different arg types)
 *
 * Thread safety:
 *   Follows the underlying C library guarantees:
 *   - Spsc : one producer thread, one consumer thread
 *   - Mpsc : multiple producer threads, one consumer thread
 *   - Mpmc : multiple producer threads, multiple consumer threads
 *   - Block : multiple producers, multiple consumers, blocking on full/empty
 *
 * Error handling:
 *   Init() returns a negative RINGBUF_* error code on failure.
 *   Use Get_RingBuf_strerror() to get a human-readable description.
 *
 * Notes:
 *   - ObjNum must be a power of two and >= 2
 *   - The ring buffer struct lives in shared memory; the RingBuf object itself
 *     holds a std::shared_ptr that calls the appropriate destructor on last copy
 *   - Do NOT pass the raw pointer obtained from Get_RingBuf() across process
 *     boundaries; Init() with MAP_EXIST in the other process instead
 */

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

/**
 * my_assert - Debug assertion macro with file/line/function context
 *
 * Evaluates @expr and, if false, prints a formatted error message to stderr
 * then calls std::abort().  The message includes the stringified expression,
 * source file, line number, enclosing function name, and a user-supplied
 * printf-style format string.
 *
 * @param expr boolean expression to test
 * @param fmt  printf-style format string for the failure message
 * @param ...  optional arguments for @fmt
 *
 * Example:
 *   my_assert(ptr != nullptr, "allocation failed, size=%zu", requested);
 */
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

/**
 * RingBufType - Tag types used to select the ring buffer implementation
 *
 * Pass one of the nested structs as the first template argument to RingBuf<>.
 *
 *   Spsc  : Single Producer, Single Consumer
 *   Mpsc  : Multiple Producers, Single Consumer
 *   Mpmc  : Multiple Producers, Multiple Consumers (slot-based, producers non-blocking)
 *   Block : Multiple Producers, Multiple Consumers, blocking on full/empty
 *
 * Example:
 *   RingBuf<RingBufType::Mpsc, MyObj, 32> r;
 */
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

/**
 * RingBufTypeTrait - Internal trait that maps a RingBufType tag to the
 *                    corresponding C API types and functions.
 *
 * Not intended for direct use. RingBuf<> inherits from the appropriate
 * specialisation to gain access to the C functions.
 *
 * Specialisations exist for:
 *   RingBufType::Spsc  -> SpscRingBuf_t
 *   RingBufType::Mpsc  -> MpscRingBuf_t
 *   RingBufType::Mpmc  -> MpmcRingBuf_t
 *   RingBufType::Block -> BlockRingBuf_t
 */
template <typename RingTypeTag>
struct RingBufTypeTrait;

template <>
struct RingBufTypeTrait<RingBufType::Spsc>
{
    using type = SpscRingBuf_t;

    static int GetRing(type **r, const size_t n, const size_t objSize, const char *path, int prot, int flag)
    {
        return Get_SpscRingBuf_e(r, n, objSize, path, prot, flag);
    }

    static void DelRing(type *r) { Del_SpscRingBuf(r); }

    static ssize_t Push(type *r, void *data, size_t len) { return Push_SpscRingBuf(r, data, len); }

    static ssize_t Pop(type *r, void *out, size_t size) { return Pop_SpscRingBuf(r, out, size); }

    static void *BeginPush(type *r) { return Begin_push_SpscRingBuf(r); }

    static void EndPush(type *r) { return End_push_SpscRingBuf(r); }

    static void *BeginPop(type *r) { return Begin_pop_SpscRingBuf(r); }

    static void EndPop(type *r) { End_pop_SpscRingBuf(r); }
};

template <>
struct RingBufTypeTrait<RingBufType::Mpsc>
{
    using type = MpscRingBuf_t;

    static int GetRing(type **r, const size_t n, const size_t objSize, const char *path, int prot, int flag)
    {
        return Get_MpscRingBuf_e(r, n, objSize, path, prot, flag);
    }

    static void DelRing(type *r) { Del_MpscRingBuf(r); }

    static ssize_t Push(type *r, void *data, size_t len) { return Push_MpscRingBuf(r, data, len); }

    static ssize_t Pop(type *r, void *out, size_t size) { return Pop_MpscRingBuf(r, out, size); }

    static int Pop_w_cb(type *r, Pop_cb cb, void *args) { return Pop_w_cb_MpscRingBuf(r, cb, args); }
};

template <>
struct RingBufTypeTrait<RingBufType::Mpmc>
{
    using type = MpmcRingBuf_t;

    static int GetRing(type **r, const size_t n, const size_t objSize, const char *path, int prot, int flag)
    {
        return Get_MpmcRingBuf_e(r, n, objSize, path, prot, flag);
    }

    static void DelRing(type *r) { Del_MpmcRingBuf(r); }

    static ssize_t Push(type *r, void *data, size_t len) { return Push_MpmcRingBuf(r, data, len); }

    static ssize_t Pop(type *r, void *out, size_t size) { return Pop_MpmcRingBuf(r, out, size); }

    static int Pop_w_cb(type *r, Pop_cb cb, void *args) { return Pop_w_cb_MpmcRingBuf(r, cb, args); }
};

template <>
struct RingBufTypeTrait<RingBufType::Block>
{
    using type = BlockRingBuf_t;

    static int GetRing(type **r, const size_t n, const size_t objSize, const char *path, int prot, int flag)
    {
        return Get_BlockRingBuf_e(r, n, objSize, path, prot, flag);
    }

    static void DelRing(type *r) { Del_BlockRingBuf(r); }

    static ssize_t Push(type *r, void *data, size_t len) { return Push_BlockRingBuf(r, data, len); }

    static ssize_t Pop(type *r, void *out, size_t size) { return Pop_BlockRingBuf(r, out, size); }
};

/**
 * is_one_of_v<T, Options...> - True if T is the same type as any type in Options
 *
 * Helper used internally to dispatch on RingBufType tags at compile time.
 *
 * Example:
 *   is_one_of_v<RingBufType::Mpsc, RingBufType::Mpsc, RingBufType::Mpmc>  // true
 *   is_one_of_v<RingBufType::Spsc, RingBufType::Mpsc, RingBufType::Mpmc>  // false
 */
template <typename T, typename... Options>
inline constexpr bool is_one_of_v = (std::is_same_v<T, Options> || ...);

/**
 * RingBuf - RAII C++ wrapper for the RingBuf C library
 *
 * @tparam RingType one of RingBufType::Spsc / Mpsc / Mpmc / Block
 * @tparam Obj      the element type stored in the ring buffer
 * @tparam ObjNum   capacity (must be a power of two, >= 2)
 *
 * The underlying C ring buffer is allocated/mapped by Init() and released
 * automatically when the last copy of the RingBuf object is destroyed
 * (reference-counted via std::shared_ptr).
 *
 * Constraints:
 *   - ObjNum must satisfy: ObjNum >= 2 && (ObjNum & (ObjNum-1)) == 0
 *   - Only valid RingType tags are accepted (static_assert on others)
 *
 * Copyable / movable: yes (shared ownership of the underlying ring buffer)
 */
template <typename RingType, typename Obj, size_t ObjNum>
class RingBuf final : private RingBufTypeTrait<RingType>
{
    static_assert(is_one_of_v<RingType, RingBufType::Spsc, RingBufType::Mpsc, RingBufType::Mpmc, RingBufType::Block>,
                  "RingType wrong");
    static_assert((ObjNum >= 2) && ((ObjNum & (ObjNum - 1)) == 0), "ObjNum need to be power of 2");

    using Base = RingBufTypeTrait<RingType>;

public:
    /**
     * Get_RingBuf - Return the raw pointer to the underlying C ring buffer
     *
     * Useful when calling C APIs directly that are not exposed by this wrapper.
     *
     * @return pointer to the ring buffer struct, nullptr if Init() has not
     *         been called or failed
     */
    typename Base::type *Get_RingBuf() const noexcept { return r_; }

    /**
     * Push - Push an object into the ring buffer by value (or moved)
     *
     * Accepts any value category of Obj (lvalue, rvalue, const).
     * The object is copied into the ring buffer's internal slot.
     *
     * @param obj object to push; must be exactly of type Obj (enforced by static_assert)
     *
     * @return slot index where data was written (>= 0), or negative RINGBUF_* error code
     *         Common errors: RINGBUF_FULL
     */
    template <typename T>
    ssize_t Push(T &&obj) noexcept
    {
        static_assert(std::is_same_v<std::decay_t<T>, Obj>, "Push incorrect object type");
        return Base::Push(r_, const_cast<void *>(static_cast<const void *>(&obj)), sizeof(Obj));
    }

    /**
     * Push - Push raw memory into the ring buffer
     *
     * Low-level overload for pushing an arbitrary memory region.
     * @size must not exceed the base object size the ring buffer was created with.
     *
     * @param data pointer to source data
     * @param size number of bytes to copy
     *
     * @return slot index (>= 0), or negative RINGBUF_* error code
     *         Common errors: RINGBUF_FULL, RINGBUF_PUSH_SIZE_TOO_LARGE
     */
    ssize_t Push(void *data, size_t size) { return Base::Push(r_, data, size); }

    /**
     * Pop - Pop an object from the ring buffer by value
     *
     * Copies the front element into @obj and advances the consumer index.
     *
     * @param obj output parameter; must be exactly of type Obj (enforced by static_assert)
     *
     * @return slot index where data was read (>= 0), or negative RINGBUF_* error code
     *         Common errors: RINGBUF_EMPTY
     */
    template <typename T>
    ssize_t Pop(T &obj) noexcept
    {
        static_assert(std::is_same_v<std::decay_t<T>, Obj>, "Pop incorrect object type");
        return Base::Pop(r_, static_cast<void *>(&obj), sizeof(Obj));
    }

    /**
     * Pop_w_cb - Pop and process an element via a callback (zero-copy pop)
     *
     * Invokes @callback with a pointer directly into the ring buffer slot,
     * avoiding an extra memory copy compared to Pop().
     *
     * Two internal paths are selected at compile time:
     *
     *   fast path  (is_fast_path == true):
     *     Conditions:
     *       1. Exactly one extra argument (@args) is provided
     *       2. That argument is convertible to void*
     *       3. @callback is convertible to Pop_cb (int(*)(const void*, void*))
     *            - i.e. a stateless lambda or plain function pointer
     *     Behaviour:
     *       - For Mpsc/Mpmc: delegates directly to the C Pop_w_cb_* function
     *       - For Spsc: calls Begin_pop / callback / End_pop inline
     *       - For Block: pops into a local copy, then invokes callback
     *
     *   trampoline path (is_fast_path == false):
     *     Used for all other callables:
     *       - Lambdas with captures [&] / [=]
     *       - Function pointers whose first argument is void* (non-const)
     *       - Callables with signatures other than int(const void*, void*)
     *     Behaviour:
     *       A C-compatible trampoline lambda wraps the user callable so it
     *       can be passed to the underlying C API.  The packed @args tuple and
     *       a reference to @callback are stored on the stack (no heap alloc).
     *       The trampoline always passes the buffer pointer as the FIRST argument
     *       to @callback, followed by the unpacked @args.
     *
     * @param callback any callable.  The first argument received inside the callback
     *                 will always be a pointer (void* or const void*) to the ring
     *                 buffer slot.  Additional arguments come from @args.
     * @param args     zero or more extra arguments forwarded to @callback after the
     *                 buffer pointer.
     *
     * Supported callback signatures (✅ compiles, ❌ does not compile):
     *   ✅ int(const void *p, void *args)        — stateless: fast path
     *   ✅ int(const void *p, void *args)        — with capture: trampoline
     *   ✅ int(void *p, void *args)              — trampoline
     *   ✅ int(const void *p)                    — trampoline, no extra args
     *   ✅ int(void *p)                          — trampoline, no extra args
     *   ❌ int(Obj *p, int n, double d, ...)     — void* cannot implicitly convert to Obj*
     *   ❌ int(Obj *p)                           — void* cannot implicitly convert to Obj*
     *   ❌ void(const void *p)                   — return type must be convertible to int
     *   ❌ int()                                 — trampoline always passes at least one arg, which is callback
     *
     * @return value returned by @callback, or negative RINGBUF_* error code
     *         if the ring buffer is empty / contention
     *
     * Warning:
     *   @callback is called synchronously on the same thread.  Do NOT enqueue
     *   the context into a thread pool; the TrampolineContext lives on the stack
     *   and will dangle if accessed after this function returns.
     *
     * Example (fast path):
     *   r.Pop_w_cb([](const void *p, void *args) noexcept -> int {
     *       process(static_cast<const MyObj *>(p));
     *       return 0;
     *   }, nullptr);
     *
     * Example (trampoline, multiple extra args):
     *   r.Pop_w_cb([](MyObj *obj, int threshold, std::string &out) -> int {
     *       if (obj->value > threshold) out = obj->name;
     *       return 0;
     *   }, threshold, out);
     */
    template <typename R = RingType, typename Callable, typename... Args>
    ssize_t Pop_w_cb(Callable &&callback, Args &&...args)
    {
        /** 
         * 如果Args是空的，tuple_element_t<0, tuple<>> 是 ill-formed
         * tuple內沒有元素，所以在確認is_fast_path時會編譯失敗
         * 所以用conditional_t來包裝，當Args是空的時候，就會退化成tuple<void *>
         * 這樣tuple_element_t<0, tuple<void *>> 就是void *，不會編譯失敗 
         */
        using _SafeFirstArg = std::tuple_element_t<
            0, std::conditional_t<(sizeof...(Args) >= 1), std::tuple<Args...>, std::tuple<void *>>>;
        constexpr auto is_fast_path = (sizeof...(Args) == 1) &&
                                      std::is_convertible_v<_SafeFirstArg, void *> &&
                                      std::is_convertible_v<Callable, Pop_cb>;
        /* 只有一個參數，且參數是void *，且cb是int(const void *, void *)，才會進來 */
        if constexpr (is_fast_path) {
            auto &first_arg = std::get<0>(std::forward_as_tuple(args...));
            if constexpr (is_one_of_v<R, RingBufType::Mpsc, RingBufType::Mpmc>) {
                return Base::Pop_w_cb(r_, callback, first_arg);
            } else if constexpr (std::is_same_v<R, RingBufType::Block>) {
                Obj obj{};
                int rc = Base::Pop(r_, reinterpret_cast<void *>(&obj), sizeof(Obj));
                if (rc < 0) {
                    return rc;
                }
                return callback(reinterpret_cast<void *>(&obj), first_arg);
            } else { // Spsc
                void *p = Base::BeginPop(r_);
                int rc = callback(p, first_arg);
                Base::EndPop(r_);
                return rc;
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

            auto trampoline = [](const void *obj_in_buf, void *user_context_ptr) -> int {
                auto *ctx = static_cast<TrampolineContext *>(user_context_ptr);
                /** 
                 * 除了傳進來的Args會被打包成tuple傳進callback以外
                 * 還有一個obj_in_buf需要當作第一個參數傳進去callback
                 * 所以要用std::invoke來呼叫才能帶入obj_in_buf跟Args裡面的參數
                 * 如果單純用std::apply，參數只能放兩個，第一個callback，第二個tuple
                 * 所以callback第一個參數一定是void *，後面隨意
                 */
                return std::apply(
                    [&](auto &&...unpacked_args) {
                        return std::invoke(ctx->cpp_callback, const_cast<void *>(obj_in_buf), unpacked_args...);
                    },
                    ctx->user_args);
            };

            // 不能丟到thread pool裡面，否則context會dangling reference
            if constexpr (is_one_of_v<R, RingBufType::Mpsc, RingBufType::Mpmc>) {
                return Base::Pop_w_cb(r_, trampoline, &context);
            } else if constexpr (std::is_same_v<R, RingBufType::Block>) {
                Obj obj{};
                int rc = Base::Pop(r_, reinterpret_cast<void *>(&obj), sizeof(Obj));
                if (rc < 0) {
                    return rc;
                }
                return trampoline(reinterpret_cast<void *>(&obj), &context);
            } else { // Spsc
                auto *p = static_cast<Obj *>(Base::BeginPop(r_));
                if (!p) {
                    return RINGBUF_EMPTY;
                }
                int rc = trampoline(p, &context);
                Base::EndPop(r_);
                return rc;
            }
        }
    }

    /**
     * Get_RingBuf_strerror - Return a human-readable description of a RINGBUF_* error code
     *
     * @param err negative error code returned by Init(), Push(), Pop(), or Pop_w_cb()
     *
     * @return a null-terminated string describing the error;
     *         never nullptr (unknown codes return a generic message)
     *
     * Example:
     *   int rc = r.Init(path, MAP_SHM | MAP_NEW, 0);
     *   if (rc < 0) fprintf(stderr, "%s\n", r.Get_RingBuf_strerror(rc));
     */
    const char *Get_RingBuf_strerror(int err) const noexcept { return RingBuf_strerror(err); }

    /**
     * Init - Allocate or map the ring buffer
     *
     * Must be called before any Push/Pop operation.
     * On success the internal shared_ptr is set up so that the last copy of
     * this RingBuf object will automatically release the ring buffer.
     *
     * @param shmPath path used as the file-backed shared memory name (e.g. "/dev/shm/myring")
     *                Pass nullptr to use anonymous (non-shared) memory.
     * @param prot    OR of RingBufMappingType flags
     *                  MAP_MALLOC  — allocate with malloc (single process only)
     *                  MAP_SHM     — file-backed shared memory (cross-process)
     *                  MAP_NEW     — create a new mapping (fails if already exists)
     *                  MAP_EXIST   — attach to an existing mapping (fails if not found)
     * @param flag    additional mmap flags passed through (e.g. MAP_SHARED, MAP_POPULATE)
     *                Pass 0 for defaults.
     *
     * @return RINGBUF_SUCCESS (0) on success, or a negative RINGBUF_* error code
     *         Common errors:
     *           RINGBUF_CAPACITY_WRONG      — ObjNum is not a power of two
     *           RINGBUF_MAPPING_NOT_EXITS   — MAP_EXIST but path does not exist
     *           RINGBUF_MAPPING_SIZE_ERROR  — MAP_EXIST but size mismatch
     *           RINGBUF_INVALID_PARAM       — nullptr or invalid arguments
     *
     * Example:
     *   RingBuf<RingBufType::Mpsc, MyObj, 64> r;
     *   int rc = r.Init("/dev/shm/myring", MAP_SHM | MAP_NEW, 0);
     *   if (rc < 0) { fprintf(stderr, "%s\n", r.Get_RingBuf_strerror(rc)); }
     */
    int Init(const char *shmPath, int prot, int flag)
    {
        int rc = Base::GetRing(&r_, ObjNum, sizeof(Obj), shmPath, prot, flag);
        p_ = std::shared_ptr<typename Base::type>(r_, dter{});
        return rc;
    }

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
