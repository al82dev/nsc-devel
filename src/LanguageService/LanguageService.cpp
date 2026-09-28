#include "nativesharp/LanguageService/LanguageService.h"

#include "nativesharp/Basic/Version.h"
#include "nativesharp/Target/TargetInfo.h"

#include <utility>

namespace nativesharp::language_service
{
class LanguageService::Implementation final
{
};

LanguageService::LanguageService()
    : implementation_(std::make_unique<Implementation>())
{
}

LanguageService::LanguageService(LanguageService&&) noexcept = default;
LanguageService& LanguageService::operator=(LanguageService&&) noexcept = default;
LanguageService::~LanguageService() = default;

LanguageServiceIdentity LanguageService::identity() const noexcept
{
    return LanguageServiceIdentity{
        kCompilerReleaseVersion,
        kLanguageVersion,
        kCompilerStage,
        target::TargetInfo::WindowsX64().llvmTriple(),
        kCompilerBuildId,
        kAnalysisAbiVersion};
}
}
