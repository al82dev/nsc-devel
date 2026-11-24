#include "nativesharp/Interop/CppScalarTypeMapping.h"
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
#include <map>
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
bool experimentalCppCallableType(std::string_view key, bool allowVoid, bool allowValueType = false)
{
    std::size_t depth = 0;
    while (key.ends_with('*')) { key.remove_suffix(1); ++depth; }
    if (depth > 64) return false;
    if ((depth != 0 || allowValueType) && key.starts_with("type:") && key.size() > 5) return true;
    constexpr std::string_view types[] = {"builtin:bool", "builtin:sbyte", "builtin:byte", "builtin:short",
        "builtin:ushort", "builtin:int", "builtin:uint", "builtin:long", "builtin:ulong",
        "builtin:float", "builtin:double"};
    return (key == "builtin:void" && (allowVoid || depth != 0)) ||
        std::find(std::begin(types), std::end(types), key) != std::end(types);
}

// EN: T10-005. The provider verified that two projects share a C++ static method symbol and that their C++ types
//     are the same Clang declarations; only the module of each imported type key may differ.
// ES: T10-005. El proveedor verifico que dos proyectos comparten el simbolo de un metodo estatico C++ y que sus
//     tipos C++ son las mismas declaraciones de Clang; solo puede diferir el modulo de cada clave de tipo importado.
std::string sharedCppTypeKeyShape(std::string key)
{
    for (std::size_t at = key.find("type:"); at != std::string::npos; at = key.find("type:", at + 5))
    {
        const auto separator = key.find("::", at + 5);
        if (separator != std::string::npos) key.erase(at + 5, separator + 2 - (at + 5));
    }
    return key;
}

