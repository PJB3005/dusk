#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"
#include "d/d_com_inf_game.h"

#include "glm/gtx/matrix_decompose.hpp"
#include "imgui.h"
#include "loader.hpp"
#include "mod.hpp"
#include "ui.hpp"

#include <numbers>

#include "bones.hpp"
#include "helpers/buffer.hpp"
#include "helpers/math.hpp"
#include "render.hpp"
#include "scene.hpp"

#include "mods/service.hpp"
#include "mods/svc/actor.hpp"
#include "mods/svc/camera.h"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"
#include "mods/svc/log.hpp"

#include <d/actor/d_a_alink.h>

#include "config.hpp"
#include "debug_imgui.hpp"
#include "helpers/hash.hpp"
#include "helpers/result.hpp"
#include "webgpu/webgpu_cpp.h"

DEFINE_HOOK_SYMBOL("daAlink_modelCallBack", int(J3DJoint* i_joint, int param_1), ModelCallback);

DEFINE_HOOK(&daAlink_c::createHeap, LinkCreateHeap);

using namespace mods::actor;
using namespace std::string_view_literals;
using namespace std::string_literals;
using namespace slugcat::gltf::scene;
using slugcat::gltf::helpers::checkResult;

namespace slugcat::gltf {

std::vector<ActorGltf*> gAllActors;

namespace {

// clang-format off
std::pair<std::string, u16> const vrmBonesToLinkJoints[]{
    {bones::vrm::kBoneModRoot,       bones::link::kJointCenter},
    {bones::vrm::kBoneSpine,         bones::link::kJointBackbone1},
    {bones::vrm::kBoneChest,         bones::link::kJointBackbone2},
    {bones::vrm::kBoneNeck,          bones::link::kJointNeck},
    {bones::vrm::kBoneHead,          bones::link::kJointHead},
    {bones::vrm::kBoneLeftShoulder,  bones::link::kJointShoulderL},
    {bones::vrm::kBoneLeftUpperArm,  bones::link::kJointArmL1},
    {bones::vrm::kBoneLeftLowerArm,  bones::link::kJointArmL2},
    {bones::vrm::kBoneLeftHand,      bones::link::kJointHandL},
    {bones::vrm::kBoneRightShoulder, bones::link::kJointShoulderR},
    {bones::vrm::kBoneRightUpperArm, bones::link::kJointArmR1},
    {bones::vrm::kBoneRightLowerArm, bones::link::kJointArmR2},
    {bones::vrm::kBoneRightHand,     bones::link::kJointHandR},
    {bones::vrm::kBoneHips,          bones::link::kJointWaist},
    {bones::vrm::kBoneLeftUpperLeg,  bones::link::kJointLegL1},
    {bones::vrm::kBoneLeftLowerLeg,  bones::link::kJointLegL2},
    {bones::vrm::kBoneLeftFoot,      bones::link::kJointFootL},
    {bones::vrm::kBoneRightUpperLeg, bones::link::kJointLegR1},
    {bones::vrm::kBoneRightLowerLeg, bones::link::kJointLegR2},
    {bones::vrm::kBoneRightFoot,     bones::link::kJointFootR},
};
// clang-format on

struct RotationPair {
    glm::quat local = glm::identity<glm::quat>();
    glm::quat global = glm::identity<glm::quat>();
};

glm::quat getRotationFromTransformInfo(J3DTransformInfo const& transformInfo) {
    Quaternion q;
    JMAEulerToQuat(
        transformInfo.mRotation.x, transformInfo.mRotation.y, transformInfo.mRotation.z, &q);

    return glm::quat(q.w, q.x, q.y, q.z);
}

void getRestLocalRotationsRecursive(
    std::vector<RotationPair>& rotations, glm::quat const& currentRotation, J3DJoint* joint) {
    if (!joint) {
        return;
    }

    auto const localRot = getRotationFromTransformInfo(joint->getTransformInfo());
    auto const newCurrent = currentRotation * localRot;

    rotations.at(joint->getJntNo()) = {localRot, newCurrent};

    getRestLocalRotationsRecursive(rotations, newCurrent, joint->getChild());

    // Tail call 🙏
    getRestLocalRotationsRecursive(rotations, currentRotation, joint->getYounger());
}

std::vector<RotationPair> getRestRotations(J3DModelData* modelData) {
    std::vector<RotationPair> rotations;
    rotations.resize(modelData->getJointNum());

    getRestLocalRotationsRecursive(
        rotations, glm::identity<glm::quat>(), modelData->getJointTree().getRootNode());

    return rotations;
}

void getLocalRotationsRecursive(std::vector<glm::quat>& rotations, J3DModel* model,
    glm::mat4 const& parentMtx, J3DJoint* joint) {
    if (!joint) {
        return;
    }

    auto const anmMtxP = model->getAnmMtx(joint->getJntNo());
    auto const anmMtx = matrix::fromDolphinMtx(anmMtxP);

    auto const localMtx = glm::inverse(parentMtx) * anmMtx;

    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;

    glm::decompose(localMtx, scale, rotation, translation, skew, perspective);

    rotations.at(joint->getJntNo()) = rotation;

    getLocalRotationsRecursive(rotations, model, anmMtx, joint->getChild());

    // Tail call 🙏
    getLocalRotationsRecursive(rotations, model, parentMtx, joint->getYounger());
}

std::vector<glm::quat> getLocalRotations(J3DModel* model) {
    std::vector<glm::quat> rotations;
    rotations.resize(model->mModelData->getJointNum());

    auto const baseMtx = matrix::fromDolphinMtx(model->getBaseTRMtx());

    getLocalRotationsRecursive(
        rotations, model, baseMtx, model->mModelData->getJointTree().getRootNode());

    return rotations;
}

void applyLinkPose(daAlink_c const& link, Scene& scene) {
    auto const localRotations = getLocalRotations(link.mpLinkModel);
    auto const restRotations = getRestRotations(link.mpLinkModel->getModelData());

    for (const auto& [humanoidBone, linkJoint] : vrmBonesToLinkJoints) {
        auto const foundEnt = scene.humanoidBones.find(humanoidBone);
        if (foundEnt == scene.humanoidBones.end()) {
            continue;
        }

        auto const& localRotJoint = localRotations.at(linkJoint);
        auto const& restRotJoint = restRotations.at(linkJoint);

        auto& entity = scene.get_entity(foundEnt->second);

        auto poseNormalized = restRotJoint.global * glm::inverse(restRotJoint.local) *
                              localRotJoint * glm::inverse(restRotJoint.global);

        entity.rotation = entity.referenceRotation * glm::inverse(entity.globalReferenceRotation) *
                          poseNormalized * entity.globalReferenceRotation;
    }
}

void applyLinkRootTranslation(
    daAlink_c const& link, Scene& scene, glm::mat4 const& replacementBaseMtx) {
    auto const baseMtx = matrix::fromDolphinMtx(link.mpLinkModel->getBaseTRMtx());
    auto const rootMtx = matrix::fromDolphinMtx(link.mpLinkModel->getAnmMtx(0));
    auto const dataOffset = link.mpLinkModel->getModelData()
                                ->getJointTree()
                                .getJointNodePointer(0)
                                ->getTransformInfo()
                                .mTranslate;

    auto const restTranslatedMtx = baseMtx * glm::translate(helpers::vec(dataOffset));

    auto rootLocal = glm::inverse(restTranslatedMtx) * rootMtx;
    auto [offset, _rotation, _scale] = matrix::decompose(rootLocal);

    auto const rootFound = scene.humanoidBones.find(bones::vrm::kBoneModRoot);
    if (rootFound == scene.humanoidBones.end()) {
        return;
    }

    auto [_replTrans, _replRot, _replScale] = matrix::decompose(replacementBaseMtx);

    auto& rootEnt = scene.get_entity(rootFound->second);
    auto parentGlobal = calcParentGlobalTransform(scene, rootEnt);
    rootEnt.translation = glm::xyz(glm::inverse(parentGlobal) * glm::vec4(offset / _replScale, 1));
}

void applyTransformsRecursive(Scene& scene, EntityId entity_id, glm::mat4 const& transform) {
    auto& entity = scene.get_entity(entity_id);
    entity.localXform = calcLocalTransform(entity);
    entity.globalXform = transform * entity.localXform;

    for (auto child : entity.children) {
        applyTransformsRecursive(scene, child, entity.globalXform);
    }
}

void recordMatricesForInterp(ActorGltf& actor) {
    auto const& scene = *actor.scene;
    auto& dstMatrices = actor.packet.entityMatrices;

    dstMatrices.resize(scene.entities.size());

    for (size_t i = 0; i < scene.entities.size(); i++) {
        auto const& entity = scene.get_entity(i);
        matrix::toInterpMatrix(entity.globalXform, dstMatrices[i]);
    }
}

}  // namespace

HookAction link_create_heap(ModContext*, void* args, void*, void*) {
    fopAcM_Create(ActorGltf::sProcName, 0, 0);
    return HOOK_CONTINUE;
}

cPhs_Step ActorGltf::Create() {
    AuroraGXSync();

    size_t length;
    checkResult(svc_config->get_string(mod_ctx, config::cVarPathHandle, nullptr, 0, &length));
    std::string buf;
    buf.resize(length);
    checkResult(svc_config->get_string(
        mod_ctx, config::cVarPathHandle, buf.data(), buf.size() + 1, nullptr));

    try {
        auto loadedScene = std::make_shared<Scene>(loader::loadScene(buf.c_str()));
        packet.scene = loadedScene;
        scene = std::move(loadedScene);
    } catch (std::runtime_error const& e) {
        mods::log::error("Failed to load VRM '{}': {}", buf, e.what());
        return cPhs_ERROR_e;
    }

    gAllActors.push_back(this);

    fopAcM_setStageLayer(this);

    return cPhs_COMPLEATE_e;
}

int ActorGltf::Delete() {
    this->~ActorGltf();
    return 1;
}

int ActorGltf::IsDelete() {
    return 1;
}

bool should_draw_actor() {
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (!link) {
        return false;
    }

    return !link->checkPlayerNoDraw() && !link->checkWolf() && link->mClothesChangeWaitTimer == 0;
}

constexpr float kModelScaleFactor = 150;

int ActorGltf::Execute() {
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (!link) {
        return 1;
    }

    if (!should_draw_actor()) {
        return 0;
    }

    mDoMtx_stack_c::copy(link->mpLinkModel->getBaseTRMtx());

    int64_t scalePercent = config::kScaleBase;
    checkResult(svc_config->get_int(mod_ctx, config::cVarVrmScaleHandle, &scalePercent));
    auto const scale = kModelScaleFactor * scalePercent / static_cast<float>(config::kScaleBase);
    mDoMtx_stack_c::scaleM(scale, scale, scale);

    auto const baseMtx = matrix::fromDolphinMtx(mDoMtx_stack_c::get());

    applyLinkPose(*link, *scene);
    applyLinkRootTranslation(*link, *scene, baseMtx);

    applyTransformsRecursive(*scene, scene->root, baseMtx);
    recordMatricesForInterp(*this);

    return 1;
}

int ActorGltf::Draw() {
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (!link || !should_draw_actor()) {
        return 0;
    }

    dComIfGd_getOpaList()->entryImm(&packet, 0);

    return 1;
}

ActorGltf::~ActorGltf() {
    auto const pos = std::ranges::find(gAllActors, this);
    if (pos != gAllActors.end()) {
        gAllActors.erase(pos);
    }
}

s16 ActorGltf::sProcName = -1;
ActorHandle ActorGltf::sActorHandle = -1;
ActorProfileDesc const ActorGltf::sProfile = FillInfo<ActorGltf>({
    .name = ACTOR_GLTF_NAME,
    .priority_group = 11,
    .draw_priority = fpcDwPi_OBJ_LBOX_e,
    .status = fopAcStts_UNK_0x40000_e | fopAcStts_NOPAUSE_e,
    .group = fopAc_ACTOR_e,
    .cull_type = fopAc_CULLBOX_CUSTOM_e,
});

ModResult modInit() {
    debug_imgui::init();
    render::init();
    config::init();
    ui::init();

    mods::hook::add_pre<LinkCreateHeap>(link_create_heap);

    if (svc_actor->register_actor(mod_ctx, &ActorGltf::sProfile, &ActorGltf::sProcName,
            &ActorGltf::sActorHandle) != MOD_OK)
    {
        mods::log::error("Failed to register actor wrock!");
        return MOD_ERROR;
    }

    mods::log::info("Actor ID: {}", ActorGltf::sProcName);

    return MOD_OK;
}

}  // namespace slugcat::gltf

extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError*) {
    return slugcat::gltf::modInit();
}

MOD_EXPORT ModResult mod_update(ModError*) {
    return MOD_OK;
}

MOD_EXPORT ModResult mod_shutdown(ModError*) {
    return MOD_OK;
}
}
