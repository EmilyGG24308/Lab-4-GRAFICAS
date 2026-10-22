#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

#include "framebuffer.h"

// Rasteriza un triángulo relleno en software.
// A, B y C ya están en coordenadas de pantalla: x, y en píxeles (y crece hacia
// abajo) y z = profundidad (menor = más cerca de la cámara).
//
// Método: se recorre el rectángulo que encierra al triángulo (recortado a la
// pantalla) y, para cada píxel, se calculan sus coordenadas baricéntricas con
// funciones de arista. Si las tres son >= 0 el píxel está dentro del triángulo,
// y la profundidad se interpola con esas mismas coordenadas para el z-buffer.
inline void triangle(Framebuffer& fb, const glm::vec3& A, const glm::vec3& B,
                     const glm::vec3& C, const Color& color) {
    auto edge = [](const glm::vec3& p, const glm::vec3& q, float x, float y) {
        return (q.x - p.x) * (y - p.y) - (q.y - p.y) * (x - p.x);
    };

    float area = edge(A, B, C.x, C.y);
    if (std::fabs(area) < 1e-6f) return;  // triángulo degenerado (área ~0)

    int minX = std::max(0, static_cast<int>(std::floor(std::min({A.x, B.x, C.x}))));
    int minY = std::max(0, static_cast<int>(std::floor(std::min({A.y, B.y, C.y}))));
    int maxX = std::min(fb.width - 1, static_cast<int>(std::ceil(std::max({A.x, B.x, C.x}))));
    int maxY = std::min(fb.height - 1, static_cast<int>(std::ceil(std::max({A.y, B.y, C.y}))));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            float px = x + 0.5f, py = y + 0.5f;  // centro del píxel
            float w0 = edge(B, C, px, py) / area;
            float w1 = edge(C, A, px, py) / area;
            float w2 = edge(A, B, px, py) / area;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;  // fuera del triángulo

            float depth = w0 * A.z + w1 * B.z + w2 * C.z;
            fb.point(x, y, depth, color);
        }
    }
}
