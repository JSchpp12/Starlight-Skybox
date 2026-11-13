#include "star_skybox/io/SkyboxLibraryReader.hpp"

#include <starlight/core/logging/LoggingFactory.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace star::skybox::io
{
namespace
{
constexpr std::string_view kSidecarSuffix = ".skyboxinfo.json";
}
/// @brief Sidecar `name` when readable, otherwise the source stem.
static std::string ReadDisplayName(const std::filesystem::path &sidecar, const std::string &fallback)
{
    try
    {
        std::ifstream stream(sidecar);
        if (!stream)
            return fallback;

        const nlohmann::json json = nlohmann::json::parse(stream);
        const auto it = json.find("name");
        if (it != json.end() && it->is_string())
        {
            const auto name = it->get<std::string>();
            if (!name.empty())
                return name;
        }
    }
    catch (const std::exception &e)
    {
        star::core::logging::warning("Failed to read skybox sidecar '", sidecar.string(), "': ", e.what());
    }

    return fallback;
}

std::vector<SkyboxLibraryEntry> SkyboxLibraryReader::scan(const std::filesystem::path &directory) const
{
    std::vector<SkyboxLibraryEntry> entries;

    std::error_code iterationError;
    std::filesystem::directory_iterator iterator(directory, iterationError);
    const std::filesystem::directory_iterator end;

    if (iterationError)
        throw std::runtime_error("Failed to open skybox directory '" + directory.string() +
                                 "': " + iterationError.message());

    for (; iterator != end; iterator.increment(iterationError))
    {
        if (iterationError)
            throw std::runtime_error("Failed while enumerating skybox directory '" + directory.string() +
                                     "': " + iterationError.message());

        std::error_code typeError;
        if (!iterator->is_regular_file(typeError) || typeError)
            continue;

        const std::string filename = iterator->path().filename().string();
        if (!filename.ends_with(kSidecarSuffix))
            continue;

        const std::string stem = filename.substr(0, filename.size() - kSidecarSuffix.size());
        if (stem.empty())
            continue;

        // MakeSkybox derives the sidecar by replacing the image extension, so the image is <stem>.exr and the
        // sidecar is <stem>.skyboxinfo.json. EXR is the only format SkyboxReader decodes.
        const std::filesystem::path texturePath = directory / (stem + ".exr");
        std::error_code imageError;
        if (!std::filesystem::is_regular_file(texturePath, imageError) || imageError)
            continue;

        entries.push_back(SkyboxLibraryEntry{
            .displayName = ReadDisplayName(iterator->path(), stem),
            .texturePath = texturePath,
        });
    }

    if (iterationError)
        throw std::runtime_error("Failed while enumerating skybox directory '" + directory.string() +
                                 "': " + iterationError.message());

    std::sort(entries.begin(), entries.end(), [](const auto &lhs, const auto &rhs) {
        if (lhs.displayName != rhs.displayName)
            return lhs.displayName < rhs.displayName;
        return lhs.texturePath.filename().string() < rhs.texturePath.filename().string();
    });

    return entries;
}

int SkyboxLibraryReader::operator()(const std::filesystem::path &directory)
{
    SkyboxLibraryResult result = std::vector<SkyboxLibraryEntry>{};

    try
    {
        result = scan(directory);
        star::core::logging::info("Skybox library scan found ", result->size(), " entr(ies) in ", directory.string());
    }
    catch (const std::exception &e)
    {
        result = std::unexpected(std::string{"Skybox library scan failed: "} + e.what());
    }
    catch (...)
    {
        result = std::unexpected(std::string{"Skybox library scan failed"});
    }

    // The worker policy does not catch exceptions, so always satisfy the promise instead of letting one escape.
    m_result.set_value(std::move(result));
    return 0;
}
} // namespace star::skybox::io
