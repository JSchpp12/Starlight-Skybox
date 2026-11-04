#pragma once

#include <starlight/structs/ExrData.hpp>

namespace star
{
struct ImageFileInfo;
} // namespace star

namespace starlight::skybox
{
struct SkyboxInfo;

class EquirectTextureTransformer
{
  public:
    EquirectTextureTransformer() = default;

    star::ExrData transform(star::ExrData data, const SkyboxInfo &skyboxInfo, const star::ImageFileInfo &imageFileInfo);

  private:
};
} // namespace starlight::skybox