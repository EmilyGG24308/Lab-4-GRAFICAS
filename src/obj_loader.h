#pragma once

#include <glm/glm.hpp>

#include <array>
#include <string>
#include <vector>

// Una cara triangular: tres índices (base 0) dentro de Model::vertices.
struct Face {
    std::array<int, 3> vertexIndices;
    int material = -1;  // índice en Model::materials, -1 si no tiene
};

struct Material {
    std::string name;
    glm::vec3 diffuse{0.8f, 0.8f, 0.8f};  // Kd del .mtl
};

struct Model {
    std::vector<glm::vec3> vertices;  // posiciones de las líneas "v x y z"
    std::vector<Face> faces;          // caras ya convertidas en triángulos
    std::vector<Material> materials;
};

// Carga posiciones y caras de un archivo OBJ. Devuelve false (y rellena
// "error") si el archivo no se puede leer, está vacío o tiene índices inválidos.
bool loadOBJ(const std::string& path, Model& model, std::string& error);
