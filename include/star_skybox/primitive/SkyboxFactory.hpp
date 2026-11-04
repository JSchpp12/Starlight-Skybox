#pragma once

#include <starlight/primitive/CubeObject.hpp>

namespace starlight::skybox::primitive
{
/// @brief Render overrides for rendering full-sky geometry (the skybox class of objects).
/// @details Disable face culling and set distance tests to less than or equal as fragments will be placed at exactly
/// the far clipping plane distance
[[nodiscard]] star::primitive::CubeObject::OptionalRenderingOverrides CreateSkyboxRenderOverrides();
} // namespace starlight::skybox::primitive