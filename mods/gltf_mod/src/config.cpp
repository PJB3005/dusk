#include "config.hpp"

#include "helpers/result.hpp"

namespace slugcat::gltf::config {

using helpers::checkResult;

namespace {

constexpr ConfigVarDesc cVarVrmPathDesc{
    .struct_size = sizeof(cVarVrmPathDesc),
    .name = "vrm_path",
    .type = CONFIG_VAR_STRING,
};

constexpr ConfigVarDesc cVarVrmScaleDesc{
    .struct_size = sizeof(cVarVrmScaleDesc),
    .name = "vrm_scale",
    .type = CONFIG_VAR_INT,
    .default_int = kScaleBase,
};

}  // namespace

ConfigVarHandle cVarPathHandle;
ConfigVarHandle cVarVrmScaleHandle;

void init() {
    checkResult(svc_config->register_var(mod_ctx, &cVarVrmPathDesc, &cVarPathHandle));
    checkResult(svc_config->register_var(mod_ctx, &cVarVrmScaleDesc, &cVarVrmScaleHandle));
}

}  // namespace slugcat::gltf::config