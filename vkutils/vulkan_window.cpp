#include "vulkan_window.hpp"

#include <tuple>
#include <limits>
#include <vector>
#include <utility>
#include <optional>
#include <algorithm>
#include <unordered_set>

#include <cstdio>
#include <cassert>
#include <format>
#include <iostream>
#include <vulkan/vulkan_core.h>

#include "error.hpp"
#include "to_string.hpp"
#include "context_helpers.hxx"

#ifdef NDEBUG
static constexpr bool enableFramebufferReadback = false;
#else
static constexpr bool enableFramebufferReadback = true;
#endif

namespace {
    // The device selection process has changed somewhat w.r.t. the one used
    // earlier (e.g., with VulkanContext).
    VkPhysicalDevice select_device(VkInstance, VkSurfaceKHR);

    float score_device(VkPhysicalDevice, VkSurfaceKHR);

    std::optional<std::uint32_t> find_queue_family(VkPhysicalDevice, VkQueueFlags, VkSurfaceKHR = VK_NULL_HANDLE);

    VkDevice create_device(
        VkPhysicalDevice physicalDevice,
        const std::vector<std::uint32_t>& queueFamilies,
        const std::vector<char const*>& enabledDeviceExtensions = {}
    );

    std::vector<VkSurfaceFormatKHR> get_surface_formats(VkPhysicalDevice, VkSurfaceKHR);

    std::tuple<VkSwapchainKHR, VkFormat, VkExtent2D> create_swapchain(
        VkPhysicalDevice,
        VkSurfaceKHR,
        VkDevice,
        GLFWwindow*,
        const std::vector<std::uint32_t>& queueFamilyIndices = {},
        VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE
    );

    void get_swapchain_images(VkDevice, VkSwapchainKHR, std::vector<VkImage>&);

    void create_swapchain_image_views(VkDevice, VkFormat, std::vector<VkImage> const&, std::vector<VkImageView>&);
}

namespace vkutils {
    // VulkanWindow
    VulkanWindow::VulkanWindow() = default;

    VulkanWindow::~VulkanWindow() {
        // Device-related objects
        for (const auto view : swapViews) {
            vkDestroyImageView(device, view, nullptr);
        }

        if (VK_NULL_HANDLE != swapchain) {
            vkDestroySwapchainKHR(device, swapchain, nullptr);
        }

        // Window and related objects
        if (VK_NULL_HANDLE != surface) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
        }

