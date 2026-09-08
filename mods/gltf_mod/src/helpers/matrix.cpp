#include "matrix.hpp"

#include "glm/mat4x4.hpp"

namespace slugcat::gltf::matrix {

glm::mat4 fromDolphinMtx(Mtx matrix) noexcept {
    return {
        matrix[0][0], matrix[1][0], matrix[2][0], 0.0f,
        matrix[0][1], matrix[1][1], matrix[2][1], 0.0f,
        matrix[0][2], matrix[1][2], matrix[2][2], 0.0f,
        matrix[0][3], matrix[1][3], matrix[2][3], 1.0f,
    };
}

}  // namespace slugcat::gltf::matrix
