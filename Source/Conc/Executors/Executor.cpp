#include "Executor.hpp"

static thread_local IExecutor* gExecutor{ nullptr };

IExecutor* CurrentExecutor() noexcept { return gExecutor; }

void SetCurrentExecutor(IExecutor* exec) noexcept { gExecutor = exec; }
