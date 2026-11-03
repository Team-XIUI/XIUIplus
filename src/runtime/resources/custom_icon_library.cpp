#include "custom_icon_library.hpp"
#include "wic_image.hpp"
#include "src/webp/decode.h"

#include <Windows.h>
#include <objbase.h>
#include <algorithm>
#include <fstream>

namespace
{
    namespace fs = std::filesystem;
    constexpr size_t file_limit = 8 * 1024 * 1024, catalog_limit = 4096;
    auto Utf8(const fs::path& path) -> std::string { const auto text = path.generic_u8string(); return {text.begin(), text.end()}; }
    auto SupportedImage(const fs::path& path) -> bool
    {
        auto extension = path.extension().wstring();
        for (auto& c : extension) if (c >= L'A' && c <= L'Z') c += L'a' - L'A';
        constexpr std::wstring_view extensions[]{L".png", L".jpg", L".jpeg", L".bmp", L".gif", L".tif", L".tiff", L".ico", L".webp"};
        return std::find(std::begin(extensions), std::end(extensions), extension) != std::end(extensions);
    }
    auto Inside(const fs::path& root, const fs::path& path) -> bool
    {
        const auto relative = path.lexically_relative(root);
        return !relative.empty() && !relative.is_absolute() && *relative.begin() != L"..";
    }
    auto ReadIcon(const fs::path& directory, const std::string& key) -> std::optional<xiui::resources::menu_texture>
    {
        const auto relative = xiui::resources::custom_icon_library::RelativePath(key);
        if (!relative) return {};
        const auto root = fs::weakly_canonical(directory), path = fs::weakly_canonical(root / *relative);
        if (!Inside(root, path) || !fs::is_regular_file(path)) return {};
        const auto size = fs::file_size(path);
        if (size == 0 || size > file_limit) return {};
        std::ifstream file{path, std::ios::binary};
        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) return {};
        return xiui::resources::custom_icon_library::DecodeImage(bytes);
    }
    auto Scan(const fs::path& directory, const std::stop_token stop, xiui::resources::custom_icon_scan_error& error) -> std::vector<xiui::resources::custom_icon_entry>
    {
        std::vector<xiui::resources::custom_icon_entry> entries;
        std::error_code directoryError;
        fs::create_directories(directory, directoryError);
        if (directoryError || !fs::is_directory(directory)) { error = xiui::resources::custom_icon_scan_error::folder; return entries; }
        const auto root = fs::weakly_canonical(directory);
        size_t visited{};
        for (fs::recursive_directory_iterator it{root, fs::directory_options::skip_permission_denied}, end; it != end && !stop.stop_requested(); ++it)
        {
            if (++visited > 32768 || entries.size() >= catalog_limit) { error = xiui::resources::custom_icon_scan_error::limit; break; }
            const auto attributes = ::GetFileAttributesW(it->path().c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 || it.depth() >= 16)
            { it.disable_recursion_pending(); continue; }
            std::error_code statusError;
            if (!it->is_regular_file(statusError) || statusError || !SupportedImage(it->path())) continue;
            const auto relative = it->path().lexically_relative(root);
            const auto key = "custom:" + Utf8(relative);
            if (!xiui::resources::custom_icon_library::RelativePath(key)) continue;
            entries.push_back({key, Utf8(relative.stem()), Utf8(relative.parent_path())});
        }
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.key < b.key; });
        return entries;
    }
}

namespace xiui::resources
{
    auto custom_icon_library::RelativePath(const std::string_view key) -> std::optional<fs::path>
    {
        if (!key.starts_with("custom:") || key.size() > 240 || key.size() == 7 || key.find(':', 7) != std::string_view::npos ||
            std::any_of(key.begin(), key.end(), [](unsigned char c) { return c < 32 || c == 127; })) return {};
        std::string name{key.substr(7)}; std::replace(name.begin(), name.end(), '\\', '/');
        try
        {
            const auto path = fs::u8path(name);
            if (path.is_absolute() || path.has_root_name() || path.has_root_directory() || !SupportedImage(path)) return {};
            for (const auto& part : path) if (part == L".." || part == L"." || part.empty()) return {};
            return path;
        }
        catch (...) { return {}; }
    }

