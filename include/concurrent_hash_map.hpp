#pragma once
#include <unordered_map>
#include <shared_mutex>
#include <array>
#include <optional>
#include <type_traits>
#include <cstddef>
#include <mutex>

template<class Key, class T,
         class Hash      = std::hash<Key>,
         class KeyEq     = std::equal_to<Key>,
         std::size_t NumBuckets = 64 /* power‑of‑two recommended */>
class ConcurrentHashMap
{
    static_assert(NumBuckets > 1, "must have at least 2 buckets");

public:
    /* aliases */
    using key_type    = Key;
    using mapped_type = T;
    using size_type   = std::size_t;

    template<class... Args>
    void try_emplace(const Key& k, Args&&... args) {
        Bucket& b = bucket_for(k);
        std::unique_lock lg(b.m);
        b.data.try_emplace(k, std::forward<Args>(args)...);
    }

    template<class V>
    void insert_or_assign(const Key& k, V&& val) {
        Bucket& b = bucket_for(k);
        std::unique_lock lg(b.m);

        auto it = b.data.find(k);
        if (it == b.data.end()) {
            b.data.emplace(k, std::forward<V>(val));
        } else {
            b.data.erase(it);
            b.data.emplace(k, std::forward<V>(val));
        }
    }

    bool erase(const Key& k) {
        Bucket& b = bucket_for(k);
        std::unique_lock lg(b.m);
        return b.data.erase(k) != 0;
    }

    void clear() {
        for (Bucket& b : buckets_) {
            std::unique_lock lg(b.m);
            b.data.clear();
        }
    }

    T* find(const Key& k) {
        Bucket& b = bucket_for(k);
        std::shared_lock sl(b.m);
        auto it = b.data.find(k);
        return it == b.data.end() ? nullptr : &it->second;
    }

    const T* find(const Key& k) const {
        return const_cast<ConcurrentHashMap*>(this)->find(k);
    }

    size_type size() const {
        size_type total = 0;
        for (Bucket& b : buckets_) {
            std::shared_lock sl(b.m);
            total += b.data.size();
        }
        return total;
    }

private:
    struct Bucket {
        mutable std::shared_mutex m;
        std::unordered_map<Key, T, Hash, KeyEq> data;
    };

    std::array<Bucket, NumBuckets> buckets_;
    Hash                           hasher_;

    Bucket& bucket_for(const Key& k) const noexcept
    {
        std::size_t h = hasher_(k);
        return const_cast<Bucket&>(buckets_[h & (NumBuckets - 1)]);
    }
};
