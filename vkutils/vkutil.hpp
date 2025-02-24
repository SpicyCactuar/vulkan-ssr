#pragma once

#include <vector>
#include <volk/volk.h>

#include "vkobject.hpp"
#include "vulkan_context.hpp"

namespace vkutils {
    ShaderModule load_shader_module(const VulkanContext&, const char* spirvPath);

    CommandPool create_command_pool(const VulkanContext&, VkCommandPoolCreateFlags = 0);

    VkCommandBuffer alloc_command_buffer(const VulkanContext&, VkCommandPool);

    Fence create_fence(const VulkanContext&, VkFenceCreateFlags = 0);

    Semaphore create_semaphore(const VulkanContext&);

    std::vector<Semaphore> create_semaphores(const VulkanContext&, std::size_t);

    Event create_event(const VulkanContext&, VkEventCreateFlags = 0);

    void buffer_barrier(
        VkCommandBuffer,
        VkBuffer,
        VkAccessFlags srcAccessMask,
        VkAccessFlags dstAccessMask,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask,
        VkDeviceSize size = VK_WHOLE_SIZE,
        VkDeviceSize offset = 0,
        uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);

    DescriptorPool create_descriptor_pool(const VulkanContext&,
                                          std::uint32_t maxDescriptors = 2048,
                                          std::uint32_t maxSets = 1024);

    VkDescriptorSet allocate_descriptor_set(const VulkanContext&, VkDescriptorPool, VkDescriptorSetLayout);

    std::vector<VkDescriptorSet> allocate_descriptor_sets(const VulkanContext& context,
                                                          VkDescriptorPool pool,
                                                          VkDescriptorSetLayout setLayout,
                                                          std::uint32_t count);

    ImageView image_to_view(const VulkanContext&,
                            VkImage image,
                            VkImageViewType type,
                            VkFormat format,
                            VkImageAspectFlags imageAspect);

    void image_barrier(
        VkCommandBuffer commandBuffer, VkImage image,
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        VkImageLayout srcLayout, VkImageLayout dstLayout,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
        const VkImageSubresourceRange& subresourceRange = VkImageSubresourceRange{
            VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
        },
        std::uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        std::uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED
    );

    Sampler create_anisotropy_sampler(const VulkanContext&);

    Sampler create_point_sampler(const VulkanContext&);

    Sampler create_screen_sampler(const VulkanContext&);

    Sampler create_shadow_sampler(const VulkanContext&);
}
