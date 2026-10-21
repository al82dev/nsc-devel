#pragma once

#include "nativesharp/LanguageService/MemberCompletionTriggerKind.h"
#include "nativesharp/LanguageService/TextPosition.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace nativesharp::language_service
{
struct MemberCompletionRequest final
{
    std::filesystem::path projectPath;
    std::filesystem::path documentPath;
    std::int64_t documentVersion = 0;
    TextPosition caretPosition;
    TextPosition operatorPosition;
    std::string prefix;
    MemberCompletionTriggerKind trigger = MemberCompletionTriggerKind::Dot;
    std::function<bool()> cancellationCheck;
};
}
