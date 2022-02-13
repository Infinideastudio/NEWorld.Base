#include "Conc/BlockingAsContext.h"
#include "FifoQueue.h"
#include "Executor.hpp"
#include "System/Semaphore.h"

class BlockingAsContext::Executor final : public IExecutor {
public:
    Executor() :
        IExecutor(static_cast<FnEnqueue>(&Executor::EnqueueRawImpl)),
        mRunning(true) {
        SetCurrentExecutor(this);
    }

    void Start() {
        while (mRunning) {
            DoWorks();
            if (mRunning) Rest();
        }
        SetCurrentExecutor(nullptr);
    }

    void Stop() noexcept { mRunning = false; }

private:
    void EnqueueRawImpl(Object* o, TaskFn fn) {
        mQueue.Add({ o, fn });
        WakeOne();
    }

    void WakeOne() noexcept {
        for (;;) {
            if (auto c = mPark.load(); c) {
                if (mPark.compare_exchange_strong(c, c - 1)) return mSignal.Signal();
            }
            else return;
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
    Internal::Executor::FifoQueue<Task, true> mQueue;
    std::atomic_int mPark{ 0 };
    Semaphore mSignal{};
};

BlockingAsContext::BlockingAsContext(): mTheExec(new Executor()) {}

BlockingAsContext::~BlockingAsContext() { delete mTheExec; }

void BlockingAsContext::Start() { mTheExec->Start(); }

void BlockingAsContext::Stop() { mTheExec->Stop(); }
