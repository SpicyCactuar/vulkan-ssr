#include "scene.hpp"

#include <array>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "config.hpp"
#include "../vkutils/error.hpp"
#include "../vkutils/to_string.hpp"
#include "../vkutils/vkutil.hpp"

#include "shadow.hpp"

namespace scene {
    vkutils::DescriptorSetLayout create_descriptor_layout(const vkutils::VulkanContext& context) {
        constexpr std::array bindings{
            VkDescriptorSetLayoutBinding{
                // number must match the index of the corresponding
                // binding = N declaration in the shader(s)!
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
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

    vkutils::Buffer create_scene_ubo(const vkutils::Allocator& allocator) {
        return vkutils::create_buffer(
            allocator,
            sizeof(glsl::SceneUniform),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );
    }

    void update_descriptor_set(const vkutils::VulkanContext& context,
                               const vkutils::Buffer& sceneUBO,
                               VkDescriptorSet sceneDescriptorSet) {
        const VkDescriptorBufferInfo sceneUboInfo{
            .buffer = sceneUBO.buffer,
            .range = VK_WHOLE_SIZE
        };

        const std::array writeDescriptor{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = sceneDescriptorSet,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &sceneUboInfo
            }
        };

        vkUpdateDescriptorSets(context.device, writeDescriptor.size(), writeDescriptor.data(), 0, nullptr);
    }

    glsl::SceneUniform create_uniform(const std::uint32_t framebufferWidth,
                                      const std::uint32_t framebufferHeight,
                                      const state::State& state) {
        // Cast boundaries once
        const auto width = static_cast<float>(framebufferWidth);
        const auto height = static_cast<float>(framebufferHeight);

        // Camera
        float const aspect = width / height;
        glm::mat4 P = glm::perspectiveRH_ZO(
            vkutils::Radians(cfg::cameraFov).value(),
            aspect,
            cfg::cameraNear,
            cfg::cameraFar
        );
        P[1][1] *= -1.0f; // mirror Y axis
        const glm::mat4 V = state.view;
        const glm::mat4 VP = P * V;

        // Window (Viewport) Matrix
        // Maps NDC [-1, 1] x [-1, 1] -> [0, w] x [0, h]
        const glm::mat4 W {
            width / 2.0f, 0.0f, 0.0f, 0.0f,
            0.0f, height / 2.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            width / 2.0f, height / 2.0f, 0.0f, 1.0f,
        };

        // Light
        glm::mat4 LP = glm::perspectiveRH_ZO(
            vkutils::Radians(cfg::lightFov).value(),
            1.0f,
            cfg::lightNear,
            cfg::lightFar
        );
        LP[1][1] *= -1.0f; // mirror Y axis
        const glm::mat4 LV = glm::lookAtRH(
            state.lightPosition,
            state.lightPosition + state.lightLookAt,
            {0.0f, 1.0f, 0.0f});
        const glm::mat4 LVP = LP * LV;

        return glsl::SceneUniform{
            .V = V,
            .P = P,
            .VP = VP,
            .LVP = LVP,
            // Scale and shift to prepare for shadow2DSampler
            .SLVP = shadow::shadowTransformationMatrix * LVP,
            .WP = W * P,
            .iP = glm::inverse(P),
            .C = state.camera
        };
    }

    void update_scene_ubo(VkCommandBuffer commandBuffer,
                          VkBuffer sceneUBO,
                          const glsl::SceneUniform& sceneUniform) {
        vkutils::buffer_barrier(commandBuffer,
                            sceneUBO,
                            VK_ACCESS_UNIFORM_READ_BIT,
                            VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT
        );

        vkCmdUpdateBuffer(commandBuffer, sceneUBO, 0, sizeof(glsl::SceneUniform), &sceneUniform);

        vkutils::buffer_barrier(commandBuffer,
                            sceneUBO,
                            VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_ACCESS_UNIFORM_READ_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
        );
    }
}
