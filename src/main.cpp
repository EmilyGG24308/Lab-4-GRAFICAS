// Lab 4 - Gráficas por computadora
// Carga el modelo OBJ del laboratorio anterior y lo dibuja triángulo por
// triángulo con el rasterizador en software de triangle.h.
//
// Uso:
//   lab4 [modelo.obj] [--screenshot [salida.png]]

#include <SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>

#include "color.h"
#include "framebuffer.h"
#include "obj_loader.h"
#include "png_writer.h"
#include "triangle.h"

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
const float MARGIN = 0.12f;  // 12 % de margen a cada lado

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
std::unique_ptr<Framebuffer> framebuffer;
Color clearColor(18, 20, 32);
Color currentColor(255, 255, 255);

Model model;

// Vista fija calculada una sola vez al iniciar (ver computeView).
struct View {
    glm::vec3 center{0.0f};    // centro de la caja envolvente del modelo
    glm::mat3 rotation{1.0f};  // orientación de la cámara
    glm::vec2 offset{0.0f};    // centro de la caja proyectada (para centrar en pantalla)
    float scale = 1.0f;        // unidades del modelo -> píxeles
} view;

const glm::vec3 LIGHT_DIR = glm::normalize(glm::vec3(0.4f, 0.7f, 1.0f));

void setColor(const Color& color) { currentColor = color; }

// Modelo -> espacio de cámara: centrar y rotar. La cámara mira hacia -z.
glm::vec3 toView(const glm::vec3& v) { return view.rotation * (v - view.center); }

// Espacio de cámara -> pantalla con proyección ortográfica.
// x crece a la derecha; y de pantalla crece hacia abajo, por eso se invierte.
// z se guarda como distancia a la cámara (menor = más cerca) para el z-buffer.
glm::vec3 toScreen(const glm::vec3& p) {
    return {SCREEN_WIDTH / 2.0f + (p.x - view.offset.x) * view.scale,
            SCREEN_HEIGHT / 2.0f - (p.y - view.offset.y) * view.scale,
            -p.z};
}

// Calcula centro, orientación y escala para que el modelo quepa centrado.
void computeView() {
    // 1. Caja envolvente 3D y su centro.
    glm::vec3 minV = model.vertices[0], maxV = model.vertices[0];
    for (const glm::vec3& v : model.vertices) {
        minV = glm::min(minV, v);
        maxV = glm::max(maxV, v);
    }
    view.center = (minV + maxV) * 0.5f;

    // 2. Vista de tres cuartos: girar alrededor de Y y luego inclinar en X.
    glm::mat4 r = glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(1, 0, 0)) *
                  glm::rotate(glm::mat4(1.0f), glm::radians(-40.0f), glm::vec3(0, 1, 0));
    view.rotation = glm::mat3(r);

    // 3. Límites de la proyección ortográfica (x, y en espacio de cámara).
    glm::vec2 pMin(toView(model.vertices[0])), pMax = pMin;
    for (const glm::vec3& v : model.vertices) {
        glm::vec2 p(toView(v));
        pMin = glm::min(pMin, p);
        pMax = glm::max(pMax, p);
    }
    view.offset = (pMin + pMax) * 0.5f;

    // 4. Escala uniforme: el lado que más ocupa decide. Se evita dividir entre 0
    //    si el modelo es plano o un único punto en alguna dirección.
    glm::vec2 extent = pMax - pMin;
    float availW = SCREEN_WIDTH * (1.0f - 2.0f * MARGIN);
    float availH = SCREEN_HEIGHT * (1.0f - 2.0f * MARGIN);
    const float EPS = 1e-6f;
    float sx = extent.x > EPS ? availW / extent.x : 0.0f;
    float sy = extent.y > EPS ? availH / extent.y : 0.0f;
    if (sx > 0 && sy > 0) view.scale = std::min(sx, sy);
    else if (sx > 0 || sy > 0) view.scale = std::max(sx, sy);
    else view.scale = 1.0f;  // un solo punto: cualquier escala sirve
}

bool init(const std::string& modelPath) {
    std::string error;
    if (!loadOBJ(modelPath, model, error)) {
        std::fprintf(stderr, "Error cargando el modelo: %s\n", error.c_str());
        return false;
    }
    std::printf("Modelo %s: %zu vertices, %zu triangulos, %zu materiales\n", modelPath.c_str(),
                model.vertices.size(), model.faces.size(), model.materials.size());
    computeView();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }
    window = SDL_CreateWindow("Lab 4 - Modelo OBJ", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return false;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        return false;
    }
    framebuffer = std::make_unique<Framebuffer>(SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!framebuffer->init(renderer)) {
        std::fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

// Paso central del ejercicio: recorrer las caras y dibujar cada triángulo.
void render() {
    const Color baseColor = currentColor;

    for (const Face& face : model.faces) {
        // 1. Índices de los tres vértices de la cara.
        int i0 = face.vertexIndices[0];
        int i1 = face.vertexIndices[1];
        int i2 = face.vertexIndices[2];

        // 2. Con los índices se recuperan las tres posiciones del modelo.
        glm::vec3 v0 = model.vertices[i0];
        glm::vec3 v1 = model.vertices[i1];
        glm::vec3 v2 = model.vertices[i2];

        // 3. Transformar a espacio de cámara.
        glm::vec3 c0 = toView(v0), c1 = toView(v1), c2 = toView(v2);

        // Sombreado plano: color del material según la orientación de la cara.
        glm::vec3 n = glm::cross(c1 - c0, c2 - c0);
        float len = glm::length(n);
        float light = len > 0 ? std::max(0.0f, glm::dot(n / len, LIGHT_DIR)) : 0.0f;
        Color color = baseColor;
        if (face.material >= 0) {
            glm::vec3 kd = model.materials[face.material].diffuse * 255.0f;
            color = Color(static_cast<int>(kd.r), static_cast<int>(kd.g), static_cast<int>(kd.b));
        }
        setColor(color * (0.3f + 0.7f * light));

        // 4. Llevar a pantalla y rasterizar con la función de triángulos.
        triangle(*framebuffer, toScreen(c0), toScreen(c1), toScreen(c2), currentColor);
    }

    setColor(baseColor);
}

void cleanup() {
    framebuffer.reset();  // destruye la textura antes que el renderer
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    std::string modelPath = "models/nave_espacial.obj";
    std::string screenshotPath;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--screenshot") {
            screenshotPath = "docs/modelo-renderizado.png";
            if (i + 1 < argc && argv[i + 1][0] != '-') screenshotPath = argv[++i];
        } else {
            modelPath = arg;
        }
    }

    if (!init(modelPath)) {
        cleanup();
        return 1;
    }

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
        }

        framebuffer->clear(clearColor);
        setColor(Color(200, 200, 210));
        render();
        framebuffer->renderBuffer(renderer);

        // Modo captura: guarda exactamente los píxeles que se acaban de presentar.
        if (!screenshotPath.empty()) {
            bool ok = png::write(screenshotPath, framebuffer->width, framebuffer->height,
                                 framebuffer->pixels);
            std::printf(ok ? "Imagen guardada en %s\n" : "No se pudo guardar %s\n", screenshotPath.c_str());
            running = false;
        }
    }

    cleanup();
    return 0;
}
