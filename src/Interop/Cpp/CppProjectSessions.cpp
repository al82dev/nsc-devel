#include "nativesharp/Interop/CppProjectSessions.h"
#include "nativesharp/Interop/CppVariadicAbiReconciliation.h"
#include "nativesharp/Compilation/Project.h"
#include "nativesharp/Compilation/ProjectGraph.h"

#include <clang/Basic/LangStandard.h>

#include <utility>

namespace nativesharp::interop
{
CppProjectSessionsResult openCppProjectSessions(
    std::span<const compilation::CppLibraryReference> references,
    const CppProjectSessionEnvironment& environment,
    CppInteropSessionFactory& factory,
    CppCancellationToken cancellation)
{
    if (!references.empty() && cancellation.cancelled())
        return CppProjectSessionFailure{0, {"cancel", "C/C++ session opening was cancelled", {}}};
    for (std::size_t index = 0; index < references.size(); ++index)
    {
        const auto& reference = references[index];
        if ((reference.language != CppLanguageMode::C && reference.language != CppLanguageMode::Cpp) ||
            reference.languageStandard.empty())
            return CppProjectSessionFailure{index, {"options", "invalid C/C++ language or empty standard", {}}};
        if (reference.volatileMode != CppVolatileMode::Default &&
            reference.volatileMode != CppVolatileMode::Microsoft &&
            reference.volatileMode != CppVolatileMode::Iso)
            return CppProjectSessionFailure{index, {"options", "invalid C/C++ volatile mode", {}}};
        const auto* standard = clang::LangStandard::getLangStandardForName(reference.languageStandard);
        if (!standard ||
            (reference.language == CppLanguageMode::C && standard->getLanguage() != clang::Language::C) ||
            (reference.language == CppLanguageMode::Cpp && standard->getLanguage() != clang::Language::CXX))
            return CppProjectSessionFailure{index, {"options", "C/C++ standard is unknown or does not match the language", {}}};
        if (reference.headerPath.empty())
            return CppProjectSessionFailure{index, {"input", "input file path is empty", {}}};
        for (const auto& include : reference.includePaths)
            if (include.path.empty() || (include.kind != CppIncludeKind::User && include.kind != CppIncludeKind::System))
                return CppProjectSessionFailure{index, {"options", "invalid C/C++ include path", {}}};
        for (const auto& macro : reference.macros)
            if (macro.text.empty() || (macro.action != CppMacroAction::Define && macro.action != CppMacroAction::Undefine))
                return CppProjectSessionFailure{index, {"options", "invalid C/C++ macro option", {}}};
    }
    CppProjectSessions result;
    result.sessions.reserve(references.size());
    for (std::size_t index = 0; index < references.size(); ++index)
    {
        const auto& reference = references[index];
        CppSessionOptions options;
        options.language = reference.language;
        options.languageStandard = reference.languageStandard;
        options.volatileMode = reference.volatileMode;
        options.targetTriple = environment.targetTriple;
        options.driverReferencePath = environment.driverReferencePath;
        options.resourceDirectory = environment.resourceDirectory;
        options.workingDirectory = environment.workingDirectory;
        options.includePaths = reference.includePaths;
        for (const auto& path : environment.systemIncludePaths)
            options.includePaths.push_back({path, CppIncludeKind::System});
        options.macros = reference.macros;
        options.capturePreprocessingFingerprint = environment.capturePreprocessingFingerprint;
        options.input = CppFileInput{reference.headerPath};

        auto opened = factory.open(std::move(options), cancellation);
        if (auto* failure = std::get_if<CppSessionOpenFailure>(&opened))
            return CppProjectSessionFailure{index, std::move(*failure)};
        auto session = std::move(std::get<std::unique_ptr<CppInteropSession>>(opened));
        if (!session || session->state() != CppSessionState::Ready)
            return CppProjectSessionFailure{index, {"factory", "factory returned a session that is not ready", {}}};
        result.sessions.push_back(std::move(session));
    }
    return result;
}

CppProjectSessionsResult openCppProjectSessions(
    const compilation::Project& project,
    const CppProjectSessionEnvironment& environment,
    CppInteropSessionFactory& factory,
    CppCancellationToken cancellation)
{
    auto result = openCppProjectSessions(project.cppLibraryReferences(), environment, factory, cancellation);
    if (auto* sessions = std::get_if<CppProjectSessions>(&result))
        sessions->projectFile = project.projectFile();
    return result;
}

CppGraphSessionsResult openCppGraphSessions(
    const compilation::ProjectGraph& graph,
    const CppProjectSessionEnvironment& environment,
    CppInteropSessionFactory& factory,
    CppCancellationToken cancellation)
{
    CppGraphSessions result;
    for (const auto& project : graph.projects())
    {
        if (project.cppLibraryReferences().empty()) continue;
        auto opened = openCppProjectSessions(project, environment, factory, cancellation);
        if (auto* failure = std::get_if<CppProjectSessionFailure>(&opened))
            return CppGraphSessionFailure{project.projectFile(), std::move(*failure)};
        result.projects.push_back(std::move(std::get<CppProjectSessions>(opened)));
    }
    return result;
}

namespace
{
bool variadicRecordOriginMatches(const CppInteropSession& target, const CppInteropSession* origin,
                                 const CppType& type, unsigned depth)
{
    if (depth > 16) return false;
    if (type.kind == CppTypeKind::Pointer)
        return type.element && variadicRecordOriginMatches(target, origin, *type.element, depth + 1);
    if (type.kind != CppTypeKind::Record) return true;
    if (!origin || origin->state() != CppSessionState::Ready || !type.recordIdentity ||
        !type.recordIdentity->clangUsr || !type.sizeBits || !type.alignmentBits ||
        type.recordIdentity->name.empty() || type.recordIdentity->anonymousContext ||
        type.recordIdentity->specializationContext || !type.recordIdentity->namespaceSegments.empty() ||
        !type.recordIdentity->containingRecords.empty()) return false;
    auto records = origin->recordsSnapshot();
    if (!records) return false;
    unsigned matches = 0;
    for (const auto& record : *records)
        if (!record.isCxx && record.complete && record.clangUsr == type.recordIdentity->clangUsr &&
            record.qualifiedName == type.recordIdentity->name && record.sizeBits == type.sizeBits &&
            record.alignmentBits == type.alignmentBits) ++matches;
    if (matches != 1) return false;
    if (origin == &target) return true;
    const auto equivalent = target.structurallyEquivalentRecord(*origin, type.recordIdentity->name);
    return equivalent && *equivalent;
}
}

std::optional<abi_plan::VariadicCallPlan> reconcileCppVariadicCallAbiPlan(
    const CppInteropSession& functionSession, std::string_view name,
    std::string_view canonicalType, std::span<const CppVariadicArgumentOrigin> arguments)
{
    if (functionSession.state() != CppSessionState::Ready) return std::nullopt;
    std::vector<CppType> types;
    types.reserve(arguments.size());
    for (const auto& argument : arguments)
    {
        if (!variadicRecordOriginMatches(functionSession, argument.origin, argument.type, 0)) return std::nullopt;
        types.push_back(argument.type);
    }
    return functionSession.cVariadicCallAbiPlan(name, canonicalType, types);
}
}
