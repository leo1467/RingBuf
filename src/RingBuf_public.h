#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

/* Platform-specific CPU pause/yield for spin-wait loops */
#if defined(__x86_64__) || defined(__i386__)
/* Use inline assembly for better AMD compatibility */
#define cpu_relax() __asm__ __volatile__("pause" ::: "memory")
#elif defined(__aarch64__) || defined(__arm__)
/* ARM yield instruction for spin-wait loops */
#define cpu_relax() __asm__ __volatile__("yield" ::: "memory")
#else
#define cpu_relax() do {} while (0)
#endif

#if DEBUG
typedef struct Obj_ {
    uint64_t magH;
    char buf[1024];
    uint64_t magT;
    uint64_t seq;
} __attribute__((__aligned__(64))) __attribute__((__packed__)) Obj ;

typedef struct Time_diff_ {
    struct timespec s;
    char pad1[64 - sizeof(struct timespec)];
    struct timespec e;
    char pad2[64 - sizeof(struct timespec)];
} __attribute__((aligned(64))) Time_diff_t;
typedef void (*testFunc)(Time_diff_t *arr, size_t pushed, char buf[], Obj *o);
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
#define RINGBUF_MAPPING_NOT_EXITS   -107 /**< Use MAP_EXIST but memory mapping does not exist */
#define RINGBUF_MAPPING_SIZE_ERROR  -108 /**< Memory mapping size mismatch */
#define RINGBUF_PUSH_SIZE_TOO_LARGE -109 /**< Push size exceeded base obj size */
#define RINGBUF_USE_SLOT_NA         -110 /**< Argument of get_buf of useSlot is not defined */
#define RINGBUF_SLOT_WRITING_DATA   -111 /**< Producer is writing slot */
#define RINGBUF_SLOT_STAT_UNKNOWN   -112 /**< Ring buffer slot data unknown, probably won't happen */

/**
 * Types of ring buffer
 * Underlying implementation is similar except for the blocked ring buffer (Not yet implementeded)
 * Each type has its own corresponding API functions
 * 
 * Spsc: single producer, single consumer
 * Mpsc: multi producer, single consumer
 * Mpmc: multi producer, multi consumer
 * Blocked: blocking ring buffer, Not yet implemented
 */
typedef struct _SpscRingBuf SpscRingBuf_t;
typedef struct _MpscRingBuf MpscRingBuf_t;
typedef struct _MpmcRingBuf MpmcRingBuf_t;
typedef struct _BlockedRingBuf BlockedRingBuf_t;

/**
 * Determine where the ring buffer located at 
 * Used to specify the memory allocation/mapping method for the ring buffer
 */
enum RingBufMappingType {
    MAP_MALLOC  = 1 << 24, /**< Default, using malloc to allocate ring buffer */
    MAP_SHM     = 1 << 25, /**< Mapping ring buffer onto shared memory */
    MAP_NEW     = 1 << 26, /**< Default, mapping to new chunk of memory */
    MAP_EXIST   = 1 << 27, /**< Mapping to a existing shared memory */
};

/**
 * Callback for callback type functions
 * Used in callback-based pop functions to avoid extra memory copy
 */
typedef int(*Pop_cb)(void *obj, void *args);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Helper function to get error description
 */
const char* RingBuf_strerror(int error_code);

/**
 * Spsc functions
 */

/**
 * Generate Spsc ring buffer
 * 
 * @objNum : number of objs can be placed into ring buffer
 * @objSize : size of obj instance
 * @shmPath : path for file backend shared memory, 
 *            will map to anonymous if not given
 * @prot : Oring RingBufMappingType
 * @flag : The same as mmap, MAP_SHARED, MAP_PRIVATE, MAP_POPULATED...
 * 
 * Return the addr of ring buffer
 */
SpscRingBuf_t *Get_SpscRingBuf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag);

/**
 * Destructor for ring buffer
 * 
 * @p : addr of ring buffer
 */
void Del_SpscRingBuf(SpscRingBuf_t *p);

/**
 * Get the addr that can be written
 * 
 * @p : addr of ring buffer
 * 
 * Return the addr of available mem in ring buffer
 * which is ready to be written, return null if full
 */
void *Begin_push_SpscRingBuf(SpscRingBuf_t *p);

/**
 * Followed up by begin push when finish writing
 * A signal to consumer that the mem is ready to be read 
 */
void End_push_SpscRingBuf(SpscRingBuf_t *p);

