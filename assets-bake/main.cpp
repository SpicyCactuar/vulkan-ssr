#include <iterator>
#include <vector>
#include <typeinfo>
#include <exception>
#include <filesystem>
#include <system_error>
#include <unordered_map>

#include <cstdio>
#include <cstring>

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "indexed_mesh.hpp"
#include "input_model.hpp"
#include "load_model_obj.hpp"

#include "../vkutils/error.hpp"

namespace {
    /*
     * File "magic". The first 16 bytes of our custom file are equal to this
     * magic value. This allows us to check whether a certain file is
     * (probably) of the right type. Having a file magic is relatively common
     * practice -- you can find a list of such magic sequences e.g. here:
     * https://en.wikipedia.org/wiki/List_of_file_signatures
     *
     * When picking a signature there are a few considerations. For example,
     * including non-printable characters (e.g. the \0) early keeps the file
     * from being misidentified as text.
     */
    constexpr char kFileMagic[16] = "\0\0SPICYMESH";
    constexpr char kFileVariant[16] = "spicy";

    /*
     * Fallback textures
     */
    constexpr char kTextureFallbackR1[] = ASSETS_SRC_PATH_"/r1.png";
    constexpr char kTextureFallbackRGBA1111[] = ASSETS_SRC_PATH_"/rgba1111.png";
    constexpr char kTextureFallbackRRGGB05051[] = ASSETS_SRC_PATH_"/rrggb05051.png";

    struct TextureInfo {
        std::uint32_t uniqueId;
        std::uint8_t channels;
        std::string newPath;
    };

    void process_model(
        const char* inputObj,
        const char* output,
        const glm::mat4& transform = glm::identity<glm::mat4>()
    );

    InputModel normalize(InputModel);

    void write_model_data(
        FILE* out,
        const InputModel& model,
        const std::vector<IndexedMesh>& indexedMeshes,
        const std::unordered_map<std::string, TextureInfo>& textures);

    std::vector<IndexedMesh> index_meshes(
        const InputModel& model,
        float errorTolerance = 1e-5f
    );

    std::unordered_map<std::string, TextureInfo> find_unique_textures(
        const InputModel&);

    std::unordered_map<std::string, TextureInfo> populate_paths(
        std::unordered_map<std::string, TextureInfo>,
        const std::filesystem::path& textureDir
    );
}


int main() try {
#   ifndef NDEBUG
    std::printf("Suggest running this in release mode (it appears to be running in debug)\n");
    std::printf("Especially under VisualStudio/MSVC, the debug build seems very slow.\n");
    /* A few notes:
     *
     * I have not profiled this at all. The following are based on previous
     * experience(s).
     *
     * - ZStd benefits immensly from compiler optimizations.
     *
     * - Under MSVC, std::unordered_set performs quite badly in debug mode. This
     *   may be further related to other debug-related options (e.g., extended
     *   iterator checking...).
     *
     *   Normally, I avoid unordered_set here, and instead rely on one of the many
     *   high quality flat_set implementations. They tend to be faster from the
     *   get go and perform more equally under different compilers.
     *
     * - NDEBUG is the standard macro to control the behaviour of assert(). When
     *   NDEBUG is defined, assert() will "do nothing" (they're expanded to an
     *   empty statement). This is typically desirable in a release build, but not
     *   necessary or guaranteed. (Indeed, the premake sets NDEBUG explicitly for
     *   this project -- this is why the check above works. But don't rely on this
     *   blindly.)
     *
     * - The VisualStudio interacive debugger's heap profiler (the thing that
     *   shows you tM_FirePit_Inst_Glow_0_Emissive.pnghe memory usage graph) carries a measurable overhead as well.
     *
     * The binary .sc23elssrmesh should be unchanged between debug and release
     * builds, so you can safely use the release build to create the file once,
     * even while debugging the main program.
     */
#	endif
    process_model(
        ASSETS_SRC_PATH_"/suntemple/suntemple.obj-zstd",
        ASSETS_PATH_"/suntemple/suntemple.spicymesh"
    );

    process_model(
        ASSETS_SRC_PATH_"/box/box.obj-zstd",
        ASSETS_PATH_"/box/box.spicymesh"
    );

    process_model(
        ASSETS_SRC_PATH_"/shapes/shapes.obj-zstd",
        ASSETS_PATH_"/shapes/shapes.spicymesh"
    );

    process_model(
        ASSETS_SRC_PATH_"/sponza/sponza_with_ship.obj-zstd",
        ASSETS_PATH_"/sponza/sponza.spicymesh"
    );

    process_model(
        ASSETS_SRC_PATH_"/sphere/sphere.obj-zstd",
        ASSETS_PATH_"/sphere/sphere.spicymesh"
    );

    process_model(
        ASSETS_SRC_PATH_"/bistro/bistro.obj-zstd",
        ASSETS_PATH_"/bistro/bistro.spicymesh"
    );

    return 0;
} catch (std::exception const& eErr) {
    std::fprintf(stderr, "Top-level exception [%s]:\n%s\nBye.\n", typeid(eErr).name(), eErr.what());
    return 1;
}

