#pragma once

#include <cstdint>
#include <filesystem>
#include <stb_image.h>
#include <string>

#include "../vkutils/allocator.hpp"
#include "../vkutils/vkimage.hpp"
#include "../vkutils/vkobject.hpp"
#include "../vkutils/vulkan_context.hpp"

namespace texture {
    // Thin wrapper that loads an image texture using stb_image
    // Higher level abstraction used for image loading caching logic in mesh.cpp
    struct Texture {
        explicit Texture(const std::string& path);

        explicit Texture(const std::filesystem::path& path);

        // Move constructor
        Texture(Texture&& other) noexcept;

        // Move assignment operator
        Texture& operator=(Texture&& other) noexcept;

        std::string path;

        stbi_uc* data;

        std::uint32_t width;
        std::uint32_t height;

        std::uint32_t sizeInBytes() const;

        ~Texture();
    };

    vkutils::Image texture_to_image(const vkutils::VulkanContext& context,
                                    const Texture& texture,
                                    VkFormat format,
                                    const vkutils::Allocator& allocator,
                                    const vkutils::CommandPool& loadCommandPool);
}
