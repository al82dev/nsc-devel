#include "nativesharp/Compilation/ProjectLoader.h"

#include "nativesharp/Compilation/GlobPattern.h"
#include "nativesharp/Compilation/SimpleXmlParser.h"
#include "nativesharp/Compilation/SdkManifest.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace nativesharp::compilation
{
namespace
{
struct SourceIncludeRule final
{
    GlobPattern include;
    std::vector<GlobPattern> excludes;
};

struct LibraryReferenceDescriptor final
{
    std::string include;
    std::optional<std::string> metadata;
    std::optional<std::string> runtime;
};

struct ProjectDescriptor final
{
    std::optional<std::string> name;
    std::optional<ProjectOutputKind> outputKind;
    std::vector<SourceIncludeRule> includes;
    std::vector<GlobPattern> removes;
    std::vector<std::string> projectReferences;
    std::vector<LibraryReferenceDescriptor> libraryReferences;
    std::vector<std::string> sdkReferences;
};

std::string trim(std::string_view value)
{
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
    {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

const std::string* attribute(const SimpleXmlElement& element, std::string_view name)
{
    const auto found = std::find_if(
        element.attributes.begin(),
        element.attributes.end(),
        [name](const auto& item)
        {
            return item.first == name;
        });
    return found == element.attributes.end() ? nullptr : &found->second;
}

bool hasOnlyAttributes(
    const SimpleXmlElement& element,
    std::initializer_list<std::string_view> allowed,
    std::string& error)
{
    for (const auto& [name, value] : element.attributes)
    {
        static_cast<void>(value);
        if (std::find(allowed.begin(), allowed.end(), name) == allowed.end())
        {
            error = "element '" + element.name + "' has unsupported attribute '" + name + "'";
            return false;
        }
    }
    return true;
}

bool isIdentifier(std::string_view value)
{
    if (value.empty())
    {
        return false;
    }
    const auto isStart = [](unsigned char character)
    {
        return std::isalpha(character) != 0 || character == '_';
    };
    if (!isStart(static_cast<unsigned char>(value.front())))
    {
        return false;
    }
    for (const char character : value.substr(1))
    {
        const unsigned char current = static_cast<unsigned char>(character);
        if (!isStart(current) && std::isdigit(current) == 0)
        {
            return false;
        }
    }
    return true;
}

bool equalsFolded(std::string_view left, std::string_view right)
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        const unsigned char leftValue = static_cast<unsigned char>(left[index]);
        const unsigned char rightValue = static_cast<unsigned char>(right[index]);
        if (std::tolower(leftValue) != std::tolower(rightValue))
        {
            return false;
        }
    }
    return true;
}

std::string pathKey(const std::filesystem::path& path)
{
    std::string key = path.generic_string();
    std::transform(
        key.begin(),
        key.end(),
        key.begin(),
        [](unsigned char value)
        {
            return static_cast<char>(std::tolower(value));
        });
    return key;
}

bool parsePatterns(std::string_view text, std::vector<GlobPattern>& patterns, std::string& error)
{
    std::size_t begin = 0;
    while (begin <= text.size())
    {
        const std::size_t end = text.find(';', begin);
        const std::size_t count = end == std::string_view::npos ? text.size() - begin : end - begin;
        const std::string item = trim(text.substr(begin, count));
        if (item.empty())
        {
            error = "glob list contains an empty pattern";
            return false;
        }
        GlobPattern pattern(item);
        if (!pattern.isValid())
        {
            error = "invalid glob pattern '" + item + "': " + pattern.validationError();
            return false;
        }
        patterns.push_back(std::move(pattern));
        if (end == std::string_view::npos)
        {
            break;
        }
        begin = end + 1;
    }
    return true;
}

bool validateEmptyBody(const SimpleXmlElement& element, std::string& error)
{
    if (!element.children.empty() || !trim(element.text).empty())
    {
        error = "element '" + element.name + "' must be empty";
        return false;
    }
    return true;
}

bool parsePropertyGroup(
    const SimpleXmlElement& group,
    ProjectDescriptor& descriptor,
    std::string& error)
{
    if (!group.attributes.empty() || !trim(group.text).empty())
    {
        error = "PropertyGroup does not accept attributes or text";
        return false;
    }
    for (const SimpleXmlElement& property : group.children)
    {
        if (!property.attributes.empty() || !property.children.empty())
        {
            error = "project property '" + property.name + "' must contain only text";
            return false;
        }
        const std::string value = trim(property.text);
        if (value.empty())
        {
            error = "project property '" + property.name + "' cannot be empty";
            return false;
        }
        if (property.name == "Name")
        {
            if (descriptor.name.has_value())
            {
                error = "project property 'Name' is declared more than once";
                return false;
            }
            descriptor.name = value;
        }
        else if (property.name == "OutputType")
        {
            if (descriptor.outputKind.has_value())
            {
                error = "project property 'OutputType' is declared more than once";
                return false;
            }
            if (equalsFolded(value, "Exe"))
            {
                descriptor.outputKind = ProjectOutputKind::Executable;
            }
            else if (equalsFolded(value, "Library") || equalsFolded(value, "StaticLibrary"))
            {
                descriptor.outputKind = ProjectOutputKind::Library;
            }
            else if (equalsFolded(value, "SharedLibrary"))
            {
                descriptor.outputKind = ProjectOutputKind::SharedLibrary;
            }
            else
            {
                error = "OutputType must be 'Exe', 'Library', 'StaticLibrary', or 'SharedLibrary'";
                return false;
            }
        }
        else
        {
            error = "unsupported project property '" + property.name + "'";
            return false;
        }
    }
    return true;
}

bool parseCompileItem(
    const SimpleXmlElement& item,
    ProjectDescriptor& descriptor,
    std::string& error)
{
    if (!validateEmptyBody(item, error) ||
        !hasOnlyAttributes(item, {"Include", "Exclude", "Remove"}, error))
    {
        return false;
    }
    const std::string* include = attribute(item, "Include");
    const std::string* exclude = attribute(item, "Exclude");
    const std::string* remove = attribute(item, "Remove");
    if ((include == nullptr) == (remove == nullptr))
    {
        error = "Compile requires exactly one of Include or Remove";
        return false;
    }
    if (remove != nullptr)
    {
        if (exclude != nullptr)
        {
            error = "Compile Remove cannot be combined with Exclude";
            return false;
        }
        return parsePatterns(*remove, descriptor.removes, error);
    }

    std::vector<GlobPattern> includes;
    if (!parsePatterns(*include, includes, error))
    {
        return false;
    }
    std::vector<GlobPattern> excludes;
    if (exclude != nullptr && !parsePatterns(*exclude, excludes, error))
    {
        return false;
    }
    for (GlobPattern& pattern : includes)
    {
        descriptor.includes.push_back(SourceIncludeRule{std::move(pattern), excludes});
    }
    return true;
}

bool parseItemGroup(
    const SimpleXmlElement& group,
    ProjectDescriptor& descriptor,
    std::string& error)
{
    if (!group.attributes.empty() || !trim(group.text).empty())
    {
        error = "ItemGroup does not accept attributes or text";
        return false;
    }
    for (const SimpleXmlElement& item : group.children)
    {
        if (item.name == "Compile")
        {
            if (!parseCompileItem(item, descriptor, error))
            {
                return false;
            }
        }
        else if (item.name == "ProjectReference")
        {
            if (!validateEmptyBody(item, error) || !hasOnlyAttributes(item, {"Include"}, error))
            {
                return false;
            }
            const std::string* include = attribute(item, "Include");
            if (include == nullptr || trim(*include).empty())
            {
                error = "ProjectReference requires a non-empty Include attribute";
                return false;
            }
            if (include->find(';') != std::string::npos)
            {
                error = "ProjectReference Include accepts one project path per item";
                return false;
            }
            descriptor.projectReferences.push_back(trim(*include));
        }
        else if (item.name == "LibraryReference")
        {
            if (!validateEmptyBody(item, error) ||
                !hasOnlyAttributes(item, {"Include", "Metadata", "Runtime"}, error))
            {
                return false;
            }
            const std::string* include = attribute(item, "Include");
            const std::string* metadata = attribute(item, "Metadata");
            const std::string* runtime = attribute(item, "Runtime");
            if (include == nullptr || trim(*include).empty())
            {
                error = "LibraryReference requires a non-empty Include attribute";
                return false;
            }
            if (include->find(';') != std::string::npos)
            {
                error = "LibraryReference Include accepts one library path per item";
                return false;
            }
            LibraryReferenceDescriptor reference;
            reference.include = trim(*include);
            if (metadata != nullptr)
            {
                const std::string value = trim(*metadata);
                if (value.empty())
                {
                    error = "LibraryReference Metadata cannot be empty";
                    return false;
                }
                reference.metadata = value;
            }
            if (runtime != nullptr)
            {
                const std::string value = trim(*runtime);
                if (value.empty())
                {
                    error = "LibraryReference Runtime cannot be empty";
                    return false;
                }
                reference.runtime = value;
            }
            descriptor.libraryReferences.push_back(std::move(reference));
        }
        else if (item.name == "SdkReference")
        {
            if (!validateEmptyBody(item, error) || !hasOnlyAttributes(item, {"Include"}, error))
            {
                return false;
            }
            const std::string* include = attribute(item, "Include");
            if (include == nullptr || trim(*include).empty())
            {
                error = "SdkReference requires a non-empty Include attribute";
                return false;
            }
            if (include->find(';') != std::string::npos)
            {
                error = "SdkReference Include accepts one SDK manifest path per item";
                return false;
            }
            descriptor.sdkReferences.push_back(trim(*include));
        }
        else
        {
            error = "unsupported project item '" + item.name + "'";
            return false;
        }
    }
    return true;
}

bool parseDescriptor(const SimpleXmlElement& root, ProjectDescriptor& descriptor, std::string& error)
{
    if (root.name != "Project")
    {
        error = "project root element must be 'Project'";
        return false;
    }
    if (!root.attributes.empty() || !trim(root.text).empty())
    {
        error = "Project does not accept attributes or text";
        return false;
    }
    for (const SimpleXmlElement& child : root.children)
    {
        if (child.name == "PropertyGroup")
        {
            if (!parsePropertyGroup(child, descriptor, error))
            {
                return false;
            }
        }
        else if (child.name == "ItemGroup")
        {
            if (!parseItemGroup(child, descriptor, error))
            {
                return false;
            }
        }
        else
        {
            error = "unsupported Project child element '" + child.name + "'";
            return false;
        }
    }
    return true;
}

bool isReservedDirectory(const std::filesystem::path& path)
{
    const std::string name = path.filename().string();
    return equalsFolded(name, "bin") || equalsFolded(name, "obj") ||
        equalsFolded(name, ".git") || equalsFolded(name, ".nsc");
}

bool matchesAny(const std::vector<GlobPattern>& patterns, const std::filesystem::path& path)
{
    return std::any_of(
        patterns.begin(),
        patterns.end(),
        [&path](const GlobPattern& pattern)
        {
            return pattern.matches(path);
        });
}
}

std::optional<Project> ProjectLoader::load(
    const std::filesystem::path& projectFile,
    std::string& error) const
{
    error.clear();
    std::error_code filesystemError;
    const std::filesystem::path canonicalProject =
        std::filesystem::weakly_canonical(projectFile, filesystemError);
    if (filesystemError || !std::filesystem::is_regular_file(canonicalProject))
    {
        error = "project file does not exist or is not a regular file: '" + projectFile.string() + "'";
        return std::nullopt;
    }
    if (canonicalProject.extension() != ".nsproj")
    {
        error = "NativeSharp project file must use the canonical .nsproj extension";
        return std::nullopt;
    }

    std::ifstream stream(canonicalProject, std::ios::binary);
    if (!stream)
    {
        error = "cannot open project file '" + canonicalProject.string() + "'";
        return std::nullopt;
    }
    constexpr std::uintmax_t kMaximumProjectBytes = 4U * 1024U * 1024U;
    const std::uintmax_t projectBytes = std::filesystem::file_size(canonicalProject, filesystemError);
    if (filesystemError || projectBytes > kMaximumProjectBytes)
    {
        error = filesystemError
            ? "cannot determine project file size for '" + canonicalProject.string() + "'"
            : "project file exceeds the 4 MiB safety limit";
        return std::nullopt;
    }
    std::string source(static_cast<std::size_t>(projectBytes), '\0');
    stream.read(source.data(), static_cast<std::streamsize>(source.size()));
    if ((!stream && !source.empty()) || stream.gcount() != static_cast<std::streamsize>(source.size()) ||
        stream.peek() != std::char_traits<char>::eof())
    {
        error = "cannot read project file '" + canonicalProject.string() + "'";
        return std::nullopt;
    }

    SimpleXmlElement root;
    SimpleXmlParser parser;
    if (!parser.parse(source, root, error))
    {
        error = canonicalProject.string() + ": " + error;
        return std::nullopt;
    }

    ProjectDescriptor descriptor;
    if (!parseDescriptor(root, descriptor, error))
    {
        error = canonicalProject.string() + ": " + error;
        return std::nullopt;
    }

    const std::string moduleName = descriptor.name.value_or(canonicalProject.stem().string());
    if (!isIdentifier(moduleName))
    {
        error = "project Name '" + moduleName + "' is not a valid NativeSharp identifier";
        return std::nullopt;
    }
    if (descriptor.includes.empty())
    {
        descriptor.includes.push_back(SourceIncludeRule{GlobPattern("**/*.ns"), {}});
    }

    const std::filesystem::path projectDirectory = canonicalProject.parent_path();
    ProjectSourceSet sourceSet(projectDirectory);
    std::vector<std::filesystem::path> candidates;
    std::vector<std::string> candidateKeys;

    std::filesystem::recursive_directory_iterator iterator(
        projectDirectory,
        std::filesystem::directory_options::skip_permission_denied,
        filesystemError);
    const std::filesystem::recursive_directory_iterator end;
    while (!filesystemError && iterator != end)
    {
        const std::filesystem::directory_entry entry = *iterator;
        if (entry.is_directory(filesystemError) && isReservedDirectory(entry.path()))
        {
            iterator.disable_recursion_pending();
        }
        else if (!filesystemError && entry.is_regular_file(filesystemError) && entry.path().extension() == ".ns")
        {
            const std::filesystem::path relative =
                std::filesystem::relative(entry.path(), projectDirectory, filesystemError).lexically_normal();
            if (!filesystemError)
            {
                bool included = false;
                for (const SourceIncludeRule& rule : descriptor.includes)
                {
                    if (rule.include.matches(relative) && !matchesAny(rule.excludes, relative))
                    {
                        included = true;
                        break;
                    }
                }
                if (included && !matchesAny(descriptor.removes, relative))
                {
                    const std::string key = pathKey(relative);
                    if (std::find(candidateKeys.begin(), candidateKeys.end(), key) == candidateKeys.end())
                    {
                        candidates.push_back(entry.path());
                        candidateKeys.push_back(key);
                    }
                }
            }
        }
        iterator.increment(filesystemError);
    }
    if (filesystemError)
    {
        error = "cannot enumerate project sources: " + filesystemError.message();
        return std::nullopt;
    }

    for (const std::filesystem::path& sourcePath : candidates)
    {
        if (!sourceSet.add(sourcePath, error))
        {
            return std::nullopt;
        }
    }
    if (!sourceSet.finalize(error, !descriptor.projectReferences.empty()))
    {
        return std::nullopt;
    }

    std::vector<std::filesystem::path> projectReferences;
    std::vector<std::string> referenceKeys;
    for (const std::string& referenceText : descriptor.projectReferences)
    {
        const std::filesystem::path unresolved = projectDirectory / referenceText;
        const std::filesystem::path reference =
            std::filesystem::weakly_canonical(unresolved, filesystemError);
        if (filesystemError || !std::filesystem::is_regular_file(reference))
        {
            error = "referenced project does not exist: '" + unresolved.string() + "'";
            return std::nullopt;
        }
        if (reference.extension() != ".nsproj")
        {
            error = "ProjectReference must refer to a .nsproj file: '" + reference.string() + "'";
            return std::nullopt;
        }
        const std::string key = pathKey(reference);
        if (std::find(referenceKeys.begin(), referenceKeys.end(), key) != referenceKeys.end())
        {
            error = "duplicate ProjectReference: '" + reference.string() + "'";
            return std::nullopt;
        }
        referenceKeys.push_back(key);
        projectReferences.push_back(reference);
    }
    std::sort(
        projectReferences.begin(),
        projectReferences.end(),
        [](const std::filesystem::path& left, const std::filesystem::path& right)
        {
            return pathKey(left) < pathKey(right);
        });

    std::vector<LibraryReference> libraryReferences;
    std::vector<std::string> libraryReferenceKeys;
    for (const LibraryReferenceDescriptor& descriptorReference : descriptor.libraryReferences)
    {
        const std::filesystem::path unresolvedLibrary = projectDirectory / descriptorReference.include;
        filesystemError.clear();
        const std::filesystem::path library =
            std::filesystem::weakly_canonical(unresolvedLibrary, filesystemError);
        if (filesystemError || !std::filesystem::is_regular_file(library))
        {
            error = "referenced library does not exist: '" + unresolvedLibrary.string() + "'";
            return std::nullopt;
        }
        if (!equalsFolded(library.extension().string(), ".lib"))
        {
            error = "LibraryReference Include must refer to a .lib file: '" + library.string() + "'";
            return std::nullopt;
        }

        std::filesystem::path unresolvedMetadata;
        if (descriptorReference.metadata.has_value())
        {
            unresolvedMetadata = projectDirectory / *descriptorReference.metadata;
        }
        else
        {
            unresolvedMetadata = unresolvedLibrary;
            unresolvedMetadata.replace_extension(".nsmeta");
        }
        filesystemError.clear();
        const std::filesystem::path metadata =
            std::filesystem::weakly_canonical(unresolvedMetadata, filesystemError);
        if (filesystemError || !std::filesystem::is_regular_file(metadata))
        {
            error = "LibraryReference metadata does not exist: '" + unresolvedMetadata.string() + "'";
            return std::nullopt;
        }
        if (!equalsFolded(metadata.extension().string(), ".nsmeta"))
        {
            error = "LibraryReference Metadata must refer to a .nsmeta file: '" + metadata.string() + "'";
            return std::nullopt;
        }

        std::filesystem::path runtime;
        if (descriptorReference.runtime.has_value())
        {
            const std::filesystem::path unresolvedRuntime = projectDirectory / *descriptorReference.runtime;
            filesystemError.clear();
            runtime = std::filesystem::weakly_canonical(unresolvedRuntime, filesystemError);
            if (filesystemError || !std::filesystem::is_regular_file(runtime))
            {
                error = "LibraryReference runtime DLL does not exist: '" + unresolvedRuntime.string() + "'";
                return std::nullopt;
            }
            if (!equalsFolded(runtime.extension().string(), ".dll"))
            {
                error = "LibraryReference Runtime must refer to a .dll file: '" + runtime.string() + "'";
                return std::nullopt;
            }
        }

        const std::string key = pathKey(library);
        if (std::find(libraryReferenceKeys.begin(), libraryReferenceKeys.end(), key) !=
            libraryReferenceKeys.end())
        {
            error = "duplicate LibraryReference: '" + library.string() + "'";
            return std::nullopt;
        }
        libraryReferenceKeys.push_back(key);
        libraryReferences.push_back(LibraryReference{library, metadata, runtime});
    }
    std::sort(
        libraryReferences.begin(),
        libraryReferences.end(),
        [](const LibraryReference& left, const LibraryReference& right)
        {
            return pathKey(left.libraryPath) < pathKey(right.libraryPath);
        });

    std::vector<SdkManifest> sdkReferences;
    std::vector<std::string> sdkReferenceKeys;
    SdkManifestLoader sdkLoader;
    for (const std::string& reference : descriptor.sdkReferences)
    {
        const std::filesystem::path unresolved = projectDirectory / reference;
        std::string sdkError;
        std::optional<SdkManifest> manifest = sdkLoader.load(unresolved, sdkError);
        if (!manifest.has_value())
        {
            error = sdkError;
            return std::nullopt;
        }
        for (const SdkLibraryDependency& dependency : manifest->libraries)
        {
            const std::filesystem::path libraryPath =
                manifest->manifestPath.parent_path() / dependency.relativePath;
            std::error_code dependencyError;
            if (!std::filesystem::is_regular_file(libraryPath, dependencyError))
            {
                error = "NSC-SDK-004: SDK '" + manifest->name +
                    "' required library '" + dependency.name + "' is missing: '" +
                    libraryPath.string() + "'";
                return std::nullopt;
            }
        }
        const std::string key = pathKey(manifest->manifestPath);
        if (std::find(sdkReferenceKeys.begin(), sdkReferenceKeys.end(), key) != sdkReferenceKeys.end())
        {
            error = "duplicate SdkReference: '" + manifest->manifestPath.string() + "'";
            return std::nullopt;
        }
        sdkReferenceKeys.push_back(key);
        sdkReferences.push_back(std::move(*manifest));
    }
    std::sort(
        sdkReferences.begin(),
        sdkReferences.end(),
        [](const SdkManifest& left, const SdkManifest& right)
        {
            return pathKey(left.manifestPath) < pathKey(right.manifestPath);
        });

    return Project(
        canonicalProject,
        moduleName,
        descriptor.outputKind.value_or(ProjectOutputKind::Executable),
        std::move(sourceSet),
        std::move(projectReferences),
        std::move(libraryReferences),
        std::move(sdkReferences));
}
}
