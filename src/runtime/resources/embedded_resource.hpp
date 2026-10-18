#pragma once

#include <Windows.h>
#include <cstdint>
#include <span>
#include <vector>

namespace xiui::resources
{
    auto DecodeEmbeddedResource(std::span<const uint8_t> bytes, size_t maximumSize) -> std::vector<uint8_t>;
    auto LoadEmbeddedResource(HMODULE module, uint16_t id, size_t maximumSize) -> std::vector<uint8_t>;
}
