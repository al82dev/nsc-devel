#pragma once

#include <string>
#include <vector>

namespace nativesharp::language_service
{
struct MemberCompletionOverload final
{
    std::string signature;
    std::string resultType;
    std::vector<std::string> parameters;
    std::vector<std::string> genericParameters;
    std::vector<std::string> constraints;
    std::vector<std::string> modifiers;
};
}
