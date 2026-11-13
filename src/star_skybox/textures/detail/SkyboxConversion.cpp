#include "star_skybox/textures/detail/SkyboxConversion.hpp"

#include "star_skybox/textures/detail/SkyboxTextureCreation.hpp"
#include "star_skybox/textures/detail/policy/EquirectTexturePolicy.hpp"

#include <starlight/core/Exceptions.hpp>
#include <starlight/core/device/DeviceContext.hpp>
#include <starlight/core/device/managers/Buffer.hpp>
#include <starlight/core/device/managers/GraphicsContainer.hpp>
#include <starlight/structs/ImageFileInfo.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace star::skybox::detail
{
namespace
{
using Setup = star::setup::RenderResourceSetup;

constexpr std::uint32_t k_cubeFaceCount = 6u;
constexpr std::uint32_t k_equirectWorkgroupSize = 8u;
} // namespace

static vk::ImageSubresourceRange CubeColorRange()
{
    return vk::ImageSubresourceRange()
        .setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(k_cubeFaceCount);
}

SkyboxConversionTexture CreateSkyboxConversionTexture(star::core::device::DeviceContext &context,
                                                      const SkyboxInfo &info)
{
    if (info.layout != SkyboxLayout::Equirect)
        STAR_THROW("GPU skybox conversion requires an equirectangular source");

    const SkyboxTextureSpec spec = TransferRequest::EquirectTexturePolicy{}.describeTexture(info, {});
    const std::string debugName = info.name.empty() ? "skybox" : info.name;

    SkyboxConversionTexture result;
    result.texture = TransferRequest::detail::CreateSampledTexture(
        context.getDevice(), spec, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage,
        vk::SharingMode::eExclusive, {}, debugName);
    if (result.texture == nullptr)
        STAR_THROW("Failed to create skybox conversion texture");

    result.storageView =
        result.texture->getImageView().get(result.texture->getBaseFormat(), vk::ImageViewType::e2DArray);
    if (result.storageView == VK_NULL_HANDLE)
    {
        vk::Device device = context.getDevice().getVulkanDevice();
        result.texture->cleanupRender(device);
        result.texture.reset();
        STAR_THROW("Skybox conversion texture has no 2D-array view");
    }

    return result;
}

void BuildSkyboxConversion(star::core::device::DeviceContext &context, const SkyboxInfo &info,
                           const SkyboxConversionInputs &inputs, vk::Image targetImage, vk::ImageView storageView,
                           const SkyboxConversionPipeline &pipeline, vk::DescriptorPool pool, star::Handle sourceBuffer,
                           Setup::ResourcePrepDefinition &out)
{
    if (targetImage == VK_NULL_HANDLE || storageView == VK_NULL_HANDLE)
        STAR_THROW("Skybox conversion requires the target texture");

    vk::DescriptorSet descriptorSet{};
    try
    {
        vk::Device device = context.getDevice().getVulkanDevice();

        descriptorSet =
            device
                .allocateDescriptorSets(
                    vk::DescriptorSetAllocateInfo().setDescriptorPool(pool).setSetLayouts(pipeline.descriptorSetLayout))
                .front();
        if (descriptorSet == VK_NULL_HANDLE)
            STAR_THROW("Failed to allocate the skybox conversion descriptor set");

        auto &bufferManager = context.getGraphicsManagers().bufferManager;
        bufferManager.waitForReady(sourceBuffer);

        const auto *record = bufferManager.get(sourceBuffer);
        const auto *source = record != nullptr ? record->getBuffer() : nullptr;
        if (source == nullptr || source->getVulkanBuffer() == VK_NULL_HANDLE)
            STAR_THROW("Skybox conversion source buffer is not available");

        const vk::DescriptorBufferInfo bufferInfo{source->getVulkanBuffer(), 0, source->getBufferSize()};
        const vk::DescriptorImageInfo imageInfo{vk::Sampler{}, storageView, vk::ImageLayout::eGeneral};

        const std::array<vk::WriteDescriptorSet, 2> writes{vk::WriteDescriptorSet()
                                                               .setDstSet(descriptorSet)
                                                               .setDstBinding(0)
                                                               .setDescriptorCount(1)
                                                               .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                                                               .setBufferInfo(bufferInfo),
                                                           vk::WriteDescriptorSet()
                                                               .setDstSet(descriptorSet)
                                                               .setDstBinding(1)
                                                               .setDescriptorCount(1)
                                                               .setDescriptorType(vk::DescriptorType::eStorageImage)
                                                               .setImageInfo(imageInfo)};
        device.updateDescriptorSets(writes, {});

        auto dispatch = std::make_shared<Setup::ComputeDispatch>();
        dispatch->pipeline = pipeline.pipeline;
        dispatch->descriptorSet = descriptorSet;
        dispatch->layout = pipeline.pipelineLayout;
        // The commit gate guarantees the pipeline is built before this definition is recorded.
        dispatch->context = &context;

        std::vector<std::byte> pushConstantBytes(sizeof(EquirectPushConstants));
        std::memcpy(pushConstantBytes.data(), &inputs.pushConstants, sizeof(EquirectPushConstants));
        dispatch->pushConstants = std::move(pushConstantBytes);

        const std::uint32_t faceSize = info.textureOptions.faceSize;
        if (faceSize == 0)
            STAR_THROW("Skybox texture options are incomplete: face_size is required");

        const std::uint32_t groups = (faceSize + k_equirectWorkgroupSize - 1u) / k_equirectWorkgroupSize;
        dispatch->dispatchDims = {groups, groups, k_cubeFaceCount};

        out.steps.push_back(Setup::makeStep(Setup::ImageLayoutChange{.image = targetImage,
                                                                     .from = vk::ImageLayout::eUndefined,
                                                                     .to = vk::ImageLayout::eGeneral,
                                                                     .range = CubeColorRange()}));
        out.steps.push_back(Setup::makeStep(std::move(dispatch)));
        out.steps.push_back(Setup::makeStep(Setup::ImageLayoutChange{.image = targetImage,
                                                                     .from = vk::ImageLayout::eGeneral,
                                                                     .to = vk::ImageLayout::eShaderReadOnlyOptimal,
                                                                     .range = CubeColorRange()}));

        out.release.gateOnRecordingSubmission = true;
        out.release.keepAlive = [pool, descriptorSet, sourceBuffer](star::core::device::DeviceContext &ctx) {
            const std::vector<vk::DescriptorSet> sets{descriptorSet};
            ctx.getDevice().getVulkanDevice().freeDescriptorSets(pool, sets);

            ctx.getGraphicsManagers().bufferManager.deleteRequest(ctx.getDevice(), sourceBuffer);
        };
    }
    catch (...)
    {
        if (descriptorSet != VK_NULL_HANDLE)
        {
            const std::vector<vk::DescriptorSet> sets{descriptorSet};
            context.getDevice().getVulkanDevice().freeDescriptorSets(pool, sets);
        }
        throw;
    }
}
} // namespace star::skybox::detail
