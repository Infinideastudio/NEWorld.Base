#pragma once

#include <mutex>
#include <atomic>
#include <thread>
#include <cstddef>
#include <utility>
#include <type_traits>
#include <condition_variable>

#include "Executor.h"
#include "SpinWait.h"

// TODO: Reduce Allocation Calls
enum class FutureErrorCode : int {
    BrokenPromise = 0,
    FutureAlreadyRetrieved = 1,
    PromiseAlreadySatisfied = 2,
    NoState = 3
};

enum class ContinuationFlag : int {
    ExecOnCompletionInvocation = 0,
    ForceAsyncInvocation = 1,
    AsyncOnlyAfterDistantCompletion = 2
};

class AsyncContinuationContext {
public:
    void Reschedule(IExecTask* task) noexcept { ThreadPool::Enqueue(task); }

    static AsyncContinuationContext* CaptureCurrent() noexcept {
        // TODO: Reimplement when multiple Threadpool support is available
        static AsyncContinuationContext dummy;
        return &dummy;
    }
};

class AAsyncContinuationExecTask : public IExecTask {
public:
    void DoScheduleContinuation() noexcept {
        if (_Flag==ContinuationFlag::ExecOnCompletionInvocation) { this->Exec(); }
        else { DoAsyncDispatchContinuation(); }
    }

    void DoScheduleContinuationAsPromiseAlreadyFulfilled() noexcept {
        if (_Flag==ContinuationFlag::ForceAsyncInvocation) { DoAsyncDispatchContinuation(); }
        else { this->Exec(); }
    }

    void Setup(const ContinuationFlag flag, AsyncContinuationContext* context) noexcept {
        SetFlag(flag);
        if (flag!=ContinuationFlag::ExecOnCompletionInvocation) {
            if (context) { SetContext(context); }
            else { Capture(); }
        }
    }

    void SetFlag(const ContinuationFlag flag) noexcept { _Flag = flag; }

    void SetContext(AsyncContinuationContext* context) noexcept { _Context = context; }

    void Capture() noexcept { SetContext(AsyncContinuationContext::CaptureCurrent()); }
private:
    ContinuationFlag _Flag{};
    AsyncContinuationContext* _Context{};

    void DoAsyncDispatchContinuation() noexcept { _Context->Reschedule(this); }
};

class FutureError : public std::logic_error {
public:
    NRTCORE_API explicit FutureError(FutureErrorCode ec);

    [[nodiscard]] const auto& Code() const noexcept { return _Code; }

    [[noreturn]] static void Throw(const FutureErrorCode ec) { throw FutureError(ec); }
private:
    FutureErrorCode _Code;
};

namespace InterOp {
    class SharedAssociatedStateBase {
        // Frequent Use, kept apart with others
        mutable std::atomic<uintptr_t> _Lock{0};

        // state control
        enum LockFlags : uintptr_t {
            SpinBit = 0b1u,
            WriteBit = 0b10u,
            ReadyBit = 0b100u,
            PmrBitRev = 0b111u
        };

        struct SyncPmrData {
            std::mutex Mutex{};
            std::condition_variable Cv{};
        };

        auto GetPmrAddress() const noexcept {
            return reinterpret_cast<SyncPmrData*>(_Lock.load() & (~uintptr_t(PmrBitRev)));
        }

        auto EnablePmr() const {
            const auto newPmr = Temp::New<SyncPmrData>();
            _Lock.fetch_or(reinterpret_cast<uintptr_t>(newPmr));
            return newPmr;
        }

        void NotifyIfPmrEnabled() const noexcept {
            if (const auto pmr = GetPmrAddress(); pmr) {
                std::lock_guard<std::mutex> lk(pmr->Mutex);
                pmr->Cv.notify_all();
            }
        }

