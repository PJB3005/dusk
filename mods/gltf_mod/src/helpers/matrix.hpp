#pragma once

#include "dolphin/mtx.h"
#include "glm/fwd.hpp"
#include "glm/mat4x4.hpp"
#include "mods/svc/interp.hpp"

namespace slugcat::gltf::matrix {

glm::mat4 fromDolphinMtx(Mtx matrix) noexcept;
std::tuple<glm::vec3, glm::quat, glm::vec3> decompose(glm::mat4 const& matrix);
void toInterpMatrix(glm::mat4 const& src, mods::interp::InterpMatrix& dst) noexcept;
glm::mat4 readInterpMatrix(mods::interp::InterpMatrix const& source) noexcept;

}  // namespace slugcat::gltf::matrix