/**
 * Get the addr that can be read
 * 
 * @p : addr of ring buffer
 * 
 * Return the addr of available mem in ring buffer
 * which is ready to be read, return null if empty
 */
void *Begin_pop_SpscRingBuf(SpscRingBuf_t *p);

/**
 * Followed up by begin pop when finish reading
 * A signal to producer that the mem is ready to be written 
 */
void End_pop_SpscRingBuf(SpscRingBuf_t *p);

#if DEBUG
ssize_t Push_SpscRingBuf(SpscRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o, size_t size);
#else
/**
 * Push memory into ring buffer
 * 
 * Spin waits if full
 * 
 * @p : addr of ring buffer
 * @args : obj that need to write into ring buffer
 * @size : size of memory to push, no exceed base obj size of ring buffer
 * 
 * Return the head index where data was pushed
 */
ssize_t Push_SpscRingBuf(SpscRingBuf_t *p, void *args, size_t size);
#endif

/**
 * Pop memory from ring buffer
 * 
 * Spin waits until finished
 * 
 * @p : addr of ring buffer
 * @buf : buffer to store data in the ring buffer
 * 
 * Return the tail index where data was popped, -1 if empty
 */
ssize_t Pop_SpscRingBuf(SpscRingBuf_t *p, void *buf);

/**
 * Not yet implemented
 * Pop a chunck of memory from ring buffer
 * 
 * @p : addr of ring buffer
 * @buf : buffer to store data in the ring buffer
 * @max_num : max number to store into buffer
 *
 * Retrun the number of objs popped
 */
ssize_t Batch_pop_SpscRingBuf(SpscRingBuf_t *p, void *buf, size_t max_num);

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
 * @objNum : number of objs can be placed into ring buffer
 * @objSize : size of obj instance
 * @shmPath : path for file backend shared memory, 
 *            will map to anonymous if not given
 * @prot : Oring RingBufMappingType
 * @flag : The same as mmap, MAP_SHARED, MAP_PRIVATE, MAP_POPULATED...
 * 
 * Return the addr of ring buffer
 */
MpscRingBuf_t *Get_MpscRingBuf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag);

/**
 * Destructor for ring buffer
 * @p : addr of ring buffer
 */
void Del_MpscRingBuf(MpscRingBuf_t *p);

#if DEBUG
ssize_t Push_MpscRingBuf(MpscRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o, size_t size);
ssize_t Try_push_MpscRingBuf(MpscRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o, size_t size);
#else
/**
 * Push memory into ring buffer
 * 
 * Spin waits if full
 * Spin waits other thread to finish writing which is ahead of this thread
 * 
 * @p : addr of ring buffer
 * @args : obj that need to write into ring buffer
 * @size : size of memory to push, no exceed base obj size of ring buffer
 * 
 * Return the head index where data was pushed
 */
ssize_t Push_MpscRingBuf(MpscRingBuf_t *p, void *args, size_t size);

/**
 * Push memory into ring buffer
 * Spin waits other thread to finish writing which is ahead of this thread
 * 
 * @p : addr of ring buffer
 * @args : obj that need to write into ring buffer
 * @size : size of memory to push, no exceed base obj size of ring buffer
 * 
 * Return the head index where data was pushed, -1 if full or contention
 */
ssize_t Try_push_MpscRingBuf(MpscRingBuf_t *p, void *args, size_t size);
#endif

/**
 * Pop memory from ring buffer
 * 
 * @p : addr of ring buffer
 * @buf : buffer to store data in the ring buffer
 * 
 * Return the tail where been popped, -1 if empty
 */
ssize_t Try_Pop_MpscRingBuf(MpscRingBuf_t *p, void *buf);

/**
 * Execute callback immediately after getting the avalible mem in ring buffer
 * Reduce one memory copy
 * 
 * @p : addr of ring buffer
 * cb : callback for excecute
 * 
 * Return the value returned by cb, -1 if empty
 */
int Pop_w_cb_MpscRingBuf(MpscRingBuf_t *p, Pop_cb cb, void *args);

/**
 * Not yet implemented
 * Pop a chunck of memory from ring buffer
 * 
 * @p : addr of ring buffer
 * @buf : buffer to store data in the ring buffer
 * @max_num : max number to store into buffer
 *
 * Retrun the number of objs popped
 */
