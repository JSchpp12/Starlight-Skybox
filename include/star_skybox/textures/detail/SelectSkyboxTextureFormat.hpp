#pragma once

#include "star_skybox/struct/SkyboxInfo.hpp"
#include "star_skybox/struct/SkyboxTextureOptions.hpp"

#include <vulkan/vulkan.hpp>

namespace star::skybox::detail
{
using star::skybox::SkyboxFormatScaling;
using star::skybox::SkyboxLayout;
/// @brief Selects the Vulkan format for a skybox texture from its layout and precision scale.
/// @details All current skybox sources are HDR, so every layout maps to a floating-point format.
inline vk::Format SelectFormat(const SkyboxLayout &layout, const SkyboxFormatScaling &formatScaling)
{
    switch (layout)
    {
    case SkyboxLayout::Linear:
    case SkyboxLayout::Equirect:
        switch (formatScaling)
        {
        case SkyboxFormatScaling::Full:
            return vk::Format::eR32G32B32A32Sfloat;
        case SkyboxFormatScaling::Half:
            return vk::Format::eR16G16B16A16Sfloat;
        }
        break;
    }

    return vk::Format::eUndefined;
}
} // namespace star::skybox::detail