namespace {
    void process_model(const char* inputObj, const char* output, const glm::mat4& transform) {
        static constexpr std::size_t vertexSize = sizeof(float) * (3 + 3 + 2);

        // Figure out output paths
        const std::filesystem::path outname(output);
        const std::filesystem::path rootdir = outname.parent_path();
        const std::filesystem::path basename = outname.stem();
        const std::filesystem::path textureDir = basename.string() + "-tex";

        // Load input model
        const auto model = normalize(load_compressed_obj(inputObj));

        std::size_t inputVerts = 0;
        for (const auto& mesh : model.meshes) {
            inputVerts += mesh.vertexCount;
        }

        std::printf("%s: %zu meshes, %zu materials\n", inputObj, model.meshes.size(), model.materials.size());
        std::printf(" - triangle soup vertices: %zu => %zu kB\n", inputVerts, inputVerts * vertexSize / 1024);

        // Index meshes
        const auto indexed = index_meshes(model);

        std::size_t outputVerts = 0, outputIndices = 0;
        for (const auto& mesh : indexed) {
            outputVerts += mesh.vertices.size();
            outputIndices += mesh.indices.size();
        }

        std::printf(" - indexed vertices: %zu with %zu indices => %zu kB\n", outputVerts, outputIndices,
                    (outputVerts * vertexSize + outputIndices * sizeof(std::uint32_t)) / 1024);

        // Find list of unique textures
        const auto textures = populate_paths(find_unique_textures(model), textureDir);

        std::printf(" - unique textures: %zu\n", textures.size());

        // Ensure output directory exists
        std::filesystem::create_directories(rootdir);

        // Output mesh data
        auto mainpath = rootdir / basename;
        mainpath.replace_extension("spicymesh");

        FILE* fof = std::fopen(mainpath.string().c_str(), "wb");
        if (!fof)
            throw vkutils::Error("Unable to open '%s' for writing", mainpath.string().c_str());

        try {
            write_model_data(fof, model, indexed, textures);
        } catch (...) {
            std::fclose(fof);
            throw;
        }

        std::fclose(fof);

        // Copy textures
        std::filesystem::create_directories(rootdir / textureDir);

        std::size_t errors = 0;
        for (const auto& textureEntry : textures) {
            const auto dest = rootdir / textureEntry.second.newPath;

            std::error_code errorCode;
            bool ret = std::filesystem::copy_file(
                textureEntry.first,
                dest,
                std::filesystem::copy_options::none,
                errorCode
            );

            if (!ret) {
                ++errors;
                std::fprintf(stderr, "copy_file(): '%s' failed: %s (%s)\n", dest.string().c_str(),
                             errorCode.message().c_str(),
                             errorCode.category().name());
            }
        }

        const auto total = textures.size();
        std::printf("Copied %zu textures out of %zu.\n", total - errors, total);
        if (errors) {
            std::fprintf(
                stderr,
                "Some copies reported an error. Currently, the code will never overwrite existing files. The errors likely just indicate that the file was copied previously. Remove old files manually, if necessary.\n");
        }
    }
}

