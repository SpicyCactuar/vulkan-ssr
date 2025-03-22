#include "offscreen.hpp"

#include <array>
#include <utility>

#include "../vkutils/error.hpp"
#include "../vkutils/to_string.hpp"

#include "config.hpp"
#include "shade.hpp"

namespace offscreen {
    vkutils::RenderPass create_render_pass(const vkutils::VulkanWindow& window) {
        constexpr std::array attachments{
            // G-Buffer attachments
            VkAttachmentDescription{
                .format = gbuffer::depthFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            },
            VkAttachmentDescription{
                .format = gbuffer::normalFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            },
            VkAttachmentDescription{
                .format = gbuffer::baseColourFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            },
            VkAttachmentDescription{
                .format = gbuffer::surfaceFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            },
            VkAttachmentDescription{
                .format = gbuffer::emissiveFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        };

        constexpr VkAttachmentReference depthAttachment{
            .attachment = 0, // attachments[0]
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };

        constexpr std::array colorAttachments{
            VkAttachmentReference{
                .attachment = 1, // attachments[1]
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            },
            VkAttachmentReference{
                .attachment = 2, // attachments[2]
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            },
            VkAttachmentReference{
                .attachment = 3, // attachments[3]
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            },
            VkAttachmentReference{
                .attachment = 4, // attachments[4]
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            }
        };

        const std::array subpasses{
            VkSubpassDescription{
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = colorAttachments.size(),
                .pColorAttachments = colorAttachments.data(),
                .pDepthStencilAttachment = &depthAttachment
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
            },
            VkSubpassDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
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
            throw vkutils::Error("Unable to create offscreen render pass\n"
                                 "vkCreateRenderPass() returned %s", vkutils::to_string(res).c_str()
            );
        }

        return vkutils::RenderPass(window.device, renderPass);
    }

    vkutils::PipelineLayout create_pipeline_layout(const vkutils::VulkanContext& context,
                                                   const vkutils::DescriptorSetLayout& sceneLayout,
                                                   const vkutils::DescriptorSetLayout& shadeLayout,
                                                   const vkutils::DescriptorSetLayout& materialLayout) {
        const std::array layouts{
            // Order must match the set = N in the shaders
            sceneLayout.handle, // set 0
            shadeLayout.handle, // set 1
            materialLayout.handle // set 2
        };

        // Create a pipeline layout that includes a push constant range
        constexpr VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(glsl::MaterialPushConstants)
        };

        const VkPipelineLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            // Initialise with layouts information
            .setLayoutCount = layouts.size(),
            .pSetLayouts = layouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange
        };

