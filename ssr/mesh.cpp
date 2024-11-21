#include "mesh.hpp"

#include <cstring> // for std::memcpy()
#include <limits>

#include "config.hpp"
#include "../vkutils/error.hpp"
#include "../vkutils/to_string.hpp"
#include "../vkutils/vkutil.hpp"

namespace {
    // Map positions from Host -> Staging -> Device memory
    void map_vertices_to_gpu_memory(const vkutils::VulkanContext& context,
                                    const vkutils::Allocator& allocator,
                                    const vkutils::CommandPool& uploadPool,
                                    const baked::BakedMeshData& mesh,
                                    const vkutils::Buffer& positionsStaging, const vkutils::Buffer& positionsGPU,
                                    const vkutils::Buffer& uvsStaging, const vkutils::Buffer& uvsGPU,
                                    const vkutils::Buffer& normalsStaging, const vkutils::Buffer& normalsGPU,
                                    const vkutils::Buffer& tangentsStaging, const vkutils::Buffer& tangentsGPU,
                                    const vkutils::Buffer& indicesStaging, const vkutils::Buffer& indicesGPU) {
        // Copy positions Host -> Staging
        const auto positionsSizeInBytes = sizeof(glm::vec3) * mesh.positions.size();
        void* positionsPointer = nullptr;
        if (const auto res = vmaMapMemory(allocator.allocator, positionsStaging.allocation, &positionsPointer);
            VK_SUCCESS != res) {
            throw vkutils::Error("Mapping memory for writing positions\n"
                                 "vmaMapMemory() returned %s", vkutils::to_string(res).c_str()
            );
        }
        std::memcpy(positionsPointer, mesh.positions.data(), positionsSizeInBytes);
        vmaUnmapMemory(allocator.allocator, positionsStaging.allocation);

        // Copy normals Host -> Staging
        const auto normalsSizeInBytes = sizeof(glm::vec3) * mesh.normals.size();
        void* normalsPointer = nullptr;
        if (const auto res = vmaMapMemory(allocator.allocator, normalsStaging.allocation, &normalsPointer);
            VK_SUCCESS != res) {
            throw vkutils::Error("Mapping memory for writing normals\n"
                                 "vmaMapMemory() returned %s", vkutils::to_string(res).c_str()
            );
        }
        std::memcpy(normalsPointer, mesh.normals.data(), normalsSizeInBytes);
        vmaUnmapMemory(allocator.allocator, normalsStaging.allocation);

        // Copy normals Host -> Staging
        const auto tangentsSizeInBytes = sizeof(glm::vec4) * mesh.tangents.size();
        void* tangentsPointer = nullptr;
        if (const auto res = vmaMapMemory(allocator.allocator, tangentsStaging.allocation, &tangentsPointer);
            VK_SUCCESS != res) {
            throw vkutils::Error("Mapping memory for writing tangents\n"
                                 "vmaMapMemory() returned %s", vkutils::to_string(res).c_str()
            );
        }
        std::memcpy(tangentsPointer, mesh.tangents.data(), tangentsSizeInBytes);
        vmaUnmapMemory(allocator.allocator, tangentsStaging.allocation);

        // Copy uvs Host -> Staging
        const auto uvsSizeInBytes = sizeof(glm::vec2) * mesh.uvs.size();
        void* uvsPointer = nullptr;
        if (const auto res = vmaMapMemory(allocator.allocator, uvsStaging.allocation, &uvsPointer);
            VK_SUCCESS != res) {
            throw vkutils::Error("Mapping memory for writing uvs\n"
                                 "vmaMapMemory() returned %s", vkutils::to_string(res).c_str()
            );
        }
        std::memcpy(uvsPointer, mesh.uvs.data(), uvsSizeInBytes);
        vmaUnmapMemory(allocator.allocator, uvsStaging.allocation);

        // Copy indices Host -> Staging
        const auto indicesSizeInBytes = sizeof(std::uint32_t) * mesh.indices.size();
        void* indicesPointer = nullptr;
        if (const auto res = vmaMapMemory(allocator.allocator, indicesStaging.allocation, &indicesPointer);
            VK_SUCCESS != res) {
            throw vkutils::Error("Mapping memory for writing indices\n"
                                 "vmaMapMemory() returned %s", vkutils::to_string(res).c_str()
            );
        }
        std::memcpy(indicesPointer, mesh.indices.data(), indicesSizeInBytes);
        vmaUnmapMemory(allocator.allocator, indicesStaging.allocation);

        // We need to ensure that the Vulkan resources are alive until all the transfers have completed. For simplicity,
        // we will just wait for the operations to complete with a fence. A more complex solution might want to queue
        // transfers, let these take place in the background while performing other tasks.
        vkutils::Fence uploadComplete = vkutils::create_fence(context);

        // Queue data uploads from staging buffers to the final buffers.
        VkCommandBuffer uploadCommand = vkutils::alloc_command_buffer(context, uploadPool.handle);

        constexpr VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = 0,
            .pInheritanceInfo = nullptr
        };
        if (const auto res = vkBeginCommandBuffer(uploadCommand, &beginInfo);
            VK_SUCCESS != res) {
            throw vkutils::Error("Beginning command buffer recording\n"
                                 "vkBeginCommandBuffer() returned %s", vkutils::to_string(res).c_str()
            );
        }

        // Copy positions Staging -> GPU
        const VkBufferCopy positionsCopy{
            .size = positionsSizeInBytes
        };

        vkCmdCopyBuffer(uploadCommand, positionsStaging.buffer, positionsGPU.buffer, 1, &positionsCopy);

        vkutils::buffer_barrier(uploadCommand,
                                positionsGPU.buffer,
                                VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
        );

