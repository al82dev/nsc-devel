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
bool validVariadicType(const abi_plan::Type& type, unsigned depth) noexcept
{
    using abi_plan::TypeKind;
    if (depth > 32) return false;
    switch (type.kind)
    {
        case TypeKind::Void:
        case TypeKind::Float:
        case TypeKind::Double:
        case TypeKind::Pointer:
            return type.widthOrCount == 0 && !type.packed && type.elements.empty();
        case TypeKind::Integer:
            return type.widthOrCount > 0 && type.widthOrCount <= 128 && !type.packed && type.elements.empty();
        case TypeKind::Structure:
            if (type.widthOrCount != 0 || type.elements.size() > 1024) return false;
            break;
        case TypeKind::Array:
            if (type.packed || type.widthOrCount > (1U << 24) || type.elements.size() != 1) return false;
            break;
        case TypeKind::Vector:
            if (type.packed || type.widthOrCount != 2 || type.elements.size() != 1 ||
                type.elements[0].kind != TypeKind::Float) return false;
            break;
        default: return false;
    }
    for (const auto& element : type.elements)
        if (element.kind == TypeKind::Void || !validVariadicType(element, depth + 1)) return false;
    return true;
}

bool equalVariadicType(const abi_plan::Type& a, const abi_plan::Type& b) noexcept
{
    if (a.kind != b.kind || a.widthOrCount != b.widthOrCount || a.packed != b.packed ||
        a.elements.size() != b.elements.size()) return false;
    for (std::size_t i = 0; i < a.elements.size(); ++i)
        if (!equalVariadicType(a.elements[i], b.elements[i])) return false;
    return true;
}

bool validVariadicAlignment(std::uint64_t value) noexcept
{
    return value != 0 && value <= (1ULL << 29) && (value & (value - 1)) == 0;
}

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

