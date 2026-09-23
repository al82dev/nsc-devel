#include "nativesharp/Driver/ProjectCommands.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Compilation/ProjectLoader.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>
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

std::string jsonEscape(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    constexpr char digits[] = "0123456789abcdef";
    for (const unsigned char character : value)
    {
        switch (character)
        {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (character < 0x20U)
            {
                result += "\\u00";
                result.push_back(digits[(character >> 4U) & 0x0fU]);
                result.push_back(digits[character & 0x0fU]);
            }
            else result.push_back(static_cast<char>(character));
            break;
        }
    }
    return result;
}

std::string pathText(const fs::path& path)
{
    return path.generic_string();
}

std::optional<std::string> environmentValue(const char* name)
{
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) return std::nullopt;
    std::string result(value);
    std::free(value);
    return result;
#else
    if (const char* value = std::getenv(name)) return std::string(value);
    return std::nullopt;
#endif
}

struct Json final
{
    using Object = std::map<std::string, Json>;
    using Array = std::vector<Json>;
    std::variant<std::nullptr_t, bool, double, std::string, Object, Array> value = nullptr;

    const Object* object() const { return std::get_if<Object>(&value); }
    const Array* array() const { return std::get_if<Array>(&value); }
    const std::string* string() const { return std::get_if<std::string>(&value); }
    const bool* boolean() const { return std::get_if<bool>(&value); }
};

class JsonParser final
{
public:
    explicit JsonParser(std::string_view text) : text_(text) {}

    bool parse(Json& value, std::string& error)
    {
        skip();
        if (!parseValue(value, error)) return false;
        skip();
        if (position_ != text_.size()) return fail(error, "unexpected trailing JSON content");
        return true;
    }

private:
    bool fail(std::string& error, std::string_view message) const
    {
        error = std::string(message) + " at byte " + std::to_string(position_);
        return false;
    }

