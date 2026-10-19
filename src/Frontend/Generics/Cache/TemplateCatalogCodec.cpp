#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "../../Internal/CloneDepthGuard.h"
#include "../../Internal/ByteReader.h"
#include "../../Internal/EnumConstResolver.h"
#include "../../Internal/SpecializationWaveTimer.h"
#include "../../Internal/InterfaceFingerprint.h"
#include "../../Internal/ImplementationFingerprint.h"
#include "../../Internal/LocatedNode.h"
#include "../../Internal/MirFunctionKey.h"
#include "../../Internal/MirCacheRecord.h"
#include "../../Internal/ProjectGenericDependencyFrame.h"
#include "../../Internal/ProjectGenericBodyFrame.h"
#include "../../Internal/ProjectGenericAliasFrame.h"
#include "../../Internal/ProjectGenericChildWork.h"
#include "../../Internal/ProjectGenericCollectFrame.h"
#include "../../Internal/ProjectGenericProcessFrame.h"
#include "../../Internal/ProjectGenericReservedDemand.h"
#include "../../Internal/TypeWork.h"
#include "nativesharp/Frontend/ProjectOpaqueResultModel.h"

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "TemplateCatalogCodec.h"
namespace nativesharp::frontend::internal
{
// NSC305-C01. SHA-256 of a template's source text (reconstructed from the lexer tokens), the C1
// cache key input that invalidates on any source change (including the body).
std::string computeTemplateSourceFingerprint(
    const ProjectSyntaxUnit& unit, const ast::AstNode* node)
{
    std::string canonical;
    const std::uint32_t begin = node->range().begin().offset();
    const std::uint32_t end = node->range().end().offset();
    for (const lexer::Token& token : unit.tokens())
    {
        const std::uint32_t tokenBegin = token.range().begin().offset();
        const std::uint32_t tokenEnd = token.range().end().offset();
        if (tokenBegin >= begin && tokenEnd <= end)
        {
            canonical += token.lexeme();
            canonical.push_back(' ');
        }
    }
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(canonical.data()), canonical.size()};
    return basic::sha256Hex(basic::sha256(bytes));
}

// NSC305-C01. The C1 cache schema version (bump on any format change).
static constexpr std::string_view templateCacheSchema = "nsc-c1-template-v1";

// NSC305-C01. The full C1 cache key: SHA-256 of (source fingerprint + schema version).
std::string templateCacheKey(std::string_view sourceFingerprint)
{
    std::string material{sourceFingerprint};
    material.push_back('|');
    material += templateCacheSchema;
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size()};
    return basic::sha256Hex(basic::sha256(bytes));
}

// NSC305-C01. Compact, relocatable (no raw pointers) serialization of the scalar/string metadata of
// a GenericTemplateRecord -- the C1 cache payload skeleton. The AST subtree (syntax/extensionSyntaxes,
// parameterSyntaxes, constraints, externalTypeReferences) is intentionally not covered here; it is
// the next layer of the codec.
static void appendByte(std::string& out, std::uint8_t value)
{
    out.push_back(static_cast<char>(value));
}

static void appendU32(std::string& out, std::uint32_t value)
{
    out.push_back(static_cast<char>(value & 0xFFu));
    out.push_back(static_cast<char>((value >> 8U) & 0xFFu));
    out.push_back(static_cast<char>((value >> 16U) & 0xFFu));
    out.push_back(static_cast<char>((value >> 24U) & 0xFFu));
}

static void appendU64(std::string& out, std::uint64_t value)
{
    for (unsigned shift = 0; shift < 64U; shift += 8U)
        out.push_back(static_cast<char>((value >> shift) & 0xFFu));
}

static void appendString(std::string& out, std::string_view value)
{
    appendU32(out, static_cast<std::uint32_t>(value.size()));
    out.append(value);
}

static void appendStringVector(std::string& out, const std::vector<std::string>& values)
{
    appendU32(out, static_cast<std::uint32_t>(values.size()));
    for (const std::string& value : values) appendString(out, value);
}


[[maybe_unused]] static std::string serializeTemplateRecord(const GenericTemplateRecord& record)
{
    std::string out;
    appendByte(out, static_cast<std::uint8_t>(record.kind));
    appendByte(out, static_cast<std::uint8_t>(record.accessibility));
    appendU64(out, record.projectOrdinal);
    appendU64(out, record.sourceOrdinal);
    appendString(out, record.moduleName);
    appendString(out, record.namespaceName);
    appendString(out, record.ownerName);
    appendString(out, record.name);
    appendString(out, record.qualifiedTypeName);
    appendStringVector(out, record.parameters);
    appendString(out, record.canonicalKey);
    appendString(out, record.specializationFamilyKey);
    appendByte(out, record.isControlledSpecialization ? 1U : 0U);
    appendByte(out, record.isControlledSpecializationFamily ? 1U : 0U);
    appendStringVector(out, record.dependencies);
    appendStringVector(out, record.unresolvedDependencies);
    appendString(out, record.sourceFingerprint);
    appendString(out, record.c1CacheKey);
    return out;
}

