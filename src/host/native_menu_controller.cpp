#include "native_menu_controller.hpp"

#include "Ashita.h"
#include "Memory.h"
#include "native_pattern.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
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

    template<typename T>
    auto Write(const uintptr_t address, const T& value) noexcept -> bool
    {
        __try { std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(T)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto Reposition(const uintptr_t method, const uintptr_t primitive, const int x, const int y) noexcept -> bool
    {
        using position_fn = void(__thiscall*)(void*, int, int);
        __try { reinterpret_cast<position_fn>(method)(reinterpret_cast<void*>(primitive), x, y); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto DrawPrimitive(const uintptr_t method, const uintptr_t primitive) noexcept -> bool
    {
        using draw_fn = void(__thiscall*)(void*);
        __try { reinterpret_cast<draw_fn>(method)(reinterpret_cast<void*>(primitive)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto NativeResource(const uintptr_t method, const uintptr_t manager, const char* name) noexcept -> uintptr_t
    {
        using find_fn = uintptr_t(__thiscall*)(void*, const char*);
        __try { return reinterpret_cast<find_fn>(method)(reinterpret_cast<void*>(manager), name); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    auto BindPrimitive(const uintptr_t method, void* primitive, const uintptr_t resource) noexcept -> bool
    {
        using bind_fn = void(__thiscall*)(void*, uintptr_t);
        __try { reinterpret_cast<bind_fn>(method)(primitive, resource); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto NativeFrame(const uintptr_t method, const uintptr_t primitive, std::array<int16_t, 4>& frame) noexcept -> bool
    {
        using frame_fn = void(__thiscall*)(void*, int16_t*);
        __try { reinterpret_cast<frame_fn>(method)(reinterpret_cast<void*>(primitive), frame.data()); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto StatusBounds(const uintptr_t primitive, const uintptr_t owner, std::array<int16_t, 4>& bounds) -> bool
    {
        int first{}, count{}, capacity{}, componentFirst{};
        if (!Read(owner + 0x38, first) || !Read(owner + 0x40, count) || !Read(owner + 0x48, capacity) || !Read(owner + 0x54, componentFirst) ||
            first < 0 || count <= first || capacity <= 0 || capacity > 64 || componentFirst < 0 || componentFirst > 128) return false;
        count = std::min(count - first, capacity);
        uintptr_t node{};
        if (!Read(primitive + 0x14, node)) return false;
        bool found{};
        for (size_t at = 0; node != 0 && at < 128; ++at)
        {
            uintptr_t next{}, component{};
            uint8_t removed{}; uint16_t kind{}; int16_t index{};
            std::array<int16_t, 4> rect{};
            if (!Read(node, next) || !Read(node + 0x10, component) || !Read(node + 0x14, removed)) break;
            node = next;
            if (removed || !Read(component, kind) || kind != 1 || !Read(component + 0x3A, index) ||
                index < componentFirst || index >= componentFirst + count || !Read(component + 0x2A, rect) || rect[2] <= 0 || rect[3] <= 0) continue;
            // The native status renderer draws each icon one pixel inside its DAT component.
            const std::array<int16_t, 4> icon{static_cast<int16_t>(rect[0] + 1), static_cast<int16_t>(rect[1] + 1),
                static_cast<int16_t>(rect[0] + rect[2] + 1), static_cast<int16_t>(rect[1] + rect[3] + 1)};
            if (!found) bounds = icon;
            else { bounds[0] = std::min(bounds[0], icon[0]); bounds[1] = std::min(bounds[1], icon[1]); bounds[2] = std::max(bounds[2], icon[2]); bounds[3] = std::max(bounds[3], icon[3]); }
            found = true;
        }
        return found;
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
        return id == "iteminfo" || id == "itemxinf" || id == "itemyinf" || id == "money   ";
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
        this->primitive_create_ = this->Find("53568BF1E8????????8D4E14E8????????33DB5666895E3A66895E3C66895E3E66895E40");
        this->primitive_destroy_ = this->Find("51558BE956578D7D148BCFE8????????33F68944240C3BC674");
        this->primitive_bind_ = this->Find("5355568BF18B4C241057894E04E8????????8BD885DB0F84");
        this->primitive_select_ = this->Find("51568BF1B8010000008B4E3489442404F7C10000010075??8B4E0485C974");
        this->resource_find_ = this->Find("515356578D791C8BCFE8????????85C08944240C74??8B5C24148D44240C8BCF50E8");
        this->main_draw_ = this->Find("83EC0853568BF16A016A018D4424108B4E0833DB6A0C5066895C241866895C241A66895C241C66895C241E");
        this->equipment_draw_ = this->Find("83EC1055568BF15733C0BD010000008B7E08668944241466894424166689442418668944241A66396F58");
        this->frame_read_ = this->Find("83EC0C568BF18A869900000084C00F84????????8D442408578D4C24085051B9");
        this->cached_frame_init_[0] = this->Find("83EC08568BF15733FF397E207D??8B4E088D4424085066897C240C66897C240E");
        this->cached_frame_init_[1] = this->Find("566A028BF1E8????????83C40485C074??B9280100008B1083C10481F934010000");
        this->cached_frame_init_[2] = this->Find("566A028BF1E8????????83C404B9040300008B1083C10481F944030000");
        this->currency_draw_ = this->Find("83EC18568BF18A86880000003C0175??E8????????5E83C418C33C0275");
        this->state_.menu_available = this->manager_ >= module && this->manager_ - module + 0x84 <= size && this->reposition_ != 0;
        this->main_record_ = this->Find("6D656E75202020206D656E7577696E6400000000000000000000000000000000");
        for (size_t at = 0; this->main_record_ != 0 && at < 512; ++at)
        {
            const auto record = this->main_record_ + at * 44;
            std::array<char, 16> name{}; uintptr_t slot{};
            if (record < module || record - module + 44 > size || !Read(record, name) || std::string_view{name.data(), 8} != "menu    ") break;
            const auto index = NativeMenuWindowIndex(std::string_view{name.data() + 8, 8});
            if (index < this->dialog_slots_.size() && this->dialog_slots_[index] == 0 && Read(record + 0x20, slot) && slot >= module && slot - module + 4 <= size)
                this->dialog_slots_[index] = slot;
        }
    }

    auto native_menu_controller::IsAttached(const menu& value) const -> bool
    {
        uintptr_t resource{}, owner{}, attached{};
        if (!Read(value.primitive + 4, resource) || resource != value.resource || !Read(value.primitive + 0x0C, owner) ||
            owner != value.owner || !Read(owner + 8, attached) || attached != value.primitive) return false;
        if (value.window >= native_window_count) return true;
        std::array<char, 16> name{};
        return Read(resource + 0x46, name) && this->WindowIndex(owner, name, native_window_count) == value.window;
    }

    auto native_menu_controller::ObservePosition(menu& value, const std::array<int16_t, 2>& current) -> void
    {
        if (value.moved)
            for (size_t axis = 0; axis < current.size(); ++axis)
                if (current[axis] != value.applied[axis]) value.original[axis] = current[axis];
    }

    auto native_menu_controller::Move(menu& value, const bool enabled) -> void
    {
        if (!this->IsAttached(value)) return;
        std::array<int16_t, 2> current{};
        if (!Read(value.primitive + 0x52, current)) return;
        if (!value.moved) value.original = current;
        this->ObservePosition(value, current);
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

        this->UpdateWindows();
        this->prepare_pending_ = true;
        this->UpdateMenus();
    }

    auto native_menu_controller::UpdateMenus(void) -> void
    {
        const auto& layout = this->layout_;
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
            const auto independentlyPlaced = std::find_if(this->windows_.begin(), this->windows_.begin() + this->window_count_,
                [&](const auto& value) { return value.primitive == primitive && layout.windows[value.window].enabled; });
            if (independentlyPlaced != this->windows_.begin() + this->window_count_ || Excluded(name) || (!DetailPanel(name) && !MovableFrame(primitive, name)) ||
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

    auto native_menu_controller::WindowIndex(const uintptr_t owner, const std::array<char, 16>& name, const size_t detailOwner) const -> size_t
    {
        for (size_t index = 0; index < this->hud_slots_.size(); ++index)
        {
            uintptr_t current{};
            if (Read(this->hud_slots_[index], current) && current == owner) return index;
        }
        if (std::string_view{name.data(), 8} != "menu    ") return native_window_count;
        for (size_t index = 0; index < this->hud_slots_.size(); ++index)
            if (native_hud_options[index].resource != nullptr && std::string_view{name.data() + 8, 8} == native_hud_options[index].resource) return index;
        const std::string_view id{name.data() + 8, 8};
        if (id == "inline  ") return static_cast<size_t>(native_hud_element::chat1);
        if (id == "evitem  " && this->currency_draw_ != 0)
        {
            uintptr_t table{}, draw{}; uint8_t page{};
            if (Read(owner, table) && Read(table + 0x10, draw) && draw == this->currency_draw_ && Read(owner + 0x88, page) && (page == 1 || page == 2))
                return native_hud_window_count + NativeMenuWindowIndex(id, page);
        }
        const auto index = NativeMenuWindowIndex(id);
        return index < std::size(native_menu_windows) ? native_hud_window_count + index : native_window_count;
    }

    auto native_menu_controller::MoveWindow(menu& value, const bool restore) -> void
    {
        if (!this->IsAttached(value)) return;
        std::array<int16_t, 2> current{};
        if (!Read(value.primitive + 0x52, current)) return;
        this->ObservePosition(value, current);
        auto placement = this->layout_.windows[value.window];
        if (restore) placement.enabled = false;
        auto target = value.original;
        if (placement.enabled)
        {
            if (!this->state_.windows[value.window].live) return;
            const float scaleX = this->layout_.viewport_width > 0 && this->state_.reference_width > 0 ? this->state_.reference_width / this->layout_.viewport_width : 1;
            const float scaleY = this->layout_.viewport_height > 0 && this->state_.reference_height > 0 ? this->state_.reference_height / this->layout_.viewport_height : 1;
            const std::array<float, 2> position{placement.x * scaleX, placement.y * scaleY};
            for (size_t axis = 0; axis < target.size(); ++axis)
            {
                if (!std::isfinite(position[axis])) return;
                target[axis] = static_cast<int16_t>(std::lround(std::clamp(position[axis] + value.original[axis] - this->origins_[value.window][axis], -16000.0f, 16000.0f)));
            }
        }
        if (current != target && !Reposition(this->reposition_, value.primitive, target[0], target[1])) return;
        this->UpdateOwnedContent(value);
        value.applied = target; value.moved = placement.enabled;
    }

    auto native_menu_controller::UpdateOwnedContent(const menu& value) -> void
    {
        uintptr_t table{}, draw{}, initialize{};
        std::array<int16_t, 2> position{};
        if (!this->IsAttached(value) || !Read(value.owner, table) ||
            !Read(table + 0x10, draw) || !Read(value.primitive + 0x52, position)) return;
        size_t cachedOffset{};
        if (this->frame_read_ != 0 && Read(table + 4, initialize))
        {
            if (initialize != 0 && std::find(this->cached_frame_init_.begin(), this->cached_frame_init_.end(), initialize) != this->cached_frame_init_.end()) cachedOffset = 0x14;
            else if (initialize >= this->module_ && initialize - this->module_ + 13 <= this->module_size_)
            {
                std::array<uint8_t, 13> code{}; int32_t relative{};
                if (Read(initialize, code) && code[0] == 0x8D && code[1] == 0x41 && code[3] == 0x8B && code[4] == 0x49 &&
                    code[5] == 8 && code[6] == 0x50 && code[7] == 0xE8 && code[12] == 0xC3 && code[2] >= 0x14 && code[2] <= 0x40)
                {
                    std::memcpy(&relative, code.data() + 8, 4);
                    if (initialize + 12 + relative == this->frame_read_) cachedOffset = code[2];
                }
            }
            if (cachedOffset != 0)
            {
                std::array<int16_t, 4> frame{};
                if (NativeFrame(this->frame_read_, value.primitive, frame)) Write(value.owner + cachedOffset, frame);
            }
        }
        if (this->equipment_draw_ == 0 || draw != this->equipment_draw_) return;
        // Equipment's ten native numeric labels keep absolute coordinates outside the DAT components.
        for (size_t index = 0; index < 10; ++index)
        {
            const auto text = value.owner + 0x88 + index * 0x20;
            std::array<int16_t, 2> offset{};
            if (!Read(text + 8, offset)) return;
            for (size_t axis = 0; axis < 2; ++axis) offset[axis] = static_cast<int16_t>(position[axis] + offset[axis]);
            if (!Write(text + 4, offset)) return;
        }
    }

    auto native_menu_controller::UpdateWindows(void) -> void
    {
        this->UpdateEditorMain();
        const auto drawn = this->drawn_;
        this->drawn_ = {};
        this->last_submitted_ = this->submitted_; this->submitted_ = {};
        this->replay_pending_ = this->editing_ && this->draw_method_ != 0;
        this->RefreshWindows(drawn);
    }

    auto native_menu_controller::ReadWindow(const uintptr_t primitive, menu& value) const -> bool
    {
        uintptr_t resource{}, owner{}, attached{};
        std::array<char, 16> name{};
        std::array<int16_t, 4> frame{};
        std::array<int16_t, 2> position{};
        uint8_t closed{};
        if (!Read(primitive + 4, resource) || resource == 0 || !Read(resource + 0x46, name) ||
            !Read(primitive + 0xC, owner) || owner == 0 || !Read(owner + 8, attached) || attached != primitive ||
            !Read(primitive + 0x70, closed) || !Read(primitive + 0x3A, frame) || frame[2] <= frame[0] || frame[3] <= frame[1] ||
            !Read(primitive + 0x52, position)) return false;
        const auto index = this->WindowIndex(owner, name, native_window_count);
        if (index >= native_window_count) return false;
        value = {primitive, owner, resource, position, position, false, DetailPanel(name), index, !closed};
        return true;
    }

    auto native_menu_controller::RefreshWindows(const std::array<bool, native_window_count>& drawn, const uintptr_t pending) -> void
    {
        uintptr_t node{};
        if (!Read(this->manager_, node)) return;
        std::array<int16_t, 2> resolution{};
        if (Read(this->manager_ + 0x80, resolution) && resolution[0] >= 320 && resolution[1] >= 240)
        {
            this->state_.reference_width = static_cast<float>(resolution[0]);
            this->state_.reference_height = static_cast<float>(resolution[1]);
        }
        const auto previous = this->windows_;
        const size_t previousCount = this->window_count_;
        this->window_count_ = 0; this->state_.windows = {};
        for (size_t index = 0; index < native_window_count; ++index)
        {
            this->origins_[index] = {32000, 32000};
            this->state_.windows[index].movable = index >= native_hud_window_count || this->hud_slots_[index] != 0;
        }
        size_t detailOwner = native_hud_window_count + static_cast<size_t>(native_dialog::details);
        const auto keep = [&](const uintptr_t primitive, const uintptr_t owner, const uintptr_t resource, const size_t index, const bool detail, const bool open) {
            if (this->window_count_ == this->windows_.size() ||
                std::any_of(this->windows_.begin(), this->windows_.begin() + this->window_count_, [&](const auto& value) { return value.primitive == primitive; })) return;
            std::array<int16_t, 2> current{};
            if (!Read(primitive + 0x52, current)) return;
            auto& value = this->windows_[this->window_count_++];
            value = {primitive, owner, resource, current, current, false, detail, index, open};
            for (size_t old = 0; old < previousCount; ++old)
                if (previous[old].primitive == primitive && previous[old].owner == owner && previous[old].resource == resource)
                {
                    if (previous[old].window == index) { value = previous[old]; value.open = open; }
                    else if (previous[old].moved && current == previous[old].applied &&
                        Reposition(this->reposition_, primitive, previous[old].original[0], previous[old].original[1]))
                    {
                        value.original = value.applied = current = previous[old].original;
                        this->UpdateOwnedContent(value);
                    }
                    break;
                }
            // Capture native resets before computing the shared window origin.
            this->ObservePosition(value, current);
            if (!value.moved)
            {
                value.original = current;
                for (size_t old = 0; old < this->count_; ++old)
                    if (this->menus_[old].primitive == primitive && this->menus_[old].owner == owner && this->menus_[old].moved)
                        value.original = this->menus_[old].original;
            }
        };
        std::array<uintptr_t, 256> visited{};
        size_t visitedCount{};
        while (node != 0 && visitedCount < visited.size() && this->window_count_ < this->windows_.size())
        {
            if (std::find(visited.begin(), visited.begin() + visitedCount, node) != visited.begin() + visitedCount) break;
            visited[visitedCount++] = node;
            uintptr_t next{}, primitive{}, resource{}, owner{}, attached{};
            uint8_t removed{}, closed{};
            if (!Read(node, next) || !Read(node + 0x10, primitive) || !Read(node + 0x14, removed)) break;
            node = next;
            std::array<char, 16> name{};
            std::array<int16_t, 4> frame{};
            std::array<int16_t, 2> current{};
            if (removed || !Read(primitive + 0x70, closed) || !Read(primitive + 4, resource) ||
                !Read(resource + 0x46, name) || !Read(primitive + 0xC, owner) || !Read(owner + 8, attached) || attached != primitive ||
                !Read(primitive + 0x3A, frame) || frame[2] <= frame[0] || frame[3] <= frame[1] || !Read(primitive + 0x52, current)) continue;
            if (closed)
            {
                for (size_t old = 0; old < previousCount; ++old)
                    if (previous[old].moved && previous[old].primitive == primitive && previous[old].owner == owner && previous[old].resource == resource)
                    { auto closing = previous[old]; this->MoveWindow(closing, true); break; }
                continue;
            }
            const size_t index = this->WindowIndex(owner, name, detailOwner);
            if (index >= native_window_count) continue;
            if (NativeDetailOwner(name) && !DetailPanel(name)) detailOwner = index;
            keep(primitive, owner, resource, index, DetailPanel(name), true);
        }
        // HUD and closed menu owners need not belong to the active menu list.
        for (size_t index = 0; index < native_window_count; ++index)
        {
            const auto slot = index < native_hud_window_count ? this->hud_slots_[index] : this->dialog_slots_[index - native_hud_window_count];
            uintptr_t owner{}, primitive{}, attached{}, resource{};
            uint8_t closed{};
            std::array<int16_t, 4> frame{};
            if (!Read(slot, owner) || owner == 0 || !Read(owner + 8, primitive) || primitive == 0 ||
                !Read(primitive + 0xC, attached) || attached != owner || !Read(primitive + 4, resource) || resource == 0 ||
                !Read(primitive + 0x70, closed) || !Read(primitive + 0x3A, frame) || frame[2] <= frame[0] || frame[3] <= frame[1]) continue;
            if (std::any_of(this->windows_.begin(), this->windows_.begin() + this->window_count_, [&](const auto& value) { return value.primitive == primitive; })) continue;
            std::array<char, 16> name{};
            if (!Read(resource + 0x46, name)) continue;
            const auto resolved = this->WindowIndex(owner, name, detailOwner);
            if (resolved < native_window_count) keep(primitive, owner, resource, resolved, false, !closed);
        }
        if (this->editor_main_created_)
        {
            uintptr_t resource{};
            const auto primitive = reinterpret_cast<uintptr_t>(this->editor_main_.data());
            if (Read(primitive + 4, resource) && resource != 0)
                keep(primitive, reinterpret_cast<uintptr_t>(this->editor_main_owner_.data()), resource,
                    native_hud_window_count + static_cast<size_t>(native_dialog::main_menu), false, true);
        }
        menu submitted{};
        if (pending != 0 && this->ReadWindow(pending, submitted))
            keep(submitted.primitive, submitted.owner, submitted.resource, submitted.window, submitted.detail, submitted.open);
        for (size_t at = 0; at < this->window_count_; ++at)
        {
            auto& value = this->windows_[at];
            auto& state = this->state_.windows[value.window];
            std::array<int16_t, 4> frame{};
            if (!Read(value.primitive + 0x3A, frame)) continue;
            if (value.window == static_cast<size_t>(native_hud_element::status) && !StatusBounds(value.primitive, value.owner, frame)) continue;
            std::array<int16_t, 2> position{};
            if (!Read(value.primitive + 0x52, position)) continue;
            const bool custom = this->layout_.windows[value.window].enabled;
            const float left = static_cast<float>(frame[0] + (custom ? value.original[0] - position[0] : 0));
            const float top = static_cast<float>(frame[1] + (custom ? value.original[1] - position[1] : 0));
            const float right = left + frame[2] - frame[0], bottom = top + frame[3] - frame[1];
            if (!state.live) { state.x = left; state.y = top; state.width = right - left; state.height = bottom - top; }
            else
            {
                const float oldRight = state.x + state.width, oldBottom = state.y + state.height;
                state.x = std::min(state.x, left); state.y = std::min(state.y, top);
                state.width = std::max(oldRight, right) - state.x; state.height = std::max(oldBottom, bottom) - state.y;
            }
            state.live = state.movable = true;
            state.visible |= drawn[value.window];
            this->origins_[value.window][0] = std::min(this->origins_[value.window][0], static_cast<int>(left));
            this->origins_[value.window][1] = std::min(this->origins_[value.window][1], static_cast<int>(top));
        }
        for (size_t at = 0; at < this->window_count_; ++at)
            if ((this->windows_[at].open && this->layout_.windows[this->windows_[at].window].enabled) || this->windows_[at].moved)
                this->MoveWindow(this->windows_[at], !this->windows_[at].open);
    }

    auto native_menu_controller::DestroyEditorMain(void) -> void
    {
        if (this->editor_main_created_ && this->primitive_destroy_ != 0)
            DrawPrimitive(this->primitive_destroy_, reinterpret_cast<uintptr_t>(this->editor_main_.data()));
        this->editor_main_created_ = false; this->editor_main_ = {}; this->editor_main_owner_ = {};
    }

    auto native_menu_controller::UpdateEditorMain(void) -> void
    {
        uintptr_t owner{}, attached{}, table{}, draw{};
        const bool supported = this->editing_ && this->primitive_create_ && this->primitive_destroy_ && this->primitive_bind_ && this->primitive_select_ &&
            this->resource_find_ && this->main_draw_ && this->main_record_ &&
            Read(this->dialog_slots_[static_cast<size_t>(native_dialog::main_menu)], owner) && Read(owner + 8, attached) &&
            Read(owner, table) && Read(table + 0x10, draw) && draw == this->main_draw_;
        uint32_t phase{};
        if (!supported || (attached != 0 && (!Read(attached + 0x10, phase) || phase != 0x0E))) { this->DestroyEditorMain(); return; }
        if (!this->editor_main_created_)
        {
            const auto resource = NativeResource(this->resource_find_, this->manager_, "menu    menuwind");
            if (resource == 0 || !DrawPrimitive(this->primitive_create_, reinterpret_cast<uintptr_t>(this->editor_main_.data()))) return;
            this->editor_main_created_ = true;
            uint32_t flags{}; uint8_t layer{}, kind{}; uint16_t group{};
            if (!Read(this->main_record_ + 0x24, flags) || !Read(this->main_record_ + 0x28, layer) ||
                !Read(this->main_record_ + 0x29, kind) || !Read(this->main_record_ + 0x2A, group)) { this->DestroyEditorMain(); return; }
            flags |= 0x40; // Allow the isolated artwork through the client's modal draw filter.
            std::memcpy(this->editor_main_.data() + 0x34, &flags, 4); this->editor_main_[0x30] = layer; this->editor_main_[0x60] = kind;
            std::memcpy(this->editor_main_.data() + 0x38, &group, 2); this->editor_main_[0x99] = (flags & 0x100000) != 0;
            if (!BindPrimitive(this->primitive_bind_, this->editor_main_.data(), resource)) { this->DestroyEditorMain(); return; }
            if (!DrawPrimitive(this->primitive_select_, reinterpret_cast<uintptr_t>(this->editor_main_.data()))) { this->DestroyEditorMain(); return; }
        }
        // The client owns the artwork and draw callback; this isolated DAT instance never joins its input manager.
        if (!Read(owner, this->editor_main_owner_)) { this->DestroyEditorMain(); return; }
        const auto primitive = reinterpret_cast<uintptr_t>(this->editor_main_.data());
        const auto editorOwner = reinterpret_cast<uintptr_t>(this->editor_main_owner_.data());
        std::memcpy(this->editor_main_owner_.data() + 8, &primitive, 4);
        std::memcpy(this->editor_main_.data() + 0xC, &editorOwner, 4);
    }

    auto native_menu_controller::BeforeDraw(const uintptr_t primitive) -> void
    {
        if (this->state_.menu_available && !this->replaying_)
        {
            bool refresh = this->prepare_pending_;
            if (!refresh)
            {
                const auto end = this->windows_.begin() + this->window_count_;
                const auto known = std::find_if(this->windows_.begin(), end, [&](const auto& value) { return value.primitive == primitive && this->IsAttached(value); });
                if (known == end)
                {
                    menu submitted{};
                    refresh = this->ReadWindow(primitive, submitted);
                }
                else
                {
                    std::array<int16_t, 2> position{};
                    uint8_t closed{};
                    refresh = !this->state_.windows[known->window].live ||
                        (Read(primitive + 0x70, closed) && !closed && !known->open) ||
                        (known->moved && Read(primitive + 0x52, position) && position != known->applied);
                }
            }
            if (refresh)
            {
                this->prepare_pending_ = false;
                auto drawn = this->drawn_;
                for (size_t index = 0; index < drawn.size(); ++index) drawn[index] |= this->state_.windows[index].visible;
                this->RefreshWindows(drawn, primitive);
                this->UpdateMenus();
            }
        }
        this->DrawInactive(primitive);
        for (size_t index = 0; index < this->window_count_; ++index)
            if (this->windows_[index].primitive == primitive && this->IsAttached(this->windows_[index]))
            {
                this->drawn_[this->windows_[index].window] = true;
                if (!this->replaying_) this->submitted_[this->windows_[index].window] = true;
                if (this->layout_.windows[this->windows_[index].window].enabled || this->windows_[index].moved)
                { this->MoveWindow(this->windows_[index]); return; }
                break;
            }
        for (size_t i = 0; i < this->count_; ++i)
            if (this->menus_[i].primitive == primitive) { this->Move(this->menus_[i], this->layout_.enabled); break; }
    }

    auto native_menu_controller::DrawInactive(const uintptr_t current) -> void
    {
        if (!this->editing_ || !this->replay_pending_ || this->replaying_ || this->draw_method_ == 0) return;
        this->replay_pending_ = false; this->replaying_ = true;
        // Submit existing client primitives during the native render pass, without opening input menus.
        for (size_t index = 0; index < this->window_count_; ++index)
        {
            auto& value = this->windows_[index];
            const bool editorMain = this->editor_main_created_ && value.primitive == reinterpret_cast<uintptr_t>(this->editor_main_.data());
            if (value.primitive == current || (!editorMain && this->last_submitted_[value.window]) || !this->IsAttached(value) ||
                !this->state_.windows[value.window].live) continue;
            DrawPrimitive(this->draw_method_, value.primitive);
        }
        this->replaying_ = false;
    }

    auto native_menu_controller::ShouldHide(const uintptr_t primitive) const -> bool
    {
        for (size_t i = 0; i < this->window_count_; ++i)
            if (this->windows_[i].primitive == primitive && this->IsAttached(this->windows_[i]) && this->layout_.windows[this->windows_[i].window].hidden) return true;
        if (this->editing_ || !this->layout_.enabled || this->comment_draw_ == 0) return false;
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
        this->editing_ = false;
        // Restore only panels still present in the live list, never cached freed objects.
        this->Update({});
        this->count_ = 0; this->module_ = this->manager_ = this->config_slot_ = this->reposition_ = this->comment_draw_ = this->draw_method_ = 0;
        this->window_count_ = 0; this->windows_ = {}; this->hud_slots_ = {}; this->dialog_slots_ = {}; this->drawn_ = {};
        this->module_size_ = 0; this->state_ = {}; this->layout_ = {};
        this->editing_ = this->replay_pending_ = this->replaying_ = this->prepare_pending_ = false; this->submitted_ = {}; this->last_submitted_ = {};
        this->DestroyEditorMain();
        this->primitive_create_ = this->primitive_destroy_ = this->primitive_bind_ = this->primitive_select_ = this->resource_find_ = this->main_draw_ = this->main_record_ = 0;
        this->equipment_draw_ = this->frame_read_ = this->currency_draw_ = 0; this->cached_frame_init_ = {};
    }
}