        if (window) {
            glfwDestroyWindow(window);

            // The following assumes that we never create more than one window;
            // if there are multiple windows, destroying one of them would
            // unload the whole GLFW library. Nevertheless, this solution is
            // convenient when only dealing with one window (which we will do
            // in the exercises), as it ensure that GLFW is unloaded after all
            // window-related resources are.
            glfwTerminate();
        }
    }

    VulkanWindow::VulkanWindow(VulkanWindow&& other) noexcept
        : VulkanContext(std::move(other)),
          window(std::exchange(other.window, VK_NULL_HANDLE)),
          surface(std::exchange(other.surface, VK_NULL_HANDLE)),
          presentFamilyIndex(other.presentFamilyIndex),
          presentQueue(std::exchange(other.presentQueue, VK_NULL_HANDLE)),
          swapchain(std::exchange(other.swapchain, VK_NULL_HANDLE)),
          swapImages(std::move(other.swapImages)),
          swapViews(std::move(other.swapViews)),
          swapchainFormat(other.swapchainFormat),
          swapchainExtent(other.swapchainExtent) {
    }

    VulkanWindow& VulkanWindow::operator=(VulkanWindow&& other) noexcept {
        VulkanContext::operator=(std::move(other));
        std::swap(window, other.window);
        std::swap(surface, other.surface);
        std::swap(presentFamilyIndex, other.presentFamilyIndex);
        std::swap(presentQueue, other.presentQueue);
        std::swap(swapchain, other.swapchain);
        std::swap(swapImages, other.swapImages);
        std::swap(swapViews, other.swapViews);
        std::swap(swapchainFormat, other.swapchainFormat);
        std::swap(swapchainExtent, other.swapchainExtent);
        return *this;
    }

    VulkanWindow make_vulkan_window(const unsigned int windowWidth,
                                    const unsigned int windowHeight,
                                    const std::string& sceneName) {
        VulkanWindow vulkanWindow;

        // Initialize Volk
        if (const auto res = volkInitialize(); VK_SUCCESS != res) {
            throw vkutils::Error("Unable to load Vulkan API\n"
                                 "Volk returned error %s", vkutils::to_string(res).c_str()
            );
        }

        /**
         * Initialize GLFW and make sure this GLFW supports Vulkan.
         * Note: this assumes that we will not create multiple windows that exist concurrently. If multiple windows are
         * to be used, the glfwInit() and the glfwTerminate() (see destructor) calls should be moved elsewhere.
         */
        if (GLFW_TRUE != glfwInit()) {
            char const* glfwErrorMessage = nullptr;
            glfwGetError(&glfwErrorMessage);
            throw vkutils::Error("GLFW initialization failed: %s", glfwErrorMessage);
        }

        // Check for instance layers and extensions
        const auto supportedLayers = detail::get_instance_layers();
        const auto supportedExtensions = detail::get_instance_extensions();

        bool enableDebugUtils = false;

        std::vector<char const*> enabledLayers, enabledInstanceExtensions;

        /**
         * GLFW may require a number of instance extensions.
         * GLFW returns a bunch of pointers-to-strings; however, GLFW manages these internally, so we must not
         * free them ourselves. GLFW guarantees that the strings remain valid until GLFW terminates.
         */
        std::uint32_t requiredExtensionsCount = 0;
        char const** requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionsCount);
        for (std::uint32_t i = 0; i < requiredExtensionsCount; ++i) {
            if (!supportedExtensions.contains(requiredExtensions[i])) {
                throw vkutils::Error("GLFW/Vulkan: required instance extension %s not supported",
                                     requiredExtensions[i]);
            }
            enabledInstanceExtensions.emplace_back(requiredExtensions[i]);
        }

        // Validation layers support.
