#pragma once

#include <cstdint>

namespace nativesharp::llvm_backend
{
struct ColdLlvmPreambleTelemetry final
{
    std::uint64_t reachabilityMicros = 0;
    std::uint64_t partitionBuildMicros = 0;
    std::uint64_t coordinatorClosureMicros = 0;
    std::uint64_t identityIndexBuildMicros = 0;
    std::uint64_t lineTableIdentityMicros = 0;
    std::uint64_t moduleKeyComposeMicros = 0;
    std::uint64_t staticFieldMaskMicros = 0;
    std::uint64_t workerWallMicros = 0;
};
}
