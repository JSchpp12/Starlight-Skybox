#pragma once

#include "star_skybox/structs/SkyboxInfo.hpp"

#include <starlight/virtual/TransferRequest_Texture.hpp>

#include <cstdint>
#include <future>
#include <memory>

namespace starlight::skybox::maker
{
using starlight::skybox::SkyboxInfo;
/// @brief Creates the transfer request that uploads a skybox texture.
/// @details The request selects its transfer policy from the loaded SkyboxInfo's layout, so the factory does not need
/// to know which layout an asset uses; the sidecar can still be read asynchronously.
class SkyboxFactory
{
  public:
    /// @param skyboxInfo Future carrying the loaded skybox description.
    /// @param graphicsQueueFamilyIndex Family index of the engine's graphics queue.
    /// @return The transfer request to submit through ManagerRenderResource.
    [[nodiscard]] static std::unique_ptr<star::TransferRequest::Texture> MakeSkybox(
        std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo, uint32_t graphicsQueueFamilyIndex);
};
} // namespace starlight::skybox::maker
