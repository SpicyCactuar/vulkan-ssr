#pragma once

#include <cstdint>
#include <vector>

#include "../vkutils/allocator.hpp"
#include "../vkutils/vkbuffer.hpp"
#include "../vkutils/vulkan_context.hpp"

#include "baked_model.hpp"

namespace mesh {
    struct Mesh {
        std::string name;

        vkutils::Buffer positions;
        vkutils::Buffer uvs;
        vkutils::Buffer normals;
        vkutils::Buffer tangents;
        vkutils::Buffer indices;
        std::uint32_t materialId;

        std::uint32_t indexCount;
    };

    std::vector<Mesh> extract_meshes(const vkutils::VulkanContext&,
                                     const vkutils::Allocator&,
                                     const baked::BakedModel& model);
}
