#pragma once

#include "JSystem/J3DGraphBase/J3DPacket.h"
#include "glm/glm.hpp"
#include "mods/svc/gfx.h"
#include "mods/svc/interp.hpp"
#include "scene.hpp"

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

class FoobarPacket final : public J3DPacket {
public:
    wgpu::RenderPipeline pipeline;
    std::shared_ptr<scene::Scene> scene;
    std::vector<mods::interp::InterpMatrix> entityMatrices;

    FoobarPacket();
    void draw() override;

private:
    [[nodiscard]] glm::mat4 readEntityMatrix(scene::EntityId id) const;
};

}
