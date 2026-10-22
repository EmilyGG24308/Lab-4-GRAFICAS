#pragma once

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "color.h"

// Escritor PNG mínimo, sin dependencias: RGBA de 8 bits y datos zlib en
// bloques "stored" (sin compresión). El archivo es un PNG real y válido, solo
// que más grande que uno comprimido. Sirve para exportar el framebuffer.
namespace png {

inline uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc = 0xFFFFFFFFu) {
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc;
}

inline void putU32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v >> 24));
    out.push_back(static_cast<uint8_t>(v >> 16));
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v));
}

inline void writeChunk(std::ofstream& file, const char type[4], const std::vector<uint8_t>& data) {
    std::vector<uint8_t> chunk;
    putU32(chunk, static_cast<uint32_t>(data.size()));
    chunk.insert(chunk.end(), type, type + 4);
    chunk.insert(chunk.end(), data.begin(), data.end());
    putU32(chunk, crc32(chunk.data() + 4, chunk.size() - 4) ^ 0xFFFFFFFFu);
    file.write(reinterpret_cast<const char*>(chunk.data()), static_cast<std::streamsize>(chunk.size()));
}

// pixels: width*height colores, fila 0 = arriba (mismo orden que el framebuffer).
inline bool write(const std::string& path, int width, int height, const std::vector<Color>& pixels) {
    if (width <= 0 || height <= 0 || pixels.size() != static_cast<size_t>(width) * height) return false;
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    const uint8_t signature[8] = {137, 'P', 'N', 'G', '\r', '\n', 26, '\n'};
    file.write(reinterpret_cast<const char*>(signature), 8);

    std::vector<uint8_t> ihdr;
    putU32(ihdr, static_cast<uint32_t>(width));
    putU32(ihdr, static_cast<uint32_t>(height));
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});  // 8 bits, RGBA, deflate, filtro 0, sin entrelazado
    writeChunk(file, "IHDR", ihdr);

    // Datos crudos: cada fila empieza con el byte de filtro 0 (None).
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(height) * (width * 4 + 1));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        for (int x = 0; x < width; ++x) {
            const Color& c = pixels[static_cast<size_t>(y) * width + x];
            raw.insert(raw.end(), {c.r, c.g, c.b, c.a});
        }
    }

    // Flujo zlib: cabecera, bloques stored de hasta 65535 bytes, Adler-32.
    std::vector<uint8_t> idat = {0x78, 0x01};
    uint32_t s1 = 1, s2 = 0;
    for (uint8_t b : raw) {
        s1 = (s1 + b) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    size_t pos = 0;
    do {
        size_t len = std::min<size_t>(65535, raw.size() - pos);
        bool last = pos + len == raw.size();
        idat.push_back(last ? 1 : 0);
        idat.push_back(static_cast<uint8_t>(len));
        idat.push_back(static_cast<uint8_t>(len >> 8));
        idat.push_back(static_cast<uint8_t>(~len));
        idat.push_back(static_cast<uint8_t>(~len >> 8));
        idat.insert(idat.end(), raw.begin() + pos, raw.begin() + pos + len);
        pos += len;
    } while (pos < raw.size());
    putU32(idat, (s2 << 16) | s1);
    writeChunk(file, "IDAT", idat);

    writeChunk(file, "IEND", {});
    return static_cast<bool>(file);
}

}  // namespace png
