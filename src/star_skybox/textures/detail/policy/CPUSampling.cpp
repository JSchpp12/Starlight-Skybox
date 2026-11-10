#include "star_skybox/textures/detail/policy/CPUSampling.hpp"

#include "star_skybox/textures/detail/SkyboxTextureCreation.hpp"

namespace star::skybox::TransferRequest
{
std::unique_ptr<star::StarTextures::Texture> CPUSampling::createFinal(
    star::core::device::StarDevice &device, const star::skybox::detail::SkyboxTransferCore &core,
    const std::vector<uint32_t> &transferQueueFamilyIndices, star::StarBuffers::Buffer *conversionBuffer) const
{
    (void)conversionBuffer;

    // The transfer queue writes the image, so it must be reachable from both the graphics and the transfer families.
    std::vector<uint32_t> queueFamilyIndices{core.graphicsQueueFamilyIndex()};
    queueFamilyIndices.insert(queueFamilyIndices.end(), transferQueueFamilyIndices.begin(),
                              transferQueueFamilyIndices.end());

    return detail::CreateSampledTexture(device, core.textureSpec(),
                                        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
                                        vk::SharingMode::eConcurrent, queueFamilyIndices, core.debugName());
}
} // namespace star::skybox::TransferRequest