        // Copy normals Staging -> GPU
        const VkBufferCopy normalsCopy{
            .size = normalsSizeInBytes
        };

        vkCmdCopyBuffer(uploadCommand, normalsStaging.buffer, normalsGPU.buffer, 1, &normalsCopy);

        vkutils::buffer_barrier(uploadCommand,
                                normalsGPU.buffer,
                                VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
        );

        // Copy uvs Staging -> GPU
        const VkBufferCopy uvsCopy{
            .size = uvsSizeInBytes
        };

        vkCmdCopyBuffer(uploadCommand, uvsStaging.buffer, uvsGPU.buffer, 1, &uvsCopy);

        vkutils::buffer_barrier(uploadCommand,
                                uvsGPU.buffer,
                                VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
        );

        // Copy tangents Staging -> GPU
        const VkBufferCopy tangentsCopy{
            .size = tangentsSizeInBytes
        };

        vkCmdCopyBuffer(uploadCommand, tangentsStaging.buffer, tangentsGPU.buffer, 1, &tangentsCopy);

        vkutils::buffer_barrier(uploadCommand,
                                tangentsGPU.buffer,
                                VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
        );

        // Copy indices Staging -> GPU
        VkBufferCopy indicesCopy{};
        indicesCopy.size = indicesSizeInBytes;

        vkCmdCopyBuffer(uploadCommand, indicesStaging.buffer, indicesGPU.buffer, 1, &indicesCopy);

        vkutils::buffer_barrier(uploadCommand,
                                indicesGPU.buffer,
                                VK_ACCESS_TRANSFER_WRITE_BIT,
                                VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
        );

        if (const auto res = vkEndCommandBuffer(uploadCommand); VK_SUCCESS != res) {
            throw vkutils::Error("Ending command buffer recording\n"
                                 "vkEndCommandBuffer() returned %s", vkutils::to_string(res).c_str()
            );
        }

        // Submit transfer commands
        const VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &uploadCommand
        };

        if (const auto res = vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, uploadComplete.handle);
            VK_SUCCESS != res) {
            throw vkutils::Error("Submitting commands\n"
                                 "vkQueueSubmit() returned %s", vkutils::to_string(res).c_str()
            );
        }

        // Wait for commands to finish before we destroy the temporary resources required for the transfers (staging
        // buffers, command pool, ...)
        //
        // The code doesn’t destory the resources implicitly – the resources are destroyed by the destructors of the
        // vkutils wrappers for the various objects once we leave the function’s scope.
        if (const auto res = vkWaitForFences(context.device, 1, &uploadComplete.handle, VK_TRUE,
                                             std::numeric_limits<std::uint64_t>::max());
            VK_SUCCESS != res) {
            throw vkutils::Error("Waiting for upload to complete\n"
                                 "vkWaitForFences() returned %s", vkutils::to_string(res).c_str()
            );
        }
    }

    std::pair<vkutils::Buffer, vkutils::Buffer> stage_to_gpu_buffers(const vkutils::Allocator& allocator,
                                                                     const std::size_t size,
                                                                     const VkBufferUsageFlagBits bufferUsage) {
        return {
            vkutils::create_buffer(
                allocator,
                size,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
            ),
            vkutils::create_buffer(
                allocator,
                size,
                bufferUsage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                0, // no additional VmaAllocationCreateFlags
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE // or just VMA MEMORY USAGE AUTO
            )
        };
    }

    mesh::Mesh allocate(const vkutils::VulkanContext& context,
                        const baked::BakedMeshData& mesh,
                        const vkutils::Allocator& allocator,
                        const vkutils::CommandPool& uploadPool) {
        auto [positionsStaging, positionsGPU] = stage_to_gpu_buffers(
            allocator, sizeof(glm::vec3) * mesh.positions.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

        auto [normalsStaging, normalsGPU] = stage_to_gpu_buffers(
            allocator, sizeof(glm::vec3) * mesh.normals.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

        auto [uvsStaging, uvsGPU] = stage_to_gpu_buffers(
            allocator, sizeof(glm::vec2) * mesh.uvs.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

        auto [tangentsStaging, tangentsGPU] = stage_to_gpu_buffers(
            allocator, sizeof(glm::vec4) * mesh.tangents.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

        auto [indicesStaging, indicesGPU] = stage_to_gpu_buffers(
            allocator, sizeof(std::uint32_t) * mesh.indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

        map_vertices_to_gpu_memory(context,
                                   allocator,
                                   uploadPool,
                                   mesh,
                                   positionsStaging, positionsGPU,
                                   uvsStaging, uvsGPU,
                                   normalsStaging, normalsGPU,
                                   tangentsStaging, tangentsGPU,
                                   indicesStaging, indicesGPU);

        return mesh::Mesh{
            .name = mesh.name,
            .positions = std::move(positionsGPU),
            .uvs = std::move(uvsGPU),
            .normals = std::move(normalsGPU),
            .tangents = std::move(tangentsGPU),
            .indices = std::move(indicesGPU),
            .materialId = mesh.materialId,
            .indexCount = static_cast<std::uint32_t>(mesh.indices.size())
        };
    }
}

namespace mesh {
    std::vector<Mesh> extract_meshes(const vkutils::VulkanContext& context,
                                     const vkutils::Allocator& allocator,
                                     const baked::BakedModel& model) {
        std::vector<Mesh> meshes;
        meshes.reserve(model.meshes.size());

        // This uses a separate command pool for simplicity
        const vkutils::CommandPool uploadPool = vkutils::create_command_pool(context);

        for (const auto& modelMesh : model.meshes) {
            meshes.emplace_back(allocate(context, modelMesh, allocator, uploadPool));
        }

        return meshes;
    }
}
