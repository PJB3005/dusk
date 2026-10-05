import shaders::uniforms::{UniformGlobal, UniformMaterial, UniformObject};

@group(0) @binding(0) var<uniform> global : UniformGlobal;

@group(1) @binding(0) var<uniform> material : UniformMaterial;
@group(1) @binding(1) var tex0_t : texture_2d<f32>;
@group(1) @binding(2) var tex0_s : sampler;

@group(2) @binding(0) var<uniform> object : UniformObject;
@if (SKINNED)
@group(2) @binding(1) var<storage, read> pose : array<mat4x4f>;

struct VsInput {
    @location(0) position: vec3f,
    @location(1) texCoord: vec2f,

    @if (SKINNED)
    @location(6) joints: vec4u,
    @if (SKINNED)
    @location(7) weights: vec4f,
}

struct VsOutput {
    @builtin(position) position: vec4f,
    @location(0) texCoord: vec2f,
}

@if (SKINNED)
fn calcSkinnedMatrix(joints: vec4u, weights: vec4f) -> mat4x4f {
    return pose[joints.x] * weights.x + pose[joints.y] * weights.y + pose[joints.z] * weights.z + pose[joints.w] * weights.w;
}

@vertex
fn vs_main(input: VsInput) -> VsOutput {
    @if (SKINNED)
    let worldPos = calcSkinnedMatrix(input.joints, input.weights) * vec4f(input.position, 1.0);
    @else
    let worldPos = object.model * vec4f(input.position, 1.0);
    let pos = global.projView * worldPos;
    return VsOutput(pos, input.texCoord);
}

@fragment
fn fs_main(input: VsOutput) -> @location(0) vec4f {
    let texVal = textureSample(tex0_t, tex0_s, input.texCoord);
    return texVal * material.color;
}
