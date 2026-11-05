#pragma once

#include "star_skybox/structs/SkyboxGPUTextureStorageApproach.hpp"

#include <star_common/Handle.hpp>

#include <string>

namespace star::core::device
{
class DeviceContext;
}

namespace starlight::skybox::maker
{
/// @brief Loads a skybox description from a JSON file and submits the texture transfer request that backs it.
class MakeSkybox
{
  public:
    explicit MakeSkybox(star::core::device::DeviceContext &deviceContext);

    /// @brief Path of the skybox texture file. Must be set before calling make().
    MakeSkybox &setTexture(std::string texture);

    /// @brief Selects the GPU resource the skybox image is transferred into.
    /// @details Defaults to SkyCubeTexture, which builds a sampled cube texture; SkyBuffer instead builds a storage
    /// buffer holding the transformed pixel data.
    MakeSkybox &setTextureHandling(SkyboxGPUTextureStorageApproach approach);

    /// @brief Queues the info read, submits the transfer request, and returns the handle to the resulting resource.
    /// @details The transfer policy is chosen from the sidecar's layout, so the same maker handles every skybox
    /// layout without the caller having to know which one the asset uses.
    [[nodiscard]] star::Handle make();

  private:
    star::core::device::DeviceContext &m_deviceContext;
    std::string m_texture;
    SkyboxGPUTextureStorageApproach m_textureHandling{SkyboxGPUTextureStorageApproach::SkyCubeTexture};
};
} // namespace starlight::skybox::maker