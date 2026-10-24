#include "nativesharp/Driver/ProjectCommands.h"
#include "Internal/Artifact.h"
#include "Internal/BuildOptions.h"
#include "Internal/BuildPreferences.h"
#include "Internal/BuildResult.h"
#include "Internal/CommandOutput.h"
#include "Internal/CommonArguments.h"
#include "Internal/CompilationInputManifestFactory.h"
#include "Internal/DependencyProvenance.h"
#include "Internal/EnvironmentOverride.h"
#include "Internal/ExplainTopic.h"
#include "Internal/Json.h"
#include "Internal/JsonParser.h"
#include "Internal/PackageRequest.h"
#include "Internal/ProgressJsonForwardingBuffer.h"
#include "Internal/ProjectCommandSupport.h"
#include "Internal/RestoredPackage.h"
#include "Internal/RestoreOptions.h"
#include "Internal/RestorePlan.h"
#include "Internal/Sidecar.h"
#include "Internal/TemporaryFile.h"

#include "nativesharp/Driver/CompilerConsole.h"

#include "nativesharp/Basic/ArtifactTransaction.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Compilation/PrecompiledProductSidecarPath.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Compilation/ProjectLoader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <streambuf>
#include <string>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <process.h>
#endif

namespace nativesharp::driver
{
namespace
{
namespace fs = std::filesystem;

constexpr std::uintmax_t kMaximumJsonBytes = 4U * 1024U * 1024U;
const Json* member(const Json& object, std::string_view name)
{
    const Json::Object* values = object.object();
    if (values == nullptr) return nullptr;
    const auto iterator = values->find(std::string(name));
    return iterator == values->end() ? nullptr : &iterator->second;
}

std::string stringMember(const Json& object, std::string_view name, std::string fallback = {})
{
    const Json* value = member(object, name);
    return value != nullptr && value->string() != nullptr ? *value->string() : std::move(fallback);
}

bool boolMember(const Json& object, std::string_view name, bool fallback)
{
    const Json* value = member(object, name);
    return value != nullptr && value->boolean() != nullptr ? *value->boolean() : fallback;
}

std::optional<double> numberMember(const Json& object, std::string_view name)
{
    const Json* value = member(object, name);
    if (value == nullptr) return std::nullopt;
    if (const double* number = std::get_if<double>(&value->value)) return *number;
    return std::nullopt;
}

bool readText(const fs::path& path, std::string& text, std::string& error)
{
    std::error_code fileError;
    const std::uintmax_t size = fs::file_size(path, fileError);
    if (fileError) { error = "cannot read '" + path.string() + "': " + fileError.message(); return false; }
    if (size > kMaximumJsonBytes)
    {
        error = "file is larger than the 4 MiB safety limit: '" + path.string() + "'";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) { error = "cannot open '" + path.string() + "'"; return false; }
    text.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    if (!stream.good() && !stream.eof()) { error = "cannot read '" + path.string() + "'"; return false; }
    return true;
}

bool readJson(const fs::path& path, Json& value, std::string& error)
{
    std::string text;
    if (!readText(path, text, error)) return false;
    if (!JsonParser(text).parse(value, error))
    {
        error = "invalid JSON in '" + path.string() + "': " + error;
        return false;
    }
    return true;
}

bool atomicWrite(const fs::path& path, std::string_view contents, std::string& error)
{
    return basic::writeTextTransactionally(path, contents, error);
}

fs::path sidecarPath(const fs::path& project)
{
    fs::path result = project;
    result.replace_extension(".nspm.json");
    return result;
}

bool loadSidecar(const fs::path& project, Sidecar& sidecar, std::string& error, bool required)
{
    const fs::path path = sidecarPath(project);
    if (!fs::exists(path))
    {
        if (required) error = "project sidecar does not exist: '" + path.string() + "'";
        else { sidecar.project = project.filename().string(); sidecar.rootNamespace = project.stem().string(); }
        return !required;
    }
    Json root;
    if (!readJson(path, root, error)) return false;
    if (root.object() == nullptr) { error = "sidecar root must be an object"; return false; }
    const Json* schema = member(root, "schemaVersion");
    if (schema == nullptr || !std::holds_alternative<double>(schema->value) ||
        std::get<double>(schema->value) != 1.0)
    { error = "unsupported .nspm.json schemaVersion"; return false; }
    sidecar.project = stringMember(root, "project", project.filename().string());
    sidecar.rootNamespace = stringMember(root, "rootNamespace", project.stem().string());
    sidecar.createInitialSource = boolMember(root, "createInitialSource", false);
    if (const Json* build = member(root, "build"))
    {
        if (build->object() == nullptr) { error = "sidecar build must be an object"; return false; }
        sidecar.build.configuration = stringMember(*build, "configuration", "Debug");
        sidecar.build.optimization = stringMember(*build, "optimization", "O0");
        sidecar.build.debugInformation = stringMember(*build, "debug_information", "full");
        sidecar.build.linker = stringMember(*build, "linker", "NSLink");
        sidecar.build.target = stringMember(*build, "target", "x86_64-pc-windows-msvc");
        sidecar.build.restoreBeforeBuild = boolMember(*build, "restore_before_build", true);
    }
    if (const Json* packages = member(root, "packages"))
    {
        if (packages->array() == nullptr) { error = "sidecar packages must be an array"; return false; }
        std::set<std::string> seen;
        for (const Json& value : *packages->array())
        {
            if (value.object() == nullptr) { error = "each package request must be an object"; return false; }
            PackageRequest package{stringMember(value, "package_id"), stringMember(value, "version", "*"),
                                   stringMember(value, "source")};
            if (package.id.empty() || package.version.empty())
            {
                error = "package id and version cannot be empty";
                return false;
            }
            std::string key = package.id;
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (!seen.insert(key).second) { error = "duplicate package request: '" + package.id + "'"; return false; }
            sidecar.packages.push_back(std::move(package));
        }
    }
    return true;
}

std::string renderSidecar(const Sidecar& sidecar)
{
    std::ostringstream stream;
    stream << "{\n  \"build\": {\n"
           << "    \"configuration\": \"" << jsonEscape(sidecar.build.configuration) << "\",\n"
           << "    \"debug_information\": \"" << jsonEscape(sidecar.build.debugInformation) << "\",\n"
           << "    \"linker\": \"" << jsonEscape(sidecar.build.linker) << "\",\n"
           << "    \"optimization\": \"" << jsonEscape(sidecar.build.optimization) << "\",\n"
           << "    \"restore_before_build\": " << (sidecar.build.restoreBeforeBuild ? "true" : "false") << ",\n"
           << "    \"target\": \"" << jsonEscape(sidecar.build.target) << "\"\n"
           << "  },\n  \"createInitialSource\": " << (sidecar.createInitialSource ? "true" : "false") << ",\n"
           << "  \"packages\": [";
    for (std::size_t index = 0; index < sidecar.packages.size(); ++index)
    {
        const PackageRequest& package = sidecar.packages[index];
        stream << (index == 0 ? "\n" : ",\n")
               << "    {\n      \"package_id\": \"" << jsonEscape(package.id) << "\",\n"
               << "      \"source\": \"" << jsonEscape(package.source) << "\",\n"
               << "      \"version\": \"" << jsonEscape(package.version) << "\"\n    }";
    }
    if (!sidecar.packages.empty()) stream << '\n';
    stream << "  ],\n  \"project\": \"" << jsonEscape(sidecar.project) << "\",\n"
           << "  \"rootNamespace\": \"" << jsonEscape(sidecar.rootNamespace) << "\",\n"
           << "  \"schemaVersion\": 1\n}\n";
    return stream.str();
}

bool validIdentifier(std::string_view value)
{
    if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value.front())) || value.front() == '_'))
        return false;
    return std::all_of(value.begin() + 1, value.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '_';
    });
}

bool safeComponent(std::string_view value)
{
    return !value.empty() && value != "." && value != ".." &&
        std::none_of(value.begin(), value.end(), [](unsigned char character) {
            return character < 0x20U || character == '<' || character == '>' || character == ':' ||
                   character == '"' || character == '/' || character == '\\' || character == '|' ||
                   character == '?' || character == '*';
        });
}

bool inferProject(std::optional<fs::path> requested, fs::path& project, std::string& error)
{
    std::error_code fileError;
    if (requested.has_value())
    {
        project = fs::absolute(*requested, fileError).lexically_normal();
        if (fileError || !fs::is_regular_file(project))
        { error = "project does not exist: '" + requested->string() + "'"; return false; }
        if (project.extension() != ".nsproj") { error = "project must have the .nsproj extension"; return false; }
        return true;
    }
    std::vector<fs::path> found;
    for (fs::directory_iterator iterator(fs::current_path(), fileError), end; iterator != end && !fileError;
         iterator.increment(fileError))
        if (iterator->is_regular_file(fileError) && iterator->path().extension() == ".nsproj")
            found.push_back(iterator->path());
    if (fileError) { error = "cannot enumerate the current directory: " + fileError.message(); return false; }
    if (found.empty()) { error = "no .nsproj was found in the current directory"; return false; }
    if (found.size() != 1U) { error = "more than one .nsproj exists; specify the project explicitly"; return false; }
    project = fs::absolute(found.front(), fileError).lexically_normal();
    return !fileError;
}

CommonArguments stripOutputFormat(std::span<const std::string_view> arguments)
{
    CommonArguments result;
    for (std::string_view argument : arguments)
    {
        if (argument == "--output-format=json") result.json = true;
        else result.values.push_back(argument);
    }
    return result;
}

