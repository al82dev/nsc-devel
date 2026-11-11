#include "nativesharp/Interop/CppProjectSessions.h"
#include "nativesharp/Interop/CppVariadicAbiReconciliation.h"
#include "nativesharp/Interop/CppCRecordAbiReplay.h"
#include "nativesharp/Interop/CppVariadicAbiCodec.h"
#include "nativesharp/Interop/CppSemanticFunction.h"
#include "nativesharp/Frontend/PrecompiledSyntaxModule.h"
#include "../Internal/CppCRecordMetadataCodec.h"
#include "nativesharp/Compilation/Project.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Frontend/NamespaceFunctionSymbol.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <clang/Basic/LangStandard.h>

#include <algorithm>
#include <functional>
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
std::optional<CppType> simpleVariadicType(std::string_view key, unsigned depth = 0)
{
    if (depth > 16) return std::nullopt;
    if (key.ends_with('*'))
    {
        auto element = simpleVariadicType(key.substr(0, key.size() - 1), depth + 1);
        if (!element) return std::nullopt;
        CppType pointer;
        pointer.kind = CppTypeKind::Pointer;
        pointer.canonicalSpelling = element->canonicalSpelling + " *";
        pointer.sizeBits = pointer.alignmentBits = 64;
        pointer.element = std::make_shared<const CppType>(std::move(*element));
        return pointer;
    }
    CppType type;
    if (key == "builtin:void") { type.kind = CppTypeKind::Void; type.canonicalSpelling = "void"; }
    else if (key == "builtin:sbyte") { type.kind = CppTypeKind::SignedInteger; type.canonicalSpelling = "signed char"; type.sizeBits = type.alignmentBits = 8; }
    else if (key == "builtin:byte") { type.kind = CppTypeKind::UnsignedInteger; type.canonicalSpelling = "unsigned char"; type.sizeBits = type.alignmentBits = 8; }
    else if (key == "builtin:short") { type.kind = CppTypeKind::SignedInteger; type.canonicalSpelling = "short"; type.sizeBits = type.alignmentBits = 16; }
    else if (key == "builtin:ushort") { type.kind = CppTypeKind::UnsignedInteger; type.canonicalSpelling = "unsigned short"; type.sizeBits = type.alignmentBits = 16; }
    else if (key == "builtin:int") { type.kind = CppTypeKind::SignedInteger; type.canonicalSpelling = "int"; type.sizeBits = type.alignmentBits = 32; }
    else if (key == "builtin:uint") { type.kind = CppTypeKind::UnsignedInteger; type.canonicalSpelling = "unsigned int"; type.sizeBits = type.alignmentBits = 32; }
    else if (key == "builtin:long") { type.kind = CppTypeKind::SignedInteger; type.canonicalSpelling = "long long"; type.sizeBits = type.alignmentBits = 64; }
    else if (key == "builtin:ulong") { type.kind = CppTypeKind::UnsignedInteger; type.canonicalSpelling = "unsigned long long"; type.sizeBits = type.alignmentBits = 64; }
    else if (key == "builtin:float") { type.kind = CppTypeKind::Floating; type.canonicalSpelling = "float"; type.sizeBits = type.alignmentBits = 32; }
    else if (key == "builtin:double") { type.kind = CppTypeKind::Floating; type.canonicalSpelling = "double"; type.sizeBits = type.alignmentBits = 64; }
    else return std::nullopt;
    if (depth == 0 && (type.kind == CppTypeKind::Void || type.canonicalSpelling == "float" ||
        (type.sizeBits && *type.sizeBits < 32))) return std::nullopt;
    return type;
}

