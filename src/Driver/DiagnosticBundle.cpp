#include "nativesharp/Driver/DiagnosticBundle.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"

#include <algorithm>
#include <fstream>
#include <span>
#include <sstream>
#include <vector>

namespace nativesharp::driver
{
namespace
{
struct InputIdentity final
{
    std::string role;
    std::filesystem::path path;
};

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
    compilation::ProjectGraphLoader loader(1, {}, projectLoadOptions);
    std::string graphError;
    std::optional<compilation::ProjectGraph> graph = loader.load(projectFile, graphError);

    std::vector<InputIdentity> inputs;
    if (graph.has_value())
    {
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
    }
    else
    {
        std::error_code existsError;
        if (std::filesystem::is_regular_file(projectFile, existsError) && !existsError)
            inputs.push_back({"project:root", projectFile});
    }
    std::ranges::sort(inputs, {}, &InputIdentity::role);

    std::ostringstream document;
    std::ostringstream action;
    for (const std::string_view argument : arguments)
    {
        if (action.tellp() != std::streampos(0)) action << ' ';
        action << redactedArgument(argument);
    }
    document << "NSDIAG 1\n"
             << "compiler_version=" << quoted(kCompilerVersion) << '\n'
             << "compiler_build=" << quoted(kCompilerBuildId) << '\n'
             << "language_version=" << quoted(kLanguageVersion) << '\n'
             << "compiler_stage=" << quoted(kCompilerStage) << '\n'
             << "target=" << quoted("x86_64-pc-windows-msvc") << '\n'
             << "action=" << quoted(action.str()) << '\n'
             << "result=" << static_cast<int>(result) << '\n';
    for (const std::string_view argument : arguments)
        document << "argument=" << quoted(redactedArgument(argument)) << '\n';
    if (!graph.has_value()) document << "project_graph_error=" << quoted(graphError) << '\n';
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
    document << "stdout=" << quoted(standardOutput) << '\n'
             << "stderr=" << quoted(standardError) << '\n';

    std::error_code filesystemError;
    if (!destination.parent_path().empty())
        std::filesystem::create_directories(destination.parent_path(), filesystemError);
    if (filesystemError)
    {
        error = "cannot create diagnostic bundle directory '" + destination.parent_path().string() + "'";
        return false;
    }
    const std::filesystem::path temporary = destination.string() + ".tmp";
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        error = "cannot open diagnostic bundle temporary destination '" + temporary.string() + "'";
        return false;
    }
    stream << document.str();
    if (!stream)
    {
        error = "cannot write diagnostic bundle destination '" + destination.string() + "'";
        stream.close();
        std::error_code removeError;
        std::filesystem::remove(temporary, removeError);
        return false;
    }
    stream.close();
    if (!stream)
    {
        std::error_code removeError;
        std::filesystem::remove(temporary, removeError);
        error = "cannot close diagnostic bundle destination '" + destination.string() + "'";
        return false;
    }
    std::filesystem::remove(destination, filesystemError);
    filesystemError.clear();
    std::filesystem::rename(temporary, destination, filesystemError);
    if (filesystemError)
    {
        std::error_code removeError;
        std::filesystem::remove(temporary, removeError);
        error = "cannot publish diagnostic bundle destination '" + destination.string() + "'";
        return false;
    }
    return true;
}
}
