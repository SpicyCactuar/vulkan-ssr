#pragma once

#include <filesystem>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace playback {
    struct Keyframe {
        std::size_t frameIndex;
        glm::vec3 framePosition;
        glm::quat frameQuaternion;
    };

    /*
     * Assumptions:
     *  - The first entry contains "frameIndex = 0"
     *  - keyframe.frameIndex is ascending
     *  - There are at least 2 entries
     *
     *  These are validated in playback::parse_playback
     */
    struct Playback {
        std::string stem;
        std::vector<Keyframe> keyframes;

        std::size_t duration_in_frames() const;
    };

    Playback parse_playback(const std::filesystem::path& playbackPath);

    std::pair<Keyframe, Keyframe> find_step(const Playback& playback, std::size_t frameIndex);
}