    auto custom_icon_library::DecodeImage(const std::span<const uint8_t> bytes) -> std::optional<menu_texture>
    {
        if (bytes.empty() || bytes.size() > file_limit) return {};
        if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "WEBP", 4) != 0)
            return DecodeWicImage(bytes, 2048, false, 128);
        WebPDecoderConfig config{};
        if (!WebPInitDecoderConfig(&config) || WebPGetFeatures(bytes.data(), bytes.size(), &config.input) != VP8_STATUS_OK ||
            config.input.width <= 0 || config.input.height <= 0 || config.input.width > 2048 || config.input.height > 2048 || config.input.has_animation) return {};
        constexpr uint32_t side = 128;
        const auto longest = std::max(config.input.width, config.input.height);
        const auto width = static_cast<uint32_t>(std::max(1, config.input.width * static_cast<int>(side) / longest));
        const auto height = static_cast<uint32_t>(std::max(1, config.input.height * static_cast<int>(side) / longest));
        menu_texture image{width, height, {}};
        image.pixels.resize(size_t{width} * height);
        config.options.use_scaling = 1;
        config.options.scaled_width = static_cast<int>(width); config.options.scaled_height = static_cast<int>(height);
        config.output.colorspace = MODE_BGRA; config.output.is_external_memory = 1;
        config.output.u.RGBA.rgba = reinterpret_cast<uint8_t*>(image.pixels.data());
        config.output.u.RGBA.stride = static_cast<int>(width * 4); config.output.u.RGBA.size = image.pixels.size() * 4;
        const auto status = WebPDecode(bytes.data(), bytes.size(), &config);
        WebPFreeDecBuffer(&config.output);
        if (status != VP8_STATUS_OK) return {};
        menu_texture fitted{side, side, {}};
        fitted.pixels.resize(size_t{side} * side);
        const uint32_t x = (side - width) / 2, y = (side - height) / 2;
        for (uint32_t row = 0; row < height; ++row)
            std::copy_n(image.pixels.data() + size_t{row} * width, width, fitted.pixels.data() + size_t{row + y} * side + x);
        return fitted;
    }

    auto custom_icon_library::SetDirectory(fs::path directory) -> void
    {
        if (directory == this->directory_) return;
        this->Shutdown();
        this->directory_ = std::move(directory);
    }

    auto custom_icon_library::Start(void) -> bool
    {
        if (this->directory_.empty()) return false;
        if (!this->worker_.joinable())
            try { this->worker_ = std::jthread{[this](std::stop_token stop) { this->Run(stop); }}; }
            catch (...) { this->error_ = custom_icon_scan_error::folder; return false; }
        return true;
    }

    auto custom_icon_library::Refresh(void) -> void
    {
        std::lock_guard lock{this->mutex_};
        if (!this->Start()) return;
        ++this->generation_; this->states_.clear(); this->requested_.clear(); this->completed_.clear();
        this->scan_requested_ = this->scanning_ = true; this->error_ = custom_icon_scan_error::none;
        this->wake_.notify_one();
    }

    auto custom_icon_library::Catalog(void) const -> custom_icon_catalog
    {
        std::lock_guard lock{this->mutex_};
        return {this->entries_, this->revision_, this->generation_, this->scanning_, this->scanned_, this->error_};
    }

    auto custom_icon_library::Request(const std::string_view key) -> void
    {
        if (!RelativePath(key)) return;
        std::lock_guard lock{this->mutex_};
        const std::string name{key};
        const auto found = this->states_.find(name);
        if (found != this->states_.end() && (found->second == custom_icon_state::loading || found->second == custom_icon_state::failed)) return;
        if (this->requested_.size() >= 512 || (found == this->states_.end() && this->states_.size() >= 8192) || !this->Start()) return;
        this->states_[name] = custom_icon_state::loading; this->requested_.push_back(name); this->wake_.notify_one();
    }

    auto custom_icon_library::State(const std::string_view key) const -> custom_icon_state
    {
        std::lock_guard lock{this->mutex_};
        const auto found = this->states_.find(std::string{key});
        return found != this->states_.end() ? found->second : custom_icon_state::idle;
    }

    auto custom_icon_library::TakeImages(const size_t limit) -> std::vector<decoded_custom_icon>
    {
        std::lock_guard lock{this->mutex_};
        std::vector<decoded_custom_icon> images;
        while (!this->completed_.empty() && images.size() < limit)
        {
            auto image = std::move(this->completed_.front()); this->completed_.pop_front();
            if (image.image) this->states_[image.key] = custom_icon_state::ready;
            images.push_back(std::move(image));
        }
        this->wake_.notify_one();
        return images;
    }

    auto custom_icon_library::Shutdown(void) -> void
    {
        { std::lock_guard lock{this->mutex_}; this->worker_.request_stop(); }
        this->wake_.notify_all();
        if (this->worker_.joinable()) this->worker_.join();
        std::lock_guard lock{this->mutex_};
        ++this->generation_; ++this->revision_;
        this->states_.clear(); this->requested_.clear(); this->completed_.clear();
        this->entries_ = std::make_shared<const std::vector<custom_icon_entry>>();
        this->scan_requested_ = this->scanning_ = this->scanned_ = false; this->error_ = custom_icon_scan_error::none;
    }

    auto custom_icon_library::Run(const std::stop_token stop) -> void
    {
        const auto com = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        while (!stop.stop_requested())
        {
            std::unique_lock lock{this->mutex_};
            this->wake_.wait(lock, [&] { return stop.stop_requested() || this->scan_requested_ || (!this->requested_.empty() && this->completed_.size() < 32); });
            if (stop.stop_requested()) break;
            const auto generation = this->generation_;
            const auto directory = this->directory_;
            if (this->scan_requested_)
            {
                this->scan_requested_ = false; lock.unlock();
                custom_icon_scan_error error{};
                std::vector<custom_icon_entry> entries;
                try { entries = Scan(directory, stop, error); } catch (...) { error = custom_icon_scan_error::folder; }
                auto catalog = std::make_shared<const std::vector<custom_icon_entry>>(std::move(entries));
                lock.lock();
                if (generation == this->generation_ && !stop.stop_requested())
                { this->entries_ = std::move(catalog); ++this->revision_; this->scanning_ = false; this->scanned_ = true; this->error_ = error; }
            }
            else
            {
                auto key = std::move(this->requested_.front()); this->requested_.pop_front(); lock.unlock();
                std::optional<menu_texture> image;
                try { if (SUCCEEDED(com)) image = ReadIcon(directory, key); } catch (...) {}
                lock.lock();
                if (generation == this->generation_ && !stop.stop_requested())
                {
                    this->states_[key] = image ? custom_icon_state::loading : custom_icon_state::failed;
                    this->completed_.push_back({std::move(key), generation, std::move(image)});
                }
            }
        }
        if (SUCCEEDED(com)) ::CoUninitialize();
    }
}
