#include "shader.hpp"

#include "helpers/string_slug.hpp"
#include "render.hpp"

#include "mods/svc/log.h"
#include "mods/svc/log.hpp"
#include "mods/svc/resource.hpp"
#include "wesl.h"

namespace slugcat::gltf::shader {

using namespace std::string_view_literals;

namespace {

struct WeslResultWrap {
    WeslResult result;

    explicit WeslResultWrap(WeslResult result) noexcept : result(result) {}

    ~WeslResultWrap() { wesl_free_result(&result); }

    WeslResult* operator->() noexcept { return &result; }
};

WeslResolveSourceResult gFailedResult = {
    .success = false,
};

WeslResolveSourceResult* resolveSource(char const* modPath, void*) noexcept {
    std::string path(modPath);
    helpers::replaceAll(path, "::"sv, "/"sv);
    path.append(".wesl"sv);

    auto const resource = mods::resource::load(path);
    if (!resource.is_ok()) {
        mods::log::error("shader resolveSource: failed to resolve '{}': file '{}' failed to load",
            modPath, path);
        return &gFailedResult;
    }

    auto const buffer = new char[resource.buffer.size + 1];
    std::memcpy(buffer, resource.buffer.data, resource.buffer.size);
    buffer[resource.buffer.size] = '\0';

    return new WeslResolveSourceResult{
        .success = true,
        .source = buffer,
    };
}

void resolveSourceFree(WeslResolveSourceResult const* result, void*) noexcept {
    if (result == &gFailedResult) {
        return;
    }

    delete[] result->source;
    delete result;
}

std::string compileShaderSource(
    std::string const& path, std::vector<std::pair<std::string, bool>> const& features) {
    if (!path.starts_with(package)) {
        throw std::runtime_error(fmt::format("Invalid shader module: '{}'", path));
    }

    std::vector<char const*> featureKeys;
    std::vector<unsigned char> featureValues;

    for (auto const& pair : features) {
        featureKeys.push_back(pair.first.c_str());
        featureValues.push_back(pair.second ? 1 : 0);
    }

    WeslCompileOptions const options{
        .imports = true,
        .condcomp = true,
        .strip = true,
        .lower = true,
        .sourcemap = false,
        .features =
            {
                .keys = featureKeys.data(),
                .values = reinterpret_cast<bool const*>(
                    featureValues.data()),  // epic std::vector<bool> moment.
                .len = features.size(),
            },
    };

    WeslResolverOptions const resolver{
        .resolve_source = resolveSource,
        .resolve_source_free = resolveSourceFree,
    };

    WeslResultWrap result(wesl_compile(path.c_str(), &options, &resolver));
    if (!result->success) {
        mods::log::error("Failed to compile WESL '{}': {}", path, result->error.message);
        throw std::runtime_error("Failed to compile WESL");
    }

    return result->data;
}

}  // namespace

wgpu::ShaderModule compileShader(std::string const& path) {
    return compileShader(path, {});
}

wgpu::ShaderModule compileShader(
    std::string const& path, std::vector<std::pair<std::string, bool>> const& features) {
    auto const source = compileShaderSource(path, features);

    wgpu::ShaderSourceWGSL shaderSourceWgsl{};
    shaderSourceWgsl.code = std::string_view(source);

    wgpu::ShaderModuleDescriptor const shaderModuleDesc{
        .nextInChain = &shaderSourceWgsl,
        .label = std::string_view(path),
    };

    return render::sDevice.CreateShaderModule(&shaderModuleDesc);
}

}  // namespace slugcat::gltf::shader