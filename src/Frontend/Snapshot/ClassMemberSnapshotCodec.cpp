#include "ClassMemberSnapshotCodec.h"

#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
void writeFlags(PrecompiledSnapshotWriter& writer, std::initializer_list<bool> flags)
{
    for (const bool flag : flags) writer.byte(flag ? 1U : 0U);
}

void readFlags(PrecompiledSnapshotReader& reader, std::initializer_list<bool*> flags)
{
    for (bool* const flag : flags) *flag = reader.byte() != 0U;
}

void writeOptional(PrecompiledSnapshotWriter& writer, const std::optional<std::uint64_t>& value)
{
    writer.byte(value.has_value() ? 1U : 0U);
    if (value.has_value()) writer.u64(*value);
}

std::optional<std::uint64_t> readOptional(PrecompiledSnapshotReader& reader)
{
    if (reader.byte() == 0U) return std::nullopt;
    return reader.u64();
}

void writeTexts(PrecompiledSnapshotWriter& writer, const std::vector<std::string>& values)
{
    writer.u32(static_cast<std::uint32_t>(values.size()));
    for (const std::string& value : values) writer.text(value);
}

std::vector<std::string> readTexts(PrecompiledSnapshotReader& reader)
{
    std::vector<std::string> values;
    const std::uint32_t count = reader.u32();
    for (std::uint32_t index = 0; index < count && !reader.failed(); ++index) values.push_back(reader.text());
    return values;
}

// EN: Parameter fields except FileId and range, which the installer derives from the part and node.
// ES: Campos del parámetro salvo FileId y rango, que el instalador deriva de la parte y del nodo.
void writeParameter(PrecompiledSnapshotWriter& writer, const MethodParameterSymbol& parameter)
{
    writer.node(parameter.syntax);
    writer.text(parameter.name);
    writer.byte(static_cast<std::uint8_t>(parameter.passKind));
    writer.text(parameter.pointeeTypeKey);
    writer.type(parameter.typeSyntax);
    writer.text(parameter.typeKey);
    writeFlags(writer, {parameter.pointerPointeeReadonly, parameter.pointerNullable, parameter.pointerHasProvenance,
        parameter.isParameterPack, parameter.ffiCallback, parameter.ffiCallbackMayEscape});
    writer.u64(parameter.pointerKnownAlignment);
    writer.u32(parameter.pointerAddressSpace);
    writer.byte(static_cast<std::uint8_t>(parameter.ownership));
    writer.text(parameter.lifetimeName);
    writer.text(parameter.parameterPackElementTypeKey);
    writer.text(parameter.ffiLengthParameterName);
    writeOptional(writer, parameter.ffiLengthParameterOrdinal.has_value()
        ? std::optional<std::uint64_t>(*parameter.ffiLengthParameterOrdinal) : std::nullopt);
    writer.node(parameter.defaultValueSyntax);
}

bool readParameter(PrecompiledSnapshotReader& reader, MethodParameterSymbol& parameter)
{
    parameter.syntax = reader.node();
    parameter.name = reader.text();
    parameter.passKind = static_cast<ParameterPassKind>(reader.byte());
    parameter.pointeeTypeKey = reader.text();
    parameter.typeSyntax = reader.type();
    parameter.typeKey = reader.text();
    readFlags(reader, {&parameter.pointerPointeeReadonly, &parameter.pointerNullable,
        &parameter.pointerHasProvenance, &parameter.isParameterPack, &parameter.ffiCallback,
        &parameter.ffiCallbackMayEscape});
    parameter.pointerKnownAlignment = static_cast<std::size_t>(reader.u64());
    parameter.pointerAddressSpace = reader.u32();
    parameter.ownership = static_cast<sema::OwnershipKind>(reader.byte());
    parameter.lifetimeName = reader.text();
    parameter.parameterPackElementTypeKey = reader.text();
    parameter.ffiLengthParameterName = reader.text();
    const std::optional<std::uint64_t> lengthOrdinal = readOptional(reader);
    if (lengthOrdinal.has_value()) parameter.ffiLengthParameterOrdinal = static_cast<std::size_t>(*lengthOrdinal);
    parameter.defaultValueSyntax = reader.node();
    return !reader.failed() && parameter.syntax != nullptr;
}

