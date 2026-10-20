#include "nativesharp/Driver/DiagnosticBundle.h"
#include "Internal/InputIdentity.h"

#include "nativesharp/Basic/ArtifactTransaction.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"

#include <algorithm>
#include <fstream>
#include <optional>
#include <span>
#include <sstream>
#include <system_error>
#include <vector>

namespace nativesharp::driver
{
namespace
{
std::string quoted(std::string_view value)
{
    std::string result;
    result.reserve(value.size() + 2U);
    result.push_back('"');
    for (char ch : value)
    {
        if (ch == '\\' || ch == '"') result.push_back('\\');
        if (ch == '\r') result += "\\r";
        else if (ch == '\n') result += "\\n";
        else result.push_back(ch);
    }
    result.push_back('"');
    return result;
}

std::string redactedArgument(std::string_view argument)
{
    constexpr std::string_view sensitivePrefixes[] = {
        "--api-key=", "--password=", "--secret=", "--token="};
    for (const std::string_view prefix : sensitivePrefixes)
        if (argument.starts_with(prefix)) return std::string(prefix) + "<redacted>";
    return std::string(argument);
}

std::optional<compilation::ProjectGraph> collectDiagnosticInputs(
    const std::filesystem::path& projectFile,
    const compilation::ProjectLoadOptions& projectLoadOptions,
    std::vector<InputIdentity>& inputs,
    std::string& graphError)
{
    compilation::ProjectGraphLoader loader(1, {}, projectLoadOptions);
    std::optional<compilation::ProjectGraph> graph = loader.load(projectFile, graphError);
    if (!graph.has_value())
    {
        std::error_code existsError;
        if (std::filesystem::is_regular_file(projectFile, existsError) && !existsError)
            inputs.push_back({"project:root", projectFile});
        return graph;
    }
    for (const compilation::Project& project : graph->projects())
    {
        const std::string module = project.moduleName();
        inputs.push_back({"project:" + module, project.projectFile()});
        for (const compilation::ProjectSource& source : project.sourceSet().sources())
            inputs.push_back({
                "source:" + module + ":" + source.projectRelativePath.generic_string(),
                source.absolutePath});
        std::size_t ordinal = 0;
        for (const compilation::LibraryReference& reference : project.libraryReferences())
        {
            const std::string prefix =
                "library:" + module + ":" + std::to_string(ordinal++);
            inputs.push_back({prefix + ":metadata", reference.metadataPath});
            inputs.push_back({prefix + ":archive", reference.libraryPath});
            if (!reference.runtimePath.empty())
                inputs.push_back({prefix + ":runtime", reference.runtimePath});
        }
    }
    return graph;
}

std::string renderDiagnosticAction(std::span<const std::string_view> arguments)
{
    std::ostringstream action;
    for (const std::string_view argument : arguments)
    {
        if (action.tellp() != std::streampos(0)) action << ' ';
        action << redactedArgument(argument);
    }
    return action.str();
}

void appendDiagnosticInputs(
    std::ostringstream& document,
    const std::vector<InputIdentity>& inputs)
{
    for (const InputIdentity& input : inputs)
    {
        basic::Sha256Digest digest{};
        std::uintmax_t size = 0;
        std::string hashError;
        if (!basic::sha256File(input.path, digest, size, hashError))
        {
            document << "input_error=" << quoted(input.role + ":" + hashError) << '\n';
            continue;
        }
        document << "input=" << quoted(input.role) << ',' << size << ','
                 << basic::sha256Hex(digest) << '\n';
    }
}
}

bool writeDiagnosticBundle(
    const std::filesystem::path& projectFile,
    const std::filesystem::path& destination,
    ExitCode result,
    std::string_view standardOutput,
    std::string_view standardError,
    const compilation::ProjectLoadOptions& projectLoadOptions,
    std::span<const std::string_view> arguments,
    std::string& error)
{
    std::string graphError;
    std::vector<InputIdentity> inputs;
    const std::optional<compilation::ProjectGraph> graph = collectDiagnosticInputs(
        projectFile,
        projectLoadOptions,
        inputs,
        graphError);
    std::ranges::sort(inputs, {}, &InputIdentity::role);

    std::ostringstream document;
    const std::string action = renderDiagnosticAction(arguments);
    document << "NSDIAG 1\n"
             << "compiler_version=" << quoted(kCompilerVersion) << '\n'
             << "compiler_build=" << quoted(kCompilerBuildId) << '\n'
             << "language_version=" << quoted(kLanguageVersion) << '\n'
             << "compiler_stage=" << quoted(kCompilerStage) << '\n'
             << "target=" << quoted("x86_64-pc-windows-msvc") << '\n'
             << "action=" << quoted(action) << '\n'
             << "result=" << static_cast<int>(result) << '\n';
    for (const std::string_view argument : arguments)
        document << "argument=" << quoted(redactedArgument(argument)) << '\n';
    if (!graph.has_value()) document << "project_graph_error=" << quoted(graphError) << '\n';
    appendDiagnosticInputs(document, inputs);
    document << "stdout=" << quoted(standardOutput) << '\n'
             << "stderr=" << quoted(standardError) << '\n';

    return basic::writeTextTransactionally(destination, document.str(), error);
}
}
