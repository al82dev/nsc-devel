#include "nativesharp/Compilation/PrecompiledSemanticSnapshot.h"
#include "nativesharp/Compilation/PrecompiledSemanticSnapshotCodec.h"

#include "../support/TestContext.h"

#include <string>

// EN: Plan item 1.2: the semantic snapshot header (dependency NSMETA digests, node and type counts,
//     node-kind digest) and its model sections round-trip exactly, and malformed bytes are rejected.
// ES: Punto 1.2 del plan: la cabecera del snapshot semántico (digests de NSMETA de dependencias,
//     cantidades de nodos y tipos, digest de clases de nodo) y sus secciones de modelos hacen ida y
//     vuelta exacta, y los bytes mal formados se rechazan.

namespace
{
using namespace nativesharp;

compilation::PrecompiledSemanticSnapshot sampleSnapshot()
{
    compilation::PrecompiledSemanticSnapshot snapshot;
    snapshot.dependencies.push_back({"NativeSharp.Core", std::string(64, 'a')});
    snapshot.dependencies.push_back({"NativeSharp.Math.Core", std::string(64, 'b')});
    snapshot.nodeCount = 1234U;
    snapshot.typeCount = 567U;
    snapshot.nodeKindDigest = std::string(64, 'c');
    snapshot.sections.push_back({"names", std::string("names\0payload", 13)});
    snapshot.sections.push_back({"members", "members-payload"});
    return snapshot;
}

bool sameSnapshot(
    const compilation::PrecompiledSemanticSnapshot& left,
    const compilation::PrecompiledSemanticSnapshot& right)
{
    if (left.dependencies.size() != right.dependencies.size() ||
        left.sections.size() != right.sections.size() || left.nodeCount != right.nodeCount ||
        left.typeCount != right.typeCount || left.nodeKindDigest != right.nodeKindDigest)
    {
        return false;
    }
    for (std::size_t index = 0; index < left.dependencies.size(); ++index)
    {
        if (left.dependencies[index].moduleName != right.dependencies[index].moduleName ||
            left.dependencies[index].metadataDigest != right.dependencies[index].metadataDigest)
        {
            return false;
        }
    }
    for (std::size_t index = 0; index < left.sections.size(); ++index)
    {
        if (left.sections[index].model != right.sections[index].model ||
            left.sections[index].payload != right.sections[index].payload)
        {
            return false;
        }
    }
    return true;
}
}

int main()
{
    tests::TestContext tests;
    using Codec = compilation::PrecompiledSemanticSnapshotCodec;
    const compilation::PrecompiledSemanticSnapshot snapshot = sampleSnapshot();
    const std::string bytes = Codec::encode(snapshot);

    compilation::PrecompiledSemanticSnapshot decoded;
    std::string error;
    tests.expect(Codec::decode(bytes, decoded, error), "a snapshot decodes: " + error);
    tests.expect(sameSnapshot(snapshot, decoded), "the decoded snapshot equals the encoded one");
    tests.expect(Codec::encode(decoded) == bytes, "re-encoding gives the same bytes");

    compilation::PrecompiledSemanticSnapshot rejected;
    tests.expect(!Codec::decode(bytes.substr(0, bytes.size() - 1), rejected, error),
        "a truncated snapshot is rejected");
    tests.expect(!Codec::decode(bytes + "x", rejected, error), "trailing bytes are rejected");
    std::string otherSchema = bytes;
    otherSchema[otherSchema.find("-v1")] = '_';
    tests.expect(!Codec::decode(otherSchema, rejected, error), "another schema is rejected");

    compilation::PrecompiledSemanticSnapshot shortDigest = sampleSnapshot();
    shortDigest.dependencies[0].metadataDigest = "abc";
    tests.expect(!Codec::decode(Codec::encode(shortDigest), rejected, error),
        "a dependency digest that is not 64 hex digits is rejected");
    compilation::PrecompiledSemanticSnapshot repeated = sampleSnapshot();
    repeated.sections.push_back({"names", "again"});
    tests.expect(!Codec::decode(Codec::encode(repeated), rejected, error),
        "a repeated model section is rejected");

    compilation::PrecompiledSemanticSnapshot empty;
    empty.nodeKindDigest = std::string(64, '0');
    compilation::PrecompiledSemanticSnapshot decodedEmpty;
    tests.expect(Codec::decode(Codec::encode(empty), decodedEmpty, error) &&
        decodedEmpty.dependencies.empty() && decodedEmpty.sections.empty(),
        "a snapshot without dependencies or sections round-trips");
    return tests.result();
}