// EN: Identity, declared type, pointer contract and parameters of the member.
// ES: Identidad, tipo declarado, contrato de puntero y parámetros del miembro.
void writeMemberSignature(PrecompiledSnapshotWriter& writer, const ClassMemberSymbol& member)
{
    writer.byte(static_cast<std::uint8_t>(member.kind));
    writer.byte(static_cast<std::uint8_t>(member.accessibility));
    writer.text(member.precompiledMangledName);
    writer.text(member.name);
    writer.type(member.declaredTypeSyntax);
    writer.text(member.declaredTypeKey);
    writer.type(member.declaredErrorTypeSyntax);
    writer.text(member.declaredErrorTypeKey);
    writer.byte(static_cast<std::uint8_t>(member.frameStorage));
    writer.u64(member.declaredPointerKnownAlignment);
    writer.u32(member.declaredPointerAddressSpace);
    writer.byte(static_cast<std::uint8_t>(member.declaredOwnership));
    writer.u32(static_cast<std::uint32_t>(member.parameters.size()));
    for (const MethodParameterSymbol& parameter : member.parameters) writeParameter(writer, parameter);
    writer.byte(static_cast<std::uint8_t>(member.ffiUnwindPolicy));
    writer.text(member.signatureKey);
}

bool readMemberSignature(PrecompiledSnapshotReader& reader, ClassMemberSymbol& member)
{
    member.kind = static_cast<ClassMemberKind>(reader.byte());
    member.accessibility = static_cast<ClassMemberAccessibility>(reader.byte());
    member.precompiledMangledName = reader.text();
    member.name = reader.text();
    member.declaredTypeSyntax = reader.type();
    member.declaredTypeKey = reader.text();
    member.declaredErrorTypeSyntax = reader.type();
    member.declaredErrorTypeKey = reader.text();
    member.frameStorage = static_cast<language::FrameStorage>(reader.byte());
    member.declaredPointerKnownAlignment = static_cast<std::size_t>(reader.u64());
    member.declaredPointerAddressSpace = reader.u32();
    member.declaredOwnership = static_cast<sema::OwnershipKind>(reader.byte());
    const std::uint32_t parameterCount = reader.u32();
    for (std::uint32_t index = 0; index < parameterCount && !reader.failed(); ++index)
    {
        MethodParameterSymbol parameter;
        if (!readParameter(reader, parameter)) return false;
        member.parameters.push_back(std::move(parameter));
    }
    member.ffiUnwindPolicy = static_cast<ast::FfiUnwindSyntaxKind>(reader.byte());
    member.signatureKey = reader.text();
    return !reader.failed();
}

// EN: Lifetime, escape, effect and storage contracts of the member.
// ES: Contratos de lifetime, escape, efectos y almacenamiento del miembro.
void writeMemberContracts(PrecompiledSnapshotWriter& writer, const ClassMemberSymbol& member)
{
    writer.text(member.returnLifetimeName);
    writeTexts(writer, member.lifetimeParameters);
    writer.u32(static_cast<std::uint32_t>(member.lifetimeOutlivesConstraints.size()));
    for (const std::pair<std::string, std::string>& constraint : member.lifetimeOutlivesConstraints)
    {
        writer.text(constraint.first);
        writer.text(constraint.second);
    }
    writer.u32(static_cast<std::uint32_t>(member.parameterEscapeSummary.size()));
    for (const language::EscapeClass escape : member.parameterEscapeSummary)
        writer.byte(static_cast<std::uint8_t>(escape));
    writer.node(member.bitWidthSyntax);
    writer.byte(static_cast<std::uint8_t>(member.inliningPolicy));
    writer.u32(member.effects.bits());
    writer.text(member.importDllName);
    writer.text(member.cAbiName);
    writer.text(member.operatorTraitKey);
    writer.text(member.associatedName);
    writer.u64(static_cast<std::uint64_t>(member.enumConstantValue));
    writeOptional(writer, member.explicitFieldOffset);
    writeOptional(writer, member.explicitFieldAlignment);
}