    void skip()
    {
        while (position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_])))
            ++position_;
    }

    bool parseValue(Json& value, std::string& error)
    {
        if (position_ >= text_.size()) return fail(error, "expected JSON value");
        const char current = text_[position_];
        if (current == '{') return parseObject(value, error);
        if (current == '[') return parseArray(value, error);
        if (current == '"')
        {
            std::string parsed;
            if (!parseString(parsed, error)) return false;
            value.value = std::move(parsed);
            return true;
        }
        if (text_.substr(position_, 4) == "true") { position_ += 4; value.value = true; return true; }
        if (text_.substr(position_, 5) == "false") { position_ += 5; value.value = false; return true; }
        if (text_.substr(position_, 4) == "null") { position_ += 4; value.value = nullptr; return true; }
        if (current == '-' || std::isdigit(static_cast<unsigned char>(current)))
        {
            const std::size_t start = position_++;
            while (position_ < text_.size() &&
                   (std::isdigit(static_cast<unsigned char>(text_[position_])) ||
                    text_[position_] == '.' || text_[position_] == 'e' || text_[position_] == 'E' ||
                    text_[position_] == '+' || text_[position_] == '-')) ++position_;
            try { value.value = std::stod(std::string(text_.substr(start, position_ - start))); }
            catch (...) { return fail(error, "invalid JSON number"); }
            return true;
        }
        return fail(error, "invalid JSON value");
    }

    bool parseString(std::string& value, std::string& error)
    {
        if (text_[position_++] != '"') return fail(error, "expected string");
        while (position_ < text_.size())
        {
            const unsigned char current = static_cast<unsigned char>(text_[position_++]);
            if (current == '"') return true;
            if (current < 0x20U) return fail(error, "control character in string");
            if (current != '\\') { value.push_back(static_cast<char>(current)); continue; }
            if (position_ >= text_.size()) return fail(error, "incomplete string escape");
            const char escaped = text_[position_++];
            switch (escaped)
            {
            case '"': value.push_back('"'); break;
            case '\\': value.push_back('\\'); break;
            case '/': value.push_back('/'); break;
            case 'b': value.push_back('\b'); break;
            case 'f': value.push_back('\f'); break;
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            case 'u':
            {
                if (position_ + 4U > text_.size()) return fail(error, "incomplete unicode escape");
                unsigned codePoint = 0;
                for (unsigned index = 0; index < 4U; ++index)
                {
                    const char digit = text_[position_++];
                    codePoint <<= 4U;
                    if (digit >= '0' && digit <= '9') codePoint += static_cast<unsigned>(digit - '0');
                    else if (digit >= 'a' && digit <= 'f') codePoint += 10U + static_cast<unsigned>(digit - 'a');
                    else if (digit >= 'A' && digit <= 'F') codePoint += 10U + static_cast<unsigned>(digit - 'A');
                    else return fail(error, "invalid unicode escape");
                }
                if (codePoint <= 0x7fU) value.push_back(static_cast<char>(codePoint));
                else if (codePoint <= 0x7ffU)
                {
                    value.push_back(static_cast<char>(0xc0U | (codePoint >> 6U)));
                    value.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
                }
                else
                {
                    value.push_back(static_cast<char>(0xe0U | (codePoint >> 12U)));
                    value.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3fU)));
                    value.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
                }
                break;
            }
            default: return fail(error, "invalid string escape");
            }
        }
        return fail(error, "unterminated string");
    }

    bool parseObject(Json& value, std::string& error)
    {
        ++position_;
        Json::Object object;
        skip();
        if (position_ < text_.size() && text_[position_] == '}')
        { ++position_; value.value = std::move(object); return true; }
        for (;;)
        {
            skip();
            if (position_ >= text_.size() || text_[position_] != '"') return fail(error, "expected object key");
            std::string key;
            if (!parseString(key, error)) return false;
            skip();
            if (position_ >= text_.size() || text_[position_++] != ':') return fail(error, "expected ':'");
            skip();
            Json member;
            if (!parseValue(member, error)) return false;
            if (!object.emplace(std::move(key), std::move(member)).second)
                return fail(error, "duplicate object key");
            skip();
            if (position_ >= text_.size()) return fail(error, "unterminated object");
            if (text_[position_] == '}') { ++position_; value.value = std::move(object); return true; }
            if (text_[position_++] != ',') return fail(error, "expected ','");
        }
    }

    bool parseArray(Json& value, std::string& error)
    {
        ++position_;
        Json::Array array;
        skip();
        if (position_ < text_.size() && text_[position_] == ']')
        { ++position_; value.value = std::move(array); return true; }
        for (;;)
        {
            skip();
            Json element;
            if (!parseValue(element, error)) return false;
            array.push_back(std::move(element));
            skip();
            if (position_ >= text_.size()) return fail(error, "unterminated array");
            if (text_[position_] == ']') { ++position_; value.value = std::move(array); return true; }
            if (text_[position_++] != ',') return fail(error, "expected ','");
        }
    }

    std::string_view text_;
    std::size_t position_ = 0;
};

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
    if (size > kMaximumJsonBytes) { error = "file is larger than the 4 MiB safety limit: '" + path.string() + "'"; return false; }
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
    std::error_code fileError;
    if (path.has_parent_path()) fs::create_directories(path.parent_path(), fileError);
    if (fileError) { error = "cannot create directory for '" + path.string() + "': " + fileError.message(); return false; }
#if defined(_WIN32)
    const std::string suffix = ".tmp." + std::to_string(_getpid());
#else
    const std::string suffix = ".tmp";
#endif
    const fs::path temporary = path.string() + suffix;
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    stream.close();
    if (!stream) { fs::remove(temporary, fileError); error = "cannot write '" + temporary.string() + "'"; return false; }
    fs::remove(path, fileError);
    fileError.clear();
    fs::rename(temporary, path, fileError);
    if (fileError)
    {
        fs::remove(temporary, fileError);
        error = "cannot publish '" + path.string() + "': " + fileError.message();
        return false;
    }
    return true;
}

