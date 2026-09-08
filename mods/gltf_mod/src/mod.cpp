#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"
#include "d/d_com_inf_game.h"

#include "loader.hpp"
#include "mod.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "shader.hpp"

#include "mods/service.hpp"
#include "mods/svc/actor.hpp"
#include "mods/svc/camera.h"
#include "mods/svc/gfx.h"
#include "mods/svc/log.hpp"
#include "mods/svc/resource.h"
#include "mods/svc/stage.h"

#include "webgpu/webgpu_cpp.h"

DEFINE_MOD();
IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(ActorService, svc_actor);
IMPORT_SERVICE(StageService, svc_stage);
IMPORT_SERVICE(GfxService, svc_gfx);
IMPORT_SERVICE(ResourceService, svc_resource);
IMPORT_SERVICE(CameraService, svc_camera);

using namespace mods::actor;
using namespace std::string_view_literals;
using namespace std::string_literals;
using namespace slugcat::gltf::scene;
using namespace slugcat::gltf::render;

namespace {

struct Payload {
    std::shared_ptr<Scene> scene;
    wgpu::RenderPipeline pipeline;

    GfxRange uniformGlobalRange;
    std::vector<GfxRange> uniformObjectRanges;
    std::vector<GfxRange> skinDataRanges;
};

GfxDrawTypeHandle gDrawModelCommandType;

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

        encoder.SetBindGroup(1, primitive.material->bindGroup, 0, nullptr);
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
        WawaDrawEntity(encoder, *payload, payload->scene->get_entity(id), draw_ctx->uniform_buffer, draw_ctx->storage_buffer, i);
        i += 1;
    }

    encoder.PopDebugGroup();

    delete payload;
}

void checkResult(ModResult res) {
    if (res != MOD_OK) {
        throw std::runtime_error("Service call failed");
    }
}

void applyTransformsRecursive(Scene& scene, EntityId entity_id, glm::mat4 const& transform) {
    auto& entity = scene.get_entity(entity_id);
    entity.globalXform = transform * entity.localXform;

    for (auto child : entity.children) {
        applyTransformsRecursive(scene, child, entity.globalXform);
    }
}

std::vector<glm::mat4> gJointCalcBuffer;

}  // namespace

FoobarPacket::FoobarPacket() {
    auto const shaderModule =
        slugcat::gltf::shader::compileShader("shaders::model", {{"SKINNED", true}});

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

void FoobarPacket::draw() {
    auto* payload = new Payload{renderData, pipeline};

    auto const& view = *g_dComIfG_gameInfo.play.mCurrentView;

    CameraInfo info{
        .struct_size = sizeof(CameraInfo),
    };
    checkResult(svc_camera->get_camera(mod_ctx, &view, &info));

    UniformGlobal globalUniforms = {};
    std::memcpy(&globalUniforms.projViewMtx, info.proj_from_world, sizeof(info.proj_from_world));

    checkResult(svc_gfx->push_uniform(
        mod_ctx, &globalUniforms, sizeof(globalUniforms), &payload->uniformGlobalRange));

    for (auto const meshEnt : renderData->meshes) {
        auto const& ent = renderData->get_entity(meshEnt);

        UniformObject const object{ent.globalXform};

        auto& objectRange = payload->uniformObjectRanges.emplace_back();

        checkResult(svc_gfx->push_uniform(mod_ctx, &object, sizeof(object), &objectRange));
    }

    for (auto const skinEntId : renderData->skinned) {
        auto const& skinEnt = renderData->get_entity(skinEntId);
        auto const& skinData = *skinEnt.skinData;

        gJointCalcBuffer.resize(skinData.joints.size());

        for (size_t i = 0; i < skinData.joints.size(); ++i) {
            auto jointEntId = skinData.joints[i];
            auto const& jointEnt = *renderData->entities[jointEntId];

            gJointCalcBuffer[i] = jointEnt.globalXform * skinData.inverseBindMatrices[i];
        }

        auto& jointRange = payload->skinDataRanges.emplace_back();

        checkResult(svc_gfx->push_storage(mod_ctx, gJointCalcBuffer.data(),
            gJointCalcBuffer.size() * sizeof(glm::mat4), &jointRange));
    }

    auto result = svc_gfx->push_draw(mod_ctx, gDrawModelCommandType, &payload, sizeof(payload));
    if (result != MOD_OK) {
        svc_log->error(mod_ctx, "Failed to push draw command!");
    }
}

cPhs_Step ActorGltf::Create() {
    AuroraGXSync();

    scale.setall(1'000);

    packet.renderData = std::make_shared<Scene>(slugcat::gltf::loader::loadScene(
        R"(C:\Program Files (x86)\Steam\steamapps\common\Lethal Company\VRMs\76561198110450751.vrm)"));
    // R"(D:\Downloads\Melon VRM1.vrm)"));

    return cPhs_COMPLEATE_e;
}

int ActorGltf::Delete() {
    this->~ActorGltf();
    return 1;
}

int ActorGltf::IsDelete() {
    return 1;
}

int ActorGltf::Execute() {
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    mDoMtx_stack_c::scaleM(scale);

    auto mtx = mDoMtx_stack_c::get();
    auto glmMtx = slugcat::gltf::matrix::fromDolphinMtx(mtx);

    applyTransformsRecursive(*packet.renderData, packet.renderData->root, glmMtx);

    return 1;
}

int ActorGltf::Draw() {
    dComIfGd_getOpaList()->entryImm(&packet, 0);

    return 1;
}

ActorGltf::~ActorGltf() {}

s16 ActorGltf::sProcName = -1;
ActorHandle ActorGltf::sActorHandle = -1;
ActorProfileDesc const ActorGltf::sProfile = FillInfo<ActorGltf>({
    .name = ACTOR_GLTF_NAME,
    .priority_group = 7,
    .draw_priority = fpcDwPi_OBJ_LBOX_e,
    .status = 0,
    .group = fopAc_ACTOR_e,
    .cull_type = fopAc_CULLBOX_CUSTOM_e,
});

extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError*) {
    slugcat::gltf::render::init();

    constexpr static GfxDrawTypeDesc drawDesc = {
        .struct_size = sizeof(GfxDrawTypeDesc),
        .label = "wawa",
        .draw = &WawaDraw,
        .user_data = nullptr,
    };
    auto result = svc_gfx->register_draw_type(mod_ctx, &drawDesc, &gDrawModelCommandType);
    if (result != MOD_OK) {
        mods::log::error("Failed to register draw!");
        return result;
    }

    if (svc_actor->register_actor(mod_ctx, &ActorGltf::sProfile, &ActorGltf::sProcName,
            &ActorGltf::sActorHandle) != MOD_OK)
    {
        mods::log::error("Failed to register actor wrock!");
        return MOD_ERROR;
    }

    static constexpr stage_actor_data_class gltfParams{
        .name = ACTOR_GLTF_NAME,
        .base =
            {
                .parameters = 0,
                .position = {0.0f, 800.0f, -1800.0f},
                .angle = {0, 0, 0},
                .setID = 0xFFFF,
            },
    };
    if (svc_stage->add_actor(mod_ctx, "F_SP103", 1, -1, &gltfParams, sizeof(gltfParams), nullptr) !=
        MOD_OK)
    {
        mods::log::error("Adding gltf to F_SP103 Failed!");
        return MOD_ERROR;
    }

    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    return MOD_OK;
}
}
