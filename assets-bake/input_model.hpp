#pragma once

#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

struct InputMaterialInfo {
    // This is purely informational and for debugging
    std::string materialName;

    glm::vec3 baseColor;
    glm::vec3 baseEmission;
    float baseRoughness;
    float baseMetalness;

    std::string baseColorTexturePath;
    std::string emissiveTexturePath;
    std::string roughnessTexturePath;
    std::string metalnessTexturePath;
    std::string normalMapTexturePath;
    std::string alphaMaskTexturePath;

    bool has_alpha_mask() const {
        return alphaMaskTexturePath != "";
    }
};

struct InputMeshInfo {
    // This is purely informational and for debugging
    std::string meshName;

    std::size_t materialIndex;

    std::size_t vertexStartIndex;
    std::size_t vertexCount;
};

struct InputModel {
    std::string modelSourcePath;

    std::vector<InputMaterialInfo> materials;
    std::vector<InputMeshInfo> meshes;

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
};
