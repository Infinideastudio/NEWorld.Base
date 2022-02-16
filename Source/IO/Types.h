#pragma once

#include <cstdint>
#include <type_traits>

namespace IO {
    class Buffer {
    public:
        template<class T>
        requires std::is_integral_v<T>
        constexpr Buffer(void *mem, T size) noexcept : mMem(reinterpret_cast<uintptr_t>(mem)), mSize(size) {}

        template<class T>
        requires std::is_integral_v<T>
        constexpr Buffer(const void *mem, T size) noexcept : mMem(reinterpret_cast<uintptr_t>(mem)), mSize(size) {}

        [[nodiscard]] auto GetMem() const noexcept { return reinterpret_cast<void*>(mMem); }

        [[nodiscard]] auto GetSize() const noexcept { return mSize; }
    private:
        uintptr_t mMem;
        uint32_t mSize;
    };
}
