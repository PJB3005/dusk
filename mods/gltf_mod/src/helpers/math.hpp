#pragma once

#include "mtx.h"
#include "glm/glm.hpp"

namespace slugcat::gltf::helpers {

constexpr float Rad2Deg = 180 / std::numbers::pi_v<float>;
constexpr float Deg2Rad = std::numbers::pi_v<float> / 180;

constexpr glm::vec3 vec(Vec const& v) noexcept {
    return {v.x, v.y, v.z};
}

}