// NSC305-C01. ByteReader-based record metadata read (shared by deserializeTemplateRecord and the
// C02 catalog restore).
[[maybe_unused]] static bool readTemplateRecord(ByteReader& reader, GenericTemplateRecord& record)
{
    std::uint8_t kind = 0;
    std::uint8_t accessibility = 0;
    std::uint64_t projectOrdinal = 0;
    std::uint64_t sourceOrdinal = 0;
    std::uint8_t controlled = 0;
    std::uint8_t controlledFamily = 0;
    if (!reader.byte(kind) || !reader.byte(accessibility) ||
        !reader.u64(projectOrdinal) || !reader.u64(sourceOrdinal) ||
        !reader.string(record.moduleName) || !reader.string(record.namespaceName) ||
        !reader.string(record.ownerName) || !reader.string(record.name) ||
        !reader.string(record.qualifiedTypeName) ||
        !reader.stringVector(record.parameters) || !reader.string(record.canonicalKey) ||
        !reader.string(record.specializationFamilyKey) ||
        !reader.byte(controlled) || !reader.byte(controlledFamily) ||
        !reader.stringVector(record.dependencies) ||
        !reader.stringVector(record.unresolvedDependencies) ||
        !reader.string(record.sourceFingerprint) || !reader.string(record.c1CacheKey))
        return false;
    record.kind = static_cast<GenericTemplateKind>(kind);
    record.accessibility = static_cast<GenericTemplateAccessibility>(accessibility);
    record.projectOrdinal = static_cast<std::size_t>(projectOrdinal);
    record.sourceOrdinal = static_cast<std::size_t>(sourceOrdinal);
    record.isControlledSpecialization = controlled != 0U;
    record.isControlledSpecializationFamily = controlledFamily != 0U;
    return true;
}

[[maybe_unused]] static bool deserializeTemplateRecord(std::string_view bytes, GenericTemplateRecord& record)
{
    ByteReader reader(bytes);
    return readTemplateRecord(reader, record);
}

// NSC305-C01. Scalar/string fields of a TypeSyntax (no pointers, no recursion): the base identity
// plus the source flags/origin tags. The type-argument/return/error/modifier graph is the next layer.
[[maybe_unused]] std::string serializeTypeSyntaxScalars(const ast::TypeSyntax& type)
{
    std::string out;
    appendU32(out, type.range().begin().offset());
    appendU32(out, type.range().end().offset());
    appendU32(out, static_cast<std::uint32_t>(type.baseTokenKind()));
    appendString(out, type.baseName());
    appendByte(out, static_cast<std::uint8_t>(type.ownershipQualifier()));
    appendByte(out, static_cast<std::uint8_t>(type.referenceQualifier()));
    appendString(out, type.lifetimeName());
    appendByte(out, type.isDynamicTrait() ? 1U : 0U);
    appendByte(out, type.isOpaqueResult() ? 1U : 0U);
    appendString(out, type.opaqueResultTraitName());
    appendByte(out, type.isExplicitTraitImplementation() ? 1U : 0U);
    appendByte(out, type.isPointeeConst() ? 1U : 0U);
    appendByte(out, type.isTuple() ? 1U : 0U);
    appendByte(out, type.isFunction() ? 1U : 0U);
    appendByte(out, type.isPackExpansion() ? 1U : 0U);
    appendByte(out, static_cast<std::uint8_t>(type.functionCarrier()));
    appendByte(out, static_cast<std::uint8_t>(type.functionCallingConvention()));
    appendByte(out, type.functionIsVariadic() ? 1U : 0U);
    appendByte(out, type.functionIsUnsafe() ? 1U : 0U);
    appendByte(out, type.functionIsNoHeap() ? 1U : 0U);
    appendByte(out, type.functionIsAsync() ? 1U : 0U);
    appendByte(out, type.functionIsNullable() ? 1U : 0U);
    appendString(out, type.functionLifetimeName());
    appendStringVector(out, type.tupleElementNames());
    appendByte(out, type.originProjectOrdinal().has_value() ? 1U : 0U);
    if (type.originProjectOrdinal().has_value())
        appendU64(out, type.originProjectOrdinal().value());
    appendByte(out, type.originNamespace().has_value() ? 1U : 0U);
    if (type.originNamespace().has_value())
        appendString(out, type.originNamespace().value());
    return out;
}

// NSC305-C01. Deserializes the scalar/string fields written by serializeTypeSyntaxScalars. The
// SourceRange's FileId is left default (invalid); the caller restores it from the owning unit.
static bool readTypeSyntaxHeader(
    ByteReader& reader,
    std::uint32_t& begin,
    std::uint32_t& end,
    std::uint32_t& tokenKind,
    std::string& baseName)
{
    return reader.u32(begin) && reader.u32(end) && reader.u32(tokenKind) &&
        reader.string(baseName);
}

static bool readTypeSyntaxQualifiers(
    ByteReader& reader,
    std::uint8_t& ownership,
    std::uint8_t& reference,
    std::string& lifetimeName,
    std::uint8_t& dynamicTrait,
    std::uint8_t& opaqueResult,
    std::string& opaqueResultTraitName,
    std::uint8_t& explicitTrait,
    std::uint8_t& pointeeConst,
    std::uint8_t& tuple,
    std::uint8_t& function,
    std::uint8_t& pack)
{
    return reader.byte(ownership) && reader.byte(reference) && reader.string(lifetimeName) &&
        reader.byte(dynamicTrait) && reader.byte(opaqueResult) &&
        reader.string(opaqueResultTraitName) && reader.byte(explicitTrait) &&
        reader.byte(pointeeConst) && reader.byte(tuple) && reader.byte(function) &&
        reader.byte(pack);
}

