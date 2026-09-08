#include "render.hpp"

#include <stdexcept>

namespace slugcat::gltf::render {

using namespace std::string_view_literals;

namespace {

void createBindGroupLayouts() {
    constexpr static wgpu::BindGroupLayoutEntry globalEntries[] = {
        {
            .binding = 0,
            .visibility = wgpu::ShaderStage::Vertex,
            .buffer =
                {
                    .type = wgpu::BufferBindingType::Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = sizeof(UniformGlobal),
                },
        },
    };

    constexpr static wgpu::BindGroupLayoutDescriptor descGlobal{
        .label = "global"sv,
        .entryCount = std::size(globalEntries),
        .entries = globalEntries,
    };

    sBindGroupLayoutGlobal = sDevice.CreateBindGroupLayout(&descGlobal);

    constexpr static wgpu::BindGroupLayoutEntry materialEntries[] = {
        {
            .binding = 0,
            .visibility = wgpu::ShaderStage::Fragment,
            .buffer =
                {
                    .type = wgpu::BufferBindingType::Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = sizeof(UniformMaterial),
                },
        },
        {
            .binding = 1,
            .visibility = wgpu::ShaderStage::Fragment,
            .texture =
                {
                    .sampleType = wgpu::TextureSampleType::Float,
                    .viewDimension = wgpu::TextureViewDimension::e2D,
                },
        },
        {
            .binding = 2,
            .visibility = wgpu::ShaderStage::Fragment,
            .sampler =
                {
                    .type = wgpu::SamplerBindingType::Filtering,
                },
        },
    };

    constexpr static wgpu::BindGroupLayoutDescriptor descMaterial{
        .label = "material"sv,
        .entryCount = std::size(materialEntries),
        .entries = materialEntries,
    };

    sBindGroupLayoutMaterial = sDevice.CreateBindGroupLayout(&descMaterial);

    constexpr static wgpu::BindGroupLayoutEntry objectEntries[] = {
        {
            .binding = 0,
            .visibility = wgpu::ShaderStage::Vertex,
            .buffer =
                {
                    .type = wgpu::BufferBindingType::Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = sizeof(UniformObject),
                },
        },
        {
            .binding = 1,
            .visibility = wgpu::ShaderStage::Vertex,
            .buffer =
                {
                    .type = wgpu::BufferBindingType::ReadOnlyStorage,
                    .hasDynamicOffset = false,
                    .minBindingSize = sizeof(glm::mat4),
                },
        },
    };

    constexpr static wgpu::BindGroupLayoutDescriptor descObject{
        .label = "object"sv,
        .entryCount = std::size(objectEntries),
        .entries = objectEntries,
    };

    sBindGroupLayoutObject = sDevice.CreateBindGroupLayout(&descObject);

    wgpu::BindGroupLayout const layouts[] = {
        sBindGroupLayoutGlobal,    // 0
        sBindGroupLayoutMaterial,  // 1
        sBindGroupLayoutObject,    // 2
    };

    wgpu::PipelineLayoutDescriptor const pipelineLayoutDesc{
        .bindGroupLayoutCount = std::size(layouts), .bindGroupLayouts = layouts};

    sPipelineLayout = sDevice.CreatePipelineLayout(&pipelineLayoutDesc);
}

}  // namespace

wgpu::Device sDevice;
wgpu::Queue sQueue;
GfxDeviceInfo sDeviceInfo{.struct_size = sizeof(sDeviceInfo)};
wgpu::Limits sLimits;

wgpu::BindGroupLayout sBindGroupLayoutGlobal;
wgpu::BindGroupLayout sBindGroupLayoutMaterial;
wgpu::BindGroupLayout sBindGroupLayoutObject;
wgpu::PipelineLayout sPipelineLayout;

void init() {
    if (svc_gfx->get_device_info(mod_ctx, &sDeviceInfo) != MOD_OK) {
        throw std::runtime_error("Failed to get GFX device info!");
    }

    sDevice = wgpu::Device(sDeviceInfo.device);
    sQueue = wgpu::Queue(sDeviceInfo.queue);

    sDevice.GetLimits(&sLimits);

    createBindGroupLayouts();
}

}  // namespace slugcat::gltf::render