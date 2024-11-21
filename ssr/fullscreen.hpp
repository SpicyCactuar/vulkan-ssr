#pragma once

#include <array>

#include "../vkutils/vulkan_context.hpp"
#include "../vkutils/vulkan_window.hpp"
#include "../vkutils/vkobject.hpp"

#include "scene.hpp"
#include "shade.hpp"
#include "ssr.hpp"

namespace fullscreen {
    vkutils::RenderPass create_render_pass(const vkutils::VulkanWindow& window);

    vkutils::PipelineLayout create_pipeline_layout(const vkutils::VulkanContext& context,
                                                   const vkutils::DescriptorSetLayout& sceneLayout,
                                                   const vkutils::DescriptorSetLayout& shadeLayout,
                                                   const vkutils::DescriptorSetLayout& gbufferLayout,
                                                   const vkutils::DescriptorSetLayout& ssrLayout,
                                                   const vkutils::DescriptorSetLayout& environmentLayout);

    vkutils::Pipeline create_fullscreen_pipeline(const vkutils::VulkanWindow& window,
                                                 VkRenderPass renderPass,
                                                 VkPipelineLayout pipelineLayout);

    void prepare_frame_command_buffer(const vkutils::VulkanWindow& vulkanWindow,
                                      const vkutils::Fence& frameFence,
                                      VkCommandBuffer frameCommandBuffer);

    void record_commands(VkCommandBuffer commandBuffer,
                         VkRenderPass renderPass,
                         VkFramebuffer framebuffer,
                         VkPipelineLayout pipelineLayout,
                         VkPipeline fullscreenPipeline,
                         const VkExtent2D& imageExtent,
                         VkBuffer sceneUBO,
                         const glsl::SceneUniform& sceneUniform,
                         VkDescriptorSet sceneDescriptorSet,
                         VkBuffer shadeUBO,
                         const glsl::ShadeUniform& shadeUniform,
                         VkDescriptorSet shadeDescriptorSet,
                         VkDescriptorSet gbufferDescriptor,
                         VkBuffer ssrUBO,
                         const glsl::SSRUniform& ssrUniform,
                         VkDescriptorSet ssrDescriptorSet,
                         VkDescriptorSet environmentDescriptorSet);

    void submit_frame_command_buffer(const vkutils::VulkanContext& context,
                                     VkCommandBuffer frameCommandBuffer,
                                     const std::array<VkSemaphore, 2>& waitSemaphores,
                                     VkSemaphore signalSemaphore,
                                     const vkutils::Fence& frameFence);
}
