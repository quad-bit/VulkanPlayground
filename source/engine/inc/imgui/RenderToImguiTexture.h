#ifndef RENDER_TO_IMGUI_IMAGE_H
#define RENDER_TO_IMGUI_IMAGE_H

#include <imgui.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include "Utils.h"
#include "ImguiEditor.h"
#include "VulkanWrappers.h"

namespace Loops
{
    class RenderToImguiImage
    {
    private:
        std::vector<VkDescriptorSet> m_guiImageDescriptorSets;
        VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
        VkSampler m_imguiImageSampler = VK_NULL_HANDLE;
        const VkDevice& m_device;
        char m_name[20];
        unsigned short m_imageLayoutChangeCounter = 0;
        uint32_t m_maxFrameInFlights = 0;

    public:
        RenderToImguiImage(const char* name, const VkDevice& device, uint32_t maxFrameInFlights, const std::vector<VkImageView>& imageViews, uint32_t width, uint32_t height):
            m_device(device), m_maxFrameInFlights(maxFrameInFlights)
        {
            strncpy(m_name, name, sizeof(m_name) - 1);

            VkSamplerCreateInfo samplerCreateInfo{};
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
            samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
            samplerCreateInfo.mipLodBias = 0.0f;
            samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
            samplerCreateInfo.minLod = 0.0f;
            samplerCreateInfo.maxLod = 1.0f;
            samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
            samplerCreateInfo.maxAnisotropy = 1.0f;
            samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

            VkUtils::ErrorCheck(vkCreateSampler(device, &samplerCreateInfo, nullptr, &m_imguiImageSampler));

            VkDescriptorPoolSize pool_sizes[1] =
            {
                //{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 * m_info.m_maxFrameInFlights},
                {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1 * maxFrameInFlights}
            };

            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.flags = 0;
            poolInfo.maxSets = 1 * maxFrameInFlights; //transforms 1
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = pool_sizes;
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            Loops::VkUtils::ErrorCheck(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool));

            VkDescriptorSetLayout layout = VK_NULL_HANDLE;
            VkDescriptorSetLayoutBinding binding[1] = {};
            binding[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            binding[0].descriptorCount = 1;
            binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            VkDescriptorSetLayoutCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            info.bindingCount = 1;
            info.pBindings = binding;
            VkUtils::ErrorCheck(vkCreateDescriptorSetLayout(device, &info, nullptr, &layout));

            m_guiImageDescriptorSets.resize(maxFrameInFlights);
            for (uint16_t i = 0; i < maxFrameInFlights; i++)
            {
                VkDescriptorSetAllocateInfo setAllocInfo{};
                setAllocInfo.descriptorPool = m_descriptorPool;
                setAllocInfo.descriptorSetCount = 1;
                setAllocInfo.pSetLayouts = &layout;
                setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

                Loops::VkUtils::ErrorCheck(vkAllocateDescriptorSets(device, &setAllocInfo, &m_guiImageDescriptorSets[i]));

                VkDescriptorImageInfo imageInfo{ m_imguiImageSampler, imageViews[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                const VkWriteDescriptorSet writes
                {
                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_guiImageDescriptorSets[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo, nullptr, nullptr
                };
                vkUpdateDescriptorSets(device, 1, &writes, 0, nullptr);
            }

            {
                auto CreateGuiImage = [this, width, height](uint32_t frameIndex)
                    {
                        const float imageAspect = (float)width / (float)height;

                        if (ImGui::Begin(m_name))
                        {
                            ImVec2 availSize = ImGui::GetContentRegionAvail();
                            const float regionRatio = (float)availSize.x / (float)availSize.y;

                            ImVec2 guiImageSize;

                            if (regionRatio > imageAspect)
                            {
                                guiImageSize.x = availSize.y * imageAspect;
                                guiImageSize.y = availSize.y;

                                float xPadding = (availSize.x - guiImageSize.x) * 0.5f;
                                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + xPadding);
                            }
                            else
                            {
                                guiImageSize.x = availSize.x;
                                guiImageSize.y = availSize.x / imageAspect;

                                float yPadding = (availSize.y - guiImageSize.y) * 0.5f;
                                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yPadding);
                            }

                            ImGui::Image((ImTextureID)m_guiImageDescriptorSets[frameIndex], guiImageSize);
                            ImGui::End();
                        }
                    };
                Loops::ImguiEditor::GetInstance()->AddPersistentCalls(CreateGuiImage);
            }
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
        }

        ~RenderToImguiImage()
        {
            if (m_imguiImageSampler != VK_NULL_HANDLE)
                vkDestroySampler(m_device, m_imguiImageSampler, nullptr);
            vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        }

        void Render(VkCommandBuffer& commandBuffer, const Loops::Dimension& frameBufferDimension, VkImage& image, uint32_t frameInFlight,
            std::function<void(uint32_t)> renderFunc)
        {
            {
                if (m_imageLayoutChangeCounter >=  m_maxFrameInFlights)
                {
                    VkImageMemoryBarrier use_barrier[1] = {};
                    use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    use_barrier[0].srcAccessMask = VK_ACCESS_NONE;
                    use_barrier[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
                    use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    use_barrier[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    use_barrier[0].image = image;
                    use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    use_barrier[0].subresourceRange.levelCount = 1;
                    use_barrier[0].subresourceRange.layerCount = 1;
                    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_NONE, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, use_barrier);
                }
                else
                    m_imageLayoutChangeCounter++;

                renderFunc(frameInFlight);

                VkImageMemoryBarrier use_barrier[1] = {};
                use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                use_barrier[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                use_barrier[0].image = image;
                use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                use_barrier[0].subresourceRange.levelCount = 1;
                use_barrier[0].subresourceRange.layerCount = 1;
                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, use_barrier);
            }
        }
    };
}

#endif // !RENDER_TO_IMGUI_IMAGE_H
