#pragma once

#include <cstdint>

#include "../vkutils/allocator.hpp"
#include "../vkutils/vkbuffer.hpp"
#include "../vkutils/vkobject.hpp"
#include "../vkutils/vulkan_context.hpp"

#include "state.hpp"

namespace glsl {
    struct SSRUniform {
        std::uint32_t mode;
        float reflectivityThreshold;
        std::uint32_t traversalScheme;
        std::uint32_t maxSteps;
        float stride;
        std::uint32_t binaryRefinementSteps;
        float thickness;
    };

    static_assert(sizeof(SSRUniform) <= 65536, "SSRUniform must be less than 65536 bytes for vkCmdUpdateBuffer");
    static_assert(sizeof(SSRUniform) % 4 == 0, "SSRUniform size must be a multiple of 4 bytes");
    static_assert(offsetof(SSRUniform, mode) % 4 == 0, "mode must be aligned to 4 bytes");
    static_assert(offsetof(SSRUniform, reflectivityThreshold) % 4 == 0, "reflectivityThreshold must be aligned to 4 bytes");
    static_assert(offsetof(SSRUniform, traversalScheme) % 4 == 0, "traversalScheme must be aligned to 4 bytes");
    static_assert(offsetof(SSRUniform, maxSteps) % 4 == 0, "maxSteps must be aligned to 4 bytes");
    static_assert(offsetof(SSRUniform, stride) % 4 == 0, "stride must be aligned to 4 bytes");
    static_assert(offsetof(SSRUniform, binaryRefinementSteps) % 4 == 0, "binaryRefinementSteps must be aligned to 4 bytes");
    static_assert(offsetof(SSRUniform, thickness) % 4 == 0, "thickness must be aligned to 4 bytes");
}

namespace ssr {
    vkutils::DescriptorSetLayout create_descriptor_layout(const vkutils::VulkanContext& context);

    vkutils::Buffer create_ssr_ubo(const vkutils::Allocator& allocator);

    void update_descriptor_set(const vkutils::VulkanContext& context,
                               const vkutils::Buffer& ssrUBO,
                               VkDescriptorSet ssrDescriptorSet);

    glsl::SSRUniform create_uniform(const state::State& state);

    void update_ssr_ubo(VkCommandBuffer commandBuffer,
                        VkBuffer ssrUBO,
                        const glsl::SSRUniform& ssrUniform);
}