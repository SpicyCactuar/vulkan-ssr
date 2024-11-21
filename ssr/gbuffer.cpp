#include "gbuffer.hpp"

#include <utility>
#include <array>

#include "../vkutils/error.hpp"
#include "../vkutils/vkutil.hpp"
#include "../vkutils/to_string.hpp"

#include "config.hpp"

namespace gbuffer {
    GBuffer::GBuffer(const vkutils::VulkanWindow& window,
                     const vkutils::Allocator& allocator) {
        const auto [windowWidth, windowHeight] = window.swapchainExtent;

        // Create depth buffer
        auto depthImage = vkutils::create_image(allocator, gbuffer::depthFormat, VK_IMAGE_TYPE_2D,
                                                windowWidth, windowHeight, 1,
                                                1,
                                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                                VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
        auto depthView = vkutils::image_to_view(window, depthImage.image, VK_IMAGE_VIEW_TYPE_2D,
                                                gbuffer::depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

        // Create normal buffer
        auto normalImage = vkutils::create_image(allocator, gbuffer::normalFormat, VK_IMAGE_TYPE_2D,
                                                 windowWidth, windowHeight, 1,
                                                 1,
                                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                 VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

        auto normalView = vkutils::image_to_view(window, normalImage.image, VK_IMAGE_VIEW_TYPE_2D,
                                                 gbuffer::normalFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        // Create base colour buffer
        auto baseColourImage = vkutils::create_image(allocator, gbuffer::baseColourFormat, VK_IMAGE_TYPE_2D,
                                                     windowWidth, windowHeight, 1,
                                                     1,
                                                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                     VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

        auto baseColourView = vkutils::image_to_view(window, baseColourImage.image, VK_IMAGE_VIEW_TYPE_2D,
                                                     gbuffer::baseColourFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        // Create surface buffer
        auto surfaceImage = vkutils::create_image(allocator, gbuffer::surfaceFormat, VK_IMAGE_TYPE_2D,
                                                  windowWidth, windowHeight, 1,
                                                  1,
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                                  VMA_MEMORY_USAGE_GPU_ONLY);

        auto surfaceView = vkutils::image_to_view(window, surfaceImage.image, VK_IMAGE_VIEW_TYPE_2D,
                                                  gbuffer::surfaceFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        this->depth = {std::move(depthImage), std::move(depthView)};
        this->normal = {std::move(normalImage), std::move(normalView)};
        this->baseColour = {std::move(baseColourImage), std::move(baseColourView)};
        this->surface = {std::move(surfaceImage), std::move(surfaceView)};
    }

    GBuffer::GBuffer(GBuffer&& other) noexcept : depth(std::exchange(other.depth, {})),
                                                 normal(std::exchange(other.normal, {})),
                                                 baseColour(std::exchange(other.baseColour, {})),
                                                 surface(std::exchange(other.surface, {})) {
    }

    GBuffer& GBuffer::operator=(GBuffer&& other) noexcept {
        if (this != &other) {
            std::swap(depth, other.depth);
            std::swap(normal, other.normal);
            std::swap(baseColour, other.baseColour);
            std::swap(surface, other.surface);
        }
        return *this;
    }

    vkutils::DescriptorSetLayout create_descriptor_layout(const vkutils::VulkanContext& context) {
        constexpr std::array bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0, // layout(set = ..., binding = 0)
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 1, // layout(set = ..., binding = 1)
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 2, // layout(set = ..., binding = 2)
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 3, // layout(set = ..., binding = 3)
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            }
        };

        const VkDescriptorSetLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = bindings.size(),
            .pBindings = bindings.data()
        };

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        if (const auto res = vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &layout);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to create gbuffer descriptor set layout\n"
                                 "vkCreateDescriptorSetLayout() returned %s", vkutils::to_string(res).c_str()
            );
        }

        return vkutils::DescriptorSetLayout(context.device, layout);
    }

    void update_descriptor_set(const vkutils::VulkanContext& context,
                               VkDescriptorSet gbufferDescriptorSet,
                               const vkutils::Sampler& screenSampler,
                               const gbuffer::GBuffer& gBuffer) {
        const VkDescriptorImageInfo depthDescriptorInfo{
            .sampler = screenSampler.handle,
            .imageView = gBuffer.depth.second.handle,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        };
        const VkDescriptorImageInfo normalDescriptorInfo{
            .sampler = screenSampler.handle,
            .imageView = gBuffer.normal.second.handle,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkDescriptorImageInfo baseColourDescriptorInfo{
            .sampler = screenSampler.handle,
            .imageView = gBuffer.baseColour.second.handle,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkDescriptorImageInfo surfaceDescriptorInfo{
            .sampler = screenSampler.handle,
            .imageView = gBuffer.surface.second.handle,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        const std::array writeDescriptor{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = gbufferDescriptorSet,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &depthDescriptorInfo,
            },
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = gbufferDescriptorSet,
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &normalDescriptorInfo,
            },
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = gbufferDescriptorSet,
                .dstBinding = 2,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &baseColourDescriptorInfo,
            },
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = gbufferDescriptorSet,
                .dstBinding = 3,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &surfaceDescriptorInfo,
            }
        };

        vkUpdateDescriptorSets(context.device, writeDescriptor.size(), writeDescriptor.data(), 0, nullptr);
    }
}
