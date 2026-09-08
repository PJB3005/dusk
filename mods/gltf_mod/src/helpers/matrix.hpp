#pragma once

#include "dolphin/mtx.h"
#include "glm/mat4x4.hpp"

namespace slugcat::gltf::matrix {

glm::mat4 fromDolphinMtx(Mtx matrix) noexcept;

}
