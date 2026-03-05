#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

/**
 * cpu_relax() - Yield CPU pipeline resources in spin-wait loops
 *
 * Usage:
 *   Call inside busy-wait loops to notify the CPU that the thread is spinning.
 *   x86/x64 issues the PAUSE instruction; ARM issues the YIELD instruction; other platforms are no-op.
 *
 * When to use:
 *   1. When retrying Push/Pop due to RINGBUF_FULL / RINGBUF_EMPTY / RINGBUF_CONTENTION
 *   2. Any while(!done) spin loop where sleep is not needed but you want to reduce CPU power and pipeline contention
 *
 * When NOT to use:
 *   1. For longer waits (> several microseconds), use sched_yield() or nanosleep() to yield the thread
 *   2. If you do not care about power or CPU usage and require ultra-low latency, you may omit (pure empty loop)
 *
 * Effects:
 *   - x86 PAUSE: Hints to CPU this is a spin-wait, reduces pipeline flushes due to memory order violations,
 *                lowers power, and gives more resources to the sibling HyperThread
 *   - ARM YIELD: Similarly hints to CPU to yield resources to another thread on the same core
 */
#if defined(__x86_64__) || defined(__i386__)
#define cpu_relax() __asm__ __volatile__("pause" ::: "memory")
#elif defined(__aarch64__) || defined(__arm__)
#define cpu_relax() __asm__ __volatile__("yield" ::: "memory")
#else
#define cpu_relax() do {} while (0)
#endif

/**
 * RingBuf specific return values
 * Negative values indicate errors, positive values indicate success with additional info
 */
#define RINGBUF_SUCCESS              0   /**< Operation successful */
#define RINGBUF_FULL                -100 /**< Ring buffer is full */
#define RINGBUF_EMPTY               -101 /**< Ring buffer is empty */
#define RINGBUF_CONTENTION          -103 /**< High contention, retry suggested */
#define RINGBUF_INVALID_PARAM       -104 /**< Invalid parameters */
#define RINGBUF_NO_MAPPING_TYPE     -105 /**< No mapping type specified */
#define RINGBUF_CAPACITY_WRONG      -106 /**< Capacity is not the power of two */
#define RINGBUF_MAPPING_NOT_EXISTS  -107 /**< Use MAP_EXISTS but memory mapping does not exist */
#define RINGBUF_MAPPING_SIZE_ERROR  -108 /**< Memory mapping size mismatch */
#define RINGBUF_PUSH_SIZE_TOO_LARGE -109 /**< Push size exceeded base obj size */
#define RINGBUF_POP_SIZE_TOO_LARGE  -110 /**< Pop size exceeded base obj size */
#define RINGBUF_USE_SLOT_NA         -111 /**< Argument of get_buf of useSlot is not defined */
#define RINGBUF_SLOT_WRITING_DATA   -112 /**< Producer is writing slot */
#define RINGBUF_SLOT_STAT_UNKNOWN   -113 /**< Ring buffer slot data unknown, probably won't happen */

/**
 * Types of ring buffer
 * Underlying implementation is similar except for the blocking ring buffer
 * Each type has its own corresponding API functions
 * 
 * Spsc: single producer, single consumer
 * Mpsc: multi producer, single consumer
 * Mpmc: multi producer, multi consumer
 * Block: blocking ring buffer, multi producer, multi consumer
 * 
 * @warning Slow producers will block consumer reads.
 */
typedef struct _SpscRingBuf SpscRingBuf_t;
typedef struct _MpscRingBuf MpscRingBuf_t;
typedef struct _MpmcRingBuf MpmcRingBuf_t;
typedef struct _BlockRingBuf_t BlockRingBuf_t;

/**
 * Determine where the ring buffer located at 
 * Used to specify the memory allocation/mapping method for the ring buffer
 */
enum RingBufMappingType {
    MAP_MALLOC  = 1 << 24, /**< Default, using malloc to allocate ring buffer */
    MAP_SHM     = 1 << 25, /**< Mapping ring buffer onto shared memory */
    MAP_NEW     = 1 << 26, /**< Default, mapping to new chunk of memory */
    MAP_EXISTS  = 1 << 27, /**< Mapping to an existing shared memory */
};

/**
 * Callback for callback type functions
 * Used in callback-based pop functions to avoid extra memory copy
 *
 * @param p pointer to the memory location inside the ring buffer (the popped object)
 *         DO NOT TRY TO MODIFY ITS CONTENTS 
 * @param args user-defined arguments for use inside the callback
 */
typedef int(*Pop_cb)(const void *p, void *args);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return a human-readable description of a RINGBUF_* error code
 *
 * @param error_code negative error code returned by any RingBuf API function
 * @return null-terminated string describing the error; never NULL
 */
