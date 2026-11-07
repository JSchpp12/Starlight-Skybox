#pragma once

#include <star_common/Handle.hpp>

#include <vulkan/vulkan.hpp>

namespace star::skybox
{
/// @brief Engine-lifetime resources for the equirectangular -> cube conversion dispatch.
/// @details Minted once by SkyboxContext and reused by every make(). The two raw layouts are owned by the context and
/// destroyed in its destructor; the pipeline Handle is owned by the engine's pipeline manager and outlives the context.
/// Per-make work only copies these handles into the transfer request.
struct SkyboxConversionPipeline
{
    /// Pipeline handle owned by the engine's pipeline manager.
    star::Handle pipeline{};
    /// Layout of the dispatch's descriptor set (equirect SSBO + cube storage image).
    vk::DescriptorSetLayout descriptorSetLayout{};
    /// Layout the pipeline, descriptor set and push constants are bound through.
    vk::PipelineLayout pipelineLayout{};
};
} // namespace star::skybox
