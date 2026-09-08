#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticRenderer.h"
#include "nativesharp/Driver/CrashBoundary.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/LLVM/LlvmVersion.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace
{
std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::vector<std::string_view> split(std::string_view value, char separator)
{
    std::vector<std::string_view> fields;
    std::size_t begin = 0;
    while (begin <= value.size())
    {
        const std::size_t end = value.find(separator, begin);
        fields.push_back(value.substr(begin, end == std::string_view::npos ? value.size() - begin : end - begin));
        if (end == std::string_view::npos)
        {
            break;
        }
        begin = end + 1;
    }
    return fields;
}
} // namespace

int main()
{
    using namespace nativesharp;
    using namespace nativesharp::diagnostics;
    using nativesharp::tests::TestContext;

    TestContext tests;

    DiagnosticEngine diagnostics;
    for (std::size_t index = 0; index < DiagnosticEngine::kDefaultErrorLimit + 9; ++index)
    {
        diagnostics.report(DiagnosticId::UnexpectedToken, DiagnosticSeverity::Error, SourceLocation(),
                           "synthetic cascade " + std::to_string(index));
    }
    tests.expect(diagnostics.errorCount() == DiagnosticEngine::kDefaultErrorLimit,
                 "diagnostic engine retains exactly the default error budget");
    tests.expect(diagnostics.errorLimitReached(), "diagnostic engine records that its error limit was reached");
    tests.expect(diagnostics.suppressedDiagnosticCount() == 9, "diagnostic engine counts suppressed diagnostics");
    tests.expect(diagnostics.diagnostics().size() == DiagnosticEngine::kDefaultErrorLimit,
                 "suppressed diagnostics do not consume unbounded storage");

    source::SourceManager emptySources;
    DiagnosticRenderer renderer(emptySources);
    std::ostringstream rendered;
    renderer.renderAll(diagnostics, rendered);
    tests.expect(
        rendered.str().find("diagnostic limit reached after 100 errors; 9 additional diagnostics suppressed") !=
            std::string::npos,
        "renderer explains the diagnostic budget instead of truncating silently");

    diagnostics.clear();
    tests.expect(!diagnostics.errorLimitReached(), "clear resets the diagnostic-limit state");
    tests.expect(diagnostics.suppressedDiagnosticCount() == 0, "clear resets the suppressed count");

    std::string hostileSource(512, '@');
    frontend::FrontendCompilation hostileCompilation;
    tests.expect(!hostileCompilation.compileBuffer("cascade.ns", std::move(hostileSource)),
                 "a large lexical error cascade is rejected");
    tests.expect(hostileCompilation.diagnostics().errorCount() == DiagnosticEngine::kDefaultErrorLimit,
                 "frontend compilation applies the diagnostic budget");
    tests.expect(hostileCompilation.diagnostics().suppressedDiagnosticCount() == 412,
                 "frontend compilation accounts for every suppressed lexical diagnostic");

    std::ostringstream boundaryError;
    const int success = driver::runWithCrashBoundary([]() { return 23; }, boundaryError);
    tests.expect(success == 23 && boundaryError.str().empty(), "crash boundary preserves successful return values");

    boundaryError.str({});
    boundaryError.clear();
    const int standardFailure = driver::runWithCrashBoundary(
        []() -> int { throw std::runtime_error("stage61 synthetic failure"); }, boundaryError);
    tests.expect(standardFailure == static_cast<int>(ExitCode::InternalError),
                 "standard exception becomes internal-error exit code 70");
    tests.expect(boundaryError.str().find("nsc: internal error: unhandled exception: stage61 synthetic failure") !=
                     std::string::npos,
                 "standard exception keeps a stable internal-error explanation");

    boundaryError.str({});
    boundaryError.clear();
    const int allocationFailure = driver::runWithCrashBoundary([]() -> int { throw std::bad_alloc(); }, boundaryError);
    tests.expect(allocationFailure == static_cast<int>(ExitCode::InternalError),
                 "allocation failure becomes internal-error exit code 70");
    tests.expect(boundaryError.str().find("out of memory") != std::string::npos, "allocation failure is named");

    boundaryError.str({});
    boundaryError.clear();
    const int unknownFailure = driver::runWithCrashBoundary([]() -> int { throw 61; }, boundaryError);
    tests.expect(unknownFailure == static_cast<int>(ExitCode::InternalError),
                 "unknown exception becomes internal-error exit code 70");
    tests.expect(boundaryError.str().find("unknown unhandled exception") != std::string::npos,
                 "unknown exception receives a deterministic diagnostic");

    const llvm_backend::LlvmVersion llvmVersion = llvm_backend::loadedLlvmVersion();
    tests.expect(llvmVersion.major != 0, "loaded LLVM reports a real version");
    tests.expect(llvm_backend::isSupportedLlvmVersion(llvmVersion),
                 "loaded LLVM belongs to the supported 23.x line (found " +
                     llvm_backend::llvmVersionString(llvmVersion) + ")");
    std::string llvmError;
    tests.expect(llvm_backend::verifyLoadedLlvmVersion(llvmError), "runtime LLVM version gate accepts this toolchain");
    tests.expect(llvmError.empty(), "accepted LLVM version leaves no error text");
    tests.expect(!llvm_backend::isSupportedLlvmVersion({22, 1, 0}) && !llvm_backend::isSupportedLlvmVersion({24, 0, 0}),
                 "runtime LLVM version gate rejects adjacent unqualified majors");

    driver::Driver compilerDriver;
    std::ostringstream versionOutput;
    std::ostringstream versionError;
    const std::vector<std::string_view> versionArguments = {"--print-llvm-version"};
    tests.expect(compilerDriver.run(versionArguments, versionOutput, versionError) == ExitCode::Success,
                 "--print-llvm-version succeeds with the supported runtime");
    tests.expect(versionOutput.str().find("LLVM " + llvm_backend::llvmVersionString(llvmVersion)) !=
                         std::string::npos &&
                     versionOutput.str().find("supported: 23.x") != std::string::npos,
                 "--print-llvm-version names loaded and supported versions");
    tests.expect(versionError.str().empty(), "supported LLVM version keeps stderr empty");

    const std::filesystem::path sourceRoot = NSC_SOURCE_DIR;
    const std::filesystem::path contractPath = sourceRoot / "spec/NativeSharp_Compiler_Contract_1_0.md";
    const std::filesystem::path mapPath = sourceRoot / "spec/NativeSharp_Compiler_1_0_conformance.map";
    const std::string contract = readFile(contractPath);
    const std::string map = readFile(mapPath);
    tests.expect(!contract.empty(), "compiler contract is readable");
    tests.expect(!map.empty(), "compiler conformance map is readable");

    std::unordered_set<std::string> ids;
    std::istringstream rows(map);
    std::string row;
    std::size_t mappedClauseCount = 0;
    while (std::getline(rows, row))
    {
        if (row.empty() || row.starts_with('#'))
        {
            continue;
        }
        const std::vector<std::string_view> fields = split(row, '|');
        tests.expect(fields.size() == 5, "compiler conformance row has five fields: " + row);
        if (fields.size() != 5)
        {
            continue;
        }

        const std::string id(fields[0]);
        tests.expect(ids.insert(id).second, "compiler contract id is unique: " + id);
        tests.expect(fields[1] == "implemented", "compiler contract clause is implemented: " + id);
        tests.expect(contract.find("## " + id + " —") != std::string::npos,
                     "compiler contract contains mapped clause: " + id);

        for (const std::string_view path : split(fields[3], ';'))
        {
            tests.expect(std::filesystem::is_regular_file(sourceRoot / std::filesystem::path(path)),
                         "implementation evidence exists for " + id + ": " + std::string(path));
        }
        for (const std::string_view path : split(fields[4], ';'))
        {
            tests.expect(std::filesystem::exists(sourceRoot / std::filesystem::path(path)),
                         "executable evidence exists for " + id + ": " + std::string(path));
        }
        ++mappedClauseCount;
    }
    tests.expect(mappedClauseCount >= 8, "all eight E8 compiler-hardening clauses remain mapped");

    return tests.result();
}
