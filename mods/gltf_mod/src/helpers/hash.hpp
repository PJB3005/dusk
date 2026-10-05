#pragma once

#include <functional>

// C++ is a language

namespace slugcat::gltf::helpers {

// https://stackoverflow.com/a/2595226
template<typename ... Args>
void hashCombine(size_t& seed, const Args& ... v) {
    ([&] {
        std::hash<Args> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
    } (), ...);

}

template<typename ... Args>
[[nodiscard]] size_t hashCombine(const Args& ... v) {
    size_t seed = 0;
    hashCombine(seed, v...);
    return seed;
}

}