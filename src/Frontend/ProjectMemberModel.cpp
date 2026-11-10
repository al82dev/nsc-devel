#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "Internal/EnumInitializerResolver.h"
#include "Internal/AnonymousMemberPromotionVisitState.h"
#include "Internal/EnumValue.h"
#include "Internal/EnumSuccessor.h"

#include "nativesharp/Language/AggregateSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Compilation/ProjectGraph.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/PrecompiledMemberIdentityIndex.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <filesystem>
#include <functional>
#include <limits>
#include <sstream>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
ProjectMemberModel::ProjectMemberModel() noexcept
    : indexes_(&ownedIndexes_)
{
}

ProjectMemberModel::ProjectMemberModel(ProjectCompilationIndexes& indexes) noexcept
    : indexes_(&indexes)
{
}

namespace
{
bool experimentalCppCallableType(std::string_view key, bool allowVoid)
{
    std::size_t depth = 0;
    while (key.ends_with('*')) { key.remove_suffix(1); ++depth; }
    if (depth > 64) return false;
    if (depth != 0 && key.starts_with("type:") && key.size() > 5) return true;
    constexpr std::string_view types[] = {"builtin:bool", "builtin:sbyte", "builtin:byte", "builtin:short",
        "builtin:ushort", "builtin:int", "builtin:uint", "builtin:long", "builtin:ulong",
        "builtin:float", "builtin:double"};
    return (key == "builtin:void" && (allowVoid || depth != 0)) ||
        std::find(std::begin(types), std::end(types), key) != std::end(types);
}

// EN: allowReconciledCType (C fields): a C enum, struct or union reconciled with a type of another project is
//     visible to the project that owns the reconciled declaration (T08-D26 adds records of precompiled producers).
// ES: allowReconciledCType (campos C): un enum, struct o union C conciliado con un tipo de otro proyecto es visible
//     para el proyecto duenio de la declaracion conciliada (T08-D26 agrega records de productores precompilados).
bool cppCallableRecordIsVisible(std::string_view key, std::size_t projectOrdinal, const ProjectNameLookup& lookup,
    bool allowReconciledCType = false)
{
    while (key.ends_with(']'))
    {
        const auto open = key.rfind('[');
        if (open == std::string_view::npos) return false;
        key = key.substr(0, open);
    }
    while (key.ends_with('*')) key.remove_suffix(1);
    if (!key.starts_with("type:")) return true;
    const auto ordinal = lookup.typeOrdinalForIdentity(key.substr(5));
    const auto* imported = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
    return imported && (imported->projectOrdinal == projectOrdinal ||
        (allowReconciledCType && (imported->origin == CppImportedTypeOrigin::CEnum ||
                                  imported->origin == CppImportedTypeOrigin::CStruct ||
                                  imported->origin == CppImportedTypeOrigin::CUnion) &&
         lookup.isReconciledCppTypeVisibleToProject(*ordinal, projectOrdinal)));
}
}

bool ProjectMemberModel::setExperimentalCppInstanceMethods(
    std::vector<CppImportedInstanceMethod> methods, std::string& error)
{
    if (built_) { error = "C++ instance methods must be configured before building members"; return false; }
    std::set<std::string> signatures, symbols;
    for (const auto& method : methods)
    {
        if (method.ownerIdentityKey.empty() || method.name.empty() || method.externalSymbol.empty() ||
            !experimentalCppCallableType(method.returnTypeKey, true) ||
            (method.returnPointeeReadonly && !method.returnTypeKey.ends_with('*')) ||
            (!method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly.size() != method.parameterTypeKeys.size()))
        { error = "invalid experimental C++ instance method"; return false; }
        std::string signature = method.ownerIdentityKey + "::" + method.name + "(";
        for (std::size_t index = 0; index < method.parameterTypeKeys.size(); ++index)
        {
            const auto& parameter = method.parameterTypeKeys[index];
            if (!experimentalCppCallableType(parameter, false)) { error = "invalid C++ instance parameter type"; return false; }
            if (!method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly[index] && !parameter.ends_with('*'))
            { error = "invalid C++ instance parameter readonly contract"; return false; }
            if (method.parameterTypeKeys.size() == 1 && !method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly[index])
                signature += "const:";
            signature += parameter + ",";
        }
        signature += ")";
        if (method.receiverConst) signature += " const";
        if (!signatures.insert(std::move(signature)).second || !symbols.insert(method.externalSymbol).second)
        { error = "duplicate C++ instance method signature or symbol"; return false; }
    }
    experimentalCppInstanceMethods_ = std::move(methods);
    error.clear();
    return true;
}

