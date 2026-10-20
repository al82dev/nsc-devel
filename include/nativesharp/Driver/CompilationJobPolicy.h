#pragma once

#include <cstddef>

namespace nativesharp::driver
{
inline constexpr std::size_t kDefaultReservedCompilationCores = 1;
inline constexpr std::size_t kMaximumCompilationJobs = 1024;

class CompilationJobPolicy final
{
public:
    CompilationJobPolicy() noexcept;
    explicit CompilationJobPolicy(std::size_t hardwareThreads) noexcept;

    void setRequestedJobs(std::size_t value) noexcept;
    void setReservedCores(std::size_t value) noexcept;

    std::size_t hardwareThreads() const noexcept;
    std::size_t requestedJobs() const noexcept;
    bool usesAutomaticJobs() const noexcept;
    std::size_t reservedCores() const noexcept;
    std::size_t effectiveJobs() const noexcept;

private:
    std::size_t hardwareThreads_;
    std::size_t requestedJobs_ = 0;
    std::size_t reservedCores_ = kDefaultReservedCompilationCores;
};

const CompilationJobPolicy* activeCompilationJobPolicy() noexcept;
}

#include "nativesharp/Driver/CompilationJobPolicyScope.h"
