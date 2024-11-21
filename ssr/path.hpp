#pragma once

#include <filesystem>

namespace path {
    inline std::filesystem::path output_file_path(const std::string& name,
                                                  const std::string& tag,
                                                  const std::string& extension) {
        // Get the current time point
        const auto now = std::chrono::system_clock::now();

        // Convert the time point to a time_t (similar to UNIX timestamp)
        const std::time_t now_c = std::chrono::system_clock::to_time_t(now);

        // Convert to local time struct tm
        const std::tm* local_time = std::localtime(&now_c);

        // Format the timestamp into a string
        char timestamp[32];
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d-%H_%M_%S", local_time);

        const std::string filePath = tag.empty()
                                         ? std::format("{}-{}.{}", name, timestamp, extension)
                                         : std::format("{}-{}-{}.{}", name, tag, timestamp, extension);


        // Ensure output folder exists
        const auto outFolder = std::filesystem::current_path() / OUT_PATH_;

        if (!std::filesystem::exists(outFolder)) {
            std::filesystem::create_directories(outFolder);
        }

        return outFolder / std::filesystem::path(filePath);
    }
}
