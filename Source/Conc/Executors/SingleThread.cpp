#include <mutex>
#include "FifoQueue.h"
#include "Internal/Semaphore.h"
#include "Executor.hpp"

std::shared_ptr<IExecutor> CreateSingleThreadExecutor() {
    class Executor final : public IExecutor {
    public:
        Executor() :
                IExecutor(static_cast<FnEnqueue>(&Executor::EnqueueRawImpl)),
                mRunning(true), mThread([this]()noexcept {

            while (mRunning) {
                gExecutor = this;
                DoWorks();
                if (mRunning) Rest();
            }
        }) {}

        ~Executor() {
            Enqueue([this]()noexcept { mRunning = false; });
            mThread.join();
        }

    private:
        void EnqueueRawImpl(Item *o, Entry fn) {
            mQueue.Add({o, fn});
            WakeOne();
        }

        void WakeOne() noexcept {
            for (;;) {
                if (auto c = mPark.load(); c) {
                    if (mPark.compare_exchange_strong(c, c - 1)) return mSignal.Signal();
                } else return;
            }
        }

        void Rest() noexcept {
            mPark.fetch_add(1); // enter protected region
            if (mQueue.SnapshotNotEmpty()) {
                // it is possible that a task was added during function invocation period of this function and the WakeOne
                // did not notice this thread is going to sleep. To prevent system stalling, we will invoke WakeOne again
                // to wake a worker (including this one)
                WakeOne();
            }
            // to keep integrity, this thread will enter sleep state regardless of whether if the snapshot check is positive
            mSignal.Wait();
        }

        void DoWorks() noexcept {
            for (;;) if (auto exec = mQueue.Get(); exec.Item) (*exec.Item.*exec.Entry)(); else return;
        }

        std::atomic_bool mRunning;
        Internal::Executor::FifoQueue<Task> mQueue;
        std::atomic_int mPark{0};
        Semaphore mSignal{};
        std::thread mThread;
    };
    return std::make_shared<Executor>();
}
