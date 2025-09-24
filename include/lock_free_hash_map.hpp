#pragma once

#include <atomic>
#include <climits>
#include <cstddef>
#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace lf {

namespace detail {
inline std::size_t next_power_of_two(std::size_t n) noexcept {
    if (n < 2) return 2;
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
#if ULONG_MAX > 0xffffffffUL
    n |= n >> 32;
#endif
    return ++n;
}
} // namespace detail

template<class Key,
         class T,
         class Hash    = std::hash<Key>,
         class KeyEq   = std::equal_to<Key>>
class LockFreeHashMap {
    struct Node {
        template<class K, class... Args>
        Node(K&& key_in, Args&&... args)
            : key(std::forward<K>(key_in)),
              value(std::forward<Args>(args)...),
              next(nullptr) {}

        Key               key;
        T                 value;
        std::atomic<Node*> next;
    };

    struct Bucket {
        Bucket() noexcept : head(nullptr) {}
        Bucket(const Bucket&) = delete;
        Bucket& operator=(const Bucket&) = delete;
        std::atomic<Node*> head;
    };

public:
    using key_type    = Key;
    using mapped_type = T;
    using size_type   = std::size_t;

    explicit LockFreeHashMap(std::size_t bucket_hint = 64)
        : bucket_count_(detail::next_power_of_two(bucket_hint)),
          bucket_mask_(bucket_count_ - 1),
          buckets_(std::make_unique<Bucket[]>(bucket_count_)),
          size_(0) {}

    ~LockFreeHashMap() { clear(); }

    LockFreeHashMap(const LockFreeHashMap&) = delete;
    LockFreeHashMap& operator=(const LockFreeHashMap&) = delete;

    template<class... Args>
    bool try_emplace(const Key& key, Args&&... args) {
        auto& bucket = bucket_for(key);
        Key key_copy(key);

        while (true) {
            Node* head = bucket.head.load(std::memory_order_acquire);
            if (contains_in_bucket(head, key_copy)) {
                return false;
            }

            Node* new_node = new Node(key_copy, std::forward<Args>(args)...);
            new_node->next.store(head, std::memory_order_relaxed);

            if (bucket.head.compare_exchange_weak(
                    head, new_node,
                    std::memory_order_release,
                    std::memory_order_acquire)) {
                size_.fetch_add(1, std::memory_order_relaxed);
                return true;
            }

            delete new_node;
        }
    }

    template<class V>
    void insert_or_assign(const Key& key, V&& value) {
        auto& bucket = bucket_for(key);
        Key key_copy(key);
        using Stored = std::decay_t<V>;
        Stored value_copy(std::forward<V>(value));

        while (true) {
            Node* head = bucket.head.load(std::memory_order_acquire);
            for (Node* current = head; current != nullptr;
                 current = current->next.load(std::memory_order_acquire)) {
                if (key_eq_(current->key, key_copy)) {
                    current->value = value_copy;
                    return;
                }
            }

            Node* new_node = new Node(key_copy, value_copy);
            new_node->next.store(head, std::memory_order_relaxed);

            if (bucket.head.compare_exchange_weak(
                    head, new_node,
                    std::memory_order_release,
                    std::memory_order_acquire)) {
                size_.fetch_add(1, std::memory_order_relaxed);
                return;
            }

            delete new_node;
        }
    }

    bool erase(const Key& key) {
        auto& bucket = bucket_for(key);
        Key key_copy(key);

        while (true) {
            Node* head = bucket.head.load(std::memory_order_acquire);
            Node* prev = nullptr;
            Node* current = head;

            while (current) {
                Node* next = current->next.load(std::memory_order_acquire);
                if (key_eq_(current->key, key_copy)) {
                    if (prev) {
                        if (!prev->next.compare_exchange_strong(
                                current, next,
                                std::memory_order_acq_rel,
                                std::memory_order_acquire)) {
                            break; // restart search
                        }
                    } else {
                        if (!bucket.head.compare_exchange_strong(
                                head, next,
                                std::memory_order_acq_rel,
                                std::memory_order_acquire)) {
                            break; // restart search
                        }
                    }
                    size_.fetch_sub(1, std::memory_order_relaxed);
                    delete current;
                    return true;
                }
                prev = current;
                current = next;
            }

            if (!current) {
                return false;
            }
        }
    }

    T* find(const Key& key) {
        return const_cast<T*>(std::as_const(*this).find(key));
    }

    const T* find(const Key& key) const {
        Key key_copy(key);
        const Bucket& bucket = bucket_for(key_copy);
        Node* current = bucket.head.load(std::memory_order_acquire);

        while (current) {
            if (key_eq_(current->key, key_copy)) {
                return &current->value;
            }
            current = current->next.load(std::memory_order_acquire);
        }
        return nullptr;
    }

    bool contains(const Key& key) const {
        return find(key) != nullptr;
    }

    size_type size() const noexcept {
        return size_.load(std::memory_order_acquire);
    }

    bool empty() const noexcept {
        return size() == 0;
    }

    void clear() noexcept {
        for (std::size_t i = 0; i < bucket_count_; ++i) {
            Node* node = buckets_[i].head.exchange(nullptr, std::memory_order_acq_rel);
            destroy_chain(node);
        }
        size_.store(0, std::memory_order_relaxed);
    }

    std::size_t bucket_count() const noexcept { return bucket_count_; }

private:
    Bucket& bucket_for(const Key& key) noexcept {
        std::size_t idx = hasher_(key) & bucket_mask_;
        return buckets_[idx];
    }

    const Bucket& bucket_for(const Key& key) const noexcept {
        std::size_t idx = hasher_(key) & bucket_mask_;
        return buckets_[idx];
    }

    bool contains_in_bucket(Node* head, const Key& key) const {
        for (Node* current = head; current != nullptr;
             current = current->next.load(std::memory_order_acquire)) {
            if (key_eq_(current->key, key)) return true;
        }
        return false;
    }

    static void destroy_chain(Node* first) noexcept {
        Node* current = first;
        while (current) {
            Node* next = current->next.load(std::memory_order_relaxed);
            delete current;
            current = next;
        }
    }

    const std::size_t         bucket_count_;
    const std::size_t         bucket_mask_;
    std::unique_ptr<Bucket[]> buckets_;
    Hash                      hasher_{};
    KeyEq                     key_eq_{};
    std::atomic<size_type>    size_;
};

} // namespace lf
