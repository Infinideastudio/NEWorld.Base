#include "Conc/Executor.h"

inline static thread_local IExecutor* gExecutor{ nullptr };