ExitCode runNew(std::span<const std::string_view> raw, std::ostream& output, std::ostream& error)
{
    CommonArguments parsed = stripOutputFormat(raw);
    CommandOutput result{parsed.json, "new", {}};
    if (parsed.values.size() < 2U)
        return result.failure(error, "usage: nsc new <console|classlib|dll> -n <name> [-o <directory>] [--force]");
    const std::string kind(parsed.values[1]);
    std::string name;
    std::optional<fs::path> directory;
    bool force = false;
    for (std::size_t index = 2; index < parsed.values.size(); ++index)
    {
        const std::string_view argument = parsed.values[index];
        if ((argument == "-n" || argument == "--name") && index + 1 < parsed.values.size())
            name = parsed.values[++index];
        else if ((argument == "-o" || argument == "--output") && index + 1 < parsed.values.size())
            directory = fs::path(parsed.values[++index]);
        else if (argument == "--force") force = true;
        else return result.failure(error, "unknown new option: '" + std::string(argument) + "'");
    }
    if (!validIdentifier(name)) return result.failure(error, "project name must be a valid NativeSharp identifier");
    std::string outputType;
    if (kind == "console") outputType = "Exe";
    else if (kind == "classlib") outputType = "StaticLibrary";
    else if (kind == "dll") outputType = "SharedLibrary";
    else return result.failure(error, "template must be console, classlib, or dll");
    std::error_code fileError;
    fs::path root = directory.value_or(fs::current_path() / name);
    root = fs::absolute(root, fileError).lexically_normal();
    if (fileError) return result.failure(error, "cannot resolve output directory");
    const fs::path project = root / (name + ".nsproj");
    const fs::path sidecar = sidecarPath(project);
    const fs::path source = root / "Main.ns";
    if (!force && (fs::exists(project) || fs::exists(sidecar)))
        return result.failure(error, "project already exists; pass --force to replace its configuration");
    fs::create_directories(root, fileError);
    if (fileError) return result.failure(error, "cannot create project directory: " + fileError.message());
    const std::string xml = "<Project>\n  <PropertyGroup>\n    <Name>" + name + "</Name>\n    <OutputType>" +
                            outputType +
                            "</OutputType>\n  </PropertyGroup>\n  <ItemGroup>\n    <Compile Include=\"**/*.ns\" />\n  "
                            "</ItemGroup>\n</Project>\n";
    Sidecar model;
    model.project = project.filename().string();
    model.rootNamespace = name;
    model.createInitialSource = true;
    std::string writeError;
    if (!atomicWrite(project, xml, writeError) || !atomicWrite(sidecar, renderSidecar(model), writeError))
        return result.failure(error, writeError, ExitCode::InternalError);
    if (!fs::exists(source))
    {
        const bool executable = outputType == "Exe";
        const std::string body = "namespace " + name + "\n{\n    public static class " +
            (executable ? std::string("Program") : std::string("Library")) +
            "\n    {\n        public static int " + (executable ? std::string("Main") : std::string("Version")) +
            "()\n        {\n            return " + (executable ? std::string("0") : std::string("1")) +
            ";\n        }\n    }\n}\n";
        if (!atomicWrite(source, body, writeError)) return result.failure(error, writeError, ExitCode::InternalError);
    }
    result.project = project;
    return result.success(output, "Created NativeSharp project: " + project.string(),
                          "\"template\":\"" + jsonEscape(kind) + "\"");
}

bool isSafeRelative(const fs::path& path)
{
    if (path.empty() || path.is_absolute() || path.has_root_name()) return false;
    for (const fs::path& component : path)
        if (component == ".." || component == ".") return false;
    return true;
}

bool appendArtifact(RestoredPackage& package, std::string type, const fs::path& relative,
                    const fs::path& destinationBase, std::string& error)
{
    if (!isSafeRelative(relative))
    {
        error = "package artifact path is not safe: '" + relative.string() + "'";
        return false;
    }
    const fs::path source = (package.sourceRoot / relative).lexically_normal();
    std::error_code fileError;
    if (package.canonicalSourceRoot.empty())
    {
        package.canonicalSourceRoot = fs::weakly_canonical(package.sourceRoot, fileError);
        if (fileError)
        {
            package.canonicalSourceRoot.clear();
            error = "cannot resolve package root: " + fileError.message();
            return false;
        }
    }
    const fs::path& canonicalRoot = package.canonicalSourceRoot;
    const fs::path canonicalSource = fs::weakly_canonical(source, fileError);
    if (fileError || !fs::is_regular_file(canonicalSource))
    {
        error = "package artifact does not exist: '" + source.string() + "'";
        return false;
    }
    auto rootIterator = canonicalRoot.begin();
    auto sourceIterator = canonicalSource.begin();
    for (; rootIterator != canonicalRoot.end(); ++rootIterator, ++sourceIterator)
        if (sourceIterator == canonicalSource.end() || *sourceIterator != *rootIterator)
        { error = "package artifact escapes its source root"; return false; }
    basic::Sha256Digest digest{};
    std::uintmax_t size = 0;
    if (!basic::sha256File(canonicalSource, digest, size, error)) return false;
    Artifact artifact;
    artifact.type = std::move(type);
    artifact.source = canonicalSource;
    artifact.relativeDestination = destinationBase / relative.filename();
    artifact.hash = basic::sha256Hex(digest);
    artifact.size = size;
    package.artifacts.push_back(std::move(artifact));
    return true;
}

bool appendOptionalProductSidecar(
    RestoredPackage& package,
    const fs::path& metadata,
    const fs::path& destinationBase,
    std::string& error)
{
    const fs::path sidecar =
        compilation::precompiledProductSidecarPath(metadata);
    const fs::path source = package.sourceRoot / sidecar;
    std::error_code fileError;
    if (!fs::is_regular_file(source, fileError))
    {
        if (!fileError || fileError == std::errc::no_such_file_or_directory)
            return true;
        error = "cannot inspect package product sidecar '" +
            source.string() + "': " + fileError.message();
        return false;
    }
    const fs::path destination = destinationBase / sidecar.filename();
    if (std::ranges::any_of(
            package.artifacts,
            [&](const Artifact& artifact)
            {
                return artifact.relativeDestination == destination;
            }))
    {
        return true;
    }
    return appendArtifact(
        package, "precompiled-products", sidecar, destinationBase, error);
}

std::optional<fs::path> packageManifest(const fs::path& root)
{
    for (const char* name : {"NativeSharp.package.json", "package.json", ".nspackage.json"})
    {
        const fs::path candidate = root / name;
        if (fs::is_regular_file(candidate)) return candidate;
    }
    return std::nullopt;
}

bool looksLikeConventionPackage(const fs::path& root)
{
    std::error_code error;
    bool library = false;
    bool metadata = false;
    for (fs::directory_iterator iterator(root, error), end; iterator != end && !error; iterator.increment(error))
    {
        if (!iterator->is_regular_file(error)) continue;
        if (iterator->path().extension() == ".lib") library = true;
        if (iterator->path().extension() == ".nsmeta") metadata = true;
    }
    return !error && library && metadata;
}

bool resolvePackage(const PackageRequest& request, const std::optional<fs::path>& sourceOverride,
                    RestoredPackage& package, std::string& error)
{
    if (!safeComponent(request.id) || !safeComponent(request.version))
    { error = "unsafe package id or version: '" + request.id + "'"; return false; }
    fs::path source;
    if (!request.source.empty()) source = request.source;
    else if (sourceOverride.has_value()) source = *sourceOverride;
    else if (const std::optional<std::string> environment = environmentValue("NSC_PACKAGE_ROOT")) source = *environment;
    else if (const std::optional<std::string> local = environmentValue("LOCALAPPDATA"))
        source = fs::path(*local) / "NativeSharp" / "packages";
    else { error = "package source is not configured; use --source or NSC_PACKAGE_ROOT"; return false; }
    std::error_code fileError;
    source = fs::absolute(source, fileError).lexically_normal();
    if (fileError) { error = "cannot resolve package source"; return false; }
    if (!packageManifest(source).has_value() && !looksLikeConventionPackage(source))
        source /= fs::path(request.id) / request.version;
    if (!fs::is_directory(source))
    {
        error = "package source directory does not exist: '" + source.string() + "'";
        return false;
    }
    package.request = request;
    package.sourceRoot = source;
    const fs::path destinationBase = fs::path(request.id) / request.version;
    const std::optional<fs::path> manifest = packageManifest(source);
    if (manifest.has_value())
    {
        Json root;
        if (!readJson(*manifest, root, error)) return false;
        if (stringMember(root, "packageId") != request.id || stringMember(root, "version") != request.version)
        { error = "package manifest identity does not match request '" + request.id + "'"; return false; }
        const Json* libraries = member(root, "libraries");
        if (libraries == nullptr || libraries->array() == nullptr || libraries->array()->empty())
        { error = "package manifest must contain at least one library"; return false; }
        std::vector<fs::path> metadataArtifacts;
        metadataArtifacts.reserve(libraries->array()->size());
        for (const Json& library : *libraries->array())
        {
            const fs::path metadataArtifact =
                stringMember(library, "metadata");
            if (!appendArtifact(package, "library", stringMember(library, "library"), destinationBase, error) ||
                !appendArtifact(package, "metadata", metadataArtifact, destinationBase, error))
                return false;
            metadataArtifacts.push_back(metadataArtifact);
            const std::string runtime = stringMember(library, "runtime");
            if (!runtime.empty() && !appendArtifact(package, "runtime", runtime, destinationBase, error)) return false;
        }
        if (const Json* files = member(root, "files"))
        {
            if (files->array() == nullptr) { error = "package files must be an array"; return false; }
            for (const Json& value : *files->array())
            {
                if (value.string() == nullptr ||
                    !appendArtifact(package, "file", *value.string(), destinationBase, error))
                    return false;
            }
        }
        for (const fs::path& metadataArtifact : metadataArtifacts)
        {
            if (!appendOptionalProductSidecar(
                    package, metadataArtifact, destinationBase, error))
                return false;
        }
    }
    else
    {
        std::vector<fs::path> libraries;
        std::vector<fs::path> metadata;
        std::vector<fs::path> runtimes;
        for (fs::directory_iterator iterator(source, fileError), end; iterator != end && !fileError;
             iterator.increment(fileError))
        {
            if (!iterator->is_regular_file()) continue;
            const std::string extension = iterator->path().extension().string();
            if (extension == ".lib") libraries.push_back(iterator->path().filename());
            else if (extension == ".nsmeta") metadata.push_back(iterator->path().filename());
            else if (extension == ".dll") runtimes.push_back(iterator->path().filename());
        }
        if (fileError) { error = "cannot enumerate package source: " + fileError.message(); return false; }
        if (libraries.size() != 1U || metadata.size() != 1U || runtimes.size() > 1U)
        { error = "convention package requires exactly one .lib, one .nsmeta, and at most one .dll"; return false; }
        if (!appendArtifact(package, "library", libraries.front(), destinationBase, error) ||
            !appendArtifact(package, "metadata", metadata.front(), destinationBase, error)) return false;
        if (!appendOptionalProductSidecar(
                package, metadata.front(), destinationBase, error)) return false;
        if (!runtimes.empty() && !appendArtifact(package, "runtime", runtimes.front(), destinationBase, error))
            return false;
    }
    return true;
}

