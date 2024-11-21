#include "shade.hpp"

#include <array>

#include "../vkutils/error.hpp"
#include "../vkutils/to_string.hpp"
#include "../vkutils/vkutil.hpp"

#include "config.hpp"

namespace shade {
    vkutils::DescriptorSetLayout create_descriptor_layout(const vkutils::VulkanContext& context) {
        constexpr std::array bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0, // layout(set = ..., binding = 0)
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 1, // layout(set = ..., binding = 1)
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
            throw vkutils::Error("Unable to create descriptor set layout\n"
                                 "vkCreateDescriptorSetLayout() returned %s", vkutils::to_string(res).c_str()
            );
        }

        return vkutils::DescriptorSetLayout(context.device, layout);
    }

    vkutils::Buffer create_shade_ubo(const vkutils::Allocator& allocator) {
        return vkutils::create_buffer(
            allocator,
            sizeof(glsl::ShadeUniform),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );
    }

    void update_descriptor_set(const vkutils::VulkanContext& context,
                               const vkutils::Buffer& shadeUBO,
                               VkDescriptorSet shadeDescriptorSet,
                               const vkutils::Sampler& shadowSampler,
                               VkImageView shadowView) {
        const VkDescriptorBufferInfo shadeUboInfo{
            .buffer = shadeUBO.buffer,
            .range = VK_WHOLE_SIZE
        };

        const VkDescriptorImageInfo shadowDescriptorInfo{
            .sampler = shadowSampler.handle,
            .imageView = shadowView,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        };

        const std::array writeDescriptor{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = shadeDescriptorSet,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &shadeUboInfo
            },
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = shadeDescriptorSet,
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &shadowDescriptorInfo,
            }
        };

        vkUpdateDescriptorSets(context.device, writeDescriptor.size(), writeDescriptor.data(), 0, nullptr);
    }

    glsl::ShadeUniform create_uniform(const state::State& state) {
        return glsl::ShadeUniform{
            .visualisationMode = static_cast<std::uint32_t>(state.visualisationMode),
            .pbrTerm = static_cast<std::uint32_t>(state.pbrTerm),
            .detailsMask = static_cast<std::uint32_t>(state.shadingDetailsMask),
            .ambient = {cfg::ambient, 1.0f},
            .camera = glsl::CameraUniform{
                .near = cfg::cameraNear,
                .far = cfg::cameraFar,
                .position = {glm::vec3(state.camera[3]), 1.0f}
            },
            .light = glsl::PointLightUniform{
                .position = state.view * glm::vec4{state.lightPosition, 1.0f},
                .colour = {cfg::lightColour, 1.0f}
            }
        };
    }

    void update_shade_ubo(const VkCommandBuffer commandBuffer,
                          const VkBuffer shadeUBO,
                          const glsl::ShadeUniform& shadeUniform) {
        vkutils::buffer_barrier(commandBuffer, shadeUBO,
                                VK_ACCESS_UNIFORM_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        vkCmdUpdateBuffer(commandBuffer, shadeUBO, 0, sizeof(glsl::ShadeUniform), &shadeUniform);

        vkutils::buffer_barrier(commandBuffer, shadeUBO,
                                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }
}