bool sameVariadicType(const CppType& left, const CppType& right, unsigned depth = 0)
{
    if (depth > 16 || left.kind != right.kind || left.canonicalSpelling != right.canonicalSpelling ||
        left.sizeBits != right.sizeBits || left.alignmentBits != right.alignmentBits ||
        left.isConst != right.isConst || left.isVolatile != right.isVolatile || left.isRestrict != right.isRestrict)
        return false;
    if (left.kind == CppTypeKind::Pointer)
        return left.element && right.element && sameVariadicType(*left.element, *right.element, depth + 1);
    if (left.kind == CppTypeKind::Record)
        return depth > 0 && left.recordIdentity && right.recordIdentity &&
            left.recordIdentity->clangUsr && !left.recordIdentity->clangUsr->empty() &&
            left.recordIdentity->clangUsr == right.recordIdentity->clangUsr &&
            left.recordIdentity->name == right.recordIdentity->name &&
            left.recordIdentity->namespaceSegments == right.recordIdentity->namespaceSegments &&
            left.recordIdentity->containingRecords == right.recordIdentity->containingRecords &&
            left.recordIdentity->anonymousContext == right.recordIdentity->anonymousContext &&
            left.recordIdentity->specializationContext == right.recordIdentity->specializationContext;
    return left.kind == CppTypeKind::Void || left.kind == CppTypeKind::Boolean ||
        left.kind == CppTypeKind::SignedInteger || left.kind == CppTypeKind::UnsignedInteger ||
        left.kind == CppTypeKind::Floating;
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

std::optional<abi_plan::Plan> reconcileCppFunctionPointerAbiPlan(
    const CppInteropSession& aliasSession, std::string_view name, const CppType& expectedPointer,
    const CppVariadicArgumentOrigin& result, std::span<const CppVariadicArgumentOrigin> parameters)
{
    if (aliasSession.state() != CppSessionState::Ready || name.empty() ||
        expectedPointer.kind != CppTypeKind::Pointer || expectedPointer.canonicalSpelling.empty() ||
        expectedPointer.isConst || expectedPointer.isVolatile || expectedPointer.isRestrict ||
        !expectedPointer.element || expectedPointer.element->kind != CppTypeKind::Function ||
        !expectedPointer.element->function) return std::nullopt;
    const auto aliases = aliasSession.typeAliasesSnapshot();
    if (!aliases) return std::nullopt;
    const CppType* observed = nullptr;
    for (const auto& alias : *aliases)
        if (alias.name == name && alias.qualifiedName == name &&
            alias.underlyingType.canonicalSpelling == expectedPointer.canonicalSpelling)
        {
            if (observed) return std::nullopt;
            observed = &alias.underlyingType;
        }
    if (!observed || observed->kind != CppTypeKind::Pointer || !observed->element ||
        observed->element->kind != CppTypeKind::Function || !observed->element->function ||
        observed->sizeBits != expectedPointer.sizeBits || observed->alignmentBits != expectedPointer.alignmentBits ||
        observed->isConst != expectedPointer.isConst || observed->isVolatile != expectedPointer.isVolatile ||
        observed->isRestrict != expectedPointer.isRestrict ||
        observed->element->canonicalSpelling != expectedPointer.element->canonicalSpelling ||
        observed->element->isConst != expectedPointer.element->isConst ||
        observed->element->isVolatile != expectedPointer.element->isVolatile ||
        observed->element->isRestrict != expectedPointer.element->isRestrict) return std::nullopt;
    const auto& signature = *observed->element->function;
    const auto& expectedSignature = *expectedPointer.element->function;
    if (signature.callingConvention != "sysv_abi" || !signature.hasPrototype || signature.variadic ||
        expectedSignature.callingConvention != signature.callingConvention ||
        expectedSignature.hasPrototype != signature.hasPrototype || expectedSignature.variadic != signature.variadic ||
        signature.parameterTypes.size() != parameters.size() ||
        expectedSignature.parameterTypes.size() != parameters.size() ||
        !sameVariadicType(signature.returnType, expectedSignature.returnType, 1) ||
        !sameVariadicType(signature.returnType, result.type, 1) ||
        !variadicRecordOriginMatches(aliasSession, result.origin, result.type, 0)) return std::nullopt;
    for (std::size_t index = 0; index < parameters.size(); ++index)
        if (!sameVariadicType(signature.parameterTypes[index], expectedSignature.parameterTypes[index], 1) ||
            !sameVariadicType(signature.parameterTypes[index], parameters[index].type, 1) ||
            !variadicRecordOriginMatches(aliasSession, parameters[index].origin, parameters[index].type, 0))
            return std::nullopt;
    auto plan = aliasSession.cFunctionPointerAbiPlan(name, expectedPointer.canonicalSpelling);
    if (!plan || plan->symbol != "__nsc_pointer_abi_probe" || plan->callingConvention != 78 || plan->variadic ||
        !plan->targetTriple.starts_with("x86_64-pc-windows-msvc") || !plan->dataLayout.starts_with("e-") ||
        plan->logicalParameters.size() != parameters.size()) return std::nullopt;
    return plan;
}

std::shared_ptr<const abi_plan::Plan> planCppFunctionPointerForAlias(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup, const frontend::CppImportedTypeAlias& alias,
    std::string_view resolvedTypeKey)
{
    if (alias.fromPrecompiledCatalog || alias.projectOrdinal >= graph.projects().size() ||
        alias.projectFile != graph.projects()[alias.projectOrdinal].projectFile() ||
        alias.targetTriple != "x86_64-pc-windows-msvc" || !alias.declaration.clangUsr ||
        alias.declaration.clangUsr->empty() || alias.declaration.name.empty() ||
        alias.declaration.qualifiedName != alias.declaration.name) return {};
    const auto mapped = projectCppStoredFunctionPointer(alias.declaration.underlyingType, alias.projectOrdinal,
        graph.projects()[alias.projectOrdinal].moduleName(), &lookup);
    if (!mapped || mapped->nativeSharpTypeKey != resolvedTypeKey) return {};
    const CppInteropSession* aliasSession = nullptr;
    for (const auto& group : projects)
    {
        if (group.projectFile != alias.projectFile) continue;
        if (aliasSession || alias.referenceIndex >= group.sessions.size() || !group.sessions[alias.referenceIndex]) return {};
        aliasSession = group.sessions[alias.referenceIndex].get();
    }
    if (!aliasSession || aliasSession->state() != CppSessionState::Ready ||
        aliasSession->optionsSnapshot().language != CppLanguageMode::C ||
        aliasSession->optionsSnapshot().targetTriple != alias.targetTriple) return {};
    const auto aliases = aliasSession->typeAliasesSnapshot();
    if (!aliases) return {};
    unsigned aliasMatches = 0;
    for (const auto& observed : *aliases)
        if (observed.name == alias.declaration.name && observed.qualifiedName == alias.declaration.qualifiedName &&
            observed.clangUsr == alias.declaration.clangUsr &&
            observed.underlyingType.canonicalSpelling == alias.declaration.underlyingType.canonicalSpelling)
            ++aliasMatches;
    if (aliasMatches != 1) return {};
    const auto& pointer = alias.declaration.underlyingType;
    if (!pointer.element || !pointer.element->function) return {};
    const auto& signature = *pointer.element->function;
    const auto originFor = [&](const CppType& type) -> std::optional<CppVariadicArgumentOrigin> {
        const CppType* record = &type;
        unsigned depth = 0;
        while (record->kind == CppTypeKind::Pointer)
        {
            if (++depth > 16 || !record->element) return std::nullopt;
            record = record->element.get();
        }
        if (record->kind != CppTypeKind::Record) return CppVariadicArgumentOrigin{type, nullptr};
        const auto identity = projectCppRecordTypeIdentity(*record, graph.projects()[alias.projectOrdinal].moduleName());
        const auto ordinal = identity ? lookup.typeOrdinalForIdentity(identity->key()) : std::nullopt;
        const auto* imported = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
        if (!imported || imported->fromPrecompiledCatalog || imported->isIncomplete ||
            !imported->deferredImportError.empty() || !imported->clangUsr || !imported->objectLayout ||
            imported->projectOrdinal >= graph.projects().size() || !record->sizeBits || !record->alignmentBits ||
            imported->objectLayout->sizeBytes > UINT64_MAX / 8 || imported->objectLayout->alignmentBytes > UINT64_MAX / 8 ||
            *record->sizeBits != imported->objectLayout->sizeBytes * 8 ||
            *record->alignmentBits != imported->objectLayout->alignmentBytes * 8 || !record->recordIdentity ||
            record->recordIdentity->clangUsr != imported->clangUsr ||
            (imported->origin != frontend::CppImportedTypeOrigin::CStruct &&
             imported->origin != frontend::CppImportedTypeOrigin::CUnion)) return std::nullopt;
        const auto& originFile = graph.projects()[imported->projectOrdinal].projectFile();
        const CppInteropSession* origin = nullptr;
        for (const auto& group : projects)
        {
            if (group.projectFile != originFile) continue;
            for (const auto& candidate : group.sessions)
            {
                if (!candidate || candidate->state() != CppSessionState::Ready) return std::nullopt;
                if (candidate->optionsSnapshot().language != CppLanguageMode::C) continue;
                const auto records = candidate->recordsSnapshot();
                if (!records) return std::nullopt;
                for (const auto& item : *records)
                {
                    if (!item.complete || item.clangUsr != imported->clangUsr || item.name != imported->identity.name()) continue;
                    const auto kind = imported->origin == frontend::CppImportedTypeOrigin::CStruct ? CppRecordKind::Struct : CppRecordKind::Union;
                    if (item.isCxx || item.kind != kind || item.sizeBits != record->sizeBits ||
                        item.alignmentBits != record->alignmentBits) return std::nullopt;
                    if (origin && (origin == candidate.get() ||
                        origin->structurallyEquivalentRecord(*candidate, item.qualifiedName) != true)) return std::nullopt;
                    origin = candidate.get();
                }
            }
        }
        if (!origin) return std::nullopt;
        return CppVariadicArgumentOrigin{type, origin};
    };
    const auto result = originFor(signature.returnType);
    if (!result) return {};
    std::vector<CppVariadicArgumentOrigin> parameters;
    for (const auto& type : signature.parameterTypes)
    {
        auto origin = originFor(type);
        if (!origin) return {};
        parameters.push_back(std::move(*origin));
    }
    auto plan = reconcileCppFunctionPointerAbiPlan(*aliasSession, alias.declaration.name, pointer, *result, parameters);
    return plan ? std::make_shared<const abi_plan::Plan>(std::move(*plan)) : nullptr;
}

std::shared_ptr<const abi_plan::Plan> planPrecompiledCppFunctionPointerForAlias(
    const compilation::ProjectGraph& graph, std::span<const frontend::PrecompiledSyntaxModule> modules,
    const frontend::ProjectNameLookup& lookup, const frontend::CppImportedTypeAlias& alias,
    std::string_view resolvedTypeKey, const CppProjectSessionEnvironment& environment,
    CppInteropSessionFactory& factory)
{
    if (!alias.fromPrecompiledCatalog || alias.projectOrdinal >= graph.projects().size() ||
        alias.targetTriple != "x86_64-pc-windows-msvc" || environment.targetTriple != alias.targetTriple ||
        alias.projectFile != graph.projects()[alias.projectOrdinal].projectFile() ||
        !alias.declaration.clangUsr || alias.declaration.clangUsr->empty()) return {};
    const auto& moduleName = graph.projects()[alias.projectOrdinal].moduleName();
    const auto mapped = projectCppStoredFunctionPointer(alias.declaration.underlyingType, alias.projectOrdinal,
        moduleName, &lookup);
    if (!mapped || mapped->nativeSharpTypeKey != resolvedTypeKey) return {};
    const auto& pointer = alias.declaration.underlyingType;
    if (pointer.kind != CppTypeKind::Pointer || pointer.isConst || pointer.isVolatile || pointer.isRestrict ||
        pointer.sizeBits != 64 || pointer.alignmentBits != 64 || !pointer.element ||
        pointer.element->kind != CppTypeKind::Function || !pointer.element->function) return {};
    const auto& signature = *pointer.element->function;
    const bool returnInteger = signature.returnType.kind == CppTypeKind::SignedInteger &&
        signature.returnType.canonicalSpelling == "int";
    const bool returnRecord = signature.returnType.kind == CppTypeKind::Record;
    if (!signature.hasPrototype || signature.variadic || signature.callingConvention != "sysv_abi" ||
        (!returnInteger && !returnRecord) ||
        (signature.parameterTypes.empty() && !returnRecord) || signature.parameterTypes.size() > 1024) return {};
    struct ReplayRecord
    {
        const CppType* type;
        std::vector<std::uint8_t> encoded;
        std::string identity;
        std::optional<CppCRecordSnapshot> snapshot;
    };
    std::vector<ReplayRecord> selectedRecords;
    std::string parameterSource;
    std::string error;
    for (std::size_t argumentIndex = 0;
         argumentIndex < signature.parameterTypes.size() + (returnRecord ? 1U : 0U); ++argumentIndex)
    {
        const bool isReturn = argumentIndex == signature.parameterTypes.size();
        const auto& argument = isReturn ? signature.returnType : signature.parameterTypes[argumentIndex];
        if (!isReturn && !parameterSource.empty()) parameterSource += ", ";
        if (argument.kind == CppTypeKind::Record)
        {
            const auto& type = argument;
            if (type.isConst || type.isVolatile || type.isRestrict || !type.recordIdentity ||
                !type.recordIdentity->clangUsr || type.recordIdentity->clangUsr->empty() ||
                type.recordIdentity->anonymousContext || type.recordIdentity->specializationContext ||
                !type.recordIdentity->namespaceSegments.empty() || !type.recordIdentity->containingRecords.empty() ||
                type.canonicalSpelling != "struct " + type.recordIdentity->name ||
                !((type.sizeBits == 96 && type.alignmentBits == 32) ||
                  (type.sizeBits == 160 && type.alignmentBits == 32) ||
                  (type.sizeBits == 128 && (type.alignmentBits == 32 || type.alignmentBits == 64)))) return {};
            const auto identity = projectCppRecordTypeIdentity(type, moduleName);
            if (!identity) return {};
            const auto ordinal = lookup.typeOrdinalForIdentity(identity->key());
            const auto* imported = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
            if (!imported || !imported->fromPrecompiledCatalog || imported->isIncomplete ||
                !imported->deferredImportError.empty() || !imported->objectLayout ||
                imported->clangUsr != type.recordIdentity->clangUsr ||
                imported->objectLayout->sizeBytes != *type.sizeBits / 8 ||
                imported->objectLayout->alignmentBytes != *type.alignmentBits / 8 ||
                imported->origin != frontend::CppImportedTypeOrigin::CStruct) return {};
            std::vector<std::uint8_t> encoded;
            if (!metadata_detail::encodeCDependencyType(type, encoded, error)) return {};
            auto found = std::find_if(selectedRecords.begin(), selectedRecords.end(), [&](const auto& value) {
                return value.identity == identity->key();
            });
            if (found == selectedRecords.end())
            {
                selectedRecords.push_back({&type, std::move(encoded), identity->key(), std::nullopt});
                found = selectedRecords.end() - 1;
            }
            else if (found->encoded != encoded) return {};
            const auto index = static_cast<std::size_t>(found - selectedRecords.begin());
            if (!isReturn)
                parameterSource += index == 0 ? "struct __nsc_abi_record" : "struct __nsc_abi_record_" + std::to_string(index);
        }
        else
        {
            if (argument.kind != CppTypeKind::SignedInteger || argument.canonicalSpelling != "int" ||
                argument.isConst || argument.isVolatile || argument.isRestrict) return {};
            parameterSource += "int";
        }
    }
    if (selectedRecords.empty()) return {};
    if (signature.parameterTypes.empty()) parameterSource = "void";
    std::string returnTypeSource = "int";
    if (returnRecord)
    {
        std::vector<std::uint8_t> encodedReturn;
        if ((signature.returnType.sizeBits != 96 && signature.returnType.sizeBits != 160) ||
            signature.returnType.alignmentBits != 32 ||
            !metadata_detail::encodeCDependencyType(signature.returnType, encodedReturn, error)) return {};
        const auto match = std::find_if(selectedRecords.begin(), selectedRecords.end(),
            [&](const auto& record) { return record.encoded == encodedReturn; });
        if (match == selectedRecords.end()) return {};
        const auto index = static_cast<std::size_t>(match - selectedRecords.begin());
        returnTypeSource = index == 0 ? "struct __nsc_abi_record" : "struct __nsc_abi_record_" + std::to_string(index);
    }
    std::vector<std::uint8_t> aliasType;
    if (!metadata_detail::encodeCDependencyType(pointer, aliasType, error)) return {};
    std::optional<metadata_detail::CDependencyAliasSnapshot> selectedAlias;
    for (const auto& module : modules)
    {
        if (module.cppCTypeCatalog.empty()) continue;
        std::vector<CppCRecordSnapshot> records;
        std::vector<CppCEnumSnapshot> enums;
        metadata_detail::CDependencyCatalog dependencies;
        if (!metadata_detail::decodeCTypeCatalogAndDependencies(module.cppCTypeCatalog,
                module.cppCTypeCatalogFormatMinor, records, enums, dependencies, error)) return {};
        for (const auto& candidate : dependencies.aliases)
        {
            const auto& declaration = candidate.declaration;
            if (candidate.moduleName != moduleName || declaration.name != alias.declaration.name ||
                declaration.qualifiedName != alias.declaration.qualifiedName ||
                declaration.clangUsr != alias.declaration.clangUsr) continue;
            std::vector<std::uint8_t> encoded;
            if (selectedAlias || candidate.referenceIdentity.empty() ||
                !metadata_detail::encodeCDependencyType(declaration.underlyingType, encoded, error) ||
                encoded != aliasType) return {};
            selectedAlias = candidate;
        }
        for (const auto& record : records)
            for (auto& selected : selectedRecords)
                if (record.identityKey == selected.identity)
                {
                    if (selected.snapshot) return {};
                    selected.snapshot = record;
                }
    }
    if (!selectedAlias) return {};
    std::string recordSource;
    for (std::size_t index = 0; index < selectedRecords.size(); ++index)
    {
        const auto& selected = selectedRecords[index];
        const auto& type = *selected.type;
        if (!selected.snapshot) return {};
        const auto& record = *selected.snapshot;
        if (!record.plainReplaySyntax || record.moduleName != moduleName || !record.namespaceSegments.empty() ||
            !record.containingRecords.empty() || record.clangUsr != type.recordIdentity->clangUsr ||
            record.name != type.recordIdentity->name || record.kind != CppRecordKind::Struct ||
            record.sizeBits != type.sizeBits || record.alignmentBits != type.alignmentBits) return {};
        if (type.alignmentBits == 64)
        {
            if (record.fields.size() != 2) return {};
            const auto& first = record.fields[0];
            const auto& second = record.fields[1];
            if (first.bitWidth || first.type.kind != CppTypeKind::Floating || first.type.canonicalSpelling != "double" ||
                first.offsetBits != 0 || second.bitWidth || second.type.kind != CppTypeKind::SignedInteger ||
                second.type.canonicalSpelling != "int" || second.offsetBits != 64) return {};
        }
        else
        {
            if (record.fields.size() != *type.sizeBits / 32) return {};
            for (std::size_t i = 0; i < record.fields.size(); ++i)
            {
                const auto& field = record.fields[i];
                if (field.bitWidth || field.type.kind != CppTypeKind::SignedInteger ||
                    field.type.canonicalSpelling != "int" || field.offsetBits != i * 32) return {};
            }
        }
        const auto declaration = makeSimpleCRecordAbiReplay(record, index);
        if (!declaration) return {};
        recordSource += *declaration;
    }
    CppSessionOptions options;
    options.language = CppLanguageMode::C;
    options.languageStandard = "c11";
    options.targetTriple = environment.targetTriple;
    options.driverReferencePath = environment.driverReferencePath;
    options.resourceDirectory = environment.resourceDirectory;
    options.workingDirectory = environment.workingDirectory;
    options.input = CppBufferInput{"nsc-pointer-replay.c", recordSource +
        "typedef " + returnTypeSource +
        " (__attribute__((sysv_abi)) *__nsc_replay_pointer)(" + parameterSource + ");\n"};
    auto opened = factory.open(std::move(options), {});
    auto* holder = std::get_if<std::unique_ptr<CppInteropSession>>(&opened);
    if (!holder || !*holder) return {};
    auto& session = **holder;
    const auto records = session.recordsSnapshot();
    const auto aliases = session.typeAliasesSnapshot();
    if (!records || !aliases) return {};
    for (std::size_t index = 0; index < selectedRecords.size(); ++index)
    {
        const auto tag = index == 0 ? std::string("__nsc_abi_record") : "__nsc_abi_record_" + std::to_string(index);
        const auto replayRecord = std::find_if(records->begin(), records->end(),
            [&](const auto& value) { return value.name == tag; });
        if (replayRecord == records->end() ||
            !matchesSimpleCRecordAbiReplay(*selectedRecords[index].snapshot, *replayRecord, index)) return {};
    }
    const auto replayAlias = std::find_if(aliases->begin(), aliases->end(),
        [](const auto& value) { return value.name == "__nsc_replay_pointer"; });
    if (replayAlias == aliases->end()) return {};
    auto plan = session.cFunctionPointerAbiPlan(replayAlias->name, replayAlias->underlyingType.canonicalSpelling);
    if (!plan || plan->symbol != "__nsc_pointer_abi_probe" || plan->callingConvention != 78 ||
        plan->variadic || !plan->targetTriple.starts_with("x86_64-pc-windows-msvc") ||
        !plan->dataLayout.starts_with("e-") || plan->logicalParameters.size() != signature.parameterTypes.size()) return {};
    return std::make_shared<const abi_plan::Plan>(std::move(*plan));
}

std::shared_ptr<const abi_plan::VariadicCallPlan> planPrecompiledCppVariadicFunctionPointerForAlias(
    const compilation::ProjectGraph& graph, std::span<const frontend::PrecompiledSyntaxModule> modules,
    const frontend::ProjectNameLookup& lookup, const frontend::CppImportedTypeAlias& alias,
    std::string_view resolvedTypeKey, std::span<const std::string> optionalTypeKeys,
    const CppProjectSessionEnvironment& environment, CppInteropSessionFactory& factory)
{
    if (!alias.fromPrecompiledCatalog || alias.projectOrdinal >= graph.projects().size() ||
        alias.targetTriple != "x86_64-pc-windows-msvc" || environment.targetTriple != alias.targetTriple ||
        alias.projectFile != graph.projects()[alias.projectOrdinal].projectFile() ||
        !alias.declaration.clangUsr || alias.declaration.clangUsr->empty() || optionalTypeKeys.empty() ||
        optionalTypeKeys.size() > 1024) return {};
    const auto& moduleName = graph.projects()[alias.projectOrdinal].moduleName();
    const auto mapped = projectCppStoredFunctionPointer(alias.declaration.underlyingType, alias.projectOrdinal,
        moduleName, &lookup);
    if (!mapped || mapped->nativeSharpTypeKey != resolvedTypeKey) return {};
    const auto& pointer = alias.declaration.underlyingType;
    if (pointer.kind != CppTypeKind::Pointer || pointer.isConst || pointer.isVolatile || pointer.isRestrict ||
        pointer.sizeBits != 64 || pointer.alignmentBits != 64 || !pointer.element ||
        pointer.element->kind != CppTypeKind::Function || !pointer.element->function) return {};
    const auto& signature = *pointer.element->function;
    const auto isInt = [](const CppType& type) {
        return type.kind == CppTypeKind::SignedInteger && type.canonicalSpelling == "int" &&
            !type.isConst && !type.isVolatile && !type.isRestrict;
    };
    const bool returnDouble = signature.returnType.kind == CppTypeKind::Floating &&
        signature.returnType.canonicalSpelling == "double" && !signature.returnType.isConst &&
        !signature.returnType.isVolatile && !signature.returnType.isRestrict;
    const bool returnVoid = signature.returnType.kind == CppTypeKind::Void &&
        signature.returnType.canonicalSpelling == "void" && !signature.returnType.isConst &&
        !signature.returnType.isVolatile && !signature.returnType.isRestrict;
    const bool returnPointer = signature.returnType.kind == CppTypeKind::Pointer &&
        signature.returnType.canonicalSpelling == "int *" && signature.returnType.sizeBits == 64 &&
        signature.returnType.alignmentBits == 64 && !signature.returnType.isConst &&
        !signature.returnType.isVolatile && !signature.returnType.isRestrict &&
        signature.returnType.element && isInt(*signature.returnType.element);
    if (!signature.hasPrototype || !signature.variadic || signature.callingConvention != "sysv_abi" ||
        (!isInt(signature.returnType) && !returnDouble && !returnVoid && !returnPointer) || signature.parameterTypes.empty() ||
        signature.parameterTypes.size() > 1024) return {};
    struct ReplayRecord
    {
        std::string identity;
        const frontend::CppImportedTypeDeclaration* imported;
        std::optional<CppCRecordSnapshot> snapshot;
    };
    std::vector<ReplayRecord> selectedRecords;
    std::vector<std::optional<std::size_t>> recordForArgument;
    for (const auto& key : optionalTypeKeys)
    {
        if (key == "builtin:int" || key == "builtin:double" || key == "builtin:int*" || key == "builtin:long" || key == "builtin:uint")
        {
            recordForArgument.push_back(std::nullopt);
            continue;
        }
        if (!key.starts_with("type:") || key.ends_with('*')) return {};
        const auto identity = std::string_view(key).substr(5);
        const auto ordinal = lookup.typeOrdinalForIdentity(identity);
        const auto* imported = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
        if (!imported || !imported->fromPrecompiledCatalog || imported->isIncomplete ||
            !imported->deferredImportError.empty() || !imported->objectLayout || !imported->clangUsr ||
            imported->origin != frontend::CppImportedTypeOrigin::CStruct ||
            !((imported->objectLayout->sizeBytes == 16 && imported->objectLayout->alignmentBytes == 8) ||
              (imported->objectLayout->sizeBytes == 20 && imported->objectLayout->alignmentBytes == 4))) return {};
        auto found = std::find_if(selectedRecords.begin(), selectedRecords.end(),
            [&](const auto& value) { return value.identity == identity; });
        if (found == selectedRecords.end())
        {
            selectedRecords.push_back({std::string(identity), imported, std::nullopt});
            recordForArgument.push_back(selectedRecords.size() - 1);
        }
        else recordForArgument.push_back(static_cast<std::size_t>(found - selectedRecords.begin()));
    }
    if (selectedRecords.empty()) return {};
    for (const auto& type : signature.parameterTypes)
    {
        if (type.kind != CppTypeKind::Record) continue;
        if (type.isConst || type.isVolatile || type.isRestrict ||
            !((type.sizeBits == 128 && type.alignmentBits == 64) || (type.sizeBits == 160 && type.alignmentBits == 32)) ||
            !type.recordIdentity || !type.recordIdentity->clangUsr || type.recordIdentity->clangUsr->empty() ||
            type.recordIdentity->anonymousContext || type.recordIdentity->specializationContext ||
            !type.recordIdentity->namespaceSegments.empty() || !type.recordIdentity->containingRecords.empty() ||
            type.canonicalSpelling != "struct " + type.recordIdentity->name) return {};
        const auto identity = projectCppRecordTypeIdentity(type, moduleName);
        if (!identity) return {};
        const auto found = std::find_if(selectedRecords.begin(), selectedRecords.end(),
            [&](const auto& value) { return value.identity == identity->key(); });
        if (found != selectedRecords.end()) continue;
        const auto ordinal = lookup.typeOrdinalForIdentity(identity->key());
        const auto* imported = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
        if (!imported || !imported->fromPrecompiledCatalog || imported->isIncomplete ||
            !imported->deferredImportError.empty() || !imported->objectLayout ||
            imported->origin != frontend::CppImportedTypeOrigin::CStruct ||
            imported->clangUsr != type.recordIdentity->clangUsr ||
            imported->objectLayout->sizeBytes != *type.sizeBits / 8 ||
            imported->objectLayout->alignmentBytes != *type.alignmentBits / 8) return {};
        selectedRecords.push_back({identity->key(), imported, std::nullopt});
    }
    std::string fixedSource;
    for (const auto& type : signature.parameterTypes)
    {
        if (!fixedSource.empty()) fixedSource += ", ";
        if (type.kind == CppTypeKind::Record)
        {
            if (type.isConst || type.isVolatile || type.isRestrict ||
                !((type.sizeBits == 128 && type.alignmentBits == 64) || (type.sizeBits == 160 && type.alignmentBits == 32)) ||
                !type.recordIdentity || !type.recordIdentity->clangUsr || type.recordIdentity->clangUsr->empty() ||
                type.recordIdentity->anonymousContext || type.recordIdentity->specializationContext ||
                !type.recordIdentity->namespaceSegments.empty() || !type.recordIdentity->containingRecords.empty() ||
                type.canonicalSpelling != "struct " + type.recordIdentity->name) return {};
            const auto identity = projectCppRecordTypeIdentity(type, moduleName);
            if (!identity) return {};
            const auto found = std::find_if(selectedRecords.begin(), selectedRecords.end(),
                [&](const auto& value) { return value.identity == identity->key(); });
            if (found == selectedRecords.end() || found->imported->clangUsr != type.recordIdentity->clangUsr ||
                found->imported->objectLayout->sizeBytes != *type.sizeBits / 8 ||
                found->imported->objectLayout->alignmentBytes != *type.alignmentBits / 8) return {};
            const auto index = static_cast<std::size_t>(found - selectedRecords.begin());
            fixedSource += index == 0 ? "struct __nsc_abi_record" : "struct __nsc_abi_record_" + std::to_string(index);
            continue;
        }
        const bool fixedDouble = type.kind == CppTypeKind::Floating && type.canonicalSpelling == "double" &&
            !type.isConst && !type.isVolatile && !type.isRestrict;
        const bool fixedLong = type.kind == CppTypeKind::SignedInteger && type.canonicalSpelling == "long long" &&
            type.sizeBits == 64 && type.alignmentBits == 64 && !type.isConst && !type.isVolatile && !type.isRestrict;
        const bool fixedUint = type.kind == CppTypeKind::UnsignedInteger && type.canonicalSpelling == "unsigned int" &&
            type.sizeBits == 32 && type.alignmentBits == 32 && !type.isConst && !type.isVolatile && !type.isRestrict;
        const bool fixedPointer = type.kind == CppTypeKind::Pointer && type.canonicalSpelling == "int *" &&
            type.sizeBits == 64 && type.alignmentBits == 64 && !type.isConst && !type.isVolatile &&
            !type.isRestrict && type.element && isInt(*type.element);
        if (!isInt(type) && !fixedDouble && !fixedPointer && !fixedLong && !fixedUint) return {};
        fixedSource += fixedDouble ? "double" : fixedPointer ? "int *" : fixedLong ? "long long" : fixedUint ? "unsigned int" : "int";
    }
    std::string error;
    std::vector<std::uint8_t> aliasType;
    if (!metadata_detail::encodeCDependencyType(pointer, aliasType, error)) return {};
    std::optional<metadata_detail::CDependencyAliasSnapshot> selectedAlias;
    for (const auto& module : modules)
    {
        if (module.cppCTypeCatalog.empty()) continue;
        std::vector<CppCRecordSnapshot> records;
        std::vector<CppCEnumSnapshot> enums;
        metadata_detail::CDependencyCatalog dependencies;
        if (!metadata_detail::decodeCTypeCatalogAndDependencies(module.cppCTypeCatalog,
                module.cppCTypeCatalogFormatMinor, records, enums, dependencies, error)) return {};
        for (const auto& candidate : dependencies.aliases)
        {
            const auto& declaration = candidate.declaration;
            if (candidate.moduleName != moduleName || declaration.name != alias.declaration.name ||
                declaration.qualifiedName != alias.declaration.qualifiedName ||
                declaration.clangUsr != alias.declaration.clangUsr) continue;
            std::vector<std::uint8_t> encoded;
            if (selectedAlias || candidate.referenceIdentity.empty() ||
                !metadata_detail::encodeCDependencyType(declaration.underlyingType, encoded, error) ||
                encoded != aliasType) return {};
            selectedAlias = candidate;
        }
        for (const auto& record : records)
            for (auto& selected : selectedRecords)
                if (record.identityKey == selected.identity)
                {
                    if (selected.snapshot) return {};
                    selected.snapshot = record;
                }
    }
    if (!selectedAlias) return {};
    std::string recordSource;
    for (std::size_t index = 0; index < selectedRecords.size(); ++index)
    {
        const auto& selected = selectedRecords[index];
        if (!selected.snapshot) return {};
        const auto& record = *selected.snapshot;
        const auto* imported = selected.imported;
        if (!record.plainReplaySyntax || record.moduleName != moduleName ||
            !record.namespaceSegments.empty() || !record.containingRecords.empty() ||
            record.clangUsr != imported->clangUsr || record.name != imported->identity.name() ||
            record.kind != CppRecordKind::Struct || !record.sizeBits || !record.alignmentBits ||
            *record.sizeBits != imported->objectLayout->sizeBytes * 8 ||
            *record.alignmentBits != imported->objectLayout->alignmentBytes * 8) return {};
        if (record.sizeBits == 128 && record.alignmentBits == 64 && record.fields.size() == 2)
        {
            const auto& first = record.fields[0];
            const auto& second = record.fields[1];
            if (first.bitWidth || first.type.kind != CppTypeKind::Floating || first.type.canonicalSpelling != "double" ||
                first.offsetBits != 0 || second.bitWidth || !isInt(second.type) || second.offsetBits != 64) return {};
        }
        else if (record.sizeBits == 160 && record.alignmentBits == 32 && record.fields.size() == 5)
        {
            for (std::size_t i = 0; i < record.fields.size(); ++i)
                if (record.fields[i].bitWidth || !isInt(record.fields[i].type) ||
                    record.fields[i].offsetBits != i * 32) return {};
        }
        else return {};
        const auto declaration = makeSimpleCRecordAbiReplay(record, index);
        if (!declaration) return {};
        recordSource += *declaration;
        const auto tag = index == 0 ? std::string("__nsc_abi_record") : "__nsc_abi_record_" + std::to_string(index);
        recordSource += "void __nsc_replay_type_" + std::to_string(index) + "(struct " + tag + ");\n";
    }
    CppSessionOptions options;
    options.language = CppLanguageMode::C;
    options.languageStandard = "c11";
    options.targetTriple = environment.targetTriple;
    options.driverReferencePath = environment.driverReferencePath;
    options.resourceDirectory = environment.resourceDirectory;
    options.workingDirectory = environment.workingDirectory;
    options.input = CppBufferInput{"nsc-variadic-pointer-replay.c", recordSource +
        "void __nsc_replay_int(int);\n"
        "void __nsc_replay_uint(unsigned int);\n"
        "void __nsc_replay_long(long long);\n"
        "void __nsc_replay_double(double);\n"
        "void __nsc_replay_int_pointer(int *);\n"
        "typedef " + std::string(returnDouble ? "double" : returnVoid ? "void" : returnPointer ? "int *" : "int") +
        " (__attribute__((sysv_abi)) *__nsc_replay_pointer)(" +
        fixedSource + ", ...);\n"};
    auto opened = factory.open(std::move(options), {});
    auto* holder = std::get_if<std::unique_ptr<CppInteropSession>>(&opened);
    if (!holder || !*holder) return {};
    auto& session = **holder;
    const auto records = session.recordsSnapshot();
    const auto functions = session.freeFunctionsSnapshot();
    const auto aliases = session.typeAliasesSnapshot();
    if (!records || !functions || !aliases) return {};
    const auto replayAlias = std::find_if(aliases->begin(), aliases->end(),
        [](const auto& value) { return value.name == "__nsc_replay_pointer"; });
    if (replayAlias == aliases->end()) return {};
    std::vector<CppType> replayTypes;
    for (std::size_t index = 0; index < selectedRecords.size(); ++index)
    {
        const auto tag = index == 0 ? std::string("__nsc_abi_record") : "__nsc_abi_record_" + std::to_string(index);
        const auto replayRecord = std::find_if(records->begin(), records->end(),
            [&](const auto& value) { return value.name == tag; });
        const auto helper = std::find_if(functions->begin(), functions->end(),
            [&](const auto& value) { return value.name == "__nsc_replay_type_" + std::to_string(index); });
        if (replayRecord == records->end() ||
            !matchesSimpleCRecordAbiReplay(*selectedRecords[index].snapshot, *replayRecord, index) ||
            helper == functions->end() || helper->parameters.size() != 1) return {};
        replayTypes.push_back(helper->parameters[0].type);
    }
    std::vector<CppType> arguments;
    if (!replayAlias->underlyingType.element || !replayAlias->underlyingType.element->function ||
        replayAlias->underlyingType.element->function->parameterTypes.size() != signature.parameterTypes.size()) return {};
    const auto doubleHelper = std::find_if(functions->begin(), functions->end(),
        [](const auto& value) { return value.name == "__nsc_replay_double"; });
    if (doubleHelper == functions->end() || doubleHelper->parameters.size() != 1 ||
        doubleHelper->parameters[0].type.kind != CppTypeKind::Floating ||
        doubleHelper->parameters[0].type.canonicalSpelling != "double") return {};
    const auto intHelper = std::find_if(functions->begin(), functions->end(),
        [](const auto& value) { return value.name == "__nsc_replay_int"; });
    if (intHelper == functions->end() || intHelper->parameters.size() != 1 ||
        !isInt(intHelper->parameters[0].type)) return {};
    const auto uintHelper = std::find_if(functions->begin(), functions->end(),
        [](const auto& value) { return value.name == "__nsc_replay_uint"; });
    if (uintHelper == functions->end() || uintHelper->parameters.size() != 1 ||
        uintHelper->parameters[0].type.kind != CppTypeKind::UnsignedInteger ||
        uintHelper->parameters[0].type.canonicalSpelling != "unsigned int") return {};
    const auto longHelper = std::find_if(functions->begin(), functions->end(),
        [](const auto& value) { return value.name == "__nsc_replay_long"; });
    if (longHelper == functions->end() || longHelper->parameters.size() != 1 ||
        longHelper->parameters[0].type.kind != CppTypeKind::SignedInteger ||
        longHelper->parameters[0].type.canonicalSpelling != "long long") return {};
    const auto pointerHelper = std::find_if(functions->begin(), functions->end(),
        [](const auto& value) { return value.name == "__nsc_replay_int_pointer"; });
    if (pointerHelper == functions->end() || pointerHelper->parameters.size() != 1 ||
        pointerHelper->parameters[0].type.kind != CppTypeKind::Pointer ||
        pointerHelper->parameters[0].type.canonicalSpelling != "int *") return {};
    for (std::size_t i = 0; i < recordForArgument.size(); ++i)
        arguments.push_back(recordForArgument[i] ? replayTypes[*recordForArgument[i]] :
            optionalTypeKeys[i] == "builtin:int" ? intHelper->parameters[0].type :
            optionalTypeKeys[i] == "builtin:uint" ? uintHelper->parameters[0].type :
            optionalTypeKeys[i] == "builtin:long" ? longHelper->parameters[0].type :
            optionalTypeKeys[i] == "builtin:double" ? doubleHelper->parameters[0].type : pointerHelper->parameters[0].type);
    auto plan = session.cVariadicFunctionPointerCallAbiPlan(replayAlias->name,
        replayAlias->underlyingType.canonicalSpelling, arguments);
    if (!plan || !validateCppVariadicSysVCallPlan(*plan, "__nsc_variadic_pointer_abi_probe", signature.parameterTypes.size())) return {};
    return std::make_shared<const abi_plan::VariadicCallPlan>(std::move(*plan));
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

std::optional<abi_plan::VariadicCallPlan> reconcileCppVariadicFunctionPointerCallAbiPlan(
    const CppInteropSession& aliasSession, std::string_view name, const CppType& expectedPointer,
    std::span<const CppVariadicArgumentOrigin> arguments)
{
    if (aliasSession.state() != CppSessionState::Ready ||
        aliasSession.optionsSnapshot().language != CppLanguageMode::C ||
        aliasSession.optionsSnapshot().targetTriple != "x86_64-pc-windows-msvc" || name.empty() ||
        expectedPointer.kind != CppTypeKind::Pointer || expectedPointer.canonicalSpelling.empty() ||
        expectedPointer.isConst || expectedPointer.isVolatile || expectedPointer.isRestrict ||
        !expectedPointer.element || expectedPointer.element->kind != CppTypeKind::Function ||
        !expectedPointer.element->function || arguments.size() > 1024) return std::nullopt;
    const auto aliases = aliasSession.typeAliasesSnapshot();
    if (!aliases) return std::nullopt;
    const CppType* observed = nullptr;
    for (const auto& alias : *aliases)
        if (alias.name == name && alias.qualifiedName == name &&
            alias.underlyingType.canonicalSpelling == expectedPointer.canonicalSpelling)
        {
            if (observed) return std::nullopt;
            observed = &alias.underlyingType;
        }
    if (!observed || observed->kind != CppTypeKind::Pointer || !observed->element ||
        observed->element->kind != CppTypeKind::Function || !observed->element->function ||
        observed->sizeBits != expectedPointer.sizeBits || observed->alignmentBits != expectedPointer.alignmentBits ||
        observed->isConst != expectedPointer.isConst || observed->isVolatile != expectedPointer.isVolatile ||
        observed->isRestrict != expectedPointer.isRestrict ||
        observed->element->canonicalSpelling != expectedPointer.element->canonicalSpelling ||
        observed->element->isConst != expectedPointer.element->isConst ||
        observed->element->isVolatile != expectedPointer.element->isVolatile ||
        observed->element->isRestrict != expectedPointer.element->isRestrict) return std::nullopt;
    const auto& signature = *observed->element->function;
    const auto& expectedSignature = *expectedPointer.element->function;
    if (signature.callingConvention != "sysv_abi" || !signature.hasPrototype || !signature.variadic ||
        expectedSignature.callingConvention != signature.callingConvention ||
        expectedSignature.hasPrototype != signature.hasPrototype || expectedSignature.variadic != signature.variadic ||
        expectedSignature.parameterTypes.size() != signature.parameterTypes.size() ||
        !sameVariadicType(signature.returnType, expectedSignature.returnType, 1) ||
        !variadicRecordOriginMatches(aliasSession, &aliasSession, signature.returnType, 0)) return std::nullopt;
    for (std::size_t i = 0; i < signature.parameterTypes.size(); ++i)
        if (!sameVariadicType(signature.parameterTypes[i], expectedSignature.parameterTypes[i], 1) ||
            !variadicRecordOriginMatches(aliasSession, &aliasSession, signature.parameterTypes[i], 0)) return std::nullopt;
    std::vector<CppType> types;
    types.reserve(arguments.size());
    for (const auto& argument : arguments)
    {
        if (!variadicRecordOriginMatches(aliasSession, argument.origin, argument.type, 0)) return std::nullopt;
        types.push_back(argument.type);
    }
    auto plan = aliasSession.cVariadicFunctionPointerCallAbiPlan(name, expectedPointer.canonicalSpelling, types);
    if (!plan || !validateCppVariadicSysVCallPlan(*plan, "__nsc_variadic_pointer_abi_probe",
                                                  signature.parameterTypes.size())) return std::nullopt;
    return plan;
}

namespace
{
std::optional<std::vector<CppVariadicArgumentOrigin>> resolveVariadicArgumentOrigins(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup, std::span<const std::string> optionalTypeKeys)
{
    std::vector<CppVariadicArgumentOrigin> arguments;
    arguments.reserve(optionalTypeKeys.size());
    for (const auto& key : optionalTypeKeys)
    {
        CppType type;
        if (auto simple = simpleVariadicType(key)) type = std::move(*simple);
        else if (key.starts_with("type:"))
        {
            const bool pointer = key.ends_with('*');
            const auto identity = std::string_view(key).substr(5, key.size() - 5 - (pointer ? 1 : 0));
            if (identity.empty() || identity.ends_with('*')) return {};
            const auto ordinal = lookup.typeOrdinalForIdentity(identity);
            const auto* imported = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
            if (!imported || imported->fromPrecompiledCatalog || imported->isIncomplete ||
                !imported->deferredImportError.empty() || !imported->clangUsr || !imported->objectLayout ||
                imported->projectOrdinal >= graph.projects().size() ||
                (imported->origin != frontend::CppImportedTypeOrigin::CStruct &&
                 imported->origin != frontend::CppImportedTypeOrigin::CUnion) ||
                !imported->identity.namespaceSegments().empty() || !imported->identity.containingTypes().empty() ||
                imported->objectLayout->sizeBytes > UINT64_MAX / 8 ||
                imported->objectLayout->alignmentBytes > UINT64_MAX / 8) return {};
            const auto& originFile = graph.projects()[imported->projectOrdinal].projectFile();
            const CppInteropSession* origin = nullptr;
            std::optional<CppRecordDeclaration> observed;
            for (const auto& group : projects)
            {
                if (group.projectFile != originFile) continue;
                for (const auto& candidate : group.sessions)
                {
                    if (!candidate || candidate->state() != CppSessionState::Ready) return {};
                    if (candidate->optionsSnapshot().language != CppLanguageMode::C) continue;
                    const auto records = candidate->recordsSnapshot();
                    if (!records) return {};
                    for (const auto& record : *records)
                    {
                        if (record.clangUsr != imported->clangUsr || record.name != imported->identity.name()) continue;
                        if (!record.complete) continue;
                        const auto expectedKind = imported->origin == frontend::CppImportedTypeOrigin::CStruct
                            ? CppRecordKind::Struct : CppRecordKind::Union;
                        if (record.isCxx || record.kind != expectedKind || record.anonymousContext ||
                            !record.namespaceSegments.empty() || !record.containingRecords.empty() ||
                            record.sizeBits != std::optional<std::uint64_t>(imported->objectLayout->sizeBytes * 8) ||
                            record.alignmentBits != std::optional<std::uint64_t>(imported->objectLayout->alignmentBytes * 8)) return {};
                        if (origin && origin != candidate.get())
                        {
                            const auto equivalent = origin->structurallyEquivalentRecord(*candidate, record.qualifiedName);
                            if (!equivalent || !*equivalent) return {};
                        }
                        else if (origin) return {};
                        origin = candidate.get();
                        observed = record;
                    }
                }
            }
            if (!origin || !observed) return {};
            type.kind = CppTypeKind::Record;
            type.canonicalSpelling = (observed->kind == CppRecordKind::Struct ? "struct " : "union ") + observed->qualifiedName;
            type.sizeBits = observed->sizeBits;
            type.alignmentBits = observed->alignmentBits;
            type.recordIdentity.emplace();
            type.recordIdentity->name = observed->name;
            type.recordIdentity->clangUsr = observed->clangUsr;
            if (pointer)
            {
                CppType wrapped;
                wrapped.kind = CppTypeKind::Pointer;
                wrapped.canonicalSpelling = type.canonicalSpelling + " *";
                wrapped.sizeBits = wrapped.alignmentBits = 64;
                wrapped.element = std::make_shared<const CppType>(std::move(type));
                type = std::move(wrapped);
            }
            arguments.push_back({std::move(type), origin});
            continue;
        }
        else return {};
        arguments.push_back({std::move(type), nullptr});
    }
    return arguments;
}
}

std::shared_ptr<const abi_plan::VariadicCallPlan> planCppVariadicCall(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup, const frontend::NamespaceFunctionSymbol& function,
    std::span<const std::string> optionalTypeKeys)
{
    if (!function.isVariadic || function.cppCallingConvention != "sysv_abi" ||
        !function.cppClangUsr || function.cppClangUsr->empty() || !function.cppReturnType ||
        function.cppParameterTypes.size() != function.parameters.size() || function.projectOrdinal >= graph.projects().size())
        return {};
    const auto arguments = resolveVariadicArgumentOrigins(graph, projects, lookup, optionalTypeKeys);
    if (!arguments) return {};
    const auto& projectFile = graph.projects()[function.projectOrdinal].projectFile();
    const CppInteropSession* selected = nullptr;
    std::string selectedName, selectedCanonical;
    for (const auto& group : projects)
    {
        if (group.projectFile != projectFile) continue;
        for (const auto& candidate : group.sessions)
        {
            if (!candidate || candidate->state() != CppSessionState::Ready ||
                candidate->optionsSnapshot().language != CppLanguageMode::C) continue;
            const auto declarations = candidate->freeFunctionsSnapshot();
            if (!declarations) continue;
            for (const auto& declaration : *declarations)
            {
                if (!declaration.variadic || declaration.callingConvention != "sysv_abi" ||
                    declaration.externalSymbol != std::optional<std::string>(function.mangledName) ||
                    declaration.clangUsr != function.cppClangUsr || declaration.parameters.size() != function.parameters.size() ||
                    !sameVariadicType(declaration.returnType, *function.cppReturnType))
                    continue;
                bool fixedMatch = true;
                for (std::size_t i = 0; i < declaration.parameters.size(); ++i)
                    if (!sameVariadicType(declaration.parameters[i].type, function.cppParameterTypes[i])) { fixedMatch = false; break; }
                if (!fixedMatch) continue;
                if (selected) return {}; // Do not guess between reference origins.
                selected = candidate.get();
                selectedName = declaration.name;
                selectedCanonical = declaration.canonicalFunctionType;
            }
        }
    }
    if (!selected) return {};
    auto plan = reconcileCppVariadicCallAbiPlan(*selected, selectedName, selectedCanonical, *arguments);
    if (!plan || plan->symbol != function.mangledName || plan->requiredArguments != function.parameters.size()) return {};
    return std::make_shared<const abi_plan::VariadicCallPlan>(std::move(*plan));
}

std::shared_ptr<const abi_plan::VariadicCallPlan> planCppVariadicFunctionPointerForAlias(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup, const frontend::CppImportedTypeAlias& alias,
    std::string_view resolvedTypeKey, std::span<const std::string> optionalTypeKeys)
{
    if (alias.fromPrecompiledCatalog || alias.projectOrdinal >= graph.projects().size() ||
        alias.projectFile != graph.projects()[alias.projectOrdinal].projectFile() ||
        alias.targetTriple != "x86_64-pc-windows-msvc" || !alias.declaration.clangUsr ||
        alias.declaration.clangUsr->empty() || alias.declaration.name.empty() ||
        alias.declaration.qualifiedName != alias.declaration.name) return {};
    const auto mapped = projectCppStoredFunctionPointer(alias.declaration.underlyingType, alias.projectOrdinal,
        graph.projects()[alias.projectOrdinal].moduleName(), &lookup);
    if (!mapped || mapped->nativeSharpTypeKey != resolvedTypeKey) return {};
    const CppInteropSession* aliasSession = nullptr;
    for (const auto& group : projects)
    {
        if (group.projectFile != alias.projectFile) continue;
        if (aliasSession || alias.referenceIndex >= group.sessions.size() || !group.sessions[alias.referenceIndex]) return {};
        aliasSession = group.sessions[alias.referenceIndex].get();
    }
    if (!aliasSession || aliasSession->state() != CppSessionState::Ready ||
        aliasSession->optionsSnapshot().language != CppLanguageMode::C ||
        aliasSession->optionsSnapshot().targetTriple != alias.targetTriple) return {};
    const auto aliases = aliasSession->typeAliasesSnapshot();
    if (!aliases) return {};
    unsigned aliasMatches = 0;
    for (const auto& observed : *aliases)
        if (observed.name == alias.declaration.name && observed.qualifiedName == alias.declaration.qualifiedName &&
            observed.clangUsr == alias.declaration.clangUsr &&
            observed.underlyingType.canonicalSpelling == alias.declaration.underlyingType.canonicalSpelling)
            ++aliasMatches;
    if (aliasMatches != 1) return {};
    const auto arguments = resolveVariadicArgumentOrigins(graph, projects, lookup, optionalTypeKeys);
    if (!arguments) return {};
    auto plan = reconcileCppVariadicFunctionPointerCallAbiPlan(
        *aliasSession, alias.declaration.name, alias.declaration.underlyingType, *arguments);
    return plan ? std::make_shared<const abi_plan::VariadicCallPlan>(std::move(*plan)) : nullptr;
}

std::shared_ptr<const abi_plan::VariadicCallPlan> planPrecompiledCppVariadicCall(
    const compilation::ProjectGraph& graph, std::span<const frontend::PrecompiledSyntaxModule> modules,
    const frontend::ProjectNameLookup& lookup, const frontend::NamespaceFunctionSymbol& function,
    std::span<const std::string> optionalTypeKeys, const CppProjectSessionEnvironment& environment,
    CppInteropSessionFactory& factory)
{
    if (!function.isVariadic || function.cppCallingConvention != "sysv_abi" || !function.cppClangUsr ||
        !function.cppReturnType || function.projectOrdinal >= graph.projects().size() ||
        graph.projects()[function.projectOrdinal].moduleName() != function.moduleName ||
        !graph.projects()[function.projectOrdinal].cppLibraryReferences().empty() ||
        function.cppParameterTypes.size() != function.parameters.size() ||
        environment.targetTriple != "x86_64-pc-windows-msvc" || optionalTypeKeys.size() > 1024) return {};
    bool erasedRecordPointer = false;
    const auto builtin = [&](const auto& self, const CppType& type, unsigned depth = 0) -> std::optional<std::string> {
        if (depth > 16) return std::nullopt;
        if (type.isConst || type.isVolatile || type.isRestrict) return std::nullopt;
        const auto& name = type.canonicalSpelling;
        if (type.kind == CppTypeKind::Pointer)
        {
            if (!type.element || type.sizeBits != 64 || type.alignmentBits != 64) return std::nullopt;
            if (type.element->kind == CppTypeKind::Record)
            {
                const auto& record = *type.element;
                if (depth != 0 || record.isConst || record.isVolatile || record.isRestrict ||
                    !record.recordIdentity || !record.recordIdentity->clangUsr ||
                    record.recordIdentity->clangUsr->empty() || record.recordIdentity->name.empty() ||
                    record.recordIdentity->anonymousContext || record.recordIdentity->specializationContext ||
                    !record.recordIdentity->namespaceSegments.empty() || !record.recordIdentity->containingRecords.empty() ||
                    name != record.canonicalSpelling + " *") return std::nullopt;
                erasedRecordPointer = true;
                return std::string("void *");
            }
            const auto element = self(self, *type.element, depth + 1);
            if (!element || name != *element + " *") return std::nullopt;
            return name;
        }
        if (type.kind == CppTypeKind::Void && name == "void") return name;
        if (type.kind == CppTypeKind::Boolean && name == "_Bool") return name;
        if (type.kind == CppTypeKind::SignedInteger &&
            (name == "signed char" || name == "short" || name == "int" || name == "long" || name == "long long")) return name;
        if (type.kind == CppTypeKind::UnsignedInteger &&
            (name == "unsigned char" || name == "unsigned short" || name == "unsigned int" ||
             name == "unsigned long" || name == "unsigned long long")) return name;
        if (type.kind == CppTypeKind::Floating && (name == "float" || name == "double")) return name;
        return std::nullopt;
    };
    std::optional<metadata_detail::CDependencyFunctionSnapshot> selected;
    std::vector<CppCRecordSnapshot> records;
    for (const auto& module : modules)
    {
        if (module.cppCTypeCatalog.empty()) continue;
        std::vector<CppCRecordSnapshot> found;
        std::vector<CppCEnumSnapshot> enums;
        metadata_detail::CDependencyCatalog dependencies;
        std::string error;
        if (!metadata_detail::decodeCTypeCatalogAndDependencies(module.cppCTypeCatalog,
                module.cppCTypeCatalogFormatMinor, found, enums, dependencies, error)) return {};
        for (const auto& candidate : dependencies.functions)
        {
            const auto& d = candidate.declaration;
            if (candidate.moduleName != function.moduleName || candidate.linkSymbol != function.mangledName ||
                d.name != function.name || d.clangUsr != function.cppClangUsr || !candidate.variadicBasePlan) continue;
            if (selected) return {};
            selected = candidate;
        }
        records.insert(records.end(), std::make_move_iterator(found.begin()), std::make_move_iterator(found.end()));
    }
    if (!selected || selected->declaration.name != selected->linkSymbol ||
        selected->declaration.parameters.size() != function.parameters.size() ||
        !sameVariadicType(selected->declaration.returnType, *function.cppReturnType)) return {};
    const auto& name = selected->declaration.name;
    const auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    if (name.empty() || !letter(name.front()) || !std::all_of(name.begin(), name.end(), [&](char c) {
            return letter(c) || (c >= '0' && c <= '9');
        })) return {};
    const auto resultType = builtin(builtin, selected->declaration.returnType);
    if (!resultType || selected->declaration.parameters.empty()) return {};
    std::string parameters;
    for (std::size_t i = 0; i < selected->declaration.parameters.size(); ++i)
    {
        const auto& type = selected->declaration.parameters[i].type;
        const auto spelling = builtin(builtin, type);
        if (!spelling || *spelling == "void" || !sameVariadicType(type, function.cppParameterTypes[i])) return {};
        if (i) parameters += ", ";
        parameters += *spelling;
    }
    struct ReplayRecord { CppCRecordSnapshot snapshot; std::string key; };
    std::vector<ReplayRecord> selectedRecords;
    std::vector<std::optional<std::pair<std::size_t, bool>>> recordForArgument;
    for (const auto& key : optionalTypeKeys)
    {
        if (!key.starts_with("type:")) { recordForArgument.push_back(std::nullopt); continue; }
        const bool pointer = key.ends_with('*');
        const auto identity = std::string_view(key).substr(5, key.size() - 5 - (pointer ? 1 : 0));
        if (identity.empty() || identity.ends_with('*')) return {};
        const auto baseKey = "type:" + std::string(identity);
        const auto ordinal = lookup.typeOrdinalForIdentity(identity);
        const auto* imported = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
        if (!imported || !imported->fromPrecompiledCatalog || imported->isIncomplete || !imported->objectLayout ||
            !imported->clangUsr || !imported->deferredImportError.empty() ||
            (imported->origin != frontend::CppImportedTypeOrigin::CStruct &&
             imported->origin != frontend::CppImportedTypeOrigin::CUnion)) return {};
        auto previous = std::find_if(selectedRecords.begin(), selectedRecords.end(), [&](const auto& value) { return value.key == baseKey; });
        if (previous != selectedRecords.end())
        { recordForArgument.emplace_back(std::make_pair(static_cast<std::size_t>(previous - selectedRecords.begin()), pointer)); continue; }
        const CppCRecordSnapshot* match = nullptr;
        for (const auto& record : records)
            if (record.identityKey == identity)
            { if (match) return {}; match = &record; }
        const auto expectedKind = imported->origin == frontend::CppImportedTypeOrigin::CStruct ?
            CppRecordKind::Struct : CppRecordKind::Union;
        if (!match || (!match->plainReplaySyntax && !match->packedReplaySyntax) || match->clangUsr != imported->clangUsr ||
            match->kind != expectedKind || match->name != imported->identity.name() ||
            !match->namespaceSegments.empty() || !match->containingRecords.empty() ||
            !match->sizeBits || !match->alignmentBits || *match->sizeBits / 8 != imported->objectLayout->sizeBytes ||
            *match->alignmentBits / 8 != imported->objectLayout->alignmentBytes || !makeSimpleCRecordAbiReplay(*match)) return {};
        if (selectedRecords.size() >= 1024) return {};
        recordForArgument.emplace_back(std::make_pair(selectedRecords.size(), pointer));
        selectedRecords.push_back({*match, baseKey});
    }
    if (selectedRecords.empty()) return {};
    const auto findDependency = [&](const CppType& type, std::string_view module) -> std::optional<std::size_t> {
        const auto identity = projectCppRecordTypeIdentity(type, module);
        if (!identity || !type.recordIdentity || !type.recordIdentity->clangUsr) return std::nullopt;
        for (std::size_t i = 0; i < selectedRecords.size(); ++i)
            if (selectedRecords[i].key == "type:" + identity->key())
            {
                const auto& snapshot = selectedRecords[i].snapshot;
                if (snapshot.clangUsr != type.recordIdentity->clangUsr || snapshot.sizeBits != type.sizeBits ||
                    snapshot.alignmentBits != type.alignmentBits || snapshot.name != type.recordIdentity->name ||
                    type.canonicalSpelling != std::string(snapshot.kind == CppRecordKind::Struct ? "struct " : "union ") + snapshot.name)
                    return std::nullopt;
                return i;
            }
        return std::nullopt;
    };
    for (std::size_t i = 0; i < selectedRecords.size(); ++i)
    {
        const auto parent = selectedRecords[i].snapshot;
        const auto collect = [&](const auto& self, const CppType& type, unsigned depth) -> bool {
            if (depth > 16) return false;
            if (type.kind == CppTypeKind::Array) return type.element && self(self, *type.element, depth + 1);
            if (type.kind != CppTypeKind::Record) return true;
            if (findDependency(type, parent.moduleName)) return true;
            const auto identity = projectCppRecordTypeIdentity(type, parent.moduleName);
            if (!identity || !type.recordIdentity || !type.recordIdentity->clangUsr) return false;
            const CppCRecordSnapshot* match = nullptr;
            for (const auto& candidate : records)
                if (candidate.identityKey == identity->key())
                { if (match) return false; match = &candidate; }
            const auto ordinal = lookup.typeOrdinalForIdentity(identity->key());
            const auto* imported = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
            if (!match || (!match->plainReplaySyntax && !match->packedReplaySyntax) ||
                match->clangUsr != type.recordIdentity->clangUsr || match->sizeBits != type.sizeBits ||
                match->alignmentBits != type.alignmentBits || !match->sizeBits || !match->alignmentBits ||
                match->name != type.recordIdentity->name ||
                type.canonicalSpelling != std::string(match->kind == CppRecordKind::Struct ? "struct " : "union ") + match->name ||
                !imported || !imported->fromPrecompiledCatalog || imported->isIncomplete ||
                imported->origin != (match->kind == CppRecordKind::Struct ? frontend::CppImportedTypeOrigin::CStruct :
                                                                          frontend::CppImportedTypeOrigin::CUnion) ||
                !imported->objectLayout || !imported->deferredImportError.empty() ||
                imported->clangUsr != match->clangUsr || imported->objectLayout->sizeBytes != *match->sizeBits / 8 ||
                imported->objectLayout->alignmentBytes != *match->alignmentBits / 8 ||
                !makeSimpleCRecordAbiReplay(*match)) return false;
            if (selectedRecords.size() >= 1024) return false;
            selectedRecords.push_back({*match, "type:" + identity->key()});
            return true;
        };
        for (const auto& field : parent.fields) if (!collect(collect, field.type, 0)) return {};
    }
    std::string source;
    const auto resolverFor = [&](std::size_t parent) -> CppReplayRecordResolver {
        return [&, parent](const CppType& type) -> std::optional<std::string> {
            const auto index = findDependency(type, selectedRecords[parent].snapshot.moduleName);
            if (!index) return std::nullopt;
            const auto tag = *index == 0 ? std::string("__nsc_abi_record") : "__nsc_abi_record_" + std::to_string(*index);
            return std::string(selectedRecords[*index].snapshot.kind == CppRecordKind::Struct ? "struct " : "union ") + tag;
        };
    };
    std::vector<unsigned char> visiting(selectedRecords.size());
    std::function<bool(std::size_t, unsigned)> emitRecord = [&](std::size_t i, unsigned depth) {
        if (depth > 64) return false;
        if (visiting[i] == 2) return true;
        if (visiting[i] != 0) return false;
        visiting[i] = 1;
        const auto visitType = [&](const auto& self, const CppType& type, unsigned depth) -> bool {
            if (depth > 16) return false;
            if (type.kind == CppTypeKind::Array) return type.element && self(self, *type.element, depth + 1);
            if (type.kind != CppTypeKind::Record) return true;
            const auto dependency = findDependency(type, selectedRecords[i].snapshot.moduleName);
            return dependency && emitRecord(*dependency, depth + 1);
        };
        for (const auto& field : selectedRecords[i].snapshot.fields)
            if (!visitType(visitType, field.type, 0)) return false;
        const auto declaration = makeSimpleCRecordAbiReplay(selectedRecords[i].snapshot, i, resolverFor(i));
        if (!declaration) return false;
        source += *declaration;
        visiting[i] = 2;
        return true;
    };
    for (std::size_t i = 0; i < selectedRecords.size(); ++i)
        if (!emitRecord(i, 0)) return {};
    for (std::size_t i = 0; i < selectedRecords.size(); ++i)
    {
        const auto tag = i == 0 ? std::string("__nsc_abi_record") : "__nsc_abi_record_" + std::to_string(i);
        source += "void __nsc_replay_type_" + std::to_string(i) + "(" +
            (selectedRecords[i].snapshot.kind == CppRecordKind::Struct ? "struct " : "union ") + tag + ");\n";
    }
    source += *resultType + " __attribute__((sysv_abi)) " + name + "(" + parameters + ", ...);\n";
    CppSessionOptions options;
    options.language = CppLanguageMode::C;
    options.languageStandard = "c11";
    options.targetTriple = environment.targetTriple;
    options.driverReferencePath = environment.driverReferencePath;
    options.resourceDirectory = environment.resourceDirectory;
    options.workingDirectory = environment.workingDirectory;
    options.input = CppBufferInput{"nsc-variadic-replay.c", std::move(source)};
    auto opened = factory.open(std::move(options), {});
    auto* holder = std::get_if<std::unique_ptr<CppInteropSession>>(&opened);
    if (!holder || !*holder) return {};
    auto& session = **holder;
    const auto observedRecords = session.recordsSnapshot();
    const auto observedFunctions = session.freeFunctionsSnapshot();
    if (!observedRecords || !observedFunctions) return {};
    std::vector<CppType> replayTypes(selectedRecords.size());
    for (std::size_t i = 0; i < selectedRecords.size(); ++i)
    {
        const auto tag = i == 0 ? std::string("__nsc_abi_record") : "__nsc_abi_record_" + std::to_string(i);
        const auto record = std::find_if(observedRecords->begin(), observedRecords->end(), [&](const auto& value) { return value.name == tag; });
        const auto helper = std::find_if(observedFunctions->begin(), observedFunctions->end(), [&](const auto& value) {
            return value.name == "__nsc_replay_type_" + std::to_string(i);
        });
        if (record == observedRecords->end() || !matchesSimpleCRecordAbiReplay(selectedRecords[i].snapshot, *record, i, resolverFor(i)) ||
            helper == observedFunctions->end() || helper->parameters.size() != 1) return {};
        replayTypes[i] = helper->parameters[0].type;
    }
    const auto replayFunction = std::find_if(observedFunctions->begin(), observedFunctions->end(),
        [&](const auto& item) { return item.name == name; });
    if (replayFunction == observedFunctions->end() || replayFunction->clangUsr != function.cppClangUsr ||
        (!erasedRecordPointer && replayFunction->canonicalFunctionType != selected->declaration.canonicalFunctionType)) return {};
    const auto base = session.cVariadicCallAbiPlan(name, replayFunction->canonicalFunctionType, {});
    if (!base || abi_plan::encodeVariadicCallPlanForCache(*base) !=
                     abi_plan::encodeVariadicCallPlanForCache(*selected->variadicBasePlan)) return {};
    std::vector<CppType> args;
    for (std::size_t i = 0; i < optionalTypeKeys.size(); ++i)
    {
        if (recordForArgument[i])
        {
            const auto [index, pointer] = *recordForArgument[i];
            if (!pointer) args.push_back(replayTypes[index]);
            else
            {
                CppType type;
                type.kind = CppTypeKind::Pointer;
                type.canonicalSpelling = replayTypes[index].canonicalSpelling + " *";
                type.sizeBits = type.alignmentBits = 64;
                type.element = std::make_shared<const CppType>(replayTypes[index]);
                args.push_back(std::move(type));
            }
            continue;
        }
        auto type = simpleVariadicType(optionalTypeKeys[i]);
        if (!type) return {};
        args.push_back(std::move(*type));
    }
    auto plan = session.cVariadicCallAbiPlan(name, replayFunction->canonicalFunctionType, args);
    if (!plan || !validateCppVariadicSysVCallPlan(*plan, function.mangledName, function.parameters.size())) return {};
    return std::make_shared<const abi_plan::VariadicCallPlan>(std::move(*plan));
}
}
