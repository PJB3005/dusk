#include "loader.hpp"

#include <numbers>

#include "helpers/collections.hpp"
#include "helpers/hash.hpp"
#include "helpers/tg3.hpp"
#include "image_io.hpp"
#include "render.hpp"

#include <ranges>

#include "mods/svc/log.h"

#include "fmt/format.h"
#include "glm/ext.hpp"
#include "helpers/buffer.hpp"
#include "tiny_gltf_v3.h"

namespace slugcat::gltf::loader {

using namespace std::string_view_literals;

namespace {
uint64_t accessor_element_size(tg3_accessor const& accessor) {
    uint64_t arity;
    switch (accessor.type) {
    case TG3_TYPE_SCALAR:
        arity = 1;
        break;
    case TG3_TYPE_VEC2:
        arity = 2;
        break;
    case TG3_TYPE_VEC3:
        arity = 3;
        break;
    case TG3_TYPE_VEC4:
        arity = 4;
        break;
    case TG3_TYPE_MAT4:
        arity = 16;
        break;
    default:
        throw std::runtime_error(fmt::format("Unknown accessor type: {}", accessor.type));
    }

    uint64_t width;
    switch (accessor.component_type) {
    case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
    case TG3_COMPONENT_TYPE_BYTE:
        width = 1;
        break;
    case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
    case TG3_COMPONENT_TYPE_SHORT:
        width = 2;
        break;
    case TG3_COMPONENT_TYPE_UNSIGNED_INT:
    case TG3_COMPONENT_TYPE_INT:
    case TG3_COMPONENT_TYPE_FLOAT:
        width = 4;
        break;
    case TG3_COMPONENT_TYPE_DOUBLE:
        width = 8;
        break;
    default:
        throw std::runtime_error(
            fmt::format("Unknown component type: {}", accessor.component_type));
    }

    return arity * width * accessor.count;
}

scene::BufferAccessor convert_accessor(wgpu::Buffer buf, tg3_accessor const& accessor) {
    return {
        std::move(buf),
        accessor.byte_offset,
        accessor_element_size(accessor),
        accessor.component_type,
        accessor.count,
    };
}

using tg3::tg3_string_view;

struct Tg3Model {
    tg3_model model{};
    tg3_error_stack error_stack{};

    explicit Tg3Model(char const* filePath) {
        tg3_parse_options options;
        tg3_parse_options_init(&options);

        tg3_error_stack_init(&error_stack);

        auto const result =
            tg3_parse_file(&model, &error_stack, filePath, std::strlen(filePath), &options);
        if (result != TG3_OK) {
            for (uint32_t i = 0; i < error_stack.count; i++) {
                auto string = fmt::format("gltf error: {}", error_stack.entries[i].message);
                svc_log->error(mod_ctx, string.c_str());
            }

            throw std::runtime_error("Failed to load model");
        }
    }

    ~Tg3Model() {
        tg3_error_stack_free(&error_stack);
        tg3_model_free(&model);
    }

    tg3_model* operator->() noexcept { return &model; }
    tg3_model& operator*() noexcept { return model; }
};

struct SamplerKey {
    wgpu::FilterMode minFilter;
    wgpu::FilterMode magFilter;
    wgpu::MipmapFilterMode mipmapFilter;
    wgpu::AddressMode modeU;
    wgpu::AddressMode modeV;

    constexpr bool operator==(SamplerKey const& other) const {
        return minFilter == other.minFilter && magFilter == other.magFilter &&
               mipmapFilter == other.mipmapFilter && modeU == other.modeU && modeV == other.modeV;
    }
};
}  // namespace
}  // namespace slugcat::gltf::loader

template <>
struct std::hash<slugcat::gltf::loader::SamplerKey> {
    std::size_t operator()(slugcat::gltf::loader::SamplerKey const& key) const noexcept {
        using namespace slugcat::gltf::helpers;

        return hashCombine(key.minFilter, key.magFilter, key.mipmapFilter, key.modeU, key.modeV);
    }
};

