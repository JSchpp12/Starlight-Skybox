#pragma once

#include <starlight/setup/RenderResourceSetup.hpp>

#include <star_common/Handle.hpp>

#include <string>

namespace star::core::device
{
class DeviceContext;
}

namespace star::skybox
{
class SkyboxContext;
}

namespace star::skybox::maker
{
/// @brief Result of building a skybox: the GPU resource and the one-shot setup the render phase must record.
/// @details The setup is complete at composition time: for a cube texture it already owns the conversion pipeline and
/// the definitions for its cleanup, plus a future the transfer worker fills with the conversion steps and its own
/// view/pool cleanup. The setup is empty for the storage-buffer path. Pass it to the object that samples the texture
/// (e.g. the CubeObject constructor).
struct SkyboxMakeResult
{
    star::Handle resource{};
    star::setup::RenderResourceSetup setup{};
};

/// @brief Loads a skybox description from a JSON file and submits the texture transfer request that backs it.
class MakeSkybox
{
  public:
    /// @param deviceContext Device the skybox belongs to.
    /// @param skyboxContext App-held conversion cache; must outlive this maker and the transfer request it submits.
    MakeSkybox(star::core::device::DeviceContext &deviceContext, star::skybox::SkyboxContext &skyboxContext);

    /// @brief Path of the skybox texture file. Must be set before calling make().
    MakeSkybox &setTexture(std::string texture);

    /// @brief Samples the cube texture from a CPU-prepared transfer copy. This is the default.
    MakeSkybox &useCPUSampling();

    /// @brief Samples the cube texture from the GPU compute conversion dispatch.
    MakeSkybox &useGPUSampling();

    /// @brief Queues the info read, submits the transfer request, and returns the resulting resource.
    /// @details The transfer policy is chosen from the sidecar's layout, so the same maker handles every skybox
    /// layout without the caller having to know which one the asset uses. A cube-texture result carries a setup the
    /// render phase must record before sampling the texture.
    [[nodiscard]] SkyboxMakeResult make();

  private:
    star::core::device::DeviceContext &m_deviceContext;
    star::skybox::SkyboxContext &m_skyboxContext;
    std::string m_texture;
    /// False selects the CPU sampling policy; true the GPU compute one.
    bool m_useGpuSampling{false};
};
} // namespace star::skybox::maker
