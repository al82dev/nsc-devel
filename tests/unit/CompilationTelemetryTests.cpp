#include "nativesharp/Basic/CompilationEvent.h"
#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/CompilationTelemetry.h"
#include "nativesharp/Driver/CompilerConsole.h"
#include "nativesharp/Driver/DiagnosticBundle.h"

#include "../support/TestContext.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int main()
{
    using nativesharp::basic::CompilationEvent;
    using nativesharp::basic::CompilationEventKind;
    using nativesharp::driver::CompilerLanguage;
    using nativesharp::driver::CompilerLanguageScope;
    using nativesharp::driver::ProgressFormat;
    using nativesharp::driver::parseProgressFormat;
    using nativesharp::driver::renderCompilationEvent;
    using nativesharp::driver::renderCompilationTimeTraceJson;
    using nativesharp::tests::TestContext;
    using namespace std::chrono_literals;

    TestContext tests;

    ProgressFormat format = ProgressFormat::Text;
    tests.expect(parseProgressFormat("text", format) && format == ProgressFormat::Text &&
            parseProgressFormat("json", format) && format == ProgressFormat::Json &&
            !parseProgressFormat("xml", format),
        "progress format accepts only the stable text and json renderers");

    const CompilationEvent finished{
        CompilationEventKind::PhaseFinished, "syntax-parse", {}, 0, 0, 17ms};
    tests.expect(renderCompilationEvent(finished, ProgressFormat::Text) ==
            "-- NSC: syntax-parse finished (17 ms)",
        "text progress preserves the existing CMake-style phase contract");
    {
        const CompilerLanguageScope language(CompilerLanguage::Spanish);
        tests.expect(renderCompilationEvent(finished, ProgressFormat::Text) ==
                "-- NSC: syntax-parse finalizada (17 ms)",
            "text progress localizes human-readable phase completion");
    }

    const CompilationEvent source{
        CompilationEventKind::WorkFinished, "syntax-read", "A:/project/source.ns", 3, 8};
    const std::string json = renderCompilationEvent(source, ProgressFormat::Json);
    tests.expect(json.find("\"schema\":\"nativesharp-progress-event\"") != std::string::npos &&
            json.find("\"event\":\"work-finished\"") != std::string::npos &&
            json.find("\"phase\":\"syntax-read\"") != std::string::npos &&
            json.find("\"detail\":\"A:/project/source.ns\"") != std::string::npos &&
            json.find("\"current\":3,\"total\":8") != std::string::npos,
        "JSON progress exposes stable structured fields without localized keys");

    const std::string trace = renderCompilationTimeTraceJson(
        "compiler output\n"
        "timing phase=syntax-read us=120 peak_working_set_bytes=4096\n"
        "timing phase=syntax-parse us=340 peak_working_set_bytes=8192\n"
        "timing total_ms=2 peak_working_set_bytes=8192\n"
        "timing project_descriptor_file_reads=2 project_descriptor_cache_hits=1\n",
        0);
    tests.expect(trace.find("\"schema\": \"nativesharp-time-trace\"") != std::string::npos &&
            trace.find("\"exit_code\": 0") != std::string::npos &&
            trace.find("\"total_ms\": 2") != std::string::npos &&
            trace.find("\"peak_working_set_bytes\": 8192") != std::string::npos &&
            trace.find("\"project_descriptor_file_reads\": 2") != std::string::npos &&
            trace.find("\"project_descriptor_cache_hits\": 1") != std::string::npos &&
            trace.find("{\"name\": \"syntax-read\", \"duration_us\": 120, "
                       "\"peak_working_set_bytes\": 4096}") !=
                std::string::npos &&
            trace.find("{\"name\": \"syntax-parse\", \"duration_us\": 340, "
                       "\"peak_working_set_bytes\": 8192}") !=
                std::string::npos &&
            trace.find("compiler output") == std::string::npos,
        "time trace contains timings rather than embedding the complete compilation log");

    const std::filesystem::path fixtureDirectory =
        std::filesystem::path(NSC_BINARY_DIR) / "tanda01-telemetry-fixture";
    std::error_code filesystemError;
    std::filesystem::remove_all(fixtureDirectory, filesystemError);
    std::filesystem::create_directories(fixtureDirectory, filesystemError);
    const std::filesystem::path projectPath = fixtureDirectory / "broken.nsproj";
    const std::filesystem::path bundlePath = fixtureDirectory / "failure.nsdiag";
    {
        std::ofstream project(projectPath, std::ios::binary | std::ios::trunc);
        project << "<Project><Broken>";
    }
    const std::string projectArgument = projectPath.string();
    const std::vector<std::string_view> bundleArguments = {
        "--check-project-oop-mir", projectArgument, "--token=do-not-persist"};
    std::string bundleError;
    const bool bundleWritten = nativesharp::driver::writeDiagnosticBundle(
        projectPath, bundlePath, nativesharp::ExitCode::CompilationError,
        "captured output", "captured error", {}, bundleArguments, bundleError);
    std::ifstream bundle(bundlePath, std::ios::binary);
    const std::string bundleContents((std::istreambuf_iterator<char>(bundle)), {});
    tests.expect(bundleWritten && bundleError.empty() &&
            bundleContents.starts_with("NSDIAG 1\n") &&
            bundleContents.find("project_graph_error=") != std::string::npos &&
            bundleContents.find("input=\"project:root\"") != std::string::npos &&
            bundleContents.find("--token=<redacted>") != std::string::npos &&
            bundleContents.find("do-not-persist") == std::string::npos &&
            !std::filesystem::exists(bundlePath.string() + ".tmp"),
        "failure bundle survives an invalid project, fingerprints the root, redacts secrets, "
        "and publishes transactionally");
    bundle.close();
    std::filesystem::remove_all(fixtureDirectory, filesystemError);

    return tests.result();
}
