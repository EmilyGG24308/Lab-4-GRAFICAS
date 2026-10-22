#include "obj_loader.h"

#include <fstream>
#include <sstream>

namespace {

// Convierte un índice OBJ al índice del vector (base 0).
//  - Positivo: OBJ empieza en 1  ->  idx - 1
//  - Negativo: relativo al final de los vértices leídos hasta ahora (-1 = último)
// Devuelve -1 si el índice no apunta a un vértice existente.
int resolveIndex(long objIndex, size_t vertexCount) {
    long count = static_cast<long>(vertexCount);
    long idx = objIndex > 0 ? objIndex - 1 : count + objIndex;
    if (objIndex == 0 || idx < 0 || idx >= count) return -1;
    return static_cast<int>(idx);
}

// Extrae el índice de posición de un token de cara: "v", "v/vt", "v//vn" o "v/vt/vn".
// Solo interesa lo que está antes de la primera '/'.
bool parsePositionIndex(const std::string& token, long& out) {
    std::string first = token.substr(0, token.find('/'));
    if (first.empty()) return false;
    try {
        size_t used = 0;
        out = std::stol(first, &used);
        return used == first.size();
    } catch (...) {
        return false;
    }
}

// Lee los colores difusos (Kd) de un .mtl. Si falla, el modelo usa colores por defecto.
void loadMTL(const std::string& path, std::vector<Material>& materials) {
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string keyword;
        ss >> keyword;
        if (keyword == "newmtl") {
            Material m;
            ss >> m.name;
            materials.push_back(m);
        } else if (keyword == "Kd" && !materials.empty()) {
            glm::vec3 kd;
            if (ss >> kd.x >> kd.y >> kd.z) materials.back().diffuse = kd;
        }
    }
}

int findMaterial(const std::vector<Material>& materials, const std::string& name) {
    for (size_t i = 0; i < materials.size(); ++i)
        if (materials[i].name == name) return static_cast<int>(i);
    return -1;
}

}  // namespace

bool loadOBJ(const std::string& path, Model& model, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "no se pudo abrir " + path;
        return false;
    }

    std::string folder = path.substr(0, path.find_last_of("/\\") + 1);
    int currentMaterial = -1;
    std::string line;
    int lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;
        std::istringstream ss(line);
        std::string keyword;
        ss >> keyword;  // líneas vacías o comentarios "#" se ignoran abajo

        if (keyword == "v") {
            glm::vec3 v;
            if (!(ss >> v.x >> v.y >> v.z)) {
                error = "vertice mal formado en la linea " + std::to_string(lineNumber);
                return false;
            }
            model.vertices.push_back(v);
        } else if (keyword == "f") {
            // Leer todos los índices de posición del polígono.
            std::vector<int> polygon;
            std::string token;
            while (ss >> token) {
                long objIndex;
                int idx;
                if (!parsePositionIndex(token, objIndex) ||
                    (idx = resolveIndex(objIndex, model.vertices.size())) < 0) {
                    error = "indice invalido '" + token + "' en la linea " + std::to_string(lineNumber);
                    return false;
                }
                polygon.push_back(idx);
            }
            if (polygon.size() < 3) {
                error = "cara con menos de 3 vertices en la linea " + std::to_string(lineNumber);
                return false;
            }
            // Triangulación en abanico: (0,1,2), (0,2,3), ... Válida para polígonos
            // convexos; las caras del modelo de la nave ya son triángulos.
            for (size_t i = 1; i + 1 < polygon.size(); ++i)
                model.faces.push_back({{polygon[0], polygon[i], polygon[i + 1]}, currentMaterial});
        } else if (keyword == "mtllib") {
            std::string name;
            if (ss >> name) loadMTL(folder + name, model.materials);
        } else if (keyword == "usemtl") {
            std::string name;
            ss >> name;
            currentMaterial = findMaterial(model.materials, name);
        }
        // vt, vn, o, s, g, # ... no son necesarios para este ejercicio.
    }

    if (file.bad()) {
        error = "error de lectura en " + path;
        return false;
    }
    if (model.vertices.empty() || model.faces.empty()) {
        error = path + " no contiene vertices o caras";
        return false;
    }
    return true;
}
