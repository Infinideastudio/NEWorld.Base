#pragma once

#include <cstdint>

namespace IO {
    struct Buffer {
        void* Memory;
        uint32_t Size;
    };
}
