#pragma once

#include "api.h"

#include <stdexcept>

namespace mods {

struct ModException : std::runtime_error {
    explicit ModException(const char* message) : std::runtime_error(message) {}
};

constexpr void throw_for_result(ModResult result) {
    switch (result) {
    case MOD_OK:
        return;
    case MOD_ERROR:
        throw ModException("unspecified error");
    case MOD_UNAVAILABLE:
        throw ModException("unavailable");
    case MOD_UNSUPPORTED:
        throw ModException("unsupported");
    case MOD_CONFLICT:
        throw ModException("encountered conflict");
    case MOD_INVALID_ARGUMENT:
        throw ModException("invalid argument");
    default:
        throw ModException("Unknown result");
    }
}

}
