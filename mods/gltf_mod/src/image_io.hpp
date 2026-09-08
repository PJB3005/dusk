#pragma once

#include <vector>

#include "tiny_gltf_v3.h"
#include "webgpu/webgpu_cpp.h"

namespace slugcat::gltf::image_io {

struct LoadedImageBuffer {
    using BufferType = std::vector<std::vector<uint8_t>>;

    wgpu::TextureFormat format;
    wgpu::Extent2D extent;
    uint32_t mipLevels;

    BufferType buffer; // One for each mip level.

    LoadedImageBuffer(wgpu::TextureFormat format, wgpu::Extent2D extent, uint32_t mipLevels, BufferType buffer) noexcept;
    LoadedImageBuffer(LoadedImageBuffer const&) = delete;
    LoadedImageBuffer(LoadedImageBuffer&&) = default;
};

LoadedImageBuffer load(std::span<uint8_t const> data, std::string_view mimeType);
LoadedImageBuffer load(tg3_model const& model, int32_t imageId);

}