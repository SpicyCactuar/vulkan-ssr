#include "screenshot.hpp"

#include <chrono>
#include <cstring>
#include <format>
#include <stb_image_write.h>

#include "../vkutils/error.hpp"
#include "../vkutils/to_string.hpp"
#include "../vkutils/vkimage.hpp"
#include "../vkutils/vkutil.hpp"

namespace screenshot {
    static constexpr std::uint32_t PIXEL_SIZE_IN_BYTES = 4;

    vkutils::Buffer create_screenshot_buffer(const vkutils::VulkanWindow& window,
                                             const vkutils::Allocator& allocator) {
        // RGBA swapchain dump
        const VkDeviceSize deviceSize = window.swapchainExtent.width * window.swapchainExtent.height *
                                        PIXEL_SIZE_IN_BYTES;
        return vkutils::create_buffer(allocator, deviceSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                      VMA_MEMORY_USAGE_AUTO_PREFER_HOST);
    }

    void record_screenshot_ready_event(const VkCommandBuffer commandBuffer,
                                       const vkutils::Event& screenshotReady) {
        // Set screenshot ready event
        vkCmdSetEvent(commandBuffer, screenshotReady.handle, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }

    void record_screenshot_commands(const vkutils::VulkanWindow& window,
                                    const VkImage frameImage,
                                    const VkImage screenshotImage,
                                    const VkCommandBuffer commandBuffer,
                                    const vkutils::Buffer& screenshotBuffer) {
        // Transition screenshot image VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        vkutils::image_barrier(commandBuffer, screenshotImage,
                               VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        // Transition frame image VK_IMAGE_LAYOUT_PRESENT_SRC_KHR -> VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        vkutils::image_barrier(commandBuffer, frameImage,
                               VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        // Blit the whole frame image
        // Since we use the same format for frame & screenshot image, blitting is available
        const VkOffset3D blitSize{
            .x = static_cast<std::int32_t>(window.swapchainExtent.width),
            .y = static_cast<std::int32_t>(window.swapchainExtent.height),
            .z = 1
        };

        const VkImageBlit imageBlitRegion{
            .srcSubresource = VkImageSubresourceLayers{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1
            },
            .srcOffsets = {
                VkOffset3D{0, 0, 0},
                blitSize
            },
            .dstSubresource = VkImageSubresourceLayers{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1
            },
            .dstOffsets = {
                VkOffset3D{0, 0, 0},
                blitSize
            }
        };

        // Issue the blit command
        vkCmdBlitImage(
            commandBuffer,
            frameImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            screenshotImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &imageBlitRegion, VK_FILTER_NEAREST);

        // Transition screenshot image VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -> VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        vkutils::image_barrier(commandBuffer, screenshotImage,
                               VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        // Transition frame image VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL -> VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        vkutils::image_barrier(commandBuffer, frameImage,
                               VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                               VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        // Copy image to our download buffer
        constexpr VkImageSubresourceLayers layers{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        };

        const VkBufferImageCopy copy{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = layers,
            .imageOffset = VkOffset3D{0, 0, 0},
            .imageExtent = VkExtent3D{window.swapchainExtent.width, window.swapchainExtent.height, 1}
        };

        vkCmdCopyImageToBuffer(commandBuffer, screenshotImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               screenshotBuffer.buffer, 1, &copy);
    }

    void write_screenshot_file(const vkutils::VulkanWindow& window,
                               const vkutils::Allocator& allocator,
                               const vkutils::Buffer& screenshotBuffer,
                               const std::filesystem::path& screenshotPath) {
        const auto [frameWidth, frameHeight] = window.swapchainExtent;
        const auto dataSizeInByes = frameWidth * frameHeight * PIXEL_SIZE_IN_BYTES;
        void* dataPointer = nullptr;
        if (const auto res = vmaMapMemory(allocator.allocator, screenshotBuffer.allocation, &dataPointer);
            VK_SUCCESS != res) {
            throw vkutils::Error("Mapping memory for writing frame data\n"
                                 "vmaMapMemory() returned %s", vkutils::to_string(res).c_str()
            );
        }

        std::vector<std::byte> buffer(dataSizeInByes);
        std::memcpy(buffer.data(), dataPointer, dataSizeInByes);
        assert(dataPointer != nullptr);

        // Why the extra copy? dataPtr points into a special memory region. This memory region may be created
        // uncached (e.g.: reads bypass CPU caches). Streaming out of such memory is OK - so memcpy will touch
        // each byte exactly once. Reading multiple times from the memory, which the compression method likely does,
        // is significantly more expensive.
        //
        // In one test, passing dataPtr directly to stbi write png() takes about 4.5s, whereas using the extra buffer
        // reduces this time to 0.5s.
        //
        // To avoid the extra copy, we could request memory with the VK MEMORY PROPERTY HOST CACHED
        // property in addition. However, not all devices support this, and it may have other overheads (i.e., the
        // device/driver likely needs to snoop on the CPU caches, similar to HOST COHERENT).
        vmaUnmapMemory(allocator.allocator, screenshotBuffer.allocation);

        // Write file
        if (!stbi_write_png(screenshotPath.string().c_str(), frameWidth, frameHeight, 4,
                            buffer.data(), frameWidth * PIXEL_SIZE_IN_BYTES)) {
            throw vkutils::Error("Unable to write screenshot image: %s\n"
                                 "stbi_write_png() returned error", screenshotPath.string().c_str());
        }

        std::printf("Output image to: %s\n", screenshotPath.string().c_str());
    }

    void take_screenshot(const vkutils::VulkanWindow& window,
                         const vkutils::CommandPool& commandPool,
                         const VkImage frameImage,
                         const vkutils::Allocator& allocator,
                         const vkutils::Event& screenshotReady,
                         const std::filesystem::path& screenshotPath) {
        // Create fence
        const vkutils::Fence fence = vkutils::create_fence(window);

        // Create screenshot image
        const vkutils::Image screenshotImage = vkutils::create_image(
            allocator, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TYPE_2D,
            window.swapchainExtent.width, window.swapchainExtent.height, 1,
            1,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST
        );

        // Create screenshot buffer
        const vkutils::Buffer screenshotBuffer = screenshot::create_screenshot_buffer(window, allocator);

        // Create screenshot command buffer
        VkCommandBuffer commandBuffer = vkutils::alloc_command_buffer(window, commandPool.handle);

        // Begin command recording
        constexpr VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };

        if (const auto res = vkBeginCommandBuffer(commandBuffer, &beginInfo); VK_SUCCESS != res) {
            throw vkutils::Error("Unable to begin recording screenshot command buffer\n"
                                 "vkBeginCommandBuffer() returned %s", vkutils::to_string(res).c_str()
            );
        }

        record_screenshot_commands(window, frameImage, screenshotImage.image, commandBuffer, screenshotBuffer);

        vkCmdWaitEvents(commandBuffer, 1, &screenshotReady.handle,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        0, nullptr, 0, nullptr, 0, nullptr);

        // End command recording
        if (const auto res = vkEndCommandBuffer(commandBuffer); VK_SUCCESS != res) {
            throw vkutils::Error("Unable to end recording screenshot command buffer\n"
                                 "vkEndCommandBuffer() returned %s", vkutils::to_string(res).c_str()
            );
        }

        // Submit command buffer
        constexpr VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        const VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = &waitStage,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer
        };

        if (const auto res = vkQueueSubmit(window.graphicsQueue, 1, &submitInfo, fence.handle);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to submit screenshot command buffer to queue\n"
                                 "vkQueueSubmit() returned %s", vkutils::to_string(res).c_str()
            );
        }

        // Wait for the fence to ensure the command buffer has completed execution
        if (const auto res = vkWaitForFences(window.device, 1, &fence.handle, VK_TRUE, UINT64_MAX);
            res != VK_SUCCESS) {
            throw vkutils::Error("Unable to wait for screenshot command buffer fence\n"
                                 "vkWaitForFences() returned %s", vkutils::to_string(res).c_str()
            );
        }

        // Write screenshot
        write_screenshot_file(window, allocator, screenshotBuffer, screenshotPath);
    }
}
