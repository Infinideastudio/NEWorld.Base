#pragma once

#include <exception>

namespace IO {
    enum Status {
        IO_OK,
        IO_EACCES, //permission denied
        IO_EADDRINUSE, //address already in use
        IO_EADDRNOTAVAIL, //address not available
        IO_EAFNOSUPPORT, // address family not supported
        IO_EAGAIN, // resource temporarily unavailable
        IO_EALREADY, // connection already in progress
        IO_EBADF, // bad file descriptor
        IO_EBUSY, // resource busy or locked
        IO_ECANCELED, // operation canceled
        IO_ECHARSET, // invalid Unicode character
        IO_ECONNABORTED, //software caused connection abort
        IO_ECONNREFUSED, //connection refused
        IO_ECONNRESET, //connection reset by peer
        IO_EDESTADDRREQ, //destination address required
        IO_EEXIST, // file already exists
        IO_EFAULT, //bad address in system call argument
        IO_E2BIG, // file too large
        IO_EHOSTUNREACH, // host is unreachable
        IO_EINTR, // interrupted system call
        IO_EINVAL, // invalid argument
        IO_EIO, // i/o error
        IO_EISCONN, // socket is already connected
        IO_EISDIR, // illegal operation on a directory
        IO_ELOOP, // too many symbolic links encountered
        IO_EMFILE, // too many open files
        IO_EMSGSIZE, // message too long
        IO_ENAMETOOLONG, // name too long
        IO_ENETDOWN, // network is down
        IO_ENETUNREACH, // network is unreachable
        IO_ENFILE, // file table overflow
        IO_ENOBUFS, // no buffer space available
        IO_ENODEV, // no such device
        IO_ENOENT, // no such file or directory
        IO_ENOMEM, // not enough memory
        IO_ENONET, // machine is not on the network
        IO_ENOPROTOOPT, // protocol not available
        IO_ENOSPC, // no space left on device
        IO_ENOSYS, // function not implemented
        IO_ENOTCONN, // socket is not connected
        IO_ENOTDIR, // not a directory
        IO_ENOTEMPTY, // directory not empty
        IO_ENOTSOCK, // socket operation on non-socket
        IO_ENOTSUP, // operation not supported on socket
        IO_EOVERFLOW, // value too large for defined data type
        IO_EPERM, // operation not permitted
        IO_EPIPE, // broken pipe
        IO_EPROTO, // protocol error
        IO_EPROTONOSUPPORT, // protocol not supported
        IO_EPROTOTYPE, // protocol wrong type for socket
        IO_ERANGE, // result too large
        IO_EROFS, // read-only file system
        IO_ESHUTDOWN, // cannot send after transport endpoint shutdown
        IO_ESPIPE, // invalid seek
        IO_ESRCH, // no such process
        IO_ETIMEDOUT, // connection timed out
        IO_ETXTBSY, // text file is busy
        IO_EXDEV, // cross-device link not permitted
        IO_UNKNOWN, // unknown error
        IO_EOF, // end of file
        IO_ENXIO, // no such device or address
        IO_EMLINK, // too many links
        IO_ENOTTY, // inappropriate ioctl for device
        IO_EFTYPE, // inappropriate file type or format
        IO_EILSEQ, // illegal byte sequence
        IO_ESOCKTNOSUPPORT // socket type not supported
    };

    struct exception_errc : std::exception {
        Status errc;

        explicit exception_errc(const Status status) noexcept: errc(status) {}
    };

    class IOResult final {
    public:
        explicit constexpr IOResult(Status status, int32_t result = 0) noexcept:
                mValue(status == IO_OK ? result : -status) {}

        [[nodiscard]] bool success() const noexcept { return mValue >= 0; }

        [[nodiscard]] Status error() const noexcept { return success() ? IO_OK : Status(-mValue); }

        [[nodiscard]] int32_t result() const noexcept { return mValue; }

    private:
        int32_t mValue;
    };
}