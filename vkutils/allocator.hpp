#pragma once

#include <volk/volk.h>
#include <vk_mem_alloc.h>

#include "vulkan_context.hpp"

namespace vkutils {
    class Allocator {
    public:
        Allocator() noexcept, ~Allocator();

        explicit Allocator(VmaAllocator) noexcept;

        Allocator(const Allocator&) = delete;

        Allocator& operator=(const Allocator&) = delete;

        Allocator(Allocator&&) noexcept;

        Allocator& operator =(Allocator&&) noexcept;

        VmaAllocator allocator = VK_NULL_HANDLE;
    };

    Allocator create_allocator(const VulkanContext&);
}
