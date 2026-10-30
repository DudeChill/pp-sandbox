#pragma once

#include <cstdint>
#include <cstring>

namespace sim {

// Minimal streaming xxHash64. Deterministic across runs and thread counts.
class Hasher {
public:
    explicit Hasher(uint64_t seed = 0) { reset(seed); }

    void reset(uint64_t seed) {
        seed_ = seed;
        v1_ = seed + kP1 + kP2;
        v2_ = seed + kP2;
        v3_ = seed;
        v4_ = seed - kP1;
        total_ = 0;
        buf_len_ = 0;
    }

    void write(const void* data, size_t len) {
        const auto* p = static_cast<const uint8_t*>(data);
        total_ += len;
        if (buf_len_ + len < 32) {
            std::memcpy(buf_ + buf_len_, p, len);
            buf_len_ += len;
            return;
        }
        if (buf_len_ != 0) {
            const size_t fill = 32 - buf_len_;
            std::memcpy(buf_ + buf_len_, p, fill);
            p += fill;
            len -= fill;
            stripe(buf_);
            buf_len_ = 0;
        }
        while (len >= 32) {
            stripe(p);
            p += 32;
            len -= 32;
        }
        if (len != 0) {
            std::memcpy(buf_, p, len);
            buf_len_ = len;
        }
    }

    void u8(uint8_t v) { write(&v, sizeof v); }
    void u32(uint32_t v) { write(&v, sizeof v); }
    void u64(uint64_t v) { write(&v, sizeof v); }
    void f32(float f) {
        uint32_t u = 0;
        std::memcpy(&u, &f, sizeof u);
        write(&u, sizeof u);
    }
    void boolean(bool b) { u32(b ? 1u : 0u); }

    [[nodiscard]] uint64_t digest() const {
        uint64_t h = 0;
        if (total_ >= 32) {
            h = rotl(v1_, 1) + rotl(v2_, 7) + rotl(v3_, 12) + rotl(v4_, 18);
            h = merge(h, v1_);
            h = merge(h, v2_);
            h = merge(h, v3_);
            h = merge(h, v4_);
        } else {
            h = seed_ + kP5;
        }
        h += total_;
        const uint8_t* p = buf_;
        size_t len = buf_len_;
        while (len >= 8) {
            h ^= round(0, read64(p));
            h = rotl(h, 27) * kP1 + kP4;
            p += 8;
            len -= 8;
        }
        if (len >= 4) {
            h ^= static_cast<uint64_t>(read32(p)) * kP1;
            h = rotl(h, 23) * kP2 + kP3;
            p += 4;
            len -= 4;
        }
        while (len != 0) {
            h ^= static_cast<uint64_t>(*p) * kP5;
            h = rotl(h, 11) * kP1;
            ++p;
            --len;
        }
        h ^= h >> 33;
        h *= kP2;
        h ^= h >> 29;
        h *= kP3;
        h ^= h >> 32;
        return h;
    }

private:
    static constexpr uint64_t kP1 = 0x9E3779B185EBCA87ull;
    static constexpr uint64_t kP2 = 0xC2B2AE3D27D4EB4Full;
    static constexpr uint64_t kP3 = 0x165667B19E3779F9ull;
    static constexpr uint64_t kP4 = 0x85EBCA77C2B2AE63ull;
    static constexpr uint64_t kP5 = 0x27D4EB2F165667C5ull;

    static uint64_t rotl(uint64_t x, int r) { return (x << r) | (x >> (64 - r)); }
    static uint32_t read32(const uint8_t* p) {
        uint32_t v = 0;
        std::memcpy(&v, p, sizeof v);
        return v;
    }
    static uint64_t read64(const uint8_t* p) {
        uint64_t v = 0;
        std::memcpy(&v, p, sizeof v);
        return v;
    }
    static uint64_t round(uint64_t acc, uint64_t input) {
        acc += input * kP2;
        acc = rotl(acc, 31);
        acc *= kP1;
        return acc;
    }
    static uint64_t merge(uint64_t acc, uint64_t val) {
        acc ^= round(0, val);
        acc = acc * kP1 + kP4;
        return acc;
    }
    void stripe(const uint8_t* p) {
        v1_ = round(v1_, read64(p));
        v2_ = round(v2_, read64(p + 8));
        v3_ = round(v3_, read64(p + 16));
        v4_ = round(v4_, read64(p + 24));
    }

    uint64_t seed_ = 0;
    uint64_t v1_ = 0;
    uint64_t v2_ = 0;
    uint64_t v3_ = 0;
    uint64_t v4_ = 0;
    uint64_t total_ = 0;
    uint8_t buf_[32]{};
    size_t buf_len_ = 0;
};

} // namespace sim
