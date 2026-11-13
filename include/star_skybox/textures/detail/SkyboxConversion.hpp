#pragma once

#include "star_skybox/struct/SkyboxConversionInputs.hpp"
#include "star_skybox/struct/SkyboxConversionPipeline.hpp"
#include "star_skybox/struct/SkyboxInfo.hpp"

#include "StarTextures/Texture.hpp"

#include <starlight/setup/RenderResourceSetup.hpp>

#include <star_common/Handle.hpp>

#include <memory>
#include <vulkan/vulkan.hpp>

namespace star::core::device
{
class DeviceContext;
}

namespace star::skybox::detail
{
/// @brief The GPU-only destination texture for the equirect conversion and its storage view.
struct SkyboxConversionTexture
{
    std::unique_ptr<star::StarTextures::Texture> texture;
    /// 2D-array view over every cube layer; the conversion dispatch writes through it.
    vk::ImageView storageView{VK_NULL_HANDLE};
};

/// @brief Creates the equirect conversion's cube texture. No pixel data is uploaded: the compute dispatch fills it.
/// @details The caller owns the returned texture and registers it with the Image manager.
/// @throws On creation failure or when the source layout is not equirect.
[[nodiscard]] SkyboxConversionTexture CreateSkyboxConversionTexture(star::core::device::DeviceContext &context,
                                                                    const SkyboxInfo &info);

/// @brief Builds the GPU conversion resources and records its one-shot steps into `out`.
/// @details Render-thread only. Allocates the conversion descriptor set from the shared pool, writes the
/// source-buffer/storage-image descriptors, and builds the layout / dispatch / layout steps plus the gated release
/// (free set, release source buffer).
/// @param info Main-thread skybox description.
/// @param targetImage Image of the already-created conversion texture, owned by the Image manager.
/// @param storageView 2D-array view of `targetImage`.
/// @throws On allocation failure; the caller handles the failure path.
void BuildSkyboxConversion(star::core::device::DeviceContext &context, const SkyboxInfo &info,
                           const SkyboxConversionInputs &inputs, vk::Image targetImage, vk::ImageView storageView,
                           const SkyboxConversionPipeline &pipeline, vk::DescriptorPool pool,
                           star::Handle sourceBuffer, star::setup::RenderResourceSetup::ResourcePrepDefinition &out);
} // namespace star::skybox::detail