namespace slugcat::gltf::loader {
namespace {

/**
 * Transient state used during loader process.
 */
struct LoaderState {
    tg3_model const& model;
    scene::Scene& scene;

    std::vector<scene::EntityId> nodesToEntities;

    std::unordered_map<int32_t, std::shared_ptr<scene::Texture>> textures;
    std::unordered_map<int32_t, std::shared_ptr<scene::Material>> materials;
    std::unordered_map<int32_t, std::shared_ptr<scene::MeshData>> meshes;
    std::unordered_map<int32_t, std::shared_ptr<scene::SharedSkinData>> skinData;
    std::unordered_map<int32_t, wgpu::Buffer> bufferViews;
    std::unordered_map<SamplerKey, wgpu::Sampler> samplers;

    scene::EntityId alloc_entity(std::string_view name) {
        scene::EntityId const id = scene.entities.size();

        scene.entities.emplace_back(std::make_unique<scene::Entity>(name));

        return id;
    }

    scene::BufferAccessor load_buffer(int32_t index) {
        auto const& accessor = model.accessors[index];

        auto const found = bufferViews.find(index);
        if (found != bufferViews.end()) {
            return convert_accessor(found->second, accessor);
        }

        auto const& bufferView = model.buffer_views[accessor.buffer_view];
        auto const& gltfBuffer = model.buffers[bufferView.buffer];

        if (bufferView.byte_stride != 0) {
            throw std::runtime_error("Unable to deal with byte_stride in buffer views!");
        }

        wgpu::BufferDescriptor bufferDesc{
            .usage = wgpu::BufferUsage::CopyDst,
            .size = bufferView.byte_length,
            .mappedAtCreation = false,
        };

        switch (bufferView.target) {
        case TG3_TARGET_ARRAY_BUFFER:
            bufferDesc.usage |= wgpu::BufferUsage::Vertex;
            break;
        case TG3_TARGET_ELEMENT_ARRAY_BUFFER:
            bufferDesc.usage |= wgpu::BufferUsage::Index;
            break;
        default:
            throw std::runtime_error("Unknown buffer target");
        }

        auto buffer = render::sDevice.CreateBuffer(&bufferDesc);
        bufferViews.emplace(index, buffer);

        render::sQueue.WriteBuffer(
            buffer, 0, &gltfBuffer.data.data[bufferView.byte_offset], bufferView.byte_length);

        return convert_accessor(std::move(buffer), accessor);
    }

    std::shared_ptr<scene::Material> loadMaterial(int32_t const material_id) {
        if (material_id < 0) {
            throw std::runtime_error("Invalid material ID");
        }

        return collections::get_or_new(materials, material_id, [&] {
            auto mat = std::make_shared<scene::Material>();
            auto const& tg3Material = model.materials[material_id];
            auto const& pbr = tg3Material.pbr_metallic_roughness;

            mat->color = {
                pbr.base_color_factor[0],
                pbr.base_color_factor[1],
                pbr.base_color_factor[2],
                pbr.base_color_factor[3],
            };
            mat->texture = loadTexture(pbr.base_color_texture.index);

            return mat;
        });
    }

    std::shared_ptr<scene::Texture> loadTexture(int32_t const textureId) {
        if (textureId < 0) {
            throw std::runtime_error("Invalid texture ID");
        }

        return collections::get_or_new(textures, textureId, [&] {
            auto texture = std::make_shared<scene::Texture>();
            auto const& tg3Texture = model.textures[textureId];

            texture->sampler = load_sampler(tg3Texture.sampler);

            if (tg3Texture.source < 0) {
                throw std::runtime_error("Invalid image ID");
            }

            auto loadedData = image_io::load(model, tg3Texture.source);

            wgpu::TextureDescriptor const textureDesc{
                .usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding,
                .dimension = wgpu::TextureDimension::e2D,
                .size =
                    {
                        .width = loadedData.extent.width,
                        .height = loadedData.extent.height,
                    },
                .format = loadedData.format,
                .mipLevelCount = loadedData.mipLevels,
                .sampleCount = 1,
            };

            texture->texture = render::sDevice.CreateTexture(&textureDesc);
            texture->textureView = texture->texture.CreateView();

            wgpu::TexelCopyTextureInfo const destInfo{
                .texture = texture->texture,
                .mipLevel = 0,
                .origin = {},
            };

            wgpu::TexelCopyBufferLayout const srcLayout{
                .offset = 0,
                .bytesPerRow = 4 * loadedData.extent.width,
                .rowsPerImage = loadedData.extent.height,
            };

            wgpu::Extent3D const writeExtent{loadedData.extent.width, loadedData.extent.height};

            auto const& bufRef = loadedData.buffer[0];
            render::sQueue.WriteTexture(
                &destInfo, bufRef.data(), bufRef.size(), &srcLayout, &writeExtent);

            return texture;
        });
    }

