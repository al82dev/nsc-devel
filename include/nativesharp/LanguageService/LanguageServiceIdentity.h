#pragma once

#include <cstdint>
#include <string_view>

namespace nativesharp::language_service
{
struct LanguageServiceIdentity final
{
    std::string_view compilerRelease;
    std::string_view compilerStage;
    std::string_view target;
    std::string_view buildId;
    std::uint32_t analysisAbi = 0;
};
}
