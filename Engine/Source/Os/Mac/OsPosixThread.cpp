#include <cerrno>
#include <mach/mach.h>
#include <sched.h>
#include <unistd.h>
#include "Engine/Source/Os/Linux/OsPosix.h"

uint32_t OsPosixThreadId() {
    uint64_t id = 0;
    pthread_threadid_np(nullptr, &id);
    return static_cast<uint32_t>(id);
}

void OsPosixSetThreadName(const char* name) {
    pthread_setname_np(name);
}

// macOS has no per-thread nice. The nice range (-20 to 19) maps onto the scheduling priorities of the thread's
// policy, and a thread is reached through its pthread_t only, so calls without one do nothing.
static bool PriorityRange(pthread_t thread, int32_t* policy, sched_param* param, int32_t* low, int32_t* high) {
    if (!thread || pthread_getschedparam(thread, policy, param) != 0) {
        return false;
    }

    *low = sched_get_priority_min(*policy);
    *high = sched_get_priority_max(*policy);
    return *high > *low;
}

bool OsPosixSetThreadNice(pthread_t thread, uint32_t tid, int32_t nice) {
    int32_t policy;
    sched_param param;
    int32_t low;
    int32_t high;

    if (!PriorityRange(thread, &policy, &param, &low, &high)) {
        return false;
    }

    nice = nice < -20 ? -20 : (nice > 19 ? 19 : nice);
    int32_t middle = (low + high) / 2;
    param.sched_priority = middle - nice * (high - low) / 40;

    return pthread_setschedparam(thread, policy, &param) == 0;
}

bool OsPosixGetThreadNice(pthread_t thread, uint32_t tid, int32_t* nice) {
    int32_t policy;
    sched_param param;
    int32_t low;
    int32_t high;

    if (!PriorityRange(thread, &policy, &param, &low, &high)) {
        return false;
    }

    int32_t middle = (low + high) / 2;
    *nice = (middle - param.sched_priority) * 40 / (high - low);
    return true;
}

// Thread affinity cannot be set on macOS, the scheduler places threads itself.
bool OsPosixSetThreadAffinity(pthread_t thread, uint64_t mask) {
    return mask != 0;
}

bool OsPosixSetProcessAffinity(uint64_t mask) {
    return mask != 0;
}

uint64_t OsPosixProcessAffinity() {
    long processors = sysconf(_SC_NPROCESSORS_ONLN);

    if (processors <= 0) {
        return 1;
    }

    return processors >= 64 ? ~uint64_t(0) : (uint64_t(1) << processors) - 1;
}

uint32_t OsPosixThreadCount() {
    thread_act_array_t threads = nullptr;
    mach_msg_type_number_t count = 0;

    if (task_threads(mach_task_self(), &threads, &count) != KERN_SUCCESS) {
        return 1;
    }

    for (mach_msg_type_number_t i = 0; i < count; i++) {
        mach_port_deallocate(mach_task_self(), threads[i]);
    }

    vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(threads), count * sizeof(thread_act_t));

    return count ? count : 1;
}

void OsPosixCondInit(pthread_cond_t* cond) {
    pthread_cond_init(cond, nullptr);
}

// macOS condition variables only take CLOCK_REALTIME deadlines, so the wait is for the time left until the
// monotonic one.
int32_t OsPosixCondTimedWait(pthread_cond_t* cond, pthread_mutex_t* mutex, const timespec* monotonicDeadline) {
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    timespec remaining;
    remaining.tv_sec = monotonicDeadline->tv_sec - now.tv_sec;
    remaining.tv_nsec = monotonicDeadline->tv_nsec - now.tv_nsec;

    if (remaining.tv_nsec < 0) {
        remaining.tv_sec--;
        remaining.tv_nsec += 1000000000;
    }

    if (remaining.tv_sec < 0) {
        return ETIMEDOUT;
    }

    return pthread_cond_timedwait_relative_np(cond, mutex, &remaining);
}
