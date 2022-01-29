#include "Conc/ManualDrainExecutor.h"
#include "FifoQueue.h"
#include "Executor.hpp"

class ManualDrainExecutor::Executor final : public IExecutor {
public:
    Executor() : IExecutor(static_cast<FnEnqueue>(&Executor::EnqueueRawImpl)) {}

    void DrainOnce() {
        SetCurrentExecutor(this);
        for (;;) if (auto exec = mQueue.Get(); exec.Item) (*exec.Item.*exec.Entry)(); else return;
        SetCurrentExecutor(nullptr);
    }
private:
    void EnqueueRawImpl(Object* o, TaskFn fn) {
        mQueue.Add({ o, fn });
    }

    Internal::Executor::FifoQueue<Task, true> mQueue;
};

ManualDrainExecutor::ManualDrainExecutor(): mTheExec(new Executor()) {}

ManualDrainExecutor::~ManualDrainExecutor() { delete mTheExec; }

void ManualDrainExecutor::DrainOnce() { mTheExec->DrainOnce(); }
