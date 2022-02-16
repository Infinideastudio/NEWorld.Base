#include "IO/Stream.h"
#include "Uring.h"
#include "Error.h"
#include <vector>
#include <cstring>
#include <arpa/inet.h>

using namespace IO;
using Internal::Core;

namespace {
    class StreamImpl : public Stream {
        template<Core::Ops Op>
        ValueAsync<IOResult> Simple(Buffer buffer) {
            auto &core = Core::Get();
            core.Lock.Enter();
            auto action = Core::Create<Op>(core, mFd, buffer.GetMem(), buffer.GetSize(), 0);
            core.Lock.Leave();
            co_return Internal::MapResult(co_await action);
        }

        template<Core::Ops Op>
        ValueAsync<IOResult> Aggregated(Buffer *vec, int count) {
            auto &core = Core::Get();
            std::vector<iovec> mapped{static_cast<size_t>(count)};
            for (auto it = vec, end = vec + count; it < end; ++it) {
                mapped.push_back({it->GetMem(), it->GetSize()});
            }
            msghdr message{
                    .msg_name = nullptr, .msg_namelen = 0,
                    .msg_iov = mapped.data(), .msg_iovlen = mapped.size(),
                    .msg_control = nullptr, .msg_controllen = 0, .msg_flags = 0
            };
            core.Lock.Enter();
            auto action = Core::Create<Op>(core, mFd, &message, 0);
            core.Lock.Leave();
            co_return Internal::MapResult(co_await action);
        }

    public:
        explicit StreamImpl(int socket) noexcept: mFd(socket) {}

        ValueAsync<IOResult> Read(Buffer buffer) override {
            return Simple<Core::Recv>(buffer);
        }

        ValueAsync<IOResult> Write(Buffer buffer) override {
            return Simple<Core::Send>(buffer);
        }

        ValueAsync<IOResult> ReadV(Buffer *vec, int count) override {
            return Aggregated<Core::RecvMsg>(vec, count);
        }

        ValueAsync<IOResult> WriteV(Buffer *vec, int count) override {
            return Aggregated<Core::SendMsg>(vec, count);
        }

        ValueAsync<Status> Close() override {
            auto &core = Core::Get();
            core.Lock.Enter();
            auto action = Core::Create<Core::Close>(core, mFd);
            core.Lock.Leave();
            co_return Internal::MapError(co_await action);
        }

    private:
        const int mFd;
    };

    class AcceptImpl : public StreamAcceptor {
    public:
        explicit AcceptImpl(int socket) noexcept: mFd(socket) {}

        ValueAsync<Status> Close() override {
            auto &core = Core::Get();
            core.Lock.Enter();
            auto action = Core::Create<Core::Close>(core, mFd);
            core.Lock.Leave();
            co_return Internal::MapError(co_await action);
        }

    protected:
        const int mFd;
    };

    class AcceptImpl4 : public AcceptImpl {
    public:
        using AcceptImpl::AcceptImpl;

        ValueAsync<Result> Once() override {
            auto &core = Core::Get();
            sockaddr_in address{};
            socklen_t length = sizeof(address);
            core.Lock.Enter();
            auto action = Core::Create<Core::Accept>(core, mFd, reinterpret_cast<sockaddr *>(&address), &length, 0);
            core.Lock.Leave();
            const auto result = Internal::MapResult(co_await action);
            if (!result.success()) co_return Result{.Stat = result.error()};
            co_return Result{
                    .Peer = Address::CreateIPv4(reinterpret_cast<std::byte *>(&(address.sin_addr.s_addr))),
                    .Handle = std::make_unique<StreamImpl>(result.result())
            };
        }
    };

    class AcceptImpl6 : public AcceptImpl {
    public:
        using AcceptImpl::AcceptImpl;

        ValueAsync<Result> Once() override {
            auto &core = Core::Get();
            sockaddr_in6 address{};
            socklen_t length = sizeof(address);
            core.Lock.Enter();
            auto action = Core::Create<Core::Accept>(core, mFd, reinterpret_cast<sockaddr *>(&address), &length, 0);
            core.Lock.Leave();
            const auto result = Internal::MapResult(co_await action);
            if (!result.success()) co_return Result{.Stat = result.error()};
            co_return Result{
                    .Peer = Address::CreateIPv6(reinterpret_cast<std::byte *>(&(address.sin6_addr.s6_addr))),
                    .Handle = std::make_unique<StreamImpl>(result.result())
            };
        }
    };
}