const char* RingBuf_strerror(int error_code);

/**
 * ERROR HANDLING PATTERN
 *
 * The library uses negative error codes for initialization failures.
 * Use the explicit error functions below to get error details:
 *
 * Example:
 *   SpscRingBuf_t *r;
 *   int err = Get_SpscRingBuf_e(&r, objNum, objSize, path, prot, flag);
 *   if (err != RINGBUF_SUCCESS) {
 *       fprintf(stderr, "Error: %s\n", RingBuf_strerror(err));
 *       return err;
 *   }
 */

/**
 * Spsc functions
 */

/**
 * Generate Spsc ring buffer (implicit error via errno - legacy)
 *
 * @param objNum number of objs can be placed into ring buffer
 * @param objSize size of obj instance (will be automatically padded to 64-byte
 *            cache line boundary to prevent false sharing in multi-threaded scenarios)
 * @param shmPath path for file backend shared memory,
 *            will map to anonymous if not given
 * @param prot bitwise OR of RingBufMappingType values
 * @param flag The same as mmap, MAP_SHARED, MAP_PRIVATE, MAP_POPULATED...
 *
 * @return the addr of ring buffer, NULL on error (check errno or use Get_SpscRingBuf_e)
 */
SpscRingBuf_t *Get_SpscRingBuf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag);

/**
 * Generate Spsc ring buffer (explicit error handling)
 *
 * @param out output parameter to store ring buffer pointer
 * @param objNum number of objs can be placed into ring buffer
 * @param objSize size of obj instance (will be automatically padded to 64-byte
 *            cache line boundary to prevent false sharing in multi-threaded scenarios)
 * @param shmPath path for file backend shared memory,
 *            will map to anonymous if not given
 * @param prot bitwise OR of RingBufMappingType values
 * @param flag The same as mmap, MAP_SHARED, MAP_PRIVATE, MAP_POPULATED...
 *
 * @return error code (RINGBUF_SUCCESS or negative error code)
 * On success, *out will contain ring buffer address
 * On error, *out will be NULL and return value indicates error
 */
int Get_SpscRingBuf_e(SpscRingBuf_t **out, const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag);

/**
 * Destructor for ring buffer
 * 
 * @param p addr of ring buffer
 */
void Del_SpscRingBuf(SpscRingBuf_t *p);

/**
 * Get the addr that can be written
 * 
 * @param p addr of ring buffer
 * 
 * @return the addr of available mem in ring buffer, which is ready to be written, return null if full
 */
void *Begin_push_SpscRingBuf(SpscRingBuf_t *p);

/**
 * Followed up by Begin_push when finished writing.
 * Signals to the consumer that the slot is ready to be read.
 *
 * @param p addr of ring buffer
 */
void End_push_SpscRingBuf(SpscRingBuf_t *p);

/**
 * Get the addr that can be read
 * 
 * @param p addr of ring buffer
 * 
 * @return the addr of available mem in ring buffer, which is ready to be read, return null if empty
 */
void *Begin_pop_SpscRingBuf(SpscRingBuf_t *p);

/**
 * Followed up by Begin_pop when finished reading.
 * Signals to the producer that the slot is ready to be written.
 *
 * @param p addr of ring buffer
 */
void End_pop_SpscRingBuf(SpscRingBuf_t *p);

#ifndef DEBUG
/**
 * Push memory into ring buffer
 * 
 * @param p addr of ring buffer
 * @param args obj that need to write into ring buffer
 * @param size size of memory to push; must not exceed the base object size of ring buffer
 * 
 * @return the head index where data was pushed(>= 0), or RINGBUF_* error code if error
 */
ssize_t Push_SpscRingBuf(SpscRingBuf_t *p, void *args, size_t size);
#endif

/**
 * Pop memory from ring buffer
 * 
 * @param p addr of ring buffer
 * @param buf buffer to store data in the ring buffer
 * @param size size of buf, must not exceed the base obj size of ring buffer
 * 
 * @return the tail index where data was popped(>= 0), or RINGBUF_* error code if error
 */
ssize_t Pop_SpscRingBuf(SpscRingBuf_t *p, void *buf, size_t size);

bool Is_empty_SpscRingBuf(SpscRingBuf_t *p);
bool Is_full_SpscRingBuf(SpscRingBuf_t *p);
size_t Capacity_SpscRingBuf(SpscRingBuf_t *p);
size_t Size_SpscRingBuf(SpscRingBuf_t *p);

/**
 * Mpsc functions
 */