struct PackageRequest final { std::string id; std::string version; std::string source; };
struct BuildPreferences final
{
    std::string configuration = "Debug";
    std::string optimization = "O0";
    std::string debugInformation = "full";
    std::string linker = "NSLink";
    std::string target = "x86_64-pc-windows-msvc";
    bool restoreBeforeBuild = true;
};
struct Sidecar final
{
    std::string project;
    std::string rootNamespace;
    BuildPreferences build;
    std::vector<PackageRequest> packages;
    bool createInitialSource = false;
};

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
            if (package.id.empty() || package.version.empty()) { error = "package id and version cannot be empty"; return false; }
            std::string key = package.id;
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
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
    if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value.front())) || value.front() == '_')) return false;
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
    for (fs::directory_iterator iterator(fs::current_path(), fileError), end; iterator != end && !fileError; iterator.increment(fileError))
        if (iterator->is_regular_file(fileError) && iterator->path().extension() == ".nsproj") found.push_back(iterator->path());
    if (fileError) { error = "cannot enumerate the current directory: " + fileError.message(); return false; }
    if (found.empty()) { error = "no .nsproj was found in the current directory"; return false; }
    if (found.size() != 1U) { error = "more than one .nsproj exists; specify the project explicitly"; return false; }
    project = fs::absolute(found.front(), fileError).lexically_normal();
    return !fileError;
}

struct CommandOutput final
{
    bool json = false;
    std::string command;
    fs::path project;

    ExitCode success(std::ostream& output, std::string_view message,
                     std::string_view extra = {}) const
    {
        if (json)
        {
            output << "{\"schemaVersion\":1,\"command\":\"" << jsonEscape(command)
                   << "\",\"success\":true";
            if (!project.empty()) output << ",\"project\":\"" << jsonEscape(pathText(project)) << '"';
            if (!extra.empty()) output << ',' << extra;
            output << ",\"message\":\"" << jsonEscape(message) << "\"}\n";
        }
        else output << message << '\n';
        return ExitCode::Success;
    }

    ExitCode failure(std::ostream& error, std::string_view message,
                     ExitCode code = ExitCode::CommandLineError) const
    {
        if (json)
            error << "{\"schemaVersion\":1,\"command\":\"" << jsonEscape(command)
                  << "\",\"success\":false,\"exitCode\":" << static_cast<int>(code)
                  << ",\"message\":\"" << jsonEscape(message) << "\"}\n";
        else error << "nsc: error: " << message << '\n';
        return code;
    }
};

