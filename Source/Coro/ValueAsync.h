#pragma once

#include <memory>
#include <exception>
#include <coroutine>
#include <Temp/Temp.h>
#include <Conc/Executor.h>

namespace Coro::ValueAsync::Internal {
    class AwaitBase {
    public:
        AwaitBase() noexcept = default;

        AwaitBase(IExecutor* next) noexcept : mNextExec(next) {}

        bool AllowDirectDispatch() const noexcept { return (mThisExec == mNextExec) || (!mNextExec); }

        // Note: this call will invalidate "this"
        void Dispatch() {
            if (mNextExec) mNextExec->Enqueue([h = mHandle]() noexcept { h.resume(); }); else mHandle.resume();
        }

        bool await_ready() const noexcept { return false; }
    protected:
        void SetHandle(std::coroutine_handle<> handle) noexcept { mHandle = handle; }
    private:
        IExecutor* mThisExec{ CurrentExecutor() }, * mNextExec = mThisExec;
        std::coroutine_handle<> mHandle{};
    };

    class SharedStateBase {
        inline static constexpr AwaitBase* INVALID_PTR = reinterpret_cast<AwaitBase*>(~uintptr_t(0));
    public:
        bool Transit(AwaitBase* next) {
            for (;;) {
                auto val = mNext.load();
                // if the state has ben finalized then direct dispatch
                if (val == INVALID_PTR) if (next->AllowDirectDispatch()) return false; else return (next->Dispatch(), true);
                if (val) std::abort();
                if (mNext.compare_exchange_weak(val, next)) return true;
            }
        }

        void unhandled_exception() {
            mException = std::current_exception();
            Dispatch();
        }

        void ReleaseOnce() noexcept { if (mRef.fetch_sub(1) == 1) mLife.destroy(); }
    protected:
        void SetHandle(std::coroutine_handle<> handle) noexcept { mLife = handle; }

        void Dispatch() { if (auto ths = mNext.exchange(INVALID_PTR); ths) ths->Dispatch(); }

        void GetState() noexcept { if (mException) std::rethrow_exception(mException); }
    private:
        std::atomic<AwaitBase*> mNext{ nullptr };
        std::exception_ptr mException{};
        std::atomic_int mRef{ 2 };
        std::coroutine_handle<> mLife{};
    };

    template <class T>
    class SharedState : public SharedStateBase {
    public:
        void return_value(T&& expr) {
            mData = std::move(expr);
            Dispatch();
        }

        void return_value(const T& expr) {
            mData = expr;
            Dispatch();
        }

        T Get() {
            GetState();
            return std::move(mData);
        }
    private:
        T mData;
    };

    template <>
    class SharedState<void> : public SharedStateBase {
    public:
        void return_void() { Dispatch(); }

        void Get() { GetState(); }
    };
}

template<class T>
class ValueAsync {
    using State = Coro::ValueAsync::Internal::SharedState<T>;
public:
    class Await : public Coro::ValueAsync::Internal::AwaitBase {
    public:
        Await(State* state) noexcept : AwaitBase(), mState(state) {}

        Await(State* state, IExecutor* next) noexcept : AwaitBase(next), mState(state) {}

        Await(Await&& other) noexcept : mState(other.mState) { other.mState = nullptr; }

        Await(const Await& other) = delete;

        Await& operator=(Await&& other) noexcept {
            this->~Await();
            mState = other.mState;
            other.mState = nullptr;
            return *this;
        }

        Await& operator=(const Await& other) = delete;

        ~Await() noexcept { if (mState) mState->ReleaseOnce(); }

        bool await_suspend(std::coroutine_handle<> handle) {
            SetHandle(handle);
            return mState->Transit(this);
        }

        T await_resume() { return mState->Get(); }
    private:
        State* mState;
    };

    class promise_type: public State {
    public:
        struct NullSuspend {
            explicit NullSuspend(State& state) noexcept : mState(state) {}

            bool await_ready() const noexcept { return false; }

            void await_suspend(std::coroutine_handle<>) const noexcept { mState.ReleaseOnce(); }

            void await_resume() const noexcept {}
        private:
            State& mState;
        };

        ValueAsync get_return_object() { return ValueAsync(this); }

        auto initial_suspend() noexcept {
            State::SetHandle(std::coroutine_handle<promise_type>::from_promise(*this));
            return std::suspend_never{};
        }

        auto final_suspend() noexcept { return NullSuspend{ *this }; }
    };

    ValueAsync(ValueAsync&& other) noexcept : mState(other.mState) { other.mState = nullptr; }

    ValueAsync(const ValueAsync& other) = delete;

    ValueAsync& operator=(ValueAsync&& other) noexcept {
        this->~ValueAsync();
        mState = other.mState;
        other.mState = nullptr;
        return *this;
    }

    ValueAsync& operator=(const ValueAsync& other) = delete;

    ~ValueAsync() noexcept { if (mState) mState->ReleaseOnce(); }

    auto operator co_await()&& {
        auto ret = Await(mState);
        mState = nullptr;
        return ret;
    }

    auto operator co_await() const& { static_assert(false, "Copying ValueAsync is Not Allowed"); }

    auto Configure(IExecutor* next)&& {
        auto ret = Await(mState);
        mState = nullptr;
        return ret;
    }

    auto Configure(IExecutor* next) const& { static_assert(false, "Copying ValueAsync is Not Allowed"); }
private:
    State* mState;

    ValueAsync(State* state) noexcept : mState(state) {}
};
