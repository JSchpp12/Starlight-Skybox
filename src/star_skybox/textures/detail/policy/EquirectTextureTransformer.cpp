#include "star_skybox/textures/detail/policy/EquirectTextureTransformer.hpp"

#include "star_skybox/struct/SkyboxInfo.hpp"

#include "starlight/core/Exceptions.hpp"
#include "starlight/structs/ImageFileInfo.hpp"

#include <tbb/parallel_for.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

#include <glm/glm.hpp>

namespace star::skybox
{
/// Number of cube faces; faces are generated in Vulkan's layer order (+X, -X, +Y, -Y, +Z, -Z).
constexpr int kFaceCount = 6;
constexpr float kPi = 3.14159265358979323846f;

/// @brief Direction of the texel at normalized face coordinates (s, t), with s growing right and t growing down.
/// @details Derived from the cube map face selection table Vulkan applies when sampling, so a direction baked into
/// a face is the same direction the sampler will read back from that texel.
static glm::vec3 FaceDirection(int face, float s, float t) noexcept
{
    const float sc = 2.0f * s - 1.0f;
    const float tc = 2.0f * t - 1.0f;

    switch (face)
    {
    case 0: // +X
        return glm::vec3(1.0f, -tc, -sc);
    case 1: // -X
        return glm::vec3(-1.0f, -tc, sc);
    case 2: // +Y
        return glm::vec3(sc, 1.0f, tc);
    case 3: // -Y
        return glm::vec3(sc, -1.0f, -tc);
    case 4: // +Z
        return glm::vec3(sc, -tc, 1.0f);
    default: // -Z
        return glm::vec3(-sc, -tc, -1.0f);
    }
}

/// @brief Wraps a source column index around the equirectangular image's horizontal seam.
static int WrapColumn(int x, int width) noexcept
{
    const int wrapped = x % width;
    return wrapped < 0 ? wrapped + width : wrapped;
}

/// @brief Bilinearly samples the equirectangular source at (u, v), wrapping u and clamping v to the poles.
static glm::vec4 SampleEquirect(const float *pixels, int width, int height, float u, float v) noexcept
{
    const float x = u * static_cast<float>(width) - 0.5f;
    const float y = v * static_cast<float>(height) - 0.5f;

    const float x0f = std::floor(x);
    const float y0f = std::floor(y);
    const float fx = x - x0f;
    const float fy = y - y0f;

    const int left = WrapColumn(static_cast<int>(x0f), width);
    const int right = WrapColumn(static_cast<int>(x0f) + 1, width);
    const int top = std::clamp(static_cast<int>(y0f), 0, height - 1);
    const int bottom = std::clamp(static_cast<int>(y0f) + 1, 0, height - 1);

    const auto texel = [pixels, width](int tx, int ty) {
        return pixels +
               4u * (static_cast<std::size_t>(ty) * static_cast<std::size_t>(width) + static_cast<std::size_t>(tx));
    };

    const float *topLeft = texel(left, top);
    const float *topRight = texel(right, top);
    const float *bottomLeft = texel(left, bottom);
    const float *bottomRight = texel(right, bottom);

    glm::vec4 result{};
    for (int channel = 0; channel < 4; ++channel)
    {
        const float upper = topLeft[channel] + (topRight[channel] - topLeft[channel]) * fx;
        const float lower = bottomLeft[channel] + (bottomRight[channel] - bottomLeft[channel]) * fx;
        result[channel] = upper + (lower - upper) * fy;
    }

    return result;
}

star::ExrData EquirectTextureTransformer::operator()(star::ExrData data, const SkyboxInfo &skyboxInfo,
                                                     const star::ImageFileInfo &imageFileInfo) const
{
    const std::uint32_t faceSize = skyboxInfo.textureOptions.faceSize;
    if (faceSize == 0)
        STAR_THROW("EquirectTextureTransformer: skybox texture options are missing face_size");

    const int width = static_cast<int>(imageFileInfo.resolution.x);
    const int height = static_cast<int>(imageFileInfo.resolution.y);
    const std::span<float> source = data.data();
    const std::size_t expectedFloats = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    if (width <= 0 || height <= 0 || source.size() < expectedFloats)
        STAR_THROW("EquirectTextureTransformer: source pixel data does not match the image resolution");

    const std::size_t faceTexels = static_cast<std::size_t>(faceSize) * static_cast<std::size_t>(faceSize);
    const std::size_t outputFloats = faceTexels * 4u * static_cast<std::size_t>(kFaceCount);
    auto *output = static_cast<float *>(std::malloc(outputFloats * sizeof(float)));
    if (output == nullptr)
        STAR_THROW("EquirectTextureTransformer: failed to allocate the cube face buffer");

    // Positive yaw rotates the environment counter-clockwise about the vertical axis, so each face direction is
    // rotated the other way before it is mapped into the equirectangular source.
    const float yaw = skyboxInfo.yawDegrees * (kPi / 180.0f);
    const float cosYaw = std::cos(yaw);
    const float sinYaw = std::sin(yaw);
    const float invFaceSize = 1.0f / static_cast<float>(faceSize);

    // Each face owns a disjoint slice of the output, so the six faces can be filled concurrently.
    tbb::parallel_for(0, kFaceCount, [&](int face) {
        float *facePixels = output + static_cast<std::size_t>(face) * faceTexels * 4u;

        for (std::uint32_t row = 0; row < faceSize; ++row)
        {
            const float t = (static_cast<float>(row) + 0.5f) * invFaceSize;

            for (std::uint32_t column = 0; column < faceSize; ++column)
            {
                const float s = (static_cast<float>(column) + 0.5f) * invFaceSize;
                const glm::vec3 direction = FaceDirection(face, s, t);

                const glm::vec3 rotated{cosYaw * direction.x - sinYaw * direction.z, direction.y,
                                        sinYaw * direction.x + cosYaw * direction.z};
                const glm::vec3 normalized = rotated / glm::length(rotated);

                // Longitude runs from +X towards +Z across u, and v runs from the +Y pole at the top row to the
                // -Y pole at the bottom row.
                const float u = 0.5f + std::atan2(normalized.z, normalized.x) / (2.0f * kPi);
                const float v = 0.5f - std::asin(std::clamp(normalized.y, -1.0f, 1.0f)) / kPi;

                const glm::vec4 color = SampleEquirect(source.data(), width, height, u, v);
                float *destination = facePixels + (static_cast<std::size_t>(row) * faceSize + column) * 4u;
                destination[0] = color.r;
                destination[1] = color.g;
                destination[2] = color.b;
                destination[3] = color.a;
            }
        }
    });

    return star::ExrData(output, outputFloats);
}
} // namespace star::skybox