struct CommonArguments final
{
    bool json = false;
    std::vector<std::string_view> values;
};

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
    if (parsed.values.size() < 2U) return result.failure(error, "usage: nsc new <console|classlib|dll> -n <name> [-o <directory>] [--force]");
    const std::string kind(parsed.values[1]);
    std::string name;
    std::optional<fs::path> directory;
    bool force = false;
    for (std::size_t index = 2; index < parsed.values.size(); ++index)
    {
        const std::string_view argument = parsed.values[index];
        if ((argument == "-n" || argument == "--name") && index + 1 < parsed.values.size()) name = parsed.values[++index];
        else if ((argument == "-o" || argument == "--output") && index + 1 < parsed.values.size()) directory = fs::path(parsed.values[++index]);
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
    const std::string xml = "<Project>\n  <PropertyGroup>\n    <Name>" + name + "</Name>\n    <OutputType>" + outputType +
        "</OutputType>\n  </PropertyGroup>\n  <ItemGroup>\n    <Compile Include=\"**/*.ns\" />\n  </ItemGroup>\n</Project>\n";
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

struct Artifact final
{
    std::string type;
    fs::path source;
    fs::path relativeDestination;
    std::string hash;
    std::uintmax_t size = 0;
};
struct RestoredPackage final { PackageRequest request; fs::path sourceRoot; std::vector<Artifact> artifacts; };
struct RestorePlan final { std::vector<RestoredPackage> packages; std::string lock; fs::path root; };

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
    if (!isSafeRelative(relative)) { error = "package artifact path is not safe: '" + relative.string() + "'"; return false; }
    const fs::path source = (package.sourceRoot / relative).lexically_normal();
    std::error_code fileError;
    const fs::path canonicalRoot = fs::weakly_canonical(package.sourceRoot, fileError);
    if (fileError) { error = "cannot resolve package root: " + fileError.message(); return false; }
    const fs::path canonicalSource = fs::weakly_canonical(source, fileError);
    if (fileError || !fs::is_regular_file(canonicalSource)) { error = "package artifact does not exist: '" + source.string() + "'"; return false; }
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
    else if (const std::optional<std::string> local = environmentValue("LOCALAPPDATA")) source = fs::path(*local) / "NativeSharp" / "packages";
    else { error = "package source is not configured; use --source or NSC_PACKAGE_ROOT"; return false; }
    std::error_code fileError;
    source = fs::absolute(source, fileError).lexically_normal();
    if (fileError) { error = "cannot resolve package source"; return false; }
    if (!packageManifest(source).has_value() && !looksLikeConventionPackage(source))
        source /= fs::path(request.id) / request.version;
    if (!fs::is_directory(source)) { error = "package source directory does not exist: '" + source.string() + "'"; return false; }
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
        for (const Json& library : *libraries->array())
        {
            if (!appendArtifact(package, "library", stringMember(library, "library"), destinationBase, error) ||
                !appendArtifact(package, "metadata", stringMember(library, "metadata"), destinationBase, error)) return false;
            const std::string runtime = stringMember(library, "runtime");
            if (!runtime.empty() && !appendArtifact(package, "runtime", runtime, destinationBase, error)) return false;
        }
        if (const Json* files = member(root, "files"))
        {
            if (files->array() == nullptr) { error = "package files must be an array"; return false; }
            for (const Json& value : *files->array())
            {
                if (value.string() == nullptr || !appendArtifact(package, "file", *value.string(), destinationBase, error)) return false;
            }
        }
    }
    else
    {
        std::vector<fs::path> libraries;
        std::vector<fs::path> metadata;
        std::vector<fs::path> runtimes;
        for (fs::directory_iterator iterator(source, fileError), end; iterator != end && !fileError; iterator.increment(fileError))
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
        if (!runtimes.empty() && !appendArtifact(package, "runtime", runtimes.front(), destinationBase, error)) return false;
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
            if (!fileError) fs::copy_file(artifact.source, destination, fs::copy_options::overwrite_existing, fileError);
            if (fileError)
            {
                fs::remove_all(staging, fileError);
                error = "cannot stage package artifact '" + artifact.source.string() + "': " + fileError.message();
                return false;
            }
            ++copied;
        }
    if (fs::exists(plan.root)) fs::rename(plan.root, backup, fileError);
    if (fileError) { fs::remove_all(staging, fileError); error = "cannot begin restore transaction: " + fileError.message(); return false; }
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

struct RestoreOptions final
{
    std::optional<fs::path> project;
    std::optional<fs::path> source;
    bool force = false;
    bool locked = false;
    bool verify = false;
};

