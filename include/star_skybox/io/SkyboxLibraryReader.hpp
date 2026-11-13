#pragma once

#include <expected>
#include <filesystem>
#include <future>
#include <string>
#include <vector>

namespace star::skybox::io
{
/// @brief One selectable skybox discovered in the media tree.
struct SkyboxLibraryEntry
{
    /// @brief Human-readable label: the sidecar's `name`, or the source file stem.
    std::string displayName;
    /// @brief Resolved absolute path of the source image, ready for MakeSkybox::setTexture.
    std::filesystem::path texturePath;
};

/// @brief Result of a library scan: the discovered entries or a human-readable failure reason.
using SkyboxLibraryResult = std::expected<std::vector<SkyboxLibraryEntry>, std::string>;

/// @brief Scans a resolved directory for `*.skyboxinfo.json` sidecars and pairs each with its sibling `.exr`.
/// @details Mirrors SkyboxInfoReader: the IO payload owns the candidate path and resolver and invokes this functor
/// with the resolved path. Call getFuture() before moving the reader into the task. `operator()` never throws: the
/// worker policy does not wrap task execution in a try/catch, so the promise is always satisfied.
class SkyboxLibraryReader
{
  public:
    /// @brief Scans `directory` (already resolved by the payload) and stores the result.
    /// @return 0 always; failures travel through the future.
    int operator()(const std::filesystem::path &directory);

    /// @brief Future resolving once the scan task runs. Call before moving the reader.
    [[nodiscard]] std::future<SkyboxLibraryResult> getFuture()
    {
        return m_result.get_future();
    }

  private:
    /// @brief Enumerates the directory. Throws on filesystem errors; operator() publishes the failure.
    [[nodiscard]] std::vector<SkyboxLibraryEntry> scan(const std::filesystem::path &directory) const;

    std::promise<SkyboxLibraryResult> m_result;
};
} // namespace star::skybox::io