        bool TryAcquireWriteAccess() const noexcept {
            SpinWait spinner{};
            for (;;) {
                while (CheckWriteBit() && (!CheckReadyBit())) { spinner.SpinOnce(); }
                if (CheckReadyBit()) return false;
                auto _ = _Lock.load();
                if (_Lock.compare_exchange_strong(_, _ | WriteBit, std::memory_order_acquire)) return true;
            }
        }

        bool CheckSpinLock() const noexcept {
            return static_cast<bool>(_Lock.load(std::memory_order_relaxed) & SpinBit);
        }

        void AcquireSpinLock() const noexcept {
            size_t iter = 0;
            for (;;) {
                while (CheckSpinLock())
                    if (++iter<100)
                        IDLE;
                    else std::this_thread::yield();
                auto _ = _Lock.load();
                if (_Lock.compare_exchange_strong(_, _ | SpinBit, std::memory_order_acquire)) return;
            }
        }

        void ReleaseSpinLock() const noexcept { _Lock.fetch_and(~uintptr_t(SpinBit)); }

        auto PrepareWait() const {
            if (const auto addr = GetPmrAddress(); addr) return addr;
            AcquireSpinLock();
            const auto pmr = EnablePmr();
            ReleaseSpinLock();
            return pmr;
        }

    protected:
        void PrepareWrite() const {
            const auto success = TryAcquireWriteAccess();
            if (!success) FutureError::Throw(FutureErrorCode::PromiseAlreadySatisfied);
        }

        void CompleteWrite() noexcept {
            _Lock.fetch_or(ReadyBit);
            NotifyIfPmrEnabled();
            DoScheduleContinuation();
        }

        void PrepareWriteUnsafe() const noexcept { (void) this; }

        void CompleteWriteUnsafe() noexcept {
            _Lock.fetch_or(ReadyBit | WriteBit);
            NotifyIfPmrEnabled();
            DoScheduleContinuation();
        }

        void CancelWrite() const noexcept { _Lock.fetch_and(~uintptr_t(WriteBit)); }
    public:
        void Wait() const {
            if (!IsReady()) {
                auto _ = PrepareWait();
                std::unique_lock<std::mutex> lk(_->Mutex);
                while (!IsReady())
                    _->Cv.wait(lk);
            }
        }

        template <class Clock, class Duration>
        bool WaitUntil(const std::chrono::time_point<Clock, Duration>& absTime) const {
            if (!IsReady()) {
                auto _ = PrepareWait();
                std::unique_lock<std::mutex> lk(_->Mutex);
                while (!IsReady() && Clock::now()<absTime)
                    _->Cv.wait_until(lk, absTime);
            }
            return IsReady();
        }

        template <class Rep, class Period>
        bool WaitFor(const std::chrono::duration<Rep, Period>& relTime) const {
            return WaitUntil(std::chrono::steady_clock::now()+relTime);
        }

        bool IsReady() const noexcept { return CheckNotExpiredReadyBit(); }
    private:
        bool CheckWriteBit() const noexcept { return static_cast<bool>(_Lock.load() & WriteBit); }

        bool CheckReadyBit() const noexcept { return static_cast<bool>(_Lock.load() & ReadyBit); }

        bool CheckNotExpiredReadyBit() const noexcept {
            return static_cast<bool>(_Lock.load() & (WriteBit | ReadyBit));
        }

        void MakeExpire() const noexcept { _Lock.fetch_and(~uintptr_t(WriteBit)); }
    protected:
        void PrepareGet() const {
            Wait();
            AcquireSpinLock();
            if (!CheckNotExpiredReadyBit()) {
                ReleaseSpinLock();
                FutureError::Throw(FutureErrorCode::FutureAlreadyRetrieved);
            }
            MakeExpire();
            ReleaseSpinLock();
            if (_Exception)
                std::rethrow_exception(_Exception);
        }