static bool readTypeSyntaxFunction(
    ByteReader& reader,
    std::uint8_t& carrier,
    std::uint8_t& convention,
    std::uint8_t& variadic,
    std::uint8_t& unsafe,
    std::uint8_t& noHeap,
    std::uint8_t& async,
    std::uint8_t& nullable,
    std::string& functionLifetimeName,
    std::vector<std::string>& tupleNames)
{
    return reader.byte(carrier) && reader.byte(convention) && reader.byte(variadic) &&
        reader.byte(unsafe) && reader.byte(noHeap) && reader.byte(async) &&
        reader.byte(nullable) && reader.string(functionLifetimeName) &&
        reader.stringVector(tupleNames);
}

static bool readTypeSyntaxOrigin(
    ByteReader& reader,
    std::uint8_t& hasProject,
    std::uint64_t& project,
    std::uint8_t& hasNamespace,
    std::string& originNamespace)
{
    return reader.byte(hasProject) && (hasProject == 0U || reader.u64(project)) &&
        reader.byte(hasNamespace) &&
        (hasNamespace == 0U || reader.string(originNamespace));
}

static void applyTypeSyntaxQualifiers(
    ast::TypeSyntax& type,
    std::uint8_t ownership,
    std::uint8_t reference,
    std::string lifetimeName,
    std::uint8_t dynamicTrait,
    std::uint8_t opaqueResult,
    std::string opaqueResultTraitName,
    std::uint8_t explicitTrait,
    std::uint8_t pointeeConst,
    std::uint8_t tuple,
    std::uint8_t function,
    std::uint8_t pack)
{
    type.setOwnershipQualifier(static_cast<ast::OwnershipQualifierSyntaxKind>(ownership));
    type.setReferenceQualifier(static_cast<ast::ReferenceQualifierSyntaxKind>(reference));
    type.setLifetimeName(std::move(lifetimeName));
    type.setDynamicTrait(dynamicTrait != 0U);
    type.setOpaqueResult(opaqueResult != 0U);
    type.setOpaqueResultTraitName(std::move(opaqueResultTraitName));
    type.setExplicitTraitImplementation(explicitTrait != 0U);
    type.setPointeeConst(pointeeConst != 0U);
    type.setTuple(tuple != 0U);
    type.setFunction(function != 0U);
    type.setPackExpansion(pack != 0U);
}

static void applyTypeSyntaxFunction(
    ast::TypeSyntax& type,
    std::uint8_t carrier,
    std::uint8_t convention,
    std::uint8_t variadic,
    std::uint8_t unsafe,
    std::uint8_t noHeap,
    std::uint8_t async,
    std::uint8_t nullable,
    std::string functionLifetimeName)
{
    type.setFunctionCarrier(static_cast<ast::FunctionTypeCarrierSyntaxKind>(carrier));
    type.setFunctionCallingConvention(
        static_cast<ast::FunctionCallingConventionSyntaxKind>(convention));
    type.setFunctionVariadic(variadic != 0U);
    type.setFunctionUnsafe(unsafe != 0U);
    type.setFunctionNoHeap(noHeap != 0U);
    type.setFunctionAsync(async != 0U);
    type.setFunctionNullable(nullable != 0U);
    type.setFunctionLifetimeName(std::move(functionLifetimeName));
}

static void applyTypeSyntaxOrigin(
    ast::TypeSyntax& type,
    std::uint8_t hasProject,
    std::uint64_t project,
    std::uint8_t hasNamespace,
    std::string originNamespace)
{
    if (hasProject != 0U) type.setOriginProjectOrdinal(static_cast<std::size_t>(project));
    if (hasNamespace != 0U) type.setOriginNamespace(std::move(originNamespace));
}

[[maybe_unused]] static bool deserializeTypeSyntaxScalars(
    std::string_view bytes, ast::TypeSyntax& type)
{
    ByteReader reader(bytes);
    std::uint32_t begin = 0, end = 0, tokenKind = 0;
    std::string baseName, lifetimeName, opaqueResultTraitName, functionLifetimeName;
    std::uint8_t ownership = 0, reference = 0, dynamicTrait = 0, opaqueResult = 0;
    std::uint8_t explicitTrait = 0, pointeeConst = 0, tuple = 0, function = 0, pack = 0;
    std::uint8_t carrier = 0, convention = 0, variadic = 0, unsafe = 0, noHeap = 0;
    std::uint8_t async = 0, nullable = 0, hasProject = 0, hasNamespace = 0;
    std::vector<std::string> tupleNames;
    std::uint64_t project = 0;
    std::string originNamespace;
    if (!readTypeSyntaxHeader(reader, begin, end, tokenKind, baseName) ||
        !readTypeSyntaxQualifiers(reader, ownership, reference, lifetimeName, dynamicTrait,
            opaqueResult, opaqueResultTraitName, explicitTrait, pointeeConst, tuple, function,
            pack) ||
        !readTypeSyntaxFunction(reader, carrier, convention, variadic, unsafe, noHeap, async,
            nullable, functionLifetimeName, tupleNames) ||
        !readTypeSyntaxOrigin(reader, hasProject, project, hasNamespace, originNamespace))
        return false;
    type.setRange(SourceRange{SourceLocation{FileId{}, begin}, SourceLocation{FileId{}, end}});
    type.setBaseTokenKind(static_cast<lexer::TokenKind>(tokenKind));
    type.setBaseName(std::move(baseName));
    applyTypeSyntaxQualifiers(type, ownership, reference, std::move(lifetimeName), dynamicTrait,
        opaqueResult, std::move(opaqueResultTraitName), explicitTrait, pointeeConst, tuple,
        function, pack);
    applyTypeSyntaxFunction(type, carrier, convention, variadic, unsafe, noHeap, async, nullable,
        std::move(functionLifetimeName));
    // Tuple element names are restored by the recursive layer alongside the elements.
    (void)tupleNames;
    applyTypeSyntaxOrigin(type, hasProject, project, hasNamespace, std::move(originNamespace));
    return true;
}

