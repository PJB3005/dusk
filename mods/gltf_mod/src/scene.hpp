#pragma once

#include "helpers/matrix.hpp"

#include <memory>
#include <string>

#include "glm/ext.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/glm.hpp"
#include "mods/svc/gfx.h"
#include "webgpu/webgpu_cpp.h"

namespace slugcat::gltf::scene {

using EntityId = uint32_t;

struct Texture {
    wgpu::Texture texture;
    wgpu::TextureView textureView;
    wgpu::Sampler sampler;
};

struct Material {
    std::string name;
    glm::vec4 color;

    std::shared_ptr<Texture> texture;
    // wgpu::BindGroup bindGroup;
    int materialId;
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
    std::string const name;

    glm::vec3 translation = glm::vec3(0.0f);
    glm::quat rotation = glm::identity<glm::quat>();
    glm::vec3 scale = glm::vec3(1.0f);

    glm::quat referenceRotation = glm::identity<glm::quat>();
    glm::quat globalReferenceRotation = glm::identity<glm::quat>();

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
    std::unordered_map<std::string, EntityId> humanoidBones;
    std::vector<std::shared_ptr<Material>> materials;

    EntityId root;
    EntityId viewing {};

    [[nodiscard]] Entity const& get_entity(EntityId id) const {
        return *entities.at(id);
    }

    [[nodiscard]] Entity& get_entity(EntityId id) {
        return *entities.at(id);
    }
};

}
