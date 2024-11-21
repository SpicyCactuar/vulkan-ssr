#pragma once

#include <vector>

#include <cstdint>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

struct TriangleSoup {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
};

struct IndexedMesh {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    std::vector<glm::vec4> tangent;

    std::vector<std::uint32_t> indices;

    glm::vec3 aabbMin, aabbMax;

    IndexedMesh();
};

IndexedMesh make_indexed_mesh(
    const TriangleSoup& soup,
    float errorTolerance = 1e-6f
);
