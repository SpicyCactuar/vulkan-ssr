#include "swapchain.hpp"

#include <array>

#include "../vkutils/error.hpp"
#include "../vkutils/to_string.hpp"

namespace swapchain {
    std::vector<vkutils::Framebuffer> create_swapchain_framebuffers(const vkutils::VulkanWindow& window,
                                                                    const VkRenderPass renderPass) {
        std::vector<vkutils::Framebuffer> framebuffers;
        framebuffers.reserve(window.swapViews.size());

        for (std::size_t i = 0; i < window.swapViews.size(); ++i) {
            const std::array attachments{
                window.swapViews[i]
            };

            const VkFramebufferCreateInfo framebufferInfo{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .flags = 0, // normal framebuffer
                .renderPass = renderPass,
                .attachmentCount = attachments.size(),
                .pAttachments = attachments.data(),
                .width = window.swapchainExtent.width,
                .height = window.swapchainExtent.height,
                .layers = 1
            };

            VkFramebuffer fb = VK_NULL_HANDLE;
            if (const auto res = vkCreateFramebuffer(window.device, &framebufferInfo, nullptr, &fb);
                VK_SUCCESS != res) {
                throw vkutils::Error("Unable to create framebuffer for swap chain image %zu\n"
                                     "vkCreateFramebuffer() returned %s", i, vkutils::to_string(res).c_str()
                );
            }
            framebuffers.emplace_back(window.device, fb);
        }

        assert(window.swapViews.size() == framebuffers.size());
        return framebuffers;
    }

    std::uint32_t acquire_swapchain_image(const vkutils::VulkanWindow& vulkanWindow,
                                          const vkutils::Semaphore& imageAvailable,
                                          bool& needToRecreateSwapchain) {
        std::uint32_t imageIndex = 0;

        const auto acquireRes = vkAcquireNextImageKHR(
            vulkanWindow.device,
            vulkanWindow.swapchain,
            std::numeric_limits<std::uint64_t>::max(),
            imageAvailable.handle,
            VK_NULL_HANDLE,
            &imageIndex
        );
        if (VK_SUBOPTIMAL_KHR == acquireRes || VK_ERROR_OUT_OF_DATE_KHR == acquireRes) {
            // This occurs when acquired image is not conditioned well and needs to be recreated.
            // For example, when the size has changed.
            // In this case we need to recreate the swap chain to match the new dimensions.
            needToRecreateSwapchain = true;
            return true;
        }
        if (VK_SUCCESS != acquireRes) {
            throw vkutils::Error("Unable to acquire next swapchain image\n"
                                 "vkAcquireNextImageKHR() returned %s", vkutils::to_string(acquireRes).c_str()
            );
        }

        return imageIndex;
    }

    void present_results(const VkQueue presentQueue,
                         VkSwapchainKHR swapchain,
                         std::uint32_t imageIndex,
                         VkSemaphore renderFinished,
                         bool& needToRecreateSwapchain) {
        // Present the results
        const VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderFinished,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &imageIndex,
            .pResults = nullptr
        };

        if (const auto presentRes = vkQueuePresentKHR(presentQueue, &presentInfo);
            VK_SUBOPTIMAL_KHR == presentRes || VK_ERROR_OUT_OF_DATE_KHR == presentRes) {
            needToRecreateSwapchain = true;
        } else if (VK_SUCCESS != presentRes) {
            throw vkutils::Error("Unable present swapchain image %u\n"
                                 "vkQueuePresentKHR() returned %s", imageIndex, vkutils::to_string(presentRes).c_str()
            );
        }
    }
}
