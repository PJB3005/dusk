#pragma once
#include "mods/svc/config.h"

namespace slugcat::gltf::config {

extern ConfigVarHandle cVarPathHandle;
extern ConfigVarHandle cVarVrmScaleHandle;

constexpr int64_t kScaleBase = 100;

void init();

}