#pragma once

#include <string>
#include <vector>

#include <cstdint>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/*
 * Baked file format:
 *
 *  1. Header:
 *    - 16*char: file magic = "\0\0SPICYMESH"
 *    - 16*char: variant = "default" (changes later)
 *
 *  2. Textures
 *    - uint32_t: U = number of (unique) textures
 *    - repeat U times:
 *      - string: path to texture
 *      - 1*uint8_t: number of channels in texture
 *
 *  3. Material information
 *    - uint32_t: M = number of materials
 *    - repeat M times:
 *      - string: name
 *      - vec3: base color factor
 *      - vec3: base emission factor
 *      - float: roughness factor
 *      - float: metalness factor
 *      - uint32_t: base color texture index
 *      - uint32_t: emissive texture index
 *      - uint32_t: roughness texture index
 *      - uint32_t: metalness texture index
 *      - uint32_t: normal map texture index
 *      - uint32_t: alphaMask texture index, or 0xFFFFFFF if none
 *
 *  4. Mesh data
 *    - uint32_t: M = number of meshes
 *    - repeat M times:
 *      - uint32_t: material index
 *      - uint32_t: V = number of vertices
 *      - uint32_t: I = number of indices
 *		- repeat V times: vec3 position
 *      - repeat V times: vec3 normal
 *      - repeat V times: vec2 texture coordinate
 *      - repeat V times: vec4 tangent
 *      - repeat I times: uint32_t index
 *
 * Strings are stored as
 *   - uint32_t: N = length of string in chars, including terminating \0
 *   - repeat N times: char in string
 *
 * See assets-bake/main.cpp (specifically write_model_data_()) for additional
 * information.
 */
namespace baked {
    constexpr std::uint32_t NO_ID = ~static_cast<std::uint32_t>(0);

    struct BakedTextureInfo {
        std::string path;
        std::uint8_t channels;
    };

    struct BakedMaterialInfo {
        std::string name;

        glm::vec3 baseColour;
        glm::vec3 emission;
        float roughness;
        float metalness;

        std::uint32_t baseColourTextureId;
        std::uint32_t emissiveTextureId;
        std::uint32_t roughnessTextureId;
        std::uint32_t metalnessTextureId;
        std::uint32_t normalMapTextureId;
        std::uint32_t alphaMaskTextureId = NO_ID;

        bool has_alpha_mask() const {
            return alphaMaskTextureId != NO_ID;
        }
    };

    struct BakedMeshData {
        std::string name;

        std::uint32_t materialId;

        std::vector<glm::vec3> positions;
        std::vector<glm::vec2> uvs;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec4> tangents;

        std::vector<std::uint32_t> indices;
    };

    struct BakedModel {
        std::vector<BakedTextureInfo> textures;
        std::vector<BakedMaterialInfo> materials;
        std::vector<BakedMeshData> meshes;
    };

    BakedModel loadBakedModel(char const* modelPath);
}
