#include "audio.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace app {
namespace {
constexpr float kSpeedOfSound = 343.0f;
constexpr float kTwoPi = 6.28318530718f;
constexpr int kMaxVoices = 32;
constexpr int kMaxDelaySamples = 48000;
} // namespace

bool Audio::init() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "audio: %s\n", SDL_GetError());
        return false;
    }
    SDL_AudioSpec want{};
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = nullptr;
    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &spec_, 0);
    if (device_ == 0) {
        std::fprintf(stderr, "audio disabled: %s\n", SDL_GetError());
        return false;
    }
    sample_rate_ = spec_.freq > 0 ? spec_.freq : 48000;
    voices_.reserve(kMaxVoices);
    buffer_.reserve(static_cast<size_t>(sample_rate_ / 30));
    SDL_PauseAudioDevice(device_, 0);
    return true;
}

void Audio::shutdown() {
    if (device_ != 0) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
}

float Audio::next_rand() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_ & 0xFFFFu) / 32767.5f - 1.0f;
}

void Audio::impact(sim::MaterialId material, float energy_j) {
    if (device_ == 0) return;
    if (voices_.size() >= kMaxVoices) voices_.erase(voices_.begin());
    Voice v;
    v.kind = Kind::Impact;
    v.frequency = sim::impact_tone_hz(material);
    v.amplitude = std::clamp(0.05f + energy_j / 2000.0f, 0.05f, 0.9f);
    v.duration = std::clamp(0.06f + energy_j / 20000.0f, 0.06f, 0.5f);
    v.noise = next_rand();
    voices_.push_back(v);
}

void Audio::gunshot(sim::Vec2 at, sim::Vec2 listener, float projectile_speed) {
    if (device_ == 0) return;
    if (voices_.size() >= kMaxVoices) voices_.erase(voices_.begin());
    const float dist = sim::length(at - listener);
    Voice v;
    v.kind = Kind::Gunshot;
    v.frequency = 70.0f;
    v.amplitude = std::clamp(0.85f - dist / 80.0f, 0.15f, 0.9f);
    v.duration = sim::bang_duration_seconds(1800.0f);
    v.delay = sim::sound_delay_seconds(dist);
    voices_.push_back(v);
    if (projectile_speed > 343.0f) {
        Voice crack;
        crack.kind = Kind::Impact;
        crack.frequency = 4200.0f;
        crack.amplitude = 0.25f;
        crack.duration = 0.03f;
        crack.delay = v.delay;
        crack.noise = next_rand();
        voices_.push_back(crack);
    }
}

float Audio::render_voice(Voice& v) {
    if (v.delay > 0.0f) {
        v.delay -= 1.0f / static_cast<float>(sample_rate_);
        return 0.0f;
    }
    if (v.t >= v.duration) return 0.0f;
    const float t = v.t;
    v.t += 1.0f / static_cast<float>(sample_rate_);
    if (v.kind == Kind::Gunshot) {
        const float env = std::exp(-t * 28.0f);
        const float thump = std::sin(kTwoPi * 62.0f * t) * std::exp(-t * 9.0f);
        return (0.7f * thump + 0.5f * next_rand() * env) * v.amplitude;
    }
    const float env = std::exp(-t * 55.0f);
    const float ring = std::sin(kTwoPi * v.frequency * t + v.noise);
    return (0.55f * ring + 0.45f * next_rand()) * env * v.amplitude;
}

void Audio::update() {
    if (device_ == 0) return;
    const Uint32 queued = SDL_GetQueuedAudioSize(device_);
    const size_t frame = static_cast<size_t>(sample_rate_ / 30);  // ~33 ms
    if (queued > frame * 4 * sizeof(int16_t)) return;

    buffer_.assign(frame, 0);
    for (Voice& v : voices_) {
        for (size_t i = 0; i < frame; ++i) {
            const float s = render_voice(v);
            if (s == 0.0f && v.t == 0.0f && v.delay > 0.0f) break;
            const int mixed = static_cast<int>(buffer_[i]) +
                              static_cast<int>(std::clamp(s, -1.0f, 1.0f) * 12000.0f);
            buffer_[i] = static_cast<int16_t>(std::clamp(mixed, -32767, 32767));
        }
    }
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
                                 [](const Voice& v) {
                                     return v.delay <= 0.0f && v.t >= v.duration;
                                 }),
                  voices_.end());
    SDL_QueueAudio(device_, buffer_.data(),
                   static_cast<Uint32>(buffer_.size() * sizeof(int16_t)));
}

} // namespace app
