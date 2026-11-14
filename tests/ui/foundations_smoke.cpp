#include "notifications/notification_center.hpp"
#include "entities/entity_name_colors.hpp"
#include "resources/item_icon.hpp"
#include "resources/menu_window.hpp"
#include "resources/skill_names.hpp"
#include "ui/nine_slice.hpp"
#include "ui/clipped_quad.hpp"
#include "world_projection.hpp"
#include "native_hud_options.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>

namespace
{
    auto check(const bool condition, const char* message) -> bool
    {
        if (!condition)
            std::fprintf(stderr, "foundations smoke failed: %s\n", message);
        return condition;
    }

    auto name_color_cases(void) -> bool
    {
        constexpr std::array<uint32_t, 3> group{0x00012CF0, 0x00022800, 0x00033000};
        return check(!xiui::EntityHasHealth(0x02) && !xiui::EntityHasHealth(0) && !xiui::EntityHasHealth(0x12) && !xiui::EntityHasHealth(0x102),
                "NPCs and scenery have no target health display") &&
            check(xiui::EntityHasHealth(0x10) && xiui::EntityHasHealth(0x01) && xiui::EntityHasHealth(0x0D) && xiui::EntityHasHealth(0x110) &&
                xiui::EntityHasHealth(0x100) && xiui::EntityHasHealth(0x104) && xiui::EntityHasHealth(0x108),
                "monsters, players, and friendly combatants retain target health") &&
            check(xiui::EntityNameColor(0x10, 0, false, false) == xiui::unclaimed_name_color, "unclaimed enemy names are yellow") &&
            check(xiui::IsGroupClaim(0x00012CF0, group) && xiui::IsGroupClaim(0x2800, group), "claims match group IDs despite the native high-word flag") &&
            check(!xiui::IsGroupClaim(0x10000, group) && !xiui::IsGroupClaim(0x1234, group), "zero and unrelated claims do not belong to the group") &&
            check(xiui::EntityNameColor(0x10, 0x12CF0, true, false) == xiui::group_claim_name_color, "group-claimed enemy names are red") &&
            check(xiui::EntityNameColor(0x10, 0x1234, false, false) == xiui::other_claim_name_color, "other groups' enemies are purple") &&
            check(xiui::EntityNameColor(0x10, 0x10000, false, false) == xiui::unclaimed_name_color, "a claim flag without a claimer stays yellow") &&
            check(xiui::EntityNameColor(0x01, 0, false, false) == 0xFFFFFFFF &&
                xiui::EntityNameColor(0x01, 0, false, true) == 0xFFC2FFFF, "player names distinguish group members") &&
            check(xiui::EntityNameColor(0x02, 0, false, false) == 0xFFC2FFC2 &&
                xiui::EntityNameColor(0x110, 0, false, false) == 0xFFC2FFC2, "NPCs and friendly allies are green") &&
            check(xiui::EntityNameColor(0x10, 0, false, false, 6) == xiui::group_claim_name_color,
                "explicit native name colors take precedence");
    }

    auto queue_cases(void) -> bool
    {
        xiui::notification_center center;
        center.Post({xiui::notification_kind::experience, 0, 100, "XP"}, 100);
        center.Post({xiui::notification_kind::experience, 0, 25, "XP"}, 300);
        if (!check(center.Entries().size() == 1 && center.Entries().front().event.amount == 125, "coalesce a reward burst"))
            return false;
        center.Post({xiui::notification_kind::experience, 0, 50, "XP"}, 800);
        center.Update(1299, 1000);
        if (!check(center.Entries().size() == 2, "coalescing refreshes expiry"))
            return false;
        center.Update(1300, 1000);
        if (!check(center.Entries().size() == 1 && center.Entries().front().event.amount == 50, "expire independently"))
            return false;
        center.Update(1800, 1000);
        center.Post({xiui::notification_kind::item, 1, -1, "Item"}, 2000);
        center.Post({xiui::notification_kind::skill, 2, std::numeric_limits<double>::quiet_NaN(), "Skill"}, 2000);
        if (!check(center.Entries().empty(), "reject invalid amounts"))
            return false;
        for (uint32_t i = 0; i < 100; ++i)
            center.Post({xiui::notification_kind::item, i, 1, "Item"}, 3000);
        return check(center.Entries().size() == xiui::notification_center::capacity && center.Entries().back().event.key == 99,
            "bound the queue and retain recent rewards");
    }