#		ifndef NDEBUG // debug builds only
        if (supportedLayers.contains("VK_LAYER_KHRONOS_validation")) {
            enabledLayers.emplace_back("VK_LAYER_KHRONOS_validation");
        }

        if (supportedExtensions.contains(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            enableDebugUtils = true;
            enabledInstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
#		endif // NDEBUG

        for (const auto& layer : enabledLayers) {
            std::printf("Enabling layer: %s\n", layer);
        }

        for (const auto& extension : enabledInstanceExtensions) {
            std::printf("Enabling instance extension: %s\n", extension);
        }

        // Create Vulkan instance
        vulkanWindow.instance = detail::create_instance(enabledLayers, enabledInstanceExtensions, enableDebugUtils);

        // Load rest of the Vulkan API
        volkLoadInstance(vulkanWindow.instance);

        // Formatter turned off for extra scopes to avoid bad indentation
        // Need to find the correct CLion property for this
        // @formatter:off
        {
            uint32_t loaderVersion;
            if (VkResult result = vkEnumerateInstanceVersion(&loaderVersion);
                result == VK_SUCCESS) {
                printf("Loaded instance using Volk (%d.%d.%d)\n",
                       VK_VERSION_MAJOR(loaderVersion),
                       VK_VERSION_MINOR(loaderVersion),
                       VK_VERSION_PATCH(loaderVersion));
            } else {
                fprintf(stderr, "Failed to retrieve Vulkan Loader Version.\n");
            }
        }
        // @formatter:on

        // Setup debug messenger
        if (enableDebugUtils) {
            vulkanWindow.debugMessenger = detail::create_debug_messenger(vulkanWindow.instance);
        }

        // Create GLFW Window and the Vulkan surface
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        vulkanWindow.window = glfwCreateWindow(windowWidth, windowHeight,
                                               std::format("Vulkan SSR - {}", sceneName).c_str(),
                                               nullptr, nullptr);
        if (!vulkanWindow.window) {
            char const* glfwErrorMessage = nullptr;
            glfwGetError(&glfwErrorMessage);
            throw vkutils::Error("Unable to create GLFW window\n"
                                 "Last error = %s", glfwErrorMessage);
        }

        if (const auto res = glfwCreateWindowSurface(vulkanWindow.instance, vulkanWindow.window,
                                                     nullptr, &vulkanWindow.surface);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to create VkSurfaceKHR\n"
                                 "glfwCreateWindowSurface() returned %s", vkutils::to_string(res).c_str());
        }

        // Select appropriate Vulkan device
        vulkanWindow.physicalDevice = select_device(vulkanWindow.instance, vulkanWindow.surface);
        if (VK_NULL_HANDLE == vulkanWindow.physicalDevice) {
            throw vkutils::Error("No suitable physical device found!");
        }

        // @formatter:off
        {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(vulkanWindow.physicalDevice, &props);
            std::printf("Selected device: %s (%d.%d.%d)\n",
                         props.deviceName,
                         VK_API_VERSION_MAJOR(props.apiVersion),
                         VK_API_VERSION_MINOR(props.apiVersion),
                         VK_API_VERSION_PATCH(props.apiVersion));
        }
        // @formatter:on

        // Create a logical device
        // Enable required extensions. The device selection method ensures that
        // the VK_KHR_swapchain extension is present, so we can safely just
        // request it without further checks.
        std::vector<char const*> enabledDeviceExensions;

        enabledDeviceExensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#       ifdef ENABLE_DIAGNOSTICS
        enabledDeviceExensions.emplace_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        enabledDeviceExensions.emplace_back(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME);
#       endif

        for (const auto& ext : enabledDeviceExensions) {
            std::printf("Enabling device extension: %s\n", ext);
        }

        // We need a graphics queue and a queue that can present
        std::vector<std::uint32_t> queueFamilyIndices;

        if (const auto index = find_queue_family(vulkanWindow.physicalDevice, VK_QUEUE_GRAPHICS_BIT,
                                                 vulkanWindow.surface)) {
            // best case: one GRAPHICS queue that can present
            vulkanWindow.graphicsFamilyIndex = *index;

            queueFamilyIndices.emplace_back(*index);
        } else {
            // otherwise: one GRAPHICS queue and any queue that can present
            const auto graphics = find_queue_family(vulkanWindow.physicalDevice, VK_QUEUE_GRAPHICS_BIT);
            const auto present = find_queue_family(vulkanWindow.physicalDevice, 0, vulkanWindow.surface);

            assert(graphics && present);

            vulkanWindow.graphicsFamilyIndex = *graphics;
            vulkanWindow.presentFamilyIndex = *present;

            queueFamilyIndices.emplace_back(*graphics);
            queueFamilyIndices.emplace_back(*present);
        }

        vulkanWindow.device = create_device(vulkanWindow.physicalDevice, queueFamilyIndices, enabledDeviceExensions);

        // Retrieve VkQueues
        vkGetDeviceQueue(vulkanWindow.device, vulkanWindow.graphicsFamilyIndex, 0, &vulkanWindow.graphicsQueue);

        assert(VK_NULL_HANDLE != vulkanWindow.graphicsQueue);

        if (queueFamilyIndices.size() >= 2)
            vkGetDeviceQueue(vulkanWindow.device, vulkanWindow.presentFamilyIndex, 0, &vulkanWindow.presentQueue);
        else {
            vulkanWindow.presentFamilyIndex = vulkanWindow.graphicsFamilyIndex;
            vulkanWindow.presentQueue = vulkanWindow.graphicsQueue;
        }

        // Create swap chain
        std::tie(vulkanWindow.swapchain, vulkanWindow.swapchainFormat, vulkanWindow.swapchainExtent) = create_swapchain(
            vulkanWindow.physicalDevice, vulkanWindow.surface, vulkanWindow.device, vulkanWindow.window,
            queueFamilyIndices);

        // Get swap chain images & create associated image views
        get_swapchain_images(vulkanWindow.device, vulkanWindow.swapchain, vulkanWindow.swapImages);
        create_swapchain_image_views(vulkanWindow.device, vulkanWindow.swapchainFormat, vulkanWindow.swapImages,
                                     vulkanWindow.swapViews);

        // Return window ready to use
        return vulkanWindow;
    }

    SwapChanges recreate_swapchain(VulkanWindow& window) {
        // Remember old format & extents
        // Both of these might change when recreating the swapchain
        const auto oldFormat = window.swapchainFormat;
        const auto oldExtent = window.swapchainExtent;

        // Destroy old objects (except for the old swap chain)
        // We keep the old swap chain object around, such that we can pass it to vkCreateSwapchainKHR() via the
        // oldSwapchain member of VkSwapchainCreateInfoKHR.
        VkSwapchainKHR oldSwapchain = window.swapchain;

        for (const auto view : window.swapViews) {
            vkDestroyImageView(window.device, view, nullptr);
        }

        window.swapViews.clear();
        window.swapImages.clear();

        // Create swap chain
        std::vector<std::uint32_t> queueFamilyIndices;
        if (window.presentFamilyIndex != window.graphicsFamilyIndex) {
            queueFamilyIndices.emplace_back(window.graphicsFamilyIndex);
            queueFamilyIndices.emplace_back(window.presentFamilyIndex);
        }

        try {
            std::tie(window.swapchain, window.swapchainFormat, window.swapchainExtent) = create_swapchain(
                window.physicalDevice, window.surface, window.device, window.window, queueFamilyIndices,
                oldSwapchain);
        } catch (...) {
            // Put pack the old swap chain handle into the VulkanWindow; this ensures that the old swap chain is
            // destroyed when this error branch occurs.
            window.swapchain = oldSwapchain;
            throw;
        }

        // Destroy old swap chain
        vkDestroySwapchainKHR(window.device, oldSwapchain, nullptr);

        // Get new swap chain images & create associated image views
        if (window.swapchain != VK_NULL_HANDLE) {
            get_swapchain_images(window.device, window.swapchain, window.swapImages);
            create_swapchain_image_views(window.device, window.swapchainFormat, window.swapImages, window.swapViews);
        }

        // Determine which swap chain properties have changed and return the information indicating this
        const auto [newWidth, newHeight] = window.swapchainExtent;
        return SwapChanges{
            .changedSize = newWidth > 0 && newHeight > 0 &&
                           (oldExtent.width != newWidth || oldExtent.height != newHeight),
            .changedFormat = oldFormat != window.swapchainFormat
        };
    }
}

