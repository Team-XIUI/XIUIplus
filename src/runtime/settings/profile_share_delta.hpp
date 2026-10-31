#pragma once

#include "profile_share.hpp"

namespace xiui::profile_share_detail
{
    auto Checksum(std::string_view data) -> uint32_t;
    auto ValidName(std::string_view name) -> bool;
    auto EncodeDelta(const std::string& name, const settings_store& snapshot) -> std::string;
    auto DecodeDelta(std::string_view data) -> std::optional<shared_profile>;
}