std::string renderLock(const RestorePlan& plan, const fs::path& project)
{
    std::ostringstream stream;
    stream << "{\n  \"packages\": [";
    for (std::size_t packageIndex = 0; packageIndex < plan.packages.size(); ++packageIndex)
    {
        const RestoredPackage& package = plan.packages[packageIndex];
        stream << (packageIndex == 0 ? "\n" : ",\n")
               << "    {\n      \"artifacts\": [";
        for (std::size_t artifactIndex = 0; artifactIndex < package.artifacts.size(); ++artifactIndex)
        {
            const Artifact& artifact = package.artifacts[artifactIndex];
            stream << (artifactIndex == 0 ? "\n" : ",\n")
                   << "        {\"path\": \"" << jsonEscape(pathText(artifact.relativeDestination))
                   << "\", \"sha256\": \"" << artifact.hash << "\", \"size\": " << artifact.size
                   << ", \"type\": \"" << artifact.type << "\"}";
        }
        if (!package.artifacts.empty()) stream << '\n';
        stream << "      ],\n      \"id\": \"" << jsonEscape(package.request.id)
               << "\",\n      \"version\": \"" << jsonEscape(package.request.version) << "\"\n    }";
    }
    if (!plan.packages.empty()) stream << '\n';
    stream << "  ],\n  \"project\": \"" << jsonEscape(project.filename().string())
           << "\",\n  \"schemaVersion\": 1,\n  \"target\": \"x86_64-pc-windows-msvc\"\n}\n";
    return stream.str();
}

bool makeRestorePlan(const fs::path& project, const Sidecar& sidecar,
                     const std::optional<fs::path>& sourceOverride, RestorePlan& plan, std::string& error)
{
    plan.root = project.parent_path() / "obj" / "nativesharp" / "packages";
    for (const PackageRequest& request : sidecar.packages)
    {
        RestoredPackage package;
        if (!resolvePackage(request, sourceOverride, package, error)) return false;
        plan.packages.push_back(std::move(package));
    }
    std::sort(plan.packages.begin(), plan.packages.end(), [](const auto& left, const auto& right) {
        return left.request.id < right.request.id ||
               (left.request.id == right.request.id && left.request.version < right.request.version);
    });
    plan.lock = renderLock(plan, project);
    return true;
}

bool verifyPlan(const RestorePlan& plan, std::size_t& verified, std::string& error)
{
    for (const RestoredPackage& package : plan.packages)
        for (const Artifact& artifact : package.artifacts)
        {
            const fs::path destination = plan.root / artifact.relativeDestination;
            basic::Sha256Digest digest{};
            std::uintmax_t size = 0;
            if (!basic::sha256File(destination, digest, size, error))
            { error = "restore verification failed for '" + destination.string() + "': " + error; return false; }
            if (size != artifact.size || basic::sha256Hex(digest) != artifact.hash)
            { error = "restore verification found stale artifact: '" + destination.string() + "'"; return false; }
            ++verified;
        }
    return true;
}

bool materialize(const RestorePlan& plan, std::size_t& copied, std::string& error)
{
    const fs::path parent = plan.root.parent_path();
#if defined(_WIN32)
    const std::string token = std::to_string(_getpid());
#else
    const std::string token = "transaction";
#endif
    const fs::path staging = parent / ("packages.stage." + token);
    const fs::path backup = parent / ("packages.backup." + token);
    std::error_code fileError;
    fs::remove_all(staging, fileError);
    fileError.clear();
    fs::remove_all(backup, fileError);
    fileError.clear();
    fs::create_directories(staging, fileError);
    if (fileError) { error = "cannot create restore staging directory: " + fileError.message(); return false; }
    for (const RestoredPackage& package : plan.packages)
        for (const Artifact& artifact : package.artifacts)
        {
            const fs::path destination = staging / artifact.relativeDestination;
            fs::create_directories(destination.parent_path(), fileError);
            if (!fileError)
                fs::copy_file(artifact.source, destination, fs::copy_options::overwrite_existing, fileError);
            if (fileError)
            {
                fs::remove_all(staging, fileError);
                error = "cannot stage package artifact '" + artifact.source.string() + "': " + fileError.message();
                return false;
            }
            ++copied;
        }
    if (fs::exists(plan.root)) fs::rename(plan.root, backup, fileError);
    if (fileError)
    {
        fs::remove_all(staging, fileError);
        error = "cannot begin restore transaction: " + fileError.message();
        return false;
    }
    fs::rename(staging, plan.root, fileError);
    if (fileError)
    {
        std::error_code rollbackError;
        if (fs::exists(backup)) fs::rename(backup, plan.root, rollbackError);
        fs::remove_all(staging, rollbackError);
        error = "cannot publish restored packages: " + fileError.message();
        return false;
    }
    fs::remove_all(backup, fileError);
    return true;
}

bool parseRestoreOptions(std::span<const std::string_view> arguments, RestoreOptions& options, std::string& error)
{
    for (std::size_t index = 1; index < arguments.size(); ++index)
    {
        const std::string_view argument = arguments[index];
        if (argument == "--force") options.force = true;
        else if (argument == "--locked-mode") options.locked = true;
        else if (argument == "--verify") options.verify = true;
        else if (argument == "--source" && index + 1 < arguments.size()) options.source = fs::path(arguments[++index]);
        else if (argument.starts_with('-'))
        {
            error = "unknown restore option: '" + std::string(argument) + "'";
            return false;
        }
        else if (options.project.has_value()) { error = "restore accepts only one project"; return false; }
        else options.project = fs::path(argument);
    }
    if (options.force && (options.locked || options.verify))
    {
        error = "--force cannot be combined with --locked-mode or --verify";
        return false;
    }
    return true;
}

ExitCode executeRestore(const RestoreOptions& options, bool json, std::ostream& output, std::ostream& error,
                        RestorePlan* completedPlan = nullptr)
{
    CommandOutput result{json, "restore", {}};
    fs::path project;
    std::string message;
    if (!inferProject(options.project, project, message)) return result.failure(error, message);
    result.project = project;
    Sidecar sidecar;
    if (!loadSidecar(project, sidecar, message, false)) return result.failure(error, message);
    RestorePlan plan;
    if (!makeRestorePlan(project, sidecar, options.source, plan, message)) return result.failure(error, message);
    const fs::path lockPath = project.parent_path() / "NativeSharp.lock.json";
    if (options.locked)
    {
        std::string existing;
        if (!readText(lockPath, existing, message))
            return result.failure(error, "locked restore requires a readable lock file: " + message);
        if (existing != plan.lock) return result.failure(error, "locked restore rejected package source changes");
    }
    std::size_t verified = 0;
    const bool current = verifyPlan(plan, verified, message);
    if (options.verify)
    {
        if (!current) return result.failure(error, message, ExitCode::CompilationError);
        if (completedPlan != nullptr) *completedPlan = std::move(plan);
        return result.success(output, "Restore verified: " + std::to_string(verified) + " artifact(s)",
                              "\"verified\":" + std::to_string(verified));
    }
    std::size_t copied = 0;
    if (!current || options.force)
    {
        if (!materialize(plan, copied, message)) return result.failure(error, message, ExitCode::InternalError);
        verified = 0;
        if (!verifyPlan(plan, verified, message)) return result.failure(error, message, ExitCode::InternalError);
    }
    // An identical lock is not rewritten: same bytes, and no new timestamp for tools watching it.
    std::string existingLock;
    std::string ignoredReadError;
    const bool rewriteLock = !options.locked &&
        !(readText(lockPath, existingLock, ignoredReadError) && existingLock == plan.lock);
    if (rewriteLock && !atomicWrite(lockPath, plan.lock, message))
        return result.failure(error, message, ExitCode::InternalError);
    if (completedPlan != nullptr) *completedPlan = std::move(plan);
    return result.success(output,
        copied == 0 ? "Restore is current: " + std::to_string(verified) + " artifact(s)" :
                      "Restored " + std::to_string(copied) + " artifact(s)",
        "\"copied\":" + std::to_string(copied) + ",\"verified\":" + std::to_string(verified) +
        ",\"lockFile\":\"" + jsonEscape(pathText(lockPath)) + "\"");
}

