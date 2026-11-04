#include "star_skybox/maker/SkyboxFactory.hpp"

#include "star_skybox/textures/TransferRequest_Skybox.hpp"

#include <utility>

namespace starlight::skybox::maker
{
using starlight::skybox::SkyboxInfo;
std::unique_ptr<star::TransferRequest::Texture> SkyboxFactory::MakeSkybox(
    std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo, uint32_t graphicsQueueFamilyIndex)
{
    return std::make_unique<starlight::skybox::TransferRequest::Skybox>(std::move(skyboxInfo),
                                                                        graphicsQueueFamilyIndex);
}
} // namespace starlight::skybox::maker