ssize_t Batch_pop_MpscRingBuf(MpscRingBuf_t *p, void *buf, size_t max_num);

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
 * @objNum : number of objs can be placed into ring buffer
 * @objSize : size of obj instance
 * @shmPath : path for file backend shared memory, 
 *            will map to anonymous if not given
 * @prot : Oring RingBufMappingType
 * @flag : The same as mmap, MAP_SHARED, MAP_PRIVATE, MAP_POPULATED...
 * 
 * Return the addr of ring buffer
 */
MpmcRingBuf_t *Get_MpmcRingBuf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag);

/**
 * Destructor for ring buffer
 * @p : addr of ring buffer
 */
void Del_MpmcRingBuf(MpmcRingBuf_t *p);

#if DEBUG
ssize_t Try_push_MpmcRingBuf(MpmcRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o, size_t size);
#else
/**
 * Push memory into ring buffer, producers don't blocked from each other
 * 
 * @p : addr of ring buffer
 * @args : obj that need to write into ring buffer
 * @size : size of memory to push, no exceed base obj size of ring buffer
 * 
 * Return the head index where data was pushed, -1 if full or contention
 */
ssize_t Try_push_MpmcRingBuf(MpmcRingBuf_t *p, void *args, size_t size);
#endif

/**
 * Pop memory from ring buffer
 * 
 * @p : addr of ring buffer
 * @buf : buffer to store data in the ring buffer
 * 
 * Return the tail index where data was popped, -1 if empty or contention
 */
ssize_t Try_pop_MpmcRingBuf(MpmcRingBuf_t *p, void *buf);

/**
 * Execute callback immediately after getting the avalible mem in ring buffer
 * Reduce one memory copy
 * 
 * @p : addr of ring buffer
 * cb : callback for excecute
 * 
 * Return the value returned by cb, -1 if empty or contention
 */
int Pop_w_cb_MpmcRingBuf(MpmcRingBuf_t *p, Pop_cb cb, void *args);

/**
 * Pop memory from ring buffer with only one consumer
 * 
 * @p : addr of ring buffer
 * @buf : buffer to store data in the ring buffer
 * 
 * Return the tail index where data was popped, -1 if empty or contention. Only safe for single consumer (MPSC) use.
 */
ssize_t Try_pop_MpmcMpscRingBuf(MpmcRingBuf_t *p, void *buf);

/**
 * Blocked functions
 */

/**
 * Generate Blocked ring buffer
 * 
 * @objNum : number of objs can be placed into ring buffer
 * @objSize : size of obj instance
 * @shmPath : path for file backend shared memory, 
 *            will map to anonymous if not given
 * @prot : Oring RingBufMappingType
 * @flag : The same as mmap, MAP_SHARED, MAP_PRIVATE, MAP_POPULATED...
 * 
 * Return the addr of ring buffer
 */
BlockedRingBuf_t *Get_BlockedRingBuf(const size_t objNum, const size_t objSize, const char *shmPath, int prot, int flag);

/**
 * Destructor for ring buffer
 * @p : addr of ring buffer
 */
void Del_BlockedRingBuf(BlockedRingBuf_t *r);

#if DEBUG
ssize_t Push_BlockedRingBuf(BlockedRingBuf_t *p, void *args, testFunc cb, Time_diff_t *arr, char buf[], Obj *o, size_t size);
#else
/**
 * Push memory into ring buffer
 * 
 * Blocking waits if full
 * 
 * @p : addr of ring buffer
 * @args : obj that need to write into ring buffer
 * @size : size of memory to push, no exceed base obj size of ring buffer
 * 
 * Return the head index where data was pushed
 */
ssize_t Push_BlockedRingBuf(BlockedRingBuf_t *p, void *args, size_t size);
#endif

/**
 * Pop memory from ring buffer
 * 
 * Blocking waits if empty
 * 
 * @p : addr of ring buffer
 * @buf : buffer to store data in the ring buffer
 * 
 * Return the tail index where data was popped
 */
ssize_t Pop_BlockedRingBuf(BlockedRingBuf_t *p, void *buf);

/**
 * Not yet implemented
 * Pop a chunck of memory from ring buffer
 * 
 * @p : addr of ring buffer
 * @buf : buffer to store data in the ring buffer
 * @max_num : max number to store into buffer
 *
 * Retrun the number of objs popped
 */
ssize_t Batch_pop_BlockedRingBuf(BlockedRingBuf_t *p, void *buf, size_t max_num);

#ifdef __cplusplus
}
#endif
