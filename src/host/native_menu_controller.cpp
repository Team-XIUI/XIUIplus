#include "native_menu_controller.hpp"

#include "Ashita.h"
#include "Memory.h"
#include "native_pattern.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string_view>

namespace
{
    template<typename T>
    auto Read(const uintptr_t address, T& value) noexcept -> bool
    {
        if (address == 0) return false;
        __try { std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto Reposition(const uintptr_t method, const uintptr_t primitive, const int x, const int y) noexcept -> bool
    {
        using position_fn = void(__thiscall*)(void*, int, int);
        __try { reinterpret_cast<position_fn>(method)(reinterpret_cast<void*>(primitive), x, y); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto NativeDetailOwner(const std::array<char, 16>& name) -> bool
    {
        constexpr std::string_view menus[]{"shop    ", "shopmain", "shopbuy ", "shopsell", "equip   ", "inspect ", "comment "};
        const std::string_view id{name.data() + 8, 8};
        return std::find(std::begin(menus), std::end(menus), id) != std::end(menus);
    }

    auto DetailPanel(const std::array<char, 16>& name) -> bool
    {
        const std::string_view id{name.data() + 8, 8};
        return id == "iteminfo" || id == "money   ";
    }

    auto MovableFrame(const uintptr_t primitive, const std::array<char, 16>& name) -> bool
    {
        constexpr std::string_view commandLists[]{"playermo", "magic   ", "ability ", "magselec", "abiselec", "mount   ", "shopmain", "shopsell", "comment "};
        const std::string_view id{name.data() + 8, 8};
        const bool commandList = std::find(std::begin(commandLists), std::end(commandLists), id) != std::end(commandLists);
        uintptr_t node{};
        if (!Read(primitive + 0x14, node)) return false;
        for (unsigned i = 0; node != 0 && i < 128; ++i)
        {
            uintptr_t next{}, component{}, definition{};
            uint16_t kind{}; uint8_t removed{}, anchor{};
            if (!Read(node, next) || !Read(node + 0x10, component) || !Read(node + 0x14, removed)) return false;
            node = next;
            if (removed || !Read(component, kind) || kind != 0) continue;
            return Read(component + 0x30, definition) && Read(definition, definition) && Read(definition + 0x13, anchor) &&
                anchor <= 3 && (anchor == 2 || commandList);
        }
        return false;
    }

    auto Excluded(const std::array<char, 16>& name) -> bool
    {
        constexpr std::string_view excluded[]{"logwindo", "logwin2 ", "log1win ", "log2win ", "inline  ", "fulllog ",
            "gaugewin", "partywin", "targetwi", "casttime", "buff    ", "netstat ", "titlewin", "helpwind",
            "shop    ", "shopbuy ", "equip   ", "inspect "};
        if (std::string_view{name.data(), 8} != "menu    ") return true;
        const std::string_view id{name.data() + 8, 8};
        return std::find(std::begin(excluded), std::end(excluded), id) != std::end(excluded);
    }
}

namespace xiui
{
    auto native_menu_controller::Find(const char* pattern) const -> uintptr_t
    {
        if (this->module_ == 0) return 0;
        return FindUniquePattern(this->module_, this->module_size_, pattern);
    }

    auto native_menu_controller::Initialize(const uintptr_t module, const size_t size) -> void
    {
        this->Shutdown();
        this->module_ = module; this->module_size_ = size;
        const auto list = this->Find("32DBB9????????E8????????85C08944240C74??8D44240CB9????????50E8????????8BF0");
        if (list != 0) Read(list + 3, this->manager_);
        const auto config = this->Find("8B0D????????85C974108B4424088B5424045052E8????????C383C8FFC3");
        if (config != 0) Read(config + 2, this->config_slot_);
        this->reposition_ = this->Find("83EC0C668B44241453558B6C2418568BF15733FF8D5E1466896E528BCB66894654897C2414897C2418E8");
        this->comment_draw_ = this->Find("81EC140200005355568BF1578D4424148B4E0833FF5066897C241866897C241A66897C241C66897C241E");
        this->state_.menu_available = this->manager_ >= module && this->manager_ - module + 0x84 <= size && this->reposition_ != 0;
    }

    auto native_menu_controller::IsAttached(const menu& value) const -> bool
    {
        uintptr_t resource{}, owner{}, attached{};
        return Read(value.primitive + 4, resource) && resource == value.resource &&
            Read(value.primitive + 0x0C, owner) && owner == value.owner && Read(owner + 8, attached) && attached == value.primitive;
    }

    auto native_menu_controller::Move(menu& value, const bool enabled) -> void
    {
        if (!this->IsAttached(value)) return;
        std::array<int16_t, 2> current{};
        if (!Read(value.primitive + 0x52, current)) return;
        if (!value.moved) value.original = current;
        else
            for (size_t axis = 0; axis < current.size(); ++axis)
                if (current[axis] != value.applied[axis]) value.original[axis] = current[axis];
        auto target = value.original;
        if (enabled)
        {
            std::array<int16_t, 4> frame{};
            int16_t originalLeft{};
            if (!Read(value.primitive + 0x42, originalLeft) || !Read(value.primitive + 0x3A, frame) || frame[2] <= frame[0] || frame[3] <= frame[1]) return;
            // +0x42 retains the creation rectangle; +0x3A tracks live resizing.
            target = {
                static_cast<int16_t>(std::clamp<int>(originalLeft + this->offset_x_, -16000, 16000)),
                static_cast<int16_t>(std::clamp<int>(this->anchor_y_ - (this->layout_.anchor_top ? 0 : frame[3] - frame[1]), -16000, 16000))};
        }
        if (current != target && !Reposition(this->reposition_, value.primitive, target[0], target[1])) return;
        value.applied = target;
        value.moved = enabled;
    }

    auto native_menu_controller::Update(const native_menu_layout& layout) -> void
    {
        this->layout_ = layout;
        uintptr_t config{}, table{};
        std::array<uint32_t, 8> setting{};
        this->state_.window_type_available = Read(this->config_slot_, config) && Read(config + 4, table) &&
            Read(table + 0x12 * 44, setting) && setting[1] == 0x12 && setting[2] < 8 && setting[6] == 0 && setting[7] == 7;
        if (this->state_.window_type_available) this->state_.window_type = static_cast<uint8_t>(setting[2]);
        this->state_.menu_visible = false; this->state_.menu_count = 0;
        if (!this->state_.menu_available) return;

        std::array<int16_t, 2> resolution{};
        if (Read(this->manager_ + 0x80, resolution) && resolution[0] >= 320 && resolution[1] >= 240)
        {
            this->state_.reference_width = static_cast<float>(resolution[0]);
            this->state_.reference_height = static_cast<float>(resolution[1]);
        }

        uintptr_t node{};
        if (!Read(this->manager_, node)) return;
        const auto previous = this->menus_;
        const auto previousCount = this->count_;
        this->count_ = 0;
        std::array<uintptr_t, 128> visited{};
        size_t visitedCount{};
        bool nativeDetails{};
        while (node != 0 && visitedCount < visited.size() && this->count_ < this->menus_.size())
        {
            if (std::find(visited.begin(), visited.begin() + visitedCount, node) != visited.begin() + visitedCount) break;
            visited[visitedCount++] = node;
            uintptr_t next{}, primitive{}, resource{}, owner{}, attached{};
            uint8_t removed{}, closed{};
            if (!Read(node, next) || !Read(node + 0x10, primitive) || !Read(node + 0x14, removed)) break;
            node = next;
            std::array<char, 16> name{};
            std::array<int16_t, 4> frame{}, liveFrame{};
            if (removed || !Read(primitive + 0x70, closed) || closed || !Read(primitive + 4, resource) ||
                !Read(resource + 0x46, name) || !Read(primitive + 0x0C, owner) ||
                !Read(owner + 8, attached) || attached != primitive) continue;
            if (std::string_view{name.data(), 8} == "menu    " && NativeDetailOwner(name)) nativeDetails = true;
            if (Excluded(name) || (!DetailPanel(name) && !MovableFrame(primitive, name)) ||
                !Read(primitive + 0x42, frame) || frame[2] <= frame[0] || frame[3] <= frame[1] ||
                !Read(primitive + 0x3A, liveFrame) || liveFrame[2] <= liveFrame[0] || liveFrame[3] <= liveFrame[1]) continue;
            auto& value = this->menus_[this->count_++];
            value = {primitive, owner, resource};
            for (size_t i = 0; i < previousCount; ++i)
                if (previous[i].primitive == primitive && previous[i].owner == owner && previous[i].resource == resource) { value = previous[i]; break; }
            value.detail = DetailPanel(name);
        }
        size_t kept = 0;
        int left = 32000, top = 32000, right = -32000, bottom = -32000, tallest = 0;
        for (size_t i = 0; i < this->count_; ++i)
        {
            auto value = this->menus_[i];
            if (this->ShouldHide(value.primitive)) { this->Move(value, false); continue; }
            if (value.detail)
            {
                std::array<char, 16> name{};
                if (nativeDetails || !Read(value.resource + 0x46, name) || !MovableFrame(value.primitive, name))
                { this->Move(value, false); continue; }
            }
            std::array<int16_t, 4> frame{}, liveFrame{};
            if (!Read(value.primitive + 0x42, frame) || !Read(value.primitive + 0x3A, liveFrame)) continue;
            this->menus_[kept++] = value;
            left = std::min<int>(left, frame[0]); top = std::min<int>(top, frame[1]);
            right = std::max<int>(right, frame[2]); bottom = std::max<int>(bottom, frame[3]);
            tallest = std::max<int>(tallest, liveFrame[3] - liveFrame[1]);
        }
        this->count_ = kept;
        this->state_.menu_count = static_cast<uint32_t>(this->count_);
        this->state_.menu_visible = nativeDetails || this->count_ != 0;
        if (this->count_ != 0)
        {
            this->state_.x = static_cast<float>(left); this->state_.y = static_cast<float>(layout.anchor_top ? top : bottom - tallest);
            this->state_.width = static_cast<float>(right - left); this->state_.height = static_cast<float>(tallest);
            const float scaleX = layout.viewport_width > 0 && this->state_.reference_width > 0 ? this->state_.reference_width / layout.viewport_width : 1;
            const float scaleY = layout.viewport_height > 0 && this->state_.reference_height > 0 ? this->state_.reference_height / layout.viewport_height : 1;
            this->offset_x_ = std::isfinite(layout.left) ? static_cast<int>(std::lround(std::clamp(layout.left * scaleX - left, -16000.0f, 16000.0f))) : 0;
            const float edge = layout.anchor_top ? layout.top : layout.bottom;
            this->anchor_y_ = std::isfinite(edge) ? static_cast<int>(std::lround(std::clamp(edge * scaleY, -16000.0f, 16000.0f))) : 0;
        }
        for (size_t i = 0; i < this->count_; ++i) this->Move(this->menus_[i], layout.enabled);
    }

    auto native_menu_controller::BeforeDraw(const uintptr_t primitive) -> void
    {
        for (size_t i = 0; i < this->count_; ++i)
            if (this->menus_[i].primitive == primitive) { this->Move(this->menus_[i], this->layout_.enabled); break; }
    }

    auto native_menu_controller::ShouldHide(const uintptr_t primitive) const -> bool
    {
        if (!this->layout_.enabled || this->comment_draw_ == 0) return false;
        uintptr_t resource{}, owner{}, attached{}, vtable{}, draw{};
        std::array<char, 16> name{};
        uint8_t mode{};
        if (!Read(primitive + 4, resource) || !Read(resource + 0x46, name) ||
            std::string_view{name.data(), name.size()} != "menu    comment " ||
            !Read(primitive + 0x0C, owner) || !Read(owner + 8, attached) || attached != primitive ||
            !Read(owner, vtable) || !Read(vtable + 0x10, draw) || draw != this->comment_draw_ ||
            !Read(owner + 0x16, mode) || mode != 1) return false;
        // Mode 1 views another player's three 40-byte lines; other modes are editors.
        std::array<std::array<char, 41>, 3> lines{};
        if (!Read(owner + 0x95, lines)) return false;
        for (const auto& line : lines)
        {
            bool terminated{};
            for (const char c : line)
            {
                if (c == '\0') { terminated = true; break; }
                if (c != ' ' && c != '\t' && c != '\r' && c != '\n') return false;
            }
            if (!terminated) return false;
        }
        return true;
    }

    auto native_menu_controller::Shutdown(void) -> void
    {
        // Restore only panels still present in the live list, never cached freed objects.
        this->Update({});
        this->count_ = 0; this->module_ = this->manager_ = this->config_slot_ = this->reposition_ = this->comment_draw_ = 0;
        this->module_size_ = 0; this->state_ = {}; this->layout_ = {};
    }
}
