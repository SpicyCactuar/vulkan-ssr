#include "ui.hpp"

#ifdef ENABLE_DIAGNOSTICS

#include <array>
#include <filesystem>
#include <optional>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.inl>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_vulkan.h"

#include "nfd.hpp"

#include "../vkutils/error.hpp"
#include "../vkutils/to_string.hpp"
#include "../vkutils/vkutil.hpp"
#include "../vkutils/vulkan_window.hpp"

#include "config.hpp"
#include "benchmark.hpp"
#include "playback.hpp"

// Based on: https://vkguide.dev/docs/new_chapter_2/vulkan_imgui_setup/
namespace ui {
    constexpr std::array visualisationModes{
        state::VisualisationMode::base,
        state::VisualisationMode::pbr,
        state::VisualisationMode::linearisedDepth,
        state::VisualisationMode::normal,
        state::VisualisationMode::position,
        state::VisualisationMode::viewDirection,
        state::VisualisationMode::lightDirection,
        state::VisualisationMode::roughness,
        state::VisualisationMode::metalness,
        state::VisualisationMode::reflectionDirection,
    };

    constexpr std::array<const char*, 10> visualisationModeLabels{
        "Base Colour",
        "PBR",
        "Linearised Depth",
        "Normal VCS",
        "Position VCS",
        "View Direction",
        "Light Direction",
        "Roughness",
        "Metalness",
        "Reflection Direction"
    };

    constexpr std::array pbrTerms{
        state::PBRTerm::all,
        state::PBRTerm::ambient,
        state::PBRTerm::diffuse,
        state::PBRTerm::distribution,
        state::PBRTerm::fresnel,
        state::PBRTerm::geometry,
        state::PBRTerm::specular,
        state::PBRTerm::brdf
    };

    constexpr std::array<const char*, 8> pbrTermLabels{
        "All",
        "Ambient",
        "Diffuse",
        "Distribution",
        "Fresnel",
        "Geometry",
        "Specular",
        "BRDF"
    };

    constexpr std::array<const char*, 5> ssrModeLabels{
        "Disabled",
        "Reflectance",
        "UV Map",
        "Heatmap",
        "Reflection Map"
    };

    constexpr std::array<const char*, 2> ssrTraversalSchemeLabels{
        "VCS",
        "DDA"
    };

    std::optional<std::filesystem::path> playbackPath = std::nullopt;

    vkutils::DescriptorPool create_descriptor_pool(const vkutils::VulkanContext& context) {
        constexpr std::array<VkDescriptorPoolSize, 11> poolSizes{
            {
                {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
            }
        };

        const VkDescriptorPoolCreateInfo createPoolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = 1000,
            .poolSizeCount = poolSizes.size(),
            .pPoolSizes = poolSizes.data()
        };

        VkDescriptorPool pool = VK_NULL_HANDLE;
        if (const auto res = vkCreateDescriptorPool(context.device, &createPoolInfo, nullptr, &pool);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to create ui descriptor pool\n"
                                 "vkCreateDescriptorPool() returned %s", vkutils::to_string(res).c_str()
            );
        }

