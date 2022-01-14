#pragma once

#include <mutex>
#include "Conc/SpinLock.h"
#include "Temp/TempQueue.h"

namespace Internal::Executor {
    template<class Task>
    class FifoQueue {
    public:
        void Add(const Task &t) {
            std::lock_guard lk{mSpin};
            mTasks.Push(t);
        }

        [[nodiscard]] Task Get() noexcept {
            if (auto exec = LockedPop(); exec.Item) return exec;
            SpinWait spinner{};
            for (auto i = 0u; i < SpinWait::SpinCountForSpinBeforeWait; ++i) {
                spinner.SpinOnce();
                if (auto exec = LockedPop(); exec.Item) return exec;
            }
            return {};
        }

        [[nodiscard]] bool SnapshotNotEmpty() const noexcept { return !mTasks.Empty(); }

        void Finalize() noexcept {}
    private:
        Lock<SpinLock> mSpin{};
        TempQueue<Task> mTasks{};

        auto LockedPop() {
            std::lock_guard lk{mSpin};
            return mTasks.Pop();
        }
    };
}