ExitCode runRestore(std::span<const std::string_view> raw, std::ostream& output, std::ostream& error)
{
    CommonArguments parsed = stripOutputFormat(raw);
    RestoreOptions options;
    std::string message;
    CommandOutput result{parsed.json, "restore", {}};
    if (!parseRestoreOptions(parsed.values, options, message)) return result.failure(error, message);
    return executeRestore(options, parsed.json, output, error);
}

std::string xmlEscape(std::string_view value)
{
    std::string result;
    for (char character : value)
    {
        if (character == '&') result += "&amp;";
        else if (character == '"') result += "&quot;";
        else if (character == '<') result += "&lt;";
        else if (character == '>') result += "&gt;";
        else result.push_back(character);
    }
    return result;
}

bool makeGeneratedProject(const fs::path& project, const RestorePlan& plan, fs::path& generated, std::string& error)
{
    std::string xml;
    if (!readText(project, xml, error)) return false;
    const std::size_t closing = xml.rfind("</Project>");
    if (closing == std::string::npos) { error = "project does not contain </Project>"; return false; }
    std::ostringstream references;
    bool any = false;
    references << "  <ItemGroup>\n";
    for (const RestoredPackage& package : plan.packages)
    {
        std::vector<const Artifact*> libraries;
        std::vector<const Artifact*> metadata;
        std::vector<const Artifact*> runtimes;
        for (const Artifact& artifact : package.artifacts)
        {
            if (artifact.type == "library") libraries.push_back(&artifact);
            else if (artifact.type == "metadata") metadata.push_back(&artifact);
            else if (artifact.type == "runtime") runtimes.push_back(&artifact);
        }
        if (libraries.size() != metadata.size()) { error = "package library/metadata count mismatch"; return false; }
        for (std::size_t index = 0; index < libraries.size(); ++index)
        {
            any = true;
            const fs::path library = fs::absolute(plan.root / libraries[index]->relativeDestination);
            const fs::path meta = fs::absolute(plan.root / metadata[index]->relativeDestination);
            references << "    <LibraryReference Include=\"" << xmlEscape(pathText(library))
                       << "\" Metadata=\"" << xmlEscape(pathText(meta)) << '"';
            if (index < runtimes.size())
            {
                const fs::path runtime = fs::absolute(plan.root / runtimes[index]->relativeDestination);
                references << " Runtime=\"" << xmlEscape(pathText(runtime)) << '"';
            }
            references << " />\n";
        }
    }
    references << "  </ItemGroup>\n";
    if (any) xml.insert(closing, references.str());
#if defined(_WIN32)
    const std::string token = std::to_string(_getpid());
#else
    const std::string token = "generated";
#endif
    generated = project.parent_path() / (".nspm." + project.stem().string() + "." + token + ".nsproj");
    return atomicWrite(generated, xml, error);
}

bool failContentStaging(
    const fs::path& staging,
    std::string message,
    std::string& error)
{
    std::error_code cleanupError;
    fs::remove_all(staging, cleanupError);
    error = std::move(message);
    return false;
}

bool copyContentEntries(
    const fs::path& source,
    const fs::path& staging,
    std::size_t& copiedFiles,
    std::string& error)
{
    std::error_code fileError;
    fs::recursive_directory_iterator iterator(source, fileError);
    const fs::recursive_directory_iterator end;
    if (fileError)
        return failContentStaging(
            staging, "cannot enumerate project Content directory: " + fileError.message(), error);
    while (iterator != end)
    {
        const fs::directory_entry& entry = *iterator;
        const fs::path relative = fs::relative(entry.path(), source, fileError);
        if (fileError)
            return failContentStaging(
                staging, "cannot resolve a Content path: " + fileError.message(), error);
        const fs::file_status status = entry.symlink_status(fileError);
        if (fileError)
            return failContentStaging(
                staging, "cannot inspect Content entry: " + fileError.message(), error);
        const fs::path target = staging / relative;
        if (fs::is_symlink(status))
            return failContentStaging(staging,
                "Content does not permit symbolic links: '" + pathText(entry.path()) + "'", error);
        if (fs::is_directory(status))
        {
            fs::create_directories(target, fileError);
            if (fileError)
                return failContentStaging(
                    staging, "cannot create Content directory: " + fileError.message(), error);
        }
        else if (fs::is_regular_file(status))
        {
            fs::create_directories(target.parent_path(), fileError);
            if (fileError)
                return failContentStaging(staging,
                    "cannot create Content parent directory: " + fileError.message(), error);
            fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing, fileError);
            if (fileError)
                return failContentStaging(
                    staging, "cannot copy Content file: " + fileError.message(), error);
            ++copiedFiles;
        }
        else
            return failContentStaging(staging,
                "Content contains an unsupported filesystem entry: '" +
                    pathText(entry.path()) + "'", error);
        iterator.increment(fileError);
        if (fileError)
            return failContentStaging(
                staging, "cannot enumerate project Content directory: " + fileError.message(), error);
    }
    return true;
}

bool publishContentStaging(
    const fs::path& destination,
    const fs::path& staging,
    const fs::path& backup,
    std::string& error)
{
    std::error_code fileError;
    const bool destinationExists = fs::exists(destination, fileError);
    if (fileError)
        return failContentStaging(
            staging, "cannot inspect output Content directory: " + fileError.message(), error);
    if (destinationExists)
    {
        fs::rename(destination, backup, fileError);
        if (fileError)
            return failContentStaging(staging,
                "cannot preserve the previous Content directory: " + fileError.message(), error);
    }
    fs::rename(staging, destination, fileError);
    if (fileError)
    {
        const std::string publishError = fileError.message();
        if (destinationExists)
        {
            std::error_code rollbackError;
            fs::rename(backup, destination, rollbackError);
        }
        return failContentStaging(
            staging, "cannot publish Content directory: " + publishError, error);
    }
    if (!destinationExists) return true;
    fs::remove_all(backup, fileError);
    if (!fileError) return true;
    error = "Content was published but its previous backup could not be removed: " +
        fileError.message();
    return false;
}

bool publishContentDirectory(const fs::path& projectRoot, const fs::path& outputDirectory,
                             std::size_t& copiedFiles, std::string& error)
{
    copiedFiles = 0;
    const fs::path source = projectRoot / "Content";
    std::error_code fileError;
    const bool sourceExists = fs::exists(source, fileError);
    if (fileError)
    {
        error = "cannot inspect project Content directory: " + fileError.message();
        return false;
    }
    if (!sourceExists) return true;
    if (!fs::is_directory(source, fileError) || fileError)
    {
        error = "project Content path is not a directory: '" + pathText(source) + "'";
        return false;
    }

#if defined(_WIN32)
    const std::string token = std::to_string(_getpid());
#else
    const std::string token = "content";
#endif
    const fs::path destination = outputDirectory / "Content";
    const fs::path staging = outputDirectory / (".nspm.Content.stage." + token);
    const fs::path backup = outputDirectory / (".nspm.Content.backup." + token);
    fs::remove_all(staging, fileError);
    fileError.clear();
    fs::remove_all(backup, fileError);
    fileError.clear();
    fs::create_directories(staging, fileError);
    if (fileError)
    {
        error = "cannot create Content staging directory: " + fileError.message();
        return false;
    }

    if (!copyContentEntries(source, staging, copiedFiles, error)) return false;
    return publishContentStaging(destination, staging, backup, error);
}

bool parseBuildProgressOption(
    std::string_view argument,
    BuildOptions& options,
    std::string& error)
{
    constexpr std::string_view progressPrefix = "--progress=";
    constexpr std::string_view formatPrefix = "--progress-format=";
    if (argument == "--progress")
    {
        if (options.progress)
        {
            error = "duplicate progress option";
            return false;
        }
        options.progress = true;
        return true;
    }
    const bool enablesProgress = argument.starts_with(progressPrefix);
    if (!enablesProgress && !argument.starts_with(formatPrefix)) return false;
    const std::string_view prefix = enablesProgress ? progressPrefix : formatPrefix;
    const std::string_view value = argument.substr(prefix.size());
    if (value != "files" && value != "text" && value != "json")
    {
        error = "progress format must be files, text or json";
        return false;
    }
    if ((enablesProgress && options.progress) || options.progressFormat.has_value())
    {
        error = "duplicate progress option";
        return false;
    }
    options.progress = options.progress || enablesProgress;
    options.progressFormat = value;
    return true;
}