/**
 * Generate Mpsc ring buffer
 *
 * @param objNum number of objs can be placed into ring buffer
 * @param objSize size of obj instance (will be automatically padded to 64-byte
 *            cache line boundary to prevent false sharing in multi-threaded scenarios)
 * @param shmPath path for file backend shared memory,
 *            will map to anonymous if not given
 * @param prot bitwise OR of RingBufMappingType values
 * @param flag The same as mmap, MAP_SHARED, MAP_PRIVATE, MAP_POPULATED...
 *
 * @return the addr of ring buffer, NULL on error (check errno or use Get_MpscRingBuf_e)
 */
MpscRingBuf_t *Get_MpscRingBuf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag);

/**
 * Generate Mpsc ring buffer (explicit error handling)
 *
 * @param out output parameter to store ring buffer pointer
 * @param objNum number of objs can be placed into ring buffer
 * @param objSize size of obj instance
 * @param shmPath path for file backend shared memory,
 *            will map to anonymous if not given
 * @param prot bitwise OR of RingBufMappingType values
 * @param flag The same as mmap, MAP_SHARED, MAP_PRIVATE, MAP_POPULATED...
 *
 * @return error code (RINGBUF_SUCCESS or negative error code)
 * On success, *out will contain ring buffer address
 * On error, *out will be NULL and return value indicates error
 */
int Get_MpscRingBuf_e(MpscRingBuf_t **out, const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag);

/**
 * Destructor for ring buffer
 * @param p addr of ring buffer
 */
void Del_MpscRingBuf(MpscRingBuf_t *p);

#ifndef DEBUG
/**
 * Push memory into ring buffer
 * 
 * @param p addr of ring buffer
 * @param args obj that need to write into ring buffer
 * @param size size of memory to push; must not exceed the base object size of ring buffer
 * 
 * @return the head index where data was pushed(>= 0), or RINGBUF_* error code if error
 */
ssize_t Push_MpscRingBuf(MpscRingBuf_t *p, void *args, size_t size);
#endif

/**
 * Pop memory from ring buffer
 * 
 * @param p addr of ring buffer
 * @param buf buffer to store data in the ring buffer
 * @param size size of buf, must not exceed the base obj size of ring buffer
 * 
 * @return the tail index where data was popped(>= 0), or RINGBUF_* error code if error
 */
ssize_t Pop_MpscRingBuf(MpscRingBuf_t *p, void *buf, size_t size);

/**
 * Execute callback immediately after getting the available slot in ring buffer.
 * Reduces one memory copy compared to Pop.
 *
 * @param p    addr of ring buffer
 * @param cb   callback to execute; receives a pointer to the slot and @p args
 * @param args user-defined argument forwarded to @p cb
 * @return value returned by @p cb, or RINGBUF_* error code if error
 */
int Pop_w_cb_MpscRingBuf(MpscRingBuf_t *p, Pop_cb cb, void *args);

bool Is_empty_MpscRingBuf(MpscRingBuf_t *p);
bool Is_full_MpscRingBuf(MpscRingBuf_t *p);
size_t Capacity_MpscRingBuf(MpscRingBuf_t *p);
size_t Size_MpscRingBuf(MpscRingBuf_t *p);

/**
 * Mpmc functions
 * Implement by using slot array
 * Use this if you don't want producers blocking each other
 */

/**
 * Generate Mpmc ring buffer
 *
 * @param objNum number of objs can be placed into ring buffer
 * @param objSize size of obj instance (will be automatically padded to 64-byte
 *            cache line boundary to prevent false sharing in multi-threaded scenarios)
 * @param shmPath path for file backend shared memory,
 *            will map to anonymous if not given
 * @param prot bitwise OR of RingBufMappingType values
 * @param flag The same as mmap, MAP_SHARED, MAP_PRIVATE, MAP_POPULATED...
 *
 * @return the addr of ring buffer, NULL on error (check errno or use Get_MpmcRingBuf_e)
 */
MpmcRingBuf_t *Get_MpmcRingBuf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag);

/**
 * Generate Mpmc ring buffer (explicit error handling)
 *
 * @param out output parameter to store ring buffer pointer
 * @param objNum number of objs can be placed into ring buffer
 * @param objSize size of obj instance
 * @param shmPath path for file backend shared memory,
 *            will map to anonymous if not given
 * @param prot bitwise OR of RingBufMappingType values
 * @param flag The same as mmap, MAP_SHARED, MAP_PRIVATE, MAP_POPULATED...
 *
 * @return error code (RINGBUF_SUCCESS or negative error code)
 * On success, *out will contain ring buffer address
 * On error, *out will be NULL and return value indicates error
 */
