#include "glfw.hpp"

#include <iostream>
#include <GLFW/glfw3.h>

#include "config.hpp"
#include "state.hpp"

namespace glfw {
    void input_state_callback(state::State* state, const int keyCode, const int action) {
        const bool isReleased = (GLFW_RELEASE == action);

        // Update input map
        switch (keyCode) {
            case GLFW_KEY_W:
                state->inputMap[static_cast<std::size_t>(state::InputState::forward)] = !isReleased;
                break;
            case GLFW_KEY_S:
                state->inputMap[static_cast<std::size_t>(state::InputState::backward)] = !isReleased;
                break;
            case GLFW_KEY_A:
                state->inputMap[static_cast<std::size_t>(state::InputState::strafeLeft)] = !isReleased;
                break;
            case GLFW_KEY_D:
                state->inputMap[static_cast<std::size_t>(state::InputState::strafeRight)] = !isReleased;
                break;
            case GLFW_KEY_E:
                state->inputMap[static_cast<std::size_t>(state::InputState::levitate)] = !isReleased;
                break;
            case GLFW_KEY_Q:
                state->inputMap[static_cast<std::size_t>(state::InputState::sink)] = !isReleased;
                break;
            case GLFW_KEY_LEFT_SHIFT:
                [[fallthrough]];
            case GLFW_KEY_RIGHT_SHIFT:
                state->inputMap[static_cast<std::size_t>(state::InputState::fast)] = !isReleased;
                break;
            case GLFW_KEY_LEFT_CONTROL:
                [[fallthrough]];
            case GLFW_KEY_RIGHT_CONTROL:
                state->inputMap[static_cast<std::size_t>(state::InputState::slow)] = !isReleased;
                break;
        }
    }

    void diagnostic_tools_callback(state::State* state, const int keyCode, const int action) {
        // Only update state on press to avoid redundancy
        if (GLFW_PRESS != action) {
            return;
        }

        // Per-frame interaction callbacks
        switch (keyCode) {
            case GLFW_KEY_L:
                // Move the camera to the cfg::lightPosition
                state->camera = glm::translate(state->lightPosition) * cfg::cameraInitialRotation;
                break;
            case GLFW_KEY_I:
                // Reset camera to initial configuration
                state->camera = glm::translate(cfg::cameraInitialPosition) * cfg::cameraInitialRotation;
                break;
            case GLFW_KEY_P:
                state->takeFrameScreenshot = true;
                break;
            default:
                break;
        }
    }

    void glfw_callback_key(GLFWwindow* window,
                           const int keyCode,
                           int /*scanCode*/,
                           const int action,
                           int /*modifierFlags*/) {
        const auto state = static_cast<state::State*>(glfwGetWindowUserPointer(window));
        assert(state);

        if (GLFW_KEY_ESCAPE == keyCode) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            return;
        }

        input_state_callback(state, keyCode, action);
        diagnostic_tools_callback(state, keyCode, action);
    }

    void glfw_callback_button(GLFWwindow* window, const int button, const int action, int) {
        const auto state = static_cast<state::State*>(glfwGetWindowUserPointer(window));
        assert(state);

        if (GLFW_MOUSE_BUTTON_RIGHT == button && GLFW_PRESS == action) {
            auto& flag = state->inputMap[static_cast<std::size_t>(state::InputState::mousing)];

            flag = !flag;
            if (flag) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
    }

    void glfw_callback_motion(GLFWwindow* window, const double x, const double y) {
        const auto state = static_cast<state::State*>(glfwGetWindowUserPointer(window));
        assert(state);
        state->mouseX = static_cast<float>(x);
        state->mouseY = static_cast<float>(y);
    }

    void setup_window(const vkutils::VulkanWindow& window, state::State& state) {
        glfwSetWindowUserPointer(window.window, &state);
        glfwSetKeyCallback(window.window, &glfw::glfw_callback_key);
        glfwSetMouseButtonCallback(window.window, &glfw::glfw_callback_button);
        glfwSetCursorPosCallback(window.window, &glfw::glfw_callback_motion);
    }
}