bool parseBuildOptions(std::span<const std::string_view> arguments, bool run, BuildOptions& options, std::string& error)
{
    bool program = false;
    for (std::size_t index = 1; index < arguments.size(); ++index)
    {
        const std::string_view argument = arguments[index];
        if (run && argument == "--") { program = true; continue; }
        if (program) { options.programArguments.emplace_back(argument); continue; }
        if ((argument == "-c" || argument == "--configuration") && index + 1 < arguments.size())
            options.configuration = std::string(arguments[++index]);
        else if (argument == "--linker" && index + 1 < arguments.size())
            options.linker = std::string(arguments[++index]);
        else if (argument == "--no-restore") options.noRestore = true;
        else if (argument == "--precompiled-only") options.precompiledOnly = true;
        else if (argument == "--dependency-report") options.dependencyReport = true;
        else if (argument == "--progress" || argument.starts_with("--progress=") ||
                 argument.starts_with("--progress-format="))
        {
            if (!parseBuildProgressOption(argument, options, error)) return false;
        }
        else if (argument == "-j" || (argument.starts_with("-j") && argument.size() > 2U))
        {
            if (options.compilerJobs.has_value())
            { error = "duplicate compilation job count"; return false; }
            std::string_view value;
            if (argument == "-j")
            {
                if (index + 1U >= arguments.size())
                { error = "-j requires a job count from 1 through 1024"; return false; }
                value = arguments[++index];
            }
            else value = argument.substr(2U);
            std::size_t jobs = 0;
            for (const char character : value)
            {
                if (character < '0' || character > '9' || jobs > 1024U)
                { error = "compilation job count must be between 1 and 1024"; return false; }
                jobs = jobs * 10U + static_cast<std::size_t>(character - '0');
            }
            if (value.empty() || jobs == 0U || jobs > 1024U)
            { error = "compilation job count must be between 1 and 1024"; return false; }
            options.compilerJobs = jobs;
        }
        else if (argument == "--library-reference-root")
        {
            if (options.libraryReferenceRoot.has_value())
            { error = "duplicate library-reference root"; return false; }
            if (index + 1 >= arguments.size())
            { error = "--library-reference-root requires an absolute directory"; return false; }
            options.libraryReferenceRoot = fs::path(arguments[++index]);
        }
        else if (argument.starts_with("--library-reference-root="))
        {
            if (options.libraryReferenceRoot.has_value())
            { error = "duplicate library-reference root"; return false; }
            const std::string_view value = argument.substr(
                std::string_view("--library-reference-root=").size());
            if (value.empty())
            { error = "--library-reference-root requires an absolute directory"; return false; }
            options.libraryReferenceRoot = fs::path(value);
        }
        else if (argument == "-O0" || argument == "-O1" || argument == "-O2" || argument == "-O3")
            options.optimization = std::string(argument.substr(1));
        else if (argument == "-g" || argument == "-g0" || argument == "-gline-tables-only")
            options.debug = std::string(argument);
        else if (argument.starts_with('-'))
        {
            error = "unknown build option: '" + std::string(argument) + "'";
            return false;
        }
        else if (options.project.has_value()) { error = "build accepts only one project"; return false; }
        else options.project = fs::path(argument);
    }
    return true;
}

bool loadPlanFromLock(
    const fs::path& project,
    RestorePlan& plan,
    std::size_t& verified,
    std::string& error)
{
    Json root;
    const fs::path lockPath = project.parent_path() / "NativeSharp.lock.json";
    if (!readJson(lockPath, root, error)) return false;
    const Json* packages = member(root, "packages");
    if (packages == nullptr || packages->array() == nullptr)
    { error = "lock file does not contain a packages array"; return false; }
    plan.root = project.parent_path() / "obj" / "nativesharp" / "packages";
    for (const Json& packageValue : *packages->array())
    {
        RestoredPackage package;
        package.request.id = stringMember(packageValue, "id");
        package.request.version = stringMember(packageValue, "version");
        if (!safeComponent(package.request.id) || !safeComponent(package.request.version))
        { error = "lock file contains an unsafe package identity"; return false; }
        const Json* artifacts = member(packageValue, "artifacts");
        if (artifacts == nullptr || artifacts->array() == nullptr)
        { error = "lock package does not contain artifacts"; return false; }
        for (const Json& artifactValue : *artifacts->array())
        {
            Artifact artifact;
            artifact.type = stringMember(artifactValue, "type");
            artifact.relativeDestination = fs::path(stringMember(artifactValue, "path"));
            artifact.hash = stringMember(artifactValue, "sha256");
            const std::optional<double> size = numberMember(artifactValue, "size");
            if (!isSafeRelative(artifact.relativeDestination) || artifact.type.empty() ||
                artifact.hash.size() != 64U || !size.has_value() || *size < 0.0)
            { error = "lock file contains an invalid artifact"; return false; }
            artifact.size = static_cast<std::uintmax_t>(*size);
            package.artifacts.push_back(std::move(artifact));
        }
        plan.packages.push_back(std::move(package));
    }
    verified = 0;
    return verifyPlan(plan, verified, error);
}

bool lockMatchesPackageRequests(const Sidecar& sidecar, const RestorePlan& plan)
{
    std::vector<std::pair<std::string, std::string>> requested;
    std::vector<std::pair<std::string, std::string>> locked;
    requested.reserve(sidecar.packages.size());
    locked.reserve(plan.packages.size());
    for (const PackageRequest& package : sidecar.packages)
        requested.emplace_back(package.id, package.version);
    for (const RestoredPackage& package : plan.packages)
        locked.emplace_back(package.request.id, package.request.version);
    std::ranges::sort(requested);
    std::ranges::sort(locked);
    return requested == locked;
}

void addVerifiedRestoreDigests(
    compilation::CompilationInputManifest& manifest,
    const RestorePlan& plan)
{
    for (const RestoredPackage& package : plan.packages)
        for (const Artifact& artifact : package.artifacts)
            manifest.addVerifiedInputDigest(
                fs::absolute(plan.root / artifact.relativeDestination),
                artifact.hash,
                artifact.size);
}

} // namespace

std::string fingerprintFiles(
    const compilation::CompilationInputManifest& manifest,
    std::vector<std::filesystem::path> files)
{
    std::ranges::sort(
        files,
        {},
        [](const std::filesystem::path& path)
        {
            return pathText(path);
        });
    std::string material;
    for (const std::filesystem::path& file : files)
    {
        basic::Sha256Digest digest{};
        std::string digestText;
        std::uintmax_t size = 0;
        std::string ignored;
        material += pathText(std::filesystem::absolute(file).lexically_normal());
        material.push_back('\0');
        if (manifest.inputDigest(file, digestText, size)) material += digestText;
        else if (basic::sha256File(file, digest, size, ignored))
            material += basic::sha256Hex(digest);
        else material += "missing";
        material.push_back('\n');
    }
    return basic::sha256Hex(basic::sha256(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size())));
}

namespace
{

std::vector<DependencyProvenance> dependencyProvenance(
    const compilation::ProjectGraph& graph,
    const RestorePlan& plan,
    const compilation::CompilationInputManifest& manifest)
{
    std::vector<DependencyProvenance> records;
    const compilation::Project* root = graph.rootProject();
    for (const compilation::Project& project : graph.projects())
    {
        std::vector<fs::path> inputs = {project.projectFile()};
        for (const compilation::ProjectSource& source : project.sourceSet().sources())
            inputs.push_back(source.absolutePath);
        records.push_back(DependencyProvenance{
            root != nullptr && project.projectFile() == root->projectFile()
                ? "source" : "project-reference",
            project.moduleName(), project.projectFile(),
            fingerprintFiles(manifest, std::move(inputs))});
        for (const compilation::LibraryReference& reference : project.libraryReferences())
        {
            std::vector<fs::path> artifacts = {reference.libraryPath, reference.metadataPath};
            if (!reference.runtimePath.empty()) artifacts.push_back(reference.runtimePath);
            if (!reference.productSidecarPath.empty())
                artifacts.push_back(reference.productSidecarPath);
            records.push_back(DependencyProvenance{
                "library-reference", reference.metadataPath.stem().string(),
                reference.libraryPath,
                fingerprintFiles(manifest, std::move(artifacts))});
        }
    }
    for (const RestoredPackage& package : plan.packages)
    {
        std::string material = package.request.id + "@" + package.request.version + "\n";
        for (const Artifact& artifact : package.artifacts)
            material += artifact.type + ":" + pathText(artifact.relativeDestination) + ":" +
                artifact.hash + "\n";
        records.push_back(DependencyProvenance{
            "store-cache", package.request.id + "@" + package.request.version, plan.root,
            basic::sha256Hex(basic::sha256(std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(material.data()), material.size())))});
    }
    std::ranges::sort(records, [](const auto& left, const auto& right) {
        if (left.kind != right.kind) return left.kind < right.kind;
        if (left.identity != right.identity) return left.identity < right.identity;
        return pathText(left.path) < pathText(right.path);
    });
    return records;
}

std::string dependencyProvenanceJson(std::span<const DependencyProvenance> records)
{
    std::ostringstream json;
    json << '[';
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        if (index != 0) json << ',';
        const DependencyProvenance& record = records[index];
        json << "{\"fingerprint\":\"" << jsonEscape(record.fingerprint)
             << "\",\"identity\":\"" << jsonEscape(record.identity)
             << "\",\"kind\":\"" << jsonEscape(record.kind)
             << "\",\"path\":\"" << jsonEscape(pathText(record.path)) << "\"}";
    }
    json << ']';
    return json.str();
}

