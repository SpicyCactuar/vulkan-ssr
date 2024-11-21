#include "state.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include "config.hpp"

namespace state {
    void update_camera_from_input(State& state, const float elapsedTime) {
        auto& camera = state.camera;

        if (state.inputMap[static_cast<std::size_t>(InputState::mousing)]) {
            // Only update the rotation on the second frame of mouse navigation. This ensures that the previousX
            // and Y variables are initialized to sensible values.
            if (state.wasMousing) {
                constexpr auto mouseSensitivity = cfg::cameraMouseSensitivity;
                const auto dx = mouseSensitivity * (state.mouseX - state.previousX);
                const auto dy = mouseSensitivity * (state.mouseY - state.previousY);

                camera = camera * glm::rotate(-dy, glm::vec3(1.f, 0.f, 0.f));
                camera = camera * glm::rotate(-dx, glm::vec3(0.f, 1.f, 0.f));
            }

            state.previousX = state.mouseX;
            state.previousY = state.mouseY;
            state.wasMousing = true;
        } else {
            state.wasMousing = false;
        }

        const auto move = elapsedTime * cfg::cameraBaseSpeed *
                          (state.inputMap[static_cast<std::size_t>(InputState::fast)] ? cfg::cameraFastMult : 1.f) *
                          (state.inputMap[static_cast<std::size_t>(InputState::slow)] ? cfg::cameraSlowMult : 1.f);

        if (state.inputMap[static_cast<std::size_t>(InputState::forward)]) {
            camera = camera * glm::translate(glm::vec3(0.f, 0.f, -move));
        }
        if (state.inputMap[static_cast<std::size_t>(InputState::backward)]) {
            camera = camera * glm::translate(glm::vec3(0.f, 0.f, +move));
        }

        if (state.inputMap[static_cast<std::size_t>(InputState::strafeLeft)]) {
            camera = camera * glm::translate(glm::vec3(-move, 0.f, 0.f));
        }
        if (state.inputMap[static_cast<std::size_t>(InputState::strafeRight)]) {
            camera = camera * glm::translate(glm::vec3(+move, 0.f, 0.f));
        }

        if (state.inputMap[static_cast<std::size_t>(InputState::levitate)]) {
            camera = camera * glm::translate(glm::vec3(0.f, +move, 0.f));
        }
        if (state.inputMap[static_cast<std::size_t>(InputState::sink)]) {
            camera = camera * glm::translate(glm::vec3(0.f, -move, 0.f));
        }
    }

    float easeInOut(const float t) {
        const float sqt = t * t;
        return sqt / (2.0f * (sqt - t) + 1.0f);
    }

    void update_camera_from_playback(State& state, const float elapsedTime) {
        // null check assumed to be performed
        const auto playback = *state.playback;
        auto& camera = state.camera;

        const auto frameIndex = state.currentBenchmarkFrame;

        const auto [from, to] = playback::find_step(playback, frameIndex);
        const float t = easeInOut(
            static_cast<float>(frameIndex - from.frameIndex) / static_cast<float>(to.frameIndex - from.frameIndex - 1));
        const auto orientation = glm::slerp(from.frameQuaternion, to.frameQuaternion, t);
        const auto position = glm::mix(from.framePosition, to.framePosition, t);
        camera = glm::translate(position) * glm::mat4_cast(orientation);
    }

    void update_state(State& state, const float elapsedTime) {
        if (state.performing_benchmarks() && state.playback != nullptr) {
            update_camera_from_playback(state, elapsedTime);
        } else {
            update_camera_from_input(state, elapsedTime);
        }

        state.view = glm::inverse(state.camera);
    }

    bool State::performing_benchmarks() const {
        return this->currentBenchmarkFrame < this->totalBenchmarkFrames;
    }

    bool State::start_benchmark() {
        if (performing_benchmarks()) {
            return false;
        }

        currentBenchmarkFrame = 0;
        return true;
    }
}