        return vkutils::DescriptorPool(context.device, pool);
    }

    void initialise(const vkutils::VulkanWindow& vulkanWindow,
                    const vkutils::DescriptorPool& uiDescriptorPool) {
        std::printf("Enabling feature: ImGui UI\n");

        // Setup ImGui Context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void) io;

        const std::string alexandriaFontPath = FONTS_PATH_"/Alexandria.ttf";
        if (std::filesystem::exists(std::filesystem::path(alexandriaFontPath))) {
            const char* rawFontPath = alexandriaFontPath.c_str();
            std::printf("Loading font into ImGui: %s\n", rawFontPath);
            io.Fonts->AddFontFromFileTTF(rawFontPath, 24.0f);
        }

        // Enable Keyboard & Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        // Set Dark Theme
        ImGui::StyleColorsDark();

        // Setup ImGui for Vulkan
        ImGui_ImplGlfw_InitForVulkan(vulkanWindow.window, true);
        ImGui_ImplVulkan_InitInfo imguiInitInfo{
            .Instance = vulkanWindow.instance,
            .PhysicalDevice = vulkanWindow.physicalDevice,
            .Device = vulkanWindow.device,
            .QueueFamily = vulkanWindow.graphicsFamilyIndex,
            .Queue = vulkanWindow.graphicsQueue,
            .DescriptorPool = uiDescriptorPool.handle,
            .MinImageCount = static_cast<std::uint32_t>(vulkanWindow.swapViews.size()),
            .ImageCount = static_cast<std::uint32_t>(vulkanWindow.swapViews.size()),
            .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
            .UseDynamicRendering = true,
            .PipelineRenderingCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &vulkanWindow.swapchainFormat
            }
        };

        ImGui_ImplVulkan_Init(&imguiInitInfo);

        std::printf("Enabling feature: NFD Extended\n");
        if (NFD_Init() != NFD_OKAY) {
            throw vkutils::Error("Failed to initialise NFD: %s", NFD_GetError());
        }
    }

    std::optional<std::filesystem::path> select_playback_file() {
        nfdu8char_t* playbackRawPath;
        constexpr std::array dialogFilters = {
            nfdu8filteritem_t{
                .name = "Playback CSV",
                .spec = "csv"
            }
        };
        const nfdopendialogu8args_t dialogArguments{
            .filterList = dialogFilters.data(),
            .filterCount = dialogFilters.size()
        };

        if (const auto result = NFD_OpenDialogU8_With(&playbackRawPath, &dialogArguments);
            NFD_OKAY == result) {
            std::filesystem::path playbackPath(playbackRawPath);
            NFD_FreePathU8(playbackRawPath);
            return {playbackPath};
        } else if (NFD_ERROR == result) {
            std::printf("Playback file selection error: %s", NFD_GetError());
            return std::nullopt;
        }

        std::printf("Playback file selection cancelled");
        return std::nullopt;
    }

    void performance_ui(state::State& state, const benchmark::FrameTime& frameTime) {
        if (!ImGui::Begin("Performance menu")) {
            // Early return if collapsed
            ImGui::End();
            return;
        }

        ImGui::BeginDisabled(state.performing_benchmarks());
        ImGui::SeparatorText("Frame Time");
        ImGui::Spacing();
        ImGui::Text("Shadow Pass (ms): %.3f", frameTime.shadowInMs);
        ImGui::Text("Offscreen Pass (ms): %.3f", frameTime.offscreenInMs);
        ImGui::Text("Deferred Pass (ms): %.3f", frameTime.deferredInMs);
        ImGui::Text("Total (ms): %.3f", frameTime.totalInMs);
        ImGui::Spacing();

        ImGui::SeparatorText("Benchmarks");
        ImGui::Spacing();
        const bool loadPlaybackFile = ImGui::Button("Load Playback file");
        ImGui::SameLine();
        ImGui::Text(playbackPath.has_value()
                        ? playbackPath.value().filename().string().c_str()
                        : "No Playback file selected");
        if (loadPlaybackFile) {
            if (playbackPath = select_playback_file(); playbackPath.has_value()) {
                state.playback = new playback::Playback(
                    std::move(playback::parse_playback(playbackPath.value())));
                state.totalBenchmarkFrames = state.playback->duration_in_frames();
                state.currentBenchmarkFrame = state.totalBenchmarkFrames;
            }
        }
        int tempBenchmarkFramesCount = static_cast<int>(state.totalBenchmarkFrames);
        int tempFramesLeft = tempBenchmarkFramesCount - static_cast<int>(state.currentBenchmarkFrame);
        if (ImGui::InputInt("Frame Count",
                            state.performing_benchmarks() ? &tempFramesLeft : &tempBenchmarkFramesCount)) {
            state.totalBenchmarkFrames = tempBenchmarkFramesCount;
            state.currentBenchmarkFrame = tempBenchmarkFramesCount;
        }
        if (ImGui::Button("Perform Benchmark")) {
            state.start_benchmark();
        }

        ImGui::SeparatorText("Utilities");
        ImGui::Spacing();
        if (ImGui::Button("Take Screenshot")) {
            state.takeFrameScreenshot = true;
        }
        ImGui::EndDisabled();
        ImGui::End();
    }

    void rendering_ui(state::State& state) {
        if (!ImGui::Begin("Rendering menu")) {
            // Early return if collapsed
            ImGui::End();
            return;
        }

        ImGui::BeginDisabled(state.performing_benchmarks());
        ImGui::SeparatorText("Camera");
        ImGui::Spacing();
        auto cameraPosition = glm::vec3(state.camera[3]);
        const bool positionChanged = ImGui::InputFloat3("Position##Camera", glm::value_ptr(cameraPosition));
        auto cameraEulerRotation = glm::degrees(1.0f) * glm::eulerAngles(glm::quat_cast(state.camera));
        const bool rotationChanged = ImGui::InputFloat3("Rotation##Camera", glm::value_ptr(cameraEulerRotation));
        if (positionChanged || rotationChanged) {
            state.camera = glm::translate(cameraPosition) * glm::toMat4(glm::quat(glm::radians(cameraEulerRotation)));
        }
        ImGui::Spacing();

        ImGui::SeparatorText("Light");
        ImGui::Spacing();
        ImGui::InputFloat3("Position##Light", glm::value_ptr(state.lightPosition));
        if (ImGui::SliderFloat3("LookAt##Light", glm::value_ptr(state.lightLookAt), -1.0f, 1.0f)) {
            state.lightLookAt = glm::normalize(state.lightLookAt);
        }
        ImGui::Spacing();

        ImGui::SeparatorText("Shading");
        ImGui::Spacing();

        int visualisationModeIndex = static_cast<int>(state.visualisationMode);
        if (ImGui::Combo("Visualisation Mode", &visualisationModeIndex, visualisationModeLabels.data(),
                         visualisationModeLabels.size())) {
            state.visualisationMode = visualisationModes[visualisationModeIndex];
        }

        int pbrTermIndex = static_cast<int>(state.pbrTerm) - 1;
        if (ImGui::Combo("PBR Term", &pbrTermIndex, pbrTermLabels.data(), pbrTermLabels.size())) {
            state.pbrTerm = pbrTerms[pbrTermIndex];
        }
        ImGui::Spacing();

        if (ImGui::BeginTable("Shading Details", 2)) {
            ImGui::TableNextColumn();

            bool isNormalMappingEnabled =
                    state.shadingDetailsMask & static_cast<std::uint8_t>(state::ShadingDetails::normalMapping);
            if (ImGui::Checkbox("Normal Mapping", &isNormalMappingEnabled)) {
                state.shadingDetailsMask ^= static_cast<std::uint8_t>(state::ShadingDetails::normalMapping);
            }

            bool isShadowsEnabled = state.shadingDetailsMask & static_cast<std::uint8_t>(
                                        state::ShadingDetails::shadows);
            if (ImGui::Checkbox("Shadows", &isShadowsEnabled)) {
                state.shadingDetailsMask ^= static_cast<std::uint8_t>(state::ShadingDetails::shadows);
            }

            ImGui::TableNextColumn();

            bool isFresnelModulationEnabled =
                    state.shadingDetailsMask & static_cast<std::uint8_t>(state::ShadingDetails::fresnelModulation);
            if (ImGui::Checkbox("Fresnel Modulation", &isFresnelModulationEnabled)) {
                state.shadingDetailsMask ^= static_cast<std::uint8_t>(state::ShadingDetails::fresnelModulation);
            }
            bool isEnvironmentEnabled =
                    state.shadingDetailsMask & static_cast<std::uint8_t>(state::ShadingDetails::environmentMapping);
            if (ImGui::Checkbox("Environment Mapping", &isEnvironmentEnabled)) {
                state.shadingDetailsMask ^= static_cast<std::uint8_t>(state::ShadingDetails::environmentMapping);
            }

            ImGui::EndTable();
        }
        ImGui::Spacing();

        ImGui::SeparatorText("SSR");
        ImGui::Spacing();
        int ssrModeIndex = static_cast<int>(state.ssrMode);
        if (ImGui::Combo("Mode", &ssrModeIndex, ssrModeLabels.data(), ssrModeLabels.size())) {
            state.ssrMode = static_cast<state::SSRMode>(ssrModeIndex);
        }
        ImGui::BeginDisabled(state::SSRMode::disabled == state.ssrMode);
        int traversalSchemeIndex = static_cast<int>(state.ssrTraversalScheme) - 1;
        if (ImGui::Combo("Traversal Scheme", &traversalSchemeIndex, ssrTraversalSchemeLabels.data(),
                         ssrTraversalSchemeLabels.size())) {
            state.ssrTraversalScheme = static_cast<state::SSRTraversalScheme>(traversalSchemeIndex + 1);
        }
        ImGui::SliderFloat("R Threshold", &state.reflectivityThreshold, 0.0f, 1.0f);
        int tempSsrMaxSteps = static_cast<int>(state.ssrMaxSteps);
        ImGui::SliderInt("Max Steps", &tempSsrMaxSteps, 1, 2000);
        state.ssrMaxSteps = static_cast<std::uint32_t>(tempSsrMaxSteps);
        ImGui::SliderFloat("Stride", &state.ssrStride, 0.1f, 10.0f);
        ImGui::SliderFloat("Thickness", &state.ssrThickness, 0.0f, cfg::cameraFar, "%.5f");
        int tempSsrBinaryRefinementSteps = static_cast<int>(state.ssrBinaryRefinementSteps);
        ImGui::SliderInt("Binary Search Steps", &tempSsrBinaryRefinementSteps, 0, 10);
        state.ssrBinaryRefinementSteps = static_cast<std::uint32_t>(tempSsrBinaryRefinementSteps);
        ImGui::Spacing();
        ImGui::EndDisabled();

        ImGui::EndDisabled();

        ImGui::End();
    }

    void debug_ui(state::State& state, const benchmark::FrameTime& frameTime) {
        rendering_ui(state);
        performance_ui(state, frameTime);
    }

    void new_frame(state::State& state, const benchmark::FrameTime& frameTime) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        debug_ui(state, frameTime);
        ImGui::Render();
    }

    void render(const vkutils::VulkanWindow& vulkanWindow,
                const std::uint32_t imageIndex,
                const vkutils::Fence& uiFence,
                const VkCommandBuffer uiCommandBuffer) {
        // Wait for UI fence
        if (const auto res = vkWaitForFences(vulkanWindow.device, 1, &uiFence.handle, VK_TRUE,
                                             std::numeric_limits<std::uint64_t>::max());
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to wait for UI command buffer fence\n"
                                 "vkWaitForFences() returned %s", vkutils::to_string(res).c_str()
            );
        }

        if (const auto res = vkResetFences(vulkanWindow.device, 1, &uiFence.handle);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to reset UI command buffer fence\n"
                                 "vkResetFences() returned %s", vkutils::to_string(res).c_str()
            );
        }

        // Begin command recording
        constexpr VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };

        if (const auto res = vkBeginCommandBuffer(uiCommandBuffer, &beginInfo); VK_SUCCESS != res) {
            throw vkutils::Error("Unable to begin recording UI command buffer\n"
                                 "vkBeginCommandBuffer() returned %s", vkutils::to_string(res).c_str()
            );
        }

        // Transition Swapchain image VK_IMAGE_LAYOUT_PRESENT_SRC_KHR -> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        vkutils::image_barrier(uiCommandBuffer, vulkanWindow.swapImages[imageIndex],
                               VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_MEMORY_READ_BIT,
                               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        // Record UI render commands
        VkRenderingAttachmentInfo colorAttachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = vulkanWindow.swapViews[imageIndex],
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE
        };

        const VkRenderingInfo renderInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = VkRect2D{
                .offset = VkOffset2D{0, 0},
                .extent = vulkanWindow.swapchainExtent
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment
        };

        vkCmdBeginRendering(uiCommandBuffer, &renderInfo);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), uiCommandBuffer);
        vkCmdEndRendering(uiCommandBuffer);

        // Transition Swapchain image VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL -> VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        vkutils::image_barrier(uiCommandBuffer, vulkanWindow.swapImages[imageIndex],
                               VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_MEMORY_READ_BIT,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                               VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        // End command recording
        if (const auto res = vkEndCommandBuffer(uiCommandBuffer); VK_SUCCESS != res) {
            throw vkutils::Error("Unable to end recording UI command buffer\n"
                                 "vkEndCommandBuffer() returned %s", vkutils::to_string(res).c_str()
            );
        }

        // Submit command buffer
        constexpr VkPipelineStageFlags waitPipelineStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        const VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pWaitDstStageMask = &waitPipelineStage,
            .commandBufferCount = 1,
            .pCommandBuffers = &uiCommandBuffer
        };

        if (const auto res = vkQueueSubmit(vulkanWindow.graphicsQueue, 1, &submitInfo, uiFence.handle);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to submit UI command buffer to queue\n"
                                 "vkQueueSubmit() returned %s", vkutils::to_string(res).c_str()
            );
        }
    }

    void destroy() {
        // Shutdown NFD
        NFD::Quit();

        // Shutdown ImGui
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}

#else

namespace ui {
    vkutils::DescriptorPool create_descriptor_pool(const vkutils::VulkanContext& context) {
        return vkutils::DescriptorPool(context.device, VK_NULL_HANDLE);
    }

    void initialise([[maybe_unused]] const vkutils::VulkanWindow& vulkanWindow,
                    [[maybe_unused]] const vkutils::DescriptorPool& uiDescriptorPool) {
        // no-op
    }

    void new_frame([[maybe_unused]] state::State& state,
                   [[maybe_unused]] const benchmark::FrameTime& frameTime) {
        // no-op
    }

    void render([[maybe_unused]] const vkutils::VulkanWindow& vulkanWindow,
                [[maybe_unused]] const std::uint32_t imageIndex,
                [[maybe_unused]] const vkutils::Fence& uiFence,
                [[maybe_unused]] const VkCommandBuffer uiCommandBuffer) {
        // no-op
    }

    void destroy() {
        // no-op
    }
}

#endif
