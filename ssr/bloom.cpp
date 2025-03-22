#include "bloom.hpp"

#include <print>

#include "gbuffer.hpp"
#include "../vkutils/vkutil.hpp"

namespace bloom {
    BloomBuffer::BloomBuffer(const vkutils::VulkanWindow& window, const vkutils::Allocator& allocator) {
        const auto [windowWidth, windowHeight] = window.swapchainExtent;

        auto brightPassImage = vkutils::create_image(allocator, bloomFormat, VK_IMAGE_TYPE_2D,
                                                     windowWidth, windowHeight, 1, 1,
                                                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                     VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

        auto brightPassView = vkutils::image_to_view(window, brightPassImage.image, VK_IMAGE_VIEW_TYPE_2D,
                                                     bloomFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        auto blurPass1Image = vkutils::create_image(allocator, bloomFormat, VK_IMAGE_TYPE_2D,
                                                    windowWidth, windowHeight, 1, 1,
                                                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                    VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

        auto blurPass1View = vkutils::image_to_view(window, blurPass1Image.image, VK_IMAGE_VIEW_TYPE_2D,
                                                    bloomFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        auto blurPass2Image = vkutils::create_image(allocator, bloomFormat, VK_IMAGE_TYPE_2D,
                                                    windowWidth, windowHeight, 1, 1,
                                                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                    VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

        auto blurPass2View = vkutils::image_to_view(window, blurPass2Image.image, VK_IMAGE_VIEW_TYPE_2D,
                                                    bloomFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        this->brightPassBuffer = std::make_pair(std::move(brightPassImage), std::move(brightPassView));
        this->blurPass1Buffer = std::make_pair(std::move(blurPass1Image), std::move(blurPass1View));
        this->blurPass2Buffer = std::make_pair(std::move(blurPass2Image), std::move(blurPass2View));
    }

    BloomBuffer::BloomBuffer(BloomBuffer&& other) noexcept
        : brightPassBuffer(std::exchange(other.brightPassBuffer, {})),
          blurPass1Buffer(std::exchange(other.blurPass1Buffer, {})),
          blurPass2Buffer(std::exchange(other.blurPass2Buffer, {})) {
    }

    BloomBuffer& BloomBuffer::operator=(BloomBuffer&& other) noexcept {
        if (this != &other) {
            std::swap(brightPassBuffer, other.brightPassBuffer);
            std::swap(blurPass1Buffer, other.blurPass1Buffer);
            std::swap(blurPass2Buffer, other.blurPass2Buffer);
        }
        return *this;
    }

    vkutils::Pipeline create_bright_pass_pipeline(const vkutils::VulkanWindow& window, VkRenderPass renderPass,
                                                  VkPipelineLayout pipelineLayout) {
        std::print("create_bright_pass_pipeline not implemented yet");
        exit(1);
    }

    vkutils::Pipeline create_blur_pipeline(const vkutils::VulkanWindow& window, VkRenderPass renderPass,
                                           VkPipelineLayout pipelineLayout) {
        std::print("create_blur_pipeline not implemented yet");
        exit(1);
    }
}
