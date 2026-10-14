#include "font_catalog.hpp"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace xiui::ui
{
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
