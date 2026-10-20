#pragma once

#include "nativesharp/LLVM/CacheMissReason.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace nativesharp::llvm_backend
{
// NSC305-P02. Per-code-unit timing and cache-lookup outcome. `lookup` includes the restore
// (byte copy) performed by `CompilationArtifactCache::restoreObject`; the remaining phases map
// one-to-one to the object generator steps. Populated for every code unit of a generate() call.
struct CodeUnitTelemetry final
{
    std::string name;
    std::size_t definitionCount = 0;
    std::size_t declarationCount = 0;
    std::size_t staticFieldCount = 0;
    std::uint64_t staticFieldMaskMicros = 0;
    std::uint64_t fingerprintMicros = 0;
    std::uint64_t lookupMicros = 0;
    std::uint64_t loweringMicros = 0;
    std::uint64_t targetMicros = 0;
    std::uint64_t optimizationMicros = 0;
    std::uint64_t emissionMicros = 0;
    std::uint64_t verificationMicros = 0;
    std::uint64_t storeMicros = 0;
    CacheMissReason missReason = CacheMissReason::Hit;
};
}
