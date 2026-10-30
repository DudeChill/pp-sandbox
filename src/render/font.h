#pragma once

#include <cstdint>

namespace render {

// 3x5 bitmap font: five rows, each row a 3-bit mask (bit 0 = rightmost column).
inline uint8_t glyph_rows(char c, uint8_t out[5]) {
    const char* rows = nullptr;
    switch (c) {
        case 'A': rows = "010101111101101"; break;
        case 'B': rows = "110101110101110"; break;
        case 'C': rows = "011100100100011"; break;
        case 'D': rows = "110101101101110"; break;
        case 'E': rows = "111100110100111"; break;
        case 'F': rows = "111100110100100"; break;
        case 'G': rows = "011100101101011"; break;
        case 'H': rows = "101101111101101"; break;
        case 'I': rows = "111010010010111"; break;
        case 'J': rows = "001001001101010"; break;
        case 'K': rows = "101101110101101"; break;
        case 'L': rows = "100100100100111"; break;
        case 'M': rows = "101111111101101"; break;
        case 'N': rows = "101111111111101"; break;
        case 'O': rows = "010101101101010"; break;
        case 'P': rows = "110101110100100"; break;
        case 'Q': rows = "010101101111011"; break;
        case 'R': rows = "110101110101101"; break;
        case 'S': rows = "011100010001110"; break;
        case 'T': rows = "111010010010010"; break;
        case 'U': rows = "101101101101011"; break;
        case 'V': rows = "101101101101010"; break;
        case 'W': rows = "101101111111101"; break;
        case 'X': rows = "101101010101101"; break;
        case 'Y': rows = "101101010010010"; break;
        case 'Z': rows = "111001010100111"; break;
        case '0': rows = "111101101101111"; break;
        case '1': rows = "010110010010111"; break;
        case '2': rows = "110001010100111"; break;
        case '3': rows = "110001010001110"; break;
        case '4': rows = "101101111001001"; break;
        case '5': rows = "111100110001110"; break;
        case '6': rows = "011100111101111"; break;
        case '7': rows = "111001010010010"; break;
        case '8': rows = "111101111101111"; break;
        case '9': rows = "111101111001110"; break;
        case ':': rows = "000010000010000"; break;
        case '.': rows = "000000000000010"; break;
        case '-': rows = "000000111000000"; break;
        case '/': rows = "001001010100100"; break;
        case '>': rows = "100010001010100"; break;
        case '<': rows = "001010100010001"; break;
        case ' ': rows = "000000000000000"; break;
        default: rows = "111101101101111"; break;  // box for unknown
    }
    for (int i = 0; i < 5; ++i) {
        const char a = rows[i * 3 + 0];
        const char b = rows[i * 3 + 1];
        const char c2 = rows[i * 3 + 2];
        out[i] = static_cast<uint8_t>(((a == '1') ? 4 : 0) |
                                      ((b == '1') ? 2 : 0) |
                                      ((c2 == '1') ? 1 : 0));
    }
    return out[0] | out[1] | out[2] | out[3] | out[4];
}

} // namespace render
