#pragma once

#include <atomic>
#include <coroutine>
#include <liburing.h>
#include "Temp/Temp.h"
#include "Conc/SpinLock.h"
#include "IO/Status.h"

namespace IO::Internal {
    class Core {
        static constexpr int QUEUE_DEPTH = 8192;
    public:
        enum Ops {
            Open, Read, Write, Sync, Close, Send, Recv, SendMsg, RecvMsg, Accept, Connect
        };

        struct Await {
            constexpr Await() noexcept = default;

            template <class Fn> requires std::is_invocable_v<Fn, Await*>
            explicit Await(Fn&& fn) noexcept { fn(this); }

            Await(Await &&) = delete;

            Await(const Await &) = delete;

            Await &operator=(Await &&) = delete;

            Await &operator=(const Await &) = delete;

            [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

            bool await_suspend(std::coroutine_handle<> h) {
                const auto address = h.address();
                for (;;) {
                    auto val = mNext.load();
                    if (val == INVALID_PTR) return false;
                    if (mNext.compare_exchange_weak(val, address)) return true;
                }
            }

            [[nodiscard]] int32_t await_resume() const noexcept { return mStatus; }

            void Release(int32_t status) {
                mStatus = status;
                if (auto ths = mNext.exchange(INVALID_PTR); ths) {
                    std::coroutine_handle<>::from_address(ths).resume();
                }
            }

        private:
            inline static void *INVALID_PTR = std::bit_cast<void *>(~uintptr_t(0));
            int32_t mStatus{};
            std::atomic<void *> mNext{nullptr};
        };

        Core() {
            io_uring_queue_init(QUEUE_DEPTH, &mRing, 0);
            std::thread([this]() { while (WaitOneCqe()); }).detach();
        }

        ~Core() { io_uring_queue_exit(&mRing); }

        template<Ops Op, class ...Args>
        static auto Wrap(Core &c, Args &&... args) {
            auto *sqe = GetSqe(c);
            if constexpr(Op == Ops::Open) io_uring_prep_openat(sqe, std::forward<Args>(args)...);
            else if constexpr(Op == Ops::Read) io_uring_prep_read(sqe, std::forward<Args>(args)...);
            else if constexpr(Op == Ops::Write) io_uring_prep_write(sqe, std::forward<Args>(args)...);
            else if constexpr(Op == Ops::Sync) io_uring_prep_fsync(sqe, std::forward<Args>(args)...);
            else if constexpr(Op == Ops::Close) io_uring_prep_close(sqe, std::forward<Args>(args)...);
            else if constexpr(Op == Ops::Send) io_uring_prep_send(sqe, std::forward<Args>(args)...);
            else if constexpr(Op == Ops::Recv) io_uring_prep_recv(sqe, std::forward<Args>(args)...);
            else if constexpr(Op == Ops::SendMsg) io_uring_prep_sendmsg(sqe, std::forward<Args>(args)...);
            else if constexpr(Op == Ops::RecvMsg) io_uring_prep_recvmsg(sqe, std::forward<Args>(args)...);
            else if constexpr(Op == Ops::Accept) io_uring_prep_accept(sqe, std::forward<Args>(args)...);
            else if constexpr(Op == Ops::Connect) io_uring_prep_connect(sqe, std::forward<Args>(args)...);
            return [&c, sqe](Await *ths) noexcept {
                io_uring_sqe_set_data(sqe, ths);
                io_uring_submit(&c.mRing);
            };
        }

        template<Ops Op, class ...Args>
        static auto Create(Core &c, Args &&... args) { return Await{Wrap<Op>(c, std::forward<Args>(args)...)}; }

        static auto &Get() {
            static Core ins{};
            return ins;
        }

        // We need the lock as we are not able to gather wait CQEs
        // Which made it not practical to use multiple rings to submit.
        // Since the ring itself is not constructed with thread safe ring,
        //     we need a large lock around the ring for safe submission
        SpinLock Lock{};
    private:
        io_uring mRing{};

        bool WaitOneCqe() {
            io_uring_cqe *cqe{};
            if (const auto ret = io_uring_wait_cqe(&mRing, &cqe); ret == 0) {
                static_cast<Await *>(io_uring_cqe_get_data(cqe))->Release(cqe->res);
                io_uring_cqe_seen(&mRing, cqe);
                return true;
            } else return (ret != ENXIO); // break if instance shutdown
        }

        static io_uring_sqe *GetSqe(Core &c) noexcept {
            SpinWait spin{};
            for (;;) if (const auto sqe = io_uring_get_sqe(&c.mRing); sqe) return sqe; else spin.SpinOnce();
        }
    };
}