bool readMemberContracts(PrecompiledSnapshotReader& reader, ClassMemberSymbol& member)
{
    member.returnLifetimeName = reader.text();
    member.lifetimeParameters = readTexts(reader);
    const std::uint32_t constraintCount = reader.u32();
    for (std::uint32_t index = 0; index < constraintCount && !reader.failed(); ++index)
    {
        std::string outer = reader.text();
        member.lifetimeOutlivesConstraints.emplace_back(std::move(outer), reader.text());
    }
    const std::uint32_t escapeCount = reader.u32();
    for (std::uint32_t index = 0; index < escapeCount && !reader.failed(); ++index)
        member.parameterEscapeSummary.push_back(static_cast<language::EscapeClass>(reader.byte()));
    member.bitWidthSyntax = reader.node();
    member.inliningPolicy = static_cast<InliningPolicy>(reader.byte());
    const std::uint32_t effectBits = reader.u32();
    for (std::uint32_t bit = 0; bit < 32U; ++bit)
    {
        if ((effectBits & (1U << bit)) != 0U)
            member.effects = member.effects.with(static_cast<sema::Effect>(1U << bit));
    }
    member.importDllName = reader.text();
    member.cAbiName = reader.text();
    member.operatorTraitKey = reader.text();
    member.associatedName = reader.text();
    member.enumConstantValue = static_cast<std::int64_t>(reader.u64());
    member.explicitFieldOffset = readOptional(reader);
    member.explicitFieldAlignment = readOptional(reader);
    return !reader.failed() && member.effects.bits() == effectBits;
}

void writeMemberFlags(PrecompiledSnapshotWriter& writer, const ClassMemberSymbol& member)
{
    writeFlags(writer, {member.isPrecompiled, member.isTransportedInlineBody, member.declaredPointerPointeeReadonly,
        member.declaredPointerNullable, member.declaredPointerHasProvenance, member.hasImplicitThis,
        member.declaringTypeIsStruct, member.isStatic, member.isNewSlot, member.isReadonly, member.isReferenceReturn,
        member.isVolatile, member.isConst, member.isBitField, member.isUnnamedBitField, member.isNoHeap,
        member.isUnsafe, member.isVirtual, member.isOverride, member.isAbstract, member.isSealed, member.isExported,
        member.isExternC, member.isExportedC, member.isVariadic, member.isImportedC,
        member.importRequiresRuntimeBinding, member.isCompilerGenerated, member.isEvent, member.isEventBacking,
        member.isPropertyAccessor, member.isIndexerAccessor, member.isGetter, member.isSetter, member.isInit,
        member.isRequired, member.isUserOperator, member.isUserConversion, member.isImplicitUserConversion,
        member.ownsAccessorStorage, member.isEnumConstant});
}

void readMemberFlags(PrecompiledSnapshotReader& reader, ClassMemberSymbol& member)
{
    readFlags(reader, {&member.isPrecompiled, &member.isTransportedInlineBody,
        &member.declaredPointerPointeeReadonly, &member.declaredPointerNullable,
        &member.declaredPointerHasProvenance, &member.hasImplicitThis, &member.declaringTypeIsStruct,
        &member.isStatic, &member.isNewSlot, &member.isReadonly, &member.isReferenceReturn, &member.isVolatile,
        &member.isConst, &member.isBitField, &member.isUnnamedBitField, &member.isNoHeap, &member.isUnsafe,
        &member.isVirtual, &member.isOverride, &member.isAbstract, &member.isSealed, &member.isExported,
        &member.isExternC, &member.isExportedC, &member.isVariadic, &member.isImportedC,
        &member.importRequiresRuntimeBinding, &member.isCompilerGenerated, &member.isEvent, &member.isEventBacking,
        &member.isPropertyAccessor, &member.isIndexerAccessor, &member.isGetter, &member.isSetter, &member.isInit,
        &member.isRequired, &member.isUserOperator, &member.isUserConversion, &member.isImplicitUserConversion,
        &member.ownsAccessorStorage, &member.isEnumConstant});
}
}

void writeClassMemberSnapshot(PrecompiledSnapshotWriter& writer, const ClassMemberSymbol& member)
{
    writeMemberSignature(writer, member);
    writeMemberContracts(writer, member);
    writeMemberFlags(writer, member);
}

bool readClassMemberSnapshot(PrecompiledSnapshotReader& reader, ClassMemberSymbol& member)
{
    if (!readMemberSignature(reader, member) || !readMemberContracts(reader, member)) return false;
    readMemberFlags(reader, member);
    return !reader.failed();
}
}