    public:
        bool Satisfied() const noexcept { return CheckReadyBit(); }
        bool Valid() const noexcept { return CheckNotExpiredReadyBit() && _Continuation==nullptr; }
        // Continuable
    public:
        void SetContinuation(AAsyncContinuationExecTask* _task) noexcept {
            if (_Continuation.exchange(_task)==reinterpret_cast<AAsyncContinuationExecTask*>(uintptr_t(~0u))) {
                DoScheduleContinuationAsPromiseAlreadyFulfilled();
            }
        }

    private:
        std::atomic<AAsyncContinuationExecTask*> _Continuation{nullptr};

        AAsyncContinuationExecTask* GetContinuationExec() noexcept {
            return _Continuation.exchange(reinterpret_cast<AAsyncContinuationExecTask*>(uintptr_t(~0u)));
        }

        void DoScheduleContinuation() noexcept {
            if (const auto continuationExec = GetContinuationExec(); continuationExec) {
                continuationExec->DoScheduleContinuation();
            }
        }

        void DoScheduleContinuationAsPromiseAlreadyFulfilled() noexcept {
            if (const auto continuationExec = GetContinuationExec(); continuationExec) {
                continuationExec->DoScheduleContinuationAsPromiseAlreadyFulfilled();
            }
        }

    public:
        virtual ~SharedAssociatedStateBase() { if (const auto _ = GetPmrAddress(); _) Temp::Delete(_); }

    private:
        // Data Store
        mutable std::atomic_int _RefCount{0};
        std::exception_ptr _Exception = nullptr;
    public:
        void SetException(const std::exception_ptr& p) {
            PrepareWrite();
            _Exception = p;
            CompleteWrite();
        }

        void SetExceptionUnsafe(const std::exception_ptr& p) noexcept {
            PrepareWriteUnsafe();
            _Exception = p;
            CompleteWriteUnsafe();
        }

        void Acquire() const noexcept { _RefCount.fetch_add(1); }

        void Release() const noexcept { if (_RefCount.fetch_sub(1)==1) Temp::Delete(this); }
    };

    template <class T>
    class SharedAssociatedState final : public SharedAssociatedStateBase {
    public:
        ~SharedAssociatedState() override { reinterpret_cast<T*>(&_Value)->~T(); }

        void SetValue(T&& v) {
            PrepareWrite();
            if constexpr (std::is_nothrow_move_constructible_v<T>)
                new(&_Value) T(std::forward<T>(v));
            else
                try { new(&_Value) T(std::forward<T>(v)); }
                catch (...) {
                    CancelWrite();
                    throw;
                }
            CompleteWrite();
        }

        void SetValue(const T& v) {
            PrepareWrite();
            if constexpr (std::is_nothrow_copy_constructible_v<T>)
                new(&_Value) T(v);
            else
                try { new(&_Value) T(v); }
                catch (...) {
                    CancelWrite();
                    throw;
                }
            CompleteWrite();
        }

        void SetValueUnsafe(T&& v) noexcept(std::is_nothrow_move_constructible_v<T>) {
            PrepareWriteUnsafe();
            new(&_Value) T(std::forward<T>(v));
            CompleteWriteUnsafe();
        }

        void SetValueUnsafe(const T& v) noexcept(std::is_nothrow_copy_constructible_v<T>) {
            PrepareWriteUnsafe();
            new(&_Value) T(v);
            CompleteWriteUnsafe();
        }

        T Get() {
            PrepareGet();
            return std::move(*reinterpret_cast<T*>(&_Value));
        }

    private:
        std::aligned_storage_t<sizeof(T), alignof(T)> _Value {};
    };

    template <class T>
    class SharedAssociatedState<T&> final : public SharedAssociatedStateBase {
    public:
        void SetValue(T& v) {
            PrepareWrite();
            _Value = std::addressof(v);
            CompleteWrite();
        }

        void SetValueUnsafe(T& v) noexcept {
            PrepareWriteUnsafe();
            _Value = std::addressof(v);
            CompleteWriteUnsafe();
        }

