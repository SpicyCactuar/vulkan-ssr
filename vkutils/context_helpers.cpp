#include "context_helpers.hxx"

#include "error.hpp"
#include "to_string.hpp"

namespace vkutils::detail {
    std::unordered_set<std::string> get_instance_layers() {
        std::uint32_t numLayers = 0;
        if (const auto res = vkEnumerateInstanceLayerProperties(&numLayers, nullptr); VK_SUCCESS != res) {
            throw vkutils::Error("Unable to enumerate layers\n"
                                 "vkEnumerateInstanceLayerProperties() returned %s\n", vkutils::to_string(res).c_str()
            );
        }

        std::vector<VkLayerProperties> layers(numLayers);
        if (const auto res = vkEnumerateInstanceLayerProperties(&numLayers, layers.data()); VK_SUCCESS != res) {
            throw vkutils::Error("Unable to get layer properties\n"
                                 "vkEnumerateInstanceLayerProperties() returned %s", vkutils::to_string(res).c_str()
            );
        }

        std::unordered_set<std::string> instanceLayers;
        for (const auto& layer : layers) {
            instanceLayers.insert(layer.layerName);
        }

        return instanceLayers;
    }

    std::unordered_set<std::string> get_instance_extensions() {
        std::uint32_t numExtensions = 0;
        if (const auto res = vkEnumerateInstanceExtensionProperties(nullptr, &numExtensions, nullptr);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to enumerate extensions\n"
                                 "vkEnumerateInstanceExtensionProperties() returned %s", vkutils::to_string(res).c_str()
            );
        }

        std::vector<VkExtensionProperties> extensions(numExtensions);
        if (const auto res = vkEnumerateInstanceExtensionProperties(nullptr, &numExtensions, extensions.data());
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to get extension properties\n"
                                 "vkEnumerateInstanceExtensionProperties() returned %s", vkutils::to_string(res).c_str()
            );
        }

        std::unordered_set<std::string> instanceExtensions;
        for (const auto& extension : extensions) {
            instanceExtensions.insert(extension.extensionName);
        }

        return instanceExtensions;
    }

    VkInstance create_instance(const std::vector<char const*>& enabledLayers,
                               const std::vector<char const*>& enabledInstanceExtensions, bool enableDebugUtils) {
        // Prepare debug messenger info
        VkDebugUtilsMessengerCreateInfoEXT debugInfo{};

        if (enableDebugUtils) {
            debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugInfo.messageSeverity =
                    /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | */
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugInfo.pfnUserCallback = &debug_util_callback;
            debugInfo.pUserData = nullptr;
        }

        // Prepare application info
        // The `apiVersion` is the *highest* version of Vulkan than the
        // application can use. We can therefore safely set it to 1.3, even if
        // we are not intending to use any 1.3 features (and want to run on
        // pre-1.3 implementations).
        VkApplicationInfo appInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "Vulkan Suntemple",
            .applicationVersion = 100,
            .apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0)
        };

        // Create instance
        VkInstanceCreateInfo instanceInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<std::uint32_t>(enabledLayers.size()),
            .ppEnabledLayerNames = enabledLayers.data(),
            .enabledExtensionCount = static_cast<std::uint32_t>(enabledInstanceExtensions.size()),
            .ppEnabledExtensionNames = enabledInstanceExtensions.data()
        };

        if (enableDebugUtils) {
            debugInfo.pNext = instanceInfo.pNext;
            instanceInfo.pNext = &debugInfo;
        }

        VkInstance instance;
        if (const auto res = vkCreateInstance(&instanceInfo, nullptr, &instance);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to create Vulkan instance\n"
                                 "vkCreateInstance() returned %s", vkutils::to_string(res).c_str()
            );
        }

        return instance;
    }
}

namespace vkutils::detail {
    VkDebugUtilsMessengerEXT create_debug_messenger(const VkInstance instance) {
        // Set up the debug messaging for the rest of the application
        constexpr VkDebugUtilsMessengerCreateInfoEXT debugInfo{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | */
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = &debug_util_callback,
            .pUserData = nullptr
        };

        VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
        if (const auto res = vkCreateDebugUtilsMessengerEXT(instance, &debugInfo, nullptr, &messenger);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to set up debug messenger\n"
                                 "vkCreateDebugUtilsMessengerEXT() returned %s", vkutils::to_string(res).c_str()
            );
        }

        return messenger;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL debug_util_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                       VkDebugUtilsMessageTypeFlagsEXT type,
                                                       const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                       void* /*userPtr*/) {
        std::fprintf(stderr, "%s (%s): %s (%d)\n%s\n--\n", vkutils::to_string(severity).c_str(),
                     vkutils::message_type_flags(type).c_str(), data->pMessageIdName, data->messageIdNumber,
                     data->pMessage);

        return VK_FALSE;
    }
}

namespace vkutils::detail {
    std::unordered_set<std::string> get_device_extensions(const VkPhysicalDevice physicalDevice) {
        std::uint32_t extensionCount = 0;
        if (const auto res = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to get device extension count\n"
                                 "vkEnumerateDeviceExtensionProperties() returned %s", vkutils::to_string(res).c_str()
            );
        }

        std::vector<VkExtensionProperties> extensions(extensionCount);
        if (const auto res = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount,
                                                                  extensions.data());
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to get device extensions\n"
                                 "vkEnumerateDeviceExtensionProperties() returned %s", vkutils::to_string(res).c_str()
            );
        }

        std::unordered_set<std::string> deviceExtensions;
        for (const auto& extension : extensions) {
            deviceExtensions.emplace(extension.extensionName);
        }

        return deviceExtensions;
    }
}
