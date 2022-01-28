#pragma once

#include <utility>

template <class Fn>
class ScopeGuard {
public:
    explicit ScopeGuard(Fn&& v): mFn(std::forward<Fn>(v)) {}

    explicit ScopeGuard(const Fn& v): mFn(std::forward<Fn>(v)) {}

    ~ScopeGuard() noexcept { mFn(); }
private:
    Fn mFn;
};
