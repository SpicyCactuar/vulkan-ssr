#pragma once

#include <volk/volk.h>
#include <vk_mem_alloc.h>

#include "allocator.hpp"

namespace vkutils {
    class Image {
    public:
        Image() noexcept, ~Image();

        explicit Image(VmaAllocator, VkImage = VK_NULL_HANDLE, VmaAllocation = VK_NULL_HANDLE) noexcept;

        Image(const Image&) = delete;

        Image& operator=(const Image&) = delete;

        Image(Image&&) noexcept;

        Image& operator=(Image&&) noexcept;

        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;

    private:
        VmaAllocator mAllocator = VK_NULL_HANDLE;
    };

    Image create_image(const Allocator& allocator, VkFormat format, VkImageType type,
                       std::uint32_t width, std::uint32_t height, std::uint32_t mipLevels,
                       std::uint32_t layers, VkImageUsageFlags imageUsageFlags,
                       VmaMemoryUsage memoryUsageFlags, VmaAllocationCreateFlags allocationFlags = 0,
                       VkImageCreateFlags imageCreateFlags = 0);

    Image create_texture_image(const Allocator& allocator,
                               std::uint32_t width, std::uint32_t height,
                               VkFormat format,
                               VkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    std::uint32_t compute_mip_level_count(std::uint32_t width, std::uint32_t height);
}
