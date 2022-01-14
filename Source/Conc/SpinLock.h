#pragma once

#include <atomic>
#include "Lock.h"
#include "SpinWait.h"

class SpinLock {
public:
    void Enter() noexcept {
        for (;;) {
            auto expect = false;
            if (mLock.compare_exchange_strong(expect, true, std::memory_order_acquire)) return;
            WaitUntilLockIsFree();
        }
    }

    void Leave() noexcept { mLock.store(false, std::memory_order_release); }

private:
    void WaitUntilLockIsFree() const noexcept {
        SpinWait spinner{};
        while (mLock.load(std::memory_order_relaxed)) { spinner.SpinOnce(); }
    }

    alignas(64) std::atomic_bool mLock = {false};
};

class Monitor {
public:
    void Enter() noexcept {
        for (;;) {
            auto expect = true;
            if (mLock.compare_exchange_strong(expect, false, std::memory_order_acquire)) return;
            WaitUntilLockIsFree();
        }
    }

    void Enter(bool &taken) noexcept {
        for (;;) {
            taken = true;
            if (mLock.compare_exchange_strong(taken, false, std::memory_order_acquire)) return;
            WaitUntilLockIsFree();
        }
    }

    void Leave() noexcept { mLock.store(true, std::memory_order_release); }

private:
    void WaitUntilLockIsFree() const noexcept {
        SpinWait spinner{};
        while (!mLock.load(std::memory_order_relaxed)) { spinner.SpinOnce(); }
    }

    alignas(64) std::atomic_bool mLock = {true};
};
