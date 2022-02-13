#include "IO/Block.h"
#include "Uring.h"
#include "Error.h"
#include "Temp/Deque.h"
#include "System/FileSystem.h"
#include <vector>
#include <fcntl.h>

using namespace IO;
using Internal::Core;

namespace {
    temp::deque<std::array<uint64_t, 3>> SpliceComplex(
            uint64_t *buffers, uint64_t *sizes,
            uint64_t *offsets, uint64_t *spans
    ) {
        auto result = temp::deque<std::array<uint64_t, 3>>();
        uint64_t offset = *offsets, span = *spans, head = *buffers, size = *sizes;
        for (;;) {
            while (size > 0) {
                const auto slice = std::min(size, span);
                result.push_back({head, offset, slice});
                head += slice, size -= slice, offset += slice, span -= slice;
                if (span == 0) (offset = *(++offsets), span = *(++spans));
                if (span == 0) return result;
            }
            head = *(++buffers), size = *(++sizes);
            if (size == 0) throw IO::exception_errc(IO::IO_EINVAL);
        }
    }

    uint32_t FlagConv(uint32_t flags) {
        uint32_t result = 0;
        const auto read = flags & Block::Flag::F_READ;
        const auto write = flags & Block::Flag::F_WRITE;
        if (read && write)
            result |= O_RDWR;
        else {
            if (read) result |= O_RDONLY;
            if (write) result |= O_WRONLY;
            if (!read && !write) throw exception_errc(IO_EACCES);
        }
        if (flags & Block::Flag::F_CREAT) result |= O_CREAT;
        if (flags & Block::Flag::F_EXCL) result |= O_EXCL;
        if (flags & Block::Flag::F_TRUNC) result |= O_TRUNC;
        return result;
    }

    class Impl final : public Block {
        template <Core::Ops Op>
        ValueAsync<IOResult> Simple(uint64_t buffer, uint64_t size, uint64_t offset) {
            auto &core = Core::Get();
            core.Lock.Enter();
            auto action = Core::Create<Op>(core, mFd, reinterpret_cast<void *>(buffer), size, offset);
            core.Lock.Leave();
            co_return Internal::MapResult(co_await action);
        }

        template <Core::Ops Op>
        auto PrepareComplex(uint64_t *buffers, uint64_t *sizes, uint64_t *offsets, uint64_t *spans) {
            auto &core = Core::Get();
            auto slices = SpliceComplex(buffers, sizes, offsets, spans);
            auto fin = std::make_unique<Core::Await[]>(slices.size());
            auto iter = fin.get();
            core.Lock.Enter();
            auto total = uint64_t(0);
            for (auto &&[buffer, offset, size]: slices) {
                total += size;
                auto wrap = Core::Wrap<Op>(core, mFd, reinterpret_cast<void *>(buffer), size, offset);
                std::construct_at(iter++, wrap);
            }
            core.Lock.Leave();
            return std::tuple{std::move(fin), slices.size(), total};
        }

        template <Core::Ops Op>
        ValueAsync<Status> Complex(uint64_t *buffers, uint64_t *sizes, uint64_t *offsets, uint64_t *spans) {
            auto&&[fin, count, total] = PrepareComplex<Op>(buffers, sizes, offsets, spans);
            auto completed = uint64_t(0);
            auto aggregated = Status::IO_OK;
            for (auto i = 0; i < count; ++i) {
                auto& fi = fin[i];
                const auto result = Internal::MapResult(co_await fi);
                if (!result.success() && aggregated == Status::IO_OK)
                    aggregated = result.error();
                else
                    completed += result.result();
            }
            if (completed != total && aggregated == Status::IO_OK) aggregated = Status::IO_EIO;
            co_return aggregated;
        }

    public:
        explicit Impl(int fd) noexcept: mFd{fd} {}

        ValueAsync<IOResult> Read(uint64_t buffer, uint64_t size, uint64_t offset) override {
            return Simple<Core::Read>(buffer, size, offset);
        }

        ValueAsync<IOResult> Write(uint64_t buffer, uint64_t size, uint64_t offset) override {
            return Simple<Core::Write>(buffer, size, offset);
        }

        ValueAsync<Status> ReadA(uint64_t *buffers, uint64_t *sizes, uint64_t *offsets, uint64_t *spans) override {
            return Complex<Core::Read>(buffers, sizes, offsets, spans);
        }

        ValueAsync<Status> WriteA(uint64_t *buffers, uint64_t *sizes, uint64_t *offsets, uint64_t *spans) override {
            return Complex<Core::Write>(buffers, sizes, offsets, spans);
        }

        ValueAsync<Status> Sync() override {
            auto &core = Core::Get();
            core.Lock.Enter();
            auto action = Core::Create<Core::Sync>(core, mFd, IORING_FSYNC_DATASYNC);
            core.Lock.Leave();
            co_return Internal::MapError(co_await action);
        }

        ValueAsync<Status> Close() override {
            auto &core = Core::Get();
            core.Lock.Enter();
            auto sync = Core::Create<Core::Sync>(core, mFd, IORING_FSYNC_DATASYNC);
            core.Lock.Leave();
            if (auto ret = Internal::MapError(co_await sync); ret == IO::IO_OK) {
                core.Lock.Enter();
                auto action = Core::Create<Core::Close>(core, mFd);
                core.Lock.Leave();
                co_return Internal::MapError(co_await action);
            } else co_return ret;
        }

        static ValueAsync<int> Open(std::string_view path, uint32_t flags) {
            auto &core = Core::Get();
            auto absolute = NEWorld::filesystem::absolute({path}).generic_string();
            core.Lock.Enter();
            auto open = Core::Create<Core::Open>(core, 0, absolute.c_str(), FlagConv(flags), 00600);
            core.Lock.Leave();
            if (const auto r = Internal::MapResult(co_await open); r.success())
                co_return r.result();
            else
                throw exception_errc(r.error());
        }

    private:
        const int mFd;
    };
}

ValueAsync<std::unique_ptr<Block>> IO::OpenBlock(std::string_view path, uint32_t flags) {
    co_return std::make_unique<Impl>(co_await Impl::Open(path, flags));
}