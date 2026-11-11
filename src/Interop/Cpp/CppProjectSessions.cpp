#include "nativesharp/Interop/CppProjectSessions.h"
#include "nativesharp/Interop/CppVariadicAbiReconciliation.h"
#include "nativesharp/Interop/CppCRecordAbiReplay.h"
#include "nativesharp/Interop/CppVariadicAbiCodec.h"
#include "nativesharp/Frontend/PrecompiledSyntaxModule.h"
#include "../Internal/CppCRecordMetadataCodec.h"
#include "nativesharp/Compilation/Project.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Frontend/NamespaceFunctionSymbol.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <clang/Basic/LangStandard.h>

#include <algorithm>
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

std::shared_ptr<const abi_plan::VariadicCallPlan> planCppVariadicCall(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup, const frontend::NamespaceFunctionSymbol& function,
    std::span<const std::string> optionalTypeKeys)
{
    if (!function.isVariadic || function.cppCallingConvention != "sysv_abi" ||
        !function.cppClangUsr || function.cppClangUsr->empty() || !function.cppReturnType ||
        function.cppParameterTypes.size() != function.parameters.size() || function.projectOrdinal >= graph.projects().size())
        return {};
    std::vector<CppVariadicArgumentOrigin> arguments;
    arguments.reserve(optionalTypeKeys.size());
    for (const auto& key : optionalTypeKeys)
    {
        CppType type;
        if (auto simple = simpleVariadicType(key)) type = std::move(*simple);
        else if (key.starts_with("type:") && !key.ends_with('*'))
        {
            const auto ordinal = lookup.typeOrdinalForIdentity(std::string_view(key).substr(5));
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
            arguments.push_back({std::move(type), origin});
            continue;
        }
        else return {};
        arguments.push_back({std::move(type), nullptr});
    }
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
    auto plan = reconcileCppVariadicCallAbiPlan(*selected, selectedName, selectedCanonical, arguments);
    if (!plan || plan->symbol != function.mangledName || plan->requiredArguments != function.parameters.size()) return {};
    return std::make_shared<const abi_plan::VariadicCallPlan>(std::move(*plan));
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
        environment.targetTriple != "x86_64-pc-windows-msvc") return {};
    const auto builtin = [](const auto& self, const CppType& type, unsigned depth = 0) -> std::optional<std::string> {
        if (depth > 16) return std::nullopt;
        if (type.isConst || type.isVolatile || type.isRestrict) return std::nullopt;
        const auto& name = type.canonicalSpelling;
        if (type.kind == CppTypeKind::Pointer)
        {
            if (!type.element || type.sizeBits != 64 || type.alignmentBits != 64) return std::nullopt;
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
    std::vector<std::optional<std::size_t>> recordForArgument;
    for (const auto& key : optionalTypeKeys)
    {
        if (!key.starts_with("type:") || key.ends_with('*')) { recordForArgument.push_back(std::nullopt); continue; }
        const auto ordinal = lookup.typeOrdinalForIdentity(std::string_view(key).substr(5));
        const auto* imported = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
        if (!imported || !imported->fromPrecompiledCatalog || imported->isIncomplete || !imported->objectLayout ||
            !imported->clangUsr || !imported->deferredImportError.empty() ||
            (imported->origin != frontend::CppImportedTypeOrigin::CStruct &&
             imported->origin != frontend::CppImportedTypeOrigin::CUnion)) return {};
        auto previous = std::find_if(selectedRecords.begin(), selectedRecords.end(), [&](const auto& value) { return value.key == key; });
        if (previous != selectedRecords.end())
        { recordForArgument.push_back(static_cast<std::size_t>(previous - selectedRecords.begin())); continue; }
        const CppCRecordSnapshot* match = nullptr;
        for (const auto& record : records)
            if (record.identityKey == std::string_view(key).substr(5))
            { if (match) return {}; match = &record; }
        const auto expectedKind = imported->origin == frontend::CppImportedTypeOrigin::CStruct ?
            CppRecordKind::Struct : CppRecordKind::Union;
        if (!match || !match->plainReplaySyntax || match->clangUsr != imported->clangUsr ||
            match->kind != expectedKind || match->name != imported->identity.name() ||
            !match->namespaceSegments.empty() || !match->containingRecords.empty() ||
            !match->sizeBits || !match->alignmentBits || *match->sizeBits / 8 != imported->objectLayout->sizeBytes ||
            *match->alignmentBits / 8 != imported->objectLayout->alignmentBytes || !makeSimpleCRecordAbiReplay(*match)) return {};
        recordForArgument.push_back(selectedRecords.size());
        selectedRecords.push_back({*match, key});
    }
    if (selectedRecords.empty()) return {};
    std::string source;
    for (std::size_t i = 0; i < selectedRecords.size(); ++i)
    {
        source += *makeSimpleCRecordAbiReplay(selectedRecords[i].snapshot, i);
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
        if (record == observedRecords->end() || !matchesSimpleCRecordAbiReplay(selectedRecords[i].snapshot, *record, i) ||
            helper == observedFunctions->end() || helper->parameters.size() != 1) return {};
        replayTypes[i] = helper->parameters[0].type;
    }
    const auto replayFunction = std::find_if(observedFunctions->begin(), observedFunctions->end(),
        [&](const auto& item) { return item.name == name; });
    if (replayFunction == observedFunctions->end() || replayFunction->clangUsr != function.cppClangUsr ||
        replayFunction->canonicalFunctionType != selected->declaration.canonicalFunctionType) return {};
    const auto base = session.cVariadicCallAbiPlan(name, replayFunction->canonicalFunctionType, {});
    if (!base || abi_plan::encodeVariadicCallPlanForCache(*base) !=
                     abi_plan::encodeVariadicCallPlanForCache(*selected->variadicBasePlan)) return {};
    std::vector<CppType> args;
    for (std::size_t i = 0; i < optionalTypeKeys.size(); ++i)
    {
        if (recordForArgument[i]) { args.push_back(replayTypes[*recordForArgument[i]]); continue; }
        auto type = simpleVariadicType(optionalTypeKeys[i]);
        if (!type) return {};
        args.push_back(std::move(*type));
    }
    auto plan = session.cVariadicCallAbiPlan(name, replayFunction->canonicalFunctionType, args);
    if (!plan || !validateCppVariadicSysVCallPlan(*plan, function.mangledName, function.parameters.size())) return {};
    return std::make_shared<const abi_plan::VariadicCallPlan>(std::move(*plan));
}
}
