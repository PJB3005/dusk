#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"
#include "d/d_com_inf_game.h"

#include "glm/gtx/matrix_decompose.hpp"
#include "imgui.h"
#include "loader.hpp"
#include "mod.hpp"

#include <fstream>
#include <numbers>

#include "helpers/buffer.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "shader.hpp"

#include "mods/service.hpp"
#include "mods/svc/actor.hpp"
#include "mods/svc/camera.h"
#include "mods/svc/gfx.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.hpp"
#include "mods/svc/resource.h"
#include "mods/svc/stage.h"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"

#include "webgpu/webgpu_cpp.h"
#include <d/actor/d_a_alink.h>

#include "mods/svc/ui.h"

DEFINE_MOD();
IMPORT_SERVICE(LogService, svc_log);
IMPORT_SERVICE(ActorService, svc_actor);
IMPORT_SERVICE(StageService, svc_stage);
IMPORT_SERVICE(GfxService, svc_gfx);
IMPORT_SERVICE(ResourceService, svc_resource);
IMPORT_SERVICE(CameraService, svc_camera);
IMPORT_SERVICE(HookService, svc_hook);
IMPORT_SERVICE(ConfigService, svc_config);
IMPORT_SERVICE(UiService, svc_ui);

DEFINE_HOOK_SYMBOL("mDoGph_Painter", int(), OnPaint);
DEFINE_HOOK_SYMBOL("dusk::ImGuiMenuTools::draw", void(), OnMenu);

using namespace mods::actor;
using namespace std::string_view_literals;
using namespace std::string_literals;
using namespace slugcat::gltf::scene;
using namespace slugcat::gltf::render;

extern "C" {

extern void* Amogus();
}

namespace {

UiElementHandle statusText1 = 0;

struct Payload {
    std::shared_ptr<Scene> scene;
    wgpu::RenderPipeline pipeline;

    GfxRange uniformGlobalRange;
    std::vector<GfxRange> uniformObjectRanges;
    std::vector<GfxRange> skinDataRanges;
    std::vector<GfxRange> materialRanges;
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

void checkResult(ModResult res) {
    if (res != MOD_OK) {
        throw std::runtime_error("Service call failed");
    }
}

glm::mat4 calcLocalTransform(Entity const& entity) {
    return glm::translate(entity.translation) * glm::mat4_cast(entity.rotation) *
           glm::scale(entity.scale);
}

std::pair<std::string, u16> const vrmBonesToLinkJoints[] {
    {"rightShoulder"s, 0x0B},
    {"rightUpperArm"s, 0x0C},
    {"rightLowerArm"s, 0x0D},
    {"rightHand"s, 0x0E},
};

//std::vector<struct >

void applyLinkPose(Scene& scene, ActorGltf& actorGltf) {
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (!link) {
        return;
    }

    for (const auto& [humanoidBone, linkJoint] : vrmBonesToLinkJoints) {
        auto const foundEnt = scene.humanoidBones.find(humanoidBone);
        if (foundEnt == scene.humanoidBones.end()) {
            continue;
        }

        auto& entity = scene.get_entity(foundEnt->second);

        auto& origJoint = *link->mpLinkModel->getModelData()->getJointNodePointer(linkJoint);
        auto const& origTransformInfo = origJoint.getTransformInfo();

        auto origRotation = glm::quat({origTransformInfo.mRotation.x / 32768, origTransformInfo.mRotation.y / 32768, origTransformInfo.mRotation.z / 32768});

        glm::mat4 tposeMtx =
            slugcat::gltf::matrix::fromDolphinMtx(link->mpLinkModel->getAnmMtx(linkJoint));
        glm::mat4 animatedMtx =
            slugcat::gltf::matrix::fromDolphinMtx(link->mpLinkModel->getAnmMtx(linkJoint));

        glm::mat4 offsetMtx = tposeMtx * glm::inverse(animatedMtx);
        auto offsetQuat = glm::toQuat(offsetMtx);

        entity.rotation = entity.referenceRotation * glm::inverse(entity.globalReferenceRotation) * offsetQuat * entity.globalReferenceRotation;
    }
}

void applyTransformsRecursive(
    Scene& scene, EntityId entity_id, glm::mat4 const& transform, ActorGltf* actorGltf) {
    auto& entity = scene.get_entity(entity_id);
    entity.localXform = calcLocalTransform(entity);
    entity.globalXform = transform * entity.localXform;

    for (auto child : entity.children) {
        applyTransformsRecursive(scene, child, entity.globalXform, actorGltf);
    }
}

std::vector<glm::mat4> gJointCalcBuffer;
std::vector<ActorGltf*> actors;

void show_entity(Scene& scene, EntityId idx) {
    ImGui::PushID(idx);

    auto const& ent = scene.get_entity(idx);

    if (ImGui::SmallButton(ent.name.c_str())) {
        scene.viewing = idx;
    }

    ImGui::Indent(4);

    for (auto const child : ent.children) {
        show_entity(scene, child);
    }

    ImGui::Unindent(4);

    ImGui::PopID();
}

void show_scene(Scene& scene) {
    if (ImGui::BeginChild("##tree", ImVec2(300, 0),
            ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened))
    {
        show_entity(scene, scene.root);
    }

    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginGroup();

    auto& selected = scene.get_entity(scene.viewing);

    ImGui::Text("Selected: %s", selected.name.c_str());

    glm::vec3 rotEuler = glm::eulerAngles(selected.rotation) * (180 / std::numbers::pi_v<float>);

    auto const changedTrans = ImGui::InputFloat3("Translation", &selected.translation.x);
    auto const changedRot = ImGui::InputFloat3("Rotation", &rotEuler.x);
    ImGui::InputFloat4("Quat", &selected.rotation.x);
    auto const changedScale = ImGui::InputFloat3("Scale", &selected.scale.x);

    if (changedRot) {
        selected.rotation = glm::quat(rotEuler / (180 / std::numbers::pi_v<float>));
    }

    if (selected.mesh) {
        auto const& mesh = selected.mesh;
        int id = 0;
        for (auto const& primitive : mesh->primitives) {
            auto& mat = *primitive.material;
            ImGui::PushID(id);
            ImGui::Text("Material: %s", mat.name.c_str());
            ImGui::ColorEdit4("color", &mat.color.r);

            ImGui::PopID();

            id += 1;
        }
    }

    ImGui::EndGroup();
}

bool active;

void on_interp_view(ModContext*, void*, void*, void*) {
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(Amogus()));

