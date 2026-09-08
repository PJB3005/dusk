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

}  // namespace slugcat::gltf::collections