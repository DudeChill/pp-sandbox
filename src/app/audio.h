#pragma once

#include <cstdint>
#include <vector>

#include "platform/sdl.h"
#include "sim/material.h"
#include "sim/math.h"
#include "sim/sound_model.h"

namespace app {

// Presentation-only procedural audio: no asset files. Every sound is
// synthesised from the event that caused it (material, energy, distance).
class Audio {
public:
    bool init();
    void shutdown();
    void impact(sim::MaterialId material, float energy_j);
    void gunshot(sim::Vec2 at, sim::Vec2 listener, float projectile_speed);
    void update();  // queue one frame of mixed samples

    [[nodiscard]] bool enabled() const { return device_ != 0; }

private:
    enum class Kind : uint8_t { Impact, Gunshot };
    struct Voice {
        Kind kind = Kind::Impact;
        float t = 0.0f;
        float duration = 0.2f;
        float frequency = 400.0f;
        float amplitude = 0.3f;
        float delay = 0.0f;
        float noise = 0.0f;
    };

    SDL_AudioDeviceID device_ = 0;
    SDL_AudioSpec spec_{};
    std::vector<Voice> voices_;
    std::vector<int16_t> buffer_;
    uint32_t rng_ = 0x1234567u;
    int sample_rate_ = 48000;
    float next_rand();

    float render_voice(Voice& voice);
};

} // namespace app
