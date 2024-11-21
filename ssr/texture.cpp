#include "texture.hpp"

#include <iostream>
#include <utility>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "baked_model.hpp"
#include "../vkutils/error.hpp"
#include "../vkutils/vkbuffer.hpp"
#include "../vkutils/to_string.hpp"
#include "../vkutils/vkutil.hpp"

namespace texture {
    Texture::Texture(const std::string& path) : path(path) {
        std::cout << "Loading texture: " << path << "\n";

        // Flip images vertically by default. Vulkan expects the first scanline to be the bottom-most scanline.
        // PNG et al. instead define the first scanline to be the top-most one.
        stbi_set_flip_vertically_on_load(1);

        // Load base image
        int baseWidthi, baseHeighti, baseChannelsi;
        const auto rawPath = path.c_str();
        data = stbi_load(rawPath, &baseWidthi, &baseHeighti, &baseChannelsi, 4 /* want 4 channels = RGBA */);

        if (data == nullptr) {
            throw vkutils::Error("%s: unable to load texture base image (%s)", rawPath, 0, stbi_failure_reason());
        }

        width = static_cast<std::uint32_t>(baseWidthi);
        height = static_cast<std::uint32_t>(baseHeighti);
    }

    Texture::Texture(const std::filesystem::path& path) : Texture(path.string()) {
    }

    Texture::Texture(Texture&& other) noexcept : path(std::exchange(other.path, "")),
                                                 data(std::exchange(other.data, nullptr)),
                                                 width(std::exchange(other.width, 0)),
                                                 height(std::exchange(other.height, 0)) {
    }

    Texture& Texture::operator=(Texture&& other) noexcept {
        if (this != &other) {
            std::swap(path, other.path);
            std::swap(data, other.data);
            std::swap(width, other.width);
            std::swap(height, other.height);
        }
        return *this;
    }

    std::uint32_t Texture::sizeInBytes() const {
        // width * height * |[r, g, b, a]|
        return width * height * 4;
    }

    Texture::~Texture() {
        if (data != nullptr) {
            stbi_image_free(data);
            data = nullptr;
        }
    }
}

