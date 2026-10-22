#pragma once

#include <algorithm>
#include <cstdint>

// Color RGBA de 8 bits por canal. El orden de los campos en memoria (r, g, b, a)
// coincide con SDL_PIXELFORMAT_RGBA32, así el framebuffer se sube tal cual a SDL.
struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    Color() = default;
    Color(int red, int green, int blue, int alpha = 255)
        : r(clamp(red)), g(clamp(green)), b(clamp(blue)), a(clamp(alpha)) {}

    // Multiplica el color por una intensidad (sombreado plano).
    Color operator*(float k) const {
        return Color(static_cast<int>(r * k), static_cast<int>(g * k),
                     static_cast<int>(b * k), a);
    }

private:
    static uint8_t clamp(int v) { return static_cast<uint8_t>(std::clamp(v, 0, 255)); }
};
