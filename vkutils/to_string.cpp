#include "to_string.hpp"

#include <iomanip>
#include <sstream>
#include <type_traits>

namespace vkutils {
    std::string to_string(const VkResult result) {
        // See:
        // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkResult.html
        switch (result) {
#			define CASE_(x) case VK_##x: return #x
            CASE_(SUCCESS);
            CASE_(NOT_READY);
            CASE_(TIMEOUT);
            CASE_(EVENT_SET);
            CASE_(EVENT_RESET);
            CASE_(INCOMPLETE);
            CASE_(ERROR_OUT_OF_HOST_MEMORY);
            CASE_(ERROR_OUT_OF_DEVICE_MEMORY);
            CASE_(ERROR_INITIALIZATION_FAILED);
            CASE_(ERROR_DEVICE_LOST);
            CASE_(ERROR_MEMORY_MAP_FAILED);
            CASE_(ERROR_LAYER_NOT_PRESENT);
            CASE_(ERROR_EXTENSION_NOT_PRESENT);
            CASE_(ERROR_FEATURE_NOT_PRESENT);
            CASE_(ERROR_INCOMPATIBLE_DRIVER);
            CASE_(ERROR_TOO_MANY_OBJECTS);
            CASE_(ERROR_FORMAT_NOT_SUPPORTED);
            CASE_(ERROR_FRAGMENTED_POOL);
            CASE_(ERROR_UNKNOWN);
            CASE_(ERROR_OUT_OF_POOL_MEMORY);
            CASE_(ERROR_INVALID_EXTERNAL_HANDLE);
            CASE_(ERROR_FRAGMENTATION);
            CASE_(ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
            CASE_(ERROR_SURFACE_LOST_KHR);
            CASE_(ERROR_NATIVE_WINDOW_IN_USE_KHR);
            CASE_(SUBOPTIMAL_KHR);
            CASE_(ERROR_OUT_OF_DATE_KHR);
            CASE_(ERROR_INCOMPATIBLE_DISPLAY_KHR);
            CASE_(ERROR_VALIDATION_FAILED_EXT);
            CASE_(ERROR_INVALID_SHADER_NV);
            CASE_(ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
            CASE_(ERROR_NOT_PERMITTED_EXT);
            CASE_(ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
            CASE_(THREAD_IDLE_KHR);
            CASE_(THREAD_DONE_KHR);
            CASE_(OPERATION_DEFERRED_KHR);
            CASE_(OPERATION_NOT_DEFERRED_KHR);
            CASE_(PIPELINE_COMPILE_REQUIRED_EXT);
            CASE_(ERROR_COMPRESSION_EXHAUSTED_EXT);

            CASE_(ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR);
            CASE_(ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR);
            CASE_(ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR);
            CASE_(ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR);
            CASE_(ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR);
            CASE_(ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR);
            CASE_(ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);
#			undef CASE_

            // Vulkan includes this extra value in the enumeration. We should never
            // see it in practice - it's handled by the fallback option at the end.
            // If this case wasn't included here, most compilers will emit a
            // warning on unhandled cases in the switch().
            case VK_RESULT_MAX_ENUM:
                break;
            // Most compilers will warn if any enumeration values were missed in
            // the switch(). This is nice, as it will tell us if new VkResult
            // values are added to Vulkan. However, if this isn't desirable,
            // uncomment the following line:
            default:
                break;
        }

        // Handle other values gracefully.
        std::ostringstream oss;
        oss << "VkResult(" << std::underlying_type_t<VkResult>(result) << ")";
        return oss.str();
    }

    std::string to_string(const VkPhysicalDeviceType physicalDeviceType) {
        // See
        // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkPhysicalDeviceType.html
        switch (physicalDeviceType) {
#			define CASE_(x) case VK_##x: return #x
            CASE_(PHYSICAL_DEVICE_TYPE_OTHER);
            CASE_(PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
            CASE_(PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
            CASE_(PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU);
            CASE_(PHYSICAL_DEVICE_TYPE_CPU);
#			undef CASE_

            case VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM: break;
        }

        // Handle other values gracefully.
        std::ostringstream oss;
        oss << "VkPhysicalDeviceType(" << std::underlying_type_t<VkPhysicalDeviceType>(physicalDeviceType) << ")";
        return oss.str();
    }

    std::string to_string(const VkDebugUtilsMessageSeverityFlagBitsEXT severity) {
        // See
        // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkDebugUtilsMessageSeverityFlagBitsEXT.html
        switch (severity) {
            // This appears fairly frequently in the output, so make the part
            // that's printed a bit shorter.
#			define CASE_(x) case VK_DEBUG_UTILS_MESSAGE_##x##_BIT_EXT: return #x
            CASE_(SEVERITY_VERBOSE);
            CASE_(SEVERITY_INFO);
            CASE_(SEVERITY_WARNING);
            CASE_(SEVERITY_ERROR);
#			undef CASE_

            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT: break;
        }

        // Handle other values gracefully.
        std::ostringstream oss;
        oss << "VkDebugUtilsMessageSeverityFlagBitsEXT("
                << std::underlying_type_t<VkDebugUtilsMessageSeverityFlagBitsEXT>(severity)
                << ")";
        return oss.str();
    }

    std::string to_string(const VkFormat format) {
        // See
        // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkFormat.html
        switch (format) {
#			define CASE_(x) case VK_##x: return #x
            CASE_(FORMAT_UNDEFINED);
            CASE_(FORMAT_R8G8B8A8_SRGB);
            CASE_(FORMAT_B8G8R8A8_SRGB);
#			undef CASE_

            case VK_FORMAT_MAX_ENUM: break;
            default: break;
        }

        // Handle other values gracefully.
        std::ostringstream oss;
        oss << "VkPhysicalDeviceType(" << std::underlying_type_t<VkFormat>(format) << ")";
        return oss.str();
    }

    std::string queue_flags(VkQueueFlags flags) {
        std::ostringstream oss;

        bool separator = false;

#		define APPEND_(x) if( VK_##x & flags ) { \
			if (separator) oss << " | "; \
			oss << #x; \
			flags &= ~VkQueueFlags(VK_##x); \
			separator = true; \
		}

        APPEND_(QUEUE_GRAPHICS_BIT);
        APPEND_(QUEUE_COMPUTE_BIT);
        APPEND_(QUEUE_TRANSFER_BIT);
        APPEND_(QUEUE_SPARSE_BINDING_BIT);
        APPEND_(QUEUE_PROTECTED_BIT);
#		if defined(VK_ENABLE_BETA_EXTENSIONS)
		APPEND_(QUEUE_VIDEO_DECODE_BIT_KHR);
		APPEND_(QUEUE_VIDEO_ENCODE_BIT_KHR);
#		endif

#		undef APPEND_

        if (flags) {
            if (separator) {
                oss << " | ";
            }
            oss << "VkQueueFlags(" << std::hex << flags << ")";
        }

        return oss.str();
    }

    std::string message_type_flags(VkDebugUtilsMessageTypeFlagsEXT flags) {
        std::ostringstream oss;

        bool separator = false;

        // This appears fairly frequently in the output, so make the part
        // that's printed a bit shorter.
#		define APPEND_(x) if( VK_DEBUG_UTILS_MESSAGE_TYPE_##x##_BIT_EXT & flags ) { \
			if( separator ) oss << ", "; \
			oss << #x; \
			flags &= ~VkDebugUtilsMessageTypeFlagsEXT(VK_DEBUG_UTILS_MESSAGE_TYPE_##x##_BIT_EXT); \
			separator = true; \
		} /*ENDM*/

        APPEND_(GENERAL);
        APPEND_(VALIDATION);
        APPEND_(PERFORMANCE);

#		undef APPEND_

        if (flags) {
            if (separator) {
                oss << " | ";
            }
            oss << "VkDebugUtilsMessageTypeFlagsEXT(" << std::hex << flags << ")";
        }

        return oss.str();
    }

    std::string memory_heap_flags(VkMemoryHeapFlags flags) {
        std::ostringstream oss;

        bool separator = false;

#		define APPEND_(x) if( VK_MEMORY_HEAP_##x##_BIT & flags ) { \
			if( separator ) oss << " | "; \
			oss << #x; \
			flags &= ~VkMemoryHeapFlags(VK_MEMORY_HEAP_##x##_BIT); \
			separator = true; \
		} /*ENDM*/

        APPEND_(DEVICE_LOCAL);
        APPEND_(MULTI_INSTANCE);

#		undef APPEND_

        if (flags) {
            if (separator) {
                oss << " | ";
            }
            oss << "VkMemoryHeapFlags(" << std::hex << flags << ")";
        }

        return oss.str();
    }

    std::string memory_property_flags(VkMemoryPropertyFlags flags) {
        std::ostringstream oss;

        bool separator = false;

#		define APPEND_(x) if( VK_MEMORY_PROPERTY_##x##_BIT & flags ) { \
			if( separator ) oss << " | "; \
			oss << #x; \
			flags &= ~VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_##x##_BIT); \
			separator = true; \
		} /*ENDM*/

        APPEND_(DEVICE_LOCAL);
        APPEND_(HOST_VISIBLE);
        APPEND_(HOST_COHERENT);
        APPEND_(HOST_CACHED);
        APPEND_(LAZILY_ALLOCATED);
        APPEND_(PROTECTED);

        //Note: skips a few of the extenions

#		undef APPEND_

        if (flags) {
            if (separator) {
                oss << " | ";
            }
            oss << "VkMemoryPropertyFlags(" << std::hex << flags << ")";
        }

        return oss.str();
    }

    std::string driver_version(const std::uint32_t vendorId,
                               const std::uint32_t driverVersion) {
        // See:
        // https://github.com/SaschaWillems/vulkan.gpuinfo.org/blob/1e6ca6e3c0763daabd6a101b860ab4354a07f5d3/functions.php#L294
        std::ostringstream oss;

        if (4318 /* NVIDIA*/ == vendorId) {
            oss
                    << (driverVersion >> 22 & 0x3ff)
                    << "."
                    << (driverVersion >> 14 & 0xff)
                    << "."
                    << (driverVersion >> 6 & 0xff)
                    << "."
                    << (driverVersion & 0x3f);
        }
#		if defined(_WIN32) // Windows only
        else if (0x8086 /* Intel, obviously */) {
            oss
                    << (driverVersion >> 14)
                    << "."
                    << (driverVersion & 0x3fff);
        }
#		endif // ~ Windows only
        else {
            // (Old) Vulkan convention, prior to the introduction of the
            // VK_API_VERSION_VARIANT bits at the top.
            oss
                    << (driverVersion >> 22)
                    << "."
                    << (driverVersion >> 12 & 0x3ff)
                    << "."
                    << (driverVersion & 0xfff);
        }

        return oss.str();
    }
}