namespace {
    std::vector<VkSurfaceFormatKHR> get_surface_formats(const VkPhysicalDevice physicalDevice,
                                                        const VkSurfaceKHR surface) {
        std::uint32_t formatsAmount = 0;
        if (const auto res = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatsAmount, nullptr);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to get surface formats count\n"
                                 "vkGetPhysicalDeviceSurfaceFormatsKHR() returned %s", vkutils::to_string(res).c_str()
            );
        }

        std::vector<VkSurfaceFormatKHR> surfaceFormats(formatsAmount);
        if (const auto res = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatsAmount,
                                                                  surfaceFormats.data());
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to get surface formats\n"
                                 "vkGetPhysicalDeviceSurfaceFormatsKHR() returned %s", vkutils::to_string(res).c_str()
            );
        }

        return surfaceFormats;
    }

    std::tuple<VkSwapchainKHR, VkFormat, VkExtent2D> create_swapchain(
        const VkPhysicalDevice physicalDevice,
        const VkSurfaceKHR surface,
        const VkDevice device,
        GLFWwindow* window,
        const std::vector<std::uint32_t>& queueFamilyIndices,
        const VkSwapchainKHR oldSwapchain) {
        const auto formats = get_surface_formats(physicalDevice, surface);

        // Pick the surface format
        // If there is an 8-bit RGB(A) SRGB format available, pick that. There are two main variations possible here
        // RGBA and BGRA. If neither is available, pick the first one that the driver gives us.
        //
        // See http://vulkan.gpuinfo.org/listsurfaceformats.php for a list of formats and statistics about where they’re
        // supported.
        assert(!formats.empty());
        VkSurfaceFormatKHR pickedFormat = formats[0];
        for (const auto format : formats) {
            if (VK_FORMAT_R8G8B8A8_SRGB == format.format && VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == format.colorSpace) {
                pickedFormat = format;
                break;
            }
            if (VK_FORMAT_B8G8R8A8_SRGB == format.format && VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == format.colorSpace) {
                pickedFormat = format;
                break;
            }
        }

        // Pick an image count
        VkSurfaceCapabilitiesKHR surfaceCapabilities;
        if (const auto res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to get surface capabilities\n"
                                 "vkGetPhysicalDeviceSurfaceCapabilitiesKHR() returned %s",
                                 vkutils::to_string(res).c_str()
            );
        }

        std::uint32_t imageCount = 2;

        if (imageCount < surfaceCapabilities.minImageCount + 1) {
            imageCount = surfaceCapabilities.minImageCount + 1;
        }

        if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount) {
            imageCount = surfaceCapabilities.maxImageCount;
        }

        // Figure out the swap extent
        VkExtent2D extent = surfaceCapabilities.currentExtent;
        if (std::numeric_limits<std::uint32_t>::max() == extent.width) {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            // Note: we must ensure that the extent is within the range defined by [ minImageExtent, maxImageExtent ].
            const auto& [minWidth, minHeight] = surfaceCapabilities.minImageExtent;
            const auto& [maxWidth, maxHeight] = surfaceCapabilities.maxImageExtent;
            extent.width = std::clamp(static_cast<std::uint32_t>(width), minWidth, maxWidth);
            extent.height = std::clamp(static_cast<std::uint32_t>(height), minHeight, maxHeight);
        }

        if (extent.width == 0 || extent.height == 0) {
            return {VK_NULL_HANDLE, pickedFormat.format, extent};
        }

        // Finally create the swap chain
        VkSwapchainCreateInfoKHR chainInfo{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface,
            .minImageCount = imageCount,
            .imageFormat = pickedFormat.format,
            .imageColorSpace = pickedFormat.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          (enableFramebufferReadback ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0),
            .preTransform = surfaceCapabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            // Force FIFO mode that waits for V-Sync
            .presentMode = VK_PRESENT_MODE_FIFO_KHR,
            .clipped = VK_TRUE,
            .oldSwapchain = oldSwapchain
        };

        if (queueFamilyIndices.size() <= 1) {
            chainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        } else {
            // Multiple queues may access this resource. There are two options. SHARING MODE CONCURRENT
            // allows access from multiple queues without transferring ownership. EXCLUSIVE would require explicit
            // ownership transfers, which we’re avoiding for now. EXCLUSIVE may result in better performance than
            // CONCURRENT.
            chainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            chainInfo.queueFamilyIndexCount = static_cast<std::uint32_t>(queueFamilyIndices.size());
            chainInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        }


        VkSwapchainKHR chain = VK_NULL_HANDLE;
        if (const auto res = vkCreateSwapchainKHR(device, &chainInfo, nullptr, &chain);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to create swap chain\n"
                                 "vkCreateSwapchainKHR() returned %s", vkutils::to_string(res).c_str()
            );
        }

        std::cout << "\nCreated Swapchain:\n"
                << "- Present mode: PRESENT_MODE_FIFO_KHR\n"
                << "- Colour format: " << vkutils::to_string(pickedFormat.format) << "\n"
                << "- Image count: " << imageCount << "\n\n";

        return {chain, pickedFormat.format, extent};
    }


    void get_swapchain_images(const VkDevice device,
                              const VkSwapchainKHR swapchain,
                              std::vector<VkImage>& images) {
        assert(0 == images.size());

        std::uint32_t imagesAmount = 0;
        if (const auto res = vkGetSwapchainImagesKHR(device, swapchain, &imagesAmount, nullptr);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to get swaipchain images count\n"
                                 "vkGetPhysicalDeviceSurfaceCapabilitiesKHR() returned %s",
                                 vkutils::to_string(res).c_str()
            );
        }

        images.resize(imagesAmount);
        if (const auto res = vkGetSwapchainImagesKHR(device, swapchain, &imagesAmount, images.data());
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to get swaipchain images\n"
                                 "vkGetPhysicalDeviceSurfaceCapabilitiesKHR() returned %s",
                                 vkutils::to_string(res).c_str()
            );
        }
    }

    void create_swapchain_image_views(const VkDevice device,
                                      const VkFormat swapchainFormat,
                                      const std::vector<VkImage>& images,
                                      std::vector<VkImageView>& imageViews) {
        assert(0 == imageViews.size());

        for (std::size_t i = 0; i < images.size(); ++i) {
            VkImageViewCreateInfo viewInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = images[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = swapchainFormat,
                .components = VkComponentMapping{
                    VK_COMPONENT_SWIZZLE_IDENTITY,
                    VK_COMPONENT_SWIZZLE_IDENTITY,
                    VK_COMPONENT_SWIZZLE_IDENTITY,
                    VK_COMPONENT_SWIZZLE_IDENTITY
                },
                .subresourceRange = VkImageSubresourceRange{
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    0, 1,
                    0, 1
                }
            };

            VkImageView view = VK_NULL_HANDLE;
            if (const auto res = vkCreateImageView(device, &viewInfo, nullptr, &view);
                VK_SUCCESS != res) {
                throw vkutils::Error("Unable to create image view for swap chain image %zu\n"
                                     "vkCreateImageView() returned %s", i, vkutils::to_string(res).c_str()
                );
            }
            imageViews.emplace_back(view);
        }

        assert(imageViews.size() == images.size());
    }
}

