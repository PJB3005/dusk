#pragma once

#include <cassert>
#include <span>

#include <mods/exceptions.hpp>
#include <mods/svc/resource.h>

namespace mods::resource {

struct LoadedResource {
    ModResult result;
    ResourceBuffer buffer{.struct_size = sizeof(buffer)};

    explicit LoadedResource(ModResult result) noexcept : result(result) {
        assert(sizeof(buffer) == buffer.struct_size);
    }
    explicit LoadedResource(ResourceBuffer buffer) noexcept : result(MOD_OK), buffer(buffer) {
        assert(sizeof(buffer) == buffer.struct_size);
    }
    LoadedResource(LoadedResource const&) = delete;
    LoadedResource(LoadedResource&&) = default;

    [[nodiscard]] constexpr bool is_ok() const noexcept { return result == MOD_OK; }

    constexpr void ensure_ok() const { throw_for_result(result); }

    constexpr std::span<uint8_t> span() noexcept {
        auto base = static_cast<uint8_t*>(buffer.data);
        return { base, base + buffer.size };
    }

    constexpr std::span<uint8_t const> span() const noexcept {
        auto base = static_cast<uint8_t const*>(buffer.data);
        return { base, base + buffer.size };
    }

    ~LoadedResource() {
        if (svc_resource && result == MOD_OK) {
            assert(sizeof(buffer) == buffer.struct_size);

            svc_resource->free(mod_ctx, &buffer);
        }
    }
};

[[nodiscard]] inline LoadedResource load(char const* relative_path) {
    if (!svc_resource) {
        return LoadedResource(MOD_UNAVAILABLE);
    }

    ResourceBuffer buffer{.struct_size = sizeof(buffer)};
    auto const result = svc_resource->load(mod_ctx, relative_path, &buffer);
    if (result != MOD_OK) {
        return LoadedResource(result);
    }

    return LoadedResource(buffer);
}

[[nodiscard]] inline LoadedResource load(std::string const& relative_path) {
    return load(relative_path.c_str());
}

}  // namespace mods::resource