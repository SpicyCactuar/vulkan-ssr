#pragma once

#include "../vkutils/vkobject.hpp"
#include "../vkutils/vulkan_window.hpp"

#include "gbuffer.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "scene.hpp"
#include "shade.hpp"

namespace offscreen {
    vkutils::RenderPass create_render_pass(const vkutils::VulkanWindow& window);

    vkutils::PipelineLayout create_pipeline_layout(const vkutils::VulkanContext& context,
                                                   const vkutils::DescriptorSetLayout& sceneLayout,
                                                   const vkutils::DescriptorSetLayout& shadeLayout,
                                                   const vkutils::DescriptorSetLayout& materialLayout);

    vkutils::Pipeline create_pipeline(const vkutils::VulkanWindow& window,
                                      VkRenderPass renderPass,
                                      VkPipelineLayout pipelineLayout);

    vkutils::Framebuffer create_offscreen_framebuffer(const vkutils::VulkanWindow& window,
                                                      VkRenderPass renderPass,
                                                      const gbuffer::GBuffer& gBuffer);

    void prepare_offscreen_command_buffer(const vkutils::VulkanContext& context,
                                          const vkutils::Fence& offscreenFence,
                                          VkCommandBuffer offscreenCommandBuffer);

    void record_commands(VkCommandBuffer commandBuffer,
                         VkRenderPass renderPass,
                         VkFramebuffer framebuffer,
                         VkPipelineLayout offscreenPipelineLayout,
                         VkPipeline offscreenPipeline,
                         const VkExtent2D& imageExtent,
                         VkBuffer sceneUBO,
                         const glsl::SceneUniform& sceneUniform,
                         VkDescriptorSet sceneDescriptors,
                         VkBuffer shadeUBO,
                         const glsl::ShadeUniform& shadeUniform,
                         VkDescriptorSet screenDescriptors,
                         const std::vector<mesh::Mesh>& meshes,
                         const std::vector<material::Material>& materials,
                         const std::vector<VkDescriptorSet>& materialDescriptorSets);

    void submit_commands(const vkutils::VulkanContext& context,
                         VkCommandBuffer offscreenCommandBuffer,
                         const vkutils::Semaphore& signalSemaphore,
                         const vkutils::Fence& offscreenFence);

    void wait_offscreen_early(const vkutils::VulkanWindow& vulkanWindow,
                              const vkutils::Semaphore& waitSemaphore);
}
