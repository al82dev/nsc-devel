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

// EN: LKF-139: a sensitive option is recognized in any letter case and in both spellings, `--token=VALUE`
//     and `--token VALUE`; every redacted value is also removed from the captured stdout and stderr, which
//     a command may echo. The bundle is still not anonymized: paths and sources are recorded on purpose.
// ES: LKF-139: una opción sensible se reconoce en cualquier caja y en ambas formas, `--token=VALOR` y
//     `--token VALOR`; cada valor redactado se quita también de stdout y stderr capturados, que un comando
//     puede repetir. El bundle sigue sin ser anónimo: rutas y fuentes se registran a propósito.
constexpr std::string_view kSensitiveOptions[] = {"--api-key", "--password", "--secret", "--token"};

bool equalsIgnoringAsciiCase(std::string_view left, std::string_view right) noexcept
{
    return left.size() == right.size() &&
        std::equal(left.begin(), left.end(), right.begin(), [](char a, char b)
            {
                const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c; };
                return lower(a) == lower(b);
            });
}

std::vector<std::string> redactedArguments(
    std::span<const std::string_view> arguments,
    std::vector<std::string>& secrets)
{
    std::vector<std::string> result;
    result.reserve(arguments.size());
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        const std::string_view argument = arguments[index];
        bool redacted = false;
        for (const std::string_view option : kSensitiveOptions)
        {
            const std::size_t equals = argument.find('=');
            if (equals != std::string_view::npos && equalsIgnoringAsciiCase(argument.substr(0, equals), option))
            {
                if (equals + 1 < argument.size()) secrets.emplace_back(argument.substr(equals + 1));
                result.push_back(std::string(argument.substr(0, equals + 1)) + "<redacted>");
                redacted = true;
                break;
            }
            if (equalsIgnoringAsciiCase(argument, option) && index + 1 < arguments.size())
            {
                result.emplace_back(argument);
                if (!arguments[index + 1].empty()) secrets.emplace_back(arguments[index + 1]);
                result.emplace_back("<redacted>");
                ++index;
                redacted = true;
                break;
            }
        }
        if (!redacted) result.emplace_back(argument);
    }
    return result;
}

std::string withoutSecrets(std::string_view text, const std::vector<std::string>& secrets)
{
    std::string result(text);
    for (const std::string& secret : secrets)
    {
        if (secret.empty()) continue;
        for (std::size_t at = result.find(secret); at != std::string::npos; at = result.find(secret, at))
        {
            result.replace(at, secret.size(), "<redacted>");
            at += std::string_view("<redacted>").size();
        }
    }
    return result;
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

std::string renderDiagnosticAction(const std::vector<std::string>& arguments)
{
    std::ostringstream action;
    for (const std::string& argument : arguments)
    {
        if (action.tellp() != std::streampos(0)) action << ' ';
        action << argument;
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
    std::vector<std::string> secrets;
    const std::vector<std::string> redacted = redactedArguments(arguments, secrets);
    const std::string action = renderDiagnosticAction(redacted);
    document << "NSDIAG 1\n"
             << "compiler_version=" << quoted(kCompilerVersion) << '\n'
             << "compiler_build=" << quoted(kCompilerBuildId) << '\n'
             << "compiler_stage=" << quoted(kCompilerStage) << '\n'
             << "target=" << quoted("x86_64-pc-windows-msvc") << '\n'
             << "action=" << quoted(action) << '\n'
             << "result=" << static_cast<int>(result) << '\n';
    for (const std::string& argument : redacted)
        document << "argument=" << quoted(argument) << '\n';
    if (!graph.has_value()) document << "project_graph_error=" << quoted(graphError) << '\n';
    appendDiagnosticInputs(document, inputs);
    document << "stdout=" << quoted(withoutSecrets(standardOutput, secrets)) << '\n'
             << "stderr=" << quoted(withoutSecrets(standardError, secrets)) << '\n';

    return basic::writeTextTransactionally(destination, document.str(), error);
}
}