namespace {
    InputModel normalize(InputModel model) {
        for (auto& material : model.materials) {
            if (material.baseColorTexturePath.empty()) {
                material.baseColorTexturePath = kTextureFallbackRGBA1111;
            }
            if (material.roughnessTexturePath.empty()) {
                material.roughnessTexturePath = kTextureFallbackR1;
            }
            if (material.metalnessTexturePath.empty()) {
                material.metalnessTexturePath = kTextureFallbackR1;
            }
            if (material.normalMapTexturePath.empty()) {
                material.normalMapTexturePath = kTextureFallbackRRGGB05051;
            }
        }

        // This should use the move constructor implicitly
        return model;
    }
}

namespace {
    void checked_write(FILE* out, const std::size_t bytes, const void* data) {
        if (const auto ret = std::fwrite(data, 1, bytes, out);
            ret != bytes) {
            throw vkutils::Error("fwrite() failed: %zu instead of %zu", ret, bytes);
        }
    }

    void write_string(FILE* out, const char* string) {
        // Write a string
        // Format:
        //  - uint32_t : N = length of string in bytes, including terminating '\0'
        //  - N x char : string
        const std::uint32_t length = static_cast<std::uint32_t>(std::strlen(string) + 1);
        checked_write(out, sizeof(std::uint32_t), &length);

        checked_write(out, length, string);
    }

    void write_model_data(FILE* out,
                          const InputModel& model,
                          const std::vector<IndexedMesh>& indexedMeshes,
                          const std::unordered_map<std::string, TextureInfo>& textures) {
        // Write header
        // Format:
        //   - char[16] : file magic
        //   - char[16] : file variant ID
        checked_write(out, sizeof(char) * 16, kFileMagic);
        checked_write(out, sizeof(char) * 16, kFileVariant);

        // Write list of unique textures
        // Format:
        //  - unit32_t : U = number of unique textures
        //  - repeat U times:
        //    - string : path to texture
        //    - uint8_t : number of channels in texture
        std::vector<const TextureInfo*> orderedUnique(textures.size());
        for (const auto& texture : textures) {
            assert(!orderedUnique[texture.second.uniqueId]);
            orderedUnique[texture.second.uniqueId] = &texture.second;
        }

        std::uint32_t const textureCount = static_cast<std::uint32_t>(orderedUnique.size());
        checked_write(out, sizeof(textureCount), &textureCount);

        for (const auto& tex : orderedUnique) {
            assert(tex);
            write_string(out, tex->newPath.c_str());

            std::uint8_t channels = tex->channels;
            checked_write(out, sizeof(channels), &channels);
        }

        // Write material information
        // Format:
        //  - uint32_t : M = number of materials
        //  - repeat M times:
        //    - uin32_t : base color texture index
        //    - uin32_t : roughness texture index
        //    - uin32_t : metalness texture index
        //    - uin32_t : normalMap texture index
        const std::uint32_t materialCount = static_cast<std::uint32_t>(model.materials.size());
        checked_write(out, sizeof(materialCount), &materialCount);

        for (const auto& material : model.materials) {
            write_string(out, material.materialName.c_str());
            checked_write(out, sizeof(glm::vec3), glm::value_ptr(material.baseColor));
            checked_write(out, sizeof(float), &material.baseRoughness);
            checked_write(out, sizeof(float), &material.baseMetalness);

            const auto writeTex = [&](const std::string& rawTexturePath) {
                if (rawTexturePath.empty()) {
                    static constexpr std::uint32_t sentinel = ~static_cast<std::uint32_t>(0);
                    checked_write(out, sizeof(std::uint32_t), &sentinel);
                    return;
                }

                const auto it = textures.find(rawTexturePath);
                assert(textures.end() != it);

                checked_write(out, sizeof(std::uint32_t), &it->second.uniqueId);
            };

            writeTex(material.baseColorTexturePath);
            writeTex(material.roughnessTexturePath);
            writeTex(material.metalnessTexturePath);
            writeTex(material.normalMapTexturePath);
        }

        // Write mesh data
        // Format:
        //  - uint32_t : M = number of meshes
        //  - repeat M times:
        //    - uint32_t : material index
        //    - uint32_t : V = number of vertices
        //    - uint32_t : I = number of indices
        //    - repeat V times: vec3 position
        //    - repeat V times: vec3 normal
        //    - repeat V times: vec2 texture coordinate
        //    - repeat I times: uint32_t index
        const std::uint32_t meshCount = static_cast<std::uint32_t>(model.meshes.size());
        checked_write(out, sizeof(meshCount), &meshCount);

        assert(model.meshes.size() == indexedMeshes.size());
        for (std::size_t i = 0; i < model.meshes.size(); ++i) {
            const auto& modelMesh = model.meshes[i];

            write_string(out, modelMesh.meshName.c_str());

            std::uint32_t materialIndex = static_cast<std::uint32_t>(modelMesh.materialIndex);
            checked_write(out, sizeof(materialIndex), &materialIndex);

            const auto& indexedMesh = indexedMeshes[i];

            std::uint32_t vertexCount = static_cast<std::uint32_t>(indexedMesh.vertices.size());
            checked_write(out, sizeof(vertexCount), &vertexCount);
            std::uint32_t indexCount = static_cast<std::uint32_t>(indexedMesh.indices.size());
            checked_write(out, sizeof(indexCount), &indexCount);

            checked_write(out, sizeof(glm::vec3) * vertexCount, indexedMesh.vertices.data());
            checked_write(out, sizeof(glm::vec3) * vertexCount, indexedMesh.normals.data());
            checked_write(out, sizeof(glm::vec2) * vertexCount, indexedMesh.texcoords.data());
            checked_write(out, sizeof(glm::vec4) * vertexCount, indexedMesh.tangent.data());

            checked_write(out, sizeof(std::uint32_t) * indexCount, indexedMesh.indices.data());
        }
    }
}