        T& Get() {
            PrepareGet();
            return *_Value;
        }

    private:
        T* _Value{};
    };

    template <>
    class SharedAssociatedState<void> final : public SharedAssociatedStateBase {
    public:
        void SetValue() {
            PrepareWrite();
            CompleteWrite();
        }

        void SetValueUnsafe() noexcept {
            PrepareWriteUnsafe();
            CompleteWriteUnsafe();
        }

        void Get() const { PrepareGet(); }
    };

}

template <class T>
class Future;

template <class T>
class Promise;

namespace InterOp {
    template <class Callable, class ...Ts>
    class DeferredCallable {
        template <std::size_t... I>
        auto Apply(std::index_sequence<I...>) { return _Function(std::move(std::get<I>(_Arguments))...); }

    protected:
        using ReturnType = std::invoke_result_t<std::decay_t<Callable>, std::decay_t<Ts>...>;
    public:
        explicit DeferredCallable(Callable&& call, Ts&& ... args)
                :_Function(std::forward<Callable>(call)), _Arguments(std::forward<Ts>(args)...) { }

        auto Invoke() { return Apply(std::make_index_sequence<std::tuple_size<decltype(_Arguments)>::value>()); }
    private:
        Callable _Function;
        std::tuple<Ts...> _Arguments;
    };

    template <class Callable>
    class DeferredCallable<Callable> {
    public:
        using ReturnType = std::invoke_result_t<std::decay_t<Callable>>;

        explicit DeferredCallable(Callable&& call)
                :_Function(std::forward<Callable>(call)) { }

        auto Invoke() { return _Function(); }
    private:
        Callable _Function;
    };

    template <class Callable, class ...Ts>
    class DeferredProcedureCallTask : public AAsyncContinuationExecTask, DeferredCallable<Callable, Ts...> {
    public:
        using ReturnType = typename DeferredCallable<Callable, Ts...>::ReturnType;

        explicit DeferredProcedureCallTask(Callable&& call, Ts&& ... args)
                :DeferredCallable<Callable, Ts...>(std::forward<Callable>(call), std::forward<Ts>(args)...) { }

        void Exec() noexcept override {
            try {
                if constexpr (std::is_same_v<ReturnType, void>) {
                    DeferredCallable<Callable, Ts...>::Invoke();
                    _Promise.SetValueUnsafe();
                }
                else
                    _Promise.SetValueUnsafe(DeferredCallable<Callable, Ts...>::Invoke());
            }
            catch (...) { _Promise.SetExceptionUnsafe(std::current_exception()); }
            Temp::Delete(this);
        }

        auto GetFuture() { return _Promise.GetFuture(); }
    private:
        Promise<ReturnType> _Promise{};
    };

    template <class Callable, class ...Ts>
    class ContinuationCallTask final : public AAsyncContinuationExecTask, DeferredCallable<Callable, Ts...> {
    public:
        using ReturnType = typename DeferredCallable<Callable, Ts...>::ReturnType;

        static_assert(std::is_same_v<ReturnType, void>, "Continuation Functions Must Return void");
        static_assert(std::is_nothrow_invocable_v<Callable, Ts...>, "Continuation Functions Must Not Throw Exceptions");

        explicit ContinuationCallTask(Callable&& call, Ts&& ... args)
                :DeferredCallable<Callable, Ts...>(std::forward<Callable>(call), std::forward<Ts>(args)...) { }

        void Exec() noexcept override {
            DeferredCallable<Callable, Ts...>::Invoke();
            Temp::Delete(this);
        }
    };

    template <class T>
    class FutureBase {
    protected:
        explicit FutureBase(SharedAssociatedState<T>* _) noexcept
                :_State(_) { _State->Acquire(); }

    public:
        FutureBase() = default;

        FutureBase(FutureBase&& other) noexcept
                :_State(other._State) { other._State = nullptr; }

