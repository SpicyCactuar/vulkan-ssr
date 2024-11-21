#pragma once

#include <array>
#include <filesystem>
#include <vector>

#include "../vkutils/vulkan_window.hpp"
#include "../vkutils/vkobject.hpp"

#include "state.hpp"

namespace benchmark {
    enum class TimestampQuery : std::uint32_t {
        frameStart = 0,
        shadowEnd = 1,
        offscreenStart = 2,
        offscreenEnd = 3,
        deferredStart = 4,
        frameEnd = 5
    };

    struct FrameTime {
        double shadowInMs;
        double offscreenInMs;
        double deferredInMs;
        double totalInMs;
    };

    static constexpr std::uint32_t timestampsCount = static_cast<std::uint32_t>(TimestampQuery::frameEnd) -
                                                     static_cast<std::uint32_t>(TimestampQuery::frameStart) + 1;

    std::ofstream benchmarks_file(const std::filesystem::path& benchmarksPath);

    std::vector<vkutils::QueryPool> create_timestamp_pools(const vkutils::VulkanWindow& window);

    std::array<std::uint64_t, timestampsCount> create_timestamp_buffer();

    void record_pipeline_top_timestamp(VkCommandBuffer commandBuffer,
                                       const vkutils::QueryPool& queryPool,
                                       TimestampQuery query);

    void record_pipeline_bottom_timestamp(VkCommandBuffer commandBuffer,
                                          const vkutils::QueryPool& queryPool,
                                          TimestampQuery query);

    void query_timestamps(const vkutils::VulkanContext& context,
                          const vkutils::QueryPool& timestampPool,
                          std::array<std::uint64_t, timestampsCount>& timestampBuffer);

    double timestamp_period(const vkutils::VulkanContext& context);

    FrameTime extract_frame_time(const std::array<std::uint64_t, timestampsCount>& timestampBuffer,
                                 double timestampPeriod);

    void process_frame(state::State& state, const FrameTime& frame, std::ofstream& benchmarksFile);
}