namespace {
    std::vector<IndexedMesh> index_meshes(const InputModel& model, float errorTolerance) {
        std::vector<IndexedMesh> indexed;

        for (const auto& mesh : model.meshes) {
            const auto endIndex = mesh.vertexStartIndex + mesh.vertexCount;

            TriangleSoup soup;

            soup.vertices.reserve(mesh.vertexCount);
            for (std::size_t i = mesh.vertexStartIndex; i < endIndex; ++i) {
                soup.vertices.emplace_back(model.positions[i]);
            }

            soup.texcoords.reserve(mesh.vertexCount);
            for (std::size_t i = mesh.vertexStartIndex; i < endIndex; ++i) {
                soup.texcoords.emplace_back(model.texcoords[i]);
            }

            soup.normals.reserve(mesh.vertexCount);
            for (std::size_t i = mesh.vertexStartIndex; i < endIndex; ++i) {
                soup.normals.emplace_back(model.normals[i]);
            }

            indexed.emplace_back(make_indexed_mesh(soup, errorTolerance));
        }

        return indexed;
    }
}

namespace {
    std::unordered_map<std::string, TextureInfo> find_unique_textures(const InputModel& model) {
        std::unordered_map<std::string, TextureInfo> unique;

        std::uint32_t textureId = 0;
        const auto addUnique = [&](const std::string& path, const std::uint8_t channels) {
            if (path.empty()) {
                return;
            }

            const TextureInfo info{
                .uniqueId = textureId,
                .channels = channels
            };

            const auto [_, isNew] = unique.emplace(std::make_pair(path, info));

            if (isNew) {
                ++textureId;
            }
        };

        for (const auto& mat : model.materials) {
            addUnique(mat.baseColorTexturePath, 4); // rgba
            addUnique(mat.roughnessTexturePath, 1); // r
            addUnique(mat.metalnessTexturePath, 1); // M
            addUnique(mat.normalMapTexturePath, 3); // xyz
        }

        return unique;
    }

    std::unordered_map<std::string, TextureInfo> populate_paths(std::unordered_map<std::string, TextureInfo> textures,
                                                                const std::filesystem::path& textureDir) {
        for (auto& entry : textures) {
            const std::filesystem::path originalPath(entry.first);
            const auto filename = originalPath.filename();
            const auto newPath = textureDir / filename;

            auto& textureInfo = entry.second;
            textureInfo.newPath = newPath.string();
        }

        return textures;
    }
}