namespace
{
std::uint64_t experimentalCppScalarBytes(std::string_view key)
{
    if (key == "builtin:bool" || key == "builtin:sbyte" || key == "builtin:byte") return 1;
    if (key == "builtin:short" || key == "builtin:ushort") return 2;
    if (key == "builtin:int" || key == "builtin:uint" || key == "builtin:float") return 4;
    if (key == "builtin:long" || key == "builtin:ulong" || key == "builtin:double") return 8;
    return 0;
}

std::uint64_t experimentalCppFieldBytes(std::string_view key)
{
    if (key.ends_with('*'))
    {
        key.remove_suffix(1);
        return key == "builtin:void" || experimentalCppScalarBytes(key) ||
            (key.starts_with("type:") && key.size() > 5) ? 8 : 0;
    }
    return experimentalCppScalarBytes(key);
}
}

bool ProjectMemberModel::setExperimentalCppLifetimeMethods(
    std::vector<CppImportedLifetimeMethod> methods, std::string& error)
{
    if (built_) { error = "C++ lifetime methods must be configured before building members"; return false; }
    std::set<std::tuple<std::size_t, std::string, CppImportedLifetimeKind, std::vector<std::string>>> signatures;
    std::set<std::string> symbols;
    for (const auto& method : methods)
    {
        if (method.ownerIdentityKey.empty() || method.externalSymbol.empty() ||
            (method.kind != CppImportedLifetimeKind::Constructor && method.kind != CppImportedLifetimeKind::Destructor) ||
            (method.kind == CppImportedLifetimeKind::Destructor && !method.parameterTypeKeys.empty()) ||
            (!method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly.size() != method.parameterTypeKeys.size()))
        { error = "invalid experimental C++ lifetime method"; return false; }
        for (const auto& parameter : method.parameterTypeKeys)
        {
            std::string_view base = parameter;
            const bool pointer = base.ends_with('*');
            while (base.ends_with('*')) base.remove_suffix(1);
            if (!experimentalCppScalarBytes(base) && !(pointer && base == "builtin:void"))
            { error = "invalid C++ lifetime parameter type"; return false; }
        }
        auto signatureTypes = method.parameterTypeKeys;
        if (signatureTypes.size() == 1 && !method.parameterPointeeReadonly.empty() &&
            method.parameterPointeeReadonly[0] && signatureTypes[0].ends_with('*'))
            signatureTypes[0] = "const:" + signatureTypes[0];
        if (!signatures.emplace(method.projectOrdinal, method.ownerIdentityKey, method.kind, std::move(signatureTypes)).second ||
            !symbols.insert(method.externalSymbol).second)
        { error = "duplicate C++ lifetime method signature or symbol"; return false; }
    }
    experimentalCppLifetimeMethods_ = std::move(methods);
    error.clear();
    return true;
}

bool ProjectMemberModel::setExperimentalCppStaticMethods(
    std::vector<CppImportedStaticMethod> methods, std::string& error)
{
    if (built_) { error = "C++ static methods must be configured before building members"; return false; }
    std::set<std::string> signatures, symbols;
    for (const auto& method : methods)
    {
        if (method.ownerIdentityKey.empty() || method.name.empty() || method.externalSymbol.empty() ||
            !experimentalCppCallableType(method.returnTypeKey, true) ||
            (method.returnPointeeReadonly && !method.returnTypeKey.ends_with('*')) ||
            (!method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly.size() != method.parameterTypeKeys.size()))
        { error = "invalid experimental C++ static method"; return false; }
        std::string signature = method.ownerIdentityKey + "::" + method.name + "(";
        for (std::size_t index = 0; index < method.parameterTypeKeys.size(); ++index)
        {
            const auto& parameter = method.parameterTypeKeys[index];
            if (!experimentalCppCallableType(parameter, false)) { error = "invalid C++ static parameter type"; return false; }
            if (!method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly[index] && !parameter.ends_with('*'))
            { error = "invalid C++ static parameter readonly contract"; return false; }
            if (method.parameterTypeKeys.size() == 1 && !method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly[index])
                signature += "const:";
            signature += parameter + ",";
        }
        signature += ")";
        if (!signatures.insert(std::move(signature)).second || !symbols.insert(method.externalSymbol).second)
        { error = "duplicate C++ static method signature or symbol"; return false; }
    }
    experimentalCppStaticMethods_ = std::move(methods);
    error.clear();
    return true;
}

