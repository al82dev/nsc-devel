#include "nativesharp/Compilation/PrecompiledSemanticSnapshotCodec.h"

#include "../Frontend/Cache/CachePayloadReader.h"
#include "../Frontend/Cache/CachePayloadWriter.h"

#include <cstdint>
#include <utility>

namespace nativesharp::compilation
{
namespace
{
constexpr std::string_view SnapshotSchema = "nsc-precompiled-semantic-snapshot-v1";

using Reader = frontend::internal::CachePayloadReader;
using Writer = frontend::internal::CachePayloadWriter;

// EN: Reads the dependency list; every entry needs a module name and a 64-digit digest.
// ES: Lee la lista de dependencias; cada entrada necesita nombre de módulo y un digest de 64 dígitos.
bool readDependencies(Reader& reader, std::vector<PrecompiledSnapshotDependency>& dependencies)
{
    std::uint32_t count = 0;
    if (!reader.u32(count)) return false;
    for (std::uint32_t index = 0; index < count; ++index)
    {
        PrecompiledSnapshotDependency dependency;
        if (!reader.string(dependency.moduleName) || !reader.string(dependency.metadataDigest) ||
            dependency.moduleName.empty() || dependency.metadataDigest.size() != 64U)
        {
            return false;
        }
        dependencies.push_back(std::move(dependency));
    }
    return true;
}

// EN: Reads the model sections; a model name is required and may not repeat.
// ES: Lee las secciones de modelos; el nombre del modelo es obligatorio y no se puede repetir.
bool readSections(Reader& reader, std::vector<PrecompiledSnapshotSection>& sections)
{
    std::uint32_t count = 0;
    if (!reader.u32(count)) return false;
    for (std::uint32_t index = 0; index < count; ++index)
    {
        PrecompiledSnapshotSection section;
        if (!reader.string(section.model) || !reader.string(section.payload) ||
            section.model.empty())
        {
            return false;
        }
        for (const PrecompiledSnapshotSection& previous : sections)
        {
            if (previous.model == section.model) return false;
        }
        sections.push_back(std::move(section));
    }
    return true;
}
}

std::string PrecompiledSemanticSnapshotCodec::encode(const PrecompiledSemanticSnapshot& snapshot)
{
    std::string bytes;
    Writer::string(bytes, SnapshotSchema);
    Writer::u32(bytes, static_cast<std::uint32_t>(snapshot.dependencies.size()));
    for (const PrecompiledSnapshotDependency& dependency : snapshot.dependencies)
    {
        Writer::string(bytes, dependency.moduleName);
        Writer::string(bytes, dependency.metadataDigest);
    }
    Writer::u64(bytes, snapshot.nodeCount);
    Writer::u64(bytes, snapshot.typeCount);
    Writer::string(bytes, snapshot.nodeKindDigest);
    Writer::u32(bytes, static_cast<std::uint32_t>(snapshot.sections.size()));
    for (const PrecompiledSnapshotSection& section : snapshot.sections)
    {
        Writer::string(bytes, section.model);
        Writer::string(bytes, section.payload);
    }
    return bytes;
}

bool PrecompiledSemanticSnapshotCodec::decode(
    std::string_view bytes,
    PrecompiledSemanticSnapshot& snapshot,
    std::string& error)
{
    Reader reader(bytes);
    std::string schema;
    PrecompiledSemanticSnapshot decoded;
    if (!reader.string(schema))
    {
        error = "precompiled semantic snapshot is truncated";
        return false;
    }
    if (schema != SnapshotSchema)
    {
        error = "precompiled semantic snapshot schema mismatch";
        return false;
    }
    if (!readDependencies(reader, decoded.dependencies) || !reader.u64(decoded.nodeCount) ||
        !reader.u64(decoded.typeCount) || !reader.string(decoded.nodeKindDigest) ||
        decoded.nodeKindDigest.size() != 64U || !readSections(reader, decoded.sections) ||
        !reader.remaining().empty())
    {
        error = "precompiled semantic snapshot is truncated or malformed";
        return false;
    }
    snapshot = std::move(decoded);
    error.clear();
    return true;
}
}
