#include "settings/settings_writer.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace
{
    auto check(bool condition, const char* message) -> bool
    {
        if (!condition) std::fprintf(stderr, "settings persistence check failed: %s\n", message);
        return condition;
    }

    auto contents(const std::filesystem::path& path) -> std::string
    {
        std::ifstream file{path, std::ios::binary};
        return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    }
}

int main(void)
{
    const auto preset = xiui::settings_store::FromIniText("\xEF\xBB\xBF; profile\r\n[General]\r\nenabled=0\r\ncount=4\r\nCOUNT=9\r\nscale = 1.25\r\nargb=80123456\r\n[]\r\nignored=1\r\n");
    if (!check(!preset.ReadBool("general", "enabled", true) && preset.ReadInt("GENERAL", "count", 0) == 9 &&
        preset.ReadFloat("general", "scale", 0) == 1.25f && preset.ReadArgb("general", "argb", 0) == 0x80123456 &&
        preset.ReadInt("missing", "key", 42) == 42, "embedded INI profiles preserve typed values, case-insensitive keys and missing-key fallbacks")) return 1;
    const auto root = std::filesystem::temp_directory_path() / ("xiui-settings-check-" + std::to_string(::GetCurrentProcessId()));
    std::filesystem::create_directory(root);
    struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); } } cleanupFiles{root};
    const auto path = root / "settings.ini";
    const xiui::settings_store store{path.string()};
    store.WriteBool("general", "enabled", false);
    store.WriteInt("GENERAL", "count", -7);
    store.WriteInt("general", "COUNT", 9);
    store.WriteFloat("general", "scale", 1.25f);
    store.WriteColor("general", "rgb", 0x80123456);
    store.WriteArgb("general", "argb", 0x80123456);
    store.WriteRect("layout.exp", {23, 45, 300, 80});
    if (!check(!std::filesystem::exists(path) && !store.ReadBool("GENERAL", "enabled", true) &&
        store.ReadInt("general", "count", 0) == 9 && store.ReadFloat("general", "scale", 0) == 1.25f,
        "serialization stays in memory and staged keys are case insensitive")) return 1;
    if (!check(store.Flush(), "staged values flush successfully")) return 1;
    const xiui::settings_store disk{path.string()};
    if (!check(!disk.ReadBool("general", "enabled", true) && disk.ReadInt("general", "count", 0) == 9 &&
        disk.ReadFloat("general", "scale", 0) == 1.25f && disk.ReadColor("general", "rgb", 0) == 0xFF123456 &&
        disk.ReadArgb("general", "argb", 0) == 0x80123456 && disk.ReadRect("layout.exp", {}).y == 45,
        "every persisted value type keeps its existing INI representation")) return 1;
    const auto original = contents(path);
    const auto stamp = std::filesystem::last_write_time(path);
    store.WriteBool("general", "enabled", false);
    store.WriteInt("general", "count", 9);
    if (!check(store.Flush() && contents(path) == original && std::filesystem::last_write_time(path) == stamp,
        "unchanged saves do not rewrite the file")) return 1;
    {
        std::ofstream file{path, std::ios::app};
        file << "\r\n[custom]\r\n; keep this comment\r\nunknown=42\r\n";
    }
    store.WriteBool("general", "enabled", true);
    if (!check(store.Flush() && contents(path).find("; keep this comment") != std::string::npos &&
        disk.ReadInt("custom", "unknown", 0) == 42, "saves preserve unrelated keys and comments")) return 1;

    xiui::settings_writer writer;
    const auto start = std::chrono::steady_clock::now();
    for (int revision = 0; revision < 100; ++revision)
    {
        xiui::settings_store snapshot{path.string()};
        snapshot.WriteInt("general", "revision", revision);
        for (int key = 0; key < 64; ++key)
            snapshot.WriteBool("toggles", std::to_string(key).c_str(), revision % 2 == 0);
        writer.Queue(std::move(snapshot));
    }
    const auto queued = std::chrono::steady_clock::now();
    if (!check(writer.Flush() && !writer.HasFailed() && disk.ReadInt("general", "revision", -1) == 99 &&
        !disk.ReadBool("toggles", "63", true), "rapid queued snapshots keep the final complete state")) return 1;
    const auto settled = std::chrono::steady_clock::now();
    const auto savedStamp = std::filesystem::last_write_time(path);
    if (!check(writer.Flush() && std::filesystem::last_write_time(path) == savedStamp,
        "flushing an idle writer does not schedule another save")) return 1;
    writer.Stop();
    writer.Stop();
    {
        xiui::settings_writer drain;
        xiui::settings_store snapshot{path.string()};
        snapshot.WriteInt("general", "revision", 100);
        drain.Queue(std::move(snapshot));
    }
    if (!check(disk.ReadInt("general", "revision", -1) == 100, "shutdown drains queued writes before unloading code")) return 1;

    const auto missingDirectory = root / "missing";
    const auto failedPath = missingDirectory / "settings.ini";
    xiui::settings_writer retry;
    xiui::settings_store snapshot{failedPath.string()};
    snapshot.WriteInt("general", "revision", 1);
    retry.Queue(std::move(snapshot));
    if (!check(!retry.Flush() && retry.HasFailed(), "write failures are reported and retain the snapshot")) return 1;
    std::filesystem::create_directory(missingDirectory);
    if (!check(retry.Flush() && !retry.HasFailed() && xiui::settings_store{failedPath.string()}.ReadInt("general", "revision", -1) == 1,
        "failed snapshots can be flushed after the path recovers")) return 1;
    xiui::settings_store stoppedFailure{(root / "still-missing" / "settings.ini").string()};
    stoppedFailure.WriteBool("general", "enabled", true);
    retry.Queue(std::move(stoppedFailure));
    if (!check(!retry.Flush(), "a later write failure is still visible")) return 1;
    retry.Stop();
    retry.Stop();
    if (!check(!retry.Flush(), "repeated shutdown after a failed write cannot wait on a stopped worker")) return 1;

    std::printf("Settings staging, delta writes, coalescing, shutdown, and failure recovery passed.\n");
    std::printf("100 snapshots queued in %.2f ms; persistence settled in %.2f ms.\n",
        std::chrono::duration<double, std::milli>(queued - start).count(),
        std::chrono::duration<double, std::milli>(settled - queued).count());
    return 0;
}