// NSC305-C01. Recursive (tree) serialization of a TypeSyntax: the scalar fields plus the ordered
// type arguments and the function return/error types. Modifiers (arrayCount expressions) and
// constExpression are the AstNode layer.
[[maybe_unused]] std::string serializeTypeSyntaxTree(const ast::TypeSyntax* type)
{
    if (type == nullptr) return {};
    std::string out = serializeTypeSyntaxScalars(*type);
    appendU32(out, static_cast<std::uint32_t>(type->typeArguments().size()));
    for (const ast::TypeSyntax* argument : type->typeArguments())
        out += serializeTypeSyntaxTree(argument);
    const bool hasReturn = type->functionReturnType() != nullptr;
    const bool hasError = type->functionErrorType() != nullptr;
    appendByte(out, hasReturn ? 1U : 0U);
    appendByte(out, hasError ? 1U : 0U);
    if (hasReturn) out += serializeTypeSyntaxTree(type->functionReturnType());
    if (hasError) out += serializeTypeSyntaxTree(type->functionErrorType());
    return out;
}

// NSC305-C01. Recursive deserialization of a TypeSyntax tree into `arena` (inverse of
// serializeTypeSyntaxTree). The SourceRange's FileId is left default (invalid); the caller restores
// it from the owning unit.
static ast::TypeSyntax* deserializeTypeSyntaxTree(ast::AstArena& arena, ByteReader& reader);

static bool deserializeTypeSyntaxArguments(
    ast::AstArena& arena,
    ByteReader& reader,
    ast::TypeSyntax& type,
    bool tuple,
    const std::vector<std::string>& tupleNames)
{
    std::uint32_t argumentCount = 0;
    if (!reader.u32(argumentCount)) return false;
    for (std::uint32_t index = 0; index < argumentCount; ++index)
    {
        ast::TypeSyntax* argument = deserializeTypeSyntaxTree(arena, reader);
        if (argument == nullptr) return false;
        if (tuple && index < tupleNames.size())
            type.addTupleElement(argument, tupleNames[index]);
        else
            type.addTypeArgument(argument);
    }
    return true;
}

static bool deserializeTypeSyntaxFunctionTypes(
    ast::AstArena& arena,
    ByteReader& reader,
    ast::TypeSyntax& type)
{
    std::uint8_t hasReturn = 0, hasError = 0;
    if (!reader.byte(hasReturn) || !reader.byte(hasError)) return false;
    if (hasReturn != 0U)
    {
        ast::TypeSyntax* returnType = deserializeTypeSyntaxTree(arena, reader);
        if (returnType == nullptr) return false;
        type.setFunctionReturnType(returnType);
    }
    if (hasError != 0U)
    {
        ast::TypeSyntax* errorType = deserializeTypeSyntaxTree(arena, reader);
        if (errorType == nullptr) return false;
        type.setFunctionErrorType(errorType);
    }
    return true;
}

[[maybe_unused]] static ast::TypeSyntax* deserializeTypeSyntaxTree(
    ast::AstArena& arena, ByteReader& reader)
{
    std::uint32_t begin = 0, end = 0, tokenKind = 0;
    std::string baseName;
    if (!readTypeSyntaxHeader(reader, begin, end, tokenKind, baseName))
        return nullptr;
    ast::TypeSyntax* type = arena.make<ast::TypeSyntax>(
        SourceRange{SourceLocation{FileId{}, begin}, SourceLocation{FileId{}, end}},
        static_cast<lexer::TokenKind>(tokenKind), std::move(baseName));
    std::string lifetimeName, opaqueResultTraitName, functionLifetimeName;
    std::uint8_t ownership = 0, reference = 0, dynamicTrait = 0, opaqueResult = 0;
    std::uint8_t explicitTrait = 0, pointeeConst = 0, tuple = 0, function = 0, pack = 0;
    std::uint8_t carrier = 0, convention = 0, variadic = 0, unsafe = 0, noHeap = 0;
    std::uint8_t async = 0, nullable = 0, hasProject = 0, hasNamespace = 0;
    std::vector<std::string> tupleNames;
    std::uint64_t project = 0;
    std::string originNamespace;
    if (!readTypeSyntaxQualifiers(reader, ownership, reference, lifetimeName, dynamicTrait,
            opaqueResult, opaqueResultTraitName, explicitTrait, pointeeConst, tuple, function,
            pack) ||
        !readTypeSyntaxFunction(reader, carrier, convention, variadic, unsafe, noHeap, async,
            nullable, functionLifetimeName, tupleNames) ||
        !readTypeSyntaxOrigin(reader, hasProject, project, hasNamespace, originNamespace))
        return nullptr;
    applyTypeSyntaxQualifiers(*type, ownership, reference, std::move(lifetimeName), dynamicTrait,
        opaqueResult, std::move(opaqueResultTraitName), explicitTrait, pointeeConst, tuple,
        function, pack);
    applyTypeSyntaxFunction(*type, carrier, convention, variadic, unsafe, noHeap, async, nullable,
        std::move(functionLifetimeName));
    applyTypeSyntaxOrigin(*type, hasProject, project, hasNamespace, std::move(originNamespace));
    if (!deserializeTypeSyntaxArguments(arena, reader, *type, tuple != 0U, tupleNames) ||
        !deserializeTypeSyntaxFunctionTypes(arena, reader, *type))
        return nullptr;
    return type;
}

