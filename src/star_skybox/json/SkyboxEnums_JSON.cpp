#include "star_skybox/json/SkyboxEnums_JSON.hpp"

#include <stdexcept>
#include <string>

namespace starlight::skybox
{
void to_json(nlohmann::json &j, const SkyboxLayout &layout)
{
    switch (layout)
    {
    case SkyboxLayout::Equirect:
        j = "equirect";
        break;
    }
}

void from_json(const nlohmann::json &j, SkyboxLayout &layout)
{
    const auto &value = j.get_ref<const std::string &>();
    if (value == "equirect")
        layout = SkyboxLayout::Equirect;
    else
        throw std::invalid_argument("Unknown SkyboxLayout value: " + value);
}

void to_json(nlohmann::json &j, const SkyboxFormatScaling &formatScaling)
{
    switch (formatScaling)
    {
    case SkyboxFormatScaling::Full:
        j = "full";
        break;
    case SkyboxFormatScaling::Half:
        j = "half";
        break;
    }
}

void from_json(const nlohmann::json &j, SkyboxFormatScaling &formatScaling)
{
    const auto &value = j.get_ref<const std::string &>();
    if (value == "full")
        formatScaling = SkyboxFormatScaling::Full;
    else if (value == "half")
        formatScaling = SkyboxFormatScaling::Half;
    else
        throw std::invalid_argument("Unknown SkyboxFormatScaling value: " + value);
}
} // namespace starlight::skybox