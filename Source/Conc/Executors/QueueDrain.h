#pragma once

namespace Internal::Executor {
    template<template<class> class Queue, class Task>
    class QueueDrain {
    public:
        void Add(const Task &t) { mQueue.Add(t); }

        void Drain() noexcept {
            for (;;) if (auto exec = mQueue.Get(); exec.Item) (*exec.Item.*exec.Entry)(); else return;
        }

        [[nodiscard]] bool ShouldActive() noexcept { return mQueue.SnapshotNotEmpty(); }

        void Finalize() { mQueue.Finalize(); }
    private:
        Queue<Task> mQueue;
    };
}
