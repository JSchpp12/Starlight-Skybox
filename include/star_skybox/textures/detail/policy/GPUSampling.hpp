#pragma once

#include "star_skybox/detail/SkyboxTransferCore.hpp"
#include "star_skybox/struct/SkyboxComputeResources.hpp"

#include "StarBuffers/Buffer.hpp"
#include "StarTextures/Texture.hpp"

#include <starlight/setup/RenderResourceSetup.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace star::skybox::TransferRequest
{
/// @brief Sampling policy for a cube texture written by a graphics-family compute dispatch.
/// @details An equirectangular source is staged raw; the policy builds the descriptor set and the setup steps that
/// convert it into the six cube layers, then publishes them with the worker's contribution. A linear source still
/// takes the transfer-queue path and contributes nothing.
class GPUSampling
{
  public:
    static constexpr bool usesComputeConversion = true;

    /// @param computeResources Pipeline/layouts/promise the maker built for the conversion dispatch.
    explicit GPUSampling(SkyboxComputeResources computeResources);

    /// @brief Creates the sampled cube texture and, for an equirectangular source, the conversion work.
    /// @param conversionBuffer Device-local equirect buffer the dispatch reads; required for the conversion path.
    [[nodiscard]] std::unique_ptr<star::StarTextures::Texture> createFinal(
        star::core::device::StarDevice &device, const star::skybox::detail::SkyboxTransferCore &core,
        const std::vector<uint32_t> &transferQueueFamilyIndices,
        star::StarBuffers::Buffer *conversionBuffer) const;

    /// @brief Publishes the conversion steps and their cleanup once the staging buffer has been handed over.
    void publishContribution();

    /// @brief Publishes a terminal failure so the setup logs it and continues with a blank resource.
    void publishFailure(const std::string &message);

  private:
    SkyboxComputeResources m_compute;
    mutable std::vector<star::setup::RenderResourceSetup::Step> m_pendingSteps{};
    mutable std::function<void(star::core::device::DeviceContext &)> m_pendingKeepAlive{};
};
} // namespace star::skybox::TransferRequest
