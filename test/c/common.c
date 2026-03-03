#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include "common.h"

void NuThdBindCore(int cpu_id)
{
    cpu_set_t cpuset;
    pthread_t current_thread = pthread_self();  // Get the current thread's ID

    CPU_ZERO(&cpuset);         // Clear the CPU set
    CPU_SET(cpu_id, &cpuset);  // Add the desired CPU to the set

    // Set the affinity of the current thread
    pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}
