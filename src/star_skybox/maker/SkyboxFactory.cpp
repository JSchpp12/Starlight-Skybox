#include "star_skybox/maker/SkyboxFactory.hpp"

#include "star_skybox/buffers/TransferRequest_SkyboxBuffer.hpp"
#include "star_skybox/textures/TransferRequest_SkyboxTexture.hpp"

#include <starlight/managers/ManagerRenderResource.hpp>

#include <utility>

namespace starlight::skybox::maker
{
using starlight::skybox::SkyboxInfo;
star::Handle SkyboxFactory::MakeSkyboxGPUData(const star::Handle &deviceID,
                                              std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo,
                                              uint32_t graphicsQueueFamilyIndex, std::filesystem::path texturePath,
                                              SkyboxGPUTextureStorageApproach storageApproach)
{
    star::Handle result;
    if (storageApproach == SkyboxGPUTextureStorageApproach::SkyCubeTexture)
    {
        auto request = std::make_unique<starlight::skybox::TransferRequest::SkyboxTexture>(
            std::move(skyboxInfo), graphicsQueueFamilyIndex, std::move(texturePath));
        result = star::ManagerRenderResource::addRequest(deviceID, std::move(request));
    }
    else
    {
        auto request = std::make_unique<starlight::skybox::TransferRequest::SkyboxBuffer>(
            std::move(skyboxInfo), graphicsQueueFamilyIndex, std::move(texturePath));
        result = star::ManagerRenderResource::addRequest(deviceID, std::move(request));
    }

    return result;
}
} // namespace starlight::skybox::maker