bool ProjectMemberModel::setExperimentalCppInstanceFields(
    std::vector<CppImportedInstanceField> fields, std::string& error)
{
    if (built_) { error = "C++ instance fields must be configured before building members"; return false; }
    std::set<std::pair<std::string, std::string>> identities;
    // EN: T08-D23: pointee flags belong to a pointer, or to the pointer elements of a fixed array.
    // ES: T08-D23: las marcas del dato apuntado son de un puntero o de los elementos puntero de un arreglo fijo.
    const auto pointerCarrier = [](std::string key)
    {
        while (const auto element = fixedArrayElementTypeKey(key)) key = *element;
        return key.ends_with('*');
    };
    for (const auto& field : fields)
    {
        // Un miembro de arreglo flexible C (`T[*]`) no tiene almacenamiento propio: tamano cero y la
        // alineacion de su elemento. A C flexible array member (`T[*]`) has zero storage and its
        // element's alignment; every other field keeps the previous size/alignment pairing rule.
        const bool flexible = flexibleArrayElementTypeKey(field.typeKey).has_value();
        if (field.ownerIdentityKey.empty() || field.name.empty() ||
            (flexible && (field.byteSize != 0 || field.byteAlignment == 0 || field.bitWidth ||
                          field.pointeeReadonly)) ||
            (!flexible && !experimentalCppFieldBytes(field.typeKey) && (!field.byteSize || !field.byteAlignment)) ||
            (!flexible && (field.byteSize == 0) != (field.byteAlignment == 0)) ||
            ((field.pointeeReadonly || field.pointeeVolatile) && !pointerCarrier(field.typeKey)))
        { error = "invalid experimental C++ instance field"; return false; }
        if (!identities.emplace(field.ownerIdentityKey, field.name).second)
        { error = "duplicate C++ instance field"; return false; }
    }
    experimentalCppInstanceFields_ = std::move(fields);
    error.clear();
    return true;
}

bool ProjectMemberModel::setExperimentalCppStaticFields(
    std::vector<CppImportedStaticField> fields, std::string& error)
{
    if (built_) { error = "C++ static fields must be configured before building members"; return false; }
    std::set<std::pair<std::string, std::string>> identities;
    std::set<std::string> symbols;
    for (const auto& field : fields)
    {
        if (field.ownerIdentityKey.empty() || field.name.empty() || field.externalSymbol.empty() ||
            !experimentalCppFieldBytes(field.typeKey) ||
            (field.pointeeReadonly && !field.typeKey.ends_with('*')))
        { error = "invalid experimental C++ static field"; return false; }
        if (!identities.emplace(field.ownerIdentityKey, field.name).second ||
            !symbols.insert(field.externalSymbol).second)
        { error = "duplicate C++ static field name or symbol"; return false; }
    }
    experimentalCppStaticFields_ = std::move(fields);
    error.clear();
    return true;
}