bool resolveBuildSettings(
    const BuildOptions& options,
    const compilation::ProjectBuildSettings& projectSettings,
    const Sidecar& sidecar,
    std::string& configuration,
    std::string& optimization,
    std::string& debug,
    std::string& linker,
    std::optional<fs::path>& libraryReferenceRoot,
    std::optional<std::size_t>& compilerJobs,
    std::string& message)
{
    configuration = options.configuration.value_or(sidecar.build.configuration);
    if (configuration != "Debug" && configuration != "Release")
    {
        message = "configuration must be Debug or Release";
        return false;
    }
    const std::optional<std::string> projectOptimization =
        projectSettings.optimizationLevel.has_value()
            ? std::optional<std::string>{
                "O" + std::to_string(*projectSettings.optimizationLevel)}
            : std::nullopt;
    optimization = options.optimization.value_or(
        projectOptimization.value_or(sidecar.build.optimization));
    if (optimization != "O0" && optimization != "O1" &&
        optimization != "O2" && optimization != "O3")
    {
        message = "optimization must be O0, O1, O2, or O3";
        return false;
    }
    debug = options.debug.value_or(sidecar.build.debugInformation == "none" ? "-g0" :
        sidecar.build.debugInformation == "line-tables-only" ? "-gline-tables-only" : "-g");
    linker = options.linker.value_or(sidecar.build.linker);
    if (linker != "NSLink")
    {
        message = "linker must be NSLink";
        return false;
    }
    if (options.libraryReferenceRoot.has_value() &&
        !options.libraryReferenceRoot->is_absolute())
    {
        message = "library-reference root requires an absolute directory";
        return false;
    }
    libraryReferenceRoot = options.libraryReferenceRoot.has_value()
        ? options.libraryReferenceRoot
        : projectSettings.libraryReferenceRoot;
    compilerJobs = options.compilerJobs.has_value()
        ? options.compilerJobs
        : projectSettings.compilerJobs;
    return true;
}

ExitCode restoreBuildPlan(
    const BuildOptions& options,
    bool json,
    const Sidecar& sidecar,
    const fs::path& project,
    CommandOutput& result,
    std::ostream& output,
    std::ostream& error,
    RestorePlan& plan,
    std::string& message)
{
    if (options.noRestore || options.precompiledOnly || !sidecar.build.restoreBeforeBuild)
    {
        std::size_t verified = 0;
        if (!sidecar.packages.empty() &&
            !loadPlanFromLock(project, plan, verified, message))
            return result.failure(error,
                std::string(options.precompiledOnly ? "--precompiled-only" : "--no-restore") +
                " requires an existing verified lock/store closure: " + message,
                ExitCode::CompilationError);
        plan.root = project.parent_path() / "obj" / "nativesharp" / "packages";
        return ExitCode::Success;
    }
    RestorePlan lockedPlan;
    std::size_t verified = 0;
    std::string lockError;
    if (!sidecar.packages.empty() &&
        loadPlanFromLock(project, lockedPlan, verified, lockError) &&
        lockMatchesPackageRequests(sidecar, lockedPlan))
    {
        plan = std::move(lockedPlan);
        if (!json)
            output << localizeCompilerMessage(
                "Restore is current: " + std::to_string(verified) + " artifact(s)") << '\n';
        return ExitCode::Success;
    }
    RestoreOptions restoreOptions;
    restoreOptions.project = project;
    std::ostringstream restoreOutput;
    std::ostringstream restoreError;
    const ExitCode restored =
        executeRestore(restoreOptions, false, restoreOutput, restoreError, &plan);
    if (restored != ExitCode::Success)
        return result.failure(error, restoreError.str(), restored);
    if (!json) output << restoreOutput.str();
    return ExitCode::Success;
}

ExitCode loadBuildClosure(
    const BuildOptions& options,
    const fs::path& generated,
    const compilation::ProjectLoadOptions& projectLoadOptions,
    const std::optional<std::size_t>& compilerJobs,
    CommandOutput& result,
    compilation::ProjectLoader& loader,
    std::optional<compilation::Project>& loaded,
    std::optional<compilation::ProjectGraph>& graph,
    std::string& message,
    std::ostream& error)
{
    loaded = loader.load(generated, projectLoadOptions, message);
    if (!loaded.has_value())
        return result.failure(error,
            "generated build project is invalid: " + message,
            ExitCode::CompilationError);
    compilation::ProjectGraphLoader graphLoader(
        compilerJobs.value_or(1U), {}, projectLoadOptions);
    graph = graphLoader.load(generated, message);
    if (!graph.has_value())
        return result.failure(error,
            "dependency closure is invalid: " + message,
            ExitCode::CompilationError);
    if (!options.precompiledOnly || graph->projects().size() <= 1U)
        return ExitCode::Success;
    const compilation::Project& sourceDependency = graph->projects().front();
    return result.failure(error,
        "--precompiled-only rejected source ProjectReference module '" +
            sourceDependency.moduleName() + "' at '" +
            pathText(sourceDependency.projectFile()) +
            "'; publish and reference its .lib + .nsmeta closure",
        ExitCode::CompilationError);
}

void updateRootDependencyProvenance(
    const fs::path& project,
    const compilation::Project& loaded,
    const compilation::CompilationInputManifest& manifest,
    std::vector<DependencyProvenance>& provenance)
{
    for (DependencyProvenance& record : provenance)
    {
        if (record.kind != "source") continue;
        record.path = project;
        std::vector<fs::path> rootInputs = {project};
        for (const compilation::ProjectSource& source : loaded.sourceSet().sources())
            rootInputs.push_back(source.absolutePath);
        record.fingerprint = fingerprintFiles(manifest, std::move(rootInputs));
    }
}

void printDependencyProvenance(
    std::ostream& output,
    const std::vector<DependencyProvenance>& provenance)
{
    output << "Dependency provenance:\n";
    for (const DependencyProvenance& record : provenance)
        output << "  " << record.kind << ' ' << record.identity << " path="
               << pathText(record.path) << " sha256=" << record.fingerprint << '\n';
}

void selectBuildAction(
    compilation::ProjectOutputKind outputKind,
    std::string& action,
    std::string& extension)
{
    if (outputKind == compilation::ProjectOutputKind::Executable)
    {
        action = "--emit-project-oop-exe";
        extension = ".exe";
    }
    else if (outputKind == compilation::ProjectOutputKind::Library)
    {
        action = "--emit-project-oop-lib";
        extension = ".lib";
    }
    else
    {
        action = "--emit-project-oop-dll";
        extension = ".dll";
    }
}

void appendNestedCompilerOptions(
    const BuildOptions& options,
    const std::optional<fs::path>& libraryReferenceRoot,
    const std::optional<std::size_t>& compilerJobs,
    std::vector<std::string>& owned)
{
    if (libraryReferenceRoot.has_value())
    {
        owned.push_back("--library-reference-root");
        owned.push_back(libraryReferenceRoot->string());
    }
    if (compilerJobs.has_value())
    {
        owned.push_back("-j");
        owned.push_back(std::to_string(*compilerJobs));
    }
    if (options.progress)
    {
        owned.push_back(options.progressFormat.has_value()
            ? "--progress=" + *options.progressFormat
            : "--progress");
    }
    else if (options.progressFormat.has_value())
    {
        owned.push_back("--progress-format=" + *options.progressFormat);
    }
}

ExitCode invokeNestedCompiler(
    const BuildOptions& options,
    bool json,
    const PreparedDriverExecutor& executor,
    compilation::CompilationInputManifest& manifest,
    const std::vector<std::string_view>& views,
    std::ostream& output,
    std::ostream& error,
    std::string& compilerOutputText,
    std::ostringstream& compilerOutput,
    std::ostringstream& compilerError)
{
    const bool forwardJsonProgress = json && options.progress &&
        options.progressFormat == "json";
    std::ostream& nestedError = json
        ? static_cast<std::ostream&>(compilerError)
        : error;
    if (forwardJsonProgress)
    {
        ProgressJsonForwardingBuffer progressBuffer(output, compilerOutputText);
        std::ostream progressOutput(&progressBuffer);
        const ExitCode code = executor(
            views, progressOutput, nestedError, &manifest);
        progressBuffer.finish();
        return code;
    }
    std::ostream& nestedOutput = json
        ? static_cast<std::ostream&>(compilerOutput)
        : output;
    const ExitCode code = executor(views, nestedOutput, nestedError, &manifest);
    if (json) compilerOutputText = compilerOutput.str();
    return code;
}

