#include "environment.hpp"

#include <array>
#include <cstring>
#include <filesystem>
#include <stb_image.h>
#include <utility>

#include "../vkutils/error.hpp"
#include "../vkutils/vkbuffer.hpp"
#include "../vkutils/to_string.hpp"
#include "../vkutils/vkutil.hpp"

#include "texture.hpp"

namespace environment {
    constexpr std::uint32_t CUBE_FACES_AMOUNT = 6;

    vkutils::Image cube_map_image(const vkutils::VulkanContext& context,
                                  const VkFormat format,
                                  const std::array<texture::Texture, CUBE_FACES_AMOUNT>& faceTextures,
                                  const vkutils::Allocator& allocator,
                                  const vkutils::CommandPool& loadCommandPool) {
        // Create staging buffer and copy faces data to it
        // Assume all the faces share width, height & channels
        const auto cubeSizeInBytes = faceTextures.size() * faceTextures.begin()->sizeInBytes();

        const auto staging = vkutils::create_buffer(allocator, cubeSizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
        uint8_t* sptr = nullptr;
        if (const auto res = vmaMapMemory(allocator.allocator, staging.allocation, reinterpret_cast<void**>(&sptr));
            VK_SUCCESS != res) {
            throw vkutils::Error("Mapping memory for writing\n"
                                 "vmaMapMemory() returned %s", vkutils::to_string(res).c_str()
            );
        }

        // Map all face textures into a single buffer
        const auto textureSizeInBytes = faceTextures.begin()->sizeInBytes();
        for (auto i = 0; i < faceTextures.size(); ++i) {
            uint32_t faceTextureSize = faceTextures[i].sizeInBytes();
            const auto faceTextureOffset = i * faceTextureSize;
            std::memcpy(sptr + faceTextureOffset, faceTextures[i].data, faceTextureSize);
        }
        vmaUnmapMemory(allocator.allocator, staging.allocation);

        // Create cube map image
        const uint32_t faceWidth = faceTextures.begin()->width,
                       faceHeight = faceTextures.begin()->height;
        const auto mipLevels = vkutils::compute_mip_level_count(faceWidth, faceHeight);
        auto cubeImage = vkutils::create_image(allocator, format, VK_IMAGE_TYPE_2D,
                                               faceWidth, faceHeight, mipLevels, static_cast<std::uint32_t>(faceTextures.size()),
                                               VK_IMAGE_USAGE_SAMPLED_BIT |
                                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                               VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                               VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                               0, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

        // Create command buffer for data upload and begin recording
        VkCommandBuffer commandBuffer = vkutils::alloc_command_buffer(context, loadCommandPool.handle);

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
        vkutils::image_barrier(commandBuffer, cubeImage.image,
                               0,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VkImageSubresourceRange{
                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                   0, mipLevels,
                                   0, static_cast<std::uint32_t>(faceTextures.size())
                               }
        );

        std::array<VkBufferImageCopy, CUBE_FACES_AMOUNT> copyRegions;
        for (std::uint32_t i = 0; i < faceTextures.size(); ++i) {
            copyRegions[i] = {
                .bufferOffset = i * textureSizeInBytes,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = VkImageSubresourceLayers{
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    0, i, 1
                },
                .imageOffset = VkOffset3D{0, 0, 0},
                .imageExtent = VkExtent3D{
                    .width = faceWidth,
                    .height = faceHeight,
                    .depth = 1
                }
            };
        }

        vkCmdCopyBufferToImage(commandBuffer, staging.buffer, cubeImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               copyRegions.size(), copyRegions.data());

        // Transition base level to TRANSFER SRC OPTIMAL
        vkutils::image_barrier(commandBuffer, cubeImage.image,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_ACCESS_TRANSFER_READ_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VkImageSubresourceRange{
                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                   0, 1,
                                   0, static_cast<std::uint32_t>(faceTextures.size())
                               }
        );

        // Process all layers (i.e.: faces)
        for (std::uint32_t layer = 0; layer < faceTextures.size(); ++layer) {
            // Process all mip levels of layer
            std::uint32_t mipWidth = faceWidth, mipHeight = faceHeight;
            for (std::uint32_t level = 1; level < mipLevels; ++level) {
                // Blit previous mipmap level (=level-1) to the current level. Note that the loop starts at level = 1.
                // Level = 0 is the base level that we initialied before the loop.
                VkImageBlit blit{};
                blit.srcSubresource = VkImageSubresourceLayers{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = level - 1,
                    .baseArrayLayer = layer,
                    .layerCount = 1
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
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = level,
                    .baseArrayLayer = layer,
                    .layerCount = 1
                };
                blit.dstOffsets[0] = {0, 0, 0};
                blit.dstOffsets[1] = {static_cast<std::int32_t>(mipWidth), static_cast<std::int32_t>(mipHeight), 1};

                vkCmdBlitImage(commandBuffer,
                               cubeImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               cubeImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1, &blit,
                               VK_FILTER_LINEAR
                );

                // Transition mip level to TRANSFER SRC OPTIMAL for the next iteration. (Technically this is
                // unnecessary for the last mip level, but transitioning it as well simplifes the final barrier following the
                // loop).
                vkutils::image_barrier(commandBuffer, cubeImage.image,
                                       VK_ACCESS_TRANSFER_WRITE_BIT,
                                       VK_ACCESS_TRANSFER_READ_BIT,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                                       VkImageSubresourceRange{
                                           VK_IMAGE_ASPECT_COLOR_BIT,
                                           level, 1,
                                           layer, 1
                                       }
                );
            }
        }

        // Whole image is currently in the TRANSFER SRC OPTIMAL layout. To use the image as a texture from
        // which we sample, it must be in the SHADER READ ONLY OPTIMAL layout.
        vkutils::image_barrier(commandBuffer, cubeImage.image,
                               VK_ACCESS_TRANSFER_READ_BIT,
                               VK_ACCESS_SHADER_READ_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                               VkImageSubresourceRange{
                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                   0, mipLevels,
                                   0, static_cast<std::uint32_t>(faceTextures.size())
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

        return cubeImage;
    }

    std::pair<vkutils::Image, vkutils::ImageView> load_cube_map(const vkutils::VulkanContext& context,
                                                                const vkutils::Allocator& allocator,
                                                                const vkutils::CommandPool& loadCommandPool) {
        // Validate skybox path
        const std::filesystem::path skyboxPath(ASSETS_SRC_PATH_"/environment/skybox/");

        std::printf("Loading skybox: %s\n", skyboxPath.string().c_str());

        if (!exists(skyboxPath) || !is_directory(skyboxPath)) {
            throw vkutils::Error("Could not find the skybox directory");
        }

        // Load textures, fails if not found
        const std::array faceTextures{
            texture::Texture(skyboxPath / "right.jpg"),
            texture::Texture(skyboxPath / "left.jpg"),
            texture::Texture(skyboxPath / "bottom.jpg"),
            texture::Texture(skyboxPath / "top.jpg"),
            texture::Texture(skyboxPath / "front.jpg"),
            texture::Texture(skyboxPath / "back.jpg")
        };

        // Return cube map image & view
        auto cubeImage = cube_map_image(context, VK_FORMAT_R8G8B8A8_UNORM, faceTextures, allocator, loadCommandPool);
        auto cubeView = vkutils::image_to_view(context, cubeImage.image, VK_IMAGE_VIEW_TYPE_CUBE,
                                               VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

        return {std::move(cubeImage), std::move(cubeView)};
    }

    vkutils::DescriptorSetLayout create_descriptor_layout(const vkutils::VulkanContext& context) {
        constexpr std::array bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0, // layout(set = ..., binding = 0)
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
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
            throw vkutils::Error("Unable to create environment descriptor set layout\n"
                                 "vkCreateDescriptorSetLayout() returned %s", vkutils::to_string(res).c_str()
            );
        }

        return vkutils::DescriptorSetLayout(context.device, layout);
    }

    void update_descriptor_set(const vkutils::VulkanContext& context,
                               const VkDescriptorSet environmentDescriptorSet,
                               const vkutils::ImageView& cubeMapView,
                               const vkutils::Sampler& sampler) {
        const VkDescriptorImageInfo cubeSamplerDescriptor{
            .sampler = sampler.handle,
            .imageView = cubeMapView.handle,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        const VkWriteDescriptorSet cubeDescriptorWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = environmentDescriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &cubeSamplerDescriptor,
        };

        vkUpdateDescriptorSets(context.device, 1, &cubeDescriptorWrite, 0, nullptr);
    }
}
