#pragma once

#include "settings_store.hpp"

#include <optional>

namespace xiui
{
    inline constexpr size_t maximum_profile_key = 400 * 1024;
    struct shared_profile { std::string name; settings_store preferences; };
    auto EncodeProfileKey(const std::string& name, const settings_store& snapshot, std::string& error) -> std::string;
    auto DecodeProfileKey(std::string_view key, std::string& error) -> std::optional<shared_profile>;
}
