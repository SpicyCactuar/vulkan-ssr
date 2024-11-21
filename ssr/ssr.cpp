#include "ssr.hpp"

#include <array>

#include "../vkutils/vkutil.hpp"
#include "../vkutils/error.hpp"
#include "../vkutils/to_string.hpp"

namespace ssr {
    vkutils::DescriptorSetLayout create_descriptor_layout(const vkutils::VulkanContext& context) {
        constexpr std::array bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0, // layout(set = ..., binding = 0)
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
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

    vkutils::Buffer create_ssr_ubo(const vkutils::Allocator& allocator) {
        return vkutils::create_buffer(
            allocator,
            sizeof(glsl::SSRUniform),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );
    }

    void update_descriptor_set(const vkutils::VulkanContext& context, const vkutils::Buffer& ssrUBO,
                               const VkDescriptorSet ssrDescriptorSet) {
        const VkDescriptorBufferInfo sceneUboInfo{
            .buffer = ssrUBO.buffer,
            .range = VK_WHOLE_SIZE
        };

        const std::array writeDescriptor{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = ssrDescriptorSet,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &sceneUboInfo
            }
        };

        vkUpdateDescriptorSets(context.device, writeDescriptor.size(), writeDescriptor.data(), 0, nullptr);
    }

    glsl::SSRUniform create_uniform(const state::State& state) {
        return glsl::SSRUniform{
            .mode = static_cast<std::uint32_t>(state.ssrMode),
            .reflectivityThreshold = state.reflectivityThreshold,
            .traversalScheme = static_cast<std::uint32_t>(state.ssrTraversalScheme),
            .maxSteps = state.ssrMaxSteps,
            .stride = state.ssrStride,
            .binaryRefinementSteps = state.ssrBinaryRefinementSteps,
            .thickness = state.ssrThickness,
        };
    }

    void update_ssr_ubo(const VkCommandBuffer commandBuffer,
                        const VkBuffer ssrUBO,
                        const glsl::SSRUniform& ssrUniform) {
        vkutils::buffer_barrier(commandBuffer, ssrUBO,
                                VK_ACCESS_UNIFORM_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        vkCmdUpdateBuffer(commandBuffer, ssrUBO, 0, sizeof(glsl::SSRUniform), &ssrUniform);

        vkutils::buffer_barrier(commandBuffer, ssrUBO,
                                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }
}