Address IO::Address::CreateIPv4(std::byte *data) noexcept {
    Address ret{};
    ret.mFamily = AF_IPv4;
    std::memcpy(ret.mStorage, data, 4);
    return ret;
}

Address IO::Address::CreateIPv6(std::byte *data) noexcept {
    Address ret{};
    ret.mFamily = AF_IPv6;
    std::memcpy(ret.mStorage, data, 16);
    return ret;
}

std::optional<Address> Address::CreateIPv4(std::string_view text) noexcept {
    if (in_addr p{}; inet_pton(AF_INET, text.data(), &p) == 1)
        return CreateIPv4(reinterpret_cast<std::byte *>(&p.s_addr));
    return std::nullopt;
}

std::optional<Address> Address::CreateIPv6(std::string_view text) noexcept {
    if (in6_addr p{}; inet_pton(AF_INET6, text.data(), &p) == 1)
        return CreateIPv6(reinterpret_cast<std::byte *>(&p.s6_addr));
    return std::nullopt;
}

namespace {
    ValueAsync<std::unique_ptr<Stream>> ConnectV4(Address address, int port) {
        const auto sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock != -1) {
            sockaddr_in in{.sin_family = AF_INET, .sin_port = htons(port)};
            auto &core = Core::Get();
            core.Lock.Enter();
            auto action = Core::Create<Core::Connect>(core, sock, reinterpret_cast<sockaddr *>(&in), sizeof(in));
            core.Lock.Leave();
            const auto result = Internal::MapResult(co_await action);
            if (result.success()) co_return std::make_unique<StreamImpl>(sock);
            close(sock);
        }
        throw exception_errc(Internal::MapError(errno));
    }

    ValueAsync<std::unique_ptr<Stream>> ConnectV6(Address address, int port) {
        const auto sock = socket(AF_INET6, SOCK_STREAM, 0);
        if (sock != -1) {
            sockaddr_in6 in{.sin6_family = AF_INET6, .sin6_port = htons(port)};
            auto &core = Core::Get();
            core.Lock.Enter();
            auto action = Core::Create<Core::Connect>(core, sock, reinterpret_cast<sockaddr *>(&in), sizeof(in));
            core.Lock.Leave();
            const auto result = Internal::MapResult(co_await action);
            if (result.success()) co_return std::make_unique<StreamImpl>(sock);
            close(sock);
            close(sock);
        }
        throw exception_errc(Internal::MapError(errno));
    }
}

ValueAsync<std::unique_ptr<Stream>> IO::Connect(Address address, int port) {
    switch (address.GetFamily()) {
        case Address::AF_IPv4:
            return ConnectV4(address, port);
        case Address::AF_IPv6:
            return ConnectV6(address, port);
        default:
            throw std::runtime_error("Invalid Peer Family");
    }
}

namespace {
    std::unique_ptr<StreamAcceptor> AcceptorV4(Address address, int port, int backlog) {
        const auto sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock != -1) {
            sockaddr_in target{.sin_family = AF_INET, .sin_port = htons(port)};
            if (bind(sock, reinterpret_cast<sockaddr *>(&target), sizeof(target)) == -1) goto error;
            if (listen(sock, backlog) != -1) return std::make_unique<AcceptImpl4>(sock);
            error:
            close(sock);
        }
        throw exception_errc(Internal::MapError(errno));
    }

    std::unique_ptr<StreamAcceptor> AcceptorV6(Address address, int port, int backlog) {
        const auto sock = socket(AF_INET6, SOCK_STREAM, 0);
        if (sock != -1) {
            sockaddr_in6 target{.sin6_family = AF_INET6, .sin6_port = htons(port)};
            if (bind(sock, reinterpret_cast<sockaddr *>(&target), sizeof(target)) == -1) goto error;
            if (listen(sock, backlog) != -1) return std::make_unique<AcceptImpl6>(sock);
            error:
            close(sock);
        }
        throw exception_errc(Internal::MapError(errno));
    }
}

std::unique_ptr<StreamAcceptor> IO::CreateAcceptor(Address address, int port, int backlog) {
    switch (address.GetFamily()) {
        case Address::AF_IPv4:
            return AcceptorV4(address, port, backlog);
        case Address::AF_IPv6:
            return AcceptorV6(address, port, backlog);
        default:
            throw std::runtime_error("Invalid Peer Family");
    }
}