#pragma once

#include "dolphin/mtx.h"
#include "glm/mat4x4.hpp"
#include "mods/svc/gfx.h"

#include "webgpu/webgpu_cpp.h"

namespace slugcat::gltf::render {

extern wgpu::Device sDevice;
extern wgpu::Queue sQueue;
extern GfxDeviceInfo sDeviceInfo;
extern wgpu::Limits sLimits;

extern wgpu::BindGroupLayout sBindGroupLayoutGlobal;
extern wgpu::BindGroupLayout sBindGroupLayoutMaterial;
extern wgpu::BindGroupLayout sBindGroupLayoutObject;
extern wgpu::PipelineLayout sPipelineLayout;

void init();

struct UniformGlobal {
    glm::mat4 projViewMtx;
};

struct UniformMaterial {
    glm::vec4 color;
};

struct UniformObject {
    glm::mat4 modelMtx;
};

}
