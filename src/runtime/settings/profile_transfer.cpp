#include "profile_transfer.hpp"
#include "chat/chat_encoding.hpp"

#include <algorithm>
#include <array>
#include <commdlg.h>
#include <objbase.h>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")

namespace
{
    constexpr UINT cancel_picker = WM_APP + 618;
    struct picker_context { xiui::profile_transfer* owner; std::stop_token stop; };
}

namespace xiui
{
    profile_transfer::~profile_transfer() { this->Stop(); }

    auto profile_transfer::Start(profile_transfer_request request) -> bool
    {
        if (this->Busy()) return false;
        this->selecting_ = request.browse;
        try
        {
            this->worker_ = std::jthread{[this, request = std::move(request)](std::stop_token stop) mutable {
                this->Run(std::move(request), stop);
            }};
            return true;
        }
        catch (...) { return false; }
    }

    auto profile_transfer::Take() -> std::optional<profile_transfer_result>
    {
        std::optional<profile_transfer_result> result;
        {
            std::lock_guard lock(this->mutex_);
            if (!this->result_) return std::nullopt;
            result = std::move(this->result_);
            this->result_.reset();
        }
        result->stopped |= this->worker_.get_stop_token().stop_requested();
        result->cancelled |= result->stopped;
        this->worker_.join();
        return result;
    }

    auto profile_transfer::Cancel() -> void
    {
        this->worker_.request_stop();
        std::lock_guard lock(this->mutex_);
        if (this->picker_ != nullptr) ::PostMessageW(this->picker_, cancel_picker, 0, 0);
    }

    auto profile_transfer::Stop() -> void
    {
        this->Cancel();
        if (this->worker_.joinable()) this->worker_.join();
        this->result_.reset();
    }

    auto CALLBACK profile_transfer::PickerHook(HWND window, UINT message, WPARAM, LPARAM lparam) -> UINT_PTR
    {
        auto* context = reinterpret_cast<picker_context*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_INITDIALOG)
        {
            context = reinterpret_cast<picker_context*>(reinterpret_cast<OPENFILENAMEW*>(lparam)->lCustData);
            ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(context));
            std::lock_guard lock(context->owner->mutex_);
            context->owner->picker_ = window;
            if (context->stop.stop_requested()) ::PostMessageW(window, cancel_picker, 0, 0);
        }
        else if (context != nullptr && message == cancel_picker)
            ::PostMessageW(::GetParent(window), WM_COMMAND, IDCANCEL, 0);
        else if (context != nullptr && message == WM_NCDESTROY)
        {
            std::lock_guard lock(context->owner->mutex_);
            context->owner->picker_ = nullptr;
        }
        return 0;
    }

    auto profile_transfer::PickFile(const profile_transfer_request& request, std::stop_token stop, std::string& error) -> std::filesystem::path
    {
        const bool save = request.kind == profile_transfer_kind::export_file;
        std::array<wchar_t, 32768> filename{};
        if (save)
        {
            auto name = FromUtf8(request.name);
            for (auto& c : name) if (c < 32 || std::wstring_view{L"<>:\"/\\|?*"}.find(c) != std::wstring_view::npos) c = L'_';
            const auto suggested = L"XIUIplus - " + name + L".xiuiplus";
            std::copy_n(suggested.begin(), std::min(suggested.size(), filename.size() - 1), filename.begin());
        }
        picker_context context{this, stop};
        OPENFILENAMEW dialog{}; dialog.lStructSize = sizeof(dialog);
        // No game owner: Windows must not disable the game while this worker shows a picker.
        dialog.lpstrFilter = L"XIUI+ profile (*.xiuiplus; *.xiui)\0*.xiuiplus;*.xiui\0All files\0*.*\0";
        dialog.lpstrDefExt = L"xiuiplus";
        dialog.lpstrFile = filename.data(); dialog.nMaxFile = static_cast<DWORD>(filename.size());
        dialog.lpstrTitle = save ? L"Export XIUI+ profile" : L"Import XIUI+ profile";
        dialog.lpfnHook = &profile_transfer::PickerHook;
        dialog.lCustData = reinterpret_cast<LPARAM>(&context);
        dialog.Flags = OFN_EXPLORER | OFN_ENABLEHOOK | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
            (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
        if (stop.stop_requested()) return {};
        const bool selected = (save ? ::GetSaveFileNameW(&dialog) : ::GetOpenFileNameW(&dialog)) != FALSE;
        if (!selected && ::CommDlgExtendedError() != 0) error = "Could not open the file picker. Try again.";
        {
            std::lock_guard lock(this->mutex_);
            this->picker_ = nullptr;
        }
        return selected ? std::filesystem::path{filename.data()} : std::filesystem::path{};
    }

    auto profile_transfer::Run(profile_transfer_request request, std::stop_token stop) -> void
    {
        profile_transfer_result result; result.kind = request.kind;
        const auto com = request.browse ? ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE) : E_FAIL;
        try
        {
            if (request.browse && FAILED(com)) result.error = "Could not initialize the file picker. Try again.";
            result.path = request.browse && result.error.empty() ? this->PickFile(request, stop, result.error) : request.path;
            this->selecting_ = false;
            result.cancelled = stop.stop_requested() || (request.browse && result.path.empty() && result.error.empty());
            if (!result.cancelled && result.error.empty())
            {
                if (request.kind == profile_transfer_kind::export_file)
                    SaveProfileFile(result.path, request.name, request.snapshot, result.error, stop);
                else if (request.kind == profile_transfer_kind::import_file)
                    result.profile = ReadProfileFile(result.path, result.error);
                else if (request.kind == profile_transfer_kind::import_key) result.profile = DecodeProfileKey(request.key, result.error);
            }
        }
        catch (...) { result.error = "Could not process the profile. Try again or choose a different file."; }
        if (SUCCEEDED(com)) ::CoUninitialize();
        result.stopped = stop.stop_requested();
        result.cancelled |= result.stopped;
        if (result.stopped) result.profile.reset();
        std::lock_guard lock(this->mutex_);
        this->result_ = std::move(result);
    }
}
