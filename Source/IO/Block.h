#pragma once

#include <string_view>
#include "System/PmrBase.h"
#include "Coro/ValueAsync.h"
#include "Status.h"

namespace IO {
    class Block: public PmrBase {
    public:
        enum Flag {
            F_READ = 1ul,
            F_WRITE = 2ul,
            F_CREAT = 4ul,
            F_EXCL = 8ul,
            F_TRUNC = 16ul,
            F_EXLOCK = 32ul
        };

        virtual ValueAsync<IOResult> Read(uint64_t buffer, uint64_t size, uint64_t offset) = 0;

        virtual ValueAsync<IOResult> Write(uint64_t buffer, uint64_t size, uint64_t offset) = 0;

        virtual ValueAsync<Status> ReadA(uint64_t *buffers, uint64_t *sizes, uint64_t *offsets, uint64_t *spans) = 0;

        virtual ValueAsync<Status> WriteA(uint64_t *buffers, uint64_t *sizes, uint64_t *offsets, uint64_t *spans) = 0;

        virtual ValueAsync<Status> Sync() = 0;

        virtual ValueAsync<Status> Close() = 0;
    };

    ValueAsync<std::unique_ptr<Block>> OpenBlock(std::string_view path, uint32_t flags);
}