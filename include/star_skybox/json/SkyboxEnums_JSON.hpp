#pragma once

#include "star_skybox/structs/SkyboxInfo.hpp"
#include "star_skybox/structs/SkyboxTextureOptions.hpp"

#include <nlohmann/json.hpp>

namespace star_skybox
{
void to_json(nlohmann::json &j, const SkyboxLayout &layout);
void from_json(const nlohmann::json &j, SkyboxLayout &layout);

void to_json(nlohmann::json &j, const SkyboxFormatScaling &formatScaling);
void from_json(const nlohmann::json &j, SkyboxFormatScaling &formatScaling);
} // namespace star_skybox