namespace texture {
    vkutils::Image texture_to_image(const vkutils::VulkanContext& context,
                                    const Texture& texture,
                                    const VkFormat format,
                                    const vkutils::Allocator& allocator,
                                    const vkutils::CommandPool& loadCommandPool) {
        // Create staging buffer and copy image data to it
        const auto sizeInBytes = texture.sizeInBytes();

        const auto staging = create_buffer(allocator, sizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                           VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
        void* sptr = nullptr;
        if (const auto res = vmaMapMemory(allocator.allocator, staging.allocation, &sptr);
            VK_SUCCESS != res) {
            throw vkutils::Error("Mapping memory for writing\n"
                                 "vmaMapMemory() returned %s", vkutils::to_string(res).c_str()
            );
        }

        std::memcpy(sptr, texture.data, sizeInBytes);
        vmaUnmapMemory(allocator.allocator, staging.allocation);

        // Create image
        vkutils::Image image = create_texture_image(allocator, texture.width, texture.height, format,
                                                    VK_IMAGE_USAGE_SAMPLED_BIT |
                                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

        // Create command buffer for data upload and begin recording
        VkCommandBuffer commandBuffer = alloc_command_buffer(context, loadCommandPool.handle);

        constexpr VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = 0,
            .pInheritanceInfo = nullptr
        };

        if (const auto res = vkBeginCommandBuffer(commandBuffer, &beginInfo);
            VK_SUCCESS != res) {
            throw vkutils::Error("Beginning command buffer recording\n"
                                 "vkBeginCommandBuffer() returned %s", vkutils::to_string(res).c_str()
            );
        }

        // Transition whole image layout
        // When copying data to the image, the image’s layout must be TRANSFER DST OPTIMAL. The current
        // image layout is UNDEFINED (which is the initial layout the image was created in).
        const auto mipLevels = vkutils::compute_mip_level_count(texture.width, texture.height);
        vkutils::image_barrier(commandBuffer, image.image,
                               0,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VkImageSubresourceRange{
                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                   0, mipLevels,
                                   0, 1
                               }
        );

        // Upload data from staging buffer to image
        const VkBufferImageCopy copy{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = VkImageSubresourceLayers{
                VK_IMAGE_ASPECT_COLOR_BIT,
                0,
                0, 1
            },
            .imageOffset = VkOffset3D{0, 0, 0},
            .imageExtent = VkExtent3D{
                .width = texture.width,
                .height = texture.height,
                .depth = 1
            }
        };

        vkCmdCopyBufferToImage(commandBuffer, staging.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &copy);

        // Transition base level to TRANSFER SRC OPTIMAL
        vkutils::image_barrier(commandBuffer, image.image,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_ACCESS_TRANSFER_READ_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VkImageSubresourceRange{
                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                   0, 1,
                                   0, 1
                               }
        );

        // Process all mipmap levels
        uint32_t mipWidth = texture.width, mipHeight = texture.height;
        for (std::uint32_t level = 1; level < mipLevels; ++level) {
            // Blit previous mipmap level (=level-1) to the current level. Note that the loop starts at level = 1.
            // Level = 0 is the base level that we initialied before the loop.
            VkImageBlit blit{};
            blit.srcSubresource = VkImageSubresourceLayers{
                VK_IMAGE_ASPECT_COLOR_BIT,
                level - 1,
                0, 1
            };
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {static_cast<std::int32_t>(mipWidth), static_cast<std::int32_t>(mipHeight), 1};

            // Next mip level
            mipWidth >>= 1;
            if (mipWidth == 0) {
                mipWidth = 1;
            }
            mipHeight >>= 1;
            if (mipHeight == 0) {
                mipHeight = 1;
            }

            blit.dstSubresource = VkImageSubresourceLayers{
                VK_IMAGE_ASPECT_COLOR_BIT,
                level,
                0, 1
            };
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {static_cast<std::int32_t>(mipWidth), static_cast<std::int32_t>(mipHeight), 1};

            vkCmdBlitImage(commandBuffer,
                           image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blit,
                           VK_FILTER_LINEAR
            );

            // Transition mip level to TRANSFER SRC OPTIMAL for the next iteration. (Technically this is
            // unnecessary for the last mip level, but transitioning it as well simplifes the final barrier following the
            // loop).
            vkutils::image_barrier(commandBuffer, image.image,
                                   VK_ACCESS_TRANSFER_WRITE_BIT,
                                   VK_ACCESS_TRANSFER_READ_BIT,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   VkImageSubresourceRange{
                                       VK_IMAGE_ASPECT_COLOR_BIT,
                                       level, 1,
                                       0, 1
                                   }
            );
        }

        // Whole image is currently in the TRANSFER SRC OPTIMAL layout. To use the image as a texture from
        // which we sample, it must be in the SHADER READ ONLY OPTIMAL layout.
        vkutils::image_barrier(commandBuffer, image.image,
                               VK_ACCESS_TRANSFER_READ_BIT,
                               VK_ACCESS_SHADER_READ_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                               VkImageSubresourceRange{
                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                   0, mipLevels,
                                   0, 1
                               }
        );

        // End command recording
        if (const auto res = vkEndCommandBuffer(commandBuffer); VK_SUCCESS != res) {
            throw vkutils::Error("Ending command buffer recording\n"
                                 "vkEndCommandBuffer() returned %s", vkutils::to_string(res).c_str()
            );
        }

        // Submit command buffer and wait for commands to complete. Commands must have completed before we can
        // destroy the temporary resources, such as the staging buffers.
        const vkutils::Fence uploadComplete = create_fence(context);

        const VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer
        };

        if (const auto res = vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, uploadComplete.handle);
            VK_SUCCESS != res) {
            throw vkutils::Error("Submitting commands\n"
                                 "vkQueueSubmit() returned %s", vkutils::to_string(res).c_str()
            );
        }

        if (const auto res = vkWaitForFences(context.device, 1, &uploadComplete.handle, VK_TRUE,
                                             std::numeric_limits<std::uint64_t>::max()); VK_SUCCESS != res) {
            throw vkutils::Error("Waiting for upload to complete\n"
                                 "vkWaitForFences() returned %s", vkutils::to_string(res).c_str()
            );
        }

        // Return resulting image
        // Most temporary resources are destroyed automatically through their destructors. However, the command
        // buffer we must free manually.
        vkFreeCommandBuffers(context.device, loadCommandPool.handle, 1, &commandBuffer);

        return image;
    }
}
