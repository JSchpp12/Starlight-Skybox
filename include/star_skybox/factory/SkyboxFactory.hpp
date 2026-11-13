#pragma once

#include "star_skybox/maker/MakeSkybox.hpp"

#include <starlight/primitive/CubeObject.hpp>

#include <cstdint>
#include <filesystem>

namespace star::core::device
{
class DeviceContext;
}

namespace star::skybox
{
class SkyboxContext;
}

namespace star::skybox::factory
{
using star::skybox::SkyboxInfo;
using star::skybox::maker::SkyboxMakeResult;

/// @brief Submits the GPU path with an already-resolved sidecar description.
/// @details The cube texture has no CPU-side pixel data, so it is created and registered with the Image manager here.
/// The buffer transfer and the conversion prep remain the only asynchronous parts.
SkyboxMakeResult MakeSkyboxGPU(star::core::device::DeviceContext &deviceContext,
                               star::skybox::SkyboxContext &skyboxContext, SkyboxInfo info,
                               uint32_t graphicsQueueFamilyIndex, std::filesystem::path texturePath);

/// @brief Submits the CPU path: one texture TransferRequest, plus a prep that validates the transfer before the pass
/// registers its waits.
SkyboxMakeResult MakeSkyboxCPU(star::core::device::DeviceContext &deviceContext, SkyboxInfo info,
                               uint32_t graphicsQueueFamilyIndex, std::filesystem::path texturePath);

/// @brief Render overrides for rendering full-sky geometry (the skybox class of objects).
/// @details Disable face culling and set depth testing to less-than-or-equal, since skybox fragments sit exactly at
/// the far clipping plane.
[[nodiscard]] star::primitive::CubeObject::OptionalRenderingOverrides CreateSkyboxRenderOverrides();
} // namespace star::skybox::factory
