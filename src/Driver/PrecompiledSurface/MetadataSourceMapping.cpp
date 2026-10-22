#include "../Internal/DriverInternal.h"

namespace nativesharp::driver
{
using target::BuiltinLayoutKind;

constexpr std::array<LayoutRow, 15> kLayoutRows = {{{"bool", BuiltinLayoutKind::Bool},
                                                    {"byte", BuiltinLayoutKind::Byte},
                                                    {"char", BuiltinLayoutKind::Char},
                                                    {"short", BuiltinLayoutKind::Short},
                                                    {"ushort", BuiltinLayoutKind::UShort},
                                                    {"int", BuiltinLayoutKind::Int},
                                                    {"uint", BuiltinLayoutKind::UInt},
                                                    {"long", BuiltinLayoutKind::Long},
                                                    {"ulong", BuiltinLayoutKind::ULong},
                                                    {"nint", BuiltinLayoutKind::NInt},
                                                    {"nuint", BuiltinLayoutKind::NUInt},
                                                    {"float", BuiltinLayoutKind::Float},
                                                    {"double", BuiltinLayoutKind::Double},
                                                    {"enum", BuiltinLayoutKind::Enum},
                                                    {"pointer", BuiltinLayoutKind::Pointer}}};

std::string foldedPathKey(const std::filesystem::path &path)
{
    const std::u8string encoded = path.lexically_normal().generic_u8string();
    std::string key(reinterpret_cast<const char *>(encoded.data()), encoded.size());
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return key;
}

std::string foldedName(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

std::string_view subsystemDisplayName(linker::WindowsExecutableSubsystem subsystem)
{
    return subsystem == linker::WindowsExecutableSubsystem::Windows ? "Windows GUI" : "Windows CUI";
}

std::string escapedStringLiteral(std::string_view value)
{
    std::string result;
    result.push_back('"');
    for (const char character : value)
    {
        switch (character)
        {
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\\"";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        case '\0':
            result += "\\0";
            break;
        default:
            result.push_back(character);
            break;
        }
    }
    result.push_back('"');
    return result;
}

std::vector<std::string> extractMetadataTypeModifiers(std::string &typeKey)
{
    std::vector<std::string> modifiers;
    while (!typeKey.empty())
    {
        if (typeKey.ends_with("[]"))
        {
            modifiers.emplace_back("[]");
            typeKey.resize(typeKey.size() - 2);
            continue;
        }
        if (typeKey.ends_with("*"))
        {
            modifiers.emplace_back("*");
            typeKey.pop_back();
            continue;
        }
        if (typeKey.ends_with("]"))
        {
            const std::size_t open = typeKey.find_last_of('[');
            if (open == std::string::npos)
                break;
            modifiers.push_back(typeKey.substr(open));
            typeKey.resize(open);
            continue;
        }
        break;
    }
    return modifiers;
}

bool sourceIteratorTypeForMetadataKey(std::string_view typeKey, std::string_view prefix, std::string_view sourceName,
                                      std::string &sourceType, std::string &error)
{
    const std::string rest = std::string(typeKey.substr(prefix.size()));
    const std::size_t pipe = rest.find('|');
    const std::string itemKey = pipe == std::string::npos ? rest : rest.substr(0, pipe);
    const std::string errorKey = pipe == std::string::npos ? std::string{} : rest.substr(pipe + 1);
    std::string itemSource;
    std::string errorSource;
    if (!sourceTypeForMetadataKey(itemKey, itemSource, error))
        return false;
    if (!errorKey.empty() && !sourceTypeForMetadataKey(errorKey, errorSource, error))
        return false;
    sourceType = std::string(sourceName) + "<" + itemSource;
    if (!errorKey.empty())
        sourceType += ", " + errorSource;
    sourceType += ">";
    return true;
}

bool sourceFutureTypeForMetadataKey(std::string_view typeKey, std::string &sourceType, std::string &error)
{
    // NS304-T05-F09: the opaque future surface materializes as Future<item>; the carried error
    // type travels through the member's error contract (declaredErrorTypeKey), not the spelling.
    const std::string rest = std::string(typeKey.substr(std::string_view("future:").size()));
    const std::size_t pipe = rest.find('|');
    const std::string itemKey = pipe == std::string::npos ? rest : rest.substr(0, pipe);
    std::string itemSource;
    if (!sourceTypeForMetadataKey(itemKey, itemSource, error))
        return false;
    sourceType = "Future<" + itemSource + ">";
    return true;
}

bool sourceCoreTypeForMetadataKey(const std::string &typeKey, std::string &sourceType, std::string &error)
{

    if (typeKey.starts_with("builtin:"))
    {
        sourceType = typeKey.substr(8);
    }
    else if (typeKey.starts_with("type:"))
    {
        const std::string identity = typeKey.substr(5);
        const std::size_t moduleSeparator = identity.find("::");
        if (moduleSeparator == std::string::npos)
        {
            error = "metadata contains malformed named type key '" + typeKey + "'";
            return false;
        }
        sourceType = identity.substr(moduleSeparator + 2);
        std::replace(sourceType.begin(), sourceType.end(), '+', '.');
        if (sourceType.find('`') != std::string::npos)
        {
            error = "metadata-backed source import does not yet support open generic "
                    "type key '" +
                    typeKey + "'";
            return false;
        }
    }
    else if (typeKey.starts_with("async-iterator:"))
    {
        // NS304-T07-F08: the opaque async-stream surface materializes as AsyncIterator<item, error>.
        return sourceIteratorTypeForMetadataKey(typeKey, "async-iterator:", "AsyncIterator", sourceType, error);
    }
    else if (typeKey.starts_with("iterator:"))
    {
        // NS304-T06-F08: the opaque iterator surface materializes as Iterator<item> (or <item, error>).
        return sourceIteratorTypeForMetadataKey(typeKey, "iterator:", "Iterator", sourceType, error);
    }
    else if (typeKey.starts_with("future:"))
    {
        return sourceFutureTypeForMetadataKey(typeKey, sourceType, error);
    }
    else
    {
        error = "metadata-backed source import cannot materialize type key '" + typeKey + "'";
        return false;
    }
    return true;
}

bool sourceTypeForMetadataKey(std::string typeKey, std::string &sourceType, std::string &error)
{
    const std::vector<std::string> modifiers = extractMetadataTypeModifiers(typeKey);
    if (!sourceCoreTypeForMetadataKey(typeKey, sourceType, error))
        return false;
    for (auto iterator = modifiers.rbegin(); iterator != modifiers.rend(); ++iterator)
    {
        sourceType += *iterator;
    }
    return true;
}

std::string ownershipPrefix(interop::LibraryMetadataOwnershipKind ownership)
{
    switch (ownership)
    {
    case interop::LibraryMetadataOwnershipKind::Owned:
        return "owned ";
    case interop::LibraryMetadataOwnershipKind::Borrowed:
        return "borrowed ";
    case interop::LibraryMetadataOwnershipKind::None:
        return {};
    }
    return {};
}

std::string lifetimeAnnotation(std::string_view lifetime)
{
    if (lifetime.empty() || lifetime.starts_with("$p"))
        return {};
    return "[" + std::string(lifetime) + "]";
}

std::string ownershipPrefix(interop::LibraryMetadataOwnershipKind ownership, std::string_view lifetime)
{
    if (ownership != interop::LibraryMetadataOwnershipKind::Borrowed || lifetimeAnnotation(lifetime).empty())
        return ownershipPrefix(ownership);
    return "borrowed" + lifetimeAnnotation(lifetime) + " ";
}

// DLL-P4 publication rule. A member that legal consumer source can name directly must have an
// importable entry. Public members qualify everywhere; protected members qualify through a
// consumer-owned derived class (including `base.Foo()`). Internal/private implementation details
// remain in producer-owned dispatch tables and must not be over-exported.
bool hasConsumerCallableAccessibility(const std::uint8_t accessibility) noexcept
{
    constexpr std::uint8_t kProtectedAccessibility = 1U;
    constexpr std::uint8_t kPublicAccessibility = 3U;
    return accessibility == kProtectedAccessibility || accessibility == kPublicAccessibility;
}

bool hasConsumerCallableAccessibility(const frontend::ClassMemberAccessibility accessibility) noexcept
{
    return accessibility == frontend::ClassMemberAccessibility::Protected ||
           accessibility == frontend::ClassMemberAccessibility::Public;
}

bool isConsumerCallableNativeMethod(const interop::LibraryMetadataMember &member) noexcept
{
    constexpr std::uint32_t kAbstract = 1U << 5U;
    constexpr std::uint32_t kExternC = 1U << 8U;
    constexpr std::uint32_t kExportedC = 1U << 9U;
    constexpr std::uint32_t kCompilerGenerated = 1U << 10U;
    constexpr std::uint32_t kExcluded = kAbstract | kCompilerGenerated;
    constexpr std::uint8_t kMethodKind = 2U;
    return member.kind == kMethodKind && hasConsumerCallableAccessibility(member.accessibility) &&
           (member.flags & kExcluded) == 0U && ((member.flags & kExternC) == 0U || (member.flags & kExportedC) != 0U) &&
           !member.mangledName.empty();
}

std::string metadataBaseTypeKey(std::string typeKey)
{
    while (!typeKey.empty())
    {
        if (typeKey.ends_with("[]"))
        {
            typeKey.resize(typeKey.size() - 2);
            continue;
        }
        if (typeKey.ends_with('*'))
        {
            typeKey.pop_back();
            continue;
        }
        if (typeKey.ends_with(']'))
        {
            const std::size_t open = typeKey.find_last_of('[');
            if (open != std::string::npos)
            {
                typeKey.resize(open);
                continue;
            }
        }
        break;
    }
    return typeKey;
}

// The metadata spelling of an open generic declaration carries CLR-style arity
// (`Module::Namespace.Type`1`).  That spelling is intentionally rejected by
// sourceTypeForMetadataKey(): it is not a complete source type until arguments
// have been supplied.  The parser's declaration lookahead needs less
// information, however; it only needs to know that `Type` can begin a type.
// Keep that lexical concern separate from metadata type materialization so a
// reconstructed template can name a generic base that is emitted later in the
// same source buffer.
void appendParserKnownTypeNames(const interop::LibraryMetadata &metadata, const std::string &writtenTypeKey,
                                std::vector<std::string> &knownTypeNames)
{
    const std::string typeKey = metadataBaseTypeKey(writtenTypeKey);
    if (!typeKey.starts_with("type:"))
        return;

    const std::string_view identity(typeKey.data() + 5U, typeKey.size() - 5U);
    const std::size_t moduleSeparator = identity.find("::");
    if (moduleSeparator == std::string_view::npos || moduleSeparator + 2U == identity.size())
        return;

    std::string sourceName;
    sourceName.reserve(identity.size() - moduleSeparator - 2U);
    bool skippingArity = false;
    for (const char character : identity.substr(moduleSeparator + 2U))
    {
        if (character == '`')
        {
            skippingArity = true;
            continue;
        }
        if (skippingArity && character >= '0' && character <= '9')
            continue;
        skippingArity = false;
        sourceName.push_back(character == '+' ? '.' : character);
    }
    if (sourceName.empty())
        return;

    knownTypeNames.push_back(sourceName);
    const std::size_t terminalSeparator = sourceName.find_last_of('.');
    knownTypeNames.push_back(terminalSeparator == std::string::npos ? sourceName
                                                                    : sourceName.substr(terminalSeparator + 1U));

    const std::string_view module = identity.substr(0, moduleSeparator);
    if (!module.empty() && module != metadata.moduleName)
        knownTypeNames.push_back(std::string(module) + "::" + sourceName);
}

const interop::LibraryMetadataType *metadataTypeForKey(const interop::LibraryMetadata &metadata,
                                                       const std::string &typeKey)
{
    const std::string base = metadataBaseTypeKey(typeKey);
    if (!base.starts_with("type:"))
        return nullptr;
    const std::string_view identity(base.data() + 5, base.size() - 5);
    const auto found = std::lower_bound(metadata.types.begin(), metadata.types.end(), identity,
                                        [](const interop::LibraryMetadataType &candidate, std::string_view value)
                                        { return candidate.identityKey < value; });
    return found == metadata.types.end() || found->identityKey != identity ? nullptr : &*found;
}

std::optional<std::string> metadataSpecializedTypeKey(
    const interop::LibraryMetadataGenericSpecialization &specialization)
{
    constexpr std::uint8_t kGenericMethodKind = 3U;
    if (specialization.kind == kGenericMethodKind || specialization.templateKey.empty() ||
        specialization.specializedName.empty())
    {
        return std::nullopt;
    }
    const std::size_t moduleSeparator = specialization.templateKey.find("::");
    if (moduleSeparator == std::string::npos)
        return std::nullopt;
    const std::size_t terminal = specialization.templateKey.find_last_of(".+");
    const std::size_t terminalBegin =
        terminal == std::string::npos || terminal < moduleSeparator + 2U ? moduleSeparator + 2U : terminal + 1U;
    if (specialization.templateKey.find('`', terminalBegin) == std::string::npos)
        return std::nullopt;
    return "type:" + specialization.templateKey.substr(0, terminalBegin) + specialization.specializedName;
}

std::optional<std::string> metadataTypeModule(const std::string &writtenTypeKey)
{
    const std::string typeKey = metadataBaseTypeKey(writtenTypeKey);
    if (!typeKey.starts_with("type:"))
        return std::nullopt;
    const std::size_t separator = typeKey.find("::", 5U);
    if (separator == std::string::npos || separator == 5U)
        return std::nullopt;
    return typeKey.substr(5U, separator - 5U);
}

bool metadataProvidesType(const interop::LibraryMetadata &metadata, const std::string &writtenTypeKey)
{
    const std::string typeKey = metadataBaseTypeKey(writtenTypeKey);
    if (metadataTypeForKey(metadata, typeKey) != nullptr)
        return true;
    return std::ranges::any_of(metadata.genericSpecializations,
                               [&](const auto &specialization)
                               {
                                   const std::optional<std::string> candidate =
                                       metadataSpecializedTypeKey(specialization);
                                   return candidate.has_value() && *candidate == typeKey;
                               });
}

bool validatePrecompiledExternalTypeClosure(
    std::span<const LoadedPrecompiledMetadata> loaded,
    const std::unordered_map<std::string, std::filesystem::path> &sourceProjects, std::string &error)
{
    std::unordered_map<std::string, const interop::LibraryMetadata *> modules;
    for (const LoadedPrecompiledMetadata &entry : loaded)
        modules.emplace(entry.metadata->moduleName, entry.metadata.get());

    std::set<std::string> failures;
    for (const LoadedPrecompiledMetadata &entry : loaded)
    {
        for (const std::string &typeKey : entry.metadata->externalTypeKeys)
        {
            const std::optional<std::string> owner = metadataTypeModule(typeKey);
            if (!owner.has_value())
            {
                failures.insert("module '" + entry.metadata->moduleName + "' records malformed external type '" +
                                typeKey + "'");
                continue;
            }
            const auto provider = modules.find(*owner);
            if (provider == modules.end())
            {
                // A project source node in the same graph is the authoritative provider. Its
                // declaration is validated by ordinary name binding after metadata is parsed.
                if (sourceProjects.contains(*owner))
                    continue;
                failures.insert("module '" + entry.metadata->moduleName + "' requires external type '" + typeKey +
                                "', but LibraryReference module '" + *owner + "' is not present");
                continue;
            }
            if (!metadataProvidesType(*provider->second, typeKey))
            {
                failures.insert("module '" + entry.metadata->moduleName + "' requires external type '" + typeKey +
                                "', but provider module '" + *owner + "' does not declare it");
            }
        }
    }
    if (failures.empty())
        return true;
    error = "precompiled LibraryReference type closure is incomplete";
    for (const std::string &failure : failures)
        error += "\n  " + failure;
    return false;
}

bool sourceTypeForGenericSpecialization(const interop::LibraryMetadata &metadata,
                                        const interop::LibraryMetadataGenericSpecialization &specialization,
                                        std::string &sourceType, std::string &error);

bool sourceTypeForMetadataKey(const interop::LibraryMetadata &metadata, const std::string &typeKey,
                              std::string &sourceType, std::string &error)
{
    const std::string baseTypeKey = metadataBaseTypeKey(typeKey);
    if (metadataTypeForKey(metadata, baseTypeKey) == nullptr)
    {
        const auto specialization =
            std::ranges::find_if(metadata.genericSpecializations,
                                 [&](const auto &candidate)
                                 {
                                     const std::optional<std::string> candidateTypeKey =
                                         metadataSpecializedTypeKey(candidate);
                                     return candidateTypeKey.has_value() && *candidateTypeKey == baseTypeKey;
                                 });
        if (specialization != metadata.genericSpecializations.end())
        {
            if (!sourceTypeForGenericSpecialization(metadata, *specialization, sourceType, error))
                return false;
            sourceType += typeKey.substr(baseTypeKey.size());
            return true;
        }
    }
    if (!sourceTypeForMetadataKey(typeKey, sourceType, error))
        return false;

    // A canonical NSMETA key owns more identity than its C#-style namespace spelling:
    // `type:Module::Namespace.Type`. The public-source bridge used to discard `Module::`, which
    // is harmless only while exactly one visible module declares that name. Store templates are
    // commonly visible both from their source ProjectReference and from a LibraryReference's
    // materialized template closure, so the unqualified spelling can bind to two templates. Keep
    // the module for every external named type; declarations owned by this metadata module remain
    // ordinary source names so their reconstructed bodies can refer to one another naturally.
    if (baseTypeKey.starts_with("type:"))
    {
        const std::string_view identity(baseTypeKey.data() + 5U, baseTypeKey.size() - 5U);
        const std::size_t moduleSeparator = identity.find("::");
        if (moduleSeparator != std::string_view::npos && identity.substr(0, moduleSeparator) != metadata.moduleName)
        {
            sourceType.insert(0, std::string(identity.substr(0, moduleSeparator)) + "::");
        }
    }
    return true;
}

bool sourceTypeForGenericSpecialization(const interop::LibraryMetadata &metadata,
                                        const interop::LibraryMetadataGenericSpecialization &specialization,
                                        std::string &sourceType, std::string &error)
{
    constexpr std::uint8_t kGenericMethodKind = 3U;
    if (specialization.kind == kGenericMethodKind)
    {
        error = "metadata method specialization cannot be used as a type";
        return false;
    }
    const std::size_t moduleSeparator = specialization.templateKey.find("::");
    if (moduleSeparator == std::string::npos)
    {
        error = "metadata contains malformed generic template key '" + specialization.templateKey + "'";
        return false;
    }
    const std::size_t terminal = specialization.templateKey.find_last_of(".+");
    const std::size_t terminalBegin =
        terminal == std::string::npos || terminal < moduleSeparator + 2U ? moduleSeparator + 2U : terminal + 1U;
    const std::size_t arity = specialization.templateKey.find('`', terminalBegin);
    if (arity == std::string::npos || specialization.typeArguments.empty())
    {
        error =
            "metadata contains an incomplete closed generic specialization '" + specialization.specializationKey + "'";
        return false;
    }

    const std::string_view templateModule(specialization.templateKey.data(), moduleSeparator);
    sourceType = specialization.templateKey.substr(moduleSeparator + 2U, arity - (moduleSeparator + 2U));
    std::replace(sourceType.begin(), sourceType.end(), '+', '.');
    if (templateModule != metadata.moduleName)
        sourceType.insert(0, std::string(templateModule) + "::");
    sourceType.push_back('<');
    for (std::size_t index = 0; index < specialization.typeArguments.size(); ++index)
    {
        if (index != 0U)
            sourceType += ", ";
        const std::string &argument = specialization.typeArguments[index];
        std::string argumentType;
        if (argument.starts_with("generic:"))
        {
            const std::string_view requested(argument.data() + 8U, argument.size() - 8U);
            const auto nested = std::ranges::find_if(metadata.genericSpecializations, [&](const auto &candidate)
                                                     { return requested == candidate.specializationKey; });
            if (nested == metadata.genericSpecializations.end() ||
                !sourceTypeForGenericSpecialization(metadata, *nested, argumentType, error))
            {
                if (nested == metadata.genericSpecializations.end())
                    error = "metadata generic specialization '" + specialization.specializationKey +
                            "' refers to missing nested specialization '" + std::string(requested) + "'";
                return false;
            }
        }
        else if (argument.starts_with("type:") || argument.starts_with("builtin:"))
        {
            if (!sourceTypeForMetadataKey(metadata, argument, argumentType, error))
                return false;
        }
        else
        {
            // NSMETA 1.7-1.11 stored readable spellings.  Builtins remain valid as written; the
            // producer-side fix now stores canonical named identities for cross-module demands.
            argumentType = argument;
        }
        sourceType += argumentType;
    }
    sourceType.push_back('>');
    return true;
}

void Driver::printDataLayout(std::ostream &output)
{
    const target::TargetDataLayout dataLayout = target::TargetDataLayout::WindowsX64();
    output << "endianness: little\n"
           << "max-natural-alignment: " << dataLayout.maximumNaturalAlignment() << "\n"
           << "type size align\n";

    for (const LayoutRow &row : kLayoutRows)
    {
        const target::TypeLayout layout = dataLayout.layoutOf(row.kind);
        output << row.name << ' ' << layout.size << ' ' << layout.alignment << '\n';
    }
}
} // namespace nativesharp::driver
