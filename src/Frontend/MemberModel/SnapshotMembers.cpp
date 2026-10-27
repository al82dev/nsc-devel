#include "nativesharp/Frontend/ProjectMemberModel.h"

#include "../Snapshot/ClassMemberSnapshotCodec.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
constexpr std::string_view kMembersSection = "members";

// EN: The payload of the `members` section, or nullptr when the snapshot has none.
// ES: El payload de la sección `members`, o nullptr si el snapshot no la tiene.
const std::string* membersSection(const PrecompiledSnapshotUnit& snapshot) noexcept
{
    for (const compilation::PrecompiledSnapshotSection& section : snapshot.snapshot.sections)
    {
        if (section.model == kMembersSection) return &section.payload;
    }
    return nullptr;
}

bool sameRange(const SourceRange& left, const SourceRange& right) noexcept
{
    return left.begin() == right.begin() && left.end() == right.end();
}

// EN: True when every field the installer derives (FileId, ranges) matches what it would derive,
//     and the member carries no generic specialization or promotion data, which are never written.
// ES: True si cada campo que deriva el instalador (FileId, rangos) coincide con lo que derivaría, y
//     el miembro no lleva datos de especialización genérica ni de promoción, que nunca se escriben.
bool derivableMember(const ClassMemberSymbol& member, FileId fileId) noexcept
{
    if (member.syntax == nullptr || member.fileId != fileId || !sameRange(member.range, member.syntax->range()))
        return false;
    if (member.genericTemplateOrdinal.has_value() || member.isControlledSpecialization ||
        member.isControlledSpecializationFamily || !member.genericTemplateKey.empty())
        return false;
    if (member.isAnonymousMemberPromotion || member.promotedSourceMemberOrdinal.has_value() ||
        !member.anonymousCarrierMemberOrdinals.empty())
        return false;
    for (const MethodParameterSymbol& parameter : member.parameters)
    {
        if (parameter.syntax == nullptr || parameter.fileId != fileId ||
            !sameRange(parameter.range, parameter.syntax->range()))
            return false;
    }
    return true;
}

// EN: Reads the `members` section of one unit; false (nothing kept) when any entry does not read.
// ES: Lee la sección `members` de una unidad; false (no se conserva nada) si alguna entrada no se lee.
bool readMembersSection(
    const PrecompiledSnapshotUnit& snapshot,
    std::vector<std::pair<const ast::AstNode*, ClassMemberSymbol>>& entries)
{
    const std::string* const payload = membersSection(snapshot);
    if (payload == nullptr) return false;
    PrecompiledSnapshotReader reader(*payload, snapshot.index);
    const std::uint64_t count = reader.u64();
    for (std::uint64_t entry = 0; entry < count && !reader.failed(); ++entry)
    {
        const ast::AstNode* const node = reader.node();
        ClassMemberSymbol member;
        if (node == nullptr || !readClassMemberSnapshot(reader, member)) return false;
        entries.emplace_back(node, std::move(member));
    }
    return !reader.failed() && reader.atEnd();
}
}

void ProjectMemberModel::setPrecompiledSnapshots(std::span<const PrecompiledSnapshotUnit> snapshots)
{
    for (const PrecompiledSnapshotUnit& snapshot : snapshots)
    {
        std::vector<std::pair<const ast::AstNode*, ClassMemberSymbol>> entries;
        if (!readMembersSection(snapshot, entries)) continue;
        for (std::pair<const ast::AstNode*, ClassMemberSymbol>& entry : entries)
            snapshotMembers_.emplace(entry.first, std::move(entry.second));
    }
}

std::size_t ProjectMemberModel::installedSnapshotMemberCount() const noexcept
{
    return installedSnapshotMembers_;
}

void ProjectMemberModel::setPrecompiledSnapshotCaptureFile(FileId fileId) noexcept
{
    snapshotCaptureFile_ = fileId;
}

void ProjectMemberModel::captureSnapshotMember(
    const NamedTypeDeclarationPart& part,
    const ClassMemberSymbol& symbol)
{
    if (snapshotCaptureFile_.has_value() && part.fileId == *snapshotCaptureFile_) capturedMembers_.push_back(symbol);
}

// EN: Writes one entry per member node, in first-collection order. A node collected more than once
//     (a trait impl re-collects its target's parts) must encode identically every time.
// ES: Escribe una entrada por nodo de miembro, en orden de primera recolección. Un nodo recolectado
//     más de una vez (un trait impl vuelve a recolectar las partes de su destino) debe codificarse
//     igual cada vez.
bool ProjectMemberModel::writePrecompiledSnapshotMembers(
    const PrecompiledSyntaxNodeIndex& index,
    std::string& payload,
    std::string& error) const
{
    if (!snapshotCaptureFile_.has_value())
    {
        error = "the API unit members were not captured";
        return false;
    }
    std::unordered_map<const ast::AstNode*, std::string> encodedByNode;
    std::string entries;
    std::uint64_t count = 0;
    for (const ClassMemberSymbol& member : capturedMembers_)
    {
        PrecompiledSnapshotWriter writer(index);
        writer.node(member.syntax);
        writeClassMemberSnapshot(writer, member);
        if (!derivableMember(member, *snapshotCaptureFile_) || writer.failed())
        {
            error = "a member of the API unit cannot be derived from its unit";
            return false;
        }
        std::string encoded = writer.take();
        const auto [found, inserted] = encodedByNode.emplace(member.syntax, encoded);
        if (!inserted && found->second != encoded)
        {
            error = "a member of the API unit is collected with different results";
            return false;
        }
        if (!inserted) continue;
        entries += encoded;
        ++count;
    }
    PrecompiledSnapshotWriter header(index);
    header.u64(count);
    payload = header.take() + entries;
    return true;
}

// EN: Installs the member computed by the producer for `memberNode`, completing the fields derived
//     from the collection position exactly as collectTypeMember does. A member that the consumer
//     sees as a generic specialization is left to collectTypeMember.
// ES: Instala el miembro calculado por el productor para `memberNode`, completando los campos
//     derivados de la posición de recolección igual que collectTypeMember. Un miembro que el
//     consumidor ve como especialización genérica se deja a collectTypeMember.
bool ProjectMemberModel::installSnapshotMember(
    std::size_t typeOrdinal,
    const NamedTypeDeclaration& type,
    std::size_t partOrdinal,
    const NamedTypeDeclarationPart& part,
    const ast::AstNode* memberNode,
    std::size_t declarationOrdinal,
    std::unordered_map<std::string, std::vector<std::size_t>>& byName,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const auto found = snapshotMembers_.find(memberNode);
    if (found == snapshotMembers_.end()) return false;
    ClassMemberSymbol symbol = found->second;
    initializeMemberIdentity(symbol, typeOrdinal, partOrdinal, declarationOrdinal, part, memberNode);
    applyMemberGenericSpecialization(symbol, memberNode);
    if (symbol.genericTemplateOrdinal.has_value()) return false;
    for (MethodParameterSymbol& parameter : symbol.parameters)
    {
        parameter.fileId = part.fileId;
        parameter.range = parameter.syntax->range();
    }
    ++installedSnapshotMembers_;
    if (isDuplicateMember(symbol, type, byName, diagnostics)) return true;
    appendCollectedMember(std::move(symbol), typeOrdinal, memberNode, byName);
    return true;
}
}