namespace {
    // Note: this finds *any* queue that supports the queueFlags. As such,
    //   find_queue_family( ..., VK_QUEUE_TRANSFER_BIT, ... );
    // might return a GRAPHICS queue family, since GRAPHICS queues typically
    // also set TRANSFER (and indeed most other operations; GRAPHICS queues are
    // required to support those operations regardless). If you wanted to find
    // a dedicated TRANSFER queue (e.g., such as those that exist on NVIDIA
    // GPUs), you would need to use different logic.
    std::optional<std::uint32_t> find_queue_family(const VkPhysicalDevice physicalDevice,
                                                   const VkQueueFlags queueFlags,
                                                   const VkSurfaceKHR surface) {
        std::uint32_t numQueues = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueues, nullptr);

        std::vector<VkQueueFamilyProperties> families(numQueues);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueues, families.data());

        for (std::uint32_t i = 0; i < numQueues; ++i) {
            const auto& family = families[i];

            if (queueFlags == (queueFlags & family.queueFlags)) {
                if (VK_NULL_HANDLE == surface) {
                    return i;
                }

                VkBool32 supported = VK_FALSE;
                if (const auto res = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supported);
                    VK_SUCCESS == res && supported) {
                    return i;
                }
            }
        }

        return {};
    }

    VkDevice create_device(const VkPhysicalDevice physicalDevice,
                           const std::vector<std::uint32_t>& queueFamilies,
                           const std::vector<char const*>& enabledDeviceExtensions) {
        if (queueFamilies.empty()) {
            throw vkutils::Error("create_device(): no queues requested");
        }

        constexpr float queuePriorities[1] = {1.0f};

        std::vector<VkDeviceQueueCreateInfo> queueInfos(queueFamilies.size());
        for (std::size_t i = 0; i < queueFamilies.size(); ++i) {
            auto& queueInfo = queueInfos[i];
            queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex = queueFamilies[i];
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = queuePriorities;
        }

        constexpr VkPhysicalDeviceFeatures deviceFeatures{
            .samplerAnisotropy = VK_TRUE
        };

        VkPhysicalDeviceHostQueryResetFeatures hostQueryResetFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES,
            .hostQueryReset = VK_TRUE
        };

        const VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
            .pNext = &hostQueryResetFeatures,
            .dynamicRendering = VK_TRUE
        };

        const VkDeviceCreateInfo deviceInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &dynamicRenderingFeature,
            .queueCreateInfoCount = static_cast<std::uint32_t>(queueInfos.size()),
            .pQueueCreateInfos = queueInfos.data(),
            .enabledExtensionCount = static_cast<std::uint32_t>(enabledDeviceExtensions.size()),
            .ppEnabledExtensionNames = enabledDeviceExtensions.data(),
            .pEnabledFeatures = &deviceFeatures
        };

        VkDevice device = VK_NULL_HANDLE;
        if (const auto res = vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device); VK_SUCCESS != res) {
            throw vkutils::Error("Unable to create logical device\n"
                                 "vkCreateDevice() returned %s", vkutils::to_string(res).c_str()
            );
        }

        return device;
    }
}