// NSC305-C01. AstNode scalar/string fields (no pointers, no children, no type syntaxes): the node
// identity and flags. The child/typeSyntax graph is the next layer.
[[maybe_unused]] std::string serializeAstNodeScalars(const ast::AstNode& node)
{
    std::string out;
    appendU32(out, static_cast<std::uint32_t>(node.kind()));
    appendU32(out, node.range().begin().offset());
    appendU32(out, node.range().end().offset());
    appendString(out, node.text());
    appendString(out, node.auxiliaryText());
    appendString(out, node.argumentName());
    appendString(out, node.allocatorFailureProfile());
    appendU32(out, static_cast<std::uint32_t>(node.tokenKind()));
    appendU32(out, static_cast<std::uint32_t>(node.modifiers().size()));
    for (const lexer::TokenKind modifier : node.modifiers())
        appendU32(out, static_cast<std::uint32_t>(modifier));
    appendStringVector(out, node.genericParameters());
    appendString(out, node.lifetimeName());
    appendByte(out, node.flag() ? 1U : 0U);
    appendByte(out, node.isReference() ? 1U : 0U);
    appendByte(out, node.isVariadic() ? 1U : 0U);
    appendByte(out, node.isUnsafe() ? 1U : 0U);
    appendByte(out, static_cast<std::uint8_t>(node.floatingPointMode()));
    appendByte(out, static_cast<std::uint8_t>(node.ffiUnwindPolicy()));
    appendU64(out, node.integerValue());
    std::uint64_t floatingBits = 0;
    const double floatingValue = node.floatingValue();
    std::memcpy(&floatingBits, &floatingValue, sizeof(floatingBits));
    appendU64(out, floatingBits);
    return out;
}

// NSC305-C01. Inverse of serializeAstNodeScalars (allocates the node into `arena`).
[[maybe_unused]] ast::AstNode* deserializeAstNodeScalars(
    ast::AstArena& arena, ByteReader& reader)
{
    std::uint32_t kind = 0;
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    if (!reader.u32(kind) || !reader.u32(begin) || !reader.u32(end)) return nullptr;
    ast::AstNode* node = arena.make<ast::AstNode>(
        static_cast<ast::AstNodeKind>(kind),
        SourceRange{SourceLocation{FileId{}, begin}, SourceLocation{FileId{}, end}});
    std::string text;
    std::string auxiliaryText;
    std::string argumentName;
    std::string allocatorFailureProfile;
    std::uint32_t tokenKind = 0;
    std::uint32_t modifierCount = 0;
    std::string lifetimeName;
    std::uint8_t flag = 0;
    std::uint8_t reference = 0;
    std::uint8_t variadic = 0;
    std::uint8_t unsafe = 0;
    std::uint8_t floatingPointMode = 0;
    std::uint8_t ffiUnwind = 0;
    std::uint64_t integerValue = 0;
    std::uint64_t floatingBits = 0;
    std::vector<std::string> genericParameters;
    if (!reader.string(text) || !reader.string(auxiliaryText) ||
        !reader.string(argumentName) || !reader.string(allocatorFailureProfile) ||
        !reader.u32(tokenKind) || !reader.u32(modifierCount))
        return nullptr;
    node->setText(std::move(text));
    node->setAuxiliaryText(std::move(auxiliaryText));
    node->setArgumentName(std::move(argumentName));
    node->setAllocatorFailureProfile(std::move(allocatorFailureProfile));
    node->setTokenKind(static_cast<lexer::TokenKind>(tokenKind));
    for (std::uint32_t index = 0; index < modifierCount; ++index)
    {
        std::uint32_t modifier = 0;
        if (!reader.u32(modifier)) return nullptr;
        node->addModifier(static_cast<lexer::TokenKind>(modifier));
    }
    if (!reader.stringVector(genericParameters) || !reader.string(lifetimeName) ||
        !reader.byte(flag) || !reader.byte(reference) || !reader.byte(variadic) ||
        !reader.byte(unsafe) || !reader.byte(floatingPointMode) || !reader.byte(ffiUnwind) ||
        !reader.u64(integerValue) || !reader.u64(floatingBits))
        return nullptr;
    for (const std::string& parameter : genericParameters)
        node->addGenericParameter(parameter);
    node->setLifetimeName(std::move(lifetimeName));
    node->setFlag(flag != 0U);
    node->setReference(reference != 0U);
    node->setVariadic(variadic != 0U);
    node->setUnsafe(unsafe != 0U);
    node->setFloatingPointMode(static_cast<ast::FloatingPointModeSyntaxKind>(floatingPointMode));
    node->setFfiUnwindPolicy(static_cast<ast::FfiUnwindSyntaxKind>(ffiUnwind));
    node->setIntegerValue(integerValue);
    double floatingValue = 0.0;
    std::memcpy(&floatingValue, &floatingBits, sizeof(floatingValue));
    node->setFloatingValue(floatingValue);
    return node;
}

