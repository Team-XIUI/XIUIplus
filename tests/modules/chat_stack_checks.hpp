#pragma once

#include "chat/chat_stack_layout.hpp"

#include <stdexcept>

inline auto RunChatStackChecks(void) -> void
{
    const xiui::ui::rect lower{18, 400, 560, 260}, upper{18, 108, 560, 260};
    const auto check = [](const bool value) { if (!value) throw std::runtime_error{"shared chat stacking geometry"}; };
    auto offsets = xiui::ChatStackOffsets(lower, upper, 160, 180);
    check(offsets[0] == 0 && offsets[1] == 100);
    check((lower.y + lower.height - 160) - (upper.y + upper.height + offsets[1]) == 32);
    offsets = xiui::ChatStackOffsets(lower, upper, 159.5f, 180);
    check(offsets[0] == 0 && offsets[1] == 100.5f);
    offsets = xiui::ChatStackOffsets(upper, lower, 180, 160);
    check(offsets[0] == 100 && offsets[1] == 0);
    offsets = xiui::ChatStackOffsets(lower, upper, 260, 260);
    check(offsets[0] == 0 && offsets[1] == 0);
    offsets = xiui::ChatStackOffsets(lower, {600, 108, 560, 260}, 160, 180);
    check(offsets[0] == 0 && offsets[1] == 0);
    offsets = xiui::ChatStackOffsets(lower, {18, 350, 560, 260}, 160, 180);
    check(offsets[0] == 0 && offsets[1] == 0);
    offsets = xiui::ChatStackOffsets(lower, upper, 400, 180);
    check(offsets[0] == 0 && offsets[1] == 0);
    offsets = xiui::ChatStackOffsets(lower, upper, -20, 180);
    check(offsets[0] == 0 && offsets[1] == 260);
}