    wgpu::Sampler load_sampler(int32_t const sampler_id) {
        auto const& tg3_sampler = model.samplers[sampler_id];
        auto const key = keyFromSampler(tg3_sampler);

        return collections::get_or_new(samplers, key, [&] {
            wgpu::SamplerDescriptor samplerDesc{
                .addressModeU = key.modeU,
                .addressModeV = key.modeV,
                .magFilter = key.magFilter,
                .minFilter = key.minFilter,
                .mipmapFilter = key.mipmapFilter,
            };

            if (samplerDesc.mipmapFilter == wgpu::MipmapFilterMode::Undefined) {
                samplerDesc.lodMaxClamp = 0.0f;
            }

            return render::sDevice.CreateSampler(&samplerDesc);
        });
    }

private:
    static constexpr wgpu::AddressMode addressModeFromTg3(int32_t wrap) {
        switch (wrap) {
        case TG3_TEXTURE_WRAP_CLAMP_TO_EDGE:
            return wgpu::AddressMode::ClampToEdge;
        case TG3_TEXTURE_WRAP_MIRRORED_REPEAT:
            return wgpu::AddressMode::MirrorRepeat;
        default:
            return wgpu::AddressMode::Repeat;
        }
    }

    static SamplerKey keyFromSampler(tg3_sampler const& sampler) {
        SamplerKey key{};

        key.magFilter = sampler.mag_filter == TG3_TEXTURE_FILTER_NEAREST ?
                            wgpu::FilterMode::Nearest :
                            wgpu::FilterMode::Linear;

        switch (sampler.min_filter) {
        case TG3_TEXTURE_FILTER_NEAREST:
        case TG3_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
        case TG3_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
            key.minFilter = wgpu::FilterMode::Nearest;
            break;
        default:
            key.minFilter = wgpu::FilterMode::Linear;
            break;
        }

        switch (sampler.min_filter) {
        case TG3_TEXTURE_FILTER_NEAREST:
        case TG3_TEXTURE_FILTER_LINEAR:
            key.mipmapFilter = wgpu::MipmapFilterMode::Undefined;  // Disable mipmaps
            break;
        case TG3_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
        case TG3_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            key.mipmapFilter = wgpu::MipmapFilterMode::Linear;
            break;
        default:
            key.mipmapFilter = wgpu::MipmapFilterMode::Nearest;
            break;
        }

        key.modeU = addressModeFromTg3(sampler.wrap_s);
        key.modeV = addressModeFromTg3(sampler.wrap_t);

        return key;
    }

public:
    std::shared_ptr<scene::SharedSkinData> loadSkinData(int32_t const skinId) {
        return collections::get_or_new(skinData, skinId, [&] {
            auto const& tg3Skin = model.skins[skinId];

            auto data = std::make_shared<scene::SharedSkinData>();

            if (tg3Skin.inverse_bind_matrices < 0) {
                // Default to identity matrices, as per glTF spec.
                data->inverseBindMatrices.resize(tg3Skin.joints_count, glm::identity<glm::mat4>());
            } else {
                auto const& accessor = model.accessors[tg3Skin.inverse_bind_matrices];
                if (accessor.buffer_view < 0) {
                    throw std::runtime_error("No buffer view on accessor");
                }

                if (accessor.count < tg3Skin.joints_count) {
                    throw std::runtime_error("Too little matrices in buffer!");
                }

                auto const& bufferView = model.buffer_views[accessor.buffer_view];
                auto const& buffer = model.buffers[bufferView.buffer];

                auto start = buffer.data.data + bufferView.byte_offset + accessor.byte_offset;
                data->inverseBindMatrices.resize(tg3Skin.joints_count);
                std::memcpy(data->inverseBindMatrices.data(), start,
                    tg3Skin.joints_count * sizeof(glm::mat4));
            }

            // Joints resolved later.

            return data;
        });
    }
};

std::optional<int32_t> find_accessor_opt(
    tg3_primitive const& primitive, std::string_view const name) {
    for (int i = 0; i < primitive.attributes_count; i++) {
        if (tg3_string_view(primitive.attributes[i].key) == name) {
            return primitive.attributes[i].value;
        }
    }

    return std::nullopt;
}

int32_t find_accessor(tg3_primitive const& primitive, std::string_view const name) {
    auto result = find_accessor_opt(primitive, name);
    if (result.has_value()) {
        return *result;
    }
    throw std::runtime_error(fmt::format("Unable to find {} attribute in mesh primitive", name));
}

glm::mat4 read_gltf_matrix(double const (&matrix)[16]) {
    return {
        matrix[0],
        matrix[1],
        matrix[2],
        matrix[3],
        matrix[4],
        matrix[5],
        matrix[6],
        matrix[7],
        matrix[8],
        matrix[9],
        matrix[10],
        matrix[11],
        matrix[12],
        matrix[13],
        matrix[14],
        matrix[15],
    };
}

glm::quat read_glm_quat(double const (&quat)[4]) {
    return {
        static_cast<float>(quat[3]),
        static_cast<float>(quat[0]),
        static_cast<float>(quat[1]),
        static_cast<float>(quat[2]),
    };
}

void applyNodeTransform(scene::Entity& entity, tg3_node const& node) {
    if (node.has_matrix) {
        entity.localXform = read_gltf_matrix(node.matrix);
    } else {
        auto mat = glm::identity<glm::mat4>();
        mat = glm::scale(mat, {node.scale[0], node.scale[2], node.scale[2]});
        mat *= glm::mat4_cast(read_glm_quat(node.rotation));
        mat = glm::translate(mat, {node.translation[0], node.translation[1], node.translation[2]});
        entity.localXform = mat;
    }
}

std::shared_ptr<scene::MeshData> makeMesh(LoaderState& state, int32_t mesh_index) {
    if (mesh_index < 0) {
        throw std::runtime_error("Invalid mesh ID!");
    }

    return collections::get_or_new(state.meshes, mesh_index, [&] {
        auto meshData = std::make_shared<scene::MeshData>();

        auto const& mesh = state.model.meshes[mesh_index];
        for (int prim = 0; prim < mesh.primitives_count; prim++) {
            auto const& primitive = mesh.primitives[prim];

            if (primitive.mode != TG3_MODE_TRIANGLES) {
                throw std::runtime_error(
                    fmt::format("Unsupported primitive mode: {}", primitive.mode));
            }

            scene::PrimitiveData primData{};
            primData.material = state.loadMaterial(primitive.material);

            primData.index = state.load_buffer(primitive.indices);
            auto const vtxIdx = find_accessor(primitive, "POSITION"sv);
            primData.vertex = state.load_buffer(vtxIdx);
            auto const texCoordIdx = find_accessor(primitive, "TEXCOORD_0"sv);
            primData.texCoord = state.load_buffer(texCoordIdx);

            auto const jointsIdx = find_accessor_opt(primitive, "JOINTS_0"sv);
            if (jointsIdx.has_value()) {
                auto const weightsIdx = find_accessor(primitive, "WEIGHTS_0"sv);

                primData.skinData = {
                    .joints = state.load_buffer(*jointsIdx),
                    .weights = state.load_buffer(weightsIdx),
                };

                if (primData.skinData->joints.componentType != TG3_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    throw std::runtime_error("Unsupported component type for joints");
                }

                if (primData.skinData->weights.componentType != TG3_COMPONENT_TYPE_FLOAT) {
                    throw std::runtime_error("Unsupported component type for weights");
                }
            }

            meshData->primitives.emplace_back(std::move(primData));
        }

        return meshData;
    });
}

scene::EntityId convertNode(LoaderState& state, int32_t nodeIdx) {
    auto const& node = state.model.nodes[nodeIdx];
    auto id = state.alloc_entity(tg3_string_view(node.name));
    auto& entity = state.scene.get_entity(id);
    state.nodesToEntities.at(nodeIdx) = id;

    if (node.mesh >= 0) {
        entity.mesh = makeMesh(state, node.mesh);

        state.scene.meshes.push_back(id);
    }

    if (node.skin >= 0) {
        entity.skinData = state.loadSkinData(node.skin);

        state.scene.skinned.push_back(id);
    }

    applyNodeTransform(entity, node);

    for (int c = 0; c < node.children_count; c++) {
        entity.children.push_back(convertNode(state, node.children[c]));
    }

    return id;
}

void initMaterials(LoaderState& state) {
    std::vector<uint8_t> bufferData;
    std::vector<std::pair<int32_t, GfxRange>> ranges;

    for (const auto& [id, material] : state.materials) {
        render::UniformMaterial const uniform{
            .color = material->color,
        };

        auto const range = buffer::appendAligned(
            bufferData, uniform, render::sLimits.minUniformBufferOffsetAlignment);
        ranges.emplace_back(id, range);
    }

    wgpu::BufferDescriptor const bufferDesc{
        .label = "Material shared UBO"sv,
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform,
        .size = bufferData.size(),
        .mappedAtCreation = false,
    };

    auto const buffer = render::sDevice.CreateBuffer(&bufferDesc);
    render::sQueue.WriteBuffer(buffer, 0, bufferData.data(), bufferData.size());

    for (auto [id, range] : ranges) {
        auto& material = state.materials[id];

        wgpu::BindGroupEntry const entries[]{
            {
                .binding = 0,
                .buffer = buffer,
                .offset = range.offset,
                .size = range.size,
            },
            {
                .binding = 1,
                .textureView = material->texture->textureView,
            },
            {
                .binding = 2,
                .sampler = material->texture->sampler,
            },
        };

        wgpu::BindGroupDescriptor const bindGroupDesc{.layout = render::sBindGroupLayoutMaterial,
            .entryCount = std::size(entries),
            .entries = entries};

        material->bindGroup = render::sDevice.CreateBindGroup(&bindGroupDesc);
    }
}

void initSkins(LoaderState& state) {
    for (auto const& [id, skin] : state.skinData) {
        auto const& tg3Skin = state.model.skins[id];
        skin->joints.resize(tg3Skin.joints_count);
        for (auto i = 0; i < tg3Skin.joints_count; i++) {
            auto const jointIdx = tg3Skin.joints[i];
            skin->joints[i] = state.nodesToEntities.at(jointIdx);
        }
    }
}

}  // namespace

scene::Scene loadScene(char const* path) {
    Tg3Model model(path);

    if (model->default_scene < 0) {
        throw std::runtime_error("No default scene defined!");
    }

    scene::Scene loaded;
    LoaderState state{*model, loaded};
    state.nodesToEntities.resize(model->nodes_count, -1);

    auto const& scene = model->scenes[model->default_scene];

    loaded.root = state.alloc_entity(fmt::format("_SceneRoot ({})", tg3_string_view(scene.name)));

    auto& rootEnt = loaded.get_entity(loaded.root);

    for (int c = 0; c < scene.nodes_count; c++) {
        auto const& node = scene.nodes[c];

        rootEnt.children.push_back(convertNode(state, node));
    }

    initMaterials(state);
    initSkins(state);

    for (auto const& entity : loaded.entities) {
        if (entity->name == "Spine"sv) {
            entity->localXform = rotate(entity->localXform, std::numbers::pi_v<float> / 4, glm::vec3(0, 1, 0));
        }
    }

    return loaded;
}

}  // namespace slugcat::gltf::loader
