#pragma once

#include "mods/svc/actor.h"

// Base actor class definitions
#include "f_op/f_op_actor.h"

// Definitions for request_of_phase_process_class and cPhs_Step
#include "SSystem/SComponent/c_phase.h"

#include "render.hpp"

#define ACTOR_GLTF_NAME "m_gltf"

namespace slugcat::gltf {

class ActorGltf : public fopAc_ac_c {
public:
    render::FoobarPacket packet;
    std::shared_ptr<scene::Scene> scene;

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

extern std::vector<ActorGltf*> gAllActors;

}
