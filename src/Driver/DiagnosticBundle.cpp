#include "nativesharp/Driver/DiagnosticBundle.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"

#include <algorithm>
#include <fstream>
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
}

bool writeDiagnosticBundle(
    const std::filesystem::path& projectFile,
    const std::filesystem::path& destination,
    ExitCode result,
    std::string_view standardOutput,
    std::string_view standardError,
    std::string& error)
{
    compilation::ProjectGraphLoader loader;
    std::optional<compilation::ProjectGraph> graph = loader.load(projectFile, error);
    if (!graph.has_value()) return false;

    std::vector<InputIdentity> inputs;
    for (const compilation::Project& project : graph->projects())
    {
        const std::string module = project.moduleName();
        inputs.push_back({"project:" + module, project.projectFile()});
        for (const compilation::ProjectSource& source : project.sourceSet().sources())
            inputs.push_back({"source:" + module + ":" + source.projectRelativePath.generic_string(),
                source.absolutePath});
        std::size_t ordinal = 0;
        for (const compilation::LibraryReference& reference : project.libraryReferences())
        {
            const std::string prefix = "library:" + module + ":" + std::to_string(ordinal++);
            inputs.push_back({prefix + ":metadata", reference.metadataPath});
            inputs.push_back({prefix + ":archive", reference.libraryPath});
            if (!reference.runtimePath.empty()) inputs.push_back({prefix + ":runtime", reference.runtimePath});
        }
    }
    std::ranges::sort(inputs, {}, &InputIdentity::role);

    std::ostringstream document;
    document << "NSDIAG 1\n"
             << "compiler_version=" << quoted(kCompilerVersion) << '\n'
             << "compiler_build=" << quoted(kCompilerBuildId) << '\n'
             << "language_version=" << quoted(kLanguageVersion) << '\n'
             << "compiler_stage=" << quoted(kCompilerStage) << '\n'
             << "target=" << quoted("x86_64-pc-windows-msvc") << '\n'
             << "action=" << quoted("--verify-project-oop-llvm -O0") << '\n'
             << "result=" << static_cast<int>(result) << '\n';
    for (const InputIdentity& input : inputs)
    {
        basic::Sha256Digest digest{};
        std::uintmax_t size = 0;
        std::string hashError;
        if (!basic::sha256File(input.path, digest, size, hashError))
        {
            error = "cannot fingerprint diagnostic input '" + input.role + "': " + hashError;
            return false;
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
    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        error = "cannot open diagnostic bundle destination '" + destination.string() + "'";
        return false;
    }
    stream << document.str();
    if (!stream)
    {
        error = "cannot write diagnostic bundle destination '" + destination.string() + "'";
        return false;
    }
    return true;
}
}
