#include "PrecompiledSnapshotReader.h"

#include "PrecompiledSnapshotReferenceKind.h"

#include <limits>

namespace nativesharp::frontend
{
namespace
{
std::uint8_t tag(PrecompiledSnapshotReferenceKind kind) noexcept
{
    return static_cast<std::uint8_t>(kind);
}
}

PrecompiledSnapshotReader::PrecompiledSnapshotReader(
    std::string_view bytes,
    const PrecompiledSyntaxNodeIndex& index) noexcept
    : reader_(bytes),
      index_(index)
{
}

std::uint8_t PrecompiledSnapshotReader::byte()
{
    std::uint8_t value = 0;
    if (failed_ || !reader_.byte(value)) failed_ = true;
    return failed_ ? 0U : value;
}

std::uint32_t PrecompiledSnapshotReader::u32()
{
    std::uint32_t value = 0;
    if (failed_ || !reader_.u32(value)) failed_ = true;
    return failed_ ? 0U : value;
}

std::uint64_t PrecompiledSnapshotReader::u64()
{
    std::uint64_t value = 0;
    if (failed_ || !reader_.u64(value)) failed_ = true;
    return failed_ ? 0U : value;
}

std::string PrecompiledSnapshotReader::text()
{
    std::string value;
    if (failed_ || !reader_.string(value)) failed_ = true;
    return failed_ ? std::string{} : value;
}

bool PrecompiledSnapshotReader::expectKind(std::uint8_t expected, bool allowNull)
{
    const std::uint8_t kind = byte();
    if (failed_) return false;
    if (kind == expected) return true;
    if (!allowNull || kind != tag(PrecompiledSnapshotReferenceKind::Null)) failed_ = true;
    return false;
}

const ast::AstNode* PrecompiledSnapshotReader::node()
{
    if (!expectKind(tag(PrecompiledSnapshotReferenceKind::SyntaxNode), true)) return nullptr;
    const std::uint64_t position = u64();
    if (failed_ || position >= index_.nodes().size())
    {
        failed_ = true;
        return nullptr;
    }
    return index_.nodes()[static_cast<std::size_t>(position)];
}

const ast::TypeSyntax* PrecompiledSnapshotReader::type()
{
    if (!expectKind(tag(PrecompiledSnapshotReferenceKind::TypeSyntax), true)) return nullptr;
    const std::uint64_t position = u64();
    if (failed_ || position >= index_.types().size())
    {
        failed_ = true;
        return nullptr;
    }
    return index_.types()[static_cast<std::size_t>(position)];
}

std::string PrecompiledSnapshotReader::typeKey()
{
    if (!expectKind(tag(PrecompiledSnapshotReferenceKind::TypeKey), false)) return {};
    return text();
}

void PrecompiledSnapshotReader::memberSignature(
    std::string& declaringTypeKey,
    std::string& signatureKey)
{
    declaringTypeKey.clear();
    signatureKey.clear();
    if (!expectKind(tag(PrecompiledSnapshotReferenceKind::MemberSignature), false)) return;
    declaringTypeKey = text();
    signatureKey = text();
}

std::string PrecompiledSnapshotReader::namespaceSymbol()
{
    if (!expectKind(tag(PrecompiledSnapshotReferenceKind::NamespaceSymbol), false)) return {};
    return text();
}

std::string PrecompiledSnapshotReader::specialization()
{
    if (!expectKind(tag(PrecompiledSnapshotReferenceKind::Specialization), false)) return {};
    return text();
}

std::size_t PrecompiledSnapshotReader::unitOrdinal()
{
    if (!expectKind(tag(PrecompiledSnapshotReferenceKind::UnitOrdinal), false)) return 0;
    const std::uint64_t value = u64();
    if (value > std::numeric_limits<std::size_t>::max()) failed_ = true;
    return failed_ ? 0U : static_cast<std::size_t>(value);
}

bool PrecompiledSnapshotReader::failed() const noexcept
{
    return failed_;
}

bool PrecompiledSnapshotReader::atEnd() const noexcept
{
    return reader_.remaining().empty();
}
}