        FutureBase& operator=(FutureBase&& other) noexcept {
            if (this!=std::addressof(other)) {
                ReleaseState();
                _State = other._State;
                other._State = nullptr;
            }
            return *this;
        }

        FutureBase(const FutureBase&) = delete;

        FutureBase& operator=(const FutureBase&) = delete;

        ~FutureBase() { ReleaseState(); }

        [[nodiscard]] bool HasState() const noexcept { return _State; }

        [[nodiscard]] bool Valid() const noexcept { return _State ? _State->Valid() : false; }

        [[nodiscard]] bool IsReady() const noexcept { return _State->IsReady(); }

        void Wait() const { _State->Wait(); }

        template <class Clock, class Duration>
        bool WaitUntil(const std::chrono::time_point<Clock, Duration>& time) const { return _State->WaitUntil(time); }

        template <class Rep, class Period>
        bool WaitFor(const std::chrono::duration<Rep, Period>& relTime) const { return _State->WaitUntil(relTime); }

        template <class Func>
        auto Then(Func fn, ContinuationFlag flag = ContinuationFlag::ExecOnCompletionInvocation,
                  AsyncContinuationContext* context = nullptr) {
            auto task = Temp::New<DeferredProcedureCallTask<std::decay_t<Func>, Future<T>>>(
                    std::forward<std::decay_t<Func>>(std::move(fn)),
                    Future(nullptr, _State)
            );
            task->Setup(flag, context);
            auto future = task->GetFuture();
            _State->SetContinuation(task);
            _State = nullptr;
            return future;
        }

        template <class Func>
        void ContinueWith(Func fn, ContinuationFlag flag = ContinuationFlag::ExecOnCompletionInvocation,
                          AsyncContinuationContext* context = nullptr) {
            const auto task = Temp::New<ContinuationCallTask<std::decay_t<Func>, Future<T>>>(
                    std::forward<std::decay_t<Func>>(std::move(fn)),
                    Future(nullptr, _State)
            );
            task->Setup(flag, context);
            _State->SetContinuation(task);
            _State = nullptr;
        }

        void Drop(SharedAssociatedState<T>* const st) noexcept {
            if (st==_State) { ReleaseState(); }
        }
    protected:
        void ReleaseState() noexcept {
            if (_State) {
                _State->Release();
                _State = nullptr;
            }
        }

        struct ReleaseRAII {
            explicit ReleaseRAII(FutureBase* _) noexcept
                    :_This(_) { }

            ReleaseRAII(const ReleaseRAII&) = delete;
            ReleaseRAII& operator=(const ReleaseRAII&) = delete;
            ReleaseRAII(ReleaseRAII&&) = delete;
            ReleaseRAII& operator=(ReleaseRAII&&) = delete;
            ~ReleaseRAII() noexcept { _This->ReleaseState(); }
            FutureBase* _This;
        };

        auto CreateRaii() noexcept { return ReleaseRAII(this); }

        SharedAssociatedState<T>* _State = nullptr;
    };

    template <class T>
    class PromiseBase {
    public:
        PromiseBase() = default;

        PromiseBase(PromiseBase&& other) noexcept
                :_State(other._State) { other._State = nullptr; }

        PromiseBase& operator=(PromiseBase&& other) noexcept {
            if (this!=std::addressof(other)) {
                ReleaseState();
                _State = other._State;
                other._State = nullptr;
            }
            return *this;
        }

        PromiseBase(const PromiseBase&) = delete;

        PromiseBase& operator=(const PromiseBase&) = delete;

        ~PromiseBase() noexcept { ReleaseState(); }

        void Drop(Future<T>& fut) { fut.Drop(_State); }

        Future<T> GetFuture() { return Future<T>(_State); }

        void SetException(std::exception_ptr p) { _State->SetException(p); };

