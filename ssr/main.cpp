#include <chrono>
#include <cstdint>
#include <cstdio>
#include <format>
#include <filesystem>
#include <fstream>
#include <vector>
#include <volk/volk.h>

#include "../vkutils/vkbuffer.hpp"
#include "../vkutils/vkimage.hpp"
#include "../vkutils/vulkan_window.hpp"

#include "baked_model.hpp"
#include "benchmark.hpp"
#include "config.hpp"
#include "environment.hpp"
#include "fullscreen.hpp"
#include "gbuffer.hpp"
#include "glfw.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "offscreen.hpp"
#include "path.hpp"
#include "scene.hpp"
#include "screenshot.hpp"
#include "shade.hpp"
#include "shadow.hpp"
#include "ssr.hpp"
#include "state.hpp"
#include "swapchain.hpp"
#include "ui.hpp"

int main(int argc, char* argv[]) try {
    // Validate args
    if (const std::string arg1(argc < 2 ? "-h" : argv[1]); arg1 == "-h" || arg1 == "--help") {
        std::printf("Usage: <binary> [scene_name] [tag]\n");
        std::printf("[scene_name] must be one of the baked scenes in %s\n", ASSETS_PATH_);
        std::printf("[tag] optional tag added to output file names\n");
        return EXIT_SUCCESS;
    }

    std::string sceneName(argv[1]);
    std::string sceneTag(argc < 3 ? "" : argv[2]);
    const std::filesystem::path assetsPath(ASSETS_PATH_);

    const std::filesystem::path scenePath = assetsPath / sceneName / std::format("{}.spicymesh", sceneName);
    if (!std::filesystem::exists(scenePath)) {
        std::fprintf(stderr, "%s does not exist\n", scenePath.string().c_str());
        return EXIT_FAILURE;
    }

    // Prepare Vulkan windoww
    vkutils::VulkanWindow vulkanWindow = vkutils::make_vulkan_window(cfg::windowWidth, cfg::windowHeight, sceneName);

    // Configure the GLFW callbacks & state
    state::State state{};
    glfw::setup_window(vulkanWindow, state);

    // Create VMA allocator
    const vkutils::Allocator allocator = vkutils::create_allocator(vulkanWindow);

    // Create pools
    const vkutils::DescriptorPool descriptorPool = vkutils::create_descriptor_pool(vulkanWindow);
    const vkutils::CommandPool commandPool = vkutils::create_command_pool(
        vulkanWindow, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // Initialise UI
    const vkutils::DescriptorPool uiDescriptorPool = ui::create_descriptor_pool(vulkanWindow);
    const vkutils::Fence uiFence = vkutils::create_fence(vulkanWindow, VK_FENCE_CREATE_SIGNALED_BIT);
    const VkCommandBuffer uiCommandBuffer = vkutils::alloc_command_buffer(vulkanWindow, commandPool.handle);
    ui::initialise(vulkanWindow, uiDescriptorPool);

    // Create descriptor layouts reused across shadow & offscreen passes
    const vkutils::DescriptorSetLayout sceneLayout = scene::create_descriptor_layout(vulkanWindow);
    const vkutils::DescriptorSetLayout materialLayout = material::create_descriptor_layout(vulkanWindow);

    // Initialise GBuffer
    gbuffer::GBuffer gBuffer(vulkanWindow, allocator);
    const vkutils::DescriptorSetLayout gbufferDescriptorLayout = gbuffer::create_descriptor_layout(vulkanWindow);

    // Load SSR descriptor
    const vkutils::DescriptorSetLayout ssrDescriptorLayout = ssr::create_descriptor_layout(vulkanWindow);
    vkutils::Buffer ssrUBO = ssr::create_ssr_ubo(allocator);
    const VkDescriptorSet ssrDescriptorSet = vkutils::allocate_descriptor_set(
        vulkanWindow, descriptorPool.handle, ssrDescriptorLayout.handle);
    ssr::update_descriptor_set(vulkanWindow, ssrUBO, ssrDescriptorSet);

    // Initialise Shadow Map Pipeline
    const vkutils::RenderPass shadowPass = shadow::create_render_pass(vulkanWindow);
    const vkutils::PipelineLayout shadowOpaqueLayout = shadow::create_opaque_pipeline_layout(vulkanWindow, sceneLayout);
    vkutils::Pipeline shadowOpaquePipeline = shadow::create_opaque_pipeline(
        vulkanWindow, shadowPass.handle, shadowOpaqueLayout.handle);
    const vkutils::PipelineLayout shadowAlphaLayout = shadow::create_alpha_pipeline_layout(
        vulkanWindow, sceneLayout, materialLayout);
    vkutils::Pipeline shadowAlphaPipeline = shadow::create_alpha_pipeline(
        vulkanWindow, shadowPass.handle, shadowAlphaLayout.handle);
    auto [shadowImage, shadowView] = shadow::create_shadow_buffer(vulkanWindow, allocator);
    const vkutils::Framebuffer shadowFramebuffer = shadow::create_shadow_framebuffer(
        vulkanWindow, shadowPass.handle, shadowView.handle);

    // Intialise Offscreen Pipeline
    const vkutils::RenderPass offscreenPass = offscreen::create_render_pass(vulkanWindow);
    const vkutils::DescriptorSetLayout shadeLayout = shade::create_descriptor_layout(vulkanWindow);
    const vkutils::PipelineLayout offscreenLayout = offscreen::create_pipeline_layout(
        vulkanWindow, sceneLayout, shadeLayout, materialLayout);
    vkutils::Pipeline offscreenOpaquePipeline = offscreen::create_opaque_pipeline(
        vulkanWindow, offscreenPass.handle, offscreenLayout.handle);
    vkutils::Pipeline offscreenAlphaPipeline = offscreen::create_alpha_pipeline(
        vulkanWindow, offscreenPass.handle, offscreenLayout.handle);
    vkutils::Framebuffer offscreenFramebuffer = offscreen::create_offscreen_framebuffer(
        vulkanWindow, offscreenPass.handle, gBuffer);

    // Initialise Environment descriptor
    const auto environmentDescriptorLayout = environment::create_descriptor_layout(vulkanWindow);
    const auto environmentDescriptorSet = vkutils::allocate_descriptor_set(
        vulkanWindow, descriptorPool.handle, environmentDescriptorLayout.handle);

    // Intialise Fullscreen Pipeline
    vkutils::RenderPass fullscreenPass = fullscreen::create_render_pass(vulkanWindow);
    const vkutils::PipelineLayout fullscreenLayout = fullscreen::create_pipeline_layout(vulkanWindow,
        sceneLayout, shadeLayout, gbufferDescriptorLayout, ssrDescriptorLayout, environmentDescriptorLayout);
    vkutils::Pipeline fullscreenPipeline = fullscreen::create_fullscreen_pipeline(
        vulkanWindow, fullscreenPass.handle, fullscreenLayout.handle);

    // Initialise per-frame Framebuffers and Synchronisation resources
    std::vector<vkutils::Framebuffer> framebuffers = swapchain::create_swapchain_framebuffers(
        vulkanWindow, fullscreenPass.handle);

    std::vector<VkCommandBuffer> frameCommandBuffers;
    std::vector<vkutils::Fence> frameFences;

    for (std::size_t i = 0; i < framebuffers.size(); ++i) {
        frameCommandBuffers.emplace_back(vkutils::alloc_command_buffer(vulkanWindow, commandPool.handle));
        frameFences.emplace_back(vkutils::create_fence(vulkanWindow, VK_FENCE_CREATE_SIGNALED_BIT));
    }

    // Initialise offscreen synchronisation resources
    const vkutils::Fence offscreenFence = vkutils::create_fence(vulkanWindow, VK_FENCE_CREATE_SIGNALED_BIT);
    const VkCommandBuffer offscreenCommandBuffer = vkutils::alloc_command_buffer(vulkanWindow, commandPool.handle);

    // Initialise semaphores
    const vkutils::Semaphore offscreenFinished = vkutils::create_semaphore(vulkanWindow);
    const vkutils::Semaphore renderFinished = vkutils::create_semaphore(vulkanWindow);
    const std::vector<vkutils::Semaphore> swapchainImagesAvailable =
            vkutils::create_semaphores(vulkanWindow, framebuffers.size());

    // Create Samplers
    const vkutils::Sampler anisotropySampler = vkutils::create_anisotropy_sampler(vulkanWindow);
    const vkutils::Sampler pointSampler = vkutils::create_point_sampler(vulkanWindow);
    const vkutils::Sampler screenSampler = vkutils::create_screen_sampler(vulkanWindow);
    const vkutils::Sampler shadowSampler = vkutils::create_shadow_sampler(vulkanWindow);

    // Load scene descriptor
    vkutils::Buffer sceneUBO = scene::create_scene_ubo(allocator);
    const VkDescriptorSet sceneDescriptorSet = vkutils::allocate_descriptor_set(
        vulkanWindow, descriptorPool.handle, sceneLayout.handle);
    scene::update_descriptor_set(vulkanWindow, sceneUBO, sceneDescriptorSet);

    // Load shade descriptor
    vkutils::Buffer shadeUbo = shade::create_shade_ubo(allocator);
    const VkDescriptorSet shadeDescriptorSet = vkutils::allocate_descriptor_set(vulkanWindow, descriptorPool.handle,
        shadeLayout.handle);
    shade::update_descriptor_set(vulkanWindow, shadeUbo, shadeDescriptorSet, shadowSampler, shadowView.handle);

    // Load gbuffer descriptor
    const VkDescriptorSet gbufferDescriptorSet = vkutils::allocate_descriptor_set(vulkanWindow, descriptorPool.handle,
        gbufferDescriptorLayout.handle);
    gbuffer::update_descriptor_set(vulkanWindow, gbufferDescriptorSet, screenSampler, gBuffer);

    // Load model
    const baked::BakedModel sceneModel = baked::loadBakedModel(scenePath.generic_string().c_str());

    // Load materials
    // Keeps all Images and ImageViews alive for the duration of the render loop
    const material::MaterialStore materialStore = material::extract_materials(sceneModel, vulkanWindow, allocator);

    // Load 1 DescriptorSet per material
    const std::vector<VkDescriptorSet> materialDescriptorSets = vkutils::allocate_descriptor_sets(
        vulkanWindow, descriptorPool.handle, materialLayout.handle,
        static_cast<std::uint32_t>(materialStore.materials.size()));

    for (std::size_t m = 0; m < materialStore.materials.size(); ++m) {
        const auto& material = materialStore.materials[m];
        const auto& materialDescriptorSet = materialDescriptorSets[m];

        material::update_descriptor_set(vulkanWindow, materialDescriptorSet, material, anisotropySampler, pointSampler);
    }

    // Extract meshes
    const auto [opaqueMeshes, alphaMeshes] =
            mesh::extract_meshes(vulkanWindow, allocator, sceneModel, materialStore.materials);

    // Load environment
    const auto cubeMap = environment::load_cube_map(vulkanWindow, allocator, commandPool);
    environment::update_descriptor_set(vulkanWindow, environmentDescriptorSet, cubeMap.second, anisotropySampler);


#ifdef ENABLE_DIAGNOSTICS
    // Screenshot resources
    const vkutils::Event screenshotReady = vkutils::create_event(vulkanWindow);
#endif

    // Benchmarking
    std::ofstream benchmarksFile;
    const std::vector<vkutils::QueryPool> timestampPools = benchmark::create_timestamp_pools(vulkanWindow);
    auto timestampBuffer = benchmark::create_timestamp_buffer();
    const auto timestampPeriod = benchmark::timestamp_period(vulkanWindow);
    std::size_t frameInFlightIndex = 0;

    // Render loop
    bool recreateSwapchain = false;

    // Initialise clock right before the render loop
    auto lastClock = cfg::Clock::now();

    while (!glfwWindowShouldClose(vulkanWindow.window)) {
        // We want to render the next frame as soon as possible => Poll events
        glfwPollEvents();

        // Recreate swapchain if needed
        if (recreateSwapchain) {
            // We need to destroy several objects, which may still be in use by the GPU.
            // Therefore, first wait for the GPU to finish processing.
            vkDeviceWaitIdle(vulkanWindow.device);

            // Recreate them
            const auto changes = recreate_swapchain(vulkanWindow);

            if (changes.changedFormat) {
                // Offscreen does not depend on swapchain format, only recreate Fullscreen pass
                fullscreenPass = fullscreen::create_render_pass(vulkanWindow);
                fullscreenPipeline = fullscreen::create_fullscreen_pipeline(
                    vulkanWindow, fullscreenPass.handle, fullscreenLayout.handle);
            }

            if (changes.changedSize) {
                // Recreate both offscreen & fullscreen passes
                gBuffer = gbuffer::GBuffer(vulkanWindow, allocator);
                offscreenOpaquePipeline = offscreen::create_opaque_pipeline(vulkanWindow, offscreenPass.handle,
                                                                            offscreenLayout.handle);
                offscreenAlphaPipeline = offscreen::create_alpha_pipeline(vulkanWindow, offscreenPass.handle,
                                                                          offscreenLayout.handle);
                offscreenFramebuffer = offscreen::create_offscreen_framebuffer(
                    vulkanWindow, offscreenPass.handle, gBuffer);

                fullscreenPipeline = fullscreen::create_fullscreen_pipeline(
                    vulkanWindow, fullscreenPass.handle, fullscreenLayout.handle);

                gbuffer::update_descriptor_set(vulkanWindow, gbufferDescriptorSet, screenSampler, gBuffer);
            }

            framebuffers = swapchain::create_swapchain_framebuffers(vulkanWindow, fullscreenPass.handle);
            recreateSwapchain = false;
            // Swapchain image has not been acquired yet, proceed with the loop
        }

        // Skip frame if swapchain has no render area
        if (vulkanWindow.swapchainExtent.width == 0 || vulkanWindow.swapchainExtent.height == 0) {
            // Cover nasty case in which window is resized but swapchain recreation is not triggered
            // Can be triggered by minimising and maximising
            int glfwWidth, glfwHeight;
            glfwGetWindowSize(vulkanWindow.window, &glfwWidth, &glfwHeight);
            recreateSwapchain = vulkanWindow.swapchainExtent.width != glfwWidth ||
                                vulkanWindow.swapchainExtent.height != glfwHeight;
            continue;
        }

        // Query frame timestamp
        benchmark::query_timestamps(vulkanWindow, timestampPools[frameInFlightIndex], timestampBuffer);

        // Obtain frame time
        const auto frameTime = benchmark::extract_frame_time(timestampBuffer, timestampPeriod);

        // Signal UI for new frame
        ui::new_frame(state, frameTime);

        // Update state
        const auto now = cfg::Clock::now();
        const auto dt = std::chrono::duration_cast<cfg::Secondsf>(now - lastClock).count();
        lastClock = now;
        state::update_state(state, dt);

        // Prepare benchmarks file, if needed
        if (state.performing_benchmarks() && !benchmarksFile.is_open()) {
            benchmarksFile = benchmark::benchmarks_file(path::output_file_path(
                state.playback != nullptr ? state.playback->stem : sceneName, sceneTag, "csv"));
        }

        // Update uniforms
        const glsl::SceneUniform sceneUniform = scene::create_uniform(
            vulkanWindow.swapchainExtent.width, vulkanWindow.swapchainExtent.height, state);
        const glsl::ShadeUniform shadeUniform = shade::create_uniform(state);
        const glsl::SSRUniform ssrUniform = ssr::create_uniform(state);

        // Prepare Offscreen command buffer
        offscreen::prepare_offscreen_command_buffer(vulkanWindow, offscreenFence, offscreenCommandBuffer);

        // Record frame start timestamp command
        benchmark::record_pipeline_top_timestamp(offscreenCommandBuffer, timestampPools[frameInFlightIndex],
                                                 benchmark::TimestampQuery::frameStart);

        // Record Shadow commands
        shadow::record_commands(
            offscreenCommandBuffer,
            shadowPass.handle,
            shadowFramebuffer.handle,
            shadowOpaqueLayout.handle,
            shadowOpaquePipeline.handle,
            shadowAlphaLayout.handle,
            shadowAlphaPipeline.handle,
            sceneUBO.buffer,
            sceneUniform,
            sceneDescriptorSet,
            opaqueMeshes,
            alphaMeshes,
            materialDescriptorSets
        );

        // Record shadow end timestamp command
        benchmark::record_pipeline_bottom_timestamp(offscreenCommandBuffer, timestampPools[frameInFlightIndex],
                                                    benchmark::TimestampQuery::shadowEnd);

        // No need for explicity synchronisation here as Subpass dependencies guarantee it implicitly
        // See https://github.com/SaschaWillems/Vulkan/blob/master/examples/shadowmapping/shadowmapping.cpp#L312C1-L312C39

        // Record GBuffer start timestamp command
        benchmark::record_pipeline_top_timestamp(offscreenCommandBuffer, timestampPools[frameInFlightIndex],
                                                 benchmark::TimestampQuery::offscreenStart);

        // Record Offscreen commands
        offscreen::record_commands(
            offscreenCommandBuffer,
            offscreenPass.handle,
            offscreenFramebuffer.handle,
            offscreenLayout.handle,
            offscreenOpaquePipeline.handle,
            offscreenAlphaPipeline.handle,
            vulkanWindow.swapchainExtent,
            sceneUBO.buffer,
            sceneUniform,
            sceneDescriptorSet,
            shadeUbo.buffer,
            shadeUniform,
            shadeDescriptorSet,
            opaqueMeshes,
            alphaMeshes, materialStore.materials, materialDescriptorSets
        );

        // Record GBuffer end timestamp command
        benchmark::record_pipeline_bottom_timestamp(offscreenCommandBuffer, timestampPools[frameInFlightIndex],
                                                    benchmark::TimestampQuery::offscreenEnd);

#ifdef ENABLE_DIAGNOSTICS
        screenshot::record_screenshot_ready_event(offscreenCommandBuffer, screenshotReady);
#endif

        // Submit Offscreen commands
        offscreen::submit_commands(vulkanWindow, offscreenCommandBuffer, offscreenFinished, offscreenFence);

        // Acquire next swap chain image, without waiting for offscreen commands to finish
        const vkutils::Semaphore& swapchainImageAvailable = swapchainImagesAvailable[frameInFlightIndex];
        const std::uint32_t imageIndex = swapchain::acquire_swapchain_image(vulkanWindow, swapchainImageAvailable,
                                                                            recreateSwapchain);

        if (recreateSwapchain) {
            // Offscreen pass was submitted but offscreenFinished is not waited for
            // Need to wait on all semaphores that were started
            offscreen::wait_offscreen_early(vulkanWindow, offscreenFinished);
            continue;
        }

        // Retrieve per-frame pipeline resources
        assert(imageIndex < frameFences.size());
        const vkutils::Fence& frameFence = frameFences[frameInFlightIndex];

        assert(imageIndex < frameCommandBuffers.size());
        assert(imageIndex < framebuffers.size());

        const VkCommandBuffer frameCommandBuffer = frameCommandBuffers[frameInFlightIndex];
        const vkutils::Framebuffer& fullscreenFramebuffer = framebuffers[frameInFlightIndex];

        // Begin Fullscreen command buffer
        fullscreen::prepare_frame_command_buffer(vulkanWindow, frameFence, frameCommandBuffer);

        // Record deferred start timestamp command
        benchmark::record_pipeline_top_timestamp(frameCommandBuffer, timestampPools[frameInFlightIndex],
                                                 benchmark::TimestampQuery::deferredStart);

        // Record Fullscreen commands
        fullscreen::record_commands(
            frameCommandBuffer,
            fullscreenPass.handle,
            fullscreenFramebuffer.handle,
            fullscreenLayout.handle,
            fullscreenPipeline.handle,
            vulkanWindow.swapchainExtent,
            sceneUBO.buffer,
            sceneUniform,
            sceneDescriptorSet,
            shadeUbo.buffer,
            shadeUniform,
            shadeDescriptorSet,
            gbufferDescriptorSet,
            ssrUBO.buffer,
            ssrUniform,
            ssrDescriptorSet,
            environmentDescriptorSet
        );

        // Record frame end timestamp command
        benchmark::record_pipeline_bottom_timestamp(frameCommandBuffer, timestampPools[frameInFlightIndex],
                                                    benchmark::TimestampQuery::frameEnd);

        // Submit fullscreen commands, waits for both offscreenFinished and swapchainImageAvailable
        fullscreen::submit_frame_command_buffer(vulkanWindow, frameCommandBuffer,
                                                {offscreenFinished.handle, swapchainImageAvailable.handle},
                                                renderFinished.handle,
                                                frameFence);

#ifdef ENABLE_DIAGNOSTICS
        // Handle screenshot
        if (state.takeFrameScreenshot) {
            screenshot::take_screenshot(vulkanWindow, commandPool, vulkanWindow.swapImages[imageIndex],
                                        allocator, screenshotReady, path::output_file_path(sceneName, sceneTag, "png"));
        }
#endif

        // Render UI on top of everything
        ui::render(vulkanWindow, imageIndex, uiFence, uiCommandBuffer);

        // Present the results after renderFinished is signalled
        swapchain::present_results(vulkanWindow.presentQueue, vulkanWindow.swapchain, imageIndex,
                                   renderFinished.handle, recreateSwapchain);

        state.takeFrameScreenshot = false;
        frameInFlightIndex = (frameInFlightIndex + 1) % timestampPools.size();

        benchmark::process_frame(state, frameTime, benchmarksFile);
    }

    // Cleanup takes place automatically in the destructors, but we sill need
    // to ensure that all Vulkan commands have finished before that.
    vkDeviceWaitIdle(vulkanWindow.device);
    ui::destroy();
    return EXIT_SUCCESS;
} catch (const std::exception& exception) {
    std::fprintf(stderr, "\n");
    std::fprintf(stderr, "Error: %s\n", exception.what());
    return EXIT_FAILURE;
}
