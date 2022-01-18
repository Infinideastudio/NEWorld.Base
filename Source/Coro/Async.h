#pragma once

#include <mutex>
#include <memory>
#include <exception>
#include <coroutine>
#include <Temp/Temp.h>
#include <Conc/SpinLock.h>
#include <Conc/Executor.h>

namespace Internal::Coro::Async {
    class AwaitBase {
    public:
        AwaitBase() noexcept = default;

        AwaitBase(IExecutor* next) noexcept : mNextExec(next) {}

        bool AllowDirectDispatch() const noexcept { return (mThisExec == mNextExec) || (!mNextExec); }

        AwaitBase* GetNext() const noexcept { return mNext; }

        AwaitBase* SetNext(AwaitBase* next) noexcept { return mNext = next; }

        // Note: this call will invalidate "this"
        void Dispatch() {
            if (mNextExec) mNextExec->Enqueue([h = mHandle]() noexcept { h.resume(); }); else mHandle.resume();
        }

        bool await_ready() const noexcept {
            // this function will always return false.
            // the short-path check will be done in the await_suspend
            // this is done to make sure that the state transition is properlly handled
            return false;
        }
    protected:
        void SetHandle(std::coroutine_handle<> handle) noexcept { mHandle = handle; }
    private:
        IExecutor* mThisExec{ CurrentExecutor() }, * mNextExec = mThisExec;
        AwaitBase* mNext{ nullptr };
        std::coroutine_handle<> mHandle{};
    };

    class SharedStateBase {
    public:
        bool Transit(AwaitBase* next) {
            if (mReady) { if (next->AllowDirectDispatch()) return false; else return (next->Dispatch(), true); }
            else {
                std::unique_lock lk{ mContLock };
                if (mReady) {
                    lk.unlock(); // able to direct dispatch, no need to hold lock.
                    if (next->AllowDirectDispatch()) return false; else (next->Dispatch(), true);
                }
                // double checked that we cannot dispatch now, chain it to the tail
                if (!mHead) mHead = next; else mTail->SetNext(next);
                mTail = next;
                return true;
            }
        }

        void UnhandledException() {
            mException = std::current_exception();
            Dispatch();
        }
    protected:
        void Dispatch() {
            mReady.store(true);
            std::unique_lock lk{ mContLock };
            auto it = mHead;
            // as the ready flag is set, any other call using the state should not touch the cont chain
            lk.unlock();
            for (; it;) {
                auto current = it;
                it = it->GetNext();
                current->Dispatch(); // this will invalidate the current pointer
            }
        }

        void GetState() noexcept { if (mException) std::rethrow_exception(mException); }
    private:
        std::atomic_bool mReady{ false };
        Lock<SpinLock> mContLock{};
        AwaitBase* mHead{ nullptr }, * mTail{ nullptr };
        std::exception_ptr mException{};
    };
}

class SwitchTo {
public:
    SwitchTo(IExecutor* next) noexcept : mNext(next) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        mNext->Enqueue([handle]() noexcept { handle.resume(); });
    }

    void await_resume() noexcept {}
private:
    IExecutor* mNext;
};

template<class T>
class Async {
    template <class T>
    class SharedState : public Internal::Coro::Async::SharedStateBase {
    public:
        void Set(T&& expr) {
            mData = std::move(expr);
            Dispatch();
        }

        void Set(const T& expr) {
            mData = expr;
            Dispatch();
        }

        T Get() {
            GetState();
            return mData; // unfortunately this will always have to be a copy.
        }
    private:
        T mData;
    };

    template <>
    class SharedState<void> : public Internal::Coro::Async::SharedStateBase {
    public:
        void Set() { Dispatch(); }

        void Get() { GetState(); }
    };

    using State = SharedState<T>;
public:
    class Await : public Internal::Coro::Async::AwaitBase {
    public:
        Await(std::shared_ptr<State> state) noexcept : AwaitBase(), mState(std::move(state)) {}

        Await(std::shared_ptr<State> state, IExecutor* next) noexcept : AwaitBase(next), mState(std::move(state)) {}

        bool await_suspend(std::coroutine_handle<> handle) {
            SetHandle(handle);
            return mState->Transit(this);
        }

        T await_resume() { return mState->Get(); }
    private:
        std::shared_ptr<State> mState;
    };
private:
    class PromiseBase {
        static std::shared_ptr<State> make_state() noexcept {
            auto alloc = temp_alloc<State>{};
            return std::allocate_shared<State>(alloc);
        }
    public:
        Async get_return_object() { return Async(mState); }

