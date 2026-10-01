#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Driver/ToolchainContract.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Runtime/RuntimeAbiContract.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
std::string readFile(const std::filesystem::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

bool writeFile(const std::filesystem::path &path, std::string_view contents)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return static_cast<bool>(stream);
}

std::string withoutLine(std::string text, std::string_view line)
{
    const std::string completeLine = std::string(line) + '\n';
    const std::size_t position = text.find(completeLine);
    if (position != std::string::npos)
    {
        text.erase(position, completeLine.size());
    }
    return text;
}

std::string replacing(std::string text, std::string_view from, std::string_view to)
{
    const std::size_t position = text.find(from);
    if (position != std::string::npos)
    {
        text.replace(position, from.size(), to);
    }
    return text;
}

bool verifyFails(const std::filesystem::path &path,
    std::string_view contents,
    std::string_view expectedError,
    std::string &observedError)
{
    if (!writeFile(path, contents))
    {
        observedError = "test could not write malformed fixture";
        return false;
    }
    observedError.clear();
    return !nativesharp::driver::verifyToolchainContract(path, observedError) &&
           observedError.find(expectedError) != std::string::npos;
}
} // namespace

int main()
{
    using namespace nativesharp;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const std::string contract = driver::currentToolchainContract();
    const std::string metadataFormat =
        std::to_string(interop::kMetadataFormatMajor) + "." +
        std::to_string(interop::kMetadataFormatMinor);
    const std::string nativeAbi =
        std::to_string(interop::kNativeAbiMajor) + "." + std::to_string(interop::kNativeAbiMinor);
    const std::string runtimeAbi =
        std::to_string(interop::kRuntimeAbiMajor) + "." + std::to_string(interop::kRuntimeAbiMinor);
    const std::string runtimeSymbolSet = std::to_string(runtime::kRuntimeSymbolSetVersion);
    const std::string mirSchema =
        std::to_string(frontend::kProjectOopMirSchemaMajor) + "." +
        std::to_string(frontend::kProjectOopMirSchemaMinor);
    tests.expect(contract.starts_with("NSTOOLCHAIN 2\n"), "contract has a versioned magic header");
    tests.expect(contract.ends_with('\n'), "contract has one canonical final newline");
    tests.expect(contract.find("compiler_release=3.0.0\n") != std::string::npos,
        "contract identifies the compatible compiler release");
    tests.expect(contract.find("language=3.0.0\n") != std::string::npos, "contract identifies the language line");
    tests.expect(contract.find("stage=" + std::string(kCompilerStage) + "\n") != std::string::npos,
        "contract identifies the current compiler stage");
    tests.expect(contract.find("target=x86_64-pc-windows-msvc\n") != std::string::npos,
        "contract identifies the supported target");
    tests.expect(contract.find("llvm=23.x\n") != std::string::npos, "contract identifies the qualified LLVM line");
    tests.expect(contract.find("nsmeta=" + metadataFormat + "\n") != std::string::npos,
        "contract identifies the library metadata format");
    tests.expect(contract.find("native_abi=" + nativeAbi + "\n") != std::string::npos &&
                     contract.find("runtime_abi=" + runtimeAbi + "\n") != std::string::npos,
        "contract identifies native and runtime ABI versions");
    tests.expect(contract.find("runtime_symbol_set=" + runtimeSymbolSet + "\n") != std::string::npos &&
                     contract.find("mir_schema=" + mirSchema + "\n") != std::string::npos,
        "contract freezes the runtime symbol set and MIR schema");
    tests.expect(contract.find("mangling_abi=1\n") != std::string::npos &&
                     contract.find("object_abi=2\n") != std::string::npos &&
                     contract.find("delegate_abi=1\n") != std::string::npos &&
                     contract.find("exception_abi=1\n") != std::string::npos,
        "contract identifies every physical ABI sub-contract");
    tests.expect(contract.find("managed_delegate_cell_size=32\n") != std::string::npos,
        "contract fixes the managed delegate cell size");
    tests.expect(contract.find("supported_linkers=lld-link,NSLink\n") != std::string::npos,
        "contract names both supported linker routes");
    tests.expect(contract.find("certified_optimizations=O0,O2\n") != std::string::npos,
        "contract distinguishes certified optimization configurations");
    tests.expect(contract.find("artifact_kinds=exe,static-lib,dll\n") != std::string::npos,
        "contract names the distributable native artifacts");
    tests.expect(contract.find("build.") == std::string::npos && contract.find("build_id") == std::string::npos,
        "per-build identity does not destroy compatibility reproducibility");
    tests.expect(contract.find("installation_files=1\n") != std::string::npos &&
                     contract.find("installation_manifest=1\n") != std::string::npos &&
                     contract.find("reproducibility_report=1\n") != std::string::npos &&
                     contract.find("update_plan=1\n") != std::string::npos &&
                     contract.find("compatibility_policy=1\n") != std::string::npos,
        "contract identifies every versioned package lifecycle format");

    std::size_t lineCount = 0;
    for (const char character : contract)
    {
        if (character == '\n')
        {
            ++lineCount;
        }
    }
    tests.expect(lineCount == 24, "canonical contract has one header and twenty-three ordered fields");

    const std::filesystem::path evidence = nativesharp::tests::evidenceRoot("nativesharp_stage62_toolchain_contract");
    std::error_code filesystemError;
    std::filesystem::remove_all(evidence, filesystemError);
    filesystemError.clear();
    std::filesystem::create_directories(evidence, filesystemError);
    tests.expect(!filesystemError, "isolated Stage 62 evidence directory is available");

    std::string error;
    const std::filesystem::path written = evidence / "nested/current.contract";
    tests.expect(driver::writeToolchainContract(written, error), "contract writer creates parent directories");
    tests.expect(error.empty(), "successful contract write keeps its error empty");
    tests.expect(readFile(written) == contract, "contract writer preserves canonical bytes");
    tests.expect(driver::verifyToolchainContract(written, error), "current compiler accepts its own contract");
    tests.expect(error.empty(), "successful contract verification keeps its error empty");

    std::string crlf;
    crlf.reserve(contract.size() + lineCount);
    for (const char character : contract)
    {
        if (character == '\n')
        {
            crlf += '\r';
        }
        crlf += character;
    }
    const std::filesystem::path crlfPath = evidence / "crlf.contract";
    tests.expect(writeFile(crlfPath, crlf) && driver::verifyToolchainContract(crlfPath, error),
        "verifier accepts transport-level CRLF without changing field semantics");

    const std::filesystem::path malformed = evidence / "malformed.contract";
    tests.expect(verifyFails(malformed,
                     replacing(contract, "NSTOOLCHAIN 2", "NSTOOLCHAIN 9"),
                     "unsupported toolchain contract header",
                     error),
        "future contract format is rejected with its header named");
    tests.expect(verifyFails(malformed, contract + "future_field=1\n", "unknown toolchain contract field", error),
        "unknown fields are rejected instead of silently ignored");
    tests.expect(verifyFails(malformed, contract + "language=3.0.0\n", "duplicate toolchain contract field", error),
        "duplicate fields are rejected");
    tests.expect(verifyFails(malformed,
                     withoutLine(contract, "runtime_abi=" + runtimeAbi),
                     "missing toolchain contract field 'runtime_abi'",
                     error),
        "missing compatibility fields are diagnosed precisely");
    tests.expect(verifyFails(malformed,
                     withoutLine(contract, "mir_schema=" + mirSchema),
                     "missing toolchain contract field 'mir_schema'",
                     error),
        "missing MIR schema identity is diagnosed precisely");
    tests.expect(verifyFails(malformed,
                     replacing(contract, "llvm=23.x", "llvm=24.x"),
                     "incompatible toolchain contract field 'llvm'",
                     error),
        "incompatible field values name found and expected contracts");
    tests.expect(verifyFails(malformed,
                     replacing(contract,
                         "language=3.0.0\nstage=" + std::string(kCompilerStage),
                         "stage=" + std::string(kCompilerStage) + "\nlanguage=3.0.0"),
                     "out of canonical order",
                     error),
        "otherwise valid fields still require deterministic canonical order");
    tests.expect(
        verifyFails(
            malformed, replacing(contract, "language=3.0.0", "language"), "malformed toolchain contract line", error),
        "malformed key-value lines identify their line");

    driver::Driver compiler;
    std::ostringstream output;
    std::ostringstream driverError;
    const std::vector<std::string_view> printArguments = {"--print-toolchain-contract"};
    tests.expect(
        compiler.run(printArguments, output, driverError) == ExitCode::Success, "public print command succeeds");
    tests.expect(output.str() == contract && driverError.str().empty(),
        "public print command emits only canonical contract bytes");

    const std::filesystem::path driverWritten = evidence / "driver.contract";
    const std::string driverWrittenText = driverWritten.string();
    const std::vector<std::string_view> writeArguments = {"--write-toolchain-contract", driverWrittenText};
    output.str({});
    output.clear();
    tests.expect(
        compiler.run(writeArguments, output, driverError) == ExitCode::Success, "public write command succeeds");
    tests.expect(readFile(driverWritten) == contract && output.str().find("written") != std::string::npos,
        "public write command creates and acknowledges the canonical artifact");

    const std::vector<std::string_view> verifyArguments = {"--verify-toolchain-contract", driverWrittenText};
    output.str({});
    output.clear();
    tests.expect(compiler.run(verifyArguments, output, driverError) == ExitCode::Success,
        "public verify command accepts a compatible package contract");
    tests.expect(output.str().find("verified") != std::string::npos && driverError.str().empty(),
        "public verify command reports success without stderr noise");

    const std::string absentText = (evidence / "absent.contract").string();
    const std::vector<std::string_view> absentArguments = {"--verify-toolchain-contract", absentText};
    output.str({});
    output.clear();
    driverError.str({});
    driverError.clear();
    tests.expect(compiler.run(absentArguments, output, driverError) == ExitCode::CompilationError,
        "missing contract is a controlled input failure");
    tests.expect(driverError.str().find("nsc: error: cannot open toolchain contract") != std::string::npos,
        "missing contract produces a useful stable diagnostic");

    const std::filesystem::path generatedContract = NSC_TOOLCHAIN_CONTRACT_PATH;
    tests.expect(std::filesystem::is_regular_file(generatedContract),
        "building nsc produces the package contract as a build artifact");
    tests.expect(
        readFile(generatedContract) == contract, "build artifact was generated by this exact compiler contract");

    const std::filesystem::path sourceRoot = NSC_SOURCE_DIR;
    const std::string cmake = readFile(sourceRoot / "CMakeLists.txt");
    tests.expect(cmake.find("install(FILES \"${NSC_TOOLCHAIN_CONTRACT}\"") != std::string::npos,
        "install rules ship the generated toolchain contract");
    const std::string normative = readFile(sourceRoot / "spec/NativeSharp_Compiler_Contract_1_0.md");
    const std::string conformance = readFile(sourceRoot / "spec/NativeSharp_Compiler_1_0_conformance.map");
    tests.expect(normative.find("## NSC-PKG-001 —") != std::string::npos &&
                     normative.find("## NSC-PKG-002 —") != std::string::npos,
        "normative compiler contract defines package production and "
        "verification");
    tests.expect(conformance.find("NSC-PKG-001|implemented|") != std::string::npos &&
                     conformance.find("NSC-PKG-002|implemented|") != std::string::npos,
        "package clauses are connected to implementation and executable "
        "evidence");

    return tests.resultKeeping(evidence);
}
