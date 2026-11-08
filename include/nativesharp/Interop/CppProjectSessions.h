#pragma once

#include "nativesharp/Compilation/CppLibraryReference.h"
#include "nativesharp/Interop/CppInteropSessionFactory.h"

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace nativesharp::compilation { class Project; class ProjectGraph; }

namespace nativesharp::interop
{
struct CppProjectSessionEnvironment
{
    std::string targetTriple;
    std::filesystem::path driverReferencePath;
    std::filesystem::path resourceDirectory;
    std::filesystem::path workingDirectory;
    std::vector<std::filesystem::path> systemIncludePaths;
    bool capturePreprocessingFingerprint = false;
};

struct CppProjectSessions
{
    // Populated by the project overload; the span overload has no project identity.
    std::filesystem::path projectFile;
    // Same order as the supplied references; each entry exclusively owns its session.
    std::vector<std::unique_ptr<CppInteropSession>> sessions;
};

struct CppProjectSessionFailure
{
    std::size_t referenceIndex = 0;
    CppSessionOpenFailure failure;
};

using CppProjectSessionsResult = std::variant<CppProjectSessions, CppProjectSessionFailure>;

CppProjectSessionsResult openCppProjectSessions(
    std::span<const compilation::CppLibraryReference> references,
    const CppProjectSessionEnvironment& environment,
    CppInteropSessionFactory& factory,
    CppCancellationToken cancellation);

CppProjectSessionsResult openCppProjectSessions(
    const compilation::Project& project,
    const CppProjectSessionEnvironment& environment,
    CppInteropSessionFactory& factory,
    CppCancellationToken cancellation);

struct CppGraphSessions
{
    // In graph order; projects without C/C++ references have no group.
    std::vector<CppProjectSessions> projects;
};

struct CppGraphSessionFailure
{
    std::filesystem::path projectFile;
    CppProjectSessionFailure projectFailure;
};

using CppGraphSessionsResult = std::variant<CppGraphSessions, CppGraphSessionFailure>;

CppGraphSessionsResult openCppGraphSessions(
    const compilation::ProjectGraph& graph,
    const CppProjectSessionEnvironment& environment,
    CppInteropSessionFactory& factory,
    CppCancellationToken cancellation);
}
