#pragma once

#include "IO/Status.h"

namespace IO::Internal {
    constexpr Status MapError(int32_t error) noexcept {
        switch(error) {
            case EACCES: return IO_EACCES;
            case EADDRINUSE: return IO_EADDRINUSE;
            case EADDRNOTAVAIL: return IO_EADDRNOTAVAIL;
            case EAFNOSUPPORT: return IO_EAFNOSUPPORT;
            case EAGAIN: return IO_EAGAIN;
            case EALREADY: return IO_EALREADY;
            case EBADF: return IO_EBADF;
            case EBUSY: return IO_EBUSY;
            case ECANCELED: return IO_ECANCELED;
            case ECONNABORTED: return IO_ECONNABORTED;
            case ECONNREFUSED: return IO_ECONNREFUSED;
            case ECONNRESET: return IO_ECONNRESET;
            case EDESTADDRREQ: return IO_EDESTADDRREQ;
            case EEXIST: return IO_EEXIST;
            case EFAULT: return IO_EFAULT;
            case E2BIG: return IO_E2BIG;
            case EHOSTUNREACH: return IO_EHOSTUNREACH;
            case EINTR: return IO_EINTR;
            case EINVAL: return IO_EINVAL;
            case EIO: return IO_EIO;
            case EISCONN: return IO_EISCONN;
            case EISDIR: return IO_EISDIR;
            case ELOOP: return IO_ELOOP;
            case EMFILE: return IO_EMFILE;
            case EMSGSIZE: return IO_EMSGSIZE;
            case ENAMETOOLONG: return IO_ENAMETOOLONG;
            case ENETDOWN: return IO_ENETDOWN;
            case ENETUNREACH: return IO_ENETUNREACH;
            case ENFILE: return IO_ENFILE;
            case ENOBUFS: return IO_ENOBUFS;
            case ENODEV: return IO_ENODEV;
            case ENOENT: return IO_ENOENT;
            case ENOMEM: return IO_ENOMEM;
            case ENONET: return IO_ENONET;
            case ENOPROTOOPT: return IO_ENOPROTOOPT;
            case ENOSPC: return IO_ENOSPC;
            case ENOSYS: return IO_ENOSYS;
            case ENOTCONN: return IO_ENOTCONN;
            case ENOTDIR: return IO_ENOTDIR;
            case ENOTEMPTY: return IO_ENOTEMPTY;
            case ENOTSOCK: return IO_ENOTSOCK;
            case ENOTSUP: return IO_ENOTSUP;
            case EOVERFLOW: return IO_EOVERFLOW;
            case EPERM: return IO_EPERM;
            case EPIPE: return IO_EPIPE;
            case EPROTO: return IO_EPROTO;
            case EPROTONOSUPPORT: return IO_EPROTONOSUPPORT;
            case EPROTOTYPE: return IO_EPROTOTYPE;
            case ERANGE: return IO_ERANGE;
            case EROFS: return IO_EROFS;
            case ESHUTDOWN: return IO_ESHUTDOWN;
            case ESPIPE: return IO_ESPIPE;
            case ESRCH: return IO_ESRCH;
            case ETIMEDOUT: return IO_ETIMEDOUT;
            case ETXTBSY: return IO_ETXTBSY;
            case EXDEV: return IO_EXDEV;
            case EOF: return IO_EOF;
            case ENXIO: return IO_ENXIO;
            case EMLINK: return IO_EMLINK;
            case ENOTTY: return IO_ENOTTY;
            case EILSEQ: return IO_EILSEQ;
            case ESOCKTNOSUPPORT: return IO_ESOCKTNOSUPPORT;
            default: return IO_UNKNOWN;
        }
    }

    constexpr IOResult MapResult(int32_t ret) noexcept {
        if (ret >= 0) return IOResult(IO_OK, ret); else return IOResult(MapError(ret));
    }
}