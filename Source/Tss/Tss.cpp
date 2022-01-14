#include <stack>
#include <mutex>
#include "Temp/Vector.h"
#include "Conc/SpinLock.h"
#include "Tss.h"

namespace tss::internal {
    class host {
    public:
        static auto &get() {
            static host instance{};
            return instance;
        }

        class context {
        public:
            context() noexcept { host::get().register_context(this); }

            ~context() {
                auto &host = host::get();
                while (!m_storage.empty()) {
                    std::vector<void *> storage;
                    storage.swap(m_storage);
                    for (uint32_t key = 0, n = static_cast<uint32_t>(storage.size()); key < n; ++key) {
                        if (const auto value = storage[key]; value) {
                            if (const auto cleanup = host.m_cleanups[key]; cleanup) cleanup(value);
                        }
                    }
                }
                host.unregister_context(this);
            }

            [[nodiscard]] void *get_value(uint32_t key) const noexcept {
                if (key < m_storage.size()) return m_storage[key]; else return nullptr;
            }

            void set_value(uint32_t key, void *value) {
                if (key >= m_storage.size())
                    m_storage.resize(key + 1, nullptr);
                m_storage[key] = value;
            }

        private:
            friend class host;

            context *m_prev{nullptr}, *m_next{nullptr};
            std::vector<void *> m_storage;
        };

        uint32_t new_key(cleanup cleanup) {
            std::lock_guard lock(m_mutex);
            // See if we can recycle some key
            uint32_t key;
            if (!m_freed_keys.empty()) {
                key = m_freed_keys.top();
                m_freed_keys.pop();
                m_cleanups[key] = cleanup;
            } else {
                key = static_cast<uint32_t>(m_cleanups.size());
                m_cleanups.push_back(cleanup);
            }
            return key;
        }

        void delete_key(uint32_t key) {
            std::unique_lock lock(m_mutex);
            temp::vector<void *> storage;
            const auto cleanup = m_cleanups[key];
            m_cleanups[key] = {nullptr, nullptr};
            for (auto it = m_head; it != m_tail; it = it->m_next) {
                if (it->m_storage.size() > key && it->m_storage[key] != nullptr) {
                    if (cleanup) storage.push_back(it->m_storage[key]);
                    it->m_storage[key] = nullptr;
                }
            }
            m_freed_keys.push(key);
            lock.unlock();

            // Run cleanup routines while the lock is released
            for (auto &it: storage) cleanup(it);
        }

    private:
        Lock<SpinLock> m_mutex;
        context *m_head{nullptr}, *m_tail{nullptr};
        std::vector<cleanup> m_cleanups;
        std::stack<uint32_t> m_freed_keys;

        host() = default;

        ~host() = default;

        void register_context(context *p) noexcept {
            std::lock_guard lock(m_mutex);
            if (!m_head) m_head = p; else (p->m_prev = m_tail, m_tail->m_next = p);
            m_tail = p;
        }

        void unregister_context(context *p) noexcept {
            std::lock_guard lock(m_mutex);
            if (p->m_next) p->m_next->m_prev = p->m_prev; else m_tail = p->m_prev;
            if (p->m_prev) p->m_next->m_prev = p->m_prev; else m_head = p->m_next;
        }
    };

    static host::context &context() noexcept {
        static thread_local host::context context{};
        return context;
    }

    uint32_t create(cleanup callback) { return host::get().new_key(callback); }

    void remove(uint32_t key) noexcept {
        if (key == invalid_key) return;
        host::get().delete_key(key);
    }

    void *get(uint32_t key) noexcept { return context().get_value(key); }

    void set(uint32_t key, void *p) { context().set_value(key, p); }
}
