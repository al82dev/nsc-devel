#include "ClassMemberSnapshotCodec.h"
#include "SnapshotCodecSupport.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
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
    writeSnapshotParameters(writer, member.parameters);
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
    if (!readSnapshotParameters(reader, member.parameters)) return false;
    member.ffiUnwindPolicy = static_cast<ast::FfiUnwindSyntaxKind>(reader.byte());
    member.signatureKey = reader.text();
    return !reader.failed();
}

// EN: Lifetime, escape, effect and storage contracts of the member.
// ES: Contratos de lifetime, escape, efectos y almacenamiento del miembro.
void writeMemberContracts(PrecompiledSnapshotWriter& writer, const ClassMemberSymbol& member)
{
    writer.text(member.returnLifetimeName);
    writeSnapshotTexts(writer, member.lifetimeParameters);
    writeSnapshotTextPairs(writer, member.lifetimeOutlivesConstraints);
    writeSnapshotEscapes(writer, member.parameterEscapeSummary);
    writer.node(member.bitWidthSyntax);
    writer.byte(static_cast<std::uint8_t>(member.inliningPolicy));
    writer.u32(member.effects.bits());
    writer.text(member.importDllName);
    writer.text(member.cAbiName);
    writer.text(member.operatorTraitKey);
    writer.text(member.associatedName);
    writer.u64(static_cast<std::uint64_t>(member.enumConstantValue));
    writeSnapshotOptional(writer, member.explicitFieldOffset);
    writeSnapshotOptional(writer, member.explicitFieldAlignment);
}

bool readMemberContracts(PrecompiledSnapshotReader& reader, ClassMemberSymbol& member)
{
    member.returnLifetimeName = reader.text();
    member.lifetimeParameters = readSnapshotTexts(reader);
    member.lifetimeOutlivesConstraints = readSnapshotTextPairs(reader);
    member.parameterEscapeSummary = readSnapshotEscapes(reader);
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
    member.explicitFieldOffset = readSnapshotOptional(reader);
    member.explicitFieldAlignment = readSnapshotOptional(reader);
    return !reader.failed() && member.effects.bits() == effectBits;
}

void writeMemberFlags(PrecompiledSnapshotWriter& writer, const ClassMemberSymbol& member)
{
    writeSnapshotFlags(writer, {member.isPrecompiled, member.isTransportedInlineBody,
        member.declaredPointerPointeeReadonly, member.declaredPointerNullable, member.declaredPointerHasProvenance,
        member.hasImplicitThis, member.declaringTypeIsStruct, member.isStatic, member.isNewSlot, member.isReadonly,
        member.isReferenceReturn, member.isVolatile, member.isConst, member.isBitField, member.isUnnamedBitField,
        member.isNoHeap, member.isUnsafe, member.isVirtual, member.isOverride, member.isAbstract, member.isSealed,
        member.isExported, member.isExternC, member.isExportedC, member.isVariadic, member.isImportedC,
        member.importRequiresRuntimeBinding, member.isCompilerGenerated, member.isEvent, member.isEventBacking,
        member.isPropertyAccessor, member.isIndexerAccessor, member.isGetter, member.isSetter, member.isInit,
        member.isRequired, member.isUserOperator, member.isUserConversion, member.isImplicitUserConversion,
        member.ownsAccessorStorage, member.isEnumConstant});
}

void readMemberFlags(PrecompiledSnapshotReader& reader, ClassMemberSymbol& member)
{
    readSnapshotFlags(reader, {&member.isPrecompiled, &member.isTransportedInlineBody,
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
