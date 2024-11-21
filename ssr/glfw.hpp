#pragma once

#if !defined(GLFW_INCLUDE_NONE)
#	define GLFW_INCLUDE_NONE
#endif

#include "../vkutils/vulkan_window.hpp"
#include "state.hpp"

namespace glfw {
    void setup_window(const vkutils::VulkanWindow& window, state::State& state);
}
