#pragma once

#include "mods/svc/actor.h"

// Base actor class definitions
#include "f_op/f_op_actor.h"

// Definitions for request_of_phase_process_class and cPhs_Step
#include "SSystem/SComponent/c_phase.h"

#include <webgpu/webgpu_cpp.h>

#include "scene.hpp"
#include "tiny_gltf_v3.h"

#define ACTOR_GLTF_NAME "m_gltf"

class ActorGltf;

class FoobarPacket final : public J3DPacket {
public:
    wgpu::RenderPipeline pipeline;
    std::shared_ptr<slugcat::gltf::scene::Scene> renderData;

    FoobarPacket();
    void draw() override;

    friend class ActorGltf;
};

class ActorGltf : public fopAc_ac_c {
public:
    FoobarPacket packet;

    request_of_phase_process_class mPhase;

    ~ActorGltf() override;
    cPhs_Step Create();
    int CreateHeap();
    int Delete();
    int IsDelete();
    int Execute();
    int Draw();
    static int createHeapCallBack(fopAc_ac_c*);

    static s16 sProcName;
    static ActorHandle sActorHandle;
    static const ActorProfileDesc sProfile;
};
