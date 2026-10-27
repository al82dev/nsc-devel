#include "ModelReferenceSnapshotCodec.h"

#include <string>

namespace nativesharp::frontend
{
void writeTypeReference(
    PrecompiledSnapshotWriter& writer,
    const ProjectNameLookup& names,
    std::size_t typeOrdinal,
    bool& written)
{
    if (typeOrdinal >= names.namedTypes().size())
    {
        written = false;
        writer.typeKey("");
        return;
    }
    writer.typeKey(names.namedTypes()[typeOrdinal].identity.key());
}

std::optional<std::size_t> readTypeReference(PrecompiledSnapshotReader& reader, const ProjectNameLookup& names)
{
    const std::string key = reader.typeKey();
    if (reader.failed()) return std::nullopt;
    return names.typeOrdinalForIdentity(key);
}

void writeTypeReferences(
    PrecompiledSnapshotWriter& writer,
    const ProjectNameLookup& names,
    std::span<const std::size_t> typeOrdinals,
    bool& written)
{
    writer.u32(static_cast<std::uint32_t>(typeOrdinals.size()));
    for (const std::size_t typeOrdinal : typeOrdinals) writeTypeReference(writer, names, typeOrdinal, written);
}

// EN: Reads every entry even after one fails to resolve, so the reader stays aligned.
// ES: Lee todas las entradas aunque una no resuelva, para que el lector siga alineado.
bool readTypeReferences(
    PrecompiledSnapshotReader& reader,
    const ProjectNameLookup& names,
    std::vector<std::size_t>& typeOrdinals)
{
    bool resolved = true;
    const std::uint32_t count = reader.u32();
    for (std::uint32_t index = 0; index < count && !reader.failed(); ++index)
    {
        const std::optional<std::size_t> typeOrdinal = readTypeReference(reader, names);
        if (typeOrdinal.has_value()) typeOrdinals.push_back(*typeOrdinal);
        else resolved = false;
    }
    return resolved && !reader.failed();
}

void writeMemberReference(
    PrecompiledSnapshotWriter& writer,
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    std::size_t memberOrdinal,
    bool& written)
{
    std::optional<std::size_t> position;
    std::size_t declaringType = names.namedTypes().size();
    if (memberOrdinal < members.members().size())
    {
        declaringType = members.members()[memberOrdinal].declaringTypeOrdinal;
        const std::span<const std::size_t> declared = members.membersOfType(declaringType);
        for (std::size_t index = 0; index < declared.size() && !position.has_value(); ++index)
        {
            if (declared[index] == memberOrdinal) position = index;
        }
    }
    if (!position.has_value() || declaringType >= names.namedTypes().size())
    {
        written = false;
        writer.memberSignature("", "");
        writer.u64(0);
        return;
    }
    writer.memberSignature(
        names.namedTypes()[declaringType].identity.key(), members.members()[memberOrdinal].signatureKey);
    writer.u64(*position);
}

std::optional<std::size_t> readMemberReference(
    PrecompiledSnapshotReader& reader,
    const ProjectNameLookup& names,
    const ProjectMemberModel& members)
{
    std::string typeKey;
    std::string signatureKey;
    reader.memberSignature(typeKey, signatureKey);
    const std::uint64_t position = reader.u64();
    if (reader.failed()) return std::nullopt;
    const std::optional<std::size_t> typeOrdinal = names.typeOrdinalForIdentity(typeKey);
    if (!typeOrdinal.has_value()) return std::nullopt;
    const std::span<const std::size_t> declared = members.membersOfType(*typeOrdinal);
    if (position >= declared.size()) return std::nullopt;
    const std::size_t memberOrdinal = declared[static_cast<std::size_t>(position)];
    if (members.members()[memberOrdinal].signatureKey != signatureKey) return std::nullopt;
    return memberOrdinal;
}

void writeMemberReferences(
    PrecompiledSnapshotWriter& writer,
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    std::span<const std::size_t> memberOrdinals,
    bool& written)
{
    writer.u32(static_cast<std::uint32_t>(memberOrdinals.size()));
    for (const std::size_t memberOrdinal : memberOrdinals)
        writeMemberReference(writer, names, members, memberOrdinal, written);
}

bool readMemberReferences(
    PrecompiledSnapshotReader& reader,
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    std::vector<std::size_t>& memberOrdinals)
{
    bool resolved = true;
    const std::uint32_t count = reader.u32();
    for (std::uint32_t index = 0; index < count && !reader.failed(); ++index)
    {
        const std::optional<std::size_t> memberOrdinal = readMemberReference(reader, names, members);
        if (memberOrdinal.has_value()) memberOrdinals.push_back(*memberOrdinal);
        else resolved = false;
    }
    return resolved && !reader.failed();
}

void writeOptionalMemberReference(
    PrecompiledSnapshotWriter& writer,
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    const std::optional<std::size_t>& memberOrdinal,
    bool& written)
{
    writer.byte(memberOrdinal.has_value() ? 1U : 0U);
    if (memberOrdinal.has_value()) writeMemberReference(writer, names, members, *memberOrdinal, written);
}

bool readOptionalMemberReference(
    PrecompiledSnapshotReader& reader,
    const ProjectNameLookup& names,
    const ProjectMemberModel& members,
    std::optional<std::size_t>& memberOrdinal)
{
    memberOrdinal.reset();
    if (reader.byte() == 0U) return !reader.failed();
    memberOrdinal = readMemberReference(reader, names, members);
    return memberOrdinal.has_value();
}
}
