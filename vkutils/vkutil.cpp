#include "vkutil.hpp"

#include <array>
#include <vector>

#include <cstdio>
#include <cassert>
#include <iostream>

#include "error.hpp"
#include "to_string.hpp"
#include "vkobject.hpp"

namespace vkutils {
    ShaderModule load_shader_module(const VulkanContext& context, const char* spirvPath) {
        assert(spirvPath);

        std::FILE* fin = std::fopen(spirvPath, "rb");
        if (fin == nullptr) {
            // Early throw if file cannot be opened
            throw Error("Cannont open ’%s’ for reading", spirvPath);
        }

        // File was opened successfully
        std::fseek(fin, 0, SEEK_END);
        const auto bytes = static_cast<std::size_t>(std::ftell(fin));
        std::fseek(fin, 0, SEEK_SET);

        // SPIR-V consists of a number of 32-bit = 4 byte words
        assert(0 == bytes % 4);
        const auto words = bytes / 4;

        std::vector<std::uint32_t> code(words);

        std::size_t offset = 0;
        while (offset != words) {
            const auto read = std::fread(code.data() + offset, sizeof(std::uint32_t), words - offset, fin);

            if (0 == read) {
                std::fclose(fin);

                throw Error("Error reading ’%s’: ferror = %d, feof = %d",
                            spirvPath,
                            std::ferror(fin),
                            std::feof(fin));
            }
            offset += read;
        }

        std::fclose(fin);

        const VkShaderModuleCreateInfo moduleInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = bytes,
            .pCode = code.data()
        };

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        if (const auto res = vkCreateShaderModule(context.device, &moduleInfo, nullptr, &shaderModule);
            VK_SUCCESS != res) {
            throw Error("Unable to create shader module from %s\n"
                        "vkCreateShaderModule() returned %s", spirvPath, to_string(res).c_str()
            );
        }

