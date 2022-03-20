#pragma once

#include <map>
#include <vector>
#include <mutex>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        std::lock_guard<std::mutex> m_;
        Value& ref_to_value;

    };

    explicit ConcurrentMap(size_t bucket_count)
        :size_(bucket_count),
        mapa_(std::vector<std::map<Key, Value>>(bucket_count)),
        mutes_(std::vector<std::mutex>(bucket_count))
    {}

    Access operator[](const Key& key) {
        size_t index = key % size_;
        return Access{ std::lock_guard(mutes_[index]), mapa_[index][key] };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;

        for (size_t index = 0; index < size_; ++index) {
            std::lock_guard guard(mutes_[index]);
            result.merge(mapa_[index]);
        }
        return result;
    }

    bool Erase(const Key& key) {
        uint64_t tmp_key = static_cast<uint64_t>(key) % size_;
        std::lock_guard guard(mutes_[tmp_key]);
        return mapa_[tmp_key].erase(key);
    }

private:
    std::size_t size_;
    std::vector<std::map<Key, Value>> mapa_;
    std::vector<std::mutex> mutes_;

};