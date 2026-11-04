#pragma once

#include <star_common/Handle.hpp>

#include <string>

namespace star::core::device
{
class DeviceContext;
}

namespace star_skybox::maker
{
/// @brief Loads a skybox description from a JSON file and submits the texture transfer request that backs it.
class MakeSkybox
{
  public:
    explicit MakeSkybox(star::core::device::DeviceContext &deviceContext);

    /// @brief Name of the skybox texture file. Must be set before calling make(). The sidecar info file is resolved
    /// from this name as <name-without-extension>.skyboxinfo.json.
    MakeSkybox &setTexture(std::string texture);

    /// @brief Queues the info read, submits the transfer request, and returns the handle to the resulting texture.
    [[nodiscard]] star::Handle make();

  private:
    star::core::device::DeviceContext &m_deviceContext;
    std::string m_texture;
};
} // namespace star_skybox::maker