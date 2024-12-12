#include "indexed_mesh.hpp"

#include <algorithm>
#include <unordered_map>

#include <cstddef>
#include <tgen.h>

#include <glm/glm.hpp>

namespace {
    // Tweakables
    constexpr float kAABBMarginFactor = 10.f;
    constexpr std::size_t kSparseGridMaxSize = 1024 * 1024;

    // Discretize mesh positions
    struct DiscretizedPosition {
        std::int32_t x, y, z;
    };

    struct Discretizer {
        Discretizer(std::uint32_t factor, glm::vec3, float);

        DiscretizedPosition discretize(const glm::vec3& position) const;

        glm::vec3 min;
        float scale;
    };

    // hash discretized mesh positions
    using VicinityKey = std::size_t;

    VicinityKey hash_discretized_position(const DiscretizedPosition& position);

    // generate vicinity map
    using VicinityMap = std::unordered_multimap<VicinityKey, std::size_t>;

    void build_vicinity_map(
        VicinityMap&,
        const Discretizer&,
        const std::vector<glm::vec3>&
    );

    bool is_vertex_mergeable(
        const TriangleSoup&,
        std::size_t vertexAIndex, std::size_t vertexBIndex,
        const glm::vec3& vertexAPos, const glm::vec3& vertexBPos,
        float);

    // collapse vertices
    using VertexMapping = std::vector<std::size_t>;
    using IndexBuffer = std::vector<std::uint32_t>;

    std::size_t collapse_vertices(
        IndexBuffer& indices,
        VertexMapping& vertices,
        const VicinityMap& vicinityMap,
        const Discretizer& discretizer,
        const TriangleSoup& soup,
        float errorTolerance);
}

IndexedMesh::IndexedMesh()
    : aabbMin(std::numeric_limits<float>::max()),
      aabbMax(std::numeric_limits<float>::min()) {
}

IndexedMesh make_indexed_mesh(const TriangleSoup& soup, float errorTolerance) {
    // Compute bounding volume
    glm::vec3 bmin(std::numeric_limits<float>::max());
    glm::vec3 bmax(std::numeric_limits<float>::min());

    for (const auto& vertex : soup.vertices) {
        bmin = min(bmin, vertex);
        bmax = max(bmax, vertex);
    }

    const auto fmin = bmin - glm::vec3(kAABBMarginFactor * errorTolerance);
    const auto fmax = bmax + glm::vec3(kAABBMarginFactor * errorTolerance);

    // Compute grid size
    const auto side = fmax - fmin;
    float const maxSide = std::max(side.x, std::max(side.y, side.z));

    float const numCells = maxSide / (2.f * errorTolerance);
    std::size_t subdiv = std::min(kSparseGridMaxSize, static_cast<std::size_t>(numCells + .5f));

    // parameters for discretization
    Discretizer discretizer(static_cast<std::uint32_t>(subdiv), fmin, maxSide);

    // build the vincinity map
    VicinityMap vincinityMap;
    build_vicinity_map(vincinityMap, discretizer, soup.vertices);

    // collapse vertices
    IndexBuffer indices;
    VertexMapping vertexMapping;

    size_t verts = collapse_vertices(indices, vertexMapping, vincinityMap, discretizer, soup, errorTolerance);

    assert(indices.size() == soup.vertices.size());
    assert(verts == vertexMapping.size());

    // shuffle vertex data
    IndexedMesh indexedMesh;

    indexedMesh.vertices.resize(verts);
    indexedMesh.texcoords.resize(verts);

    if (!soup.normals.empty()) {
        indexedMesh.normals.resize(verts);
    }

    for (size_t i = 0; i < verts; ++i) {
        size_t const from = vertexMapping[i];
        assert(from < soup.vertices.size());

        indexedMesh.vertices[i] = soup.vertices[from];
        indexedMesh.texcoords[i] = soup.texcoords[from];

        if (!soup.normals.empty()) {
            indexedMesh.normals[i] = soup.normals[from];
        }
    }

    // move indices
    indexedMesh.indices = std::move(indices);

    // Compute tangents
    const std::vector<tgen::VIndexT> triIndicesPos(indexedMesh.indices.begin(), indexedMesh.indices.end());
    const std::vector<tgen::VIndexT> triIndicesUV(indexedMesh.indices.begin(), indexedMesh.indices.end());
    std::vector<tgen::RealT> positions3D;
    positions3D.reserve(glm::vec3::length() * indexedMesh.vertices.size());
    for (const auto position : indexedMesh.vertices) {
        positions3D.emplace_back(position.x);
        positions3D.emplace_back(position.y);
        positions3D.emplace_back(position.z);
    }
    std::vector<tgen::RealT> uvs2D;
    uvs2D.reserve(glm::vec2::length() * indexedMesh.texcoords.size());
    for (const auto uv : indexedMesh.texcoords) {
        uvs2D.emplace_back(uv.x);
        uvs2D.emplace_back(uv.y);
    }
    std::vector<tgen::RealT> normals3D;
    normals3D.reserve(glm::vec3::length() * indexedMesh.normals.size());
    for (const auto normal : indexedMesh.normals) {
        normals3D.emplace_back(normal.x);
        normals3D.emplace_back(normal.y);
        normals3D.emplace_back(normal.z);
    }
    std::vector<tgen::RealT> cTangents3D;
    std::vector<tgen::RealT> cBitangents3D;
    std::vector<tgen::RealT> vTangents3D;
    std::vector<tgen::RealT> vBitangents3D;
    std::vector<tgen::RealT> tangents4D;

    tgen::computeCornerTSpace(triIndicesPos, triIndicesUV, positions3D, uvs2D, cTangents3D, cBitangents3D);
    tgen::computeVertexTSpace(triIndicesUV, cTangents3D, cBitangents3D, triIndicesUV.size(), vTangents3D,
                              vBitangents3D);
    tgen::orthogonalizeTSpace(normals3D, vTangents3D, vBitangents3D);
    tgen::computeTangent4D(normals3D, vTangents3D, vBitangents3D, tangents4D);

    assert(verts == tangents4D.size() / glm::vec4::length());

    indexedMesh.tangent.reserve(verts);
    for (std::size_t t = 0; t < verts; ++t) {
        indexedMesh.tangent.emplace_back(
            tangents4D[4 * t],
            tangents4D[4 * t + 1],
            tangents4D[4 * t + 2],
            tangents4D[4 * t + 3]
        );
    }

    // meta-data & return
    indexedMesh.aabbMin = bmin;
    indexedMesh.aabbMax = bmax;

    return indexedMesh;
}

