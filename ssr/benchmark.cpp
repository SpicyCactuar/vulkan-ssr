#include "benchmark.hpp"

#include <fstream>

#ifdef ENABLE_DIAGNOSTICS

#include "../vkutils/error.hpp"
#include "../vkutils/to_string.hpp"

namespace benchmark {
    std::ofstream benchmarks_file(const std::filesystem::path& benchmarksPath) {
        std::ofstream benchmarksFile;
        benchmarksFile.open(benchmarksPath);

        std::printf("Writing benchmarks file: %s\n", benchmarksPath.string().c_str());

        // Attempt to write and then check if file is good
        benchmarksFile << "frame, shadow, offscreen, deferred, total\n";

        if (!benchmarksFile.good()) {
            throw vkutils::Error("Unable to create benchmarks file\n"
                                 "File path: %s", benchmarksPath.string());
        }

        return benchmarksFile;
    }

    std::vector<vkutils::QueryPool> create_timestamp_pools(const vkutils::VulkanWindow& window) {
        std::vector<vkutils::QueryPool> queryPools;
        const std::size_t frames = window.swapViews.size();
        queryPools.reserve(frames);

        // One QueryPool per frame-in-flight
        for (std::size_t i = 0; i < frames; ++i) {
            constexpr VkQueryPoolCreateInfo frameQueryPoolInfo{
                .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                .queryType = VK_QUERY_TYPE_TIMESTAMP,
                .queryCount = timestampsCount
            };

            VkQueryPool frameQueryPool;
            if (const auto res = vkCreateQueryPool(window.device, &frameQueryPoolInfo, nullptr, &frameQueryPool);
                VK_SUCCESS != res) {
                throw vkutils::Error("Unable to create query pool\n"
                                     "vkCreateQueryPool() returned %s", vkutils::to_string(res).c_str());
            }
            vkResetQueryPool(window.device, frameQueryPool, 0, timestampsCount);
            queryPools.emplace_back(window.device, frameQueryPool);
        }

        return queryPools;
    }

    std::array<std::uint64_t, timestampsCount> create_timestamp_buffer() {
        return {};
    }

