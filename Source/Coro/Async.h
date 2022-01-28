#pragma once

#include <mutex>
#include <memory>
#include <Conc/SpinLock.h>
#include "Internal.h"

namespace Coro::Async::Internal {
    using namespace Coro::Internal;

    class AwaitCoreChained : public AwaitCore {
    public:
        AwaitCoreChained() noexcept = default;

        explicit AwaitCoreChained(IExecutor *next) noexcept: AwaitCore(next) {}

        [[nodiscard]] AwaitCoreChained *GetNext() const noexcept { return mNext; }

        AwaitCoreChained *SetNext(AwaitCoreChained *next) noexcept { return mNext = next; }

    private:
        AwaitCoreChained *mNext{nullptr};
    };

    class ContinuationControl {
    public:
        bool Transit(AwaitCoreChained *next) {
            const auto current = CurrentExecutor();
            if (mReady) { if (next->CanExecInPlace(current)) return false; else return (next->Dispatch(), true); }
            else {
                std::unique_lock lk{mContLock};
                if (mReady) {
                    lk.unlock(); // able to direct dispatch, no need to hold lock.
                    if (next->CanExecInPlace(current)) return false; else return (next->Dispatch(), true);
                }
                // double-checked that we cannot dispatch now, chain it to the tail
                if (!mHead) mHead = next; else mTail->SetNext(next);
                mTail = next;
                return true;
            }
        }

        void DispatchContinuation() {
            mReady.store(true);
            std::unique_lock lk{mContLock};
            auto it = mHead;
            // as the ready flag is set, any other call using the state should not touch the cont chain
            lk.unlock();
            for (; it;) {
                auto current = it;
                it = it->GetNext();
                current->Dispatch(); // this will invalidate the current pointer
            }
        }

    private:
        std::atomic_bool mReady{false};
        Lock<SpinLock> mContLock{};
        AwaitCoreChained *mHead{nullptr}, *mTail{nullptr};
    };

    template<class T>
    using SharedContinuableValueMedia = ContinuableValueMedia<T, ContinuationControl>;

    template<class T>
    using SharedContinuableValueMediaHandle = std::shared_ptr<SharedContinuableValueMedia<T>>;

    template<class T>
    class PromiseValueMedia {
    public:
        explicit PromiseValueMedia(SharedContinuableValueMediaHandle<T> h) noexcept: mHandle(std::move(h)) {}

        template<class ...U>
        void return_value(U &&... v) { mHandle->Set(std::forward<U>(v)...); }

        void unhandled_exception() { mHandle->Fail(); }

    private:
        SharedContinuableValueMediaHandle<T> mHandle;
    };

    template<>
    class PromiseValueMedia<void> {
    public:
        explicit PromiseValueMedia(SharedContinuableValueMediaHandle<void> h) noexcept: mHandle(std::move(h)) {}

        void return_void() { mHandle->Set(); }

        void unhandled_exception() { mHandle->Fail(); }

    private:
        SharedContinuableValueMediaHandle<void> mHandle;
    };
}

template<class T>
class Async {
    using State = Coro::Async::Internal::SharedContinuableValueMedia<T>;
    using StateHandle = Coro::Async::Internal::SharedContinuableValueMediaHandle<T>;
    using PromiseMedia = Coro::Async::Internal::PromiseValueMedia<T>;

    class FlexAwaitCore : Coro::Async::Internal::AwaitCoreChained {
    public:
        explicit FlexAwaitCore(StateHandle state) : mState(state) {}

        FlexAwaitCore(StateHandle state, IExecutor *next) noexcept: AwaitCoreChained(next), mState(state) {}

        bool Transit(std::coroutine_handle<> h) {
            SetHandle(h);
            return mState->Transit(this);
        }

        T Get() { return mState->GetCopy(); }

    private:
        StateHandle mState;
    };

public:
    using Await = Coro::Internal::Await<FlexAwaitCore>;

    class promise_type : public PromiseMedia {
        static StateHandle make_state() noexcept {
            auto alloc = temp_alloc<State>{};
            return std::allocate_shared<State>(alloc);
        }

    public:
        promise_type() : PromiseMedia(make_state()) {}

        Async get_return_object() { return Async(this); }

        constexpr auto initial_suspend() noexcept { return std::suspend_never{}; }

        constexpr auto final_suspend() noexcept { return std::suspend_never{}; }
    };

    auto operator co_await()&& { return Await(std::move(mState)); }

    auto operator co_await() const& { return Await(mState); }

    auto Configure(IExecutor *next) &&{ return Await(std::move(mState), next); }

    auto Configure(IExecutor *next) const &{ return Await(mState, next); }

private:
    StateHandle mState;

    explicit Async(std::shared_ptr<State> state) noexcept: mState(std::move(state)) {}
};