    auto invitation_cases(void) -> bool
    {
        xiui::notification_center center;
        center.Post({xiui::notification_kind::party_invite, 99, 0, "Party Invitation: Inviter"}, 1000);
        center.Post({xiui::notification_kind::experience, 0, 100, "XP"}, 1000);
        center.Update(4200, 3200);
        if (!check(center.Entries().size() == 1 && center.Entries().front().event.kind == xiui::notification_kind::party_invite,
            "party invites survive the regular notification duration")) return false;
        const auto& invite = center.Entries().front();
        if (!check(invite.Remaining(1000, 3200) == 90000 && invite.Remaining(2000, 3200) == 89000 &&
            invite.Remaining(90999, 3200) == 1 && invite.Remaining(91000, 3200) == 0,
            "invite countdown reaches zero at the 90-second deadline")) return false;
        for (uint32_t i = 0; i < 100; ++i)
            center.Post({xiui::notification_kind::item, i, 1, "Item"}, 5000);
        if (!check(center.Entries().size() == xiui::notification_center::capacity && center.Entries().front().event.key == 99,
            "reward bursts cannot evict the pending invitation")) return false;
        center.Post({xiui::notification_kind::party_invite, 100, 0, "Alliance Invitation: Leader"}, 6000);
        center.Update(9200, 3200);
        if (!check(center.Entries().size() == 1 && center.Entries().front().event.key == 100 &&
            center.Entries().front().Remaining(6000, 3200) == 90000,
            "a new party or alliance invitation replaces the old one and restarts its countdown")) return false;
        center.Update(95999, 3200);
        if (!check(center.Entries().size() == 1, "invitations remain until their own expiry")) return false;
        center.Update(96000, 3200);
        if (!check(center.Entries().empty(), "expired invitations leave the queue")) return false;
        center.Post({xiui::notification_kind::party_invite, 99, 0, "Party Invitation: Inviter"}, 100000);
        center.Post({xiui::notification_kind::item, 1, 1, "Item"}, 100000);
        center.Dismiss(xiui::notification_kind::party_invite);
        return check(center.Entries().size() == 1 && center.Entries().front().event.kind == xiui::notification_kind::item,
            "answering an invitation dismisses only the invitation");
    }

    auto packet_cases(void) -> bool
    {
        constexpr uint32_t player = 0x12345678;
        std::array<uint8_t, 0x1C> packet{};
        const auto dword = [&packet](size_t offset, uint32_t value) { std::memcpy(packet.data() + offset, &value, sizeof(value)); };
        const auto word = [&packet](size_t offset, uint16_t value) { std::memcpy(packet.data() + offset, &value, sizeof(value)); };
        dword(4, player);
        dword(0x10, 526);
        dword(0x14, 3);
        word(0x18, 253);
        for (size_t size = 0; size < packet.size(); ++size)
            if (!check(!xiui::DecodeRewardPacket(0x2D, {packet.data(), size}, player), "reject truncated reward packets"))
                return false;
        auto event = xiui::DecodeRewardPacket(0x2D, packet, player);
        if (!check(event && event->amount == 526 && event->subject == "XP", "chain XP uses reward amount, not chain count"))
            return false;
        if (!check(!xiui::DecodeRewardPacket(0x2D, packet, player + 1), "ignore another player's reward"))
            return false;
        word(0x18, 0x8000 | 372);
        event = xiui::DecodeRewardPacket(0x2D, packet, player);
        if (!check(event && event->subject == "Limit points", "decode limit chains with the message flag"))
            return false;
        dword(8, player);
        dword(0x0C, 3);
        dword(0x10, 3);
        word(0x18, 38);
        event = xiui::DecodeRewardPacket(0x29, packet, player);
        if (!check(event && event->key == 3 && std::abs(event->amount - 0.3) < 0.001, "decode skill tenths"))
            return false;
        word(0x18, 53);
        return check(!xiui::DecodeRewardPacket(0x29, packet, player), "ignore the duplicate integer skill-level message");
    }

