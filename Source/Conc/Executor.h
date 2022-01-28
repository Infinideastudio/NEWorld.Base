#pragma once

#include <memory>
#include <Temp/Temp.h>
#include <System/PmrBase.h>

/*
concept Executable {
    void operator()() noexcept;
}

concept InstanceExecutable {
    void operator()(int instance) noexcept;
    bool ShouldSpawn() const noexcept;
}
*/

using TaskFn = void (Object::*)() noexcept;

struct Task {
    Object *Item{nullptr};
    TaskFn Entry{nullptr};
};

class IExecutor {
protected:
private:
    template<class T>
    struct Wrap : Object {
        using Alloc = temp_alloc<Wrap>;

        T Fn;

        explicit Wrap(T &&fn) : Fn(std::forward<T>(fn)) {}

        void Run() noexcept {
            auto alloc = Alloc{};
            (Fn(), allocator_destruct(alloc, this));
        }
    };
public:
    template<class Fn>
    void Enqueue(Fn &&fn) noexcept {
        using Type = Wrap<std::decay_t<Fn>>;
        auto alloc = typename Type::Alloc{};
        const auto ptr = allocator_construct<Type>(alloc, std::forward<Fn>(fn));
        (*this.*EnqueueRaw)(ptr, static_cast<TaskFn>(&Type::Run));
    }

protected:
    using FnEnqueue = void (IExecutor::*)(Object *o, TaskFn fn);

    explicit IExecutor(FnEnqueue enqueue) : EnqueueRaw{enqueue} {}

    FnEnqueue EnqueueRaw;
};

IExecutor* CurrentExecutor() noexcept;

std::shared_ptr<IExecutor> CreateSingleThreadExecutor();

std::shared_ptr<IExecutor> CreateScalingFIFOExecutor(int min, int max, int linger);

std::shared_ptr<IExecutor> CreateScalingBagExecutor(int min, int max, int linger);
