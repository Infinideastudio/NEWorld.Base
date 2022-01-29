#pragma once

#include "Executor.h"
#include "Coro/ValueAsync.h"

class BlockingAsContext {
public:
	// creates the executor queue, mark the current thread as the executor
	BlockingAsContext();

	BlockingAsContext(BlockingAsContext&&) = delete;
	BlockingAsContext(const BlockingAsContext&) = delete;
	BlockingAsContext& operator=(BlockingAsContext&&) = delete;
	BlockingAsContext& operator=(const BlockingAsContext&) = delete;

	// destruct everthing
	~BlockingAsContext();

	// drain the queue until the awaiting operation has completed.
	// any item submitted to the executer after the completion of the awaited
	// item is invalid and will result in undefined behaviour
	template <class T>
	void Await(T&& awaitable) {
		CoAwaitToStop(std::forward<T>(awaitable)); // Call a coroutine to await the awaitable which calls Stop() afterwards
		Start();
	}
private:
	void Start();
	void Stop();

	template <class T>
	ValueAsync<void> CoAwaitToStop(T&& awaitable) {
		co_await std::forward<T>(awaitable);
		Stop();
	}

	class Executor;
	Executor* mTheExec;
};