bool ProjectMemberModel::build(
    const ProjectNameLookup& nameLookup,
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics,
    const ProjectGenericModel* genericModel)
{
    if (built_)
    {
        return !diagnostics.hasErrors();
    }

    initializeBuildState(nameLookup, genericModel);

    collectDeclaredTypeMembers(nameLookup, diagnostics);
    collectExternalTraitImplMembers(nameLookup, diagnostics);
    for (const auto& imported : experimentalCppLifetimeMethods_)
    {
        const auto ordinal = nameLookup.typeOrdinalForIdentity(imported.ownerIdentityKey);
        const auto* owner = ordinal ? nameLookup.cppImportedType(*ordinal) : nullptr;
        if (!owner || owner->projectOrdinal != imported.projectOrdinal)
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidType, diagnostics::DiagnosticSeverity::Error,
                SourceRange{}, "C++ lifetime method owner is not the imported type in its project");
            return false;
        }
        ClassMemberSymbol member;
        member.kind = imported.kind == CppImportedLifetimeKind::Constructor
            ? ClassMemberKind::Constructor : ClassMemberKind::Destructor;
        member.accessibility = ClassMemberAccessibility::Public;
        member.declaringTypeOrdinal = *ordinal;
        member.name = member.kind == ClassMemberKind::Constructor ? ".ctor" : ".dtor";
        member.declaredTypeKey = "builtin:void";
        member.hasImplicitThis = true;
        member.declaringTypeIsStruct = true;
        member.isCppExternal = true;
        member.cppExternalSymbol = imported.externalSymbol;
        member.cppDllImport = imported.dllImport;
        member.cppLifetimeNonThrowing = imported.isNonThrowing;
        member.cppLifetimeImplicit = imported.isImplicit;
        member.cppLifetimeTrivial = imported.isTrivial;
        member.signatureKey = member.name + "(";
        for (std::size_t i = 0; i < imported.parameterTypeKeys.size(); ++i)
        {
            MethodParameterSymbol parameter;
            parameter.name = "arg" + std::to_string(i);
            parameter.typeKey = imported.parameterTypeKeys[i];
            parameter.pointerPointeeReadonly = !imported.parameterPointeeReadonly.empty() && imported.parameterPointeeReadonly[i];
            member.parameters.push_back(std::move(parameter));
            if (i) member.signatureKey += ",";
            if (imported.parameterTypeKeys.size() == 1 && !imported.parameterPointeeReadonly.empty() && imported.parameterPointeeReadonly[i]) member.signatureKey += "const:";
            member.signatureKey += imported.parameterTypeKeys[i];
        }
        member.signatureKey += ")";
        const auto index = members_.size();
        members_.push_back(std::move(member));
        membersByType_[*ordinal].push_back(index);
        membersByName_[*ordinal][members_[index].name].push_back(index);
        if (imported.kind == CppImportedLifetimeKind::Constructor) constructorsByType_[*ordinal].push_back(index);
    }
    for (const auto& imported : experimentalCppInstanceMethods_)
    {
        const auto ordinal = nameLookup.typeOrdinalForIdentity(imported.ownerIdentityKey);
        const auto* owner = ordinal ? nameLookup.cppImportedType(*ordinal) : nullptr;
        if (!owner || owner->projectOrdinal != imported.projectOrdinal ||
            !cppCallableRecordIsVisible(imported.returnTypeKey, imported.projectOrdinal, nameLookup) ||
            std::any_of(imported.parameterTypeKeys.begin(), imported.parameterTypeKeys.end(), [&](const auto& key) {
                return !cppCallableRecordIsVisible(key, imported.projectOrdinal, nameLookup);
            }))
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidType, diagnostics::DiagnosticSeverity::Error,
                SourceRange{}, "C++ instance method owner or parameter type is not imported in its project");
            return false;
        }
    }
    for (const auto& imported : experimentalCppInstanceMethods_)
    {
        const auto ordinal = *nameLookup.typeOrdinalForIdentity(imported.ownerIdentityKey);
        ClassMemberSymbol member;
        member.kind = ClassMemberKind::Method;
        member.accessibility = ClassMemberAccessibility::Public;
        member.declaringTypeOrdinal = ordinal;
        member.name = imported.name;
        member.declaredTypeKey = imported.returnTypeKey;
        member.declaredPointerPointeeReadonly = imported.returnPointeeReadonly;
        member.hasImplicitThis = true;
        member.declaringTypeIsStruct = true;
        member.isReadonly = imported.receiverConst;
        member.isCppExternal = true;
        member.cppExternalSymbol = imported.externalSymbol;
        member.cppDllImport = imported.dllImport;
        member.signatureKey = member.name + "(";
        for (std::size_t i = 0; i < imported.parameterTypeKeys.size(); ++i)
        {
            MethodParameterSymbol parameter;
            parameter.name = "arg" + std::to_string(i);
            parameter.typeKey = imported.parameterTypeKeys[i];
            parameter.pointerPointeeReadonly = !imported.parameterPointeeReadonly.empty() && imported.parameterPointeeReadonly[i];
            member.parameters.push_back(std::move(parameter));
            if (i) member.signatureKey += ",";
            if (imported.parameterTypeKeys.size() == 1 && !imported.parameterPointeeReadonly.empty() && imported.parameterPointeeReadonly[i]) member.signatureKey += "const:";
            member.signatureKey += imported.parameterTypeKeys[i];
        }
        member.signatureKey += ")";
        if (imported.receiverConst) member.signatureKey += " const";
        const auto memberOrdinal = members_.size();
        members_.push_back(std::move(member));
        membersByType_[ordinal].push_back(memberOrdinal);
        membersByName_[ordinal][imported.name].push_back(memberOrdinal);
    }
    for (const auto& imported : experimentalCppStaticMethods_)
    {
        const auto ordinal = nameLookup.typeOrdinalForIdentity(imported.ownerIdentityKey);
        const auto* owner = ordinal ? nameLookup.cppImportedType(*ordinal) : nullptr;
        if (!owner || owner->projectOrdinal != imported.projectOrdinal ||
            !cppCallableRecordIsVisible(imported.returnTypeKey, imported.projectOrdinal, nameLookup) ||
            std::any_of(imported.parameterTypeKeys.begin(), imported.parameterTypeKeys.end(), [&](const auto& key) {
                return !cppCallableRecordIsVisible(key, imported.projectOrdinal, nameLookup);
            }))
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidType, diagnostics::DiagnosticSeverity::Error,
                SourceRange{}, "C++ static method owner or parameter type is not imported in its project");
            return false;
        }
        ClassMemberSymbol member;
        member.kind = ClassMemberKind::Method;
        member.accessibility = ClassMemberAccessibility::Public;
        member.declaringTypeOrdinal = *ordinal;
        member.name = imported.name;
        member.declaredTypeKey = imported.returnTypeKey;
        member.declaredPointerPointeeReadonly = imported.returnPointeeReadonly;
        member.declaringTypeIsStruct = true;
        member.isStatic = true;
        member.isCppExternal = true;
        member.cppExternalSymbol = imported.externalSymbol;
        member.cppDllImport = imported.dllImport;
        member.signatureKey = member.name + "(";
        for (std::size_t i = 0; i < imported.parameterTypeKeys.size(); ++i)
        {
            MethodParameterSymbol parameter;
            parameter.name = "arg" + std::to_string(i);
            parameter.typeKey = imported.parameterTypeKeys[i];
            parameter.pointerPointeeReadonly = !imported.parameterPointeeReadonly.empty() && imported.parameterPointeeReadonly[i];
            member.parameters.push_back(std::move(parameter));
            if (i) member.signatureKey += ",";
            if (imported.parameterTypeKeys.size() == 1 && !imported.parameterPointeeReadonly.empty() && imported.parameterPointeeReadonly[i]) member.signatureKey += "const:";
            member.signatureKey += imported.parameterTypeKeys[i];
        }
        member.signatureKey += ")";
        const auto existing = membersByName_[*ordinal].find(imported.name);
        if (existing != membersByName_[*ordinal].end())
            for (const auto existingOrdinal : existing->second)
                if (members_[existingOrdinal].kind != ClassMemberKind::Method ||
                    members_[existingOrdinal].signatureKey == member.signatureKey ||
                    members_[existingOrdinal].cppExternalSymbol == imported.externalSymbol)
                {
                    diagnostics.report(diagnostics::DiagnosticId::InvalidType, diagnostics::DiagnosticSeverity::Error,
                        SourceRange{}, "C++ static method conflicts with an indexed member");
                    return false;
                }
        const auto memberOrdinal = members_.size();
        members_.push_back(std::move(member));
        membersByType_[*ordinal].push_back(memberOrdinal);
        membersByName_[*ordinal][imported.name].push_back(memberOrdinal);
    }
    for (const auto& imported : experimentalCppInstanceFields_)
    {
        const auto ordinal = nameLookup.typeOrdinalForIdentity(imported.ownerIdentityKey);
        const auto* owner = ordinal ? nameLookup.cppImportedType(*ordinal) : nullptr;
        const auto* ownerLayout = owner && owner->memberAccessLayout ? &*owner->memberAccessLayout :
            owner && owner->objectLayout ? &*owner->objectLayout : nullptr;
        const auto width = imported.byteSize ? imported.byteSize : experimentalCppFieldBytes(imported.typeKey);
        const auto alignment = imported.byteAlignment ? imported.byteAlignment : width;
        const bool invalidBits = imported.bitOffset.has_value() != imported.bitWidth.has_value() ||
            (imported.bitOffset && (width == 0 || width > 8 || (width & (width - 1)) != 0 ||
             *imported.bitOffset >= width * 8 || !*imported.bitWidth ||
             *imported.bitWidth > width * 8 - *imported.bitOffset));
        if (!owner || owner->projectOrdinal != imported.projectOrdinal || !ownerLayout ||
            invalidBits || alignment == 0 || (alignment & (alignment - 1)) != 0 ||
            !cppCallableRecordIsVisible(imported.typeKey, imported.projectOrdinal, nameLookup, true) ||
            imported.byteOffset > ownerLayout->sizeBytes ||
            width > ownerLayout->sizeBytes - imported.byteOffset ||
            imported.byteOffset % alignment != 0 || ownerLayout->alignmentBytes % alignment != 0 ||
            membersByName_[*ordinal].contains(imported.name))
        {
            const char* reason = !owner ? "owner missing" :
                owner->projectOrdinal != imported.projectOrdinal ? "owner project mismatch" :
                !ownerLayout ? "owner layout missing" : invalidBits ? "invalid bitfield" :
                alignment == 0 || (alignment & (alignment - 1)) != 0 ? "invalid alignment" :
                !cppCallableRecordIsVisible(imported.typeKey, imported.projectOrdinal, nameLookup, true) ? "field type not visible" :
                imported.byteOffset > ownerLayout->sizeBytes ? "offset outside owner" :
                width > ownerLayout->sizeBytes - imported.byteOffset ? "field exceeds owner" :
                imported.byteOffset % alignment != 0 || ownerLayout->alignmentBytes % alignment != 0 ? "unaligned field" :
                "duplicate member";
            diagnostics.report(diagnostics::DiagnosticId::InvalidType, diagnostics::DiagnosticSeverity::Error,
                SourceRange{}, "C++ instance field owner, layout or name is invalid: " +
                    imported.ownerIdentityKey + "." + imported.name + " (" + reason +
                    "; type=" + imported.typeKey + "; project=" + std::to_string(imported.projectOrdinal) + ")");
            return false;
        }
        ClassMemberSymbol member;
        member.kind = ClassMemberKind::Field;
        member.accessibility = ClassMemberAccessibility::Public;
        member.declaringTypeOrdinal = *ordinal;
        member.name = imported.name;
        member.declaredTypeKey = imported.typeKey;
        member.declaredPointerPointeeReadonly = imported.pointeeReadonly;
        member.declaredPointerPointeeVolatile = imported.pointeeVolatile;
        member.isReadonly = imported.isConst;
        member.isVolatile = imported.isVolatile;
        member.isCppExternal = true;
        member.cppMicrosoftVolatileSemantics = imported.microsoftVolatileSemantics;
        member.isCppAnonymousCarrier = imported.isAnonymousCarrier;
        member.isCompilerGenerated = imported.isAnonymousCarrier;
        member.cppClangUsr = imported.clangUsr;
        member.explicitFieldOffset = imported.byteOffset;
        member.explicitFieldAlignment = alignment;
        member.isBitField = imported.bitWidth.has_value();
        member.explicitBitOffset = imported.bitOffset;
        member.explicitBitWidth = imported.bitWidth;
        if (member.isBitField) member.explicitBitStorageSize = width;
        const auto memberOrdinal = members_.size();
        members_.push_back(std::move(member));
        membersByType_[*ordinal].push_back(memberOrdinal);
        membersByName_[*ordinal][imported.name].push_back(memberOrdinal);
    }
    for (const auto& imported : experimentalCppStaticFields_)
    {
        const auto ordinal = nameLookup.typeOrdinalForIdentity(imported.ownerIdentityKey);
        const auto* owner = ordinal ? nameLookup.cppImportedType(*ordinal) : nullptr;
        if (!owner || owner->projectOrdinal != imported.projectOrdinal ||
            !cppCallableRecordIsVisible(imported.typeKey, imported.projectOrdinal, nameLookup) ||
            membersByName_[*ordinal].contains(imported.name))
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidType, diagnostics::DiagnosticSeverity::Error,
                SourceRange{}, "C++ static field owner or name is invalid");
            return false;
        }
        ClassMemberSymbol member;
        member.kind = ClassMemberKind::Field;
        member.accessibility = ClassMemberAccessibility::Public;
        member.declaringTypeOrdinal = *ordinal;
        member.name = imported.name;
        member.declaredTypeKey = imported.typeKey;
        member.isStatic = true;
        member.declaredPointerPointeeReadonly = imported.pointeeReadonly;
        member.isReadonly = imported.isConst;
        member.isVolatile = imported.isVolatile;
        member.isCppExternal = true;
        member.cppExternalSymbol = imported.externalSymbol;
        member.cppDllImport = imported.dllImport;
        const auto memberOrdinal = members_.size();
        members_.push_back(std::move(member));
        membersByType_[*ordinal].push_back(memberOrdinal);
        membersByName_[*ordinal][imported.name].push_back(memberOrdinal);
    }
    // Method-body shards share this model. Complete the ownership closure while build() is still
    // single-threaded so every later query is a read-only lookup instead of a lazy cache write.
    buildTransitiveOwnershipCache();

    validateCopyTypes(nameLookup, diagnostics);

    validateRawUnions(nameLookup, diagnostics);
    collectAnonymousMemberPromotions(nameLookup, diagnostics);
    buildMembersDeclaredByTypeIndex();
    buildMemberNameIndex();
    buildUserConversionIndex();
    indexes_->rebuildMembers(members_);

    validateCAbiExportNames(diagnostics);
    validateImportedCIdentities(diagnostics);
    scanMemberBodies(diagnostics);
    scanCompilationUnits(units, diagnostics);
    built_ = true;
    return !diagnostics.hasErrors();
}


