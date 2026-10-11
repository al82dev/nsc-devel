#include "LlvmTestSupport.h"

#include <array>
#include <filesystem>
#include <fstream>

namespace nsc::suite
{
namespace
{
using namespace llvm_support;
using namespace nativesharp::llvm_backend;

void coffContract(TestContext& context, std::size_t variant)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("nsc-host-llvm-coff-" + std::to_string(variant) + ".obj");
    std::array<unsigned char, 20> bytes{};
    bytes[0] = 0x64;
    bytes[1] = 0x86;
    bytes[2] = 1;
    if (variant == 4) bytes[0] = 0x4c;
    if (variant == 5) bytes[2] = 0;
    if (variant == 6) bytes[16] = 4;
    if (variant != 3)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        const std::size_t size = variant == 7 ? 12 : bytes.size();
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(size));
    }
    CoffObjectVerifier verifier;
    const bool valid = verifier.verify(path);
    const bool expected = variant == 1 || variant == 2 || variant == 8;
    context.expect(valid == expected, "COFF verifier must enforce header, machine, section and optional-header invariants");
    if (expected)
        context.expect(verifier.info().machine == 0x8664 && verifier.info().sectionCount == 1 &&
            verifier.info().optionalHeaderSize == 0, "accepted COFF header fields must be published exactly");
    else
        context.expect(!verifier.error().empty(), "rejected COFF object must retain a diagnostic");
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

void debugKindContract(TestContext& context, std::size_t variant)
{
    context.expect(debugInformationKindName(DebugInformationKind::None) == "none", "debug none name must be exact");
    context.expect(debugInformationKindName(DebugInformationKind::LineTablesOnly) == "line-tables-only",
        "line-table debug name must be exact");
    context.expect(debugInformationKindName(DebugInformationKind::Full) == "full", "full debug name must be exact");
    if (variant >= 3)
        context.expect(debugInformationKindName(static_cast<DebugInformationKind>(255)).empty(),
            "unknown debug kind must have no invented name");
}

void prefixMapContract(TestContext& context, std::size_t variant)
{
    DebugPrefixMap map{std::filesystem::path("A:/sources"), std::filesystem::path("/src")};
    context.expect(map.sourcePrefix.generic_string() == "A:/sources", "source prefix must preserve portable spelling");
    context.expect(map.replacementPrefix.generic_string() == "/src", "replacement prefix must preserve identity");
    if (variant == 8)
    {
        std::vector<DebugPrefixMap> maps(2048, map);
        context.expect(maps.front().sourcePrefix == maps.back().sourcePrefix,
            "prefix-map value model must remain deterministic at scale");
    }
}
}

void registerLlvmFoundationModule(TestRegistry& registry)
{
    addLlvmContract(registry, 1, "integration", coffContract);
    addLlvmContract(registry, 7, "unit", debugKindContract);
    addLlvmContract(registry, 8, "unit", prefixMapContract);
}
}
