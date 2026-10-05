#include <numbers>

#include "debug_imgui.hpp"
#include "helpers/result.hpp"
#include "imgui.h"
#include "mod.hpp"
#include "mods/svc/hook.hpp"
#include "scene.hpp"

DEFINE_HOOK_SYMBOL("mDoGph_Painter", int(), OnPaint);

extern "C" void* Amogus();

namespace slugcat::gltf::debug_imgui {

using namespace slugcat::gltf::scene;

namespace {

void show_entity(Scene& scene, EntityId idx) {
    ImGui::PushID(idx);

    auto const& ent = scene.get_entity(idx);

    if (ImGui::SmallButton(ent.name.c_str())) {
        scene.viewing = idx;
    }

    ImGui::Indent(4);

    for (auto const child : ent.children) {
        show_entity(scene, child);
    }

    ImGui::Unindent(4);

    ImGui::PopID();
}

void show_scene(Scene& scene) {
    if (ImGui::BeginChild("##tree", ImVec2(300, 0),
            ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened))
    {
        show_entity(scene, scene.root);
    }

    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginGroup();

    auto& selected = scene.get_entity(scene.viewing);

    ImGui::Text("Selected: %s", selected.name.c_str());

    glm::vec3 rotEuler = glm::eulerAngles(selected.rotation) * (180 / std::numbers::pi_v<float>);

    auto const changedTrans = ImGui::InputFloat3("Translation", &selected.translation.x);
    auto const changedRot = ImGui::InputFloat3("Rotation", &rotEuler.x);
    ImGui::InputFloat4("Quat", &selected.rotation.x);
    auto const changedScale = ImGui::InputFloat3("Scale", &selected.scale.x);

    if (changedRot) {
        selected.rotation = glm::quat(rotEuler / (180 / std::numbers::pi_v<float>));
    }

    if (selected.mesh) {
        auto const& mesh = selected.mesh;
        int id = 0;
        for (auto const& primitive : mesh->primitives) {
            auto& mat = *primitive.material;
            ImGui::PushID(id);
            ImGui::Text("Material: %s", mat.name.c_str());
            ImGui::ColorEdit4("color", &mat.color.r);

            ImGui::PopID();

            id += 1;
        }
    }

    ImGui::EndGroup();
}

bool active;

void on_interp_view(ModContext*, void*, void*, void*) {
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(Amogus()));

    if (ImGui::IsKeyPressed(ImGuiKey_Pause)) {
        active = !active;
    }

    if (!active) {
        return;
    }

    int idx = 0;
    for (auto const actor : gAllActors) {
        auto& scene = *actor->scene;
        ImGui::PushID(idx);
        if (ImGui::Begin("Real")) {
            show_scene(scene);
        }

        ImGui::End();
        ImGui::PopID();

        idx += 1;
    }
}

}

void init() {
    helpers::checkResult(mods::hook::add_post<OnPaint>(svc_hook, on_interp_view));
}

}  // namespace slugcat::gltf::debug_imgui