namespace {
    Discretizer::Discretizer(const std::uint32_t factor,
                             const glm::vec3 min,
                             const float side): min(min), scale(factor / side) {
    }

    DiscretizedPosition Discretizer::discretize(const glm::vec3& position) const {
        return {
            .x = static_cast<std::int32_t>((position[0] - min[0]) * scale),
            .y = static_cast<std::int32_t>((position[1] - min[1]) * scale),
            .z = static_cast<std::int32_t>((position[2] - min[2]) * scale)
        };
    }
}

namespace {
    std::hash<VicinityKey> scopedHash;

    VicinityKey hash_discretized_position(const DiscretizedPosition& position) {
        // Based on boost::hash_combine
        std::size_t hash = scopedHash(position.x);
        hash ^= scopedHash(position.y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= scopedHash(position.z) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
    }
}

namespace {
    void build_vicinity_map(VicinityMap& map, const Discretizer& discretizer, const std::vector<glm::vec3>& positions) {
        for (std::size_t index = 0; index < positions.size(); ++index) {
            DiscretizedPosition dp = discretizer.discretize(positions[index]);
            VicinityKey vk = hash_discretized_position(dp);

            map.insert(std::make_pair(vk, index));
        }
    }
}

namespace {
    bool is_vertex_mergeable(const TriangleSoup& soup,
                             const size_t vertexAIndex, const size_t vertexBIndex,
                             const glm::vec3& vertexAPos, const glm::vec3& vertexBPos,
                             const float errorTolerance) {
        // Compare all elements component-wise.
        // start with positions, since we've already got those
        for (std::size_t i = 0; i < 3; ++i) {
            if (std::abs(vertexAPos[i] - vertexBPos[i]) > errorTolerance) {
                return false;
            }
        }

        // Compare normals
        if (!soup.normals.empty()) {
            const auto nI = soup.normals[vertexAIndex];
            const auto nJ = soup.normals[vertexBIndex];
            for (size_t i = 0; i < 3; ++i) {
                if (std::abs(nI[i] - nJ[i]) > errorTolerance) {
                    return false;
                }
            }
        }

        // Compare tex coord
        const auto tI = soup.texcoords[vertexAIndex];
        const auto tJ = soup.texcoords[vertexBIndex];
        for (std::size_t i = 0; i < 2; ++i) {
            if (std::abs(tI[i] - tJ[i]) > errorTolerance) {
                return false;
            }
        }

        return true;
    }
}

namespace {
    constexpr size_t kNeighbourCount = 27;

