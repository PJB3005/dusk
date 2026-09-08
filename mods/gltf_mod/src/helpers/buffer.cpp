#include "buffer.hpp"

#include <stdexcept>

namespace slugcat::gltf::buffer {
GfxRange appendAligned(
    std::vector<uint8_t>& buffer, void const* data, size_t dataSize, size_t alignment) {
    auto const misalignment = buffer.size() % alignment;
    if (misalignment != 0) {
        buffer.resize(buffer.size() + (alignment - misalignment), 0);
    }

    auto const start = buffer.size();
    buffer.resize(start + dataSize);
    std::memcpy(&buffer[start], data, dataSize);

    if (start > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("That's a BIG buffer");
    }

    return {static_cast<uint32_t>(start), static_cast<uint32_t>(dataSize)};
}
}  // namespace slugcat::gltf::buffer