bool validateCppVariadicSysVCallPlan(const abi_plan::VariadicCallPlan& plan,
                                     std::string_view expectedSymbol,
                                     std::size_t expectedRequiredArguments) noexcept
{
    using abi_plan::Passing;
    using abi_plan::TypeKind;
    if (expectedSymbol.empty() || plan.symbol != expectedSymbol || plan.callingConvention != 78 ||
        !plan.targetTriple.starts_with("x86_64-pc-windows-msvc") || !plan.dataLayout.starts_with("e-") ||
        plan.requiredArguments != expectedRequiredArguments || plan.requiredArguments > plan.arguments.size() ||
        plan.arguments.size() > 1024 ||
        plan.physicalParameters.size() > 2049 || !validVariadicType(plan.physicalReturn, 0) ||
        (plan.returnSignExtend && plan.returnZeroExtend)) return false;
    const auto& result = plan.result;
    if (result.offset != 0) return false;
    std::size_t next = 0;
    if (result.passing == Passing::Indirect)
    {
        if (plan.physicalReturn.kind != TypeKind::Void || plan.returnSignExtend || plan.returnZeroExtend ||
            result.coercion || result.signExtend || result.zeroExtend ||
            !validVariadicAlignment(result.alignment) || plan.physicalParameters.empty()) return false;
        const auto& p = plan.physicalParameters[0];
        if (!p.structureReturn || p.byValue || p.inRegister || p.signExtend || p.zeroExtend ||
            p.type.kind != TypeKind::Pointer || !validVariadicType(p.type, 0) || !p.pointee ||
            p.pointee->kind == TypeKind::Void || !validVariadicType(*p.pointee, 0) ||
            p.alignment != result.alignment) return false;
        next = 1;
    }
    else if (result.passing == Passing::Ignore)
    {
        if (plan.physicalReturn.kind != TypeKind::Void || result.coercion || result.byValue ||
            result.signExtend || result.zeroExtend ||
            plan.returnSignExtend || plan.returnZeroExtend) return false;
    }
    else if (result.passing == Passing::Direct || result.passing == Passing::Extend)
    {
        if (!result.coercion || result.byValue || plan.physicalReturn.kind == TypeKind::Void ||
            !validVariadicType(*result.coercion, 0) ||
            !equalVariadicType(*result.coercion, plan.physicalReturn)) return false;
        if (result.passing == Passing::Extend)
        {
            if (result.signExtend == result.zeroExtend || result.signExtend != plan.returnSignExtend ||
                result.zeroExtend != plan.returnZeroExtend || plan.physicalReturn.kind != TypeKind::Integer) return false;
        }
        else if (plan.returnSignExtend || plan.returnZeroExtend || result.signExtend || result.zeroExtend) return false;
    }
    else return false;
    for (std::size_t i = 0; i < plan.arguments.size(); ++i)
    {
        const auto& argument = plan.arguments[i];
        if (argument.firstPhysical != next || argument.offset != 0 ||
            argument.physicalCount > plan.physicalParameters.size() - next) return false;
        if (argument.passing == Passing::Ignore)
        {
            if (argument.physicalCount != 0 || argument.coercion || argument.byValue ||
                argument.signExtend || argument.zeroExtend) return false;
            continue;
        }
        if (argument.passing == Passing::Indirect)
        {
            if (argument.physicalCount != 1 || !argument.byValue || argument.coercion || argument.flatten ||
                argument.signExtend || argument.zeroExtend || !validVariadicAlignment(argument.alignment)) return false;
            const auto& p = plan.physicalParameters[next++];
            if (!p.byValue || p.structureReturn || p.inRegister || p.noAlias || p.signExtend || p.zeroExtend ||
                p.type.kind != TypeKind::Pointer || !validVariadicType(p.type, 0) || !p.pointee ||
                p.pointee->kind == TypeKind::Void || !validVariadicType(*p.pointee, 0) ||
                p.alignment != argument.alignment) return false;
            continue;
        }
        if ((argument.passing != Passing::Direct && argument.passing != Passing::Extend) ||
            !argument.coercion || argument.coercion->kind == TypeKind::Void ||
            !validVariadicType(*argument.coercion, 0) || argument.byValue ||
            (argument.alignment != 0 && !validVariadicAlignment(argument.alignment))) return false;
        const auto& coercion = *argument.coercion;
        const bool flattened = argument.passing == Passing::Direct && argument.flatten && coercion.kind == TypeKind::Structure;
        const auto count = flattened ? coercion.elements.size() : 1U;
        if (argument.physicalCount != count) return false;
        if (argument.passing == Passing::Extend && (argument.signExtend == argument.zeroExtend ||
            coercion.kind != TypeKind::Integer)) return false;
        if (argument.passing == Passing::Direct && (argument.signExtend || argument.zeroExtend)) return false;
        for (std::size_t part = 0; part < count; ++part)
        {
            const auto& p = plan.physicalParameters[next++];
            const auto& expected = flattened ? coercion.elements[part] : coercion;
            if (!validVariadicType(p.type, 0) || !equalVariadicType(p.type, expected) || p.structureReturn ||
                p.byValue || p.inRegister || p.pointee || (p.alignment != 0 &&
                (p.type.kind != TypeKind::Pointer || !validVariadicAlignment(p.alignment))) ||
                (p.noAlias && (i >= plan.requiredArguments ||
                p.type.kind != TypeKind::Pointer)) || p.signExtend != argument.signExtend ||
                p.zeroExtend != argument.zeroExtend) return false;
        }
    }
    return next == plan.physicalParameters.size();
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
    auto plan = functionSession.cVariadicCallAbiPlan(name, canonicalType, types);
    if (!plan) return std::nullopt;
    const auto functions = functionSession.freeFunctionsSnapshot();
    if (!functions) return std::nullopt;
    const CppFunctionDeclaration* match = nullptr;
    for (const auto& function : *functions)
        if (function.name == name && function.canonicalFunctionType == canonicalType && function.variadic &&
            function.externalSymbol && *function.externalSymbol == plan->symbol)
        {
            if (match) return std::nullopt;
            match = &function;
        }
    if (!match || !validateCppVariadicSysVCallPlan(*plan, *match->externalSymbol, match->parameters.size()))
        return std::nullopt;
    return plan;
}
}