namespace {
    float score_device(const VkPhysicalDevice physicalDevice, const VkSurfaceKHR surface) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice, &props);

        // Only consider Vulkan 1.2+ devices
        const auto major = VK_API_VERSION_MAJOR(props.apiVersion);
        const auto minor = VK_API_VERSION_MINOR(props.apiVersion);

        if (major < 1 || (major == 1 && minor < 2)) {
            std::fprintf(stderr, "Info: Discarding device '%s': insufficient vulkan version\n", props.deviceName);
            return -1.0f;
        }

        // Check that the device supports the VK KHR swapchain extension
        const auto deviceExtensions = vkutils::detail::get_device_extensions(physicalDevice);
        if (!deviceExtensions.contains(VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
            std::fprintf(stderr, "Info: Discarding device ’%s’: extension %s missing\n",
                         props.deviceName, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            return -1.0f;
        }

        // Ensure there is a queue family that can present to the given surface
        if (!find_queue_family(physicalDevice, 0, surface)) {
            std::fprintf(stderr, "Info: Discarding device ’%s’: can’t present to surface\n", props.deviceName);
            return -1.0f;
        }

        // Also ensure there is a queue family that supports graphics commands
        if (!find_queue_family(physicalDevice, VK_QUEUE_GRAPHICS_BIT)) {
            std::fprintf(stderr, "Info: Discarding device ’%s’: no graphics queue family\n", props.deviceName);
            return -1.0f;
        }

        // Discrete GPU > Integrated GPU > others
        float score = 0.f;

        if (VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == props.deviceType) {
            score += 500.0f;
        } else if (VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU == props.deviceType) {
            score += 100.0f;
        }

        return score;
    }

    VkPhysicalDevice select_device(const VkInstance instance, const VkSurfaceKHR surface) {
        std::uint32_t numDevices = 0;
        if (const auto res = vkEnumeratePhysicalDevices(instance, &numDevices, nullptr);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to get physical device count\n"
                                 "vkEnumeratePhysicalDevices() returned %s", vkutils::to_string(res).c_str()
            );
        }

        std::vector<VkPhysicalDevice> devices(numDevices, VK_NULL_HANDLE);
        if (const auto res = vkEnumeratePhysicalDevices(instance, &numDevices, devices.data());
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to get physical device list\n"
                                 "vkEnumeratePhysicalDevices() returned %s", vkutils::to_string(res).c_str()
            );
        }

        float bestScore = -1.0f;
        VkPhysicalDevice bestDevice = VK_NULL_HANDLE;

        for (const auto device : devices) {
            const auto score = score_device(device, surface);
            if (score > bestScore) {
                bestScore = score;
                bestDevice = device;
            }
        }

        return bestDevice;
    }
}
