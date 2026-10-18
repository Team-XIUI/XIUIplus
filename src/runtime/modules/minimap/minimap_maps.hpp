#pragma once

#include "Ashita.h"
#include "resources/menu_texture.hpp"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <vector>

namespace xiui
{
    struct minimap_image { IDirect3DTexture8* texture{}; bool enhanced{}, known{}, loading{}; };

    class minimap_maps
    {
    public:
        ~minimap_maps(void) { this->Shutdown(); }
        auto SetDevice(IDirect3DDevice8* device) -> void;
        auto Shutdown(void) -> void;
        auto Acquire(uint16_t zone, int floor, bool enhanced) -> minimap_image;
        auto Count(void) const -> size_t { return this->records_.size(); }
    private:
        friend struct minimap_checks;
        static auto DecodeImage(std::span<const uint8_t> bytes) -> std::optional<resources::menu_texture>;
        struct record { uint16_t zone{}; int16_t floor{}; uint8_t style{}; std::span<const uint8_t> bytes{}; };
        struct entry { size_t record{}; IDirect3DTexture8* texture{}; uint64_t used{}; };
        struct decoded { size_t record{}; uint64_t generation{}; std::optional<resources::menu_texture> image{}; };
        struct selection { uint16_t zone{}; int floor{}; bool enhanced{}; size_t record{}; };
        std::optional<selection> selection_{};
        std::vector<record> records_{};
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
