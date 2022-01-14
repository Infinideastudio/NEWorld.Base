#pragma once

#include <chrono>

#if __has_include(<mach/semaphore.h>)
#include <mach/semaphore.h>
#include <mach/mach_init.h>
#include <mach/task.h>

class Semaphore {
public:
    explicit Semaphore() noexcept
            :handle(New()) { }

    ~Semaphore() { Release(handle); }

    void Wait() noexcept {
        while (semaphore_wait(handle)!=KERN_SUCCESS) { }
    }

    template <class Rep, class Period>
    bool WaitFor(const std::chrono::duration<Rep, Period>& relTime) noexcept {
        return TimedWait(std::chrono::duration_cast<std::chrono::milliseconds>(relTime).count());
    }

    template <class Clock, class Duration>
    bool WaitUntil(const std::chrono::time_point<Clock, Duration>& absTime) noexcept {
        return WaitFor(absTime - Clock::now());
    }


    void Signal() noexcept { aemaphore_signal(handle); }
private:
    static semaphore_t New() noexcept {
        semaphore_t ret;
        semaphore_create(mach_task_self(), &ret, SYNC_POLICY_FIFO, 0);
        return ret;
    }

    bool TimedWait(const long long ms) noexcept {
         mach_timespec_t ts;
         ts.tv_sec = ms / 1000;
         ts.tv_nsec = (ms % 1000) * 1000000;
         bool wait = true;
         const auto err = semaphore_timedwait(sem, ts);
         return err == KERN_SUCCESS;
    }

    static void Release(Semaphore_t sem) noexcept {
        semaphore_destroy(mach_task_self(), sem);
    }

    Semaphore_t handle;
};

#elif __has_include(<Windows.h>)

#include "Internal/system.h"

class Semaphore {
public:
    Semaphore() noexcept
            : handle(CreateSemaphore(nullptr, 0, MAXLONG, nullptr)) {}

    ~Semaphore() noexcept { CloseHandle(handle); }

    void Wait() noexcept { WaitForSingleObject(handle, INFINITE); }

    template <class Rep, class Period>
    bool WaitFor(const std::chrono::duration<Rep, Period>& relTime) noexcept {
        return TimedWait(std::chrono::duration_cast<std::chrono::milliseconds>(relTime).count());
    }

    template <class Clock, class Duration>
    bool WaitUntil(const std::chrono::time_point<Clock, Duration>& absTime) noexcept {
        return WaitFor(absTime - Clock::now());
    }

    void Signal() noexcept {
        LONG last;
        ReleaseSemaphore(handle, 1, &last);
    }

private:
    HANDLE handle;

    bool TimedWait(const long long ms) noexcept {
        return WaitForSingleObject(handle, ms) != WAIT_TIMEOUT;
    }
};

#elif __has_include(<Semaphore.h>)
#include <Semaphore.h>
class Semaphore {
public:
    Semaphore() noexcept { sem_init(&_Semaphore, 0, 0); }

    ~Semaphore() noexcept { sem_destroy(&_Semaphore); }

    void Wait() noexcept { sem_wait(&_Semaphore); }

    void Signal() noexcept { sem_post(&_Semaphore); }
private:
    sem_t _Semaphore;
};

#else
class Semaphore {
public:
    Semaphore() noexcept {}

    ~Semaphore() noexcept {}

    void Wait() noexcept {}

    void Signal() noexcept {}
};

# error "No Adaquate Semaphore Supported to be adapted from"
#endif