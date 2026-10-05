#include "scene.hpp"

namespace slugcat::gltf::scene {

glm::mat4 calcLocalTransform(Entity const& entity) {
    return glm::translate(entity.translation) * glm::mat4_cast(entity.rotation) *
           glm::scale(entity.scale);
}

glm::mat4 calcParentGlobalTransform(Scene const& scene, Entity const& entity) {
    return calcParentGlobalTransform(scene, entity, glm::identity<glm::mat4>());
}

glm::mat4 calcParentGlobalTransform(Scene const& scene, Entity const& entity, glm::mat4 const& rootXform) {
    if (!entity.parent.has_value()) {
        return rootXform;
    }

    auto const& parentEnt = scene.get_entity(*entity.parent);
    auto const parentLocal = calcLocalTransform(parentEnt);
    return calcParentGlobalTransform(scene, parentEnt, rootXform) * parentLocal;
}
}  // namespace slugcat::gltf::scene
