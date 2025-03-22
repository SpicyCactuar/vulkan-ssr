#pragma once

#include <optional>

#include "../vkutils/vkimage.hpp"
#include "../vkutils/vkutil.hpp"
#include "../vkutils/vulkan_context.hpp"

#include "baked_model.hpp"
#include "material.hpp"

namespace glsl {
    struct MaterialPushConstants {
        glm::vec3 baseColour;
        float roughness;
        glm::vec3 emission;
        float metalness;
    };

    static_assert(sizeof(MaterialPushConstants) <= 65536,
                  "MaterialPushConstants must be less than 65536 bytes for vkCmdUpdateBuffer");
    static_assert(sizeof(MaterialPushConstants) % 4 == 0, "MaterialPushConstants size must be a multiple of 4 bytes");
    static_assert(offsetof(MaterialPushConstants, baseColour) % 16 == 0, "baseColour must be aligned to 16 bytes");
    static_assert(offsetof(MaterialPushConstants, roughness) % 4 == 0, "roughness must be aligned to 4 bytes");
    static_assert(offsetof(MaterialPushConstants, emission) % 16 == 0, "emission must be aligned to 4 bytes");
    static_assert(offsetof(MaterialPushConstants, metalness) % 4 == 0, "metalness must be aligned to 4 bytes");
}

namespace material {
    struct Material {
        std::string name;

        glsl::MaterialPushConstants pushConstants;

        vkutils::ImageView baseColour;
        vkutils::ImageView emissive;
        vkutils::ImageView roughness;
        vkutils::ImageView metalness;
        vkutils::ImageView normalMap;
        std::optional<vkutils::ImageView> alphaMask;

        bool has_alpha_mask() const {
            return alphaMask.has_value();
        }

        static constexpr VkFormat COLOUR_FORMAT = VK_FORMAT_R8G8B8A8_SRGB;
        static constexpr VkFormat LINEAR_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
    };

    struct MaterialStore {
        std::vector<vkutils::Image> textures;
        std::vector<Material> materials;
    };

    MaterialStore extract_materials(const baked::BakedModel& model,
                                    const vkutils::VulkanContext& context,
                                    const vkutils::Allocator& allocator);

    vkutils::DescriptorSetLayout create_descriptor_layout(const vkutils::VulkanContext&);

    void update_descriptor_set(const vkutils::VulkanContext& context,
                               VkDescriptorSet materialDescriptorSet,
                               const material::Material& material,
                               const vkutils::Sampler& anisotropySampler,
                               const vkutils::Sampler& pointSampler);
}
