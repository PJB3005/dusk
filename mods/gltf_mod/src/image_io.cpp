#include "image_io.hpp"
#include "helpers/tg3.hpp"

#include <stdexcept>

#include "fmt/format.h"
#include "png.h"

namespace slugcat::gltf::image_io {

namespace {

struct PngStructs {
    png_structp pStruct = nullptr;
    png_infop pInfo = nullptr;

    ~PngStructs() { png_destroy_read_struct(&pStruct, &pInfo, nullptr); }
};

struct MemoryCursor {
    std::span<uint8_t const> bytes;
    size_t pos = 0;
};

void readPngData(png_structp png, png_bytep data, const size_t length) {
    auto* cursor = static_cast<MemoryCursor*>(png_get_io_ptr(png));
    if (length > cursor->bytes.size() - cursor->pos) {
        png_error(png, "unexpected end of data");
    }
    std::memcpy(data, cursor->bytes.data() + cursor->pos, length);
    cursor->pos += length;
}

LoadedImageBuffer loadPng(std::span<uint8_t const> data) {
    PngStructs structs;
    MemoryCursor cursor{data};

    structs.pStruct = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!structs.pStruct) {
        throw std::runtime_error("png_create_read_struct failed");
    }

    structs.pInfo = png_create_info_struct(structs.pStruct);
    if (!structs.pInfo) {
        throw std::runtime_error("png_create_info_struct failed");
    }

    // I'm scared of putting any locals after that setjmp.
    std::vector<png_bytep> rowPointers;
    std::vector<uint8_t> imageData;
    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type, compression_type, filter_type;
    size_t rowBytes;
    int i;

    if (setjmp(png_jmpbuf(structs.pStruct))) {
        throw std::runtime_error("libpng encountered an error");
    }

    png_set_read_fn(structs.pStruct, &cursor, readPngData);
    png_read_info(structs.pStruct, structs.pInfo);

    if (!png_get_IHDR(structs.pStruct, structs.pInfo, &width, &height, &bit_depth, &color_type,
            &interlace_type, &compression_type, &filter_type))
    {
        throw std::runtime_error("libpng unable to read IHDR");
    }

    // Always read as RGBA8.
    png_set_gray_to_rgb(structs.pStruct);
    png_set_filler(structs.pStruct, 0xFF, PNG_FILLER_AFTER);
    png_set_expand(structs.pStruct);
    png_set_strip_16(structs.pStruct);

    png_read_update_info(structs.pStruct, structs.pInfo);
    rowBytes = png_get_rowbytes(structs.pStruct, structs.pInfo);
    rowPointers.resize(height);

    imageData.resize(rowBytes * height, 0);

    for (i = 0; i < height; i++) {
        rowPointers[i] = imageData.data() + i * rowBytes;
    }

    png_read_image(structs.pStruct, rowPointers.data());
    png_read_end(structs.pStruct, nullptr);
    return LoadedImageBuffer(
        wgpu::TextureFormat::RGBA8Unorm, {width, height}, 1, {std::move(imageData)});
}

}  // namespace

LoadedImageBuffer::LoadedImageBuffer(wgpu::TextureFormat format, wgpu::Extent2D extent,
    uint32_t mipLevels, BufferType buffer) noexcept
    : format(format), extent(extent), mipLevels(mipLevels), buffer(std::move(buffer)) {}

LoadedImageBuffer load(std::span<uint8_t const> data, std::string_view mimeType) {
    if (mimeType == "image/png") {
        return loadPng(data);
    }

    throw std::runtime_error(fmt::format("Unsupported image format: {}", mimeType));
}

LoadedImageBuffer load(tg3_model const& model, int32_t imageId) {
    if (imageId < 0) {
        throw std::runtime_error("Invalid image ID");
    }

    auto& image = model.images[imageId];
    if (image.uri.len > 0) {
        throw std::runtime_error("Cannot load images from URI");
    }

    if (image.buffer_view < 0) {
        throw std::runtime_error("Invalid image buffer");
    }

    auto& bufferView = model.buffer_views[image.buffer_view];
    auto& buffer = model.buffers[bufferView.buffer];

    return load({buffer.data.data + bufferView.byte_offset, bufferView.byte_length}, tg3::tg3_string_view(image.mime_type));
}

}  // namespace slugcat::gltf::image_io