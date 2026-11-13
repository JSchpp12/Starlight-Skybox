#pragma once

#include "star_skybox/struct/SkyboxInfo.hpp"

#include <expected>
#include <future>
#include <string>
#include <utility>

namespace star::core::device
{
class DeviceContext;
}

namespace star::skybox::io
{
/// @brief Handle to an in-flight skybox sidecar read.
/// @details Owns the read's future and the request that produces it. request() queues the read on the IO worker;
/// callers can poll isReady() without blocking the render thread and consume the description with get() once ready.
/// get() returns an unexpected (rather than throwing) when the sidecar is missing, malformed, or the result was
/// already consumed.
class SkyboxReadResult
{
  public:
    /// @brief Queues the sidecar read for @p texture on the IO worker and returns its handle.
    [[nodiscard]] static SkyboxReadResult request(star::core::device::DeviceContext &deviceContext,
                                                  const std::string &texture);

    SkyboxReadResult(SkyboxReadResult &&) = default;
    SkyboxReadResult &operator=(SkyboxReadResult &&) = default;
    SkyboxReadResult(const SkyboxReadResult &) = delete;
    SkyboxReadResult &operator=(const SkyboxReadResult &) = delete;
    ~SkyboxReadResult() = default;

    /// @brief True once the IO worker has resolved the read (success or failure).
    [[nodiscard]] bool isReady() const;

    /// @brief Consumes the resolved description.
    /// @details Invalidates this result. Call only after isReady() to avoid blocking the calling thread. Returns an
    /// unexpected error message when the read failed or this result was already consumed.
    [[nodiscard]] std::expected<SkyboxInfo, std::string> get();

  private:
    explicit SkyboxReadResult(std::future<SkyboxInfo> future) : m_future(std::move(future))
    {
    }

    std::future<SkyboxInfo> m_future;
};
} // namespace star::skybox::io
