#include "font_catalog.hpp"

#include <atomic>
#include <mutex>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    using xiui::ui::font_entry;

    auto Wide(const std::string_view text) -> std::wstring
    {
        const int size = text.empty() ? 0 : ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring result(static_cast<size_t>(std::max(size, 0)), L'\0');
        if (size > 0) ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size);
        return result;
    }

    auto Utf8(const std::wstring_view text) -> std::string
    {
        const int size = text.empty() ? 0 : ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<size_t>(std::max(size, 0)), '\0');
        if (size > 0) ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    // Fixed storage: dropdowns hold label pointers and draws read entries while names register.
    struct font_registry
    {
        std::mutex lock{};
        std::array<font_entry, xiui::ui::max_font_count> entries{};
        std::array<const char*, xiui::ui::max_font_count> labels{};
        std::atomic<int> count{};
    };

    auto Add(font_registry& registry, font_entry entry) -> int
    {
        const int index = registry.count.load(std::memory_order_relaxed);
        if (index >= xiui::ui::max_font_count) return -1;
        registry.entries[index] = std::move(entry);
        registry.labels[index] = registry.entries[index].label.c_str();
        registry.count.store(index + 1, std::memory_order_release);
        return index;
    }

    // Outline text fonts only: bitmap and vector faces do not scale to supersampled HUD glyphs, and symbol fonts cannot show text.
    auto CALLBACK Collect(const LOGFONTW* font, const TEXTMETRICW* metrics, const DWORD type, const LPARAM parameter) -> int
    {
        const auto flags = reinterpret_cast<const NEWTEXTMETRICW*>(metrics)->ntmFlags;
        const bool outline = (type & TRUETYPE_FONTTYPE) != 0 || (flags & (NTM_PS_OPENTYPE | NTM_TT_OPENTYPE)) != 0;
        if (outline && font->lfCharSet != SYMBOL_CHARSET && font->lfFaceName[0] != L'\0' && font->lfFaceName[0] != L'@')
            reinterpret_cast<std::vector<std::wstring>*>(parameter)->emplace_back(font->lfFaceName);
        return 1;
    }

    auto Load(font_registry& registry) -> void
    {
        using namespace xiui::ui;
        for (int index = 0; index < builtin_font_count; ++index)
        {
            const auto& face = builtin_font_faces[index];
            Add(registry, {builtin_font_names[index], face.family, Wide(face.family), face.weight, face.italic, true});
        }
        std::vector<std::wstring> installed;
        if (const auto dc = ::CreateCompatibleDC(nullptr))
        {
            LOGFONTW query{}; query.lfCharSet = DEFAULT_CHARSET;
            ::EnumFontFamiliesExW(dc, &query, Collect, reinterpret_cast<LPARAM>(&installed), 0);
            ::DeleteDC(dc);
        }
        std::sort(installed.begin(), installed.end(), [](const auto& a, const auto& b) { return ::_wcsicmp(a.c_str(), b.c_str()) < 0; });
        installed.erase(std::unique(installed.begin(), installed.end(), [](const auto& a, const auto& b) { return ::_wcsicmp(a.c_str(), b.c_str()) == 0; }), installed.end());
        for (const auto& family : installed)
        {
            // Bahnschrift, Segoe UI, Tahoma and Grammara are already listed as tuned XIUI+ faces.
            bool listed{};
            for (int index = 0; index <= grammara_font; ++index) listed = listed || ::_wcsicmp(registry.entries[index].wide_family.c_str(), family.c_str()) == 0;
            if (!listed) Add(registry, {Utf8(family), Utf8(family), family, FW_BOLD, false, false});
        }
    }

    auto Registry(void) -> font_registry&
    {
        static font_registry registry{};
        static std::once_flag loaded{};
        std::call_once(loaded, [] { Load(registry); });
        return registry;
    }
}

namespace xiui::ui
{
    auto FontCount(void) -> int { return Registry().count.load(std::memory_order_acquire); }
    auto FontNames(void) -> const char* const* { return Registry().labels.data(); }
    auto FontIndex(const int index) -> int { return std::clamp(index, 0, FontCount() - 1); }
    auto FontEntry(const int font) -> const font_entry& { return Registry().entries[FontIndex(font)]; }

    auto FontFamilyIndex(const std::string_view family) -> int
    {
        const auto wide = Wide(family);
        if (wide.empty() || wide.size() >= LF_FACESIZE || wide.find_first_of(L"\r\n\t") != std::wstring::npos) return 0;
        auto& registry = Registry();
        const std::lock_guard guard{registry.lock};
        const int count = registry.count.load(std::memory_order_relaxed);
        for (int index = builtin_font_count; index < count; ++index)
            if (::_wcsicmp(registry.entries[index].wide_family.c_str(), wide.c_str()) == 0) return index;
        const auto added = Add(registry, {std::string{family}, std::string{family}, wide, FW_BOLD, false, false});
        return added < 0 ? 0 : added;
    }

    auto FontFamily(const int font) -> std::string
    {
        const auto& entry = FontEntry(font);
        return entry.builtin ? std::string{} : entry.family;
    }

    embedded_font::~embedded_font(void) { this->Clear(); }

    auto embedded_font::Load(void) -> bool
    {
        if (this->handle_ != nullptr) return true;
        const auto module = reinterpret_cast<HMODULE>(&__ImageBase);
        const auto resource = ::FindResourceW(module, MAKEINTRESOURCEW(501), MAKEINTRESOURCEW(10));
        const auto size = resource != nullptr ? ::SizeofResource(module, resource) : 0;
        const auto loaded = resource != nullptr ? ::LoadResource(module, resource) : nullptr;
        auto* data = loaded != nullptr ? ::LockResource(loaded) : nullptr;
        DWORD count{};
        if (data != nullptr && size > 0) this->handle_ = ::AddFontMemResourceEx(data, size, nullptr, &count);
        return this->handle_ != nullptr;
    }

    auto embedded_font::Clear(void) -> void
    {
        if (this->handle_ != nullptr) ::RemoveFontMemResourceEx(this->handle_);
        this->handle_ = nullptr;
    }
}
