#pragma once

#include <glm/glm.hpp>

#include "../vkutils/allocator.hpp"
#include "../vkutils/vkbuffer.hpp"
#include "../vkutils/vkobject.hpp"
#include "../vkutils/vulkan_context.hpp"

#include "state.hpp"

namespace glsl {
    struct alignas(16) CameraUniform {
        float near;
        float far;
        alignas(16) glm::vec4 position;
    };

    // We want to use vkCmdUpdateBuffer() to update the contents of our uniform buffers. vkCmdUpdateBuffer()
    // has a number of requirements, including the two below. See
    // https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdUpdateBuffer.html
    static_assert(sizeof(CameraUniform) <= 65536, "CameraUniform must be less than 65536 bytes for vkCmdUpdateBuffer");
    static_assert(sizeof(CameraUniform) % 4 == 0, "CameraUniform size must be a multiple of 4 bytes");
    static_assert(offsetof(CameraUniform, near) % 4 == 0, "near must be aligned to 4 bytes");
    static_assert(offsetof(CameraUniform, far) % 4 == 0, "far must be aligned to 4 bytes");
    static_assert(offsetof(CameraUniform, position) % 16 == 0, "position must be aligned to 16 bytes");

    struct PointLightUniform {
        glm::vec4 position;
        glm::vec4 colour;
    };

    static_assert(sizeof(PointLightUniform) <= 65536,
                  "PointLightUniform must be less than 65536 bytes for vkCmdUpdateBuffer");
    static_assert(sizeof(PointLightUniform) % 4 == 0, "PointLightUniform size must be a multiple of 4 bytes");
    static_assert(offsetof(PointLightUniform, position) % 16 == 0, "position must be aligned to 16 bytes");
    static_assert(offsetof(PointLightUniform, colour) % 16 == 0, "colour must be aligned to 16 bytes");

    struct ShadeUniform {
        std::uint32_t visualisationMode;
        std::uint32_t pbrTerm;
        std::uint32_t detailsMask;
        alignas(16) glm::vec4 ambient;
        CameraUniform camera;
        PointLightUniform light;
    };

    static_assert(sizeof(ShadeUniform) <= 65536, "ShadeUniform must be less than 65536 bytes for vkCmdUpdateBuffer");
    static_assert(sizeof(ShadeUniform) % 4 == 0, "ShadeUniform size must be a multiple of 4 bytes");
    static_assert(offsetof(ShadeUniform, visualisationMode) % 4 == 0, "visualisationMode must be aligned to 4 bytes");
    static_assert(offsetof(ShadeUniform, pbrTerm) % 4 == 0, "pbrTerm must be aligned to 4 bytes");
    static_assert(offsetof(ShadeUniform, detailsMask) % 4 == 0, "detailsMask must be aligned to 4 bytes");
    static_assert(offsetof(ShadeUniform, ambient) % 16 == 0, "ambient must be aligned to 16 bytes");
    static_assert(offsetof(ShadeUniform, camera) % 16 == 0, "ambient must be aligned to 16 bytes");
    static_assert(offsetof(ShadeUniform, light) % 16 == 0, "light must be aligned to 16 bytes");
}

namespace shade {
    vkutils::DescriptorSetLayout create_descriptor_layout(const vkutils::VulkanContext& context);

    vkutils::Buffer create_shade_ubo(const vkutils::Allocator& allocator);

    void update_descriptor_set(const vkutils::VulkanContext& context,
                               const vkutils::Buffer& shadeUBO,
                               VkDescriptorSet shadeDescriptorSet,
                               const vkutils::Sampler& shadowSampler,
                               VkImageView shadowView);

    glsl::ShadeUniform create_uniform(const state::State& state);

    void update_shade_ubo(VkCommandBuffer commandBuffer,
                          VkBuffer shadeUBO,
                          const glsl::ShadeUniform& shadeUniform);
}
