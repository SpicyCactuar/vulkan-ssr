#pragma once

#include <string>

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "../vkutils/vkbuffer.hpp"
#include "../vkutils/allocator.hpp"
#include "../vkutils/vkobject.hpp"
#include "../vkutils/vulkan_context.hpp"

#include "state.hpp"

// GLSL
namespace glsl {
    struct SceneUniform {
        glm::mat4 V;
        glm::mat4 P;
        glm::mat4 VP;
        glm::mat4 LVP;
        glm::mat4 SLVP;
        glm::mat4 WP;
        glm::mat4 iP;
        glm::mat4 C;
    };

    // We want to use vkCmdUpdateBuffer() to update the contents of our uniform buffers. vkCmdUpdateBuffer()
    // has a number of requirements, including the two below. See
    // https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/vkCmdUpdateBuffer.html
    static_assert(sizeof(SceneUniform) <= 65536,
                  "SceneUniform must be less than 65536 bytes for vkCmdUpdateBuffer");
    static_assert(sizeof(SceneUniform) % 4 == 0, "SceneUniform size must be a multiple of 4 bytes");
}

// Scene data
namespace scene {
    vkutils::DescriptorSetLayout create_descriptor_layout(const vkutils::VulkanContext& context);

    vkutils::Buffer create_scene_ubo(const vkutils::Allocator& allocator);

    void update_descriptor_set(const vkutils::VulkanContext& context,
                               const vkutils::Buffer& sceneUBO,
                               VkDescriptorSet sceneDescriptorSet);

    glsl::SceneUniform create_uniform(std::uint32_t framebufferWidth,
                                      std::uint32_t framebufferHeight,
                                      const state::State& state);

    void update_scene_ubo(VkCommandBuffer commandBuffer,
                          VkBuffer sceneUBO,
                          const glsl::SceneUniform& sceneUniform);
}
