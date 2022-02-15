#pragma once

#include "System/PmrBase.h"
#include "Coro/ValueAsync.h"
#include "Types.h"
#include "Status.h"
#include <variant>

namespace IO {
    class Stream : public PmrBase {
    public:
        virtual ValueAsync<IOResult> Read(uint64_t buffer, uint64_t size) = 0;

        virtual ValueAsync<IOResult> Write(uint64_t buffer, uint64_t size) = 0;

        virtual ValueAsync<IOResult> ReadV(Buffer *vec, int count) = 0;

        virtual ValueAsync<IOResult> WriteV(Buffer *vec, int count) = 0;

        virtual ValueAsync<Status> Close() = 0;
    };

    class Address {
    public:
        enum Family {
            AF_IPv4, AF_IPv6, AF_UNIX
        };

        static Address CreateIPv4(std::byte *data) noexcept;

        static Address CreateIPv6(std::byte *data) noexcept;

        static std::optional<Address> CreateIPv4(std::string_view text) noexcept;

        static std::optional<Address> CreateIPv6(std::string_view text) noexcept;

        [[nodiscard]] auto GetFamily() const noexcept { return mFamily; }

        [[nodiscard]] auto GetData() const noexcept { return mStorage; }

    private:
        Family mFamily;
        std::byte mStorage[16];
    };

    ValueAsync<std::unique_ptr<Stream>> Connect(Address address, int port);

    class StreamAcceptor : public PmrBase {
    public:
        struct Result {
            Status Stat{IO_OK};
            Address Peer{};
            std::unique_ptr<Stream> Handle{nullptr};
        };

        virtual ValueAsync<Result> Once() = 0;

        virtual ValueAsync<Status> Close() = 0;
    };

    std::unique_ptr<StreamAcceptor> CreateAcceptor(Address address, int port, int backlog);
}