bool parseRestoreOptions(std::span<const std::string_view> arguments, RestoreOptions& options, std::string& error)
{
    for (std::size_t index = 1; index < arguments.size(); ++index)
    {
        const std::string_view argument = arguments[index];
        if (argument == "--force") options.force = true;
        else if (argument == "--locked-mode") options.locked = true;
        else if (argument == "--verify") options.verify = true;
        else if (argument == "--source" && index + 1 < arguments.size()) options.source = fs::path(arguments[++index]);
        else if (argument.starts_with('-')) { error = "unknown restore option: '" + std::string(argument) + "'"; return false; }
        else if (options.project.has_value()) { error = "restore accepts only one project"; return false; }
        else options.project = fs::path(argument);
    }
    if (options.force && (options.locked || options.verify)) { error = "--force cannot be combined with --locked-mode or --verify"; return false; }
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
        if (!readText(lockPath, existing, message)) return result.failure(error, "locked restore requires a readable lock file: " + message);
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
    if (!options.locked && !atomicWrite(lockPath, plan.lock, message))
        return result.failure(error, message, ExitCode::InternalError);
    if (completedPlan != nullptr) *completedPlan = plan;
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

class TemporaryFile final
{
public:
    explicit TemporaryFile(fs::path path) : path_(std::move(path)) {}
    ~TemporaryFile() { std::error_code error; fs::remove(path_, error); }
private:
    fs::path path_;
};

class EnvironmentOverride final
{
public:
    EnvironmentOverride(std::string name, std::optional<std::string> value) : name_(std::move(name))
    {
        if (!value.has_value()) return;
        active_ = true;
        previous_ = environmentValue(name_.c_str());
        set(*value);
    }
    ~EnvironmentOverride()
    {
        if (!active_) return;
        if (previous_.has_value()) set(*previous_); else clear();
    }
private:
    void set(const std::string& value)
    {
#if defined(_WIN32)
        _putenv_s(name_.c_str(), value.c_str());
#else
        setenv(name_.c_str(), value.c_str(), 1);
#endif
    }
    void clear()
    {
#if defined(_WIN32)
        _putenv_s(name_.c_str(), "");
#else
        unsetenv(name_.c_str());
#endif
    }
    std::string name_;
    std::optional<std::string> previous_;
    bool active_ = false;
};

struct BuildOptions final
{
    std::optional<fs::path> project;
    std::optional<std::string> configuration;
    std::optional<std::string> optimization;
    std::optional<std::string> debug;
    std::optional<std::string> linker;
    bool noRestore = false;
    std::vector<std::string> programArguments;
};

bool parseBuildOptions(std::span<const std::string_view> arguments, bool run, BuildOptions& options, std::string& error)
{
    bool program = false;
    for (std::size_t index = 1; index < arguments.size(); ++index)
    {
        const std::string_view argument = arguments[index];
        if (run && argument == "--") { program = true; continue; }
        if (program) { options.programArguments.emplace_back(argument); continue; }
        if ((argument == "-c" || argument == "--configuration") && index + 1 < arguments.size()) options.configuration = std::string(arguments[++index]);
        else if (argument == "--linker" && index + 1 < arguments.size()) options.linker = std::string(arguments[++index]);
        else if (argument == "--no-restore") options.noRestore = true;
        else if (argument == "-O0" || argument == "-O1" || argument == "-O2" || argument == "-O3") options.optimization = std::string(argument.substr(1));
        else if (argument == "-g" || argument == "-g0" || argument == "-gline-tables-only") options.debug = std::string(argument);
        else if (argument.starts_with('-')) { error = "unknown build option: '" + std::string(argument) + "'"; return false; }
        else if (options.project.has_value()) { error = "build accepts only one project"; return false; }
        else options.project = fs::path(argument);
    }
    return true;
}

bool loadPlanFromLock(const fs::path& project, RestorePlan& plan, std::string& error)
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
    std::size_t verified = 0;
    return verifyPlan(plan, verified, error);
}

struct BuildResult final { fs::path project; fs::path artifact; std::string compilerOutput; std::string compilerError; };