        void SetExceptionUnsafe(std::exception_ptr p) noexcept { _State->SetExceptionUnsafe(p); }
    protected:
        SharedAssociatedState<T>& GetState() {
            if (_State) return *_State;
            FutureError::Throw(FutureErrorCode::NoState);
        }

        SharedAssociatedState<T>* _State = MakeState();
    private:
        void ReleaseState() noexcept {
            if (_State) {
                if (!_State->Satisfied())
                    SetExceptionUnsafe(std::make_exception_ptr(FutureError(FutureErrorCode::BrokenPromise)));
                _State->Release();
                _State = nullptr;
            }
        }

        static SharedAssociatedState<T>* MakeState() {
            auto _ = Temp::New<SharedAssociatedState<T>>();
            _->Acquire();
            return _;
        }
    };
}

template <class T>
class Future : public InterOp::FutureBase<T> {
    friend class InterOp::PromiseBase<T>;
    friend class InterOp::FutureBase<T>;

    explicit Future(InterOp::SharedAssociatedState<T>* _) noexcept
            :InterOp::FutureBase<T>(_) { }

    Future(std::nullptr_t,
           InterOp::SharedAssociatedState<T>* _) noexcept { InterOp::FutureBase<T>::_State = _; }

public:
    Future() noexcept = default;

    auto Get() {
        auto _ = InterOp::FutureBase<T>::CreateRaii();
        return InterOp::FutureBase<T>::_State->Get();
    }
};

template <class T>
class Future<T&> : public InterOp::FutureBase<T&> {
    friend class InterOp::PromiseBase<T&>;
    friend class InterOp::FutureBase<T&>;

    explicit Future(InterOp::SharedAssociatedState<T&>* _) noexcept
            :InterOp::FutureBase<T&>(_) { }

    Future(std::nullptr_t,
           InterOp::SharedAssociatedState<T&>* _) noexcept { InterOp::FutureBase<T&>::template _State = _; }

public:
    Future() noexcept = default;

    auto& Get() {
        auto _ = InterOp::FutureBase<T&>::CreateRaii();
        return InterOp::FutureBase<T&>::_State->Get();
    }
};

template <>
class Future<void> : public InterOp::FutureBase<void> {
    friend class InterOp::PromiseBase<void>;
    friend class InterOp::FutureBase<void>;

    explicit Future(InterOp::SharedAssociatedState<void>* _) noexcept
            :FutureBase<void>(_) { }

    Future(std::nullptr_t, InterOp::SharedAssociatedState<void>* _) noexcept { _State = _; }
public:
    Future() noexcept = default;

    void Get() {
        auto _ = CreateRaii();
        _State->Get();
    }
};

template <class T>
class Promise : public InterOp::PromiseBase<T> {
public:
    void SetValue(T&& v) { InterOp::PromiseBase<T>::GetState().SetValue(std::move(v)); }

    void SetValue(const T& v) { InterOp::PromiseBase<T>::GetState().SetValue(v); }

    void SetValueUnsafe(T&& v) noexcept(std::is_nothrow_move_constructible_v<T>) {
        InterOp::PromiseBase<T>::GetState().SetValueUnsafe(std::move(v));
    }

    void SetValueUnsafe(const T& v) noexcept(std::is_nothrow_copy_constructible_v<T>) {
        InterOp::PromiseBase<T>::GetState().SetValueUnsafe(v);
    }
};

template <class T>
class Promise<T&> : public InterOp::PromiseBase<T&> {
public:
    void SetValue(T& v) { InterOp::PromiseBase<T&>::template GetState().SetValue(v); }

    void SetValueUnsafe(T& v) noexcept { InterOp::PromiseBase<T&>::template GetState().SetValueUnsafe(v); }
};

template <>
class Promise<void> : public InterOp::PromiseBase<void> {
public:
    void SetValue() { GetState().SetValue(); }

    void SetValueUnsafe() noexcept { GetState().SetValueUnsafe(); }
};