ExitCode executeConfiguredBuild(
    const BuildOptions& options,
    bool json,
    const PreparedDriverExecutor& executor,
    const fs::path& project,
    const compilation::ProjectBuildSettings& projectSettings,
    const std::string& configuration,
    const std::string& optimization,
    const std::string& debug,
    const std::string& linker,
    const std::optional<fs::path>& libraryReferenceRoot,
    const std::optional<std::size_t>& compilerJobs,
    const fs::path& generated,
    const compilation::Project& loaded,
    compilation::CompilationInputManifest& manifest,
    const std::vector<DependencyProvenance>& provenance,
    CommandOutput& result,
    std::ostream& output,
    std::ostream& error,
    BuildResult* buildResult)
{
    std::string action;
    std::string extension;
    selectBuildAction(loaded.outputKind(), action, extension);
    const fs::path artifact = projectSettings.outputPath.value_or(
        project.parent_path() / "bin" / configuration / (loaded.moduleName() + extension));
    std::error_code fileError;
    fs::create_directories(artifact.parent_path(), fileError);
    if (fileError)
        return result.failure(error,
            "cannot create build output directory: " + fileError.message(),
            ExitCode::InternalError);

    std::vector<std::string> owned{action, generated.string(), "-" + optimization, debug};
    appendNestedCompilerOptions(options, libraryReferenceRoot, compilerJobs, owned);
    owned.push_back("-o");
    owned.push_back(artifact.string());
    std::vector<std::string_view> views;
    for (const std::string& value : owned) views.push_back(value);

    std::string compilerOutputText;
    std::ostringstream compilerOutput;
    std::ostringstream compilerError;
    const ExitCode code = invokeNestedCompiler(options, json, executor, manifest, views,
        output, error, compilerOutputText, compilerOutput, compilerError);
    if (code != ExitCode::Success)
    {
        if (json)
            return result.failure(error,
                compilerError.str().empty() ? "compiler failed" : compilerError.str(),
                code);
        return code;
    }

    std::size_t contentFiles = 0;
    std::string message;
    if (!publishContentDirectory(
            project.parent_path(), artifact.parent_path(), contentFiles, message))
        return result.failure(error, message, ExitCode::InternalError);
    if (buildResult != nullptr)
        *buildResult = BuildResult{
            project, artifact, compilerOutputText, compilerError.str()};
    return result.success(output,
        "Build succeeded: " + artifact.string(),
        "\"artifact\":\"" + jsonEscape(pathText(artifact)) +
        "\",\"configuration\":\"" + jsonEscape(configuration) +
        "\",\"optimization\":\"" + jsonEscape(optimization) +
        "\",\"linker\":\"" + jsonEscape(linker) +
        "\",\"contentFiles\":" + std::to_string(contentFiles) +
        (options.dependencyReport
            ? ",\"dependencyProvenance\":" + dependencyProvenanceJson(provenance)
            : std::string{}) +
        ",\"stdout\":\"" + jsonEscape(compilerOutputText) +
        "\",\"stderr\":\"" + jsonEscape(compilerError.str()) + "\"");
}

ExitCode executeBuild(const BuildOptions& options, bool json, const PreparedDriverExecutor& executor,
                      std::ostream& output, std::ostream& error, BuildResult* buildResult)
{
    CommandOutput result{json, "build", {}};
    fs::path project;
    std::string message;
    if (!inferProject(options.project, project, message))
        return result.failure(error, message);
    result.project = project;

    compilation::ProjectLoader loader;
    const std::optional<compilation::ProjectBuildSettings> projectSettings =
        loader.loadBuildSettings(project, message);
    if (!projectSettings.has_value())
        return result.failure(error, message, ExitCode::CompilationError);
    Sidecar sidecar;
    if (!loadSidecar(project, sidecar, message, false))
        return result.failure(error, message);

    std::string configuration, optimization, debug, linker;
    std::optional<fs::path> libraryReferenceRoot;
    std::optional<std::size_t> compilerJobs;
    if (!resolveBuildSettings(options, *projectSettings, sidecar,
            configuration, optimization, debug, linker,
            libraryReferenceRoot, compilerJobs, message))
        return result.failure(error, message);

    RestorePlan plan;
    const ExitCode restored = restoreBuildPlan(
        options, json, sidecar, project, result, output, error, plan, message);
    if (restored != ExitCode::Success) return restored;

    fs::path generated;
    if (!makeGeneratedProject(project, plan, generated, message))
        return result.failure(error, message, ExitCode::InternalError);
    TemporaryFile removeGenerated(generated);
    compilation::ProjectLoadOptions projectLoadOptions;
    projectLoadOptions.libraryReferenceRoot = libraryReferenceRoot;
    std::optional<compilation::Project> loaded;
    std::optional<compilation::ProjectGraph> graph;
    const ExitCode closure = loadBuildClosure(options, generated,
        projectLoadOptions, compilerJobs, result, loader, loaded, graph, message, error);
    if (closure != ExitCode::Success) return closure;

    compilation::CompilationInputManifest manifest = createCompilationInputManifest(
        std::move(*graph), std::move(projectLoadOptions));
    addVerifiedRestoreDigests(manifest, plan);
    compilation::LibraryMetadataRepository metadataRepository;
    if (!manifest.calculateIdentity(metadataRepository, message))
        return result.failure(error, message, ExitCode::CompilationError);
    // The provenance hashes every project input again; only --dependency-report prints it or puts
    // it in the JSON result, so it is computed only then.
    std::vector<DependencyProvenance> provenance;
    if (options.dependencyReport)
    {
        provenance = dependencyProvenance(manifest.projectGraph(), plan, manifest);
        updateRootDependencyProvenance(project, *loaded, manifest, provenance);
        if (!json) printDependencyProvenance(output, provenance);
    }
    return executeConfiguredBuild(
        options,
        json,
        executor,
        project,
        *projectSettings,
        configuration,
        optimization,
        debug,
        linker,
        libraryReferenceRoot,
        compilerJobs,
        generated,
        *loaded,
        manifest,
        provenance,
        result,
        output,
        error,
        buildResult);
}

ExitCode runBuild(std::span<const std::string_view> raw, std::ostream& output, std::ostream& error,
                  const PreparedDriverExecutor& executor)
{
    CommonArguments parsed = stripOutputFormat(raw);
    BuildOptions options;
    std::string message;
    CommandOutput result{parsed.json, "build", {}};
    if (!parseBuildOptions(parsed.values, false, options, message)) return result.failure(error, message);
    return executeBuild(options, parsed.json, executor, output, error, nullptr);
}

#if defined(_WIN32)
std::wstring quoteWindows(std::wstring_view value)
{
    if (value.find_first_of(L" \t\"") == std::wstring_view::npos) return std::wstring(value);
    std::wstring result(1, L'"');
    std::size_t slashes = 0;
    for (wchar_t character : value)
    {
        if (character == L'\\') { ++slashes; continue; }
        if (character == L'"')
        {
            result.append(slashes * 2U + 1U, L'\\');
            result.push_back(L'"');
            slashes = 0;
            continue;
        }
        result.append(slashes, L'\\'); slashes = 0; result.push_back(character);
    }
    result.append(slashes * 2U, L'\\');
    result.push_back(L'"');
    return result;
}

bool launch(const fs::path &executable, const std::vector<std::string> &arguments, unsigned long &exitCode,
            std::string &error)
{
    std::wstring command = quoteWindows(executable.wstring());
    for (const std::string& argument : arguments)
    {
        const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argument.data(),
                                             static_cast<int>(argument.size()), nullptr, 0);
        if (size <= 0) { error = "program argument is not valid UTF-8"; return false; }
        std::wstring wide(static_cast<std::size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argument.data(), static_cast<int>(argument.size()),
                            wide.data(), size);
        command.push_back(L' ');
        command += quoteWindows(wide);
    }
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const std::wstring directory = executable.parent_path().wstring();
    if (!CreateProcessW(executable.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE, 0, nullptr,
                        directory.c_str(), &startup, &process))
    { error = "cannot launch program (Windows error " + std::to_string(GetLastError()) + ")"; return false; }
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    const BOOL obtained = GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    if (!obtained) { error = "cannot read program exit code"; return false; }
    exitCode = code;
    return true;
}
#endif

ExitCode runRun(std::span<const std::string_view> raw, std::ostream& output, std::ostream& error,
                const PreparedDriverExecutor& executor)
{
    CommonArguments parsed = stripOutputFormat(raw);
    CommandOutput result{parsed.json, "run", {}};
    BuildOptions options;
    std::string message;
    if (!parseBuildOptions(parsed.values, true, options, message)) return result.failure(error, message);
    BuildResult built;
    std::ostringstream buildOutput;
    const ExitCode code = executeBuild(options, parsed.json, executor, buildOutput, error, &built);
    if (code != ExitCode::Success) return code;
    if (built.artifact.extension() != ".exe") return result.failure(error, "nsc run requires an executable project");
#if defined(_WIN32)
    unsigned long processExit = 0;
    if (!parsed.json) output << buildOutput.str();
    if (!launch(built.artifact, options.programArguments, processExit, message))
        return result.failure(error, message, ExitCode::CompilationError);
    result.project = built.project;
    if (processExit != 0U)
        return result.failure(error, "program exited with code " + std::to_string(processExit),
                              ExitCode::CompilationError);
    return result.success(output, "Program exited with code 0",
        "\"artifact\":\"" + jsonEscape(pathText(built.artifact)) + "\",\"programExitCode\":0");
#else
    return result.failure(error, "nsc run is currently supported on Windows only");
#endif
}

ExitCode runClean(std::span<const std::string_view> raw, std::ostream& output, std::ostream& error)
{
    CommonArguments parsed = stripOutputFormat(raw);
    CommandOutput result{parsed.json, "clean", {}};
    if (parsed.values.size() > 2U) return result.failure(error, "usage: nsc clean [file.nsproj]");
    std::optional<fs::path> requested;
    if (parsed.values.size() == 2U) requested = fs::path(parsed.values[1]);
    fs::path project;
    std::string message;
    if (!inferProject(requested, project, message)) return result.failure(error, message);
    result.project = project;
    const fs::path root = project.parent_path();
    std::uintmax_t removed = 0;
    std::error_code fileError;
    for (const char* name : {"bin", "obj"})
    {
        const fs::path target = root / name;
        if (target.parent_path() != root)
            return result.failure(error, "clean safety validation failed", ExitCode::InternalError);
        removed += fs::remove_all(target, fileError);
        if (fileError)
            return result.failure(error, "cannot clean '" + target.string() + "': " + fileError.message(),
                                  ExitCode::InternalError);
    }
    for (fs::directory_iterator iterator(root, fileError), end; iterator != end && !fileError;
         iterator.increment(fileError))
        if (iterator->is_regular_file() && iterator->path().filename().string().starts_with(".nspm.") &&
            iterator->path().extension() == ".nsproj") { fs::remove(iterator->path(), fileError); ++removed; }
    if (fileError)
        return result.failure(error, "cannot clean generated project files: " + fileError.message(),
                              ExitCode::InternalError);
    return result.success(output,
                          "Cleaned " + std::to_string(removed) + " filesystem entr" + (removed == 1U ? "y" : "ies"),
                          "\"removed\":" + std::to_string(removed));
}

