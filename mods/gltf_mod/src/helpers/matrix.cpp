#include "matrix.hpp"
#include "glm/gtx/matrix_decompose.hpp"

namespace slugcat::gltf::matrix {

glm::mat4 fromDolphinMtx(Mtx matrix) noexcept {
    // clang-format off
    return {
        matrix[0][0], matrix[1][0], matrix[2][0], 0.0f,
        matrix[0][1], matrix[1][1], matrix[2][1], 0.0f,
        matrix[0][2], matrix[1][2], matrix[2][2], 0.0f,
        matrix[0][3], matrix[1][3], matrix[2][3], 1.0f,
    };
    // clang-format on
}

/**
 * Decompose a matrix to translation, rotation, and scale.
 *
 * @remarks Skew and perspective decomposition is discarded.
 *
 * @return A pair containing the translation, rotation, and scale. In that order.
 */
std::tuple<glm::vec3, glm::quat, glm::vec3> decompose(glm::mat4 const& matrix) {
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;

    glm::decompose(matrix, scale, rotation, translation, skew, perspective);

    return {translation, rotation, scale};
}

void toInterpMatrix(glm::mat4 const& src, mods::interp::InterpMatrix& dst) noexcept {
    dst.mtx[0][0] = src[0][0];
    dst.mtx[1][0] = src[0][1];
    dst.mtx[2][0] = src[0][2];
    dst.mtx[0][1] = src[1][0];
    dst.mtx[1][1] = src[1][1];
    dst.mtx[2][1] = src[1][2];
    dst.mtx[0][2] = src[2][0];
    dst.mtx[1][2] = src[2][1];
    dst.mtx[2][2] = src[2][2];
    dst.mtx[0][3] = src[3][0];
    dst.mtx[1][3] = src[3][1];
    dst.mtx[2][3] = src[3][2];

    dst.record();
}

glm::mat4 readInterpMatrix(mods::interp::InterpMatrix const& source) noexcept {
    Mtx result;
    source.readInterpolated(result);
    return fromDolphinMtx(result);
}


}  // namespace slugcat::gltf::matrix