    DiscretizedPosition neighbour(DiscretizedPosition const& position, std::size_t index) {
        static constexpr std::int32_t offset[kNeighbourCount][3]{
            {0, 0, 0}, {0, 0, 1}, {0, 0, -1},
            {0, 1, 0}, {0, 1, 1}, {0, 1, -1},
            {0, -1, 0}, {0, -1, 1}, {0, -1, -1},

            {1, 0, 0}, {1, 0, 1}, {1, 0, -1},
            {1, 1, 0}, {1, 1, 1}, {1, 1, -1},
            {1, -1, 0}, {1, -1, 1}, {1, -1, -1},

            {-1, 0, 0}, {-1, 0, 1}, {-1, 0, -1},
            {-1, 1, 0}, {-1, 1, 1}, {-1, 1, -1},
            {-1, -1, 0}, {-1, -1, 1}, {-1, -1, -1},
        };

        assert(index < kNeighbourCount);

        return {
            .x = position.x + offset[index][0],
            .y = position.y + offset[index][1],
            .z = position.z + offset[index][2]
        };
    }

    size_t collapse_vertices(IndexBuffer& indices,
                             VertexMapping& vertices,
                             const VicinityMap& vicinityMap,
                             const Discretizer& discretizer,
                             const TriangleSoup& soup,
                             const float errorTolerance) {
        vertices.clear();
        vertices.reserve(soup.vertices.size());

        indices.clear();
        indices.reserve(soup.vertices.size());

        // initialize collapse map
        VertexMapping collapseMap(soup.vertices.size());
        std::fill(collapseMap.begin(), collapseMap.end(), ~static_cast<std::size_t>(0));

        // process vertices
        std::size_t nextVertex = 0;
        for (std::size_t i = 0; i < soup.vertices.size(); ++i) {
            // check if this vertex already was merged somewhere
            if (~static_cast<size_t>(0) != collapseMap[i]) {
                assert(collapseMap[i] < vertices.size());
                indices.push_back(static_cast<std::uint32_t>(collapseMap[i]));
                continue;
            }

            // get position and look for possible neighbours
            const auto self = soup.vertices[i];
            DiscretizedPosition const dp = discretizer.discretize(self);

            bool merged = false;
            std::size_t target = ~static_cast<std::size_t>(0);

            for (std::size_t j = 0; j < kNeighbourCount; ++j) {
                DiscretizedPosition const dq = neighbour(dp, j);
                VicinityKey const vk = hash_discretized_position(dq);

                // get vertices in this bucket
                for (auto [it, jt] = vicinityMap.equal_range(vk); it != jt; ++it) {
                    std::size_t const idx = it->second;

                    if (idx == i) {
                        // don't try to merge with self
                        continue;
                    }

                    if (~static_cast<std::size_t>(0) != collapseMap[idx]) {
                        // don't remerge
                        continue;
                    }

                    if (const auto other = soup.vertices[idx];
                        is_vertex_mergeable(soup, i, idx, self, other, errorTolerance)) {
                        std::size_t toWhere;

                        if (merged) {
                            toWhere = target;
                        } else {
                            toWhere = nextVertex++;
                            vertices.push_back(i);

                            collapseMap[i] = toWhere;
                            indices.push_back(static_cast<std::uint32_t>(toWhere));
                        }

                        collapseMap[idx] = toWhere;

                        target = toWhere;
                        merged = true;
                    }
                }
            }

            if (!merged) {
                const std::size_t toWhere = nextVertex++;

                collapseMap[i] = toWhere;
                vertices.push_back(i);
                indices.push_back(static_cast<std::uint32_t>(toWhere));
            }
        }

        return nextVertex;
    }
}
