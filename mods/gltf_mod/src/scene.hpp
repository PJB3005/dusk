#pragma once

#include "helpers/matrix.hpp"

#include <memory>
#include <string>

#include "glm/ext/matrix_transform.hpp"
#include "glm/glm.hpp"
#include "webgpu/webgpu_cpp.h"

namespace slugcat::gltf::scene {

using EntityId = uint32_t;

struct Texture {
    wgpu::Texture texture;
    wgpu::TextureView textureView;
    wgpu::Sampler sampler;
};

struct Material {
    glm::vec4 color;

    std::shared_ptr<Texture> texture;
    wgpu::BindGroup bindGroup;
};

struct BufferAccessor {
    wgpu::Buffer buffer;
    uint64_t offset;
    uint64_t size;
    int32_t componentType; // Sure why not
    uint64_t count;
};

struct SharedSkinData {
    std::vector<EntityId> joints;
    std::vector<glm::mat4> inverseBindMatrices;
};

struct PrimitiveSkinData {
    BufferAccessor joints;
    BufferAccessor weights;
};

struct PrimitiveData {
    BufferAccessor vertex;
    BufferAccessor texCoord;
    BufferAccessor index;
    std::shared_ptr<Material> material;
    std::optional<PrimitiveSkinData> skinData;
};

struct MeshData {
    std::vector<PrimitiveData> primitives;
};

struct Entity {
    std::string name;

    glm::mat4 localXform = glm::identity<glm::mat4>();
    glm::mat4 globalXform = glm::identity<glm::mat4>();

    std::vector<EntityId> children;

    std::shared_ptr<MeshData> mesh; // Optional
    std::shared_ptr<SharedSkinData> skinData; // Optional

    explicit Entity(std::string_view const name) : name(name) {
    }
    Entity(Entity const&) = delete;
    Entity(Entity&&) = default;
};

struct Scene {
    std::vector<std::unique_ptr<Entity>> entities;
    std::vector<EntityId> meshes;
    std::vector<EntityId> skinned;

    EntityId root;

    [[nodiscard]] Entity const& get_entity(EntityId id) const {
        return *entities.at(id);
    }

    [[nodiscard]] Entity& get_entity(EntityId id) {
        return *entities.at(id);
    }
};

}
