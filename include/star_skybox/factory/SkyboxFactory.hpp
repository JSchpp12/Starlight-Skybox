#pragma once

#include "star_skybox/context/SkyboxContext.hpp"
#include "star_skybox/maker/MakeSkybox.hpp"
#include "star_skybox/struct/SkyboxComputeResources.hpp"
#include "star_skybox/struct/SkyboxInfo.hpp"
#include "star_skybox/textures/TransferRequest_SkyboxTexture.hpp"

#include <starlight/core/device/DeviceContext.hpp>
#include <starlight/managers/ManagerRenderResource.hpp>
#include <starlight/primitive/CubeObject.hpp>
#include <starlight/setup/RenderResourceSetup.hpp>

#include <star_common/Handle.hpp>

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <utility>

namespace star::skybox::factory
{
using star::skybox::SkyboxInfo;
using star::skybox::maker::SkyboxMakeResult;

/// @brief Creates and submits the transfer request for the selected sampling policy (defined below).
template <typename SamplingPolicy>
[[nodiscard]] star::Handle MakeSkyboxGPUData(star::core::device::DeviceContext &deviceContext,
                                             star::skybox::SkyboxContext &skyboxContext,
                                             std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo,
                                             uint32_t graphicsQueueFamilyIndex, std::filesystem::path texturePath,
                                             star::setup::RenderResourceSetup::Builder &setupBuilder);

/// @brief Builds the skybox resource and the setup the consuming object records.
/// @details Selects the transfer route from the sampling policy: GPU sampling reuses the context's conversion pipeline
/// and shared pool, and adds the definitions that release the per-make resources.
/// @tparam SamplingPolicy How the cube image is written and sampled.
/// @param deviceContext Device context the skybox belongs to.
/// @param skyboxContext App-held conversion cache; only used by GPU sampling.
/// @param skyboxInfo Future carrying the loaded skybox description.
/// @param graphicsQueueFamilyIndex Family index of the engine's graphics queue.
/// @param texturePath Path of the source image file the code selected; the request transfers this image.
template <typename SamplingPolicy>
[[nodiscard]] SkyboxMakeResult MakeSkybox(star::core::device::DeviceContext &deviceContext,
                                          star::skybox::SkyboxContext &skyboxContext,
                                          std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo,
                                          uint32_t graphicsQueueFamilyIndex, std::filesystem::path texturePath)
{
    star::setup::RenderResourceSetup::Builder setupBuilder{};

    const star::Handle resource =
        MakeSkyboxGPUData<SamplingPolicy>(deviceContext, skyboxContext, std::move(skyboxInfo),
                                          graphicsQueueFamilyIndex, std::move(texturePath), setupBuilder);

    return SkyboxMakeResult{.resource = resource, .setup = std::move(setupBuilder).build()};
}

/// @brief Creates and submits the transfer request for the selected sampling policy.
/// @details For GPU sampling this also ensures the context's conversion resources exist and adds its contribution to
/// the setup builder. For CPU sampling it only creates the request.
/// @param setupBuilder Receives the definitions the setup interpreter will record.
/// @return Handle to the transferred resource.
template <typename SamplingPolicy>
[[nodiscard]] star::Handle MakeSkyboxGPUData(star::core::device::DeviceContext &deviceContext,
                                             star::skybox::SkyboxContext &skyboxContext,
                                             std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo,
                                             uint32_t graphicsQueueFamilyIndex, std::filesystem::path texturePath,
                                             star::setup::RenderResourceSetup::Builder &setupBuilder)
{
    constexpr bool computeSampling = SamplingPolicy::usesComputeConversion;

    SkyboxComputeResources computeResources{};
    if constexpr (computeSampling)
    {
        // Engine-lifetime pipeline/layouts and the shared pool are created once; the promise and the set are per make.
        skyboxContext.ensureConversionResources();
        const SkyboxConversionPipeline &pipeline = skyboxContext.conversionPipeline();
        const vk::DescriptorPool pool = skyboxContext.conversionPool();
        const vk::DescriptorSetLayout setLayout = pipeline.descriptorSetLayout;

        // Allocate here, on the calling (render) thread: only the pool and layout are needed. The transfer worker fills
        // the set later, and the per-make keepAlive frees it, so every pool operation stays on this thread.
        vk::Device device = deviceContext.getDevice().getVulkanDevice();
        const vk::DescriptorSet descriptorSet =
            device.allocateDescriptorSets(
                vk::DescriptorSetAllocateInfo().setDescriptorPool(pool).setSetLayouts(setLayout)).front();

        std::promise<std::expected<star::setup::RenderResourceSetup::ResourcePrepDefinition, std::string>>
            contributionPromise;
        setupBuilder.add(contributionPromise.get_future());

        computeResources = SkyboxComputeResources::From(pipeline, pool, descriptorSet,
                                                        std::move(contributionPromise));
    }

    std::optional<star::Handle> stagingBuffer{};

    std::unique_ptr<star::TransferRequest::Texture> request;
    if constexpr (computeSampling)
        request = std::make_unique<star::skybox::TransferRequest::SkyboxTexture<SamplingPolicy>>(
            std::move(skyboxInfo), graphicsQueueFamilyIndex, std::move(texturePath), std::move(computeResources));
    else
        request = std::make_unique<star::skybox::TransferRequest::SkyboxTexture<SamplingPolicy>>(
            std::move(skyboxInfo), graphicsQueueFamilyIndex, std::move(texturePath));

    const star::Handle resource = star::ManagerRenderResource::addRequest(
        deviceContext.getDeviceID(), std::move(request), nullptr, false, nullptr, &stagingBuffer);

    if constexpr (computeSampling)
    {
        // The staging buffer is manager-owned once adopted; release its handle once the recording submission has
        // completed. A linear asset under this policy has no compute conversion and therefore no staging handle.
        // The conversion layouts/pool are context-owned, so nothing else is released here.
        const std::optional<star::Handle> bufferHandle = stagingBuffer;
        setupBuilder.add(star::setup::RenderResourceSetup::ResourcePrepDefinition{
            .steps = {},
            .release = {.gateOnRecordingSubmission = true,
                        .keepAlive = [bufferHandle](star::core::device::DeviceContext &context) {
                            if (bufferHandle.has_value())
                                context.getBufferManager().deleteRequest(context.getDevice(), *bufferHandle);
                        }}});
    }

    return resource;
}

/// @brief Render overrides for rendering full-sky geometry (the skybox class of objects).
/// @details Disable face culling and set distance tests to less than or equal as fragments will be placed at exactly
/// the far clipping plane distance.
[[nodiscard]] inline star::primitive::CubeObject::OptionalRenderingOverrides CreateSkyboxRenderOverrides()
{
    // Explicitly sized depth state: depth test on, writes off, far-passing compare.
    // The remaining vk::PipelineDepthStencilStateCreateInfo fields (depth bounds test, stencil) default to disabled in
    // a default-constructed handle struct.
    const vk::PipelineDepthStencilStateCreateInfo depthStencil =
        vk::PipelineDepthStencilStateCreateInfo()
            .setDepthTestEnable(VK_TRUE)
            .setDepthWriteEnable(VK_FALSE)
            .setDepthCompareOp(vk::CompareOp::eLessOrEqual);

    return star::primitive::CubeObject::OptionalRenderingOverrides{.cullMode = vk::CullModeFlagBits::eNone,
                                                                   .depthStencil = depthStencil};
}
} // namespace star::skybox::factory