// NSC305-C01. Recursive (tree) serialization of an AstNode: scalars + the typeSyntax/throwsType/
// typeSyntaxes graph + first/second/third + children. Nested generic parameter/constraint/lifetime
// structs are the next layer.
[[maybe_unused]] std::string serializeAstNodeTree(const ast::AstNode* node)
{
    if (node == nullptr) return {};
    std::string out = serializeAstNodeScalars(*node);
    const ast::TypeSyntax* typeSyntax = node->typeSyntax();
    const ast::TypeSyntax* throwsType = node->throwsTypeSyntax();
    appendByte(out, typeSyntax != nullptr ? 1U : 0U);
    appendByte(out, throwsType != nullptr ? 1U : 0U);
    if (typeSyntax != nullptr) out += serializeTypeSyntaxTree(typeSyntax);
    if (throwsType != nullptr) out += serializeTypeSyntaxTree(throwsType);
    appendU32(out, static_cast<std::uint32_t>(node->typeSyntaxes().size()));
    for (const ast::TypeSyntax* syntax : node->typeSyntaxes())
        out += serializeTypeSyntaxTree(syntax);
    const ast::AstNode* first = node->first();
    const ast::AstNode* second = node->second();
    const ast::AstNode* third = node->third();
    appendByte(out, first != nullptr ? 1U : 0U);
    appendByte(out, second != nullptr ? 1U : 0U);
    appendByte(out, third != nullptr ? 1U : 0U);
    if (first != nullptr) out += serializeAstNodeTree(first);
    if (second != nullptr) out += serializeAstNodeTree(second);
    if (third != nullptr) out += serializeAstNodeTree(third);
    appendU32(out, static_cast<std::uint32_t>(node->children().size()));
    for (const ast::AstNode* child : node->children())
        out += serializeAstNodeTree(child);
    return out;
}

// NSC305-C01. Inverse of serializeAstNodeTree (allocates the tree into `arena`).
[[maybe_unused]] ast::AstNode* deserializeAstNodeTree(
    ast::AstArena& arena, ByteReader& reader)
{
    ast::AstNode* node = deserializeAstNodeScalars(arena, reader);
    if (node == nullptr) return nullptr;
    std::uint8_t hasTypeSyntax = 0;
    std::uint8_t hasThrows = 0;
    if (!reader.byte(hasTypeSyntax) || !reader.byte(hasThrows)) return nullptr;
    if (hasTypeSyntax != 0U)
    {
        ast::TypeSyntax* type = deserializeTypeSyntaxTree(arena, reader);
        if (type == nullptr) return nullptr;
        node->setTypeSyntax(type);
    }
    if (hasThrows != 0U)
    {
        ast::TypeSyntax* type = deserializeTypeSyntaxTree(arena, reader);
        if (type == nullptr) return nullptr;
        node->setThrowsTypeSyntax(type);
    }
    std::uint32_t typeSyntaxCount = 0;
    if (!reader.u32(typeSyntaxCount)) return nullptr;
    for (std::uint32_t index = 0; index < typeSyntaxCount; ++index)
    {
        ast::TypeSyntax* type = deserializeTypeSyntaxTree(arena, reader);
        if (type == nullptr) return nullptr;
        node->addTypeSyntax(type);
    }
    std::uint8_t hasFirst = 0;
    std::uint8_t hasSecond = 0;
    std::uint8_t hasThird = 0;
    if (!reader.byte(hasFirst) || !reader.byte(hasSecond) || !reader.byte(hasThird)) return nullptr;
    if (hasFirst != 0U)
    {
        ast::AstNode* child = deserializeAstNodeTree(arena, reader);
        if (child == nullptr) return nullptr;
        node->setFirst(child);
    }
    if (hasSecond != 0U)
    {
        ast::AstNode* child = deserializeAstNodeTree(arena, reader);
        if (child == nullptr) return nullptr;
        node->setSecond(child);
    }
    if (hasThird != 0U)
    {
        ast::AstNode* child = deserializeAstNodeTree(arena, reader);
        if (child == nullptr) return nullptr;
        node->setThird(child);
    }
    std::uint32_t childCount = 0;
    if (!reader.u32(childCount)) return nullptr;
    for (std::uint32_t index = 0; index < childCount; ++index)
    {
        ast::AstNode* child = deserializeAstNodeTree(arena, reader);
        if (child == nullptr) return nullptr;
        node->addChild(child);
    }
    return node;
}

// NSC305-C01. GenericParameterSyntax + LifetimeParameterSyntax + LifetimeOutlivesConstraint (no
// TypeSyntax recursion in the parameter; constantType is serialized as a full tree).
[[maybe_unused]] std::string serializeGenericParameter(const ast::GenericParameterSyntax& parameter)
{
    std::string out;
    appendString(out, parameter.name);
    appendByte(out, static_cast<std::uint8_t>(parameter.kind));
    appendByte(out, parameter.constantType != nullptr ? 1U : 0U);
    if (parameter.constantType != nullptr)
        out += serializeTypeSyntaxTree(parameter.constantType);
    appendU32(out, parameter.range.begin().offset());
    appendU32(out, parameter.range.end().offset());
    return out;
}

