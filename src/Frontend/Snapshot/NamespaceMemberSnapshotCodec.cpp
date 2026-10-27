#include "NamespaceMemberSnapshotCodec.h"
#include "SnapshotCodecSupport.h"

#include <bit>
#include <cstdint>

namespace nativesharp::frontend
{
void writeNamespaceFunctionSnapshot(PrecompiledSnapshotWriter& writer, const NamespaceFunctionSymbol& function)
{
    writer.type(function.declaredTypeSyntax);
    writer.text(function.declaredTypeKey);
    writer.type(function.declaredErrorTypeSyntax);
    writer.text(function.declaredErrorTypeKey);
    writer.byte(static_cast<std::uint8_t>(function.declaredOwnership));
    writeSnapshotParameters(writer, function.parameters);
    writer.text(function.signatureKey);
    writer.text(function.callableIdentityKey);
    writer.text(function.mangledName);
    writer.text(function.returnLifetimeName);
    writeSnapshotTexts(writer, function.lifetimeParameters);
    writeSnapshotTextPairs(writer, function.lifetimeOutlivesConstraints);
    writeSnapshotEscapes(writer, function.parameterEscapeSummary);
    writer.u32(function.effects.bits());
    writer.byte(static_cast<std::uint8_t>(function.inliningPolicy));
    writer.u32(static_cast<std::uint32_t>(function.operatorToken));
    writeSnapshotFlags(writer, {function.isReferenceReturn, function.isVariadic, function.isExternC,
        function.isExportedC, function.isExtension, function.isStaticExtension});
}

bool readNamespaceFunctionSnapshot(PrecompiledSnapshotReader& reader, NamespaceFunctionSymbol& function)
{
    function.declaredTypeSyntax = reader.type();
    function.declaredTypeKey = reader.text();
    function.declaredErrorTypeSyntax = reader.type();
    function.declaredErrorTypeKey = reader.text();
    function.declaredOwnership = static_cast<sema::OwnershipKind>(reader.byte());
    if (!readSnapshotParameters(reader, function.parameters)) return false;
    function.signatureKey = reader.text();
    function.callableIdentityKey = reader.text();
    function.mangledName = reader.text();
    function.returnLifetimeName = reader.text();
    function.lifetimeParameters = readSnapshotTexts(reader);
    function.lifetimeOutlivesConstraints = readSnapshotTextPairs(reader);
    function.parameterEscapeSummary = readSnapshotEscapes(reader);
    const std::uint32_t effectBits = reader.u32();
    for (std::uint32_t bit = 0; bit < 32U; ++bit)
    {
        if ((effectBits & (1U << bit)) != 0U)
            function.effects = function.effects.with(static_cast<sema::Effect>(1U << bit));
    }
    function.inliningPolicy = static_cast<InliningPolicy>(reader.byte());
    function.operatorToken = static_cast<lexer::TokenKind>(reader.u32());
    readSnapshotFlags(reader, {&function.isReferenceReturn, &function.isVariadic, &function.isExternC,
        &function.isExportedC, &function.isExtension, &function.isStaticExtension});
    return !reader.failed() && function.effects.bits() == effectBits;
}

void writeNamespaceStateSnapshot(PrecompiledSnapshotWriter& writer, const NamespaceStateSymbol& state)
{
    writer.type(state.declaredTypeSyntax);
    writer.text(state.declaredTypeKey);
    writer.text(state.identityKey);
    writer.text(state.storageName);
    writer.node(state.initializerSyntax);
    writer.byte(static_cast<std::uint8_t>(state.initializerKind));
    writer.u64(state.integerValue);
    writer.u64(std::bit_cast<std::uint64_t>(state.floatingValue));
    writer.byte(state.booleanValue ? 1U : 0U);
    writer.text(state.stringValue);
}

bool readNamespaceStateSnapshot(PrecompiledSnapshotReader& reader, NamespaceStateSymbol& state)
{
    state.declaredTypeSyntax = reader.type();
    state.declaredTypeKey = reader.text();
    state.identityKey = reader.text();
    state.storageName = reader.text();
    state.initializerSyntax = reader.node();
    state.initializerKind = static_cast<NamespaceStateInitializerKind>(reader.byte());
    state.integerValue = reader.u64();
    state.floatingValue = std::bit_cast<double>(reader.u64());
    state.booleanValue = reader.byte() != 0U;
    state.stringValue = reader.text();
    return !reader.failed();
}
}
