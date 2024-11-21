#pragma once

#include <string>
#include <vector>
#include <unordered_set>

#include <volk/volk.h>

/*
 * context_helpers.hxx is an internal header that defines functions used
 * internally by both vulkan_context.cpp and vulkan_window.cpp. These
 * functions were previously defined locally in vulkan_context.cpp
 */
namespace vkutils {
    namespace detail {
        std::unordered_set<std::string> get_instance_layers();

        std::unordered_set<std::string> get_instance_extensions();

        VkInstance create_instance(
            const std::vector<char const*>& enabledLayers = {},
            const std::vector<char const*>& enabledInstanceExtensions = {},
            bool enableDebugUtils = false
        );

        VkDebugUtilsMessengerEXT create_debug_messenger(VkInstance);

        VKAPI_ATTR VkBool32 VKAPI_CALL debug_util_callback(VkDebugUtilsMessageSeverityFlagBitsEXT,
                                                           VkDebugUtilsMessageTypeFlagsEXT,
                                                           const VkDebugUtilsMessengerCallbackDataEXT*, void*);


        std::unordered_set<std::string> get_device_extensions(VkPhysicalDevice);
    }
}
