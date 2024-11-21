#include "vkbuffer.hpp"

#include <cassert>
#include <utility>

#include "error.hpp"
#include "to_string.hpp"

namespace vkutils {
    Buffer::Buffer() noexcept = default;

    Buffer::~Buffer() {
        if (VK_NULL_HANDLE != buffer) {
            assert(VK_NULL_HANDLE != mAllocator);
            assert(VK_NULL_HANDLE != allocation);
            vmaDestroyBuffer(mAllocator, buffer, allocation);
        }
    }

    Buffer::Buffer(const VmaAllocator allocator, const VkBuffer buffer, const VmaAllocation allocation) noexcept
        : buffer(buffer),
          allocation(allocation),
          mAllocator(allocator) {
    }

    Buffer::Buffer(Buffer&& aOther) noexcept
        : buffer(std::exchange(aOther.buffer, VK_NULL_HANDLE)),
          allocation(std::exchange(aOther.allocation, VK_NULL_HANDLE)),
          mAllocator(std::exchange(aOther.mAllocator, VK_NULL_HANDLE)) {
    }

    Buffer& Buffer::operator=(Buffer&& aOther) noexcept {
        std::swap(buffer, aOther.buffer);
        std::swap(allocation, aOther.allocation);
        std::swap(mAllocator, aOther.mAllocator);
        return *this;
    }
}

namespace vkutils {
    Buffer create_buffer(const Allocator& allocator,
                         const VkDeviceSize deviceSize,
                         const VkBufferUsageFlags bufferUsageFlag,
                         const VmaAllocationCreateFlags memoryFlags,
                         const VmaMemoryUsage memoryUsage) {
        const VkBufferCreateInfo bufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = deviceSize,
            .usage = bufferUsageFlag
        };

        const VmaAllocationCreateInfo allocInfo{
            .flags = memoryFlags,
            .usage = memoryUsage
        };

        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;

        if (const auto res = vmaCreateBuffer(allocator.allocator, &bufferInfo, &allocInfo, &buffer, &allocation,
                                             nullptr);
            VK_SUCCESS != res) {
            throw Error("Unable to allocate buffer.\n"
                        "vmaCreateBuffer() returned %s", to_string(res).c_str()
            );
        }

        return Buffer(allocator.allocator, buffer, allocation);
    }
}
