#pragma once

#include "nativesharp/LanguageService/MemberCompletionEntry.h"
#include "nativesharp/LanguageService/MemberCompletionStatus.h"

#include <cstdint>
#include <vector>

namespace nativesharp::language_service
{
struct MemberCompletionResult final
{
    MemberCompletionStatus status = MemberCompletionStatus::Unavailable;
    std::vector<MemberCompletionEntry> entries;
    std::uint64_t analysisNanoseconds = 0;
    std::uint64_t resolutionNanoseconds = 0;

    bool isIncomplete() const noexcept;
};
}
