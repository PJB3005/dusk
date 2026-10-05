#pragma once

#include <unordered_map>

namespace slugcat::gltf::collections {

template <typename Constructor, typename Result>
concept ReturnsConstructible = std::is_constructible_v<Result, decltype(std::declval<Constructor>()())>;

template <typename TResult, typename TKey, ReturnsConstructible<TResult> Constructor>
constexpr TResult& get_or_new(
    std::unordered_map<TKey, TResult>& map, const TKey& key, Constructor ctor) {
    auto const found = map.find(key);
    if (found != map.end()) {
        return found->second;
    }

    return map.emplace(key, ctor()).first->second;
}

// https://stackoverflow.com/questions/7631996/remove-an-element-from-a-vector-by-value-c
template<typename T>
void remove(std::vector<T> & v, const T & item)
{
    v.erase(std::remove(v.begin(), v.end(), item), v.end());
}

}  // namespace slugcat::gltf::collections
