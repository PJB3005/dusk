#pragma once

#include "mtx.h"
#include "glm/glm.hpp"

namespace slugcat::gltf::helpers {

constexpr glm::vec3 vec(Vec const& v) noexcept {
    return {v.x, v.y, v.z};
}

}