    auto geometry_cases(void) -> bool
    {
        using xiui::ui::textured_vertex;
        const std::array<textured_vertex, 4> quad{{{9.75f, 19.25f, 0, 1, 0x20406080, 0, 0}, {29.75f, 19.25f, 0, 1, 0x6080A0C0, 1, 0},
            {9.75f, 39.25f, 0, 1, 0xA0C0E0FF, 0, 1}, {29.75f, 39.25f, 0, 1, 0xFFE0C0A0, 1, 1}}};
        const xiui::ui::rect exactClip{10.25f, 19.75f, 20, 20};
        std::vector<textured_vertex> triangles;
        xiui::ui::AppendClippedQuad(triangles, quad, exactClip);
        constexpr int fan[]{0, 1, 3, 0, 3, 2};
        if (!check(triangles.size() == 6, "fully contained glyphs retain both triangles on fractional inclusive clip edges")) return false;
        for (size_t index = 0; index < triangles.size(); ++index)
            if (!check(std::memcmp(&triangles[index], &quad[fan[index]], sizeof(textured_vertex)) == 0,
                "inside clipping preserves exact fan order, colors, UVs and positions")) return false;
        triangles.clear(); xiui::ui::AppendClippedQuad(triangles, quad, xiui::ui::rect{30.26f, 19.75f, 20, 20});
        if (!check(triangles.empty(), "fully rejected glyphs produce no triangles just beyond the fractional clip edge")) return false;
        triangles.clear(); xiui::ui::AppendClippedQuad(triangles, quad, xiui::ui::rect{20.25f, 19.75f, 10, 20});
        const auto edge = std::find_if(triangles.begin(), triangles.end(), [](const auto& vertex) { return vertex.x == 19.75f && vertex.y == 19.25f; });
        if (!check(edge != triangles.end() && edge->u == 0.5f && edge->v == 0 && edge->color == 0x406080A0,
            "partially clipped controls keep interpolated edge colors and UVs")) return false;

        const auto frame = xiui::ui::LayoutNineSliceFrame({10, 20, 320, 160}, 24, 5);
        if (!check(frame[0].bounds.width == 24 && frame[1].bounds.height == 5 && frame[1].bounds.x == 34 &&
            frame[5].bounds.x == 325 && frame[5].bounds.width == 5 && frame[7].bounds.y == 175 &&
            frame[4].bounds.width == 320 && frame[4].bounds.height == 160,
            "native frame keeps thin edges, larger corner fades, and a continuous background"))
            return false;
        const auto large = xiui::ui::LayoutNineSlice({10, 20, 320, 160}, 16);
        if (!check(large[0].bounds.width == 16 && large[8].bounds.width == 16 && large[8].bounds.x == 314,
            "resizing keeps corners fixed and aligned"))
            return false;
        if (!check(large[4].bounds.width == 288 && large[4].uv.width == 18 && large[4].uv.height == 8,
            "center tiles at native density"))
            return false;
        const auto tiny = xiui::ui::LayoutNineSlice({0, 0, 10, 8}, 16);
        float area{};
        for (const auto& tile : tiny)
        {
            if (!check(tile.bounds.width >= 0 && tile.bounds.height >= 0, "tiny containers have no inverted tiles"))
                return false;
            area += tile.bounds.width * tile.bounds.height;
        }
        if (!check(area == 80 && tiny[4].bounds.width == 0, "tiny slices cover the requested bounds without overlap"))
            return false;

        D3DMATRIX view{};
        view._11 = view._22 = view._33 = view._44 = 1;
        D3DMATRIX projection = view;
        projection._34 = 1;
        projection._44 = 0;
        const D3DVIEWPORT8 viewport{10, 20, 800, 600, 0, 1};
        float x{}, y{};
        if (!check(xiui::ProjectWorldPoint({2, 1, 10}, view, projection, viewport, x, y) && std::abs(x - 490) < 0.001f && std::abs(y - 290) < 0.001f,
            "project a world anchor into the active viewport"))
            return false;
        return check(!xiui::ProjectWorldPoint({0, 0, -10}, view, projection, viewport, x, y) &&
            !xiui::ProjectWorldPoint({20, 0, 10}, view, projection, viewport, x, y), "hide behind-camera and off-screen anchors");
    }