ExitCode executeBuild(const BuildOptions& options, bool json, const DriverExecutor& executor,
                      std::ostream& output, std::ostream& error, BuildResult* buildResult)
{
    CommandOutput result{json, "build", {}};
    fs::path project;
    std::string message;
    if (!inferProject(options.project, project, message)) return result.failure(error, message);
    result.project = project;
    Sidecar sidecar;
    if (!loadSidecar(project, sidecar, message, false)) return result.failure(error, message);
    std::string configuration = options.configuration.value_or(sidecar.build.configuration);
    if (configuration != "Debug" && configuration != "Release") return result.failure(error, "configuration must be Debug or Release");
    std::string optimization = options.optimization.value_or(sidecar.build.optimization);
    if (optimization != "O0" && optimization != "O1" && optimization != "O2" && optimization != "O3")
        return result.failure(error, "optimization must be O0, O1, O2, or O3");
    std::string debug = options.debug.value_or(sidecar.build.debugInformation == "none" ? "-g0" :
        sidecar.build.debugInformation == "line-tables-only" ? "-gline-tables-only" : "-g");
    std::string linker = options.linker.value_or(sidecar.build.linker);
    if (linker != "NSLink" && linker != "lld-link") return result.failure(error, "linker must be NSLink or lld-link");

    RestorePlan plan;
    if (options.noRestore || !sidecar.build.restoreBeforeBuild)
    {
        if (!sidecar.packages.empty() && !loadPlanFromLock(project, plan, message))
            return result.failure(error, message, ExitCode::CompilationError);
        plan.root = project.parent_path() / "obj" / "nativesharp" / "packages";
    }
    else
    {
        RestoreOptions restoreOptions;
        restoreOptions.project = project;
        std::ostringstream restoreOutput;
        std::ostringstream restoreError;
        const ExitCode restored = executeRestore(restoreOptions, false, restoreOutput, restoreError, &plan);
        if (restored != ExitCode::Success) return result.failure(error, restoreError.str(), restored);
        if (!json) output << restoreOutput.str();
    }
    fs::path generated;
    if (!makeGeneratedProject(project, plan, generated, message)) return result.failure(error, message, ExitCode::InternalError);
    TemporaryFile removeGenerated(generated);
    compilation::ProjectLoader loader;
    std::optional<compilation::Project> loaded = loader.load(generated, message);
    if (!loaded.has_value()) return result.failure(error, "generated build project is invalid: " + message, ExitCode::CompilationError);
    std::string action;
    std::string extension;
    if (loaded->outputKind() == compilation::ProjectOutputKind::Executable) { action = "--emit-project-oop-exe"; extension = ".exe"; }
    else if (loaded->outputKind() == compilation::ProjectOutputKind::Library) { action = "--emit-project-oop-lib"; extension = ".lib"; }
    else { action = "--emit-project-oop-dll"; extension = ".dll"; }
    const fs::path artifact = project.parent_path() / "bin" / configuration / (loaded->moduleName() + extension);
    std::error_code fileError;
    fs::create_directories(artifact.parent_path(), fileError);
    if (fileError) return result.failure(error, "cannot create build output directory: " + fileError.message(), ExitCode::InternalError);

    std::optional<std::string> linkerPath;
    if (linker == "NSLink")
    {
        const fs::path installed = "C:/native-sharp/bin/nslink.exe";
        if (fs::is_regular_file(installed)) linkerPath = installed.string();
    }
    else
    {
        const fs::path installed = "C:/clang-llvm/bin/lld-link.exe";
        if (fs::is_regular_file(installed)) linkerPath = installed.string();
    }
    EnvironmentOverride linkerEnvironment("NSC_LLD_LINK", linkerPath);
    const std::vector<std::string> owned{action, generated.string(), "-" + optimization, debug, "-o", artifact.string()};
    std::vector<std::string_view> views;
    for (const std::string& value : owned) views.push_back(value);
    std::ostringstream compilerOutput;
    std::ostringstream compilerError;
    std::ostream& nestedOutput = json ? static_cast<std::ostream&>(compilerOutput) : output;
    std::ostream& nestedError = json ? static_cast<std::ostream&>(compilerError) : error;
    const ExitCode code = executor(views, nestedOutput, nestedError);
    if (code != ExitCode::Success)
    {
        if (json)
            return result.failure(error, compilerError.str().empty() ? "compiler failed" : compilerError.str(), code);
        return code;
    }
    if (buildResult != nullptr) *buildResult = BuildResult{project, artifact, compilerOutput.str(), compilerError.str()};
    return result.success(output, "Build succeeded: " + artifact.string(),
        "\"artifact\":\"" + jsonEscape(pathText(artifact)) + "\",\"configuration\":\"" +
        jsonEscape(configuration) + "\",\"optimization\":\"" + jsonEscape(optimization) +
        "\",\"linker\":\"" + jsonEscape(linker) + "\",\"stdout\":\"" +
        jsonEscape(compilerOutput.str()) + "\",\"stderr\":\"" + jsonEscape(compilerError.str()) + "\"");
}

