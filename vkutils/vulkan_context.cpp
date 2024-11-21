#include "vulkan_context.hpp"

#include <vector>
#include <utility>

#include "to_string.hpp"

namespace vkutils {
    // VulkanContext
    VulkanContext::VulkanContext() = default;

    VulkanContext::~VulkanContext() {
        // Device-related objects
        if (VK_NULL_HANDLE != device) {
            vkDestroyDevice(device, nullptr);
        }

        // Instance-related objects
        if (VK_NULL_HANDLE != debugMessenger) {
            vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        if (VK_NULL_HANDLE != instance) {
            vkDestroyInstance(instance, nullptr);
        }
    }

    VulkanContext::VulkanContext(VulkanContext&& other) noexcept
    /* See https://en.cppreference.com/w/cpp/utility/exchange */
        : instance(std::exchange(other.instance, VK_NULL_HANDLE)),
          physicalDevice(std::exchange(other.physicalDevice, VK_NULL_HANDLE)),
          device(std::exchange(other.device, VK_NULL_HANDLE)),
          graphicsFamilyIndex(other.graphicsFamilyIndex),
          graphicsQueue(std::exchange(other.graphicsQueue, VK_NULL_HANDLE)),
          debugMessenger(std::exchange(other.debugMessenger, VK_NULL_HANDLE)) {
    }

    VulkanContext& VulkanContext::operator=(VulkanContext&& other) noexcept {
        /* We can't just copy over the data from other, as we need to ensure that
         * any potential objects help by `this` are destroyed properly. Swapping
         * the data of `this` with other will do so: the data of `this` ends up in
         * other, and is subsequently destroyed properly by other's destructor.
         *
         * Advantages are that the move-operation is quite cheap and can trivially
         * be `noexcept`. Disadvantage is that the destruction of the resources
         * held by `this` is delayed until other's destruction.
         *
         * This is a somewhat common way of implementing move assignments.
         */
        std::swap(instance, other.instance);
        std::swap(physicalDevice, other.physicalDevice);
        std::swap(device, other.device);
        std::swap(graphicsFamilyIndex, other.graphicsFamilyIndex);
        std::swap(graphicsQueue, other.graphicsQueue);
        std::swap(debugMessenger, other.debugMessenger);
        return *this;
    }
}
