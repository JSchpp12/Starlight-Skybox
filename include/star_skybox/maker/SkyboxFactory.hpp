#pragma once

#include "star_skybox/structs/SkyboxGPUTextureStorageApproach.hpp"
#include "star_skybox/structs/SkyboxInfo.hpp"

#include <star_common/Handle.hpp>

#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>

namespace starlight::skybox::maker
{
using starlight::skybox::SkyboxInfo;

/// @brief Creates the transfer request that uploads a skybox and submits it through ManagerRenderResource.
/// @details The request selects its transfer policy from the loaded SkyboxInfo's layout, so the factory does not need
/// to know which layout an asset uses; the sidecar can still be read asynchronously.
class SkyboxFactory
{
  public:
    /// @param deviceID Device the skybox is uploaded to.
    /// @param skyboxInfo Future carrying the loaded skybox description.
    /// @param graphicsQueueFamilyIndex Family index of the engine's graphics queue.
    /// @param texturePath Path of the source image file the code selected; the request transfers this image.
    /// @param storageApproach GPU resource to transfer the skybox into: a cube texture or a storage buffer.
    /// @return Handle to the transferred skybox resource.
    [[nodiscard]] static star::Handle MakeSkyboxGPUData(const star::Handle &deviceID,
                                                        std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo,
                                                        uint32_t graphicsQueueFamilyIndex,
                                                        std::filesystem::path texturePath,
                                                        SkyboxGPUTextureStorageApproach storageApproach);
};
} // namespace starlight::skybox::maker