ExitCode runBuild(std::span<const std::string_view> raw, std::ostream& output, std::ostream& error,
                  const DriverExecutor& executor)
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
        if (character == L'"') { result.append(slashes * 2U + 1U, L'\\'); result.push_back(L'"'); slashes = 0; continue; }
        result.append(slashes, L'\\'); slashes = 0; result.push_back(character);
    }
    result.append(slashes * 2U, L'\\');
    result.push_back(L'"');
    return result;
}

bool launch(const fs::path& executable, const std::vector<std::string>& arguments, unsigned long& exitCode, std::string& error)
{
    std::wstring command = quoteWindows(executable.wstring());
    for (const std::string& argument : arguments)
    {
        const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argument.data(), static_cast<int>(argument.size()), nullptr, 0);
        if (size <= 0) { error = "program argument is not valid UTF-8"; return false; }
        std::wstring wide(static_cast<std::size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argument.data(), static_cast<int>(argument.size()), wide.data(), size);
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
                const DriverExecutor& executor)
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
        return result.failure(error, "program exited with code " + std::to_string(processExit), ExitCode::CompilationError);
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
        if (target.parent_path() != root) return result.failure(error, "clean safety validation failed", ExitCode::InternalError);
        removed += fs::remove_all(target, fileError);
        if (fileError) return result.failure(error, "cannot clean '" + target.string() + "': " + fileError.message(), ExitCode::InternalError);
    }
    for (fs::directory_iterator iterator(root, fileError), end; iterator != end && !fileError; iterator.increment(fileError))
        if (iterator->is_regular_file() && iterator->path().filename().string().starts_with(".nspm.") &&
            iterator->path().extension() == ".nsproj") { fs::remove(iterator->path(), fileError); ++removed; }
    if (fileError) return result.failure(error, "cannot clean generated project files: " + fileError.message(), ExitCode::InternalError);
    return result.success(output, "Cleaned " + std::to_string(removed) + " filesystem entr" + (removed == 1U ? "y" : "ies"),
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
        if (command == "add" && argument == "--version" && index + 1 < parsed.values.size()) version = parsed.values[++index];
        else if (command == "add" && argument == "--source" && index + 1 < parsed.values.size()) source = parsed.values[++index];
        else if (argument.starts_with('-')) return result.failure(error, "unknown package option: '" + std::string(argument) + "'");
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
            output << package.id << ' ' << package.version << (package.source.empty() ? "" : " " + package.source) << '\n';
        return ExitCode::Success;
    }
    const auto matches = [&](const PackageRequest& package) {
        std::string left = package.id;
        std::string right = id;
        std::transform(left.begin(), left.end(), left.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(right.begin(), right.end(), right.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return left == right;
    };
    auto found = std::find_if(sidecar.packages.begin(), sidecar.packages.end(), matches);
    if (command == "add")
    {
        if (!safeComponent(id) || !safeComponent(version)) return result.failure(error, "unsafe package id or version");
        if (found == sidecar.packages.end()) sidecar.packages.push_back(PackageRequest{id, version, source});
        else *found = PackageRequest{id, version, source};
        std::sort(sidecar.packages.begin(), sidecar.packages.end(), [](const auto& left, const auto& right) { return left.id < right.id; });
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
} // namespace

bool isProjectCommand(std::string_view command) noexcept
{
    return command == "new" || command == "restore" || command == "build" || command == "run" ||
           command == "clean" || command == "add" || command == "remove" || command == "list";
}

ExitCode runProjectCommand(std::span<const std::string_view> arguments, std::ostream& output,
                           std::ostream& error, const DriverExecutor& executor)
{
    if (arguments.empty()) return ExitCode::CommandLineError;
    if (arguments.front() == "new") return runNew(arguments, output, error);
    if (arguments.front() == "restore") return runRestore(arguments, output, error);
    if (arguments.front() == "build") return runBuild(arguments, output, error, executor);
    if (arguments.front() == "run") return runRun(arguments, output, error, executor);
    if (arguments.front() == "clean") return runClean(arguments, output, error);
    return runPackage(arguments, output, error);
}
} // namespace nativesharp::driver