int Get_MpmcRingBuf_e(MpmcRingBuf_t **out, const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag);

/**
 * Destructor for ring buffer
 * @param p addr of ring buffer
 */
void Del_MpmcRingBuf(MpmcRingBuf_t *p);

#ifndef DEBUG
/**
 * Push memory into ring buffer, producers don't block from each other
 * 
 * @param p addr of ring buffer
 * @param args obj that need to write into ring buffer
 * @param size size of memory to push; must not exceed the base object size of ring buffer
 * 
 * @return the head index where data was pushed(>= 0), or RINGBUF_* error code if error
 */
ssize_t Push_MpmcRingBuf(MpmcRingBuf_t *p, void *args, size_t size);
#endif

/**
 * Pop memory from ring buffer
 * 
 * @param p addr of ring buffer
 * @param buf buffer to store data in the ring buffer
 * @param size size of buf, must not exceed the base obj size of ring buffer
 * 
 * @return the tail index where data was popped(>= 0), or RINGBUF_* error code if error
 */
ssize_t Pop_MpmcRingBuf(MpmcRingBuf_t *p, void *buf, size_t size);

/**
 * Execute callback immediately after getting the available slot in ring buffer.
 * Reduces one memory copy compared to Pop.
 *
 * @param p    addr of ring buffer
 * @param cb   callback to execute; receives a pointer to the slot and @p args
 * @param args user-defined argument forwarded to @p cb
 *
 * @return value returned by @p cb, or RINGBUF_* error code if error
 */
int Pop_w_cb_MpmcRingBuf(MpmcRingBuf_t *p, Pop_cb cb, void *args);

/**
 * Block functions
 */

/**
 * Generate Blocking ring buffer
 *
 * @param objNum number of objs can be placed into ring buffer
 * @param objSize size of obj instance (will be automatically padded to 64-byte
 *            cache line boundary to prevent false sharing in multi-threaded scenarios)
 * @param shmPath path for file backend shared memory,
 *            will map to anonymous if not given
 * @param prot bitwise OR of RingBufMappingType values
 * @param flag The same as mmap, MAP_SHARED, MAP_PRIVATE, MAP_POPULATED...
 *
 * @return the addr of ring buffer, NULL on error (check errno or use Get_BlockRingBuf_e)
 */
BlockRingBuf_t *Get_BlockRingBuf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag);

/**
 * Generate Blocking ring buffer (explicit error handling)
 *
 * @param out output parameter to store ring buffer pointer
 * @param objNum number of objs can be placed into ring buffer
 * @param objSize size of obj instance
 * @param shmPath path for file backend shared memory,
 *            will map to anonymous if not given
 * @param prot bitwise OR of RingBufMappingType values
 * @param flag The same as mmap, MAP_SHARED, MAP_PRIVATE, MAP_POPULATED...
 *
 * @return error code (RINGBUF_SUCCESS or negative error code)
 * On success, *out will contain ring buffer address
 * On error, *out will be NULL and return value indicates error
 */
int Get_BlockRingBuf_e(BlockRingBuf_t **out, const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag);

/**
 * Destructor for ring buffer
 * @param r addr of ring buffer
 */
void Del_BlockRingBuf(BlockRingBuf_t *r);

#ifndef DEBUG
/**
 * Push memory into ring buffer
 * 
 * Blocking waits if full
 * 
 * @param p addr of ring buffer
 * @param args obj that need to write into ring buffer
 * @param size size of memory to push; must not exceed the base object size of ring buffer
 * 
 * @return the head index where data was pushed(>= 0), or RINGBUF_* error code if error
 */
ssize_t Push_BlockRingBuf(BlockRingBuf_t *p, void *args, size_t size);
#endif

/**
 * Pop memory from ring buffer
 * 
 * Blocking waits if empty
 * 
 * @param p addr of ring buffer
 * @param buf buffer to store data in the ring buffer
 * @param size size of buf, must not exceed the base obj size of ring buffer
 * 
 * @return the tail index where data was popped(>= 0), or RINGBUF_* error code if error
 */
ssize_t Pop_BlockRingBuf(BlockRingBuf_t *p, void *buf, size_t size);

/**
 * Pop a chunk of memory from ring buffer
 * 
 * @note Not yet implemented
 *
 * @param p addr of ring buffer
 * @param buf buffer to store data in the ring buffer
 * @param max_num max number to store into buffer
 *
 * @return the number of objs popped (> 0), or RINGBUF_* error code if error
 */
ssize_t Batch_pop_BlockRingBuf(BlockRingBuf_t *p, void *buf, size_t max_num);

#ifdef __cplusplus
}
#endif
