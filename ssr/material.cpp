#include "material.hpp"

#include <array>

#include "config.hpp"
#include "texture.hpp"
#include "../vkutils/error.hpp"
#include "../vkutils/to_string.hpp"

namespace material {
    void load_material_texture(const baked::BakedModel& model,
                               const std::uint32_t textureId,
                               const VkFormat format,
                               const vkutils::VulkanContext& context,
                               const vkutils::Allocator& allocator,
                               const vkutils::CommandPool& loadCommandPool,
                               std::vector<vkutils::Image>& textures) {
        if (textures[textureId].image != VK_NULL_HANDLE) {
            return;
        }

        const auto bakedTexture = model.textures[textureId];
        textures[textureId] = texture_to_image(
            context, texture::Texture(bakedTexture.path), format, allocator, loadCommandPool);
    }

    MaterialStore extract_materials(const baked::BakedModel& model,
                                    const vkutils::VulkanContext& context,
                                    const vkutils::Allocator& allocator) {
        std::vector<vkutils::Image> textures;
        textures.resize(model.textures.size());
        std::vector<Material> materials;
        materials.reserve(model.materials.size());
        const vkutils::CommandPool loadCommandPool = vkutils::create_command_pool(
            context, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

        for (const auto& modelMaterial : model.materials) {
            load_material_texture(model, modelMaterial.baseColourTextureId, Material::COLOUR_FORMAT,
                                  context, allocator, loadCommandPool, textures);
            load_material_texture(model, modelMaterial.roughnessTextureId, Material::LINEAR_FORMAT,
                                  context, allocator, loadCommandPool, textures);
            load_material_texture(model, modelMaterial.metalnessTextureId, Material::LINEAR_FORMAT,
                                  context, allocator, loadCommandPool, textures);
            load_material_texture(model, modelMaterial.normalMapTextureId, Material::LINEAR_FORMAT,
                                  context, allocator, loadCommandPool, textures);

            assert(textures[modelMaterial.baseColourTextureId].image != VK_NULL_HANDLE);
            assert(textures[modelMaterial.roughnessTextureId].image != VK_NULL_HANDLE);
            assert(textures[modelMaterial.metalnessTextureId].image != VK_NULL_HANDLE);
            assert(textures[modelMaterial.normalMapTextureId].image != VK_NULL_HANDLE);

            // Marked as [[maybe_unused]] to avoid generating warnings in release mode
            // Variable is only accessed by assert(...) calls, only relevant in debug mode
            [[maybe_unused]] const auto& material = materials.emplace_back(
                modelMaterial.name,
                glsl::MaterialPushConstants{
                    .baseColour = modelMaterial.baseColour,
                    .roughness = modelMaterial.roughness,
                    .metalness = modelMaterial.metalness
                },
                vkutils::image_to_view(context, textures[modelMaterial.baseColourTextureId].image,
                                       VK_IMAGE_VIEW_TYPE_2D, Material::COLOUR_FORMAT,
                                       VK_IMAGE_ASPECT_COLOR_BIT),
                vkutils::image_to_view(context, textures[modelMaterial.roughnessTextureId].image,
                                       VK_IMAGE_VIEW_TYPE_2D, Material::LINEAR_FORMAT,
                                       VK_IMAGE_ASPECT_COLOR_BIT),
                vkutils::image_to_view(context, textures[modelMaterial.metalnessTextureId].image,
                                       VK_IMAGE_VIEW_TYPE_2D, Material::LINEAR_FORMAT,
                                       VK_IMAGE_ASPECT_COLOR_BIT),
                vkutils::image_to_view(context, textures[modelMaterial.normalMapTextureId].image,
                                       VK_IMAGE_VIEW_TYPE_2D, Material::LINEAR_FORMAT,
                                       VK_IMAGE_ASPECT_COLOR_BIT)
            );

            assert(material.baseColour.handle != VK_NULL_HANDLE);
            assert(material.roughness.handle != VK_NULL_HANDLE);
            assert(material.metalness.handle != VK_NULL_HANDLE);
            assert(material.normalMap.handle != VK_NULL_HANDLE);
        }

        return MaterialStore{
            .textures = std::move(textures),
            .materials = std::move(materials)
        };
    }

    vkutils::DescriptorSetLayout create_descriptor_layout(const vkutils::VulkanContext& context) {
        constexpr std::array bindings{
            // Base Colour
            VkDescriptorSetLayoutBinding{
                .binding = 0, // layout(set = ..., binding = 0)
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            },
            // Roughness
            VkDescriptorSetLayoutBinding{
                .binding = 1, // layout(set = ..., binding = 1)
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            },
            // Metalness
            VkDescriptorSetLayoutBinding{
                .binding = 2, // layout(set = ..., binding = 2)
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            },
            // Normal Map
            VkDescriptorSetLayoutBinding{
                .binding = 3, // layout(set = ..., binding = 3)
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            }
        };

        const VkDescriptorSetLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = bindings.size(),
            .pBindings = bindings.data()
        };

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        if (const auto res = vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &layout);
            VK_SUCCESS != res) {
            throw vkutils::Error("Unable to create descriptor set layout\n"
                                 "vkCreateDescriptorSetLayout() returned %s", vkutils::to_string(res).c_str()
            );
        }

        return vkutils::DescriptorSetLayout(context.device, layout);
    }

    void update_descriptor_set(const vkutils::VulkanContext& context,
                               const VkDescriptorSet materialDescriptorSet,
                               const material::Material& material,
                               const vkutils::Sampler& anisotropySampler,
                               const vkutils::Sampler& pointSampler) {
        const std::array<const VkDescriptorImageInfo, 4> textureDescriptors{
            VkDescriptorImageInfo{
                .sampler = anisotropySampler.handle,
                .imageView = material.baseColour.handle,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            },
            VkDescriptorImageInfo{
                .sampler = pointSampler.handle,
                .imageView = material.roughness.handle,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            },
            VkDescriptorImageInfo{
                .sampler = pointSampler.handle,
                .imageView = material.metalness.handle,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            },
            VkDescriptorImageInfo{
                .sampler = anisotropySampler.handle,
                .imageView = material.normalMap.handle,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        };

        const std::array<const VkWriteDescriptorSet, textureDescriptors.size()> writeDescriptor{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = materialDescriptorSet,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &textureDescriptors[0]
            },
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = materialDescriptorSet,
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &textureDescriptors[1]
            },
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = materialDescriptorSet,
                .dstBinding = 2,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &textureDescriptors[2]
            },
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = materialDescriptorSet,
                .dstBinding = 3,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &textureDescriptors[3]
            }
        };

        vkUpdateDescriptorSets(context.device, writeDescriptor.size(), writeDescriptor.data(), 0, nullptr);
    }
}