// NSC305-C01. GenericConstraintClause (the source `where` clause; TypeSyntax references as trees).
[[maybe_unused]] std::string serializeGenericConstraint(const ast::GenericConstraintClause& clause)
{
    std::string out;
    appendString(out, clause.parameterName);
    appendU32(out, clause.parameterRange.begin().offset());
    appendU32(out, clause.parameterRange.end().offset());
    appendByte(out, clause.requiresClass ? 1U : 0U);
    appendByte(out, clause.requiresValueType ? 1U : 0U);
    appendByte(out, clause.requiresDefaultConstructor ? 1U : 0U);
    appendByte(out, clause.requiresCopy ? 1U : 0U);
    appendByte(out, clause.requiresMove ? 1U : 0U);
    appendByte(out, clause.requiresTrackedLifetime ? 1U : 0U);
    appendByte(out, clause.requiresUnmanaged ? 1U : 0U);
    appendByte(out, clause.requiresNotNull ? 1U : 0U);
    appendByte(out, clause.requiresRefLike ? 1U : 0U);
    appendByte(out, clause.equalityConstraint != nullptr ? 1U : 0U);
    if (clause.equalityConstraint != nullptr)
        out += serializeTypeSyntaxTree(clause.equalityConstraint);
    appendByte(out, clause.associatedProjection != nullptr ? 1U : 0U);
    if (clause.associatedProjection != nullptr)
        out += serializeTypeSyntaxTree(clause.associatedProjection);
    appendU32(out, static_cast<std::uint32_t>(clause.typeConstraints.size()));
    for (const ast::TypeSyntax* constraint : clause.typeConstraints)
        out += serializeTypeSyntaxTree(constraint);
    appendU32(out, clause.range.begin().offset());
    appendU32(out, clause.range.end().offset());
    appendByte(out, clause.substitutedArgument != nullptr ? 1U : 0U);
    if (clause.substitutedArgument != nullptr)
        out += serializeTypeSyntaxTree(clause.substitutedArgument);
    return out;
}

// NSC305-C01. Inverse of serializeGenericParameter (allocates the constantType into `arena`).
[[maybe_unused]] static bool deserializeGenericParameter(
    ast::AstArena& arena, ByteReader& reader, ast::GenericParameterSyntax& parameter)
{
    std::uint8_t kind = 0;
    std::uint8_t hasConstantType = 0;
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    if (!reader.string(parameter.name) || !reader.byte(kind) || !reader.byte(hasConstantType))
        return false;
    parameter.kind = static_cast<ast::GenericParameterSyntaxKind>(kind);
    if (hasConstantType != 0U)
    {
        parameter.constantType = deserializeTypeSyntaxTree(arena, reader);
        if (parameter.constantType == nullptr) return false;
    }
    if (!reader.u32(begin) || !reader.u32(end)) return false;
    parameter.range =
        SourceRange{SourceLocation{FileId{}, begin}, SourceLocation{FileId{}, end}};
    return true;
}

// NSC305-C01. Inverse of serializeGenericConstraint (allocates TypeSyntax references into `arena`).
[[maybe_unused]] static bool deserializeGenericConstraint(
    ast::AstArena& arena, ByteReader& reader, ast::GenericConstraintClause& clause)
{
    std::uint8_t requiresClass = 0;
    std::uint8_t requiresValueType = 0;
    std::uint8_t requiresDefaultConstructor = 0;
    std::uint8_t requiresCopy = 0;
    std::uint8_t requiresMove = 0;
    std::uint8_t requiresTrackedLifetime = 0;
    std::uint8_t requiresUnmanaged = 0;
    std::uint8_t requiresNotNull = 0;
    std::uint8_t requiresRefLike = 0;
    std::uint8_t hasEquality = 0;
    std::uint8_t hasProjection = 0;
    std::uint8_t hasSubstituted = 0;
    std::uint32_t parameterBegin = 0;
    std::uint32_t parameterEnd = 0;
    std::uint32_t clauseBegin = 0;
    std::uint32_t clauseEnd = 0;
    std::uint32_t constraintCount = 0;
    if (!reader.string(clause.parameterName) || !reader.u32(parameterBegin) ||
        !reader.u32(parameterEnd) || !reader.byte(requiresClass) ||
        !reader.byte(requiresValueType) || !reader.byte(requiresDefaultConstructor) ||
        !reader.byte(requiresCopy) || !reader.byte(requiresMove) ||
        !reader.byte(requiresTrackedLifetime) || !reader.byte(requiresUnmanaged) ||
        !reader.byte(requiresNotNull) || !reader.byte(requiresRefLike) ||
        !reader.byte(hasEquality) || !reader.byte(hasProjection) || !reader.u32(constraintCount))
        return false;
    clause.parameterRange =
        SourceRange{SourceLocation{FileId{}, parameterBegin}, SourceLocation{FileId{}, parameterEnd}};
    clause.requiresClass = requiresClass != 0U;
    clause.requiresValueType = requiresValueType != 0U;
    clause.requiresDefaultConstructor = requiresDefaultConstructor != 0U;
    clause.requiresCopy = requiresCopy != 0U;
    clause.requiresMove = requiresMove != 0U;
    clause.requiresTrackedLifetime = requiresTrackedLifetime != 0U;
    clause.requiresUnmanaged = requiresUnmanaged != 0U;
    clause.requiresNotNull = requiresNotNull != 0U;
    clause.requiresRefLike = requiresRefLike != 0U;
    if (hasEquality != 0U)
    {
        clause.equalityConstraint = deserializeTypeSyntaxTree(arena, reader);
        if (clause.equalityConstraint == nullptr) return false;
    }
    if (hasProjection != 0U)
    {
        clause.associatedProjection = deserializeTypeSyntaxTree(arena, reader);
        if (clause.associatedProjection == nullptr) return false;
    }
    clause.typeConstraints.clear();
    for (std::uint32_t index = 0; index < constraintCount; ++index)
    {
        ast::TypeSyntax* constraint = deserializeTypeSyntaxTree(arena, reader);
        if (constraint == nullptr) return false;
        clause.typeConstraints.push_back(constraint);
    }
    if (!reader.u32(clauseBegin) || !reader.u32(clauseEnd) || !reader.byte(hasSubstituted))
        return false;
    clause.range = SourceRange{SourceLocation{FileId{}, clauseBegin}, SourceLocation{FileId{}, clauseEnd}};
    if (hasSubstituted != 0U)
    {
        clause.substitutedArgument = deserializeTypeSyntaxTree(arena, reader);
        if (clause.substitutedArgument == nullptr) return false;
    }
    return true;
}