    double timestamp_period(const vkutils::VulkanContext& context) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(context.physicalDevice, &properties);
        return properties.limits.timestampPeriod;
    }

    void record_pipeline_top_timestamp(const VkCommandBuffer commandBuffer,
                                       const vkutils::QueryPool& queryPool,
                                       const TimestampQuery query) {
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool.handle,
                            static_cast<std::uint32_t>(query));
    }

    void record_pipeline_bottom_timestamp(const VkCommandBuffer commandBuffer,
                                          const vkutils::QueryPool& queryPool,
                                          const TimestampQuery query) {
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool.handle,
                            static_cast<std::uint32_t>(query));
    }

    void query_timestamps(const vkutils::VulkanContext& context,
                          const vkutils::QueryPool& timestampPool,
                          std::array<std::uint64_t, timestampsCount>& timestampBuffer) {
        const auto res = vkGetQueryPoolResults(
            context.device, timestampPool.handle,
            0, timestampsCount, sizeof(uint64_t) * timestampsCount,
            static_cast<void*>(timestampBuffer.data()), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

        // If result is not VK_SUCCESS or VK_NOT_READY, query was unsuccessful
        // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkGetQueryPoolResults.html
        // See "Return Codes"
        if (VK_NOT_READY != res && VK_SUCCESS != res) {
            throw vkutils::Error("Unable to query frame timestamps\n"
                                 "vkGetQueryPoolResults() returned %s", vkutils::to_string(res).c_str());
        }

        vkResetQueryPool(context.device, timestampPool.handle, 0, timestampsCount);
    }

    double elapsedTimeInMs(const std::array<std::uint64_t, timestampsCount>& timestampBuffer,
                           const double timestampPeriod, const TimestampQuery start, const TimestampQuery end) {
        const std::uint64_t elapsedTime =
                timestampBuffer[static_cast<std::uint32_t>(end)] - timestampBuffer[static_cast<std::uint32_t>(start)];
        return elapsedTime * timestampPeriod / 1e6;
    }

    FrameTime extract_frame_time(const std::array<std::uint64_t, timestampsCount>& timestampBuffer,
                                 const double timestampPeriod) {
        return FrameTime{
            .shadowInMs = elapsedTimeInMs(timestampBuffer, timestampPeriod,
                                          TimestampQuery::frameStart, TimestampQuery::shadowEnd),
            .offscreenInMs = elapsedTimeInMs(timestampBuffer, timestampPeriod,
                                             TimestampQuery::offscreenStart, TimestampQuery::offscreenEnd),
            .deferredInMs = elapsedTimeInMs(timestampBuffer, timestampPeriod,
                                            TimestampQuery::deferredStart, TimestampQuery::frameEnd),
            .totalInMs = elapsedTimeInMs(timestampBuffer, timestampPeriod,
                                         TimestampQuery::frameStart, TimestampQuery::frameEnd)
        };
    }

    void process_frame(state::State& state, const FrameTime& frame, std::ofstream& benchmarksFile) {
        if (!state.performing_benchmarks()) {
            return;
        }

        const auto row = std::format("{}, {:.3f}, {:.3f}, {:.3f}, {:.3f}\n", state.currentBenchmarkFrame + 1,
                                     frame.shadowInMs, frame.offscreenInMs, frame.deferredInMs, frame.totalInMs);
        benchmarksFile << row;
        state.currentBenchmarkFrame++;

        // Finished benchmarking, close file
        if (!state.performing_benchmarks()) {
            std::printf("Closing benchmarks file\n");
            benchmarksFile.close();
        }
    }
}

#else

namespace benchmark {
    std::ofstream benchmarks_file([[maybe_unused]] const std::filesystem::path& benchmarksPath) {
        return std::ofstream();
    }

    std::vector<vkutils::QueryPool> create_timestamp_pools(const vkutils::VulkanWindow& window) {
        std::vector<vkutils::QueryPool> queryPools;
        const std::size_t frames = window.swapViews.size();
        queryPools.reserve(frames);

        // One QueryPool per frame-in-flight
        for (std::size_t i = 0; i < frames; ++i) {
            queryPools.emplace_back(window.device, VK_NULL_HANDLE);
        }

        return queryPools;
    }

    std::array<std::uint64_t, timestampsCount> create_timestamp_buffer() {
        return {};
    }

    double timestamp_period([[maybe_unused]] const vkutils::VulkanContext& context) {
        return 0.0;
    }

    void record_pipeline_top_timestamp([[maybe_unused]] const VkCommandBuffer commandBuffer,
                                       [[maybe_unused]] const vkutils::QueryPool& queryPool,
                                       [[maybe_unused]] const TimestampQuery query) {
        // no-op
    }

    void record_pipeline_bottom_timestamp([[maybe_unused]] const VkCommandBuffer commandBuffer,
                                          [[maybe_unused]] const vkutils::QueryPool& queryPool,
                                          [[maybe_unused]] const TimestampQuery query) {
        // no-op
    }

    void query_timestamps([[maybe_unused]] const vkutils::VulkanContext& context,
                          [[maybe_unused]] const vkutils::QueryPool& timestampPool,
                          [[maybe_unused]] std::array<std::uint64_t, timestampsCount>& timestampBuffer) {
        // no-op
    }

    FrameTime extract_frame_time([[maybe_unused]] const std::array<std::uint64_t, timestampsCount>& timestampBuffer,
                                 [[maybe_unused]] const double timestampPeriod) {
        return FrameTime{};
    }

    void process_frame(state::State& state, const FrameTime& frame, std::ofstream& benchmarksFile) {
        // no-op
    }
}

#endif