ExitCode runPackage(std::span<const std::string_view> raw, std::ostream& output, std::ostream& error)
{
    CommonArguments parsed = stripOutputFormat(raw);
    const std::string command(parsed.values.front());
    CommandOutput result{parsed.json, command, {}};
    if (parsed.values.size() < 2U || parsed.values[1] != "package")
        return result.failure(error, "usage: nsc " + command + " package ...");
    std::optional<fs::path> requested;
    std::string id;
    std::string version = "*";
    std::string source;
    std::size_t index = 2;
    if (command != "list")
    {
        if (index >= parsed.values.size()) return result.failure(error, "package id is required");
        id = parsed.values[index++];
    }
    for (; index < parsed.values.size(); ++index)
    {
        const std::string_view argument = parsed.values[index];
        if (command == "add" && argument == "--version" && index + 1 < parsed.values.size())
            version = parsed.values[++index];
        else if (command == "add" && argument == "--source" && index + 1 < parsed.values.size())
            source = parsed.values[++index];
        else if (argument.starts_with('-'))
            return result.failure(error, "unknown package option: '" + std::string(argument) + "'");
        else if (requested.has_value()) return result.failure(error, "only one project may be specified");
        else requested = fs::path(argument);
    }
    fs::path project;
    std::string message;
    if (!inferProject(requested, project, message)) return result.failure(error, message);
    result.project = project;
    Sidecar sidecar;
    if (!loadSidecar(project, sidecar, message, false)) return result.failure(error, message);
    if (command == "list")
    {
        if (parsed.json)
        {
            std::ostringstream packages;
            packages << "\"packages\":[";
            for (std::size_t packageIndex = 0; packageIndex < sidecar.packages.size(); ++packageIndex)
            {
                const auto& package = sidecar.packages[packageIndex];
                if (packageIndex != 0) packages << ',';
                packages << "{\"id\":\"" << jsonEscape(package.id) << "\",\"version\":\""
                         << jsonEscape(package.version) << "\",\"source\":\"" << jsonEscape(package.source) << "\"}";
            }
            packages << ']';
            return result.success(output, std::to_string(sidecar.packages.size()) + " package(s)", packages.str());
        }
        for (const auto& package : sidecar.packages)
            output << package.id << ' ' << package.version << (package.source.empty() ? "" : " " + package.source)
                   << '\n';
        return ExitCode::Success;
    }
    const auto matches = [&](const PackageRequest& package) {
        std::string left = package.id;
        std::string right = id;
        std::transform(left.begin(), left.end(), left.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(right.begin(), right.end(), right.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return left == right;
    };
    auto found = std::find_if(sidecar.packages.begin(), sidecar.packages.end(), matches);
    if (command == "add")
    {
        if (!safeComponent(id) || !safeComponent(version)) return result.failure(error, "unsafe package id or version");
        if (found == sidecar.packages.end()) sidecar.packages.push_back(PackageRequest{id, version, source});
        else *found = PackageRequest{id, version, source};
        std::sort(sidecar.packages.begin(), sidecar.packages.end(),
                  [](const auto &left, const auto &right) { return left.id < right.id; });
    }
    else
    {
        if (found == sidecar.packages.end()) return result.failure(error, "package is not present: '" + id + "'");
        sidecar.packages.erase(found);
    }
    if (!atomicWrite(sidecarPath(project), renderSidecar(sidecar), message))
        return result.failure(error, message, ExitCode::InternalError);
    return result.success(output, command == "add" ? "Package request saved: " + id : "Package request removed: " + id,
                          "\"package\":\"" + jsonEscape(id) + "\"");
}

std::optional<ExplainTopic> explainTopic(std::string_view topic)
{
    if (topic == "build")
        return ExplainTopic{"available", "project defaults, progress, diagnostics, cache", "long-lived build server"};
    if (topic == "cache")
        return ExplainTopic{"available", "content-addressed objects, quarantine, eviction, pipeline identity",
                            "cross-process daemon ownership"};
    if (topic == "overload")
        return ExplainTopic{"partial", "deterministic semantic diagnostics", "ranked candidate causal chain"};
    if (topic == "trait")
        return ExplainTopic{"partial", "static and dynamic interface MIR", "complete coherence causal chain"};
    if (topic == "effect")
        return ExplainTopic{"partial", "MIR ownership, cleanup, allocation and synchronization sites",
                            "general effect type system"};
    if (topic == "api")
        return ExplainTopic{"available", "nsc --api-diff over NSMETA fingerprints", "source migration fix-its"};
    if (topic == "abi")
        return ExplainTopic{"available", "nsc --abi-diff and structural ABI report",
                            "final cross-toolchain certification"};
    if (topic == "session")
        return ExplainTopic{"partial",
                            "one project descriptor session per invocation, bounded scheduler, cancellation hooks",
                            "persistent isolated compiler server"};
    if (topic == "packages")
        return ExplainTopic{"partial", "new, restore, deterministic lock, build, run, clean, add, remove, list",
                            "test, pack, publish registry workflow"};
    if (topic == "debug-allocator")
        return ExplainTopic{"missing", "MIR allocation and drop visibility",
                            "guarded allocator, leak, UAF and double-drop runtime"};
    return std::nullopt;
}

ExitCode runExplain(std::span<const std::string_view> raw, std::ostream& output,
                    std::ostream& error, const PreparedDriverExecutor& executor)
{
    CommonArguments parsed = stripOutputFormat(raw);
    CommandOutput result{parsed.json, "explain", {}};
    if (parsed.values.size() < 2U)
        return result.failure(error, "usage: nsc explain <topic> [file.nsproj]");
    const std::string_view topic = parsed.values[1];
    if (topic == "optimization")
    {
        if (parsed.values.size() != 2U)
            return result.failure(error, "usage: nsc explain optimization");
        if (parsed.json)
            return result.failure(error,
                                  "optimization contract is a versioned text manifest; omit --output-format=json");
        constexpr std::array<std::string_view, 1> nested = {"--print-optimization-contract"};
        return executor(nested, output, error, nullptr);
    }
    if (topic == "costs")
    {
        if (parsed.values.size() > 3U)
            return result.failure(error, "usage: nsc explain costs [file.nsproj]");
        std::optional<fs::path> requested;
        if (parsed.values.size() == 3U) requested = fs::path(parsed.values[2]);
        fs::path project;
        std::string message;
        if (!inferProject(requested, project, message)) return result.failure(error, message);
        const std::string action = parsed.json ? "--explain-project-costs-json" : "--explain-project-costs";
        const std::string projectText = project.string();
        const std::array<std::string_view, 2> nested = {action, projectText};
        return executor(nested, output, error, nullptr);
    }
    if (parsed.values.size() != 2U)
        return result.failure(error, "topic '" + std::string(topic) + "' does not accept a project argument");
    const std::optional<ExplainTopic> description = explainTopic(topic);
    if (!description.has_value())
        return result.failure(error, "unknown explain topic '" + std::string(topic) +
                                         "'; expected build, cache, optimization, costs, overload, trait, effect, api, "
                                         "abi, session, packages, or debug-allocator");
    if (parsed.json)
    {
        output << "{\"schema\":\"NSEXPLAIN 1.0\",\"topic\":\"" << jsonEscape(topic)
               << "\",\"status\":\"" << description->status
               << "\",\"present\":\"" << jsonEscape(description->present)
               << "\",\"missing\":\"" << jsonEscape(description->missing) << "\"}\n";
    }
    else
    {
        output << "NSEXPLAIN 1.0\n"
               << "topic=" << topic << '\n'
               << "status=" << description->status << '\n'
               << "present=" << description->present << '\n'
               << "missing=" << description->missing << '\n';
    }
    return ExitCode::Success;
}
} // namespace

bool isProjectCommand(std::string_view command) noexcept
{
    return command == "new" || command == "restore" || command == "build" || command == "run" ||
           command == "clean" || command == "add" || command == "remove" || command == "list" ||
           command == "explain";
}

ExitCode runProjectCommand(std::span<const std::string_view> arguments, std::ostream& output,
                           std::ostream& error, const PreparedDriverExecutor& executor)
{
    if (arguments.empty()) return ExitCode::CommandLineError;
    if (arguments.front() == "new") return runNew(arguments, output, error);
    if (arguments.front() == "restore") return runRestore(arguments, output, error);
    if (arguments.front() == "build") return runBuild(arguments, output, error, executor);
    if (arguments.front() == "run") return runRun(arguments, output, error, executor);
    if (arguments.front() == "clean") return runClean(arguments, output, error);
    if (arguments.front() == "explain") return runExplain(arguments, output, error, executor);
    return runPackage(arguments, output, error);
}

ExitCode runProjectCommand(std::span<const std::string_view> arguments, std::ostream& output,
                           std::ostream& error, const DriverExecutor& executor)
{
    return runProjectCommand(
        arguments,
        output,
        error,
        [&executor](std::span<const std::string_view> nested,
                    std::ostream& nestedOutput,
                    std::ostream& nestedError,
                    compilation::CompilationInputManifest*)
        {
            return executor(nested, nestedOutput, nestedError);
        });
}
} // namespace nativesharp::driver
