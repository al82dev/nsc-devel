#include "PrecompiledSnapshotWriter.h"

#include "PrecompiledSnapshotReferenceKind.h"
#include "../Cache/CachePayloadWriter.h"

#include <optional>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
using Writer = internal::CachePayloadWriter;

std::uint8_t tag(PrecompiledSnapshotReferenceKind kind) noexcept
{
    return static_cast<std::uint8_t>(kind);
}
}

PrecompiledSnapshotWriter::PrecompiledSnapshotWriter(const PrecompiledSyntaxNodeIndex& index) noexcept
    : index_(index)
{
}

void PrecompiledSnapshotWriter::byte(std::uint8_t value)
{
    Writer::byte(bytes_, value);
}

void PrecompiledSnapshotWriter::u32(std::uint32_t value)
{
    Writer::u32(bytes_, value);
}

void PrecompiledSnapshotWriter::u64(std::uint64_t value)
{
    Writer::u64(bytes_, value);
}

void PrecompiledSnapshotWriter::text(std::string_view value)
{
    Writer::string(bytes_, value);
}

void PrecompiledSnapshotWriter::kind(std::uint8_t value)
{
    Writer::byte(bytes_, value);
}

void PrecompiledSnapshotWriter::node(const ast::AstNode* node)
{
    if (node == nullptr)
    {
        kind(tag(PrecompiledSnapshotReferenceKind::Null));
        return;
    }
    const std::optional<std::size_t> position = index_.indexOf(node);
    if (!position.has_value()) failed_ = true;
    kind(tag(PrecompiledSnapshotReferenceKind::SyntaxNode));
    Writer::u64(bytes_, position.value_or(0));
}

void PrecompiledSnapshotWriter::type(const ast::TypeSyntax* type)
{
    if (type == nullptr)
    {
        kind(tag(PrecompiledSnapshotReferenceKind::Null));
        return;
    }
    const std::optional<std::size_t> position = index_.indexOf(type);
    if (!position.has_value()) failed_ = true;
    kind(tag(PrecompiledSnapshotReferenceKind::TypeSyntax));
    Writer::u64(bytes_, position.value_or(0));
}

void PrecompiledSnapshotWriter::typeKey(std::string_view key)
{
    kind(tag(PrecompiledSnapshotReferenceKind::TypeKey));
    Writer::string(bytes_, key);
}

void PrecompiledSnapshotWriter::memberSignature(
    std::string_view declaringTypeKey,
    std::string_view signatureKey)
{
    kind(tag(PrecompiledSnapshotReferenceKind::MemberSignature));
    Writer::string(bytes_, declaringTypeKey);
    Writer::string(bytes_, signatureKey);
}

void PrecompiledSnapshotWriter::namespaceSymbol(std::string_view identityKey)
{
    kind(tag(PrecompiledSnapshotReferenceKind::NamespaceSymbol));
    Writer::string(bytes_, identityKey);
}

void PrecompiledSnapshotWriter::specialization(std::string_view specializationKey)
{
    kind(tag(PrecompiledSnapshotReferenceKind::Specialization));
    Writer::string(bytes_, specializationKey);
}

void PrecompiledSnapshotWriter::unitOrdinal(std::size_t relativeOrdinal)
{
    kind(tag(PrecompiledSnapshotReferenceKind::UnitOrdinal));
    Writer::u64(bytes_, static_cast<std::uint64_t>(relativeOrdinal));
}

bool PrecompiledSnapshotWriter::failed() const noexcept
{
    return failed_;
}

std::string PrecompiledSnapshotWriter::take()
{
    return std::move(bytes_);
}
}
