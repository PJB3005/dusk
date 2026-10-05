#pragma once

#include <vector>

#include "mods/svc/gfx.h"

namespace slugcat::gltf::buffer {

GfxRange appendAligned(std::vector<uint8_t>& buffer, void const* data, size_t dataSize, size_t alignment);

template<typename T>
requires std::is_trivially_copyable_v<T>
GfxRange appendAligned(std::vector<uint8_t>& buffer, T const& item, size_t const alignment) {
    return appendAligned(buffer, &item, sizeof(item), alignment);
}

}