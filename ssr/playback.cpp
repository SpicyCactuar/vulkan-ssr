#include "playback.hpp"

#include <fstream>
#include <iostream>

#include <glm/gtx/string_cast.hpp>

#include "../vkutils/error.hpp"

namespace playback {
    std::size_t Playback::duration_in_frames() const {
        return keyframes.back().frameIndex;
    }

    Playback parse_playback(const std::filesystem::path& playbackPath) {
        if (playbackPath.extension() != ".csv") {
            throw vkutils::Error("Provided playback file is not .csv: %s", playbackPath.string().c_str());
        }

        std::printf("Parsing playback file: %s\n", playbackPath.string().c_str());

        std::ifstream playbackFile;
        playbackFile.open(playbackPath);

        if (!playbackFile.good() || !playbackFile.is_open()) {
            throw vkutils::Error("Could not open playback file: %s", playbackPath.string().c_str());
        }

        std::vector<Keyframe> keyframes;
        char delimiter = ',';

        std::string line;
        // Skip header line
        if (!std::getline(playbackFile, line)) {
            throw vkutils::Error("File must not be empty: %s", playbackPath.string().c_str());
        }

        while (std::getline(playbackFile, line)) {
            std::istringstream lineStream(line);

            std::size_t frameIndex;
            float px, py, pz, qa, qx, qy, qz;
            if (!(lineStream
                  >> frameIndex >> delimiter
                  >> px >> delimiter
                  >> py >> delimiter
                  >> pz >> delimiter
                  >> qa >> delimiter
                  >> qx >> delimiter
                  >> qy >> delimiter
                  >> qz)) {
                throw vkutils::Error("Failed to parse line: %u", keyframes.size() + 1);
            }

            keyframes.emplace_back(Keyframe{
                    frameIndex, {px, py, pz}, glm::angleAxis(glm::radians(qa), normalize(glm::vec3{qx, qy, qz}))
                }
            );

            if (keyframes.size() == 1) {
                // First keyframe should have frame = 0
                assert(keyframes.back().frameIndex == 0);
            } else {
                // Subsequent keyframes should be ascending
                assert(keyframes[keyframes.size() - 1].frameIndex > keyframes[keyframes.size() - 2].frameIndex);
            }
        }

        if (keyframes.empty()) {
            std::printf("CSV contained no playback information: %s"
                        "Creating stub at (0, 0, 0) facing forward\n", playbackPath.string().c_str());
            keyframes.emplace_back(
                0,
                glm::vec3{0.0f, 0.0f, 0.0f},
                glm::angleAxis(glm::radians(0.0f), normalize(glm::vec3{0.0f, 1.0f, 0.0f}))
            );

            keyframes.emplace_back(
                1000,
                glm::vec3{0.0f, 0.0f, 0.0f},
                glm::angleAxis(glm::radians(0.0f), normalize(glm::vec3{0.0f, 1.0f, 0.0f}))
            );
        } else if (keyframes.size() == 1) {
            std::printf("CSV contained 1 playback entry, adding 1000 frames long extra copy for interpolation\n");
            keyframes.emplace_back(
                keyframes.back().frameIndex + 500,
                keyframes.front().framePosition,
                keyframes.front().frameQuaternion
            );
        }

        return Playback{
            .stem = playbackPath.stem().string(),
            .keyframes = std::move(keyframes)
        };
    }

    std::pair<Keyframe, Keyframe> find_step(const Playback& playback, const std::size_t frameIndex) {
        for (std::size_t i = 0; i < playback.keyframes.size(); ++i) {
            if (frameIndex < playback.keyframes[i].frameIndex) {
                return {playback.keyframes[i - 1], playback.keyframes[i]};
            }
        }

        throw vkutils::Error("Attempting to step between frames not present in Playback");
    }
}