// NSC305-C02. Full catalog entry: the record metadata followed by the AST subtree (the syntax node).
// The indexes are rebuilt from the restored records, so only these two are persisted.
[[maybe_unused]] std::string serializeCatalogEntry(const GenericTemplateRecord& record)
{
    std::string out = serializeTemplateRecord(record);
    out += serializeAstNodeTree(record.syntax);
    return out;
}

// NSC305-C02. Inverse of serializeCatalogEntry: reads the record metadata, then the syntax AST tree
// into `arena`, and links record.syntax to the restored node.
[[maybe_unused]] bool restoreCatalogEntry(
    ast::AstArena& arena, std::string_view bytes, GenericTemplateRecord& record)
{
    ByteReader reader(bytes);
    if (!readTemplateRecord(reader, record)) return false;
    record.syntax = deserializeAstNodeTree(arena, reader);
    return record.syntax != nullptr;
}

// NSC305-C04. Interface/implementation fingerprints for the MIR cache. A private body change updates
// only the implementation fingerprint, so dependents that observe the interface stay valid.


[[maybe_unused]] static std::string fingerprintKey(std::string_view material)
{
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size()};
    return basic::sha256Hex(basic::sha256(bytes));
}

[[maybe_unused]] static std::string interfaceFingerprintKey(const InterfaceFingerprint& fingerprint)
{
    std::string material = "iface\n";
    material += fingerprint.signature;
    material += "\n";
    material += fingerprint.layoutAbi;
    for (const std::string& dependency : fingerprint.publicDependencies)
    {
        material += "\n";
        material += dependency;
    }
    return fingerprintKey(material);
}

[[maybe_unused]] static std::string implementationFingerprintKey(const ImplementationFingerprint& fingerprint)
{
    std::string material = "impl\n";
    material += fingerprint.normalizedBody;
    for (const std::string& interface : fingerprint.usedInterfaces)
    {
        material += "\n";
        material += interface;
    }
    return fingerprintKey(material);
}

// NSC305-C04. Interface fingerprint of a MIR function from its observable key fields (the mangled
// name, return/error type keys and parameter type keys). The implementation fingerprint (body) is
// derived from the normalized MIR node stream by the MIR cache consumer.
[[maybe_unused]] static std::string mirInterfaceFingerprintKey(
    std::string_view mangledName,
    std::string_view returnTypeKey,
    std::string_view errorTypeKey,
    const std::vector<std::string>& parameterTypeKeys)
{
    InterfaceFingerprint fingerprint;
    fingerprint.signature = std::string(mangledName) + "(";
    for (std::size_t index = 0; index < parameterTypeKeys.size(); ++index)
    {
        if (index != 0U) fingerprint.signature += ",";
        fingerprint.signature += parameterTypeKeys[index];
    }
    fingerprint.signature += ")->" + std::string(returnTypeKey);
    if (!errorTypeKey.empty()) fingerprint.signature += " throws " + std::string(errorTypeKey);
    fingerprint.publicDependencies = parameterTypeKeys;
    return interfaceFingerprintKey(fingerprint);
}

// NSC305-C04. Key fields of a MIR function (the observable identity that feeds the interface
// fingerprint); the full MIR node stream is the implementation-fingerprint layer.

[[maybe_unused]] static std::string serializeMirFunctionKey(const MirFunctionKey& key)
{
    std::string out;
    appendString(out, key.mangledName);
    appendString(out, key.genericSpecializationKey);
    appendString(out, key.returnTypeKey);
    appendString(out, key.errorTypeKey);
    appendStringVector(out, key.parameterTypeKeys);
    return out;
}

[[maybe_unused]] static bool restoreMirFunctionKey(std::string_view bytes, MirFunctionKey& key)
{
    ByteReader reader(bytes);
    return reader.string(key.mangledName) &&
        reader.string(key.genericSpecializationKey) &&
        reader.string(key.returnTypeKey) &&
        reader.string(key.errorTypeKey) &&
        reader.stringVector(key.parameterTypeKeys);
}

// NSC305-C04. The MIR cache record: the observable key + the interface fingerprint + the
// implementation fingerprint. A private body change updates only `implementationFingerprint`, so a
// dependent that consumed only the interface stays valid. The full MIR node stream is the next layer.

[[maybe_unused]] std::string serializeMirCacheRecord(const MirCacheRecord& record)
{
    std::string out = serializeMirFunctionKey(record.key);
    appendString(out, record.interfaceFingerprint);
    appendString(out, record.implementationFingerprint);
    return out;
}

[[maybe_unused]] bool restoreMirCacheRecord(std::string_view bytes, MirCacheRecord& record)
{
    ByteReader reader(bytes);
    return reader.string(record.key.mangledName) &&
        reader.string(record.key.genericSpecializationKey) &&
        reader.string(record.key.returnTypeKey) &&
        reader.string(record.key.errorTypeKey) &&
        reader.stringVector(record.key.parameterTypeKeys) &&
        reader.string(record.interfaceFingerprint) &&
        reader.string(record.implementationFingerprint);
}
}