std::vector<std::string> sharedCppTypeKeyShapes(const std::vector<std::string>& keys)
{
    std::vector<std::string> result;
    for (const auto& key : keys) result.push_back(sharedCppTypeKeyShape(key));
    return result;
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
    // C3-01: the Core carrier of C `_Float16` is visible to every project. / El portador Core de `_Float16` es
    //     visible para todo proyecto.
    if (key == interop::kCppHalfTypeKey) return true;
    const auto ordinal = lookup.typeOrdinalForIdentity(key.substr(5));
    const auto* imported = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
    // T13-007: a NativeSharp struct mirrored for a C++ template. / Un struct NativeSharp reflejado para C++.
    if (!imported && lookup.isCppNativeMirrorType(key.substr(5))) return true;
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
    std::set<std::string> signatures;
    std::map<std::string, const CppImportedInstanceMethod*> symbols;
    for (const auto& method : methods)
    {
        if (method.ownerIdentityKey.empty() || method.name.empty() || method.externalSymbol.empty() ||
            !experimentalCppCallableType(method.returnTypeKey, !method.returnsReference, true) ||
            (method.returnPointeeReadonly && !method.returnTypeKey.ends_with('*')) ||
            (!method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly.size() != method.parameterTypeKeys.size()) ||
            (!method.parameterPassKinds.empty() && method.parameterPassKinds.size() != method.parameterTypeKeys.size()) ||
            (!method.objectSizeModes.empty() && method.objectSizeModes.size() != method.parameterTypeKeys.size()))
        { error = "invalid experimental C++ instance method"; return false; }
        std::string signature = method.ownerIdentityKey + "::" + method.name + "(";
        for (std::size_t index = 0; index < method.parameterTypeKeys.size(); ++index)
        {
            const auto& parameter = method.parameterTypeKeys[index];
            const auto passKind = method.parameterPassKinds.empty() ? ParameterPassKind::Value
                                                                     : method.parameterPassKinds[index];
            if (!experimentalCppCallableType(parameter, false, true) ||
                (passKind != ParameterPassKind::Value && !parameter.ends_with('*')))
            { error = "invalid C++ instance parameter type"; return false; }
            if (passKind == ParameterPassKind::Ref) signature += "ref:";
            else if (passKind == ParameterPassKind::In) signature += "in:";
            if (!method.objectSizeModes.empty() &&
                (method.objectSizeModes[index] > 8 || (method.objectSizeModes[index] && !parameter.ends_with('*'))))
            { error = "invalid C++ instance parameter object size contract"; return false; }
            if (!method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly[index] && !parameter.ends_with('*'))
            { error = "invalid C++ instance parameter readonly contract"; return false; }
            if (method.parameterTypeKeys.size() == 1 && !method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly[index])
                signature += "const:";
            signature += parameter + ",";
        }
        signature += ")";
        // T19-006: the C++ spellings keep overloads with equal NativeSharp types apart. / Las separan.
        signature += method.cppSignatureSuffix;
        if (method.receiverConst) signature += " const";
        if (method.receiverRvalue) signature += " &&"; // X2: `value() &` and `value() &&`
        const auto [symbol, inserted] = symbols.emplace(method.externalSymbol, &method);
        if (!signatures.insert(std::move(signature)).second || (!inserted &&
            (!symbol->second->sharedExternalSymbolVerified || !method.sharedExternalSymbolVerified ||
             symbol->second->name != method.name || symbol->second->returnTypeKey != method.returnTypeKey ||
             symbol->second->parameterTypeKeys != method.parameterTypeKeys ||
             symbol->second->receiverConst != method.receiverConst ||
             symbol->second->returnPointeeReadonly != method.returnPointeeReadonly ||
             symbol->second->parameterPointeeReadonly != method.parameterPointeeReadonly ||
             symbol->second->objectSizeModes != method.objectSizeModes || symbol->second->dllImport != method.dllImport)))
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
            (!method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly.size() != method.parameterTypeKeys.size()) ||
            (!method.parameterPassKinds.empty() && method.parameterPassKinds.size() != method.parameterTypeKeys.size()))
        { error = "invalid experimental C++ lifetime method"; return false; }
        // T11-006: scalars, pointers, enums and trivial records by value, and ref/in references.
        for (std::size_t index = 0; index < method.parameterTypeKeys.size(); ++index)
        {
            const auto& parameter = method.parameterTypeKeys[index];
            const auto passKind = method.parameterPassKinds.empty() ? ParameterPassKind::Value
                                                                     : method.parameterPassKinds[index];
            if (!experimentalCppCallableType(parameter, false, true) ||
                (passKind != ParameterPassKind::Value && !parameter.ends_with('*')))
            { error = "invalid C++ lifetime parameter type"; return false; }
        }
        auto signatureTypes = method.parameterTypeKeys;
        if (signatureTypes.size() == 1 && !method.parameterPointeeReadonly.empty() &&
            method.parameterPointeeReadonly[0] && signatureTypes[0].ends_with('*'))
            signatureTypes[0] = "const:" + signatureTypes[0];
        for (std::size_t index = 0; index < method.parameterPassKinds.size(); ++index)
            if (method.parameterPassKinds[index] != ParameterPassKind::Value)
                signatureTypes[index] = (method.parameterPassKinds[index] == ParameterPassKind::Ref ? "ref:" : "in:") +
                    signatureTypes[index];
        // T19-006: the C++ spellings keep `S(int)` and `S(long)` apart. / Las separan.
        if (!method.cppSignatureSuffix.empty()) signatureTypes.push_back(method.cppSignatureSuffix);
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
    std::set<std::string> signatures;
    std::map<std::string, const CppImportedStaticMethod*> symbols;
    for (const auto& method : methods)
    {
        if (method.ownerIdentityKey.empty() || method.name.empty() || method.externalSymbol.empty() ||
            !experimentalCppCallableType(method.returnTypeKey, !method.returnsReference, true) ||
            (method.returnPointeeReadonly && !method.returnTypeKey.ends_with('*')) ||
            (!method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly.size() != method.parameterTypeKeys.size()) ||
            (!method.parameterPassKinds.empty() &&
             method.parameterPassKinds.size() != method.parameterTypeKeys.size()) ||
            (!method.objectSizeModes.empty() && method.objectSizeModes.size() != method.parameterTypeKeys.size()))
        { error = "invalid experimental C++ static method"; return false; }
        std::string signature = method.ownerIdentityKey + "::" + method.name + "(";
        for (std::size_t index = 0; index < method.parameterTypeKeys.size(); ++index)
        {
            const auto& parameter = method.parameterTypeKeys[index];
            // T10-005: a static method can take an imported enum by value; build() checks that the type is one.
            if (!experimentalCppCallableType(parameter, false, true)) { error = "invalid C++ static parameter type"; return false; }
            // T19-007: a ref or in parameter carries its referent's address. / Lleva la direccion del referente.
            const auto passKind = method.parameterPassKinds.empty() ? ParameterPassKind::Value
                                                                     : method.parameterPassKinds[index];
            if (passKind != ParameterPassKind::Value && !parameter.ends_with('*'))
            { error = "invalid C++ static parameter type"; return false; }
            if (passKind == ParameterPassKind::Ref) signature += "ref:";
            else if (passKind == ParameterPassKind::In) signature += "in:";
            if (!method.objectSizeModes.empty() &&
                (method.objectSizeModes[index] > 8 || (method.objectSizeModes[index] && !parameter.ends_with('*'))))
            { error = "invalid C++ static parameter object size contract"; return false; }
            if (!method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly[index] && !parameter.ends_with('*'))
            { error = "invalid C++ static parameter readonly contract"; return false; }
            if (method.parameterTypeKeys.size() == 1 && !method.parameterPointeeReadonly.empty() && method.parameterPointeeReadonly[index])
                signature += "const:";
            signature += parameter + ",";
        }
        signature += ")";
        // T19-006: the C++ spellings keep overloads with equal NativeSharp types apart. / Las separan.
        signature += method.cppSignatureSuffix;
        const auto [symbol, inserted] = symbols.emplace(method.externalSymbol, &method);
        if (!signatures.insert(std::move(signature)).second || (!inserted &&
            (!symbol->second->sharedExternalSymbolVerified || !method.sharedExternalSymbolVerified ||
             symbol->second->name != method.name ||
             sharedCppTypeKeyShape(symbol->second->returnTypeKey) != sharedCppTypeKeyShape(method.returnTypeKey) ||
             sharedCppTypeKeyShapes(symbol->second->parameterTypeKeys) != sharedCppTypeKeyShapes(method.parameterTypeKeys) ||
             symbol->second->returnPointeeReadonly != method.returnPointeeReadonly ||
             symbol->second->parameterPointeeReadonly != method.parameterPointeeReadonly ||
             symbol->second->objectSizeModes != method.objectSizeModes || symbol->second->dllImport != method.dllImport)))
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
    cppNotImportedFields_.clear();
    std::erase_if(fields, [this](CppImportedInstanceField& field)
    {
        if (!field.notImportedReason) return false;
        cppNotImportedFields_.emplace(std::pair{field.ownerIdentityKey, field.name}, *field.notImportedReason);
        return true;
    });
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
        const bool invalidArrayLayout = (!field.cppArrayElementStrides.empty() && field.cppArrayElementStrides.size() != field.cppArrayPhysicalLayouts.size()) ||
            (!field.cppArrayPhysicalLayouts.empty() && field.cppArrayPhysicalLayouts.front().first != field.byteSize) ||
            std::ranges::any_of(field.cppArrayPhysicalLayouts, [](const auto& layout) {
                return layout.first == 0 || layout.second == 0 || (layout.second & (layout.second - 1)) != 0;
            });
        if (field.ownerIdentityKey.empty() || field.name.empty() || invalidArrayLayout ||
            (flexible && (field.byteSize != 0 || field.byteAlignment == 0 || field.bitWidth ||
                          field.pointeeReadonly)) ||
            (!flexible && !experimentalCppFieldBytes(field.typeKey) && (!field.byteSize || !field.byteAlignment)) ||
            (!flexible && (field.byteSize == 0) != (field.byteAlignment == 0)) ||
            ((field.pointeeReadonly || field.pointeeVolatile || field.pointeeAtomic) &&
             !pointerCarrier(field.typeKey)))
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
    std::map<std::string, const CppImportedStaticField*> symbols;
    for (const auto& field : fields)
    {
        // T10-006: an imported enum or record, or a fixed array, has no scalar width; build() checks its type.
        const bool aggregate = (field.typeKey.starts_with("type:") && !field.typeKey.ends_with('*')) ||
            field.typeKey.ends_with(']');
        if (field.ownerIdentityKey.empty() || field.name.empty() || field.externalSymbol.empty() ||
            (!experimentalCppFieldBytes(field.typeKey) && !aggregate) ||
            (field.pointeeReadonly && !field.typeKey.ends_with('*')))
        { error = "invalid experimental C++ static field"; return false; }
        // T19-036: a verified shared symbol is the same storage under two owners. / El mismo almacenamiento.
        const auto [symbol, inserted] = symbols.emplace(field.externalSymbol, &field);
        // Item 9.8: a C++ mangled symbol names one entity; a derived record reaches its base's static member.
        //     / Un simbolo C++ decorado nombra una entidad; un derivado alcanza el miembro static de su base.
        const bool sharedSymbol = (symbol->second->sharedExternalSymbolVerified && field.sharedExternalSymbolVerified) ||
            field.externalSymbol.starts_with('?');
        if (!identities.emplace(field.ownerIdentityKey, field.name).second || (!inserted &&
            (!sharedSymbol ||
             symbol->second->ownerIdentityKey == field.ownerIdentityKey || symbol->second->name != field.name ||
             sharedCppTypeKeyShape(symbol->second->typeKey) != sharedCppTypeKeyShape(field.typeKey) ||
             symbol->second->isConst != field.isConst || symbol->second->isVolatile != field.isVolatile ||
             symbol->second->dllImport != field.dllImport || symbol->second->threadLocal != field.threadLocal ||
             symbol->second->tlsOnDemand != field.tlsOnDemand ||
             symbol->second->referenceStorage != field.referenceStorage)))
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
        // EN: T11-006. A by-value parameter is an imported enum or a trivial C++ record.
        // ES: T11-006. Un parametro por valor es un enum importado o un record C++ trivial.
        const bool valueParameters = std::all_of(imported.parameterTypeKeys.begin(), imported.parameterTypeKeys.end(),
            [&](const std::string& key) {
                if (!key.starts_with("type:") || key.ends_with('*') || key.ends_with(']')) return true;
                const auto type = nameLookup.typeOrdinalForIdentity(std::string_view(key).substr(5));
                const auto* value = type ? nameLookup.cppImportedType(*type) : nullptr;
                // X4-05: a NativeSharp struct mirrored for a C++ class template (`Box<P>(P x)`), as its methods
                //     already accept. / Un struct NativeSharp reflejado para una plantilla de clase C++.
                if (!value && nameLookup.isCppNativeMirrorType(std::string_view(key).substr(5))) return true;
                return value && ((value->enumBase && (value->origin == CppImportedTypeOrigin::CppEnum ||
                                                      value->origin == CppImportedTypeOrigin::CEnum)) ||
                                 (value->origin == CppImportedTypeOrigin::CppRecord &&
                                  nameLookup.cppTrivialValueEligible(*type)));
            });
        if (!valueParameters ||
            std::any_of(imported.parameterTypeKeys.begin(), imported.parameterTypeKeys.end(), [&](const auto& key) {
                return !cppCallableRecordIsVisible(key, imported.projectOrdinal, nameLookup);
            }))
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidType, diagnostics::DiagnosticSeverity::Error,
                SourceRange{}, "C++ constructor parameter type is not imported in its project");
            return false;
        }
        ClassMemberSymbol member;
        member.kind = imported.kind == CppImportedLifetimeKind::Constructor
            ? ClassMemberKind::Constructor : ClassMemberKind::Destructor;
        member.accessibility = imported.accessibility;
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
        member.cppFactoryConstructor = imported.factory; // X2-05
        member.signatureKey = member.name + "(";
        for (std::size_t i = 0; i < imported.parameterTypeKeys.size(); ++i)
        {
            MethodParameterSymbol parameter;
            parameter.name = "arg" + std::to_string(i);
            parameter.typeKey = imported.parameterTypeKeys[i];
            parameter.pointerPointeeReadonly = !imported.parameterPointeeReadonly.empty() && imported.parameterPointeeReadonly[i];
            parameter.passKind = imported.parameterPassKinds.empty() ? ParameterPassKind::Value
                                                                     : imported.parameterPassKinds[i];
            // T19-007: a C++ `T&&` parameter. / Un parametro C++ `T&&`.
            parameter.cppRvalueReference = i < imported.parameterRvalueReferences.size() &&
                imported.parameterRvalueReferences[i];
            parameter.cppConstReference = !parameter.cppRvalueReference &&
                parameter.passKind == ParameterPassKind::In; // X1-04
            if (parameter.passKind != ParameterPassKind::Value)
            {
                parameter.pointeeTypeKey = parameter.typeKey.substr(0, parameter.typeKey.size() - 1);
                parameter.pointerNullable = false;
                parameter.lifetimeName = "$p" + std::to_string(i);
            }
            if (i) member.signatureKey += ",";
            if (parameter.passKind == ParameterPassKind::Ref) member.signatureKey += "ref:";
            else if (parameter.passKind == ParameterPassKind::In) member.signatureKey += "in:";
            member.parameters.push_back(std::move(parameter));
            if (imported.parameterTypeKeys.size() == 1 && !imported.parameterPointeeReadonly.empty() && imported.parameterPointeeReadonly[i]) member.signatureKey += "const:";
            member.signatureKey += imported.parameterTypeKeys[i];
        }
        member.signatureKey += ")";
        // T19-006: as for a C++ method. / Como en un metodo C++.
        member.signatureKey += imported.cppSignatureSuffix;
        member.cppParameterSpellings = imported.cppParameterSpellings;
        member.cppDeclarationSpelling = imported.cppDeclarationSpelling;
        const auto index = members_.size();
        members_.push_back(std::move(member));
        membersByType_[*ordinal].push_back(index);
        membersByName_[*ordinal][members_[index].name].push_back(index);
        // X2-05: a factory constructor is reached only through its function, a hidden move constructor only by an
        //     implicit move. / Solo se alcanzan por su funcion o por un movimiento implicito.
        if (imported.kind == CppImportedLifetimeKind::Constructor && !imported.factory && !imported.implicitMoveOnly)
            constructorsByType_[*ordinal].push_back(index);
    }
    for (const auto& imported : experimentalCppInstanceMethods_)
    {
        const auto ordinal = nameLookup.typeOrdinalForIdentity(imported.ownerIdentityKey);
        const auto* owner = ordinal ? nameLookup.cppImportedType(*ordinal) : nullptr;
        // EN: T11-005. A type passed or returned by value is an imported enum or a trivial C++ record.
        // ES: T11-005. Un tipo pasado o devuelto por valor es un enum importado o un record C++ trivial.
        const auto valueTypeIsAllowed = [&](std::string_view key) {
            if (!key.starts_with("type:") || key.ends_with('*') || key.ends_with(']')) return true;
            const auto type = nameLookup.typeOrdinalForIdentity(key.substr(5));
            const auto* value = type ? nameLookup.cppImportedType(*type) : nullptr;
            if (!value && nameLookup.isCppNativeMirrorType(key.substr(5))) return true;
            return value && ((value->enumBase && (value->origin == CppImportedTypeOrigin::CppEnum ||
                                                  value->origin == CppImportedTypeOrigin::CEnum)) ||
                             (value->origin == CppImportedTypeOrigin::CppRecord &&
                              nameLookup.cppTrivialValueEligible(*type)));
        };
        if (!owner || owner->projectOrdinal != imported.projectOrdinal ||
            !cppCallableRecordIsVisible(imported.returnTypeKey, imported.projectOrdinal, nameLookup) ||
            (!imported.returnsReference && !imported.constructedResult && // X2-05: constructed in the destination
             !valueTypeIsAllowed(imported.returnTypeKey)) ||
            std::any_of(imported.parameterTypeKeys.begin(), imported.parameterTypeKeys.end(), [&](const auto& key) {
                return !cppCallableRecordIsVisible(key, imported.projectOrdinal, nameLookup) || !valueTypeIsAllowed(key);
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
        member.accessibility = imported.accessibility;
        member.declaringTypeOrdinal = ordinal;
        member.name = imported.name;
        member.cppDeleted = imported.isDeleted; // X1-03
        member.cppRvalueReceiver = imported.receiverRvalue; // X1-05
        member.declaredTypeKey = imported.returnTypeKey;
        member.declaredPointerPointeeReadonly = imported.returnPointeeReadonly;
        member.hasImplicitThis = true;
        member.declaringTypeIsStruct = true;
        member.isReadonly = imported.receiverConst;
        member.isCppExternal = true;
        member.cppExternalSymbol = imported.externalSymbol;
        member.cppDllImport = imported.dllImport;
        member.cppObjectSizeModes = imported.objectSizeModes;
        // EN: Item 11.1. A C++ `operator[]` arrives as `get_Item`: it is the indexer getter of the record.
        // ES: Item 11.1. Un `operator[]` de C++ llega como `get_Item`: es el getter de indexador del record.
        if (member.name == "get_Item" && imported.parameterTypeKeys.size() == 1)
        {
            member.isIndexerAccessor = true;
            member.isGetter = true;
        }
        // T11-005: a ref return of a C++ method is bound to the receiver, as for a NativeSharp method.
        member.isReferenceReturn = imported.returnsReference;
        member.cppReadonlyReferenceReturn = imported.returnsReadonlyReference;
        member.cppConstructedResult = imported.constructedResult; // X2-05
        if (imported.returnsReference) member.returnLifetimeName = "this";
        member.signatureKey = member.name + "(";
        for (std::size_t i = 0; i < imported.parameterTypeKeys.size(); ++i)
        {
            MethodParameterSymbol parameter;
            parameter.name = "arg" + std::to_string(i);
            parameter.typeKey = imported.parameterTypeKeys[i];
            parameter.pointerPointeeReadonly = !imported.parameterPointeeReadonly.empty() && imported.parameterPointeeReadonly[i];
            parameter.passKind = imported.parameterPassKinds.empty() ? ParameterPassKind::Value
                                                                     : imported.parameterPassKinds[i];
            // T19-007: a C++ `T&&` parameter. / Un parametro C++ `T&&`.
            parameter.cppRvalueReference = i < imported.parameterRvalueReferences.size() &&
                imported.parameterRvalueReferences[i];
            parameter.cppConstReference = !parameter.cppRvalueReference &&
                parameter.passKind == ParameterPassKind::In; // X1-04
            if (parameter.passKind != ParameterPassKind::Value)
            {
                parameter.pointeeTypeKey = parameter.typeKey.substr(0, parameter.typeKey.size() - 1);
                parameter.pointerNullable = false;
                parameter.lifetimeName = "$p" + std::to_string(i);
            }
            if (i) member.signatureKey += ",";
            if (parameter.passKind == ParameterPassKind::Ref) member.signatureKey += "ref:";
            else if (parameter.passKind == ParameterPassKind::In) member.signatureKey += "in:";
            member.parameters.push_back(std::move(parameter));
            if (imported.parameterTypeKeys.size() == 1 && !imported.parameterPointeeReadonly.empty() && imported.parameterPointeeReadonly[i]) member.signatureKey += "const:";
            member.signatureKey += imported.parameterTypeKeys[i];
        }
        member.signatureKey += ")";
        // T19-006: C++ overloads with equal NativeSharp types stay distinct. / Siguen distintas.
        member.signatureKey += imported.cppSignatureSuffix;
        member.cppParameterSpellings = imported.cppParameterSpellings;
        member.cppDeclarationSpelling = imported.cppDeclarationSpelling;
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
        // EN: T10-005. A type passed or returned by value is an imported enum or a trivial C++ record.
        // ES: T10-005. Un tipo pasado o devuelto por valor es un enum importado o un record C++ trivial.
        const auto valueTypeIsEnum = [&](std::string_view key) {
            if (!key.starts_with("type:") || key.ends_with('*') || key.ends_with(']')) return true;
            const auto type = nameLookup.typeOrdinalForIdentity(key.substr(5));
            const auto* value = type ? nameLookup.cppImportedType(*type) : nullptr;
            if (!value && nameLookup.isCppNativeMirrorType(key.substr(5))) return true;
            return value && ((value->enumBase && (value->origin == CppImportedTypeOrigin::CppEnum ||
                                                  value->origin == CppImportedTypeOrigin::CEnum)) ||
                             (value->origin == CppImportedTypeOrigin::CppRecord &&
                              nameLookup.cppTrivialValueEligible(*type)));
        };
        if (!owner || owner->projectOrdinal != imported.projectOrdinal ||
            !cppCallableRecordIsVisible(imported.returnTypeKey, imported.projectOrdinal, nameLookup) ||
            (!imported.constructedResult && !valueTypeIsEnum(imported.returnTypeKey)) || // X2-05
            std::any_of(imported.parameterTypeKeys.begin(), imported.parameterTypeKeys.end(), [&](const auto& key) {
                return !cppCallableRecordIsVisible(key, imported.projectOrdinal, nameLookup) || !valueTypeIsEnum(key);
            }))
        {
            diagnostics.report(diagnostics::DiagnosticId::InvalidType, diagnostics::DiagnosticSeverity::Error,
                SourceRange{}, "C++ static method owner or parameter type is not imported in its project");
            return false;
        }
        ClassMemberSymbol member;
        member.kind = ClassMemberKind::Method;
        member.accessibility = imported.accessibility;
        member.declaringTypeOrdinal = *ordinal;
        member.name = imported.name;
        member.declaredTypeKey = imported.returnTypeKey;
        member.declaredPointerPointeeReadonly = imported.returnPointeeReadonly;
        member.declaringTypeIsStruct = true;
        member.isStatic = true;
        member.isCppExternal = true;
        member.cppExternalSymbol = imported.externalSymbol;
        member.cppDllImport = imported.dllImport;
        member.cppObjectSizeModes = imported.objectSizeModes;
        // EN: T19-007. A reference result is a ref return, bound to the only ref or in parameter or else to static
        //     storage, as for a C++ free function (T10-003).
        // ES: T19-007. Un resultado por referencia es un retorno ref, ligado al unico parametro ref o in o, si no, a
        //     almacenamiento estatico, como en una funcion libre C++ (T10-003).
        member.isReferenceReturn = imported.returnsReference;
        member.cppReadonlyReferenceReturn = imported.returnsReadonlyReference;
        member.cppConstructedResult = imported.constructedResult; // X2-05
        std::vector<std::string> referenceInputs;
        member.signatureKey = member.name + "(";
        for (std::size_t i = 0; i < imported.parameterTypeKeys.size(); ++i)
        {
            MethodParameterSymbol parameter;
            parameter.name = "arg" + std::to_string(i);
            parameter.typeKey = imported.parameterTypeKeys[i];
            parameter.pointerPointeeReadonly = !imported.parameterPointeeReadonly.empty() && imported.parameterPointeeReadonly[i];
            parameter.passKind = imported.parameterPassKinds.empty() ? ParameterPassKind::Value
                                                                     : imported.parameterPassKinds[i];
            // T19-007: a C++ `T&&` parameter. / Un parametro C++ `T&&`.
            parameter.cppRvalueReference = i < imported.parameterRvalueReferences.size() &&
                imported.parameterRvalueReferences[i];
            parameter.cppConstReference = !parameter.cppRvalueReference &&
                parameter.passKind == ParameterPassKind::In; // X1-04
            if (parameter.passKind != ParameterPassKind::Value)
            {
                parameter.pointeeTypeKey = parameter.typeKey.substr(0, parameter.typeKey.size() - 1);
                parameter.pointerNullable = false;
                parameter.lifetimeName = "$p" + std::to_string(i);
                referenceInputs.push_back(parameter.lifetimeName);
            }
            if (i) member.signatureKey += ",";
            if (parameter.passKind == ParameterPassKind::Ref) member.signatureKey += "ref:";
            else if (parameter.passKind == ParameterPassKind::In) member.signatureKey += "in:";
            member.parameters.push_back(std::move(parameter));
            if (imported.parameterTypeKeys.size() == 1 && !imported.parameterPointeeReadonly.empty() && imported.parameterPointeeReadonly[i]) member.signatureKey += "const:";
            member.signatureKey += imported.parameterTypeKeys[i];
        }
        member.signatureKey += ")";
        if (imported.returnsReference)
            member.returnLifetimeName = referenceInputs.size() == 1 ? referenceInputs.front() : "static";
        // T19-006: C++ overloads with equal NativeSharp types stay distinct. / Siguen distintas.
        member.signatureKey += imported.cppSignatureSuffix;
        member.cppParameterSpellings = imported.cppParameterSpellings;
        member.cppDeclarationSpelling = imported.cppDeclarationSpelling;
        // X2: a C++ conversion operator is an explicit user conversion. / Una conversion explicita de usuario.
        if (imported.name == "op_Explicit" && imported.cppSignatureSuffix.starts_with("#conversion:"))
        {
            member.isUserConversion = true;
            member.isUserOperator = true;
        }
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
            (imported.bitOffset && (width == 0 || width > 16 || (width & (width - 1)) != 0 ||
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
        member.accessibility = imported.accessibility;
        member.declaringTypeOrdinal = *ordinal;
        member.name = imported.name;
        member.declaredTypeKey = imported.typeKey;
        member.declaredPointerPointeeReadonly = imported.pointeeReadonly;
        member.declaredPointerPointeeVolatile = imported.pointeeVolatile;
        member.declaredPointerLevelQualifiers = imported.pointerLevelQualifiers;
        member.cppAtomicStorage = imported.atomicStorage;
        member.cppDeclaredPointerPointeeAtomic = imported.pointeeAtomic;
        member.isReadonly = imported.isConst;
        member.isVolatile = imported.isVolatile;
        member.isCppExternal = true;
        member.cppMicrosoftVolatileSemantics = imported.microsoftVolatileSemantics;
        member.cppArrayElementStrides = imported.cppArrayElementStrides;
        member.cppArrayPhysicalLayouts = imported.cppArrayPhysicalLayouts;
        member.isCppAnonymousCarrier = imported.isAnonymousCarrier;
        member.isCompilerGenerated = imported.isAnonymousCarrier;
        member.cppClangUsr = imported.clangUsr;
        member.explicitFieldOffset = imported.byteOffset;
        member.cppVolatileUnitOffset = imported.volatileUnitByteOffset;
        member.cppVolatileUnitBitOffset = imported.volatileUnitBitOffset;
        member.cppVolatileUnitBytes = imported.volatileUnitBytes;
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
        member.accessibility = imported.accessibility;
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
        member.cppThreadLocal = imported.threadLocal;
        member.cppTlsOnDemand = imported.tlsOnDemand;
        member.cppReferenceStorage = imported.referenceStorage;
        const auto memberOrdinal = members_.size();
        members_.push_back(std::move(member));
        membersByType_[*ordinal].push_back(memberOrdinal);
        membersByName_[*ordinal][imported.name].push_back(memberOrdinal);
    }
    sharedStaticDerivedByType_.assign(nameLookup.namedTypes().size(), {});
    std::optional<std::size_t> objectRoot;
    for (std::size_t ordinal = 0; ordinal < nameLookup.namedTypes().size(); ++ordinal)
    {
        const auto& type = nameLookup.namedTypes()[ordinal];
        if (type.kind == NamedTypeDeclarationKind::Class &&
            type.identity.fullyQualifiedName() == "NativeSharp.Core.Object")
        {
            objectRoot = ordinal;
            break;
        }
    }
    for (std::size_t ordinal = 0; ordinal < nameLookup.namedTypes().size(); ++ordinal)
    {
        const auto& type = nameLookup.namedTypes()[ordinal];
        if (type.kind != NamedTypeDeclarationKind::Class &&
            type.kind != NamedTypeDeclarationKind::Interface) continue;
        bool hasClassBase = false;
        const auto addBase = [&](const ast::TypeSyntax* syntax)
        {
            const auto base = typeOrdinalForKey(canonicalTypeKey(syntax, nameLookup));
            if (!base.has_value() || *base == ordinal || *base >= sharedStaticDerivedByType_.size()) return;
            const auto kind = nameLookup.namedTypes()[*base].kind;
            if (kind != NamedTypeDeclarationKind::Class && kind != NamedTypeDeclarationKind::Interface) return;
            sharedStaticDerivedByType_[*base].push_back(ordinal);
            hasClassBase = hasClassBase || kind == NamedTypeDeclarationKind::Class;
        };
        for (const auto& part : type.parts)
        {
            if (part.syntax == nullptr) continue;
            if (!part.syntax->typeSyntaxes().empty())
                for (const ast::TypeSyntax* base : part.syntax->typeSyntaxes()) addBase(base);
            else if (part.syntax->typeSyntax() != nullptr) addBase(part.syntax->typeSyntax());
        }
        if (type.kind == NamedTypeDeclarationKind::Class && !hasClassBase && objectRoot.has_value() &&
            *objectRoot != ordinal)
            sharedStaticDerivedByType_[*objectRoot].push_back(ordinal);
    }
    for (const ClassMemberSymbol& member : members_)
    {
        if (member.kind != ClassMemberKind::Field || !member.isStatic ||
            !typeContainsThreadLocalSharedPointer(member.declaredTypeKey)) continue;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            member.range,
            "shared static storage cannot contain a thread-local shared pointer (Rc or WeakRc)");
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
