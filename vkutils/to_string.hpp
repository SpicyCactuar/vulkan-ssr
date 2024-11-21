#pragma once

#include <string>
#include <cstdint>

#include <volk/volk.h>

namespace vkutils {
    std::string to_string(VkResult);

    std::string to_string(VkPhysicalDeviceType);

    std::string to_string(VkDebugUtilsMessageSeverityFlagBitsEXT);

    std::string to_string(VkFormat);

    std::string queue_flags(VkQueueFlags);

    std::string message_type_flags(VkDebugUtilsMessageTypeFlagsEXT);

    std::string memory_heap_flags(VkMemoryHeapFlags);

    std::string memory_property_flags(VkMemoryPropertyFlags);

    std::string driver_version(std::uint32_t vendorId, std::uint32_t driverVersion);
}
