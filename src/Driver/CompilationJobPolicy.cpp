#include "nativesharp/Driver/CompilationJobPolicy.h"

#include <algorithm>
#include <thread>

namespace nativesharp::driver
{
namespace
{
thread_local const CompilationJobPolicy* activePolicy = nullptr;

std::size_t normalizedHardwareThreads(std::size_t value) noexcept
{
    return value == 0 ? 1 : value;
}
}

CompilationJobPolicy::CompilationJobPolicy() noexcept
    : CompilationJobPolicy(std::thread::hardware_concurrency())
{
}

CompilationJobPolicy::CompilationJobPolicy(std::size_t hardwareThreads) noexcept
    : hardwareThreads_(normalizedHardwareThreads(hardwareThreads))
{
}

void CompilationJobPolicy::setRequestedJobs(std::size_t value) noexcept
{
    requestedJobs_ = value;
}

void CompilationJobPolicy::setReservedCores(std::size_t value) noexcept
{
    reservedCores_ = value;
}

std::size_t CompilationJobPolicy::hardwareThreads() const noexcept
{
    return hardwareThreads_;
}

std::size_t CompilationJobPolicy::requestedJobs() const noexcept
{
    return requestedJobs_;
}

bool CompilationJobPolicy::usesAutomaticJobs() const noexcept
{
    return requestedJobs_ == 0;
}

std::size_t CompilationJobPolicy::reservedCores() const noexcept
{
    return reservedCores_;
}

std::size_t CompilationJobPolicy::effectiveJobs() const noexcept
{
    const std::size_t available =
        hardwareThreads_ > reservedCores_ ? hardwareThreads_ - reservedCores_ : 1;
    return requestedJobs_ == 0 ? available : std::min(requestedJobs_, available);
}

CompilationJobPolicyScope::CompilationJobPolicyScope(
    const CompilationJobPolicy& policy) noexcept
    : previous_(activePolicy)
{
    activePolicy = &policy;
}

CompilationJobPolicyScope::~CompilationJobPolicyScope()
{
    activePolicy = previous_;
}

const CompilationJobPolicy* activeCompilationJobPolicy() noexcept
{
    return activePolicy;
}
}