std::span<const ClassMemberSymbol> ProjectMemberModel::members() const noexcept
{
    return members_;
}

std::span<const ModuleInitializationRecord> ProjectMemberModel::moduleInitializers() const noexcept
{
    return moduleInitializers_;
}

std::span<const MemberBodyBinding> ProjectMemberModel::bodyBindings() const noexcept
{
    return bodyBindings_;
}

std::span<const std::size_t> ProjectMemberModel::membersOfType(std::size_t typeOrdinal) const noexcept
{
    static const std::vector<std::size_t> empty;
    if (typeOrdinal >= membersByType_.size())
    {
        return empty;
    }
    return membersByType_[typeOrdinal];
}

std::span<const std::size_t> ProjectMemberModel::membersDeclaredByType(
    std::size_t typeOrdinal) const noexcept
{
    static const std::vector<std::size_t> empty;
    return membersDeclaredByTypeIndexBuilt_ && typeOrdinal < membersDeclaredByType_.size()
        ? std::span<const std::size_t>(membersDeclaredByType_[typeOrdinal])
        : std::span<const std::size_t>(empty);
}

bool ProjectMemberModel::hasMembersDeclaredByTypeIndex() const noexcept
{
    return membersDeclaredByTypeIndexBuilt_;
}

void ProjectMemberModel::buildUserConversionIndex()
{
    // EN: membersByType_ and the user-conversion flag of every member are final at this point of
    //     build(): members are only appended while collecting, before this call.
    // ES: membersByType_ y la marca de conversión de usuario de cada miembro son definitivos en
    //     este punto de build(): los miembros solo se agregan al recolectar, antes de esta llamada.
    userConversionsByType_.clear();
    userConversionsByType_.resize(membersByType_.size());
    for (std::size_t typeOrdinal = 0; typeOrdinal < membersByType_.size(); ++typeOrdinal)
    {
        for (const std::size_t memberOrdinal : membersByType_[typeOrdinal])
        {
            if (memberOrdinal < members_.size() && members_[memberOrdinal].isUserConversion)
            {
                userConversionsByType_[typeOrdinal].push_back(memberOrdinal);
            }
        }
    }
    userConversionIndexBuilt_ = true;
}

std::span<const std::size_t> ProjectMemberModel::userConversionsOfType(
    std::size_t typeOrdinal) const noexcept
{
    static const std::vector<std::size_t> empty;
    return userConversionIndexBuilt_ && typeOrdinal < userConversionsByType_.size()
        ? std::span<const std::size_t>(userConversionsByType_[typeOrdinal])
        : std::span<const std::size_t>(empty);
}

bool ProjectMemberModel::hasUserConversionIndex() const noexcept
{
    return userConversionIndexBuilt_;
}

std::span<const std::size_t> ProjectMemberModel::constructorsOfType(
    std::size_t typeOrdinal) const noexcept
{
    static const std::vector<std::size_t> empty;
    if (typeOrdinal >= constructorsByType_.size())
    {
        return empty;
    }
    return constructorsByType_[typeOrdinal];
}

std::optional<std::size_t> ProjectMemberModel::typeOrdinalForTypeKey(
    std::string_view typeKey) const noexcept
{
    return typeOrdinalForKey(typeKey);
}

bool ProjectMemberModel::isBuilt() const noexcept
{
    return built_;
}

}
