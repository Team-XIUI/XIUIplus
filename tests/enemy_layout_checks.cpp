#include "modules/enemylist/enemy_list_module.hpp"

#include <cmath>
#include <cstdio>

namespace xiui
{
    struct enemy_list_layout_test
    {
        static auto Check(const bool value, const char* message) -> bool
        { if (!value) std::printf("FAIL: %s\n", message); return value; }

        static auto Run(void) -> bool
        {
            const auto now = ::GetTickCount64();
            enemy_list_module module;
            module.ApplyTextDefaults(ui::theme::CreateDefault());
            module.state_.bounds = {30, 40, 220, 0};
            module.max_rows_ = 2; module.max_columns_ = 2; module.column_gap_ = 17; module.gap_ = 9;
            for (uint32_t index = 0; index < 4; ++index)
            {
                enemy_entry entry{};
                entry.id = 100 + index; entry.index = 20 + index; entry.name = "Enemy"; entry.target_name = "Player";
                module.source_.entries_.push_back(std::move(entry));
            }
            auto& casting = module.source_.entries_[1];
            casting.cast_name = "Stone"; casting.cast_started = now; casting.cast_seconds = 4;
            combat_source combat;
            combat.ApplyStatus(103, 2, 0, 236, now);
            module.SetCombat(&combat);
            const auto layout = module.LayoutEntries(module.source_.Entries(), now);
            if (!Check(layout.count == 4 && layout.entries[0].column == 0 && layout.entries[2].column == 1 &&
                layout.entries[1].bounds.height > layout.entries[0].bounds.height &&
                layout.entries[3].bounds.height > layout.entries[2].bounds.height &&
                layout.entries[1].bounds.y >= layout.entries[0].bounds.y + layout.entries[0].bounds.height + module.gap_,
                "enemy columns preserve unequal cast heights and configured gaps")) return false;
            for (size_t index = 0; index < layout.count; ++index)
            {
                const auto& row = layout.entries[index];
                if (!Check(module.HitTarget(row.bounds.x + 1, row.bounds.y + 1, now).first == 100 + index,
                    "enemy pointer targeting matches each measured card")) return false;
            }
            if (!Check(module.HitTarget(layout.entries[0].bounds.x + 1,
                layout.entries[0].bounds.y + layout.entries[0].bounds.height + 1, now).first == 0 &&
                module.HitTarget(layout.entries[0].bounds.x + layout.entries[0].bounds.width + 1,
                    layout.entries[0].bounds.y + 1, now).first == 0,
                "enemy row and column gaps are not targets")) return false;
            if (!Check(module.BeginControllerBrowse(103, 23) && module.BeginControllerStatus() && module.ControllerStatusBounds().width > 0,
                "enemy status inspection starts only on a visible effect")) return false;
            combat.ApplyStatus(103, 10, 0, 236, now);
            if (!Check(module.MoveControllerStatus(1) && module.controller_status_id_ == 10,
                "enemy status inspection cycles actual visible effects")) return false;
            combat.ApplyStatus(103, 2, 0, 206, now);
            if (!Check(module.ControllerStatusBounds().width > 0 && module.controller_status_id_ == 10,
                "enemy inspected effect identity survives another effect's removal")) return false;
            combat.ApplyStatus(103, 10, 0, 206, now);
            if (!Check(module.ControllerStatusBounds().width == 0 && !module.MoveControllerStatus(1) && !module.BeginControllerStatus(),
                "removed enemy effects do not redirect inspection to a different icon")) return false;
            if (!Check(module.BeginControllerBrowse(100, 20) && module.MoveControllerBrowseHorizontal(1) &&
                module.ControllerMember()->id == 102 && module.ControllerBounds(now).x == layout.entries[2].bounds.x,
                "enemy controller navigation follows the adjacent visible column")) return false;
            std::swap(module.source_.entries_[0], module.source_.entries_[2]);
            if (!Check(module.ControllerMember()->id == 102 && module.ControllerBounds(now).x == layout.entries[0].bounds.x,
                "enemy controller focus keeps entity identity after grid reorder")) return false;
            module.target_panel_ = true; module.target_offset_y_ = -20;
            const auto panels = module.LayoutEntries(module.source_.Entries(), now);
            const auto& first = panels.entries[0];
            if (!Check(first.target.y == panels.bounds.y && first.bounds.y >= first.target.y + 20 &&
                panels.bounds.width > layout.bounds.width && module.HitTarget(first.target.x + 1, first.target.y + 1, now).first == 102,
                "enemy target panels contribute to row height, column width and pointer bounds")) return false;
            module.click_target_ = false;
            if (!Check(module.HitTarget(first.bounds.x + 1, first.bounds.y + 1, now).first == 0 && module.ControllerMember()->id == 102,
                "enemy click preference preserves controller targeting")) return false;

            enemy_list_module old;
            old.ApplyTextDefaults(ui::theme::CreateDefault());
            const auto settings = settings_store::FromIniText("[enemylist]\nmax_rows=3\nspacing=11\n[layout.enemylist]\nx=25\ny=35\nwidth=240\nheight=100\n");
            old.Load(settings);
            if (!Check(old.max_rows_ == 3 && old.max_columns_ == 1 && old.gap_ == 11 && !old.target_panel_ && old.click_target_,
                "existing enemy settings retain one column and inline targets")) return false;
            module.target_panel_ = false; module.max_rows_ = 2; module.max_columns_ = 2;
            module.BeginControllerBrowse(100, 20);
            module.viewport_ = {0, 0, 300, 500};
            module.state_.bounds.x = 280;
            const auto constrained = module.LayoutEntries(module.source_.Entries(), now);
            if (!Check(constrained.count == 2 && constrained.bounds.x + constrained.bounds.width <= 300 &&
                !module.ControllerMember(), "viewport capacity hides complete columns and releases hidden controller identities")) return false;
            module.target_panel_ = true; module.target_offset_x_ = 500; module.target_offset_y_ = -500;
            const auto offsets = module.LayoutEntries(module.source_.Entries(), now);
            if (!Check(offsets.bounds.width <= module.viewport_.width && offsets.bounds.height <= module.viewport_.height &&
                module.HitTarget(offsets.entries[0].target.x + 1, offsets.entries[0].target.y + 1, now).first == 0,
                "giant target offsets stay measured inside the viewport and respect disabled pointer targeting")) return false;
            module.target_offset_y_ = -20;
            auto saved = settings_store::FromIniText(""); module.Save(saved);
            enemy_list_module restored; restored.ApplyTextDefaults(ui::theme::CreateDefault()); restored.Load(saved);
            return Check(restored.max_rows_ == module.max_rows_ && restored.max_columns_ == module.max_columns_ &&
                restored.column_gap_ == module.column_gap_ && !restored.click_target_ && restored.target_offset_y_ == -20,
                "enemy grid, target panel and input preferences survive persistence");
        }
    };

    auto RunEnemyLayoutChecks(void) -> bool { return enemy_list_layout_test::Run(); }
}
