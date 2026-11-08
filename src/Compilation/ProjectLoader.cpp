#include "nativesharp/Compilation/ProjectLoader.h"

#include "Internal/LibraryReferenceDescriptor.h"
#include "Internal/ProjectDescriptor.h"
#include "Internal/ProjectLoaderCache.h"
#include "Internal/SourceIncludeRule.h"

#include "nativesharp/Compilation/GlobPattern.h"
#include "nativesharp/Compilation/ImplicitProjectRegistry.h"
#include "nativesharp/Compilation/PrecompiledProductSidecarPath.h"
#include "nativesharp/Compilation/SimpleXmlParser.h"
#include "nativesharp/Compilation/SdkManifest.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::compilation
{
namespace
{

bool parsePositiveDecimal(std::string_view value, std::size_t maximum, std::size_t& result)
{
    if (value.empty())
    {
        return false;
    }
    std::size_t parsed = 0;
    for (const char character : value)
    {
        if (character < '0' || character > '9')
        {
            return false;
        }
        const std::size_t digit = static_cast<std::size_t>(character - '0');
        if (parsed > (maximum - digit) / 10U)
        {
            return false;
        }
        parsed = parsed * 10U + digit;
    }
    if (parsed == 0 || parsed > maximum)
    {
        return false;
    }
    result = parsed;
    return true;
}

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

std::string displayPathUtf8(const std::filesystem::path& path)
{
    const auto encoded = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}

std::string pathKey(const std::filesystem::path& path)
{
    std::string key = displayPathUtf8(path);
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

std::optional<bool> parseIdentityProperty(
    const SimpleXmlElement& property,
    const std::string& value,
    ProjectDescriptor& descriptor,
    std::string& error)
{
    if (property.name == "Name")
    {
        if (descriptor.name.has_value())
        {
            error = "project property 'Name' is declared more than once";
            return false;
        }
        descriptor.name = value;
        return true;
    }
    if (property.name == "Package")
    {
        if (descriptor.packageName.has_value())
        {
            error = "project property 'Package' is declared more than once";
            return false;
        }
        if (!isIdentifier(value))
        {
            error = "Package must be a NativeSharp identifier";
            return false;
        }
        descriptor.packageName = value;
        return true;
    }
    if (property.name == "OutputType")
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
        return true;
    }
    return std::nullopt;
}

std::optional<bool> parseExecutionProperty(
    const SimpleXmlElement& property,
    const std::string& value,
    ProjectDescriptor& descriptor,
    std::string& error)
{
    if (property.name == "CppConsumerVolatileMode")
    {
        if (descriptor.cppConsumerVolatileMode.has_value())
        {
            error = "project property 'CppConsumerVolatileMode' is declared more than once";
            return false;
        }
        if (value == "Microsoft") descriptor.cppConsumerVolatileMode = CppConsumerVolatileMode::Microsoft;
        else if (value == "ISO") descriptor.cppConsumerVolatileMode = CppConsumerVolatileMode::Iso;
        else
        {
            error = "CppConsumerVolatileMode must be 'Microsoft' or 'ISO'";
            return false;
        }
        return true;
    }
    if (property.name == "OptimizationLevel")
    {
        if (descriptor.buildSettings.optimizationLevel.has_value())
        {
            error = "project property 'OptimizationLevel' is declared more than once";
            return false;
        }
        if (value.size() != 2U || (value[0] != 'O' && value[0] != 'o') ||
            value[1] < '0' || value[1] > '3')
        {
            error = "OptimizationLevel must be 'O0', 'O1', 'O2', or 'O3'";
            return false;
        }
        descriptor.buildSettings.optimizationLevel = static_cast<unsigned>(value[1] - '0');
        return true;
    }
    if (property.name == "Language")
    {
        // Compatibility-only input. Historical project files remain readable, but this
        // value cannot select, hide, or reject syntax in the cumulative language.
        (void)value;
        return true;
    }
    if (property.name == "PanicProfile")
    {
        if (descriptor.panicProfile.has_value())
        {
            error = "project property 'PanicProfile' is declared more than once";
            return false;
        }
        descriptor.panicProfile = language::parsePanicProfile(value);
        if (!descriptor.panicProfile.has_value())
        {
            error = "PanicProfile must be 'abort' or 'unwind-native'";
            return false;
        }
        return true;
    }
    if (property.name == "PanicHandler")
    {
        if (descriptor.panicHandler.has_value())
        {
            error = "project property 'PanicHandler' is declared more than once";
            return false;
        }
        if (value.empty() || value.size() > 255U ||
            std::ranges::any_of(value, [](unsigned char ch) {
                return ch <= 0x20U || ch == 0x7fU;
            }))
        {
            error = "PanicHandler must be a non-empty qualified function name without whitespace";
            return false;
        }
        descriptor.panicHandler = value;
        return true;
    }
    return std::nullopt;
}

std::optional<bool> parsePathProperty(
    const SimpleXmlElement& property,
    const std::string& value,
    ProjectDescriptor& descriptor,
    std::string& error)
{
    if (property.name == "OutputPath")
    {
        if (descriptor.buildSettings.outputPath.has_value())
        {
            error = "project property 'OutputPath' is declared more than once";
            return false;
        }
        descriptor.buildSettings.outputPath = std::filesystem::path(value);
        return true;
    }
    if (property.name == "LibraryReferenceRoot")
    {
        if (descriptor.buildSettings.libraryReferenceRoot.has_value())
        {
            error = "project property 'LibraryReferenceRoot' is declared more than once";
            return false;
        }
        descriptor.buildSettings.libraryReferenceRoot = std::filesystem::path(value);
        return true;
    }
    if (property.name == "CompilerJobs")
    {
        if (descriptor.buildSettings.compilerJobs.has_value())
        {
            error = "project property 'CompilerJobs' is declared more than once";
            return false;
        }
        std::size_t jobs = 0;
        if (!parsePositiveDecimal(value, 1024U, jobs))
        {
            error = "CompilerJobs must be an integer from 1 through 1024";
            return false;
        }
        descriptor.buildSettings.compilerJobs = jobs;
        return true;
    }
    return std::nullopt;
}

std::optional<bool> parseGenericLimitProperty(
    const SimpleXmlElement& property,
    const std::string& value,
    ProjectDescriptor& descriptor,
    std::string& error)
{
    if (property.name == "GenericInstantiationDepth")
    {
        if (descriptor.buildSettings.genericInstantiationDepth.has_value())
        {
            error = "project property 'GenericInstantiationDepth' is declared more than once";
            return false;
        }
        std::size_t depth = 0;
        if (!parsePositiveDecimal(value, MaximumGenericInstantiationDepth, depth))
        {
            error = "GenericInstantiationDepth must be an integer from 1 through " +
                std::to_string(MaximumGenericInstantiationDepth);
            return false;
        }
        descriptor.buildSettings.genericInstantiationDepth = depth;
        return true;
    }
    if (property.name == "GenericInstantiationLimit")
    {
        if (descriptor.buildSettings.genericInstantiationCount.has_value())
        {
            error = "project property 'GenericInstantiationLimit' is declared more than once";
            return false;
        }
        std::size_t count = 0;
        if (!parsePositiveDecimal(value, MaximumGenericInstantiationCount, count))
        {
            error = "GenericInstantiationLimit must be an integer from 1 through " +
                std::to_string(MaximumGenericInstantiationCount);
            return false;
        }
        descriptor.buildSettings.genericInstantiationCount = count;
        return true;
    }
    return std::nullopt;
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
        const std::optional<bool> identity =
            parseIdentityProperty(property, value, descriptor, error);
        if (identity.has_value())
        {
            if (!*identity) return false;
            continue;
        }
        const std::optional<bool> execution =
            parseExecutionProperty(property, value, descriptor, error);
        if (execution.has_value())
        {
            if (!*execution) return false;
            continue;
        }
        const std::optional<bool> path = parsePathProperty(property, value, descriptor, error);
        if (path.has_value())
        {
            if (!*path) return false;
            continue;
        }
        const std::optional<bool> generic =
            parseGenericLimitProperty(property, value, descriptor, error);
        if (generic.has_value())
        {
            if (!*generic) return false;
            continue;
        }
        error = "unsupported project property '" + property.name + "'";
        return false;
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

bool parseProjectReferenceItem(
    const SimpleXmlElement& item,
    ProjectDescriptor& descriptor,
    std::string& error)
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
    return true;
}

bool parseLibraryReferenceItem(
    const SimpleXmlElement& item,
    ProjectDescriptor& descriptor,
    std::string& error)
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
    return true;
}

bool parseCppLibraryReferenceItem(
    const SimpleXmlElement& item, ProjectDescriptor& descriptor, std::string& error)
{
    if (!hasOnlyAttributes(item, {"Header", "Language", "Standard", "Library", "Runtime", "VolatileMode"}, error))
        return false;
    if (!trim(item.text).empty())
    {
        error = "CppLibraryReference does not accept text";
        return false;
    }
    const auto* header = attribute(item, "Header");
    const auto* language = attribute(item, "Language");
    const auto* standard = attribute(item, "Standard");
    if (!header || trim(*header).empty() || !language || !standard || trim(*standard).empty())
    {
        error = "CppLibraryReference requires non-empty Header, Language and Standard attributes";
        return false;
    }
    CppLibraryReferenceDescriptor reference;
    reference.header = trim(*header);
    reference.languageStandard = trim(*standard);
    if (const auto* mode = attribute(item, "VolatileMode"))
    {
        if (trim(*mode) == "Default") reference.volatileMode = interop::CppVolatileMode::Default;
        else if (trim(*mode) == "Microsoft") reference.volatileMode = interop::CppVolatileMode::Microsoft;
        else if (trim(*mode) == "ISO") reference.volatileMode = interop::CppVolatileMode::Iso;
        else { error = "CppLibraryReference VolatileMode must be Default, Microsoft or ISO"; return false; }
    }
    if (trim(*language) == "C") reference.language = interop::CppLanguageMode::C;
    else if (trim(*language) == "C++") reference.language = interop::CppLanguageMode::Cpp;
    else
    {
        error = "CppLibraryReference Language must be C or C++";
        return false;
    }
    if (const auto* library = attribute(item, "Library"))
    {
        if (trim(*library).empty()) { error = "CppLibraryReference Library cannot be empty"; return false; }
        reference.library = trim(*library);
    }
    if (const auto* runtime = attribute(item, "Runtime"))
    {
        if (trim(*runtime).empty() || !reference.library)
        {
            error = "CppLibraryReference Runtime requires a non-empty Library and Runtime";
            return false;
        }
        reference.runtime = trim(*runtime);
    }
    for (const auto& child : item.children)
    {
        if (!validateEmptyBody(child, error)) return false;
        if (child.name == "IncludeDirectory")
        {
            if (!hasOnlyAttributes(child, {"Path", "Kind"}, error)) return false;
            const auto* path = attribute(child, "Path");
            const auto* kind = attribute(child, "Kind");
            if (!path || trim(*path).empty()) { error = "IncludeDirectory requires Path"; return false; }
            interop::CppIncludeKind includeKind = interop::CppIncludeKind::User;
            if (kind && trim(*kind) == "System") includeKind = interop::CppIncludeKind::System;
            else if (kind && trim(*kind) != "User")
            {
                error = "IncludeDirectory Kind must be User or System";
                return false;
            }
            reference.includePaths.push_back({std::filesystem::path(trim(*path)), includeKind});
        }
        else if (child.name == "Define" || child.name == "Undefine")
        {
            if (!hasOnlyAttributes(child, {"Value"}, error)) return false;
            const auto* value = attribute(child, "Value");
            if (!value || trim(*value).empty()) { error = child.name + " requires Value"; return false; }
            reference.macros.push_back({child.name == "Define" ? interop::CppMacroAction::Define :
                interop::CppMacroAction::Undefine, trim(*value)});
        }
        else
        {
            error = "unsupported CppLibraryReference child '" + child.name + "'";
            return false;
        }
    }
    descriptor.cppLibraryReferences.push_back(std::move(reference));
    return true;
}

bool parseSdkReferenceItem(
    const SimpleXmlElement& item,
    ProjectDescriptor& descriptor,
    std::string& error)
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
    return true;
}

