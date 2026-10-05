#include "render.hpp"

#include <stdexcept>

#include "SSystem/SComponent/c_xyz.h"
#include "d/d_com_inf_game.h"
#include "helpers/result.hpp"
#include "mods/svc/camera.h"
#include "mods/svc/log.hpp"
#include "scene.hpp"
#include "shader.hpp"
#include "tiny_gltf_v3.h"

namespace slugcat::gltf::render {

using namespace slugcat::gltf::scene;
using namespace std::string_view_literals;
using helpers::checkResult;

namespace {

struct Payload {
    std::shared_ptr<Scene> scene;
    wgpu::RenderPipeline pipeline;

    GfxRange uniformGlobalRange;
    std::vector<GfxRange> uniformObjectRanges;
    std::vector<GfxRange> skinDataRanges;
    std::vector<GfxRange> materialRanges;
};


void bindVertexBuffer(
    wgpu::RenderPassEncoder const& encoder, BufferAccessor const& buffer, uint32_t slot) {
    encoder.SetVertexBuffer(slot, buffer.buffer, buffer.offset, buffer.size);
}

void WawaDrawEntity(wgpu::RenderPassEncoder const& encoder, Payload const& payload,
    Entity const& entity, wgpu::Buffer const& uniform, wgpu::Buffer const& storage, int meshIdx) {
    auto const& mesh = *entity.mesh;

    for (auto const& primitive : mesh.primitives) {
        encoder.SetVertexBuffer(
            0, primitive.vertex.buffer, primitive.vertex.offset, primitive.vertex.size);
        encoder.SetVertexBuffer(
            1, primitive.texCoord.buffer, primitive.texCoord.offset, primitive.texCoord.size);

        if (entity.skinData) {
            auto const& sharedSkinData = *entity.skinData;
            if (!primitive.skinData.has_value()) {
                throw std::runtime_error("Missing skin data on primitive!");
            }

            auto const& primSkinData = *primitive.skinData;

            bindVertexBuffer(encoder, primSkinData.joints, 2);
            bindVertexBuffer(encoder, primSkinData.weights, 3);
        }

        encoder.SetIndexBuffer(primitive.index.buffer,
            primitive.index.componentType == TG3_COMPONENT_TYPE_UNSIGNED_SHORT ?
                wgpu::IndexFormat::Uint16 :
                wgpu::IndexFormat::Uint32,
            primitive.index.offset, primitive.index.size);

        uint32_t dynamicOffset = payload.uniformObjectRanges[meshIdx].offset;
        auto const storageRange = payload.skinDataRanges[meshIdx];

        wgpu::BindGroupEntry const entries[]{
            {
                .binding = 0,
                .buffer = uniform,
                .offset = dynamicOffset,
                .size = sizeof(UniformObject),
            },
            {
                .binding = 1,
                .buffer = storage,
                .offset = storageRange.offset,
                .size = storageRange.size,
            },
        };
        wgpu::BindGroupDescriptor const bgDesc{
            .layout = sBindGroupLayoutObject,
            .entryCount = std::size(entries),
            .entries = entries,
        };

        auto bg = sDevice.CreateBindGroup(&bgDesc);

        auto const& matRange = payload.materialRanges[primitive.material->materialId];
        wgpu::BindGroupEntry const matEntries[]{
            {
                .binding = 0,
                .buffer = uniform,
                .offset = matRange.offset,
                .size = matRange.size,
            },
            {
                .binding = 1,
                .textureView = primitive.material->texture->textureView,
            },
            {
                .binding = 2,
                .sampler = primitive.material->texture->sampler,
            },
        };

        wgpu::BindGroupDescriptor const matDesc{
            .layout = sBindGroupLayoutMaterial,
            .entryCount = std::size(matEntries),
            .entries = matEntries,
        };

        auto bgMat = sDevice.CreateBindGroup(&matDesc);

        encoder.SetBindGroup(1, bgMat, 0, nullptr);
        encoder.SetBindGroup(2, bg, 0, nullptr);
        encoder.DrawIndexed(primitive.index.count, 1, 0, 0);
    }
}

void WawaDraw(ModContext*, const GfxDrawContext* draw_ctx, const void* payload_raw,
    size_t payload_size, void*) {
    assert(payload_size == sizeof(Payload const*));

    auto* payload = *static_cast<Payload* const*>(payload_raw);

    wgpu::RenderPassEncoder encoder = draw_ctx->pass;

    wgpu::BindGroupEntry const entry{
        .binding = 0,
        .buffer = draw_ctx->uniform_buffer,
        .offset = payload->uniformGlobalRange.offset,
        .size = payload->uniformGlobalRange.size,
    };
    wgpu::BindGroupDescriptor const bindGroupDescriptor{
        .label = "global"sv,
        .layout = sBindGroupLayoutGlobal,
        .entryCount = 1,
        .entries = &entry,
    };
    auto bindGroup = sDevice.CreateBindGroup(&bindGroupDescriptor);

    /*
    wgpu::BindGroupEntry const entryObject[]{{
                                                 .binding = 0,
                                                 .buffer = draw_ctx->uniform_buffer,
                                                 .offset = 0,
                                                 .size = sizeof(UniformObject),
                                             },
        {
            .binding = 0,
            .buffer = draw_ctx->uniform_buffer,
            .offset = 0,
            .size = sizeof(UniformObject),
        }};
    wgpu::BindGroupDescriptor const bindGroupDescObject{
        .label = "object"sv,
        .layout = sBindGroupLayoutObject,
        .entryCount = std::size(entryObject),
        .entries = entryObject,
    };
    auto const bindGroupObject = sDevice.CreateBindGroup(&bindGroupDescObject);
    */

    encoder.PushDebugGroup("REAL"sv);

    encoder.SetPipeline(payload->pipeline);
    encoder.SetBindGroup(0, bindGroup, 0, nullptr);

    int i = 0;
    for (auto id : payload->scene->meshes) {
        WawaDrawEntity(encoder, *payload, payload->scene->get_entity(id), draw_ctx->uniform_buffer,
            draw_ctx->storage_buffer, i);
        i += 1;
    }

    encoder.PopDebugGroup();

    delete payload;
}

GfxDrawTypeHandle gDrawModelCommandType;

void createBindGroupLayouts() {
    constexpr static wgpu::BindGroupLayoutEntry globalEntries[] = {
        {
            .binding = 0,
            .visibility = wgpu::ShaderStage::Vertex,
            .buffer =
                {
                    .type = wgpu::BufferBindingType::Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = sizeof(UniformGlobal),
                },
        },
    };

    constexpr static wgpu::BindGroupLayoutDescriptor descGlobal{
        .label = "global"sv,
        .entryCount = std::size(globalEntries),
        .entries = globalEntries,
    };

    sBindGroupLayoutGlobal = sDevice.CreateBindGroupLayout(&descGlobal);

    constexpr static wgpu::BindGroupLayoutEntry materialEntries[] = {
        {
            .binding = 0,
            .visibility = wgpu::ShaderStage::Fragment,
            .buffer =
                {
                    .type = wgpu::BufferBindingType::Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = sizeof(UniformMaterial),
                },
        },
        {
            .binding = 1,
            .visibility = wgpu::ShaderStage::Fragment,
            .texture =
                {
                    .sampleType = wgpu::TextureSampleType::Float,
                    .viewDimension = wgpu::TextureViewDimension::e2D,
                },
        },
        {
            .binding = 2,
            .visibility = wgpu::ShaderStage::Fragment,
            .sampler =
                {
                    .type = wgpu::SamplerBindingType::Filtering,
                },
        },
    };

    constexpr static wgpu::BindGroupLayoutDescriptor descMaterial{
        .label = "material"sv,
        .entryCount = std::size(materialEntries),
        .entries = materialEntries,
    };

    sBindGroupLayoutMaterial = sDevice.CreateBindGroupLayout(&descMaterial);

    constexpr static wgpu::BindGroupLayoutEntry objectEntries[] = {
        {
            .binding = 0,
            .visibility = wgpu::ShaderStage::Vertex,
            .buffer =
                {
                    .type = wgpu::BufferBindingType::Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = sizeof(UniformObject),
                },
        },
        {
            .binding = 1,
            .visibility = wgpu::ShaderStage::Vertex,
            .buffer =
                {
                    .type = wgpu::BufferBindingType::ReadOnlyStorage,
                    .hasDynamicOffset = false,
                    .minBindingSize = sizeof(glm::mat4),
                },
        },
    };

    constexpr static wgpu::BindGroupLayoutDescriptor descObject{
        .label = "object"sv,
        .entryCount = std::size(objectEntries),
        .entries = objectEntries,
    };

    sBindGroupLayoutObject = sDevice.CreateBindGroupLayout(&descObject);

    wgpu::BindGroupLayout const layouts[] = {
        sBindGroupLayoutGlobal,    // 0
        sBindGroupLayoutMaterial,  // 1
        sBindGroupLayoutObject,    // 2
    };

    wgpu::PipelineLayoutDescriptor const pipelineLayoutDesc{
        .bindGroupLayoutCount = std::size(layouts), .bindGroupLayouts = layouts};

    sPipelineLayout = sDevice.CreatePipelineLayout(&pipelineLayoutDesc);
}

}  // namespace

wgpu::Device sDevice;
wgpu::Queue sQueue;
GfxDeviceInfo sDeviceInfo{.struct_size = sizeof(sDeviceInfo)};
wgpu::Limits sLimits;

wgpu::BindGroupLayout sBindGroupLayoutGlobal;
wgpu::BindGroupLayout sBindGroupLayoutMaterial;
wgpu::BindGroupLayout sBindGroupLayoutObject;
wgpu::PipelineLayout sPipelineLayout;

void init() {
    if (svc_gfx->get_device_info(mod_ctx, &sDeviceInfo) != MOD_OK) {
        throw std::runtime_error("Failed to get GFX device info!");
    }

    sDevice = wgpu::Device(sDeviceInfo.device);
    sQueue = wgpu::Queue(sDeviceInfo.queue);

    sDevice.GetLimits(&sLimits);

    createBindGroupLayouts();

    constexpr static GfxDrawTypeDesc drawDesc = {
        .struct_size = sizeof(GfxDrawTypeDesc),
        .label = "DrawScene",
        .draw = &WawaDraw,
        .user_data = nullptr,
    };
    auto result = svc_gfx->register_draw_type(mod_ctx, &drawDesc, &gDrawModelCommandType);
    if (result != MOD_OK) {
        throw std::runtime_error("Failed to register draw!");
    }
}


FoobarPacket::FoobarPacket() {
    auto const shaderModule =
        shader::compileShader("shaders::model", {{"SKINNED", true}});

    static constexpr wgpu::VertexAttribute attr{
        .format = wgpu::VertexFormat::Float32x3,
        .shaderLocation = 0,
    };
    static constexpr wgpu::VertexAttribute attrTexCoord[]{{
        .format = wgpu::VertexFormat::Float32x2,
        .shaderLocation = 1,
    }};
    static constexpr wgpu::VertexAttribute attrJoints[]{{
        .format = wgpu::VertexFormat::Uint16x4,
        .shaderLocation = 6,
    }};
    static constexpr wgpu::VertexAttribute attrWeights[]{{
        .format = wgpu::VertexFormat::Float32x4,
        .shaderLocation = 7,
    }};

    static constexpr wgpu::VertexBufferLayout buffers[]{
        {
            .stepMode = wgpu::VertexStepMode::Vertex,
            .arrayStride = sizeof(cXyz),
            .attributeCount = 1,
            .attributes = &attr,
        },
        {
            .stepMode = wgpu::VertexStepMode::Vertex,
            .arrayStride = sizeof(cXy),
            .attributeCount = std::size(attrTexCoord),
            .attributes = attrTexCoord,
        },
        {
            .stepMode = wgpu::VertexStepMode::Vertex,
            .arrayStride = 8,  // vec4<unsigned short>
            .attributeCount = std::size(attrJoints),
            .attributes = attrJoints,
        },
        {
            .stepMode = wgpu::VertexStepMode::Vertex,
            .arrayStride = 16,  // vec4<float>
            .attributeCount = std::size(attrWeights),
            .attributes = attrWeights,
        },
    };

    static constexpr wgpu::BlendState blend{
        .color = {.operation = wgpu::BlendOperation::Add,
            .srcFactor = wgpu::BlendFactor::SrcAlpha,
            .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha},
        .alpha = {.operation = wgpu::BlendOperation::Add,
            .srcFactor = wgpu::BlendFactor::One,
            .dstFactor = wgpu::BlendFactor::One},
    };

    wgpu::ColorTargetState const targetState = {
        .format = (wgpu::TextureFormat)sDeviceInfo.color_format,
        .blend = &blend,
        .writeMask = wgpu::ColorWriteMask::All,
    };

    wgpu::FragmentState const fragmentState{
        .module = shaderModule,
        .entryPoint = "fs_main"sv,
        .targetCount = 1,
        .targets = &targetState,
    };

    static constexpr wgpu::DepthStencilState depthStencil{
        .format = wgpu::TextureFormat::Depth32Float,
        .depthWriteEnabled = WGPUOptionalBool_True,
        .depthCompare = wgpu::CompareFunction::Greater,
    };

    auto pipelineDesc = wgpu::RenderPipelineDescriptor{
        .label = "Wawa"sv,
        .layout = sPipelineLayout,
        .vertex =
            {
                .module = shaderModule,
                .entryPoint = "vs_main"sv,
                .bufferCount = std::size(buffers),
                .buffers = buffers,
            },
        .primitive =
            {
                .topology = wgpu::PrimitiveTopology::TriangleList,
                .frontFace = wgpu::FrontFace::CCW,
                .cullMode = wgpu::CullMode::Back,
            },
        .depthStencil = &depthStencil,
        .multisample =
            {
                .count = 1,
                .mask = 0xFFFF'FFFF,
            },
        .fragment = &fragmentState,
    };

    pipeline = sDevice.CreateRenderPipeline(&pipelineDesc);
}

std::vector<glm::mat4> gJointCalcBuffer;

void FoobarPacket::draw() {
    auto* payload = new Payload{scene, pipeline};

    auto const& view = *g_dComIfG_gameInfo.play.mCurrentView;

    CameraInfo info{
        .struct_size = sizeof(CameraInfo),
    };
    checkResult(svc_camera->get_camera(mod_ctx, &view, &info));

    UniformGlobal globalUniforms = {};
    std::memcpy(&globalUniforms.projViewMtx, info.proj_from_world, sizeof(info.proj_from_world));

    checkResult(svc_gfx->push_uniform(
        mod_ctx, &globalUniforms, sizeof(globalUniforms), &payload->uniformGlobalRange));

    for (auto const meshEnt : scene->meshes) {
        UniformObject const object{readEntityMatrix(meshEnt)};

        auto& objectRange = payload->uniformObjectRanges.emplace_back();

        checkResult(svc_gfx->push_uniform(mod_ctx, &object, sizeof(object), &objectRange));
    }

    for (auto const skinEntId : scene->skinned) {
        auto const& skinEnt = scene->get_entity(skinEntId);
        auto const& skinData = *skinEnt.skinData;

        gJointCalcBuffer.resize(skinData.joints.size());

        for (size_t i = 0; i < skinData.joints.size(); ++i) {
            auto jointEntId = skinData.joints[i];

            gJointCalcBuffer[i] = readEntityMatrix(jointEntId) * skinData.inverseBindMatrices[i];
        }

        auto& jointRange = payload->skinDataRanges.emplace_back();

        checkResult(svc_gfx->push_storage(mod_ctx, gJointCalcBuffer.data(),
            gJointCalcBuffer.size() * sizeof(glm::mat4), &jointRange));
    }

    for (auto const mat : scene->materials) {
        UniformMaterial const matUniform{
            mat->color,
        };

        auto& range = payload->materialRanges.emplace_back();
        checkResult(svc_gfx->push_uniform(mod_ctx, &matUniform, sizeof(matUniform), &range));
    }

    auto result = svc_gfx->push_draw(mod_ctx, gDrawModelCommandType, &payload, sizeof(payload));
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "Failed to push draw command!");
    }
}

glm::mat4 FoobarPacket::readEntityMatrix(EntityId id) const {
    return matrix::readInterpMatrix(entityMatrices.at(id));
}

}  // namespace slugcat::gltf::render