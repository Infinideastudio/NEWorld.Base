#pragma once

template <class T>
class Lock {
public:
    void lock() noexcept { mLock.Enter(); }
    void unlock() noexcept { mLock.Leave(); }
private:
    T mLock;
};

template <class T>
class LockRef {
public:
    explicit LockRef(T& l) noexcept: mLock(l) {}
    void lock() noexcept { mLock.Enter(); }
    void unlock() noexcept { mLock.Leave(); }
private:
    T& mLock;
};
