#pragma once

#include <memory>
#include <cstdint>
#include <type_traits>

namespace tss {
    namespace internal {
        struct cleanup {
            void (*fn)(void *, void *) noexcept;

            void *user;

            void operator()(void *p) const noexcept { fn(p, user); }

            explicit operator bool() const noexcept { return fn; }
        };

        constexpr uint32_t invalid_key = 0xffffffff;

        uint32_t create(cleanup callback);

        void remove(uint32_t key) noexcept;

        void *get(uint32_t key) noexcept;

        void set(uint32_t key, void *p);

        template<class T>
        class pointer_base {
        public:
            using element_type = T;

            explicit pointer_base(cleanup clean) : m_key(create(clean)) {}

            pointer_base(pointer_base const &) = delete;

            pointer_base &operator=(pointer_base const &) = delete;

            pointer_base(pointer_base &&other) noexcept: m_key(other.m_key) { other.m_key = invalid_key; }

            pointer_base &operator=(pointer_base &&other) noexcept {
                if (m_key != invalid_key) remove(m_key);
                m_key = other.m_key;
                other.m_key = invalid_key;
            }

            ~pointer_base() noexcept { remove(m_key); }

            T *get() const noexcept { return static_cast<T *>(internal::get(m_key)); }

            T *operator->() const noexcept { return get(); }

            T &operator*() const noexcept { return *get(); }

            bool operator!() const noexcept { return !get(); }

        protected:
            uint32_t m_key;
        };

        template<class T, class Alloc, bool IsSame>
        class pointer_alloc_base;

        template<class T, class Alloc>
        class pointer_alloc_base<T, Alloc, true> : public pointer_base<T> {
        public:
            explicit pointer_alloc_base() : pointer_base<T>({&cleanup, nullptr}) {}

            template<class ...Ts>
            void emplace(Ts &&... args) {
                if (const auto p = pointer_base<T>::get(); p) if (m_cleanup) m_cleanup(p);
                Alloc alloc{};
                internal::set(pointer_base<T>::m_key, allocator_construct(alloc, std::forward<Ts>(args)...));
            }

            void clear() noexcept {
                if (const auto p = pointer_base<T>::get(); p) {
                    internal::set(pointer_base<T>::m_key, nullptr);
                    if (m_cleanup) m_cleanup(p);
                }
            }

        private:
            const internal::cleanup m_cleanup{&cleanup, nullptr};

            static void cleanup(void *p, void *u) noexcept {
                Alloc alloc{};
                if (p) allocator_destruct(alloc, static_cast<T *>(p));
            }
        };

        template<class T, class Alloc>
        class pointer_alloc_base<T, Alloc, false> : public internal::pointer_base<T> {
        public:
            explicit pointer_alloc_base() : pointer_base<T>(get_cleanup()) {}

            template<class ...Ts>
            void emplace(Ts &&... args) {
                if (const auto p = pointer_base<T>::get(); p) if (m_cleanup) m_cleanup(p);
                internal::set(pointer_base<T>::m_key, allocator_construct(m_alloc, std::forward<Ts>(args)...));
            }

            void clear() noexcept {
                if (const auto p = pointer_base<T>::get(); p) {
                    internal::set(pointer_base<T>::m_key, nullptr);
                    if (m_cleanup) m_cleanup(p);
                }
            }

            pointer_alloc_base(pointer_alloc_base &&) = delete;

            pointer_alloc_base &operator=(pointer_alloc_base &&) = delete;

        private:
            Alloc m_alloc;
            const internal::cleanup m_cleanup = get_cleanup();

            [[nodiscard]] internal::cleanup get_cleanup() const noexcept { return {&cleanup, this}; }

            static void cleanup(void *p, void *u) noexcept {
                if (p) allocator_destruct(reinterpret_cast<pointer_alloc_base *>(u)->m_alloc, static_cast<T *>(p));
            }
        };
    }

    template<class T, class Alloc = std::allocator<T>>
    class pointer : public internal::pointer_alloc_base<
            T, Alloc, std::is_same_v<typename std::allocator_traits<Alloc>::is_always_equal, std::true_type>
    > {
    };

    template<class T>
    class pointer<T, void> : public internal::pointer_base<T> {
    public:
        explicit pointer() : internal::pointer_base<T>({nullptr, nullptr}), m_cleanup{nullptr, nullptr} {}

        explicit pointer(void (*cleanup)(void *, void *) noexcept, void *user) :
                internal::pointer_base<T>({cleanup, user}), m_cleanup{cleanup, user} {}

        void reset(T *new_ptr = nullptr) noexcept {
            const auto old_ptr = internal::pointer_base<T>::get();
            if (new_ptr != old_ptr) {
                if (m_cleanup) m_cleanup(old_ptr);
                internal::set(internal::pointer_base<T>::m_key, new_ptr);
            }
        }

        T *release() noexcept {
            const auto p = internal::pointer_base<T>::get();
            if (p) internal::set(internal::pointer_base<T>::m_key, nullptr);
            return p;
        }

    private:
        const internal::cleanup m_cleanup;
    };

    template<typename T>
    inline T *get_pointer(pointer<T> const &ptr) noexcept { return ptr.get(); }
}
