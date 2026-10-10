#include "RuntimeTestSupport.h"

#include <array>
#include <fstream>
#include <iterator>
#include <set>

namespace nsc::suite
{
namespace
{
using namespace runtime_support;

void writeAndVerifyContractFile(TestContext& context, std::size_t variant,
    const std::filesystem::path& artifact, std::string& error);

void abiCatalogueContract(TestContext& context, std::size_t variant)
{
    using namespace runtime;
    if (variant == 1)
        context.expect(kRuntimeAbiMajor == 1 && kRuntimeAbiMinor == 2,
            "runtime ABI version must remain 1.2");
    else if (variant == 2)
        context.expect(kRuntimeSymbolSetVersion == 4 && kRuntimeAbiSymbols.size() == 99,
            "runtime symbol set 4 must contain exactly 99 entries");
    else if (variant == 3)
        context.expect(std::ranges::none_of(kRuntimeAbiSymbols, [](const RuntimeAbiSymbol& symbol)
            { return symbol.name == "__nativesharp_future_when_all"; }),
            "an absent capability hook must not appear in the frozen catalogue");
    else if (variant == 4)
        context.expect(std::ranges::all_of(kRuntimeAbiSymbols, [](const RuntimeAbiSymbol& symbol)
            { return !symbol.name.empty() && !symbol.llvmType.empty(); }),
            "every runtime ABI entry must have a name and canonical type");
    else if (variant == 5)
    {
        std::set<std::string_view> names;
        for (const RuntimeAbiSymbol& symbol : kRuntimeAbiSymbols) names.insert(symbol.name);
        context.expect(names.size() == kRuntimeAbiSymbols.size(), "runtime ABI names must be unique");
    }
    else if (variant == 6)
        context.expect(std::ranges::count_if(kRuntimeAbiSymbols, [](const RuntimeAbiSymbol& symbol)
            { return symbol.kind == RuntimeAbiSymbolKind::Data; }) == 1,
            "the symbol catalogue must distinguish its single TLS data export");
    else if (variant == 7)
        context.expect(std::ranges::is_sorted(kRuntimeAbiSymbols,
            [](const RuntimeAbiSymbol& left, const RuntimeAbiSymbol& right)
            { return left.name < right.name; }),
            "runtime ABI names must retain canonical ordering");
    else
        for (std::size_t repetition = 0; repetition < 4096; ++repetition)
            context.expect(kRuntimeAbiSymbols[repetition % kRuntimeAbiSymbols.size()].name.starts_with(
                "__nativesharp_"), "catalogue scans must preserve the NativeSharp prefix");
}

void manifestContract(TestContext& context, std::size_t variant)
{
    using namespace runtime;
    const std::string manifest = runtimeAbiContractManifest();
    if (variant == 1)
        context.expect(manifest.starts_with("NSRUNTIMEABI 1.2\nsymbol_set=4\n"),
            "manifest must start with the frozen ABI and symbol-set versions");
    else if (variant == 2)
        context.expect(manifest.find("target=x86_64-pc-windows-msvc\n") != std::string::npos &&
            manifest.find("calling_convention=win64-c\n") != std::string::npos,
            "manifest must serialize target and calling convention");
    else if (variant == 3)
        for (const RuntimeAbiSymbol& symbol : kRuntimeAbiSymbols)
            context.expect(manifest.find(symbol.name) != std::string::npos,
                "every required catalogue symbol must be serialized");
    else if (variant == 4)
        context.expect(manifest.find("function __nativesharp_alloc ptr(i64)\n") != std::string::npos &&
            manifest.find("data __nativesharp_exception_tls_index i32\n") != std::string::npos,
            "manifest grammar must distinguish exact function and data entries");
    else if (variant == 5)
        context.expect(std::ranges::count(manifest, '\n') == 104 && manifest.find('\r') == std::string::npos,
            "manifest must contain five headers and 99 LF-terminated symbols");
    else if (variant == 6)
        context.expect(manifest.ends_with("function __nativesharp_wait_for_handle_timeout i1(i64,i32)\n"),
            "manifest must preserve the canonical final symbol and type");
    else if (variant == 7)
        context.expect(runtimeAbiContractManifest() == manifest,
            "manifest serialization must be deterministic");
    else
        for (std::size_t repetition = 0; repetition < 256; ++repetition)
            context.expect(runtimeAbiContractManifest() == manifest,
                "repeated manifest serialization must remain byte-identical");
}

void fingerprintContract(TestContext& context, std::size_t variant)
{
    using namespace runtime;
    const std::string manifest = runtimeAbiContractManifest();
    const std::uint64_t independent = independentFnv1a(manifest);
    if (variant == 1)
        context.expect(runtimeAbiContractFingerprint() == independent,
            "runtime ABI fingerprint must equal independent FNV-1a");
    else if (variant == 2)
        context.expect(runtimeAbiContractFingerprintHex() == "2cddeff864c7dfc6",
            "runtime ABI hexadecimal identity must match the frozen value");
    else if (variant == 3)
        context.expect(runtimeAbiContractFingerprintHex() != independentHex(independentFnv1a("")),
            "missing manifest input must not share the runtime identity");
    else if (variant == 4)
        context.expect(runtimeAbiContractFingerprintHex().size() == 16 &&
            std::ranges::all_of(runtimeAbiContractFingerprintHex(), [](char value)
            { return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f'); }),
            "fingerprint text must be fixed-width lowercase hexadecimal");
    else if (variant == 5)
        context.expect(independentHex(independent).size() == 16,
            "independent formatting must preserve leading hexadecimal zeroes");
    else if (variant == 6)
        context.expect(runtimeAbiContractFingerprintHex() == independentHex(independent),
            "numeric and hexadecimal fingerprints must project the same identity");
    else if (variant == 7)
        context.expect(runtimeAbiContractFingerprint() == runtimeAbiContractFingerprint(),
            "fingerprint calculation must be deterministic");
    else
        for (std::size_t repetition = 0; repetition < 4096; ++repetition)
            context.expect(runtimeAbiContractFingerprint() == independent,
                "fingerprint calculation must remain stable at scale");
}

void contractFileCase(TestContext& context, std::size_t variant)
{
    using namespace runtime;
    const std::filesystem::path root = casePath(4, variant);
    const std::filesystem::path artifact = root / "nested" / "NativeSharpRuntime.abi";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::string error;
    if (variant == 3)
        context.expect(!verifyRuntimeAbiContractFile(artifact, error) && !error.empty(),
            "missing ABI artifact must be rejected at the file boundary");
    else if (variant == 4)
        context.expect(!writeRuntimeAbiContract({}, error) && !error.empty(),
            "empty ABI artifact destination must be rejected");
    else
        writeAndVerifyContractFile(context, variant, artifact, error);
    std::filesystem::remove_all(root, ignored);
}

void registerIdentityContracts(TestRegistry& registry)
{
    addRuntimeContract(registry, 1, "unit", abiCatalogueContract, {}, {"abi-catalogue"});
    addRuntimeContract(registry, 2, "unit", manifestContract, {}, {"abi-manifest"});
    addRuntimeContract(registry, 3, "unit", fingerprintContract, {}, {"abi-fingerprint"});
    addRuntimeContract(registry, 4, "integration", contractFileCase, {}, {"abi-file"});
}

void writeAndVerifyContractFile(TestContext& context, std::size_t variant,
    const std::filesystem::path& artifact, std::string& error)
{
    using namespace runtime;
    context.expect(writeRuntimeAbiContract(artifact, error) && error.empty(),
        "ABI writer must create the canonical artifact");
    if (variant == 5)
    {
        std::ofstream stream(artifact, std::ios::binary | std::ios::app);
        stream << "function __nativesharp_future void()\n";
        stream.close();
        context.expect(!verifyRuntimeAbiContractFile(artifact, error) &&
            error.find("expected fingerprint 2cddeff864c7dfc6") != std::string::npos,
            "additional ABI content must be rejected with the expected fingerprint");
        return;
    }
    if (variant == 6)
    {
        std::string crlf = runtimeAbiContractManifest();
        std::size_t offset = 0;
        while ((offset = crlf.find('\n', offset)) != std::string::npos)
        {
            crlf.insert(offset, 1, '\r');
            offset += 2;
        }
        std::ofstream stream(artifact, std::ios::binary | std::ios::trunc);
        stream << crlf;
    }
    const std::size_t repetitions = variant == 8 ? 64 : (variant == 7 ? 2 : 1);
    for (std::size_t index = 0; index < repetitions; ++index)
    {
        context.expect(verifyRuntimeAbiContractFile(artifact, error) && error.empty(),
            "exact ABI artifact must verify deterministically");
        if (variant == 7 && index + 1 < repetitions)
            context.expect(writeRuntimeAbiContract(artifact, error),
                "repeated ABI writes must replace the artifact deterministically");
    }
}
}

void registerRuntimeAbiIdentityModule(TestRegistry& registry)
{
    registerIdentityContracts(registry);
}
}
