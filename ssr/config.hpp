#pragma once

#if !defined(GLM_FORCE_RADIANS)
#	define GLM_FORCE_RADIANS
#endif

#include <chrono>
#include <vulkan/vulkan_core.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "../vkutils/angle.hpp"

using namespace vkutils::literals;

namespace cfg {
    constexpr const char* shadowMapOpaqueVertPath = ASSETS_PATH_ "/shaders/shadow_map_opaque.vert.spv";
    constexpr const char* shadowMapOpaqueFragPath = ASSETS_PATH_ "/shaders/shadow_map_opaque.frag.spv";
    constexpr const char* shadowMapAlphaVertPath = ASSETS_PATH_ "/shaders/shadow_map_alpha.vert.spv";
    constexpr const char* shadowMapAlphaFragPath = ASSETS_PATH_ "/shaders/shadow_map_alpha.frag.spv";
    constexpr const char* offscreenVertPath = ASSETS_PATH_ "/shaders/offscreen.vert.spv";
    constexpr const char* offscreenOpaqueFragPath = ASSETS_PATH_ "/shaders/offscreen_opaque.frag.spv";
    constexpr const char* offscreenAlphaFragPath = ASSETS_PATH_ "/shaders/offscreen_alpha.frag.spv";
    constexpr const char* fullscreenVertPath = ASSETS_PATH_ "/shaders/fullscreen.vert.spv";
    constexpr const char* fullscreenFragPath = ASSETS_PATH_ "/shaders/fullscreen.frag.spv";

    // Low resolution setting
    constexpr VkExtent2D resolutionLow{1280, 800};

    // Mid resolution setting
    constexpr VkExtent2D resolutionMid{1680, 1050};

    // High resolution setting
    constexpr VkExtent2D resolutionHigh{1920, 1200};

    static const auto [windowWidth, windowHeight] = resolutionHigh;

    constexpr glm::vec3 cameraInitialPosition{0.0f, 8.0f, 8.0f};
    constexpr glm::mat4 cameraInitialRotation = glm::identity<glm::mat4>();

    constexpr float cameraNear = 0.1f;
    constexpr float cameraFar = 100.0f;

    constexpr auto cameraFov = 60.0_degf;

    using Clock = std::chrono::steady_clock;
    using Secondsf = std::chrono::duration<float, std::ratio<1>>;

    constexpr float cameraBaseSpeed = 1.7f; // units/second
    constexpr float cameraFastMult = 5.0f; // speed multiplier
    constexpr float cameraSlowMult = 0.05f; // speed multiplier

    constexpr float cameraMouseSensitivity = 0.001f; // radians per pixel

    constexpr VkClearColorValue clearColour{0.52f, 0.80f, 0.92f, 1.0f};
    constexpr VkClearColorValue noColour{0.0f, 0.0f, 0.0f, 0.0f};

    constexpr glm::vec3 ambient{0.02f, 0.02f, 0.02f};

    constexpr glm::vec3 lightColour{1.0f, 1.0f, 1.0f};
    constexpr glm::vec3 initialLightPosition{0.0f, 5.0f, -5.0f};
    // Look forward, and slightly downward to avoid shadow acne
    constexpr glm::vec3 initialLightLookDirection{0.0f, -0.01f, -1.0f};
    constexpr float lightNear = 1.0f;
    constexpr float lightFar = 100.0f;
    constexpr auto lightFov = 90.0_degf;
}
