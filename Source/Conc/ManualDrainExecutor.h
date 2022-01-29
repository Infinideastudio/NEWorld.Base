#pragma once

#include "Executor.h"
#include "Coro/ValueAsync.h"

class ManualDrainExecutor {
public:
    ManualDrainExecutor();

    ManualDrainExecutor(ManualDrainExecutor&&) = delete;
    ManualDrainExecutor(const ManualDrainExecutor&) = delete;
    ManualDrainExecutor& operator=(ManualDrainExecutor&&) = delete;
    ManualDrainExecutor& operator=(const ManualDrainExecutor&) = delete;

	~ManualDrainExecutor();

	void DrainOnce();
private:
	class Executor;
	Executor* mTheExec;
};