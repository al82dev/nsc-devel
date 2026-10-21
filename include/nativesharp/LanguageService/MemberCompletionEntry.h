#pragma once

#include "nativesharp/LanguageService/MemberCompletionEntryKind.h"
#include "nativesharp/LanguageService/MemberCompletionOverload.h"

#include <optional>
#include <string>
#include <vector>

namespace nativesharp::language_service
{
struct MemberCompletionEntry final
{
    std::string insertText;
    std::string name;
    MemberCompletionEntryKind kind = MemberCompletionEntryKind::Field;
    bool isStatic = false;
    std::string declaringType;
    std::string resultType;
    std::optional<std::string> documentation;
    std::vector<MemberCompletionOverload> overloads;
};
}
