#pragma once

#include "star_skybox/command/SkyboxTypes.hpp"

#include <star_common/IServiceCommandWithReply.hpp>

#include <string_view>

namespace star::skybox::command
{
namespace get_all_skybox_registrations
{
inline constexpr const char *GetTypeName()
{
    return "skGAR";
}
} // namespace get_all_skybox_registrations

/// @brief Resolve every library entry and return the registrations.
/// @details Blocks until the library scan and every sidecar read are complete. Read failures are recorded per entry;
/// the scan error is reported separately so the synthetic configured entry remains usable.
struct GetAllSkyboxRegistrations : public star::common::IServiceCommandWithReply<GetAllSkyboxRegistrationsReply>
{
    static constexpr std::string_view GetUniqueTypeName()
    {
        return get_all_skybox_registrations::GetTypeName();
    }

    GetAllSkyboxRegistrations() = default;
    explicit GetAllSkyboxRegistrations(uint16_t type)
        : star::common::IServiceCommandWithReply<GetAllSkyboxRegistrationsReply>(type)
    {
    }
};
} // namespace star::skybox::command
