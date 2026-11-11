#pragma once

#include "Ashita.h"
#include "resources/menu_texture.hpp"
#include "design_system.hpp"

#include <condition_variable>
#include <mutex>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace xiui
{
    using minimap_outline = std::array<ui::point, 256>;
    struct minimap_image { IDirect3DTexture8* texture{}; bool enhanced{}, known{}, loading{}; std::shared_ptr<const minimap_outline> outline{}; };

    class minimap_maps
    {
    public:
        ~minimap_maps(void) { this->Shutdown(); }
        auto SetDevice(IDirect3DDevice8* device) -> void;
        auto Shutdown(void) -> void;
        // Bundled artwork wins; nativeMap is the client's own map DAT, used when no bundled image exists.
        auto Acquire(uint16_t zone, int floor, bool enhanced, std::string_view nativeMap = {}) -> minimap_image;
        auto Count(void) const -> size_t { return this->records_.size(); }
        auto SourceName(void) const -> const char*
        { return !this->selection_ || this->selection_->record == no_source ? "none" : this->selection_->record < this->records_.size() ? "bundled" : "native"; }
    private:
        friend struct minimap_checks;
        static constexpr size_t no_source = SIZE_MAX;
        static auto DecodeImage(std::span<const uint8_t> bytes) -> std::optional<resources::menu_texture>;
        static auto DecodeNativeMap(std::span<const uint8_t> dat) -> std::optional<resources::menu_texture>;
        static auto ExtractOutline(const resources::menu_texture& image) -> std::shared_ptr<const minimap_outline>;
        struct record { uint16_t zone{}; int16_t floor{}; uint8_t style{}; std::span<const uint8_t> bytes{}; };
        // Sources at or after records_.size() index native_paths_.
        struct entry { size_t record{}; IDirect3DTexture8* texture{}; uint64_t used{}; std::shared_ptr<const minimap_outline> outline{}; };
        struct decoded { size_t record{}; uint64_t generation{}; std::optional<resources::menu_texture> image{}; std::shared_ptr<const minimap_outline> outline{}; };
        struct selection { uint16_t zone{}; int floor{}; bool enhanced{}; std::string native{}; size_t record{}; };
        std::optional<selection> selection_{};
        std::vector<record> records_{};
        std::vector<std::string> native_paths_{};
        std::vector<entry> textures_{};
        IDirect3DDevice8* device_{};
        uint64_t tick_{}, generation_{};
        bool initialized_{}, stopping_{};
        std::thread worker_{};
        std::mutex mutex_{};
        std::condition_variable wake_{};
        std::optional<std::pair<size_t, uint64_t>> requested_{};
        std::optional<decoded> completed_{};
        auto Initialize(void) -> void;
        auto Run(void) -> void;
        auto ReleaseTextures(void) -> void;
    };
}
