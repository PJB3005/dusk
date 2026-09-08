#pragma once

#include <span>
#include <string>

#include "webgpu/webgpu_cpp.h"

namespace slugcat::gltf::shader {

constexpr std::string_view package = "shaders::";

wgpu::ShaderModule compileShader(std::string const& path);
wgpu::ShaderModule compileShader(std::string const& path, std::vector<std::pair<std::string, bool>> const& features);

}  // namespace slugcat::gltf::shader
