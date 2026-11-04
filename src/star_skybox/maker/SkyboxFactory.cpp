#include "star_skybox/maker/SkyboxFactory.hpp"

#include "star_skybox/textures/TransferRequest_Skybox.hpp"

#include <utility>

namespace star_skybox::maker
{
std::unique_ptr<star::TransferRequest::Texture> SkyboxFactory::MakeSkybox(
    std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo, uint32_t graphicsQueueFamilyIndex)
{
    return std::make_unique<star_skybox::TransferRequest::Skybox>(std::move(skyboxInfo), graphicsQueueFamilyIndex);
}
} // namespace star_skybox::maker
