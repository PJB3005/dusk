#pragma once

#include "tiny_gltf_v3.h"

namespace slugcat::gltf::tg3 {

constexpr std::string_view tg3_string_view(tg3_str str) noexcept {
    return {str.data, str.len};
}

}