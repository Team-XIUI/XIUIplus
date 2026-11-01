#pragma once

#include "chat_model.hpp"

namespace xiui
{
    auto EmphasizeChatText(std::vector<chat_span>& spans, uint32_t mode) -> void;
}
