#pragma once

#include <memory>

template<class T, class Alloc, class... Types>
T* allocator_construct(Alloc& alloc, Types&&... args) {
    using traits = std::allocator_traits<Alloc>;
    const auto result = traits::allocate(alloc, 1);
    traits::construct(alloc, result, std::forward<Types>(args)...);
    return result;
}

template<class T, class Alloc>
void allocator_destruct(Alloc& alloc, T* ptr) {
    using traits = std::allocator_traits<Alloc>;
    traits::destroy(alloc, ptr);
    traits::deallocate(alloc, ptr, 1);
}
