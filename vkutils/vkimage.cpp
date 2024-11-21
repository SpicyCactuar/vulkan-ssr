#include "vkimage.hpp"

#include <vector>
#include <utility>
#include <algorithm>

#include <cassert>

#include "error.hpp"
#include "vkutil.hpp"
#include "vkbuffer.hpp"
#include "to_string.hpp"

namespace vkutils {
    Image::Image() noexcept = default;

    Image::~Image() {
        if (VK_NULL_HANDLE != image) {
            assert(VK_NULL_HANDLE != mAllocator);
            assert(VK_NULL_HANDLE != allocation);
            vmaDestroyImage(mAllocator, image, allocation);
        }
    }

    Image::Image(const VmaAllocator allocator, const VkImage image, const VmaAllocation allocation) noexcept
        : image(image), allocation(allocation), mAllocator(allocator) {
    }

    Image::Image(Image&& other) noexcept
        : image(std::exchange(other.image, VK_NULL_HANDLE)),
          allocation(std::exchange(other.allocation, VK_NULL_HANDLE)),
          mAllocator(std::exchange(other.mAllocator, VK_NULL_HANDLE)) {
    }

    Image& Image::operator=(Image&& other) noexcept {
        std::swap(image, other.image);
        std::swap(allocation, other.allocation);
        std::swap(mAllocator, other.mAllocator);
        return *this;
    }
}

namespace vkutils {
    vkutils::Image create_image(const vkutils::Allocator& allocator, const VkFormat format, const VkImageType type,
                                const std::uint32_t width, const std::uint32_t height,
                                const std::uint32_t mipLevels,
                                const std::uint32_t layers,
                                const VkImageUsageFlags imageUsageFlags, const VmaMemoryUsage memoryUsageFlags,
                                const VmaAllocationCreateFlags allocationFlags,
                                const VkImageCreateFlags imageCreateFlags) {
        const VkImageCreateInfo imageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .flags = imageCreateFlags,
            .imageType = type,
            .format = format,
            .extent = {
                .width = width,
                .height = height,
                .depth = 1
            },
            .mipLevels = mipLevels,
            .arrayLayers = layers,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = imageUsageFlags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };

        const VmaAllocationCreateInfo allocInfo{
            .flags = allocationFlags,
            .usage = memoryUsageFlags
        };

        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;

        if (const auto res = vmaCreateImage(allocator.allocator, &imageCreateInfo, &allocInfo, &image, &allocation,
                                            nullptr);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to allocate image.\n"
                                 "vmaCreateImage() returned %s", vkutils::to_string(res).c_str()
            );
        }

        return vkutils::Image(allocator.allocator, image, allocation);
    }

    vkutils::Image create_texture_image(const vkutils::Allocator& allocator,
                                        const std::uint32_t width,
                                        const std::uint32_t height,
                                        const VkFormat format,
                                        const VkImageUsageFlags imageUsageFlags) {
        return create_image(allocator, format, VK_IMAGE_TYPE_2D,
                            width, height, vkutils::compute_mip_level_count(width, height), 1,
                            imageUsageFlags, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    }

    std::uint32_t compute_mip_level_count(const std::uint32_t width, const std::uint32_t height) {
        const std::uint32_t bits = width | height;
        const std::uint32_t leadingZeros = std::countl_zero(bits);
        return 32 - leadingZeros;
    }
}
