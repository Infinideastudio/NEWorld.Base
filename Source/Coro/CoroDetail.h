#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-throw-keyword-missing"
#pragma once

#include <utility>
#include <optional>
#include <coroutine>
#include <exception>
#include <Conc/Executor.h>

namespace Coro::Internal {
    template<class T>
    class ValueStore;

    template<>
    class ValueStore<void> {
    public:
        void Set() {}

        void Fail(std::exception_ptr e) { mFail = std::move(e); }

        void Get() { if (mFail) std::rethrow_exception(mFail); }

        void GetCopy() { if (mFail) std::rethrow_exception(mFail); }
    private:
        std::exception_ptr mFail{nullptr};
    };

    template<class T>
    class ValueStore {
    public:
        template<class ...U>
        void Set(U &&... v) {
            static_assert(sizeof...(v) == 1, "Too may values to set in a ValueStore Object");
            mValue.emplace(std::forward<U>(v)...);
        }

        void Fail(std::exception_ptr e) { mFail = std::move(e); }

        T &&Get() { if (mFail) std::rethrow_exception(mFail); else return std::move(mValue).value(); }

        T GetCopy() { if (mFail) std::rethrow_exception(mFail); else return mValue.value(); }
    private:
        std::exception_ptr mFail{nullptr};
        std::optional<T> mValue{std::nullopt};
    };

    template<class T, class ContControl>
    class ContinuableValueMedia : public ContControl {
    public:
        template<class ...U>
        explicit ContinuableValueMedia(U &&... v): ContControl(std::forward<U>(v)...) {}

        template<class ...U>
        void Set(U &&... v) {
            mValueStore.Set(std::forward<U>(v)...);
            ContControl::DispatchContinuation();
        }

        void Fail() {
            mValueStore.Fail(std::current_exception());
            ContControl::DispatchContinuation();
        }

        T Get() { return mValueStore.Get(); }

        T GetCopy() { return mValueStore.GetCopy(); }
    private:
        ValueStore<T> mValueStore;
    };

    class AwaitCore: public Object {
    public:
        AwaitCore() noexcept: mExec(CurrentExecutor()) {}

        explicit AwaitCore(IExecutor *next) noexcept: mExec(next) {}

        // Await-related classes are not supposed to be copied nor moved
        AwaitCore(AwaitCore &&) = delete;

        AwaitCore(const AwaitCore &) = delete;

        AwaitCore &operator=(AwaitCore &&) = delete;

        AwaitCore &operator=(const AwaitCore &) = delete;

        bool CanExecInPlace(IExecutor *exec = CurrentExecutor()) const noexcept { return (exec == mExec) || (!mExec); }

        // Note: this call will invalidate "this"
        void Dispatch() { if (mExec) mExec->Enqueue([h = mCo]() noexcept { h.resume(); }); else mCo.resume(); }

        void SetHandle(std::coroutine_handle<> handle) noexcept { mCo = handle; }
    private:
        IExecutor *mExec;
        std::coroutine_handle<> mCo{};
    };

    // helper class for generating actual await client class.
    // the 'Core' class should have a Transit(coroutine_handle<>) and a Get() function
    template <class Core>
    struct Await: private Core {
        template<class ...U>
        explicit Await(U &&... v): Core(std::forward<U>(v)...) {}

        // the short-path check should be done in Core::Transit
        // this is done to make sure that the state transition is properly handled
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        [[nodiscard]] bool await_suspend(std::coroutine_handle<> h) { return Core::Transit(h); }

        auto await_resume() { return Core::Get(); }
    };
}
#pragma clang diagnostic pop