bool parseFriendItem(
    const SimpleXmlElement& item,
    ProjectDescriptor& descriptor,
    std::string& error)
{
    if (!validateEmptyBody(item, error) || !hasOnlyAttributes(item, {"Include"}, error))
    {
        return false;
    }
    const std::string* include = attribute(item, "Include");
    const std::string module = include == nullptr ? std::string{} : trim(*include);
    if (!isIdentifier(module))
    {
        error = "Friend requires one NativeSharp module identifier in Include";
        return false;
    }
    const auto duplicate = std::ranges::find_if(
        descriptor.friendModules,
        [&](const std::string& existing) { return equalsFolded(existing, module); });
    if (duplicate != descriptor.friendModules.end())
    {
        error = "Friend module '" + module + "' is declared more than once";
        return false;
    }
    descriptor.friendModules.push_back(module);
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
            if (!parseProjectReferenceItem(item, descriptor, error)) return false;
        }
        else if (item.name == "LibraryReference")
        {
            if (!parseLibraryReferenceItem(item, descriptor, error)) return false;
        }
        else if (item.name == "CppLibraryReference")
        {
            if (!parseCppLibraryReferenceItem(item, descriptor, error)) return false;
        }
        else if (item.name == "SdkReference")
        {
            if (!parseSdkReferenceItem(item, descriptor, error)) return false;
        }
        else if (item.name == "Friend")
        {
            if (!parseFriendItem(item, descriptor, error)) return false;
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
    const auto encoded = path.filename().generic_u8string();
    const std::string name(reinterpret_cast<const char*>(encoded.data()), encoded.size());
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

bool loadDescriptorFile(
    const std::filesystem::path& projectFile,
    std::filesystem::path& canonicalProject,
    ProjectDescriptor& descriptor,
    ProjectLoaderCache& cache,
    std::string& error)
{
    error.clear();
    std::error_code filesystemError;
    canonicalProject = std::filesystem::weakly_canonical(projectFile, filesystemError);
    // EN: Freeze 3.0 C-05: the implicit project of a lone `.ns` has no file; its text was
    //     registered by the driver. Its descriptor is parsed each time (it is tiny).
    // ES: Congelamiento 3.0 C-05: el proyecto implicito de un `.ns` suelto no tiene archivo; el
    //     driver registro su texto. Su descriptor se analiza cada vez (es diminuto).
    if (!filesystemError)
    {
        const std::optional<std::string> implicitText = ImplicitProjectRegistry::find(canonicalProject);
        if (implicitText.has_value())
        {
            SimpleXmlElement implicitRoot;
            SimpleXmlParser implicitParser;
            if (implicitParser.parse(*implicitText, implicitRoot, error) &&
                parseDescriptor(implicitRoot, descriptor, error))
            {
                descriptor.implicitProject = true;
                return true;
            }
            error = canonicalProject.string() + ": " + error;
            return false;
        }
    }
    if (filesystemError || !std::filesystem::is_regular_file(canonicalProject))
    {
        error = "project file does not exist or is not a regular file: '" + projectFile.string() + "'";
        return false;
    }
    if (canonicalProject.extension() != ".nsproj")
    {
        error = "NativeSharp project file must use the canonical .nsproj extension";
        return false;
    }

    const std::string descriptorKey = pathKey(canonicalProject);
    {
        const std::lock_guard<std::mutex> lock(cache.mutex);
        const auto found = cache.descriptors.find(descriptorKey);
        if (found != cache.descriptors.end())
        {
            descriptor = found->second;
            ++cache.statistics.descriptorCacheHits;
            return true;
        }
    }

    std::ifstream stream(canonicalProject, std::ios::binary);
    if (!stream)
    {
        error = "cannot open project file '" + canonicalProject.string() + "'";
        return false;
    }
    constexpr std::uintmax_t kMaximumProjectBytes = 4U * 1024U * 1024U;
    const std::uintmax_t projectBytes = std::filesystem::file_size(canonicalProject, filesystemError);
    if (filesystemError || projectBytes > kMaximumProjectBytes)
    {
        error = filesystemError
            ? "cannot determine project file size for '" + canonicalProject.string() + "'"
            : "project file exceeds the 4 MiB safety limit";
        return false;
    }
    std::string source(static_cast<std::size_t>(projectBytes), '\0');
    stream.read(source.data(), static_cast<std::streamsize>(source.size()));
    if ((!stream && !source.empty()) || stream.gcount() != static_cast<std::streamsize>(source.size()) ||
        stream.peek() != std::char_traits<char>::eof())
    {
        error = "cannot read project file '" + canonicalProject.string() + "'";
        return false;
    }

    SimpleXmlElement root;
    SimpleXmlParser parser;
    if (!parser.parse(source, root, error) || !parseDescriptor(root, descriptor, error))
    {
        error = canonicalProject.string() + ": " + error;
        return false;
    }
    {
        const std::lock_guard<std::mutex> lock(cache.mutex);
        const auto [found, inserted] = cache.descriptors.emplace(descriptorKey, descriptor);
        ++cache.statistics.descriptorFileReads;
        if (!inserted)
        {
            descriptor = found->second;
            ++cache.statistics.descriptorCacheHits;
        }
    }
    return true;
}

std::filesystem::path resolveProjectPath(
    const std::filesystem::path& projectDirectory,
    const std::filesystem::path& value)
{
    return (value.is_absolute() ? value : projectDirectory / value).lexically_normal();
}

ProjectLoadOptions effectiveLoadOptions(const ProjectLoadOptions& options)
{
    ProjectLoadOptions effectiveOptions = options;
    if (effectiveOptions.libraryReferenceRoot.has_value() ||
        effectiveOptions.libraryReferenceFallbackRoot.has_value())
    {
        return effectiveOptions;
    }
    char* environmentRoot = nullptr;
    std::size_t environmentRootLength = 0;
    if (_dupenv_s(&environmentRoot, &environmentRootLength, "NS_LIB_ROOT") == 0 &&
        environmentRoot != nullptr && *environmentRoot != '\0')
    {
        const std::filesystem::path root{environmentRoot};
        if (root.is_absolute())
            effectiveOptions.libraryReferenceFallbackRoot = root.lexically_normal();
    }
    std::free(environmentRoot);
    return effectiveOptions;
}

bool prepareProjectDescriptor(
    const std::filesystem::path& canonicalProject,
    ProjectDescriptor& descriptor,
    std::string& moduleName,
    std::string& error)
{
    moduleName = descriptor.name ? *descriptor.name : displayPathUtf8(canonicalProject.stem());
    if (!isIdentifier(moduleName))
    {
        error = "project Name '" + moduleName + "' is not a valid NativeSharp identifier";
        return false;
    }
    if (descriptor.includes.empty())
    {
        descriptor.includes.push_back(SourceIncludeRule{GlobPattern("**/*.ns"), {}});
    }
    return true;
}

bool collectSourceCandidates(
    const std::filesystem::path& projectDirectory,
    const ProjectDescriptor& descriptor,
    std::vector<std::filesystem::path>& candidates,
    std::string& error)
{
    std::vector<std::string> candidateKeys;
    std::error_code filesystemError;
    if (descriptor.implicitProject)
    {
        for (const SourceIncludeRule& rule : descriptor.includes)
        {
            const auto& pattern = rule.include.pattern();
            const std::filesystem::path relative = std::filesystem::path(std::u8string(pattern.begin(), pattern.end())).lexically_normal();
            const std::filesystem::path source = projectDirectory / relative;
            if (source.extension() != ".ns" || !std::filesystem::is_regular_file(source, filesystemError))
            {
                error = "implicit project source does not exist: '" + source.string() + "'";
                return false;
            }
            const std::string key = pathKey(relative);
            if (std::find(candidateKeys.begin(), candidateKeys.end(), key) != candidateKeys.end()) continue;
            candidates.push_back(source);
            candidateKeys.push_back(key);
        }
        return true;
    }
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
        else if (!filesystemError && entry.is_regular_file(filesystemError) &&
            entry.path().extension() == ".ns")
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
    if (!filesystemError) return true;
    error = "cannot enumerate project sources: " + filesystemError.message();
    return false;
}

bool populateSourceSet(
    const std::vector<std::filesystem::path>& candidates,
    bool hasProjectReferences,
    ProjectSourceSet& sourceSet,
    std::string& error)
{
    for (const std::filesystem::path& sourcePath : candidates)
    {
        if (!sourceSet.add(sourcePath, error)) return false;
    }
    return sourceSet.finalize(error, hasProjectReferences);
}

std::optional<std::vector<std::filesystem::path>> loadProjectReferences(
    const std::filesystem::path& projectDirectory,
    const ProjectDescriptor& descriptor,
    std::string& error)
{
    std::vector<std::filesystem::path> projectReferences;
    std::vector<std::string> referenceKeys;
    std::error_code filesystemError;
    for (const std::string& referenceText : descriptor.projectReferences)
    {
        const std::filesystem::path unresolved = projectDirectory / referenceText;
        const std::filesystem::path reference =
            std::filesystem::weakly_canonical(unresolved, filesystemError);
        if (filesystemError || !std::filesystem::is_regular_file(reference))
        {
            error = "referenced project does not exist: '" + displayPathUtf8(unresolved) + "'";
            return std::nullopt;
        }
        if (reference.extension() != ".nsproj")
        {
            error = "ProjectReference must refer to a .nsproj file: '" + displayPathUtf8(reference) + "'";
            return std::nullopt;
        }
        const std::string key = pathKey(reference);
        if (std::find(referenceKeys.begin(), referenceKeys.end(), key) != referenceKeys.end())
        {
            error = "duplicate ProjectReference: '" + displayPathUtf8(reference) + "'";
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
    return projectReferences;
}

std::filesystem::path resolveLibraryReferencePath(
    const std::filesystem::path& projectDirectory,
    const ProjectDescriptor& descriptor,
    const ProjectLoadOptions& options,
    std::string_view text)
{
    const std::filesystem::path path{std::u8string(text.begin(), text.end())};
    const std::optional<std::filesystem::path>& explicitRoot =
        options.libraryReferenceRoot.has_value()
            ? options.libraryReferenceRoot
            : descriptor.buildSettings.libraryReferenceRoot;
    if (path.is_absolute()) return projectDirectory / path;
    if (explicitRoot.has_value())
    {
        return resolveProjectPath(projectDirectory, *explicitRoot) / path;
    }
    const std::filesystem::path local = projectDirectory / path;
    if (!options.libraryReferenceFallbackRoot.has_value()) return local;
    std::error_code existenceError;
    if (std::filesystem::is_regular_file(local, existenceError)) return local;
    return *options.libraryReferenceFallbackRoot / path;
}

bool loadLibraryArtifact(
    const std::filesystem::path& unresolvedLibrary,
    std::filesystem::path& library,
    std::string& error)
{
    std::error_code filesystemError;
    library = std::filesystem::weakly_canonical(unresolvedLibrary, filesystemError);
    if (filesystemError || !std::filesystem::is_regular_file(library))
    {
        error = "referenced library does not exist: '" + displayPathUtf8(unresolvedLibrary) + "'";
        return false;
    }
    if (equalsFolded(library.extension().string(), ".lib")) return true;
    error = "LibraryReference Include must refer to a .lib file: '" + displayPathUtf8(library) + "'";
    return false;
}

bool loadMetadataArtifact(
    const LibraryReferenceDescriptor& reference,
    const std::filesystem::path& unresolvedLibrary,
    const std::filesystem::path& projectDirectory,
    const ProjectDescriptor& descriptor,
    const ProjectLoadOptions& options,
    std::filesystem::path& metadata,
    std::string& error)
{
    std::filesystem::path unresolvedMetadata;
    if (reference.metadata.has_value())
        unresolvedMetadata = resolveLibraryReferencePath(
            projectDirectory, descriptor, options, *reference.metadata);
    else
    {
        unresolvedMetadata = unresolvedLibrary;
        unresolvedMetadata.replace_extension(".nsmeta");
    }
    std::error_code filesystemError;
    metadata = std::filesystem::weakly_canonical(unresolvedMetadata, filesystemError);
    if (filesystemError || !std::filesystem::is_regular_file(metadata))
    {
        error = "LibraryReference metadata does not exist: '" + displayPathUtf8(unresolvedMetadata) + "'";
        return false;
    }
    if (equalsFolded(metadata.extension().string(), ".nsmeta")) return true;
    error = "LibraryReference Metadata must refer to a .nsmeta file: '" + displayPathUtf8(metadata) + "'";
    return false;
}

bool loadRuntimeArtifact(
    const LibraryReferenceDescriptor& reference,
    const std::filesystem::path& projectDirectory,
    const ProjectDescriptor& descriptor,
    const ProjectLoadOptions& options,
    std::filesystem::path& runtime,
    std::string& error)
{
    if (!reference.runtime.has_value()) return true;
    const std::filesystem::path unresolvedRuntime = resolveLibraryReferencePath(
        projectDirectory, descriptor, options, *reference.runtime);
    std::error_code filesystemError;
    runtime = std::filesystem::weakly_canonical(unresolvedRuntime, filesystemError);
    if (filesystemError || !std::filesystem::is_regular_file(runtime))
    {
        error = "LibraryReference runtime DLL does not exist: '" + displayPathUtf8(unresolvedRuntime) + "'";
        return false;
    }
    if (equalsFolded(runtime.extension().string(), ".dll")) return true;
    error = "LibraryReference Runtime must refer to a .dll file: '" + displayPathUtf8(runtime) + "'";
    return false;
}

std::optional<std::vector<LibraryReference>> loadLibraryReferences(
    const std::filesystem::path& projectDirectory,
    const ProjectDescriptor& descriptor,
    const ProjectLoadOptions& options,
    std::string& error)
{
    std::vector<LibraryReference> libraryReferences;
    std::vector<std::string> libraryReferenceKeys;
    for (const LibraryReferenceDescriptor& reference : descriptor.libraryReferences)
    {
        const std::filesystem::path unresolvedLibrary = resolveLibraryReferencePath(
            projectDirectory, descriptor, options, reference.include);
        std::filesystem::path library;
        if (!loadLibraryArtifact(unresolvedLibrary, library, error)) return std::nullopt;
        std::filesystem::path metadata;
        if (!loadMetadataArtifact(
                reference, unresolvedLibrary, projectDirectory, descriptor, options, metadata, error))
            return std::nullopt;
        std::filesystem::path runtime;
        if (!loadRuntimeArtifact(
                reference, projectDirectory, descriptor, options, runtime, error))
            return std::nullopt;
        const std::filesystem::path productSidecar =
            availablePrecompiledProductSidecarPath(metadata);
        const std::string key = pathKey(library);
        if (std::find(libraryReferenceKeys.begin(), libraryReferenceKeys.end(), key) !=
            libraryReferenceKeys.end())
        {
            error = "duplicate LibraryReference: '" + displayPathUtf8(library) + "'";
            return std::nullopt;
        }
        libraryReferenceKeys.push_back(key);
        libraryReferences.push_back(LibraryReference{
            library, metadata, runtime, productSidecar});
    }
    std::sort(
        libraryReferences.begin(),
        libraryReferences.end(),
        [](const LibraryReference& left, const LibraryReference& right)
        {
            return pathKey(left.libraryPath) < pathKey(right.libraryPath);
        });
    return libraryReferences;
}

std::optional<std::vector<CppLibraryReference>> loadCppLibraryReferences(
    const std::filesystem::path& projectDirectory, const ProjectDescriptor& descriptor,
    const ProjectLoadOptions& options, std::string& error)
{
    std::vector<CppLibraryReference> result;
    for (const auto& input : descriptor.cppLibraryReferences)
    {
        CppLibraryReference reference;
        std::error_code ec;
        reference.headerPath = std::filesystem::weakly_canonical(projectDirectory / input.header, ec);
        if (ec || !std::filesystem::is_regular_file(reference.headerPath, ec) || ec)
        {
            error = "CppLibraryReference header does not exist: '" + input.header + "'";
            return std::nullopt;
        }
        if (input.library)
        {
            const auto library = resolveLibraryReferencePath(projectDirectory, descriptor, options, *input.library);
            if (!loadLibraryArtifact(library, reference.libraryPath, error)) return std::nullopt;
        }
        if (input.runtime)
        {
            const auto runtime = resolveLibraryReferencePath(projectDirectory, descriptor, options, *input.runtime);
            ec.clear();
            reference.runtimePath = std::filesystem::weakly_canonical(runtime, ec);
            if (ec || !std::filesystem::is_regular_file(reference.runtimePath, ec) || ec ||
                !equalsFolded(reference.runtimePath.extension().string(), ".dll"))
            {
                error = "CppLibraryReference Runtime must refer to an existing .dll file: '" + *input.runtime + "'";
                return std::nullopt;
            }
        }
        reference.language = input.language;
        reference.languageStandard = input.languageStandard;
        reference.volatileMode = input.volatileMode;
        reference.macros = input.macros;
        for (const auto& include : input.includePaths)
        {
            ec.clear();
            const auto path = std::filesystem::weakly_canonical(projectDirectory / include.path, ec);
            if (ec || !std::filesystem::is_directory(path, ec) || ec)
            {
                error = "CppLibraryReference include directory does not exist: '" + include.path.string() + "'";
                return std::nullopt;
            }
            reference.includePaths.push_back({path, include.kind});
        }
        result.push_back(std::move(reference));
    }
    return result;
}

std::optional<std::vector<SdkManifest>> loadSdkReferences(
    const std::filesystem::path& projectDirectory,
    const ProjectDescriptor& descriptor,
    std::string& error)
{
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
        if (std::find(sdkReferenceKeys.begin(), sdkReferenceKeys.end(), key) !=
            sdkReferenceKeys.end())
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
    return sdkReferences;
}
}

ProjectLoader::ProjectLoader()
    : cache_(std::make_shared<ProjectLoaderCache>())
{
}

std::optional<ProjectBuildSettings> ProjectLoader::loadBuildSettings(
    const std::filesystem::path& projectFile,
    std::string& error) const
{
    std::filesystem::path canonicalProject;
    ProjectDescriptor descriptor;
    if (!loadDescriptorFile(projectFile, canonicalProject, descriptor, *cache_, error))
    {
        return std::nullopt;
    }
    ProjectBuildSettings settings = std::move(descriptor.buildSettings);
    const std::filesystem::path projectDirectory = canonicalProject.parent_path();
    if (settings.outputPath.has_value())
    {
        settings.outputPath = resolveProjectPath(projectDirectory, *settings.outputPath);
    }
    if (settings.libraryReferenceRoot.has_value())
    {
        settings.libraryReferenceRoot =
            resolveProjectPath(projectDirectory, *settings.libraryReferenceRoot);
    }
    return settings;
}

std::optional<Project> ProjectLoader::load(
    const std::filesystem::path& projectFile,
    std::string& error) const
{
    return load(projectFile, ProjectLoadOptions{}, error);
}

std::optional<Project> ProjectLoader::load(
    const std::filesystem::path& projectFile,
    const ProjectLoadOptions& options,
    std::string& error) const
{
    const ProjectLoadOptions effectiveOptions = effectiveLoadOptions(options);
    std::filesystem::path canonicalProject;
    ProjectDescriptor descriptor;
    if (!loadDescriptorFile(projectFile, canonicalProject, descriptor, *cache_, error))
    {
        return std::nullopt;
    }
    std::string moduleName;
    if (!prepareProjectDescriptor(canonicalProject, descriptor, moduleName, error))
        return std::nullopt;

    const std::filesystem::path projectDirectory = canonicalProject.parent_path();
    ProjectSourceSet sourceSet(projectDirectory);
    std::vector<std::filesystem::path> candidates;
    if (!collectSourceCandidates(projectDirectory, descriptor, candidates, error))
        return std::nullopt;
    if (!populateSourceSet(
            candidates, !descriptor.projectReferences.empty(), sourceSet, error))
        return std::nullopt;

    std::optional<std::vector<std::filesystem::path>> projectReferences =
        loadProjectReferences(projectDirectory, descriptor, error);
    if (!projectReferences.has_value()) return std::nullopt;

    std::optional<std::vector<LibraryReference>> libraryReferences =
        loadLibraryReferences(projectDirectory, descriptor, effectiveOptions, error);
    if (!libraryReferences.has_value()) return std::nullopt;

    std::optional<std::vector<CppLibraryReference>> cppLibraryReferences =
        loadCppLibraryReferences(projectDirectory, descriptor, effectiveOptions, error);
    if (!cppLibraryReferences.has_value()) return std::nullopt;

    std::optional<std::vector<SdkManifest>> sdkReferences =
        loadSdkReferences(projectDirectory, descriptor, error);
    if (!sdkReferences.has_value()) return std::nullopt;

    return Project(
        canonicalProject,
        moduleName,
        descriptor.outputKind.value_or(ProjectOutputKind::Executable),
        std::move(sourceSet),
        std::move(*projectReferences),
        std::move(*libraryReferences),
        std::move(*sdkReferences),
        descriptor.panicProfile.value_or(language::PanicProfile::Abort),
        descriptor.panicHandler.value_or(std::string{}),
        descriptor.packageName.value_or(moduleName),
        std::move(descriptor.friendModules),
        std::move(*cppLibraryReferences),
        descriptor.cppConsumerVolatileMode);
}

ProjectLoaderStatistics ProjectLoader::statistics() const
{
    const std::lock_guard<std::mutex> lock(cache_->mutex);
    return cache_->statistics;
}
}
