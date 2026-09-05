#include "nativesharp/Basic/Version.h"

#include "nativesharp/Basic/BuildId.h"

namespace nativesharp
{
const std::string_view kCompilerVersion = detail::kGeneratedCompilerVersion;
const std::string_view kCompilerBuildId = detail::kGeneratedCompilerBuildId;
}