        VkPipelineLayout layout = VK_NULL_HANDLE;
        if (const auto res = vkCreatePipelineLayout(context.device, &layoutInfo, nullptr, &layout);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to create offscreen pipeline layout\n"
                                 "vkCreatePipelineLayout() returned %s", vkutils::to_string(res).c_str());
        }

        return vkutils::PipelineLayout(context.device, layout);
    }

    vkutils::Pipeline create_opaque_pipeline(const vkutils::VulkanWindow& window,
                                             VkRenderPass renderPass,
                                             VkPipelineLayout pipelineLayout) {
        // Load only vertex and fragment shader modules
        const vkutils::ShaderModule vert = vkutils::load_shader_module(window, cfg::offscreenVertPath);
        const vkutils::ShaderModule frag = vkutils::load_shader_module(window, cfg::offscreenOpaqueFragPath);

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

        // Create vertex inputs
        constexpr std::array vertexBindings{
            // Positions Binding
            VkVertexInputBindingDescription{
                .binding = 0,
                .stride = sizeof(glm::vec3),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            },
            // UVs Binding
            VkVertexInputBindingDescription{
                .binding = 1,
                .stride = sizeof(glm::vec2),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            },
            // Normals Binding
            VkVertexInputBindingDescription{
                .binding = 2,
                .stride = sizeof(glm::vec3),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            },
            // Tangents Binding
            VkVertexInputBindingDescription{
                .binding = 3,
                .stride = sizeof(glm::vec4),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            }
        };

        // Create vertex attributes
        constexpr std::array vertexAttributes{
            // Positions attribute
            VkVertexInputAttributeDescription{
                .location = 0, // must match shader
                .binding = vertexBindings[0].binding,
                .format = VK_FORMAT_R32G32B32_SFLOAT, // (x, y, z)
                .offset = 0
            },
            // UVs attribute
            VkVertexInputAttributeDescription{
                .location = 1, // must match shader
                .binding = vertexBindings[1].binding,
                .format = VK_FORMAT_R32G32_SFLOAT, // (u, v)
                .offset = 0
            },
            // Normals attribute
            VkVertexInputAttributeDescription{
                .location = 2, // must match shader
                .binding = vertexBindings[2].binding,
                .format = VK_FORMAT_R32G32B32_SFLOAT, // (i, j, k)
                .offset = 0

            },
            // Tangents attribute
            VkVertexInputAttributeDescription{
                .location = 3, // must match shader
                .binding = vertexBindings[3].binding,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT, // (x, y, z, w)
                .offset = 0
            }
        };

        // Create Pipeline with Vertex input
        const VkPipelineVertexInputStateCreateInfo inputInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = vertexBindings.size(),
            .pVertexBindingDescriptions = vertexBindings.data(),
            .vertexAttributeDescriptionCount = vertexAttributes.size(),
            .pVertexAttributeDescriptions = vertexAttributes.data()
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
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1.0f // required.
        };

        // Define multisampling state
        constexpr VkPipelineMultisampleStateCreateInfo samplingInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
        };

        // Define a blend state per colour GBuffer attachment
        constexpr std::array blendStates{
            VkPipelineColorBlendAttachmentState{
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            },
            VkPipelineColorBlendAttachmentState{
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            },
            VkPipelineColorBlendAttachmentState{
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            },
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
            throw vkutils::Error("Unable to create offscreen opaque pipeline\n"
                                 "vkCreateGraphicsPipelines() returned %s", vkutils::to_string(res).c_str());
        }

        return vkutils::Pipeline(window.device, pipeline);
    }

    vkutils::Pipeline create_alpha_pipeline(const vkutils::VulkanWindow& window,
                                            const VkRenderPass renderPass,
                                            const VkPipelineLayout pipelineLayout) {
        // Load only vertex and fragment shader modules
        const vkutils::ShaderModule vert = vkutils::load_shader_module(window, cfg::offscreenVertPath);
        const vkutils::ShaderModule frag = vkutils::load_shader_module(window, cfg::offscreenAlphaFragPath);

        // Define shader stages in the pipeline
        const std::array stages = {
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

        // Create vertex inputs
        constexpr std::array vertexBindings = {
            // Positions Binding
            VkVertexInputBindingDescription{
                .binding = 0,
                .stride = sizeof(glm::vec3),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            },
            // UVs Binding
            VkVertexInputBindingDescription{
                .binding = 1,
                .stride = sizeof(glm::vec2),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            },
            // Normals Binding
            VkVertexInputBindingDescription{
                .binding = 2,
                .stride = sizeof(glm::vec3),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            },
            // Tangents Binding
            VkVertexInputBindingDescription{
                .binding = 3,
                .stride = sizeof(glm::vec4),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            }
        };

        // Create vertex attributes
        constexpr std::array vertexAttributes = {
            // Positions attribute
            VkVertexInputAttributeDescription{
                .location = 0, // must match shader
                .binding = vertexBindings[0].binding,
                .format = VK_FORMAT_R32G32B32_SFLOAT, // (x, y, z)
                .offset = 0
            },
            // UVs attribute
            VkVertexInputAttributeDescription{
                .location = 1, // must match shader
                .binding = vertexBindings[1].binding,
                .format = VK_FORMAT_R32G32_SFLOAT, // (u, v)
                .offset = 0
            },
            // Normals attribute
            VkVertexInputAttributeDescription{
                .location = 2, // must match shader
                .binding = vertexBindings[2].binding,
                .format = VK_FORMAT_R32G32B32_SFLOAT, // (i, j, k)
                .offset = 0
            },
            // Tangents attribute
            VkVertexInputAttributeDescription{
                .location = 3, // must match shader
                .binding = vertexBindings[3].binding,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT, // (x, y, z, w)
                .offset = 0
            }
        };

        // Create Pipeline with Vertex input
        const VkPipelineVertexInputStateCreateInfo inputInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = vertexBindings.size(),
            .pVertexBindingDescriptions = vertexBindings.data(),
            .vertexAttributeDescriptionCount = vertexAttributes.size(),
            .pVertexAttributeDescriptions = vertexAttributes.data()
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
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1.0f // required.
        };

        // Define multisampling state
        constexpr VkPipelineMultisampleStateCreateInfo samplingInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
        };

        // Define a blend state per colour GBuffer attachment
        constexpr std::array blendStates{
            VkPipelineColorBlendAttachmentState{
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            },
            VkPipelineColorBlendAttachmentState{
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            },
            VkPipelineColorBlendAttachmentState{
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            },
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
            throw vkutils::Error("Unable to create offscreen alpha mask pipeline\n"
                                 "vkCreateGraphicsPipelines() returned %s", vkutils::to_string(res).c_str());
        }

        return vkutils::Pipeline(window.device, pipeline);
    }

    vkutils::Framebuffer create_offscreen_framebuffer(const vkutils::VulkanWindow& window,
                                                      const VkRenderPass offscreenRenderPass,
                                                      const gbuffer::GBuffer& gBuffer) {
        const std::array attachments{
            gBuffer.depth.second.handle,
            gBuffer.normal.second.handle,
            gBuffer.baseColour.second.handle,
            gBuffer.surface.second.handle,
            gBuffer.emissive.second.handle
        };
        const VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = offscreenRenderPass,
            .attachmentCount = attachments.size(),
            .pAttachments = attachments.data(),
            .width = window.swapchainExtent.width,
            .height = window.swapchainExtent.height,
            .layers = 1
        };

        VkFramebuffer framebuffer;
        if (const auto res = vkCreateFramebuffer(window.device, &framebufferInfo, nullptr, &framebuffer);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to create offscreen framebuffer\n"
                                 "vkCreateImageView() returned %s", vkutils::to_string(res).c_str()
            );
        }

        return vkutils::Framebuffer(window.device, framebuffer);
    }

    void prepare_offscreen_command_buffer(const vkutils::VulkanContext& context,
                                          const vkutils::Fence& offscreenFence,
                                          VkCommandBuffer offscreenCommandBuffer) {
        // Wait for offscreen fence
        if (const auto res = vkWaitForFences(context.device, 1, &offscreenFence.handle, VK_TRUE,
                                             std::numeric_limits<std::uint64_t>::max());
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to wait for offscreen command buffer fence\n"
                                 "vkWaitForFences() returned %s", vkutils::to_string(res).c_str()
            );
        }

        if (const auto res = vkResetFences(context.device, 1, &offscreenFence.handle);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to reset offscreen command buffer fence\n"
                                 "vkResetFences() returned %s", vkutils::to_string(res).c_str()
            );
        }

        // Begin command recording
        constexpr VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };

        if (const auto res = vkBeginCommandBuffer(offscreenCommandBuffer, &beginInfo); VK_SUCCESS != res) {
            throw vkutils::Error("Unable to begin recording offscreen command buffer\n"
                                 "vkBeginCommandBuffer() returned %s", vkutils::to_string(res).c_str()
            );
        }
    }

    void record_commands(VkCommandBuffer commandBuffer,
                         VkRenderPass renderPass,
                         VkFramebuffer framebuffer,
                         VkPipelineLayout pipelineLayout,
                         VkPipeline opaquePipeline,
                         VkPipeline alphaPipeline,
                         const VkExtent2D& imageExtent,
                         VkBuffer sceneUBO,
                         const glsl::SceneUniform& sceneUniform,
                         VkDescriptorSet sceneDescriptorSet,
                         VkBuffer shadeUBO,
                         const glsl::ShadeUniform& shadeUniform,
                         VkDescriptorSet shadeDescriptorSet,
                         const std::vector<mesh::Mesh>& opaqueMeshes,
                         const std::vector<mesh::Mesh>& alphaMeshes,
                         const std::vector<material::Material>& materials,
                         const std::vector<VkDescriptorSet>& materialDescriptorSets) {
        // Begin render pass
        // Clear in order: depth, normal, baseColour, surface
        constexpr std::array clearValues{
            // Clear G-Buffer
            VkClearValue{
                .depthStencil = VkClearDepthStencilValue{
                    .depth = 1.0f
                }
            },
            VkClearValue{
                .color = cfg::noColour
            },
            VkClearValue{
                .color = cfg::noColour
            },
            VkClearValue{
                .color = cfg::noColour
            },
            VkClearValue{
                .color = cfg::noColour
            }
        };

        // Prepare uniforms
        scene::update_scene_ubo(commandBuffer, sceneUBO, sceneUniform);
        shade::update_shade_ubo(commandBuffer, shadeUBO, shadeUniform);

        // Bind scene descriptor set into layout(set = 0, ...)
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout, 0, 1,
                                &sceneDescriptorSet, 0, nullptr);

        // Bind screen descriptor set into layout(set = 1, ...)
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout, 1, 1,
                                &shadeDescriptorSet, 0, nullptr);

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

        // First opaque pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, opaquePipeline);

        // Draw opaque meshes
        for (const auto& mesh : opaqueMeshes) {
            // Push the constants to the command buffer
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(glsl::MaterialPushConstants), &materials[mesh.materialId].pushConstants);

            // Bind mesh descriptor set into layout(set = 2, ...)
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout, 2, 1,
                                    &materialDescriptorSets[mesh.materialId], 0, nullptr);

            // Bind mesh vertex buffers into layout(location = {1, 2, 3, 4})
            const std::array vertexBuffers{
                mesh.positions.buffer, mesh.uvs.buffer, mesh.normals.buffer, mesh.tangents.buffer
            };
            constexpr std::array<VkDeviceSize, vertexBuffers.size()> offsets{};
            vkCmdBindVertexBuffers(commandBuffer, 0, vertexBuffers.size(), vertexBuffers.data(), offsets.data());

            // Bind mesh vertex indices
            vkCmdBindIndexBuffer(commandBuffer, mesh.indices.buffer, 0, VK_INDEX_TYPE_UINT32);

            // Draw mesh vertices
            vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1, 0, 0, 0);
        }

        // Then alpha pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, alphaPipeline);

        // Draw alpha meshes
        for (const auto& mesh : alphaMeshes) {
            // Push the constants to the command buffer
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(glsl::MaterialPushConstants), &materials[mesh.materialId].pushConstants);

            // Bind mesh descriptor set into layout(set = 2, ...)
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout, 2, 1,
                                    &materialDescriptorSets[mesh.materialId], 0, nullptr);

            // Bind mesh vertex buffers into layout(location = {1, 2, 3, 4})
            const std::array vertexBuffers{
                mesh.positions.buffer, mesh.uvs.buffer, mesh.normals.buffer, mesh.tangents.buffer
            };
            constexpr std::array<VkDeviceSize, vertexBuffers.size()> offsets{};
            vkCmdBindVertexBuffers(commandBuffer, 0, vertexBuffers.size(), vertexBuffers.data(), offsets.data());

            // Bind mesh vertex indices
            vkCmdBindIndexBuffer(commandBuffer, mesh.indices.buffer, 0, VK_INDEX_TYPE_UINT32);

            // Draw mesh vertices
            vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1, 0, 0, 0);
        }

        // End render pass
        vkCmdEndRenderPass(commandBuffer);
    }

    void submit_commands(const vkutils::VulkanContext& context,
                         const VkCommandBuffer offscreenCommandBuffer,
                         const vkutils::Semaphore& signalSemaphore,
                         const vkutils::Fence& offscreenFence) {
        // End command recording
        if (const auto res = vkEndCommandBuffer(offscreenCommandBuffer); VK_SUCCESS != res) {
            throw vkutils::Error("Unable to end recording offscreen command buffer\n"
                                 "vkEndCommandBuffer() returned %s", vkutils::to_string(res).c_str()
            );
        }

        // Submit command buffer, with signal semaphore only
        const VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &offscreenCommandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &signalSemaphore.handle
        };

        if (const auto res = vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, offscreenFence.handle);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to submit offscreen command buffer to queue\n"
                                 "vkQueueSubmit() returned %s", vkutils::to_string(res).c_str()
            );
        }
    }

    void wait_offscreen_early(const vkutils::VulkanWindow& vulkanWindow,
                              const vkutils::Semaphore& waitSemaphore) {
        // Wait for offscreen finished pass to complete
        // Needed in cases in which the offscreen pass was submitted but not waited upon
        // Occurs when swapchain is recreated

        // TODO: Determine whether this approach is better than acquiring swapchain earlier
        constexpr VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        const VkSubmitInfo waitSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &waitSemaphore.handle,
            .pWaitDstStageMask = &waitStage,
            .commandBufferCount = 0 // No command buffers to execute
        };
        vkQueueSubmit(vulkanWindow.graphicsQueue, 1, &waitSubmitInfo, VK_NULL_HANDLE);
    }
}
