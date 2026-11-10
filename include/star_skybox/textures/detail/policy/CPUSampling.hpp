#pragma once

#include "star_skybox/detail/SkyboxTransferCore.hpp"

#include "StarBuffers/Buffer.hpp"
#include "StarTextures/Texture.hpp"

#include <memory>
#include <string>
#include <vector>

namespace star::skybox::TransferRequest
{
/// @brief Sampling policy for a cube texture the transfer queue writes and the fragment shader samples.
class CPUSampling
{
  public:
    /// @brief The transfer queue writes the image directly; there is no compute conversion to publish.
    static constexpr bool usesComputeConversion = false;

    CPUSampling() = default;

    /// @brief Creates the sampled cube texture the transfer copy writes into.
    /// @param stagingBuffer Unused; the transfer worker owns it for the duration of the copy.
    [[nodiscard]] std::unique_ptr<star::StarTextures::Texture> createFinal(
        star::core::device::StarDevice &device, const star::skybox::detail::SkyboxTransferCore &core,
        const std::vector<uint32_t> &transferQueueFamilyIndices, star::StarBuffers::Buffer *conversionBuffer) const;

    /// @brief No compute-side contribution exists for this policy.
    void publishContribution()
    {
    }

    /// @brief No compute-side contribution exists for this policy, so there is nothing to fail.
    void publishFailure(const std::string &)
    {
    }
};
} // namespace star::skybox::TransferRequest
