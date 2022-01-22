#pragma once

#include "Async.h"
#include "ValueAsync.h"

class SwitchTo {
public:
    SwitchTo(IExecutor* next) noexcept : mNext(next) {}

    constexpr bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        mNext->Enqueue([handle]() noexcept { handle.resume(); });
    }

    constexpr void await_resume() noexcept {}
private:
    IExecutor* mNext;
};

struct Redispatch {
    constexpr bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        CurrentExecutor()->Enqueue([handle]() noexcept { handle.resume(); });
    }

    void await_resume() noexcept {}
};

template <class Container> 
ValueAsync<void> AwaitAll(Container c) { for (auto&& x : c) co_await std::move(x); }