        return ShaderModule(context.device, shaderModule);
    }

    CommandPool create_command_pool(const VulkanContext& context, const VkCommandPoolCreateFlags flags) {
        const VkCommandPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = flags,
            .queueFamilyIndex = context.graphicsFamilyIndex
        };

        VkCommandPool cpool = VK_NULL_HANDLE;
        if (const auto res = vkCreateCommandPool(context.device, &poolInfo, nullptr, &cpool);
            VK_SUCCESS != res) {
            throw Error("Unable to create command pool\n"
                        "vkCreateCommandPool() returned %s", to_string(res).c_str());
        }
        return CommandPool(context.device, cpool);
    }

    VkCommandBuffer alloc_command_buffer(const VulkanContext& context, const VkCommandPool commandPool) {
        const VkCommandBufferAllocateInfo commandBufferInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (const auto res = vkAllocateCommandBuffers(context.device, &commandBufferInfo, &commandBuffer);
            VK_SUCCESS != res) {
            throw Error("Unable to allocate command buffer\n"
                        "vkAllocateCommandBuffers() returned %s", to_string(res).c_str()
            );
        }

        return commandBuffer;
    }

    Fence create_fence(const VulkanContext& context, const VkFenceCreateFlags flags) {
        const VkFenceCreateInfo fenceInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = flags
        };

        VkFence fence = VK_NULL_HANDLE;
        if (const auto res = vkCreateFence(context.device, &fenceInfo, nullptr, &fence);
            VK_SUCCESS != res) {
            throw Error("Unable to create fence\n"
                        "vkCreateFence() returned %s", to_string(res).c_str());
        }

        return Fence(context.device, fence);
    }

    Semaphore create_semaphore(const VulkanContext& context) {
        constexpr VkSemaphoreCreateInfo semaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };

        VkSemaphore semaphore = VK_NULL_HANDLE;
        if (const auto res = vkCreateSemaphore(context.device, &semaphoreInfo, nullptr, &semaphore);
            VK_SUCCESS != res) {
            throw Error("Unable to create semaphore\n"
                        "vkCreateSemaphore() returned %s", to_string(res).c_str()
            );
        }

        return Semaphore(context.device, semaphore);
    }

    Event create_event(const VulkanContext& context, const VkEventCreateFlags flags) {
        const VkEventCreateInfo eventCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
            .flags = flags
        };

        VkEvent event = VK_NULL_HANDLE;
        if (const auto res = vkCreateEvent(context.device, &eventCreateInfo, nullptr, &event);
            res != VK_SUCCESS) {
            throw Error("Unable to create event\n"
                        "vkCreateEvent() returned %s", to_string(res).c_str()
            );
        }

        return Event(context.device, event);
    }

    void buffer_barrier(const VkCommandBuffer commandBuffer,
                        const VkBuffer buffer,
                        const VkAccessFlags srcAccessMask, const VkAccessFlags dstAccessMask,
                        const VkPipelineStageFlags srcStageMask, const VkPipelineStageFlags dstStageMask,
                        const VkDeviceSize size, const VkDeviceSize offset,
                        const uint32_t srcQueueFamilyIndex, const uint32_t dstQueueFamilyIndex) {
        const VkBufferMemoryBarrier bufferBarrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = srcAccessMask,
            .dstAccessMask = dstAccessMask,
            .srcQueueFamilyIndex = srcQueueFamilyIndex,
            .dstQueueFamilyIndex = dstQueueFamilyIndex,
            .buffer = buffer,
            .offset = offset,
            .size = size
        };

        vkCmdPipelineBarrier(
            commandBuffer,
            srcStageMask,
            dstStageMask,
            0,
            0, nullptr,
            1, &bufferBarrier,
            0, nullptr
        );
    }

    DescriptorPool create_descriptor_pool(const VulkanContext& context,
                                          const std::uint32_t maxDescriptors,
                                          const std::uint32_t maxSets) {
        const std::array pools{
            VkDescriptorPoolSize{
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = maxDescriptors
            },
            VkDescriptorPoolSize{
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = maxDescriptors
            }
        };

        const VkDescriptorPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = maxSets,
            .poolSizeCount = pools.size(),
            .pPoolSizes = pools.data()
        };

        VkDescriptorPool pool = VK_NULL_HANDLE;
        if (const auto res = vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &pool);
            VK_SUCCESS != res) {
            throw Error("Unable to create descriptor pool\n"
                        "vkCreateDescriptorPool() returned %s", to_string(res).c_str()
            );
        }

        return DescriptorPool(context.device, pool);
    }

    VkDescriptorSet allocate_descriptor_set(const VulkanContext& context,
                                            const VkDescriptorPool pool,
                                            const VkDescriptorSetLayout setLayout) {
        const VkDescriptorSetAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &setLayout
        };

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        if (const auto res = vkAllocateDescriptorSets(context.device, &allocInfo, &descriptorSet);
            VK_SUCCESS != res) {
            throw Error("Unable to allocate descriptor set\n"
                        "vkAllocateDescriptorSets() returned %s", to_string(res).c_str()
            );
        }

        return descriptorSet;
    }

    std::vector<VkDescriptorSet> allocate_descriptor_sets(const VulkanContext& context,
                                                          const VkDescriptorPool pool,
                                                          const VkDescriptorSetLayout setLayout,
                                                          const std::uint32_t count) {
        const std::vector setLayouts(count, setLayout);
        std::vector<VkDescriptorSet> descriptorSets(count);

        const VkDescriptorSetAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = pool,
            .descriptorSetCount = count,
            .pSetLayouts = setLayouts.data()
        };

        if (const auto res = vkAllocateDescriptorSets(context.device, &allocInfo, descriptorSets.data());
            VK_SUCCESS != res) {
            throw Error("Unable to allocate descriptor set\n"
                        "vkAllocateDescriptorSets() returned %s", to_string(res).c_str()
            );
        }

        return descriptorSets;
    }

    ImageView image_to_view(const VulkanContext& context,
                            const VkImage image,
                            const VkImageViewType type,
                            const VkFormat format,
                            const VkImageAspectFlags imageAspect) {
        const VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = type,
            .format = format,
            .components = VkComponentMapping{}, // == identity
            .subresourceRange = VkImageSubresourceRange{
                imageAspect,
                0, VK_REMAINING_MIP_LEVELS,
                0, VK_REMAINING_ARRAY_LAYERS
            }
        };

        VkImageView view = VK_NULL_HANDLE;
        if (const auto res = vkCreateImageView(context.device, &viewInfo, nullptr, &view);
            VK_SUCCESS != res) {
            throw Error("Unable to create image view\n"
                        "vkCreateImageView() returned %s", to_string(res).c_str()
            );
        }

        return ImageView(context.device, view);
    }

    void image_barrier(const VkCommandBuffer commandBuffer, const VkImage image,
                       const VkAccessFlags srcAccessMask, const VkAccessFlags dstAccessMask,
                       const VkImageLayout srcLayout, const VkImageLayout dstLayout,
                       const VkPipelineStageFlags srcStageMask, const VkPipelineStageFlags dstStageMask,
                       const VkImageSubresourceRange& subresourceRange,
                       const std::uint32_t srcQueueFamilyIndex, const std::uint32_t dstQueueFamilyIndex) {
        const VkImageMemoryBarrier imageBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = srcAccessMask,
            .dstAccessMask = dstAccessMask,
            .oldLayout = srcLayout,
            .newLayout = dstLayout,
            .srcQueueFamilyIndex = srcQueueFamilyIndex,
            .dstQueueFamilyIndex = dstQueueFamilyIndex,
            .image = image,
            .subresourceRange = subresourceRange
        };

        vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0,
                             0, nullptr,
                             0, nullptr,
                             1, &imageBarrier);
    }

    Sampler create_anisotropy_sampler(const VulkanContext& context) {
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(context.physicalDevice, &deviceFeatures);

        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(context.physicalDevice, &deviceProperties);

        if (deviceFeatures.samplerAnisotropy) {
            std::cout << "Sampling with Anisotropic filtering" <<
                    " | Max samples = " << deviceProperties.limits.maxSamplerAnisotropy <<
                    "\n";
        }

        const VkSamplerCreateInfo samplerInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias = 0.0f,
            .anisotropyEnable = deviceFeatures.samplerAnisotropy,
            .maxAnisotropy = deviceFeatures.samplerAnisotropy
                                 ? deviceProperties.limits.maxSamplerAnisotropy
                                 : 0.0f,
            .minLod = 0.0f,
            .maxLod = VK_LOD_CLAMP_NONE
        };

        VkSampler sampler = VK_NULL_HANDLE;
        if (const auto res = vkCreateSampler(context.device, &samplerInfo, nullptr, &sampler);
            VK_SUCCESS != res) {
            throw Error("Unable to create sampler\n"
                        "vkCreateSampler() returned %s", to_string(res).c_str()
            );
        }

        return Sampler(context.device, sampler);
    }

    Sampler create_point_sampler(const VulkanContext& context) {
        constexpr VkSamplerCreateInfo samplerInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias = 0.0f,
            .minLod = 0.0f,
            .maxLod = VK_LOD_CLAMP_NONE
        };

        VkSampler sampler = VK_NULL_HANDLE;
        if (const auto res = vkCreateSampler(context.device, &samplerInfo, nullptr, &sampler);
            VK_SUCCESS != res) {
            throw Error("Unable to create sampler\n"
                        "vkCreateSampler() returned %s", to_string(res).c_str()
            );
        }

        return Sampler(context.device, sampler);
    }

    Sampler create_screen_sampler(const VulkanContext& context) {
        constexpr VkSamplerCreateInfo samplerInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .mipLodBias = 0.0f,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK
        };

        VkSampler sampler = VK_NULL_HANDLE;
        if (const auto res = vkCreateSampler(context.device, &samplerInfo, nullptr, &sampler);
            VK_SUCCESS != res) {
            throw Error("Unable to create sampler\n"
                        "vkCreateSampler() returned %s", to_string(res).c_str()
            );
        }

        return Sampler(context.device, sampler);
    }

    Sampler create_shadow_sampler(const VulkanContext& context) {
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(context.physicalDevice, &deviceFeatures);

        constexpr VkSamplerCreateInfo samplerInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .mipLodBias = 0.0f,
            .compareEnable = VK_TRUE,
            .compareOp = VK_COMPARE_OP_LESS,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
        };

        VkSampler sampler = VK_NULL_HANDLE;
        if (const auto res = vkCreateSampler(context.device, &samplerInfo, nullptr, &sampler);
            VK_SUCCESS != res) {
            throw Error("Unable to create sampler\n"
                        "vkCreateSampler() returned %s", to_string(res).c_str()
            );
        }

        return Sampler(context.device, sampler);
    }
}