        auto initial_suspend() noexcept { return std::suspend_never{}; }

        auto final_suspend() noexcept { return std::suspend_never{}; }

        void unhandled_exception() { mState->UnhandledException(); }
    protected:
        std::shared_ptr<State> mState = make_state();
    };
public:
    template <class U>
    struct Promise : PromiseBase {
        void return_value(const T& expr) { mState->Set(expr); }

        void return_value(T&& expr) { mState->Set(std::move(expr)); }
    };

    template <>
    struct Promise<void> : PromiseBase {
        void return_void() { mState->Set(); }
    };

    using promise_type = Promise<T>;

    auto operator co_await()&& { return Await(std::move(mState)); }

    auto operator co_await() const& { return Await(mState); }

    auto Configure(IExecutor* next)&& { return Await(std::move(mState), next); }

    auto Configure(IExecutor* next) const& { return Await(mState, next); }
private:
    std::shared_ptr<State> mState;

    Async(std::shared_ptr<State> state) noexcept : mState(std::move(state)) {}
};

namespace Internal::Coro::ValueAsync {
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

        void UnhandledException() {
            mException = std::current_exception();
            Dispatch();
        }

        void ReleaseOnce() noexcept { if (mRef.fetch_sub(1) == 1) mLife.destroy(); }
    protected:
        void Dispatch() { if (auto ths = mNext.exchange(INVALID_PTR); ths) ths->Dispatch(); }

        void GetState() noexcept { if (mException) std::rethrow_exception(mException); }
    private:
        std::atomic<AwaitBase*> mNext{ nullptr };
        std::exception_ptr mException{};
        std::atomic_int mRef{ 2 };
        std::coroutine_handle<> mLife{};
    };
}

template<class T>
class ValueAsync {
    template <class T>
    class SharedState : public Internal::Coro::ValueAsync::SharedStateBase {
    public:
        void Set(T&& expr) {
            mData = std::move(expr);
            Dispatch();
        }

        void Set(const T& expr) {
            mData = expr;
            Dispatch();
        }

        T Get() {
            GetState();
            return std::move(mData); // unfortunately this will always have to be a copy.
        }
    private:
        T mData;
    };

    template <>
    class SharedState<void> : public Internal::Coro::ValueAsync::SharedStateBase {
    public:
        void Set() { Dispatch(); }

        void Get() { GetState(); }
    };

    using State = SharedState<T>;
public:
    class Await : public Internal::Coro::ValueAsync::AwaitBase {
    public:
        Await(State* state) noexcept : AwaitBase(), mState(state) {}

        Await(State* state, IExecutor* next) noexcept : AwaitBase(next), mState(state) {}

        Await(Await&& other) = delete;

        Await(const Await& other) = delete;

        Await& operator=(Await&& other) = delete;

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
private:
    class PromiseBase {
    public:
        struct NullSuspend {
            explicit NullSuspend(State& state) noexcept : mState(state) {}

            bool await_ready() const noexcept { return true; }

            void await_suspend(std::coroutine_handle<>) const noexcept { mState.ReleaseOnce(); }

            void await_resume() const noexcept {}
        private:
            State& mState;
        };

        ValueAsync get_return_object() { return ValueAsync(&mState); }

        auto initial_suspend() noexcept { return std::suspend_never{}; }

        auto final_suspend() noexcept { return NullSuspend{ mState }; }

        void unhandled_exception() { mState.UnhandledException(); }
    protected:
        State mState;
    };
public:
    template <class U>
    struct Promise : PromiseBase {
        void return_value(const T& expr) { mState.Set(expr); }

        void return_value(T&& expr) { mState.Set(std::move(expr)); }
    };

    template <>
    struct Promise<void> : PromiseBase {
        void return_void() { mState.Set(); }
    };

    using promise_type = Promise<T>;

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
        return (mState = nullptr, ret);
    }

    auto operator co_await() const& { static_assert(false, "Copying ValueAsync is Not Allowed"); }

    auto Configure(IExecutor* next)&& {
        auto ret = Await(mState);
        return (mState = nullptr, ret);
    }

    auto Configure(IExecutor* next) const& { static_assert(false, "Copying ValueAsync is Not Allowed"); }
private:
    State* mState;

    ValueAsync(State* state) noexcept : mState(state) {}
};
