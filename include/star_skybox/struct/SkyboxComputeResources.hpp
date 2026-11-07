#pragma once

#include "star_skybox/struct/SkyboxConversionPipeline.hpp"

#include <starlight/setup/RenderResourceSetup.hpp>

#include <star_common/Handle.hpp>

#include <expected>
#include <future>
#include <optional>
#include <string>
#include <vulkan/vulkan.hpp>

namespace star::skybox
{
/// @brief Per-make inputs for the equirectangular -> cube conversion dispatch.
/// @details Composed by the factory on the calling thread from the app-held SkyboxContext. The handles are copies of
/// the context's engine-lifetime resources, so the transfer worker never dereferences the context.
///
/// The descriptor set is allocated here on the calling (render) thread: allocation only needs the pool and set layout,
/// not the resources the set will bind. The transfer worker fills it in (vkUpdateDescriptorSets touches the set, not
/// the pool), and the per-make keepAlive frees it back to the pool. So every pool operation stays on the render thread.
struct SkyboxComputeResources
{
    /// Pipeline handle owned by the engine's pipeline manager.
    star::Handle pipeline{};
    /// Layout the pipeline, descriptor set and push constants are bound through, owned by SkyboxContext.
    vk::PipelineLayout pipelineLayout{};
    /// Shared conversion pool the set was allocated from; freed back to it once the dispatch gate passes.
    vk::DescriptorPool descriptorPool{};
    /// Set allocated from descriptorPool by the maker. The worker writes its descriptors.
    vk::DescriptorSet descriptorSet{};
    /// Producer side of the worker contribution. The maker puts the matching future into the setup it returns.
    /// @details The produced value is the definition, or a failure reason; a failed producer publishes an unexpected so
    /// the setup can log it and continue with a blank resource.
    std::optional<
        std::promise<std::expected<star::setup::RenderResourceSetup::ResourcePrepDefinition, std::string>>>
        contributionPromise{};

    /// @brief Builds the per-make inputs from the context's cached pipeline/pool and the allocated set.
    static SkyboxComputeResources From(
        const SkyboxConversionPipeline &pipeline, vk::DescriptorPool pool, vk::DescriptorSet set,
        std::promise<std::expected<star::setup::RenderResourceSetup::ResourcePrepDefinition, std::string>> promise)
    {
        return SkyboxComputeResources{.pipeline = pipeline.pipeline,
                                      .pipelineLayout = pipeline.pipelineLayout,
                                      .descriptorPool = pool,
                                      .descriptorSet = set,
                                      .contributionPromise = std::move(promise)};
    }

    bool isValid() const
    {
        return pipeline.isInitialized() && pipelineLayout && descriptorPool && descriptorSet &&
               contributionPromise.has_value();
    }
};
} // namespace star::skybox
