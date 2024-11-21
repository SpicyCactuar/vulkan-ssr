#include "fullscreen.hpp"

#include "../vkutils/error.hpp"
#include "../vkutils/to_string.hpp"
#include "../vkutils/vkutil.hpp"

#include "config.hpp"

namespace fullscreen {
    vkutils::RenderPass create_render_pass(const vkutils::VulkanWindow& window) {
        const std::array attachments{
            VkAttachmentDescription{
                .format = window.swapchainFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            }
        };

        constexpr std::array colorAttachments{
            VkAttachmentReference{
                .attachment = 0, // attachments[0]
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            }
        };

        const std::array subpasses{
            VkSubpassDescription{
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = colorAttachments.size(),
                .pColorAttachments = colorAttachments.data(),
                .pDepthStencilAttachment = nullptr
            }
        };

        // Requires a subpass dependency to ensure that the first transition happens after the presentation engine is
        // done with it.
        // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples-(Legacy-synchronization-APIs)#swapchain-image-acquire-and-present
        constexpr std::array subpassDependencies{
            VkSubpassDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
            }
        };

        // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkRenderPassCreateInfo.html
        const VkRenderPassCreateInfo passInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = attachments.size(),
            .pAttachments = attachments.data(),
            .subpassCount = subpasses.size(),
            .pSubpasses = subpasses.data(),
            .dependencyCount = subpassDependencies.size(),
            .pDependencies = subpassDependencies.data()
        };

        VkRenderPass renderPass = VK_NULL_HANDLE;
        if (const auto res = vkCreateRenderPass(window.device, &passInfo, nullptr, &renderPass);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to create fullscreen render pass\n"
                                 "vkCreateRenderPass() returned %s", vkutils::to_string(res).c_str()
            );
        }

        return vkutils::RenderPass(window.device, renderPass);
    }

    vkutils::PipelineLayout create_pipeline_layout(const vkutils::VulkanContext& context,
                                                   const vkutils::DescriptorSetLayout& sceneLayout,
                                                   const vkutils::DescriptorSetLayout& shadeLayout,
                                                   const vkutils::DescriptorSetLayout& gbufferLayout,
                                                   const vkutils::DescriptorSetLayout& ssrLayout,
                                                   const vkutils::DescriptorSetLayout& environmentLayout) {
        const std::array layouts{
            // Order must match the set = N in the shaders
            sceneLayout.handle, // set 0
            shadeLayout.handle, // set 1
            gbufferLayout.handle, // set 2,
            ssrLayout.handle, // set 3
            environmentLayout.handle // set 4
        };

        const VkPipelineLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = layouts.size(),
            .pSetLayouts = layouts.data(),
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr
        };

        VkPipelineLayout layout = VK_NULL_HANDLE;
        if (const auto res = vkCreatePipelineLayout(context.device, &layoutInfo, nullptr, &layout);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to create fullscreen pipeline layout\n"
                                 "vkCreatePipelineLayout() returned %s", vkutils::to_string(res).c_str());
        }

        return vkutils::PipelineLayout(context.device, layout);
    }

    void prepare_frame_command_buffer(const vkutils::VulkanWindow& vulkanWindow,
                                      const vkutils::Fence& frameFence,
                                      const VkCommandBuffer frameCommandBuffer) {
        // Wait for frame fence
        if (const auto res = vkWaitForFences(vulkanWindow.device, 1, &frameFence.handle, VK_TRUE,
                                             std::numeric_limits<std::uint64_t>::max());
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to wait for frame command buffer fence\n"
                                 "vkWaitForFences() returned %s", vkutils::to_string(res).c_str()
            );
        }

        if (const auto res = vkResetFences(vulkanWindow.device, 1, &frameFence.handle);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to reset frame command buffer fence\n"
                                 "vkResetFences() returned %s", vkutils::to_string(res).c_str()
            );
        }

        // Begin command recording
        constexpr VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };

        if (const auto res = vkBeginCommandBuffer(frameCommandBuffer, &beginInfo); VK_SUCCESS != res) {
            throw vkutils::Error("Unable to begin recording frame command buffer\n"
                                 "vkBeginCommandBuffer() returned %s", vkutils::to_string(res).c_str()
            );
        }
    }

    vkutils::Pipeline create_fullscreen_pipeline(const vkutils::VulkanWindow& window,
                                                 VkRenderPass renderPass,
                                                 VkPipelineLayout pipelineLayout) {
        // Load only vertex and fragment shader modules
        const vkutils::ShaderModule vert = vkutils::load_shader_module(window, cfg::fullscreenVertPath);
        const vkutils::ShaderModule frag = vkutils::load_shader_module(window, cfg::fullscreenFragPath);

        // Define shader stages in the pipeline
        const std::array stages{
            // Vertex shader
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vert.handle,
                .pName = "main"
            },
            // Fragment shader
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = frag.handle,
                .pName = "main"
            }
        };

        VkPipelineVertexInputStateCreateInfo inputInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
        };

        // Define which primitive (point, line, triangle, ...) the input is assembled into for rasterization.
        constexpr VkPipelineInputAssemblyStateCreateInfo assemblyInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE
        };

        // Define viewport and scissor regions
        const VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(window.swapchainExtent.width),
            .height = static_cast<float>(window.swapchainExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        const VkRect2D scissor{
            .offset = VkOffset2D{0, 0},
            .extent = window.swapchainExtent
        };

        const VkPipelineViewportStateCreateInfo viewportInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .pViewports = &viewport,
            .scissorCount = 1,
            .pScissors = &scissor
        };

        // Define rasterisation options
        constexpr VkPipelineRasterizationStateCreateInfo rasterInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            // Screen triangle is back facing, important not to cull it
            .cullMode = VK_CULL_MODE_FRONT_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1.0f // required.
        };

        // Define multisampling state
        constexpr VkPipelineMultisampleStateCreateInfo samplingInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
        };

        // Define opaque blend state
        constexpr std::array blendStates{
            VkPipelineColorBlendAttachmentState{
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            }
        };

        const VkPipelineColorBlendStateCreateInfo blendInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .attachmentCount = blendStates.size(),
            .pAttachments = blendStates.data()
        };

        // Define depth info
        constexpr VkPipelineDepthStencilStateCreateInfo depthInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f
        };

        // Create pipeline
        const VkGraphicsPipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = stages.size(),
            .pStages = stages.data(),
            .pVertexInputState = &inputInfo,
            .pInputAssemblyState = &assemblyInfo,
            .pTessellationState = nullptr, // no tessellation
            .pViewportState = &viewportInfo,
            .pRasterizationState = &rasterInfo,
            .pMultisampleState = &samplingInfo,
            .pDepthStencilState = &depthInfo,
            .pColorBlendState = &blendInfo,
            .pDynamicState = nullptr, // no dynamic states
            .layout = pipelineLayout,
            .renderPass = renderPass,
            .subpass = 0 // first subpass of renderPass
        };

        VkPipeline pipeline = VK_NULL_HANDLE;
        if (const auto res = vkCreateGraphicsPipelines(window.device, VK_NULL_HANDLE,
                                                       1, &pipelineInfo, nullptr, &pipeline);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to create fullscreen pipeline\n"
                                 "vkCreateGraphicsPipelines() returned %s", vkutils::to_string(res).c_str());
        }

        return vkutils::Pipeline(window.device, pipeline);
    }

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
                         VkDescriptorSet environmentDescriptorSet) {
        // Begin render pass
        constexpr std::array clearValues{
            // Clear to empty background
            VkClearValue{
                .color = cfg::clearColour
            }
        };

        scene::update_scene_ubo(commandBuffer, sceneUBO, sceneUniform);
        shade::update_shade_ubo(commandBuffer, shadeUBO, shadeUniform);
        ssr::update_ssr_ubo(commandBuffer, ssrUBO, ssrUniform);

        // Bind screen descriptor set into layout(set = 0, ...)
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout, 0, 1,
                                &sceneDescriptorSet, 0, nullptr);

        // Bind shade descriptor set into layout(set = 1, ...)
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout, 1, 1,
                                &shadeDescriptorSet, 0, nullptr);

        // Bind GBuffer descriptor set into layout(set = 2, ...)
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout, 2, 1,
                                &gbufferDescriptor, 0, nullptr);

        // Bind SSR descriptor set into layout(set = 3, ...)
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout, 3, 1,
                                &ssrDescriptorSet, 0, nullptr);

        // Bind Environment descriptor set into layout(set = 4, ...)
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout, 4, 1,
                                &environmentDescriptorSet, 0, nullptr);

        // Create render pass command
        const VkRenderPassBeginInfo passInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = renderPass,
            .framebuffer = framebuffer,
            .renderArea = VkRect2D{
                .offset = VkOffset2D{0, 0},
                .extent = imageExtent
            },
            .clearValueCount = clearValues.size(),
            .pClearValues = clearValues.data()
        };

        // Begin render pass
        vkCmdBeginRenderPass(commandBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Bind fullscreen pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fullscreenPipeline);

        // Draw single triangle
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        // End the render pass
        vkCmdEndRenderPass(commandBuffer);
    }

    void submit_frame_command_buffer(const vkutils::VulkanContext& context,
                                     VkCommandBuffer frameCommandBuffer,
                                     const std::array<VkSemaphore, 2>& waitSemaphores,
                                     const VkSemaphore signalSemaphore,
                                     const vkutils::Fence& frameFence) {
        // End command recording
        if (const auto res = vkEndCommandBuffer(frameCommandBuffer); VK_SUCCESS != res) {
            throw vkutils::Error("Unable to end recording frame command buffer\n"
                                 "vkEndCommandBuffer() returned %s", vkutils::to_string(res).c_str()
            );
        }

        // Submit command buffer
        constexpr std::array<VkPipelineStageFlags, 2> waitPipelineStages{
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        };
        const VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = static_cast<std::uint32_t>(waitSemaphores.size()),
            .pWaitSemaphores = waitSemaphores.data(),
            .pWaitDstStageMask = waitPipelineStages.data(),
            .commandBufferCount = 1,
            .pCommandBuffers = &frameCommandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &signalSemaphore
        };

        if (const auto res = vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, frameFence.handle);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to submit frame command buffer to queue\n"
                                 "vkQueueSubmit() returned %s", vkutils::to_string(res).c_str()
            );
        }
    }
}
