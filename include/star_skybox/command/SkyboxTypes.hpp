#pragma once

#include "star_skybox/struct/SkyboxInfo.hpp"

#include <star_common/Handle.hpp>

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace star::skybox::command
{
/// @brief Error raised by the skybox file service commands for scan/read failures.
class SkyServiceException : public std::runtime_error
{
  public:
    explicit SkyServiceException(std::string message) : std::runtime_error(std::move(message)) {}
};

/// @brief One library entry surfaced to callers/GUI.
/// @details The description is returned by value, so callers never hold a pointer into service storage.
struct SkyboxRegistration
{
    star::Handle handle{};
    std::string displayName;
    std::filesystem::path texturePath;
    std::optional<SkyboxInfo> info;
    std::string error;
};

struct GetAllSkyboxRegistrationsReply
{
    std::vector<SkyboxRegistration> registrations;
    /// Library-scan error, if the scan failed.
    std::optional<std::string> scanError;
};
} // namespace star::skybox::command