    auto resource_cases(void) -> bool
    {
        if (!check(xiui::resources::SkillName(3) == "Sword" && xiui::resources::SkillName(29) == "Evasion" &&
            xiui::resources::SkillName(36) == "Elemental Magic" && xiui::resources::SkillName(49) == "Woodworking" &&
            xiui::resources::SkillName(62) == "Skill 62", "name weapon, defensive, magic, crafting, and unknown skills"))
            return false;
        std::array<uint8_t, 56> dib{};
        const auto dword = [&dib](const size_t offset, const uint32_t value) { std::memcpy(dib.data() + offset, &value, sizeof(value)); };
        dword(0, 40);
        dword(4, 2);
        dword(8, 2);
        dib[12] = 1;
        dib[14] = 8;
        dword(32, 2);
        dword(44, 0x80FF0000);
        dib[49] = 1;
        dib[52] = 1;
        auto bitmap = xiui::resources::DecodeItemIcon(dib);
        if (!check(bitmap && bitmap->width == 2 && bitmap->height == 2 && bitmap->pixels[0] == 0xFFFF0000 &&
            bitmap->pixels[1] == 0 && bitmap->pixels[2] == 0 && bitmap->pixels[3] == 0xFFFF0000,
            "decode padded item-icon rows with correct orientation and transparent black"))
            return false;
        if (!check(!xiui::resources::DecodeItemIcon({dib.data(), dib.size() - 1}), "reject truncated item icons"))
            return false;
        // Older Ashita cores rebuild a complete .bmp inside the fixed bitmap buffer.
        std::array<uint8_t, 14 + 56 + 32> file{'B', 'M'};
        const uint32_t pixels = 14 + 48;
        std::memcpy(file.data() + 10, &pixels, sizeof(pixels));
        std::memcpy(file.data() + 14, dib.data(), dib.size());
        const auto rebuilt = xiui::resources::DecodeItemIcon(file);
        const auto padded = xiui::resources::DecodeItemIcon(std::span{file}.subspan(14));
        if (!check(rebuilt && padded && rebuilt->pixels == bitmap->pixels && padded->pixels == bitmap->pixels,
            "decode rebuilt .bmp files and headerless DIBs from padded Ashita buffers"))
            return false;
        const uint32_t overlap = 14 + 40;
        std::memcpy(file.data() + 10, &overlap, sizeof(overlap));
        if (!check(!xiui::resources::DecodeItemIcon(file), "reject .bmp pixel offsets inside the palette"))
            return false;
        dib[52] = 2;
        return check(!xiui::resources::DecodeItemIcon(dib), "reject invalid palette indices");
    }

    auto game_hud_cases(void) -> bool
    {
        xiui::native_hud_preferences preferences;
        if (!check(preferences.HiddenMask() == 0, "leave the game HUD unchanged by default"))
            return false;
        preferences.shown[static_cast<size_t>(xiui::native_hud_element::player)] = false;
        preferences.shown[static_cast<size_t>(xiui::native_hud_element::chat2)] = false;
        const auto selected = xiui::NativeHudBit(xiui::native_hud_element::player) | xiui::NativeHudBit(xiui::native_hud_element::chat2);
        if (!check(preferences.HiddenMask() == selected, "hide individual native elements independently"))
            return false;
        preferences.enabled = false;
        if (!check(preferences.HiddenMask() == 0, "pause hiding without clearing the choices"))
            return false;
        preferences.enabled = true;
        if (!check(preferences.HiddenMask() == selected, "resume the same native HUD choices"))
            return false;
        preferences.shown.fill(true);
        return check(preferences.HiddenMask() == 0, "restore all native elements");
    }
}

int main(int argc, char** argv)
{
    if (!name_color_cases() || !queue_cases() || !invitation_cases() || !packet_cases() || !geometry_cases() || !resource_cases() || !game_hud_cases())
        return 1;
    if (argc > 1)
    {
        std::ifstream file(argv[1], std::ios::binary);
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>{file}, {}};
        const auto skin = xiui::resources::DecodeMenuWindow(bytes);
        if (!check(skin && skin->textures[0].width == 128 && skin->textures[1].width == 32 &&
            skin->textures[0].pixels[0] == 0xFF31305A && skin->textures[1].pixels[0] == 0 &&
            skin->patches[0].width == 24 && skin->patches[1].height == 5 && skin->patches[7].v == 26 &&
            skin->patches[4].colors[0] == 0x20404040 && skin->patches[1].additive && !skin->patches[4].additive,
            "decode the native DAT's pixels, transparent frame, slices, gradients, and blend modes"))
            return 1;
        bytes.resize(0x76);
        if (!check(!xiui::resources::DecodeMenuWindow(bytes), "reject a truncated compressed texture"))
            return 1;
    }
    for (int index = 2; index < argc; ++index)
    {
        std::ifstream file(argv[index], std::ios::binary);
        const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>{file}, {}};
        const auto skin = xiui::resources::DecodeMenuWindow(bytes);
        if (!check(skin.has_value(), "decode each installed Window Type DAT")) return 1;
        for (size_t tile = 0; tile < skin->patches.size(); ++tile)
        {
            const auto& patch = skin->patches[tile];
            const auto& image = skin->textures[patch.texture];
            if (!check((tile % 3 == 1 || uint32_t{patch.u} + patch.width <= image.width) &&
                (tile / 3 == 1 || uint32_t{patch.v} + patch.height <= image.height),
                "native window slices preserve bounded corners and repeating edges")) return 1;
        }
    }
    std::puts("Notification, resource, nine-slice, and world projection checks passed.");
    return 0;
}