    if (ImGui::IsKeyPressed(ImGuiKey_Pause)) {
        active = !active;
    }

    if (!active) {
        return;
    }

    int idx = 0;
    for (auto actor : actors) {
        auto& scene = *actor->packet.renderData;
        ImGui::PushID(idx);
        if (ImGui::Begin("Real")) {
            show_scene(scene);
        }

        ImGui::End();
        ImGui::PopID();

        idx += 1;
    }
}

constexpr ConfigVarDesc cVarVrmPathDesc {
    .struct_size = sizeof(cVarVrmPathDesc),
    .name = "vrm_path",
    .type = CONFIG_VAR_STRING,
};

ConfigVarHandle cVarPathHandle;

ModResult build(ModContext*, UiElementHandle panel, void*, ModError*) {
    svc_ui->pane_add_section(mod_ctx, panel, "Settings");

    UiControlDesc control1 = UI_CONTROL_DESC_INIT;
    control1.kind = UI_CONTROL_FILE_PICKER;
    control1.label = "Path";
    control1.help_rml = "Path to .vrm";
    control1.binding = UI_BINDING_CONFIG_VAR;
    control1.config_var = cVarPathHandle;
    svc_ui->pane_add_control(mod_ctx, panel, &control1, &statusText1);

    return MOD_OK;
}

ModResult update(ModContext*, void*, ModError*) {
    return MOD_OK;
}

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

    for (auto const mat : renderData->materials) {
        UniformMaterial const matUniform {
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

cPhs_Step ActorGltf::Create() {
    AuroraGXSync();

    scale.setall(100);

    size_t length;
    checkResult(svc_config->get_string(mod_ctx, cVarPathHandle, nullptr, 0, &length));
    std::string buf;
    buf.resize(length);
    checkResult(svc_config->get_string(mod_ctx, cVarPathHandle, buf.data(), buf.size() + 1, nullptr));

    try {
        packet.renderData = std::make_shared<Scene>(
            slugcat::gltf::loader::loadScene(buf.c_str()));
    } catch (std::runtime_error const& e) {
        mods::log::error("Failed to load VRM '{}': {}", buf, e.what());
        return cPhs_ERROR_e;
    }

    actors.push_back(this);

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

    applyTransformsRecursive(*packet.renderData, packet.renderData->root, glmMtx, this);
    applyLinkPose(*packet.renderData, *this);

    return 1;
}

int ActorGltf::Draw() {
    dComIfGd_getOpaList()->entryImm(&packet, 0);

    return 1;
}

ActorGltf::~ActorGltf() {
    auto const pos = std::ranges::find(actors, this);
    if (pos != actors.end()) {
        actors.erase(pos);
    }
}

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

    mods::log::info("Actor ID: {}", ActorGltf::sProcName);

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

    checkResult(svc_config->register_var(mod_ctx, &cVarVrmPathDesc, &cVarPathHandle));

    checkResult(mods::hook::add_post<OnPaint>(svc_hook, on_interp_view));
    //checkResult(mods::hook::add_post<OnMenu>(svc_hook, on_menu));

    UiModsPanelDesc panel = UI_MODS_PANEL_DESC_INIT;
    panel.build = build;
    checkResult(svc_ui->register_mods_panel(mod_ctx, &panel));

    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError*) {
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    return MOD_OK;
}
}
