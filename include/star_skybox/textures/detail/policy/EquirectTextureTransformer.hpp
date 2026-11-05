#pragma once

#include <starlight/structs/ExrData.hpp>

namespace star
{
struct ImageFileInfo;
} // namespace star

namespace starlight::skybox
{
struct SkyboxInfo;

/// @brief Reshapes an equirectangular skybox image into the six faces of a cube texture.
/// @details Resamples the RGBA float pixels decoded from an EXR file into six square faces, one per cube face, in the
/// layer order Vulkan uses when sampling a cube map (+X, -X, +Y, -Y, +Z, -Z). The faces are written back-to-back so
/// the result can be uploaded directly into a cube-compatible image with six array layers. Satisfies
/// io::SkyboxTransform for use with io::SkyboxReader.
class EquirectTextureTransformer
{
  public:
    EquirectTextureTransformer() = default;

    /// @param data RGBA float pixels decoded from the source EXR file, row-major with the first row at the top.
    /// @param skyboxInfo Skybox description; supplies the cube face size and the yaw rotation about the vertical axis.
    /// @param imageFileInfo Source image header; supplies the equirectangular image's resolution.
    /// @returns The six cube faces, each faceSize x faceSize, stacked vertically in face order.
    /// @throws RuntimeError when the face size is zero or the pixel data does not match the image resolution.
    star::ExrData operator()(star::ExrData data, const SkyboxInfo &skyboxInfo,
                             const star::ImageFileInfo &imageFileInfo) const;
};
} // namespace starlight::skybox
