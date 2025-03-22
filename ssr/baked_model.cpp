#include "baked_model.hpp"

#include <cstdio>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>

#include "../vkutils/error.hpp"

// TODO: Rename methods to snake_case
namespace baked {
    // See asset3-bake/main.cpp for more info
    constexpr char kFileMagic[16] = "\0\0SPICYMESH";
    constexpr char kFileVariant[16] = "spicy";

    constexpr std::uint32_t kMaxString = 32 * 1024;

    void checkedRead(FILE* input, const std::size_t bytes, void* buffer) {
        auto ret = std::fread(buffer, 1, bytes, input);

        if (bytes != ret) {
            throw vkutils::Error("checkedRead(): expected %zu bytes, got %zu", bytes, ret);
        }
    }

    std::uint32_t readUint32(FILE* input) {
        std::uint32_t ret;
        checkedRead(input, sizeof(std::uint32_t), &ret);
        return ret;
    }

    float readFloat(FILE* input) {
        float ret;
        checkedRead(input, sizeof(float), &ret);
        return ret;
    }

    template<glm::length_t L>
    glm::vec<L, float> readVec(FILE* input) {
        glm::vec<L, float> ret;
        checkedRead(input, sizeof(float) * L, &ret);
        return ret;
    }

    std::string readString(FILE* aFin) {
        const auto length = readUint32(aFin);

        if (length >= kMaxString) {
            throw vkutils::Error("readString(): unexpectedly long string (%u bytes)", length);
        }

        std::string ret;
        ret.resize(length);

        checkedRead(aFin, length, ret.data());
        return ret;
    }

    BakedModel loadBakedModelFromFile(FILE* input, char const* inputName) {
        BakedModel bakedModel;

        // Figure out base path
        char const* pathBeg = inputName;
        char const* pathEnd = std::strrchr(pathBeg, '/');

        std::string const prefix = pathEnd ? std::string(pathBeg, pathEnd + 1) : "";

        // Read header and verify file magic and variant
        char magic[16];
        checkedRead(input, 16, magic);

        if (0 != std::memcmp(magic, kFileMagic, 16)) {
            throw vkutils::Error("loadBakedModelFromFile(): %s: invalid file signature!", inputName);
        }

        char variant[16];
        checkedRead(input, 16, variant);

        if (0 != std::memcmp(variant, kFileVariant, 16)) {
            throw vkutils::Error("loadBakedModelFromFile(): %s: file variant is '%s', expected '%s'", inputName,
                                 variant,
                                 kFileVariant);
        }

        // Read texture info
        const auto textureCount = readUint32(input);
        for (std::uint32_t i = 0; i < textureCount; ++i) {
            const std::string name = readString(input);
            std::uint8_t channels;
            checkedRead(input, sizeof(std::uint8_t), &channels);

            BakedTextureInfo info{
                .path = prefix + name,
                .channels = channels
            };

            bakedModel.textures.emplace_back(std::move(info));
        }

        // Read material info
        const auto materialCount = readUint32(input);
        for (std::uint32_t i = 0; i < materialCount; ++i) {
            const BakedMaterialInfo info{
                .name = readString(input),
                .baseColour = readVec<3>(input),
                .emission = readVec<3>(input),
                .roughness = readFloat(input),
                .metalness = readFloat(input),
                .baseColourTextureId = readUint32(input),
                .emissiveTextureId = readUint32(input),
                .roughnessTextureId = readUint32(input),
                .metalnessTextureId = readUint32(input),
                .normalMapTextureId = readUint32(input),
                .alphaMaskTextureId = readUint32(input)
            };

            assert(info.baseColourTextureId < bakedModel.textures.size());
            assert(info.emissiveTextureId < bakedModel.textures.size());
            assert(info.roughnessTextureId < bakedModel.textures.size());
            assert(info.metalnessTextureId < bakedModel.textures.size());
            assert(info.normalMapTextureId < bakedModel.textures.size());
            assert(info.alphaMaskTextureId < bakedModel.textures.size() || info.alphaMaskTextureId == NO_ID);

            bakedModel.materials.emplace_back(info);
        }

        // Read mesh data
        const auto meshCount = readUint32(input);
        for (std::uint32_t i = 0; i < meshCount; ++i) {
            BakedMeshData data;
            data.name = readString(input);
            data.materialId = readUint32(input);
            assert(data.materialId < bakedModel.materials.size());

            const auto V = readUint32(input);
            const auto I = readUint32(input);

            data.positions.resize(V);
            checkedRead(input, V * sizeof(glm::vec3), data.positions.data());

            data.normals.resize(V);
            checkedRead(input, V * sizeof(glm::vec3), data.normals.data());

            data.uvs.resize(V);
            checkedRead(input, V * sizeof(glm::vec2), data.uvs.data());

            data.tangents.resize(V);
            checkedRead(input, V * sizeof(glm::vec4), data.tangents.data());

            data.indices.resize(I);
            checkedRead(input, I * sizeof(std::uint32_t), data.indices.data());

            bakedModel.meshes.emplace_back(std::move(data));
        }

        // Check trailing bytes
        char byte;
        if (const auto check = std::fread(&byte, 1, 1, input); 0 != check) {
            std::fprintf(stderr, "Note: '%s' contains trailing bytes\n", inputName);
        }

        return bakedModel;
    }

    BakedModel loadBakedModel(char const* modelPath) {
        std::printf("Loading scene: %s\n", modelPath);
        FILE* modelFile = std::fopen(modelPath, "rb");
        if (!modelFile) {
            throw vkutils::Error("loadBakedModel(): unable to open '%s' for reading", modelPath);
        }

        try {
            auto ret = loadBakedModelFromFile(modelFile, modelPath);
            std::fclose(modelFile);
            return ret;
        } catch (...) {
            std::fclose(modelFile);
            throw;
        }
    }
}
