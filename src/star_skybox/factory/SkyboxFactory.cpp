#include "star_skybox/factory/SkyboxFactory.hpp"

#include "star_skybox/buffers/TransferRequest_SkyboxBuffer.hpp"
#include "star_skybox/context/SkyboxContext.hpp"
#include "star_skybox/textures/TransferRequest_SkyboxTexture.hpp"
#include "star_skybox/textures/detail/SkyboxConversion.hpp"

#include <starlight/command/transfer/SubmitBufferTransfer.hpp>
#include <starlight/command/transfer/SubmitTextureTransfer.hpp>
#include <starlight/core/device/DeviceContext.hpp>
#include <starlight/core/device/managers/Buffer.hpp>
#include <starlight/core/device/managers/GraphicsContainer.hpp>
#include <starlight/core/device/managers/Image.hpp>
#include <starlight/core/logging/LoggingFactory.hpp>
#include <starlight/core/transfer/ReadinessCondition.hpp>

#include <expected>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace star::skybox::factory
{
namespace gpu_detail
{
/// @brief Render-thread state shared by the GPU conversion prep callback.
struct SkyboxGpuPrepState
{
    std::future<std::expected<SkyboxConversionInputs, std::string>> inputs;
    star::core::device::DeviceContext *context{nullptr};
    SkyboxInfo info{};
    SkyboxConversionPipeline pipeline{};
    vk::DescriptorPool pool{};
    star::Handle sourceBuffer{};
    /// Destination texture, already created and registered with the Image manager by the factory.
    vk::Image targetImage{};
    vk::ImageView targetStorageView{};
};

void RunGpuPrep(SkyboxGpuPrepState &state, star::setup::RenderResourceSetup::ResourcePrepDefinition &out)
{
    if (state.context == nullptr || state.targetImage == VK_NULL_HANDLE || state.targetStorageView == VK_NULL_HANDLE)
        throw std::runtime_error("Skybox GPU prep state is incomplete");

    const auto fail = [&state, &out](const std::string &reason) {
        star::core::logging::warning("Skybox GPU conversion failed: ", reason);

        // The service's failure path waits for device idleness and runs this release, so the source buffer is not
        // stranded when the conversion cannot be built.
        if (state.sourceBuffer.isInitialized())
        {
            out.release.keepAlive = [buffer = state.sourceBuffer](star::core::device::DeviceContext &context) {
                context.getGraphicsManagers().bufferManager.deleteRequest(context.getDevice(), buffer);
            };
            out.release.gateOnRecordingSubmission = true;
        }
    };

    std::expected<SkyboxConversionInputs, std::string> inputs;
    try
    {
        inputs = state.inputs.get();
    }
    catch (const std::exception &e)
    {
        fail(e.what());
        throw;
    }

    if (!inputs)
    {
        fail(inputs.error());
        throw std::runtime_error(inputs.error());
    }

    // The inputs future resolves only after the worker's staging write and submit2 succeed and the buffer slot is
    // published. A worker failure rejects the future, so prep throws here and MaterialService keeps the previous
    // resource. Submission ordering is then provided by the pass's ForceCPURecordingWait.

    try
    {
        detail::BuildSkyboxConversion(*state.context, state.info, *inputs, state.targetImage, state.targetStorageView,
                                      state.pipeline, state.pool, state.sourceBuffer, out);
    }
    catch (const std::exception &e)
    {
        fail(e.what());
        throw;
    }
}
} // namespace gpu_detail

SkyboxMakeResult MakeSkyboxGPU(star::core::device::DeviceContext &deviceContext,
                               star::skybox::SkyboxContext &skyboxContext, SkyboxInfo info,
                               uint32_t graphicsQueueFamilyIndex, std::filesystem::path texturePath)
{
    skyboxContext.ensureConversionResources();

    detail::SkyboxConversionTexture conversion = detail::CreateSkyboxConversionTexture(deviceContext, info);
    const vk::Image targetImage = conversion.texture->getVulkanImage();
    const vk::ImageView targetStorageView = conversion.storageView;

    // The Image manager owns the ready texture from here on. No worker and no pending record: the material handle is
    // valid before the conversion prep runs.
    star::Handle image;
    try
    {
        image = deviceContext.getGraphicsManagers().imageManager.submit(
            star::core::device::manager::ImageRequest{std::move(*conversion.texture)});
    }
    catch (...)
    {
        vk::Device device = deviceContext.getDevice().getVulkanDevice();
        conversion.texture->cleanupRender(device);
        throw;
    }

    std::promise<std::expected<SkyboxConversionInputs, std::string>> inputsPromise;
    auto inputsFuture = inputsPromise.get_future();

    auto state = std::make_shared<gpu_detail::SkyboxGpuPrepState>();
    state->info = info; // copy before info is moved into the transfer request

    star::command::transfer::SubmitBufferResult bufferResult;
    try
    {
        auto bufferRequest = std::make_unique<TransferRequest::SkyboxBuffer>(
            std::move(info), graphicsQueueFamilyIndex, std::move(texturePath), std::move(inputsPromise));
        star::command::transfer::SubmitBufferTransfer bufferCommand{std::move(bufferRequest)};
        deviceContext.getCmdBus().submit(bufferCommand);
        bufferResult = bufferCommand.getReply().get();
    }
    catch (...)
    {
        // The texture is already owned by the manager; drop the record so a failed submission does not strand it.
        deviceContext.getGraphicsManagers().imageManager.deleteRequest(deviceContext.getDevice(), image);
        throw;
    }

    state->inputs = std::move(inputsFuture);
    state->context = &deviceContext;
    state->pipeline = skyboxContext.conversionPipeline();
    state->pool = skyboxContext.conversionPool();
    state->sourceBuffer = bufferResult.buffer;
    state->targetImage = targetImage;
    state->targetStorageView = targetStorageView;

    SkyboxMakeResult result;
    result.resource = image;
    // The conversion dispatch can only be recorded once the shared pipeline is built; the material service gates the
    // swap commit on this condition.
    result.condition =
        star::ReadinessCondition{.resource = state->pipeline.pipeline,
                                 .readiness = star::PipelineReadiness{.manager = &deviceContext.getPipelineManager(),
                                                                      .context = &deviceContext}};
    if (bufferResult.semaphore.vkSemaphore != VK_NULL_HANDLE)
    {
        result.wait = {.key = bufferResult.buffer,
                       .semaphore = bufferResult.semaphore,
                       .stages = vk::PipelineStageFlagBits::eComputeShader};
    }

    result.prep.prepare = [state](const star::core::device::DeviceContext &,
                                  star::setup::RenderResourceSetup::ResourcePrepDefinition &out) {
        gpu_detail::RunGpuPrep(*state, out);
    };

    return result;
}

SkyboxMakeResult MakeSkyboxCPU(star::core::device::DeviceContext &deviceContext, SkyboxInfo info,
                               uint32_t graphicsQueueFamilyIndex, std::filesystem::path texturePath)
{
    star::command::transfer::SubmitTextureTransfer command{std::make_unique<TransferRequest::SkyboxTexture>(
        std::move(info), graphicsQueueFamilyIndex, std::move(texturePath))};
    deviceContext.getCmdBus().submit(command);
    const auto submission = command.getReply().get();

    SkyboxMakeResult result;
    result.resource = submission.image;
    if (submission.semaphore.vkSemaphore != VK_NULL_HANDLE)
    {
        result.wait = {.key = submission.image,
                       .semaphore = submission.semaphore,
                       .stages = vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eVertexShader |
                                 vk::PipelineStageFlagBits::eFragmentShader};
    }

    return result;
}

star::primitive::CubeObject::OptionalRenderingOverrides CreateSkyboxRenderOverrides()
{
    // Explicitly sized depth state: depth test on, writes off, far-passing compare. The remaining
    // vk::PipelineDepthStencilStateCreateInfo fields (depth bounds test, stencil) default to disabled.
    const vk::PipelineDepthStencilStateCreateInfo depthStencil = vk::PipelineDepthStencilStateCreateInfo()
                                                                     .setDepthTestEnable(VK_TRUE)
                                                                     .setDepthWriteEnable(VK_FALSE)
                                                                     .setDepthCompareOp(vk::CompareOp::eLessOrEqual);

    return star::primitive::CubeObject::OptionalRenderingOverrides{.cullMode = vk::CullModeFlagBits::eNone,
                                                                   .depthStencil = depthStencil};
}
} // namespace star::skybox::factory
