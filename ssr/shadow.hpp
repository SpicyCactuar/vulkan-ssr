#pragma once

#include <utility>

#include "../vkutils/vkobject.hpp"
#include "../vkutils/vkimage.hpp"
#include "../vkutils/vulkan_window.hpp"

#include "mesh.hpp"
#include "scene.hpp"

namespace shadow {
    constexpr VkFormat shadowFormat = VK_FORMAT_D32_SFLOAT;
    constexpr VkExtent2D shadowMapExtent{.width = 2048, .height = 2048};

    // Bias matrix to transform coordinates from [-1, 1] to [0, 1]
    // Only (x, y) is shifted and scaled
    // textureProj uses position_lcs.zw as-is for depth comparison and perspective divide respectively
    constexpr glm::mat4 shadowTransformationMatrix{
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f
    };

    vkutils::RenderPass create_render_pass(const vkutils::VulkanWindow& window);

    vkutils::PipelineLayout create_opaque_pipeline_layout(const vkutils::VulkanContext& context,
                                                          const vkutils::DescriptorSetLayout& sceneLayout);

    vkutils::Pipeline create_opaque_pipeline(const vkutils::VulkanWindow& window,
                                             VkRenderPass renderPass,
                                             VkPipelineLayout pipelineLayout);

    vkutils::PipelineLayout create_alpha_pipeline_layout(const vkutils::VulkanContext& context,
                                                         const vkutils::DescriptorSetLayout& sceneLayout,
                                                         const vkutils::DescriptorSetLayout& materialLayout);

    vkutils::Pipeline create_alpha_pipeline(const vkutils::VulkanWindow& window,
                                            VkRenderPass renderPass,
                                            VkPipelineLayout pipelineLayout);

    std::pair<vkutils::Image, vkutils::ImageView> create_shadow_buffer(const vkutils::VulkanWindow&,
                                                                       const vkutils::Allocator&);

    vkutils::Framebuffer create_shadow_framebuffer(const vkutils::VulkanWindow& window,
                                                   VkRenderPass shadowRenderPass,
                                                   VkImageView shadowView);

    void record_commands(VkCommandBuffer commandBuffer,
                         VkRenderPass renderPass,
                         VkFramebuffer framebuffer,
                         VkPipelineLayout opaqueLayout,
                         VkPipeline opaquePipeline,
                         VkPipelineLayout alphaLayout,
                         VkPipeline alphaPipeline,
                         VkBuffer sceneUBO,
                         const glsl::SceneUniform& sceneUniform,
                         VkDescriptorSet sceneDescriptors,
                         const std::vector<mesh::Mesh>& opaqueMeshes,
                         const std::vector<mesh::Mesh>& alphaMeshes,
                         const std::vector<VkDescriptorSet>& materialDescriptors);
}
