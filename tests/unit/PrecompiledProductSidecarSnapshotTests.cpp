#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Compilation/PrecompiledProductSidecar.h"
#include "nativesharp/Compilation/PrecompiledProductSidecarCodec.h"

#include "../../src/Frontend/Cache/CachePayloadWriter.h"
#include "../support/TestContext.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// EN: Sidecar schema v2 (plan item 1.1). A sidecar without a semantic snapshot must keep the v1
//     bytes exactly; a sidecar with one is written as v2 and both schemas are read back. The v1
//     reference encoder below is a literal copy of the encoder before v2.
// ES: Esquema v2 del sidecar (punto 1.1 del plan). Un sidecar sin snapshot semántico debe conservar
//     exactamente los bytes de v1; uno con snapshot se escribe como v2 y se leen ambos esquemas. El
//     codificador de referencia v1 de abajo es una copia literal del codificador anterior a v2.

namespace
{
using namespace nativesharp;
using Writer = frontend::internal::CachePayloadWriter;

std::string hexDigest(std::string_view bytes)
{
    const std::span<const std::uint8_t> view{
        reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()};
    return basic::sha256Hex(basic::sha256(view));
}

// EN: The v1 encoder as it was before schema v2.
// ES: El codificador v1 tal como era antes del esquema v2.
std::string legacyV1Encode(
    const compilation::PrecompiledProductSidecar& sidecar,
    std::span<const std::uint8_t> metadataBytes)
{
    std::string bytes;
    Writer::string(bytes, "nsc-precompiled-product-sidecar-v1");
    Writer::string(bytes, kCompilerBuildId);
    Writer::u64(bytes, static_cast<std::uint64_t>(metadataBytes.size()));
    Writer::string(bytes, basic::sha256Hex(basic::sha256(metadataBytes)));
    Writer::string(bytes, sidecar.publicApiSource);
    Writer::string(bytes, sidecar.publicTemplateSource);
    Writer::string(bytes, sidecar.publicApiSyntax);
    Writer::string(bytes, sidecar.publicTemplateSyntax);
    const std::string checksum = hexDigest(bytes);
    Writer::string(bytes, checksum);
    return bytes;
}

compilation::PrecompiledProductSidecar sampleSidecar()
{
    compilation::PrecompiledProductSidecar sidecar;
    sidecar.publicApiSource = "namespace Sample { public class A { } }";
    sidecar.publicTemplateSource = "namespace Sample { public class B<T> { } }";
    sidecar.publicApiSyntax = std::string("api-syntax\0bytes", 16);
    sidecar.publicTemplateSyntax = "template-syntax";
    return sidecar;
}
}

int main()
{
    tests::TestContext tests;
    const std::vector<std::uint8_t> metadata = {0x4eU, 0x53U, 0x4dU, 0x45U, 0x54U, 0x41U, 0x01U};
    const std::vector<std::uint8_t> otherMetadata = {0x4eU, 0x53U, 0x4dU, 0x45U, 0x54U, 0x41U, 0x02U};
    using Codec = compilation::PrecompiledProductSidecarCodec;

    const compilation::PrecompiledProductSidecar withoutSnapshot = sampleSidecar();
    const std::string v1 = Codec::encode(withoutSnapshot, metadata);
    tests.expect(v1 == legacyV1Encode(withoutSnapshot, metadata),
        "a sidecar without a snapshot is still written as v1, byte for byte");

    compilation::PrecompiledProductSidecar decodedV1;
    decodedV1.publicApiSnapshot = "stale";
    std::string error;
    tests.expect(Codec::decode(v1, metadata, decodedV1, error), "a v1 sidecar decodes: " + error);
    tests.expect(decodedV1.publicApiSnapshot.empty(), "a v1 sidecar has no snapshot");
    tests.expect(decodedV1.publicApiSyntax == withoutSnapshot.publicApiSyntax &&
        decodedV1.publicTemplateSource == withoutSnapshot.publicTemplateSource,
        "a v1 sidecar keeps its four products");

    compilation::PrecompiledProductSidecar withSnapshot = sampleSidecar();
    withSnapshot.publicApiSnapshot = std::string("snapshot\0payload", 16);
    const std::string v2 = Codec::encode(withSnapshot, metadata);
    tests.expect(v2.find("nsc-precompiled-product-sidecar-v2") != std::string::npos,
        "a sidecar with a snapshot is written as v2");
    compilation::PrecompiledProductSidecar decodedV2;
    tests.expect(Codec::decode(v2, metadata, decodedV2, error), "a v2 sidecar decodes: " + error);
    tests.expect(decodedV2.publicApiSnapshot == withSnapshot.publicApiSnapshot,
        "a v2 sidecar returns its snapshot");
    tests.expect(decodedV2.publicApiSource == withSnapshot.publicApiSource &&
        decodedV2.publicTemplateSyntax == withSnapshot.publicTemplateSyntax,
        "a v2 sidecar keeps its four products");

    std::string tampered = v2;
    tampered[tampered.size() / 2] = static_cast<char>(tampered[tampered.size() / 2] ^ 0x5a);
    compilation::PrecompiledProductSidecar rejected;
    tests.expect(!Codec::decode(tampered, metadata, rejected, error),
        "a modified v2 sidecar is rejected");
    tests.expect(!Codec::decode(v2, otherMetadata, rejected, error),
        "a v2 sidecar of another NSMETA is rejected");
    tests.expect(!Codec::decode(v2.substr(0, v2.size() - 1), metadata, rejected, error),
        "a truncated v2 sidecar is rejected");
    return tests.result();
}
