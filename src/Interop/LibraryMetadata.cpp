#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Interop/LibraryMetadataMerger.h"

#include "nativesharp/Basic/ArtifactTransaction.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Compilation/Project.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nativesharp::interop
{
namespace
{
constexpr std::array<std::uint8_t, 8> kMagic = {'N', 'S', 'M', 'E', 'T', 'A', '1', 0};
constexpr std::uint16_t kFormatMajor = kMetadataFormatMajor;
constexpr std::uint16_t kFormatMinor = kMetadataFormatMinor;
constexpr std::uint32_t kEndianMarker = 0x01020304U;
constexpr std::uint32_t kWindowsX64Target = 1;
constexpr std::uint32_t kPointerSize = 8;
constexpr std::uint32_t kPointerAlignment = 8;
constexpr std::string_view kTargetTriple = "x86_64-pc-windows-msvc";
// The compatibility window is explicit rather than inferred from SemVer arithmetic. Its N-1
// endpoint is shared with the public compiler compatibility matrix.
constexpr std::size_t kHeaderSize = 100;
constexpr std::uint64_t kMaximumPayloadSize = 128ULL * 1024ULL * 1024ULL;
constexpr std::uint32_t kMaximumRecordCount = 1'000'000U;
constexpr std::uint32_t kMaximumStringSize = 16U * 1024U * 1024U;

std::string_view compilerReleasePart(std::string_view version) noexcept
{
    const std::size_t channelOrBuildMetadata = version.find_first_of("-+");
    return version.substr(0, channelOrBuildMetadata);
}

std::string foldedAscii(std::string value)
{
    std::ranges::transform(value, value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

class ByteWriter final
{
public:
    void u8(std::uint8_t value) { bytes_.push_back(value); }

    void u16(std::uint16_t value)
    {
        u8(static_cast<std::uint8_t>(value & 0xffU));
        u8(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    }

    void u32(std::uint32_t value)
    {
        for (unsigned shift = 0; shift < 32U; shift += 8U)
        {
            u8(static_cast<std::uint8_t>((value >> shift) & 0xffU));
        }
    }

    void u64(std::uint64_t value)
    {
        for (unsigned shift = 0; shift < 64U; shift += 8U)
        {
            u8(static_cast<std::uint8_t>((value >> shift) & 0xffU));
        }
    }

    void raw(std::span<const std::uint8_t> bytes)
    {
        bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    }

    void text(std::string_view value)
    {
        u32(static_cast<std::uint32_t>(value.size()));
        bytes_.insert(bytes_.end(), value.begin(), value.end());
    }

    const std::vector<std::uint8_t>& bytes() const noexcept { return bytes_; }

private:
    std::vector<std::uint8_t> bytes_;
};

class ByteReader final
{
public:
    explicit ByteReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    bool u8(std::uint8_t& value)
    {
        if (position_ >= bytes_.size()) return false;
        value = bytes_[position_++];
        return true;
    }

    bool u16(std::uint16_t& value)
    {
        std::uint8_t a = 0;
        std::uint8_t b = 0;
        if (!u8(a) || !u8(b)) return false;
        value = static_cast<std::uint16_t>(a) |
            static_cast<std::uint16_t>(static_cast<std::uint16_t>(b) << 8U);
        return true;
    }

    bool u32(std::uint32_t& value)
    {
        value = 0;
        for (unsigned shift = 0; shift < 32U; shift += 8U)
        {
            std::uint8_t byte = 0;
            if (!u8(byte)) return false;
            value |= static_cast<std::uint32_t>(byte) << shift;
        }
        return true;
    }

    bool u64(std::uint64_t& value)
    {
        value = 0;
        for (unsigned shift = 0; shift < 64U; shift += 8U)
        {
            std::uint8_t byte = 0;
            if (!u8(byte)) return false;
            value |= static_cast<std::uint64_t>(byte) << shift;
        }
        return true;
    }

    bool text(std::string& value)
    {
        std::uint32_t size = 0;
        if (!u32(size) || size > kMaximumStringSize || remaining() < size) return false;
        value.assign(
            reinterpret_cast<const char*>(bytes_.data() + position_),
            static_cast<std::size_t>(size));
        position_ += size;
        return true;
    }

    bool raw(std::span<std::uint8_t> destination)
    {
        if (remaining() < destination.size()) return false;
        std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(position_), destination.size(), destination.begin());
        position_ += destination.size();
        return true;
    }

    std::size_t remaining() const noexcept { return bytes_.size() - position_; }
    bool atEnd() const noexcept { return position_ == bytes_.size(); }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t position_ = 0;
};

std::uint64_t stableTypeId(std::string_view value) noexcept
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char character : value)
    {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(character));
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string typeKeyForOrdinal(
    std::span<const frontend::NamedTypeDeclaration> types,
    std::size_t ordinal)
{
    if (ordinal >= types.size()) return {};
    return "type:" + types[ordinal].identity.key();
}

std::string metadataBaseTypeKey(std::string typeKey)
{
    while (!typeKey.empty())
    {
        if (typeKey.ends_with("[]"))
        {
            typeKey.resize(typeKey.size() - 2U);
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

std::optional<std::string> metadataTypeModule(std::string_view writtenTypeKey)
{
    const std::string typeKey = metadataBaseTypeKey(std::string(writtenTypeKey));
    if (!typeKey.starts_with("type:")) return std::nullopt;
    const std::size_t separator = typeKey.find("::", 5U);
    if (separator == std::string::npos || separator == 5U) return std::nullopt;
    return typeKey.substr(5U, separator - 5U);
}

std::optional<std::string> specializedTypeKey(
    std::uint8_t kind,
    std::string_view templateKey,
    std::string_view specializedName)
{
    if (kind == static_cast<std::uint8_t>(frontend::GenericTemplateKind::Method) ||
        templateKey.empty() || specializedName.empty())
    {
        return std::nullopt;
    }
    const std::size_t moduleSeparator = templateKey.find("::");
    if (moduleSeparator == std::string_view::npos) return std::nullopt;
    const std::size_t terminal = templateKey.find_last_of(".+");
    const std::size_t terminalBegin = terminal == std::string_view::npos || terminal < moduleSeparator + 2U
        ? moduleSeparator + 2U
        : terminal + 1U;
    const std::size_t arity = templateKey.find('`', terminalBegin);
    if (arity == std::string_view::npos) return std::nullopt;
    return "type:" + std::string(templateKey.substr(0, terminalBegin)) + std::string(specializedName);
}

std::optional<std::string> specializedTypeKey(
    const frontend::GenericSpecializationRecord& specialization)
{
    return specializedTypeKey(
        static_cast<std::uint8_t>(specialization.kind),
        specialization.templateKey,
        specialization.specializedName);
}

std::optional<std::string> specializedTypeKey(
    const LibraryMetadataGenericSpecialization& specialization)
{
    return specializedTypeKey(
        specialization.kind,
        specialization.templateKey,
        specialization.specializedName);
}

bool looksLikeCompilerSpecializedType(std::string_view typeKey)
{
    if (!typeKey.starts_with("type:")) return false;
    const std::size_t terminal = typeKey.find_last_of(".+");
    const std::string_view name = typeKey.substr(terminal == std::string_view::npos ? 5U : terminal + 1U);
    const std::size_t generic = name.find("__g");
    const std::size_t hash = name.rfind("__h");
    if (generic == std::string_view::npos || hash == std::string_view::npos || hash <= generic + 3U ||
        hash + 3U + 16U != name.size())
    {
        return false;
    }
    std::size_t cursor = generic + 3U;
    if (cursor >= hash || !std::isdigit(static_cast<unsigned char>(name[cursor]))) return false;
    while (cursor < hash && std::isdigit(static_cast<unsigned char>(name[cursor]))) ++cursor;
    if (cursor + 2U > hash || name.substr(cursor, 2U) != "__") return false;
    for (const char character : name.substr(hash + 3U))
    {
        if (!std::isxdigit(static_cast<unsigned char>(character))) return false;
    }
    return true;
}

std::optional<std::size_t> rootProjectOrdinal(const frontend::ProjectCompilation& compilation)
{
    if (compilation.projectGraph() == nullptr || compilation.projectGraph()->rootProject() == nullptr)
    {
        return std::nullopt;
    }
    const auto projects = compilation.projectGraph()->projects();
    const compilation::Project* root = compilation.projectGraph()->rootProject();
    for (std::size_t ordinal = 0; ordinal < projects.size(); ++ordinal)
    {
        if (&projects[ordinal] == root) return ordinal;
    }
    return std::nullopt;
}

LibraryMetadataTypeKind metadataKind(frontend::NamedTypeDeclarationKind kind)
{
    switch (kind)
    {
        case frontend::NamedTypeDeclarationKind::Class: return LibraryMetadataTypeKind::Class;
        case frontend::NamedTypeDeclarationKind::Interface: return LibraryMetadataTypeKind::Interface;
        case frontend::NamedTypeDeclarationKind::Struct: return LibraryMetadataTypeKind::Struct;
        case frontend::NamedTypeDeclarationKind::Enum: return LibraryMetadataTypeKind::Enum;
        case frontend::NamedTypeDeclarationKind::Delegate: return LibraryMetadataTypeKind::Delegate;
    }
    return LibraryMetadataTypeKind::Struct;
}

LibraryMetadataOwnershipKind metadataOwnership(sema::OwnershipKind ownership) noexcept
{
    switch (ownership)
    {
        case sema::OwnershipKind::None: return LibraryMetadataOwnershipKind::None;
        case sema::OwnershipKind::Owned: return LibraryMetadataOwnershipKind::Owned;
        case sema::OwnershipKind::Borrowed: return LibraryMetadataOwnershipKind::Borrowed;
    }
    return LibraryMetadataOwnershipKind::None;
}

LibraryMetadataParameterPassKind metadataPassKind(frontend::ParameterPassKind passKind) noexcept
{
    switch (passKind)
    {
        case frontend::ParameterPassKind::Value: return LibraryMetadataParameterPassKind::Value;
        case frontend::ParameterPassKind::Ref: return LibraryMetadataParameterPassKind::Ref;
        case frontend::ParameterPassKind::Out: return LibraryMetadataParameterPassKind::Out;
        case frontend::ParameterPassKind::In: return LibraryMetadataParameterPassKind::In;
    }
    return LibraryMetadataParameterPassKind::Value;
}

bool metadataDefaultValue(
    const ast::AstNode* syntax,
    LibraryMetadataParameter& parameter,
    std::string& error)
{
    if (syntax == nullptr) return true;

    const ast::AstNode* value = syntax;
    bool negative = false;
    if (syntax->kind() == ast::AstNodeKind::UnaryExpression)
    {
        value = syntax->first();
        negative = syntax->tokenKind() == lexer::TokenKind::Minus;
    }
    if (value == nullptr)
    {
        error = "default parameter metadata has an incomplete literal expression";
        return false;
    }

    switch (value->kind())
    {
        case ast::AstNodeKind::NullLiteralExpression:
            parameter.defaultValueKind = LibraryMetadataDefaultValueKind::Null;
            return true;
        case ast::AstNodeKind::BooleanLiteralExpression:
            parameter.defaultValueKind = LibraryMetadataDefaultValueKind::Boolean;
            parameter.defaultValueBits = value->tokenKind() == lexer::TokenKind::KwTrue ? 1U : 0U;
            return true;
        case ast::AstNodeKind::IntegerLiteralExpression:
            parameter.defaultValueKind = LibraryMetadataDefaultValueKind::Integer;
            parameter.defaultValueFlags = negative ? 1U : 0U;
            parameter.defaultValueBits = value->integerValue();
            return true;
        case ast::AstNodeKind::FloatingLiteralExpression:
        {
            parameter.defaultValueKind = LibraryMetadataDefaultValueKind::Floating;
            const double parsed = negative ? -value->floatingValue() : value->floatingValue();
            parameter.defaultValueBits = std::bit_cast<std::uint64_t>(parsed);
            return true;
        }
        case ast::AstNodeKind::CharacterLiteralExpression:
            parameter.defaultValueKind = LibraryMetadataDefaultValueKind::Character;
            parameter.defaultValueBits = value->integerValue();
            return true;
        case ast::AstNodeKind::StringLiteralExpression:
            parameter.defaultValueKind = LibraryMetadataDefaultValueKind::String;
            parameter.defaultValueText = value->text();
            return true;
        default:
            error = "default parameter metadata requires a literal expression";
            return false;
    }
}

bool readOwnership(
    ByteReader& reader,
    std::uint16_t formatMinor,
    LibraryMetadataOwnershipKind& ownership)
{
    if (formatMinor == 0)
    {
        ownership = LibraryMetadataOwnershipKind::None;
        return true;
    }

    std::uint8_t raw = 0;
    if (!reader.u8(raw) || raw > static_cast<std::uint8_t>(LibraryMetadataOwnershipKind::Borrowed))
    {
        return false;
    }
    ownership = static_cast<LibraryMetadataOwnershipKind>(raw);
    return true;
}

std::uint16_t typeFlags(const frontend::NamedTypeDeclaration& type) noexcept
{
    std::uint16_t flags = 0;
    if (type.isPartial) flags |= 1U << 0U;
    if (type.isAbstract) flags |= 1U << 1U;
    if (type.isSealed) flags |= 1U << 2U;
    if (type.isStatic) flags |= 1U << 3U;
    if (type.syntax != nullptr && type.syntax->auxiliaryText().find("borrowedview") != std::string::npos)
        flags |= 1U << 4U;
    if (type.syntax != nullptr && type.syntax->auxiliaryText().find("union") != std::string::npos)
        flags |= 1U << 5U;
    if (type.syntax != nullptr && type.syntax->hasModifier(lexer::TokenKind::KwReadonly))
        flags |= 1U << 6U;
    return flags;
}

std::uint32_t memberFlags(const frontend::ClassMemberSymbol& member) noexcept
{
    std::uint32_t flags = 0;
    if (member.isStatic) flags |= 1U << 0U;
    if (member.isReadonly) flags |= 1U << 1U;
    if (member.isConst) flags |= 1U << 2U;
    if (member.isVirtual) flags |= 1U << 3U;
    if (member.isOverride) flags |= 1U << 4U;
    if (member.isAbstract) flags |= 1U << 5U;
    if (member.isSealed) flags |= 1U << 6U;
    if (member.isExported) flags |= 1U << 7U;
    if (member.isExternC) flags |= 1U << 8U;
    if (member.isExportedC) flags |= 1U << 9U;
    if (member.isCompilerGenerated) flags |= 1U << 10U;
    if (member.isEvent) flags |= 1U << 11U;
    if (member.isPropertyAccessor) flags |= 1U << 12U;
    if (member.isIndexerAccessor) flags |= 1U << 13U;
    if (member.isGetter) flags |= 1U << 14U;
    if (member.isSetter) flags |= 1U << 15U;
    if (member.isUserOperator) flags |= 1U << 16U;
    if (member.isUserConversion) flags |= 1U << 17U;
    if (member.isImplicitUserConversion) flags |= 1U << 18U;
    if (member.isNewSlot) flags |= 1U << 19U;
    if (member.isImportedC) flags |= 1U << 20U;
    if (member.syntax != nullptr &&
        member.syntax->auxiliaryText().find("anonymous-member-carrier") != std::string::npos)
        flags |= kMetadataFieldAnonymousCarrier;
    if (member.isBitField) flags |= kMetadataFieldBitField;
    if (member.isUnnamedBitField) flags |= kMetadataFieldUnnamedBitField;
    flags |= static_cast<std::uint32_t>(member.inliningPolicy) << kMetadataMemberInliningShift;
    if (member.isVolatile) flags |= kMetadataMemberVolatile;
    if (member.isReferenceReturn) flags |= kMetadataMemberReferenceReturn;
    if (member.isNoHeap) flags |= kMetadataMemberNoHeap;
    if (member.isVariadic) flags |= kMetadataMemberVariadic;
    return flags;
}

std::string signatureForMember(
    std::span<const frontend::ClassMemberSymbol> members,
    std::optional<std::size_t> ordinal)
{
    if (!ordinal.has_value() || *ordinal >= members.size()) return {};
    return members[*ordinal].signatureKey;
}

struct TemplateSourceReplacement final
{
    std::size_t begin = 0;
    std::size_t end = 0;
    std::string text;
};

std::string sourceNameForTemplateType(std::string_view writtenTypeKey)
{
    const std::string typeKey = metadataBaseTypeKey(std::string(writtenTypeKey));
    if (!typeKey.starts_with("type:")) return {};
    const std::string_view identity(typeKey.data() + 5U, typeKey.size() - 5U);
    const std::size_t moduleSeparator = identity.find("::");
    if (moduleSeparator == std::string_view::npos || moduleSeparator == 0U) return {};
    const std::string_view moduleName = identity.substr(0, moduleSeparator);
    const std::string_view name = identity.substr(moduleSeparator + 2U);
    std::string sourceName;
    sourceName.reserve(moduleName.size() + name.size() + 2U);
    sourceName += moduleName;
    sourceName += "::";
    for (std::size_t index = 0; index < name.size(); ++index)
    {
        if (name[index] == static_cast<char>(0x60))
        {
            while (index + 1U < name.size() &&
                   std::isdigit(static_cast<unsigned char>(name[index + 1U])) != 0)
            {
                ++index;
            }
            continue;
        }
        sourceName.push_back(name[index] == '+' ? '.' : name[index]);
    }
    return sourceName;
}

bool rangeIsInside(SourceRange inner, SourceRange outer) noexcept
{
    return inner.isValid() && outer.isValid() &&
        inner.begin().fileId() == outer.begin().fileId() &&
        inner.begin().offset() >= outer.begin().offset() &&
        inner.end().offset() <= outer.end().offset();
}

bool materializeGenericTemplateSource(
    const frontend::ProjectCompilation& compilation,
    const frontend::GenericTemplateRecord& generic,
    std::string& source,
    std::unordered_set<std::string>& externalTypeKeys,
    std::string& error)
{
    const SourceRange rootRange = generic.syntax->range();
    source = std::string(compilation.sourceManager().getText(rootRange));
    std::vector<TemplateSourceReplacement> replacements;
    const auto appendReplacement = [&](SourceRange range,
                                       std::size_t localBegin,
                                       std::size_t localEnd,
                                       std::string text) -> bool
    {
        if (!rangeIsInside(range, rootRange) || localBegin >= localEnd ||
            localEnd > range.end().offset() - range.begin().offset())
        {
            error = "cannot canonicalize an external type reference in reusable template '" +
                generic.canonicalKey + "'";
            return false;
        }
        replacements.push_back({
            static_cast<std::size_t>(range.begin().offset() - rootRange.begin().offset()) + localBegin,
            static_cast<std::size_t>(range.begin().offset() - rootRange.begin().offset()) + localEnd,
            std::move(text)});
        return true;
    };
    // Generic declarations are removed from the ordinary NameLookup input before this late
    // metadata phase. ProjectGenericModel therefore records these references while the original
    // using/alias scope still exists; consume that semantic record instead of guessing from the
    // source spelling after name lookup has discarded the declaration.
    for (const frontend::GenericTemplateExternalTypeReference& reference :
         generic.externalTypeReferences)
    {
        const std::string typeKey = metadataBaseTypeKey(reference.typeKey);
        const std::size_t moduleSeparator = typeKey.find("::", 5U);
        const std::string sourceType = sourceNameForTemplateType(typeKey);
        if (moduleSeparator == std::string::npos || sourceType.empty())
        {
            error = "reusable template '" + generic.canonicalKey +
                "' records malformed external type key '" + reference.typeKey + "'";
            return false;
        }
        if (typeKey.find(static_cast<char>(0x60), moduleSeparator + 2U) == std::string::npos)
            externalTypeKeys.insert(typeKey);

        const SourceRange typeRange = reference.range;
        const std::string_view written = compilation.sourceManager().getText(typeRange);
        std::size_t baseBegin = 0;
        while (baseBegin < written.size() &&
               std::isspace(static_cast<unsigned char>(written[baseBegin])) != 0)
        {
            ++baseBegin;
        }
        for (const std::string_view qualifier :
             {std::string_view("owned"), std::string_view("borrowed")})
        {
            if (written.substr(baseBegin).starts_with(qualifier) &&
                baseBegin + qualifier.size() < written.size() &&
                std::isspace(static_cast<unsigned char>(
                    written[baseBegin + qualifier.size()])) != 0)
            {
                baseBegin += qualifier.size();
                while (baseBegin < written.size() &&
                       std::isspace(static_cast<unsigned char>(written[baseBegin])) != 0)
                {
                    ++baseBegin;
                }
                break;
            }
        }
        std::size_t baseEnd = baseBegin;
        while (baseEnd < written.size())
        {
            const unsigned char character = static_cast<unsigned char>(written[baseEnd]);
            if (std::isalnum(character) == 0 && character != '_' &&
                character != '.' && character != ':')
            {
                break;
            }
            ++baseEnd;
        }
        if (!appendReplacement(typeRange, baseBegin, baseEnd, sourceType)) return false;
    }

    std::sort(replacements.begin(), replacements.end(), [](const auto& left, const auto& right) {
        if (left.begin != right.begin) return left.begin > right.begin;
        return left.end > right.end;
    });
    replacements.erase(
        std::unique(replacements.begin(), replacements.end(), [](const auto& left, const auto& right) {
            return left.begin == right.begin && left.end == right.end && left.text == right.text;
        }),
        replacements.end());
    std::size_t nextBegin = source.size();
    for (const TemplateSourceReplacement& replacement : replacements)
    {
        if (replacement.end > nextBegin)
        {
            error = "overlapping canonical type references in reusable template '" +
                generic.canonicalKey + "'";
            return false;
        }
        source.replace(
            replacement.begin, replacement.end - replacement.begin, replacement.text);
        nextBegin = replacement.begin;
    }
    return true;
}

bool buildMetadataModel(
    const frontend::ProjectCompilation& compilation,
    LibraryMetadata& metadata,
    std::string& error)
{
    if (compilation.projectGraph() == nullptr || compilation.projectGraph()->rootProject() == nullptr ||
        compilation.nameLookup() == nullptr || compilation.genericModel() == nullptr ||
        compilation.memberModel() == nullptr || compilation.inheritanceModel() == nullptr ||
        compilation.interfaceModel() == nullptr || compilation.objectLayoutModel() == nullptr ||
        compilation.oopMirModel() == nullptr)
    {
        error = "Stage 48.2 metadata requires the complete project semantic/MIR model";
        return false;
    }
    const std::optional<std::size_t> rootOrdinal = rootProjectOrdinal(compilation);
    if (!rootOrdinal.has_value())
    {
        error = "cannot identify root project ordinal for metadata";
        return false;
    }

    metadata = {};
    metadata.formatMajor = kFormatMajor;
    metadata.formatMinor = kFormatMinor;
    metadata.targetId = kWindowsX64Target;
    metadata.pointerSize = kPointerSize;
    metadata.pointerAlignment = kPointerAlignment;
    metadata.producerVersion = std::string(kCompilerVersion);
    metadata.moduleName = compilation.projectGraph()->rootProject()->moduleName();
    metadata.targetTriple = std::string(kTargetTriple);
    metadata.languageEdition = static_cast<std::uint8_t>(
        compilation.projectGraph()->rootProject()->languageEdition());

    const auto types = compilation.nameLookup()->namedTypes();
    const auto members = compilation.memberModel()->members();
    std::unordered_set<std::size_t> rootProjectTypes;
    for (std::size_t ordinal = 0; ordinal < types.size(); ++ordinal)
    {
        if (types[ordinal].projectOrdinal == *rootOrdinal) rootProjectTypes.insert(ordinal);
    }
    const auto isPubliclyReachableRootType = [&](std::size_t typeOrdinal) {
        while (typeOrdinal < types.size())
        {
            const frontend::NamedTypeDeclaration& type = types[typeOrdinal];
            if (!rootProjectTypes.contains(typeOrdinal) ||
                type.accessibility != frontend::NamedTypeAccessibility::Public)
            {
                return false;
            }
            if (!type.containingTypeOrdinal.has_value()) return true;
            typeOrdinal = *type.containingTypeOrdinal;
        }
        return false;
    };
    std::unordered_map<std::string, std::size_t> rootTypeByKey;
    for (std::size_t ordinal = 0; ordinal < types.size(); ++ordinal)
    {
        if (rootProjectTypes.contains(ordinal))
            rootTypeByKey.emplace(typeKeyForOrdinal(types, ordinal), ordinal);
    }
    std::unordered_set<std::size_t> publishedRootTypes;
    for (const std::size_t ordinal : rootProjectTypes)
        if (isPubliclyReachableRootType(ordinal)) publishedRootTypes.insert(ordinal);

    // Public signatures can expose a root-module helper whose declaration has no source-level
    // public modifier (the canonical example is an `extern C delegate`). Such a type is part of
    // the contract because a public record already names it, while an unrelated internal type is
    // not. Compute that closure before serializing records instead of publishing every root type.
    const auto includeTypeKey = [&](std::string typeKey) {
        typeKey = metadataBaseTypeKey(std::move(typeKey));
        const auto found = rootTypeByKey.find(typeKey);
        return found != rootTypeByKey.end() && publishedRootTypes.insert(found->second).second;
    };
    bool addedPublishedType = true;
    while (addedPublishedType)
    {
        addedPublishedType = false;
        const std::vector<std::size_t> current(
            publishedRootTypes.begin(), publishedRootTypes.end());
        for (const std::size_t typeOrdinal : current)
        {
            if (const frontend::ClassInheritanceInfo* inheritance =
                    compilation.inheritanceModel()->classInfo(typeOrdinal))
                addedPublishedType = includeTypeKey(inheritance->baseTypeKey) || addedPublishedType;
            if (typeOrdinal < types.size() &&
                types[typeOrdinal].kind == frontend::NamedTypeDeclarationKind::Class)
            {
                for (const std::size_t interfaceOrdinal :
                     compilation.interfaceModel()->interfacesOfClass(typeOrdinal))
                    if (rootProjectTypes.contains(interfaceOrdinal))
                        addedPublishedType = publishedRootTypes.insert(interfaceOrdinal).second ||
                            addedPublishedType;
            }
            else if (const frontend::InterfaceTypeInfo* interfaceInfo =
                         compilation.interfaceModel()->interfaceInfo(typeOrdinal))
            {
                for (const std::size_t interfaceOrdinal :
                     interfaceInfo->directBaseInterfaceOrdinals)
                    if (rootProjectTypes.contains(interfaceOrdinal))
                        addedPublishedType = publishedRootTypes.insert(interfaceOrdinal).second ||
                            addedPublishedType;
            }
            // A delegate is a nominal type whose signature owns additional type edges.  Those
            // edges are not ClassMemberSymbols and do not appear in an object layout, so walking
            // only members and fields stops one node too early.  Preserve the complete transitive
            // contract even when a delegate parameter or return type is itself a non-root helper.
            if (typeOrdinal < types.size() &&
                types[typeOrdinal].kind == frontend::NamedTypeDeclarationKind::Delegate &&
                types[typeOrdinal].syntax != nullptr)
            {
                const ast::AstNode* delegateSyntax = types[typeOrdinal].syntax;
                addedPublishedType = includeTypeKey(compilation.memberModel()->canonicalTypeKey(
                    delegateSyntax->typeSyntax(), *compilation.nameLookup())) || addedPublishedType;
                for (const ast::AstNode* parameter : delegateSyntax->children())
                {
                    if (parameter != nullptr &&
                        parameter->kind() == ast::AstNodeKind::ParameterDeclaration)
                    {
                        addedPublishedType = includeTypeKey(
                            compilation.memberModel()->canonicalTypeKey(
                                parameter->typeSyntax(), *compilation.nameLookup())) ||
                            addedPublishedType;
                    }
                }
            }
            const frontend::StructValueLayout* structLayout =
                compilation.objectLayoutModel()->structLayoutForType(typeOrdinal);
            const frontend::ClassObjectLayout* classLayout =
                compilation.objectLayoutModel()->layoutForType(typeOrdinal);
            const auto includeLayout = [&](std::span<const frontend::ObjectFieldLayout> fields) {
                for (const frontend::ObjectFieldLayout& field : fields)
                    addedPublishedType = includeTypeKey(field.typeKey) || addedPublishedType;
            };
            if (structLayout != nullptr) includeLayout(structLayout->fields);
            if (classLayout != nullptr) includeLayout(classLayout->fields);
            for (const frontend::ClassMemberSymbol& member : members)
            {
                if (member.declaringTypeOrdinal != typeOrdinal ||
                    (member.accessibility != frontend::ClassMemberAccessibility::Public &&
                     member.accessibility != frontend::ClassMemberAccessibility::Protected))
                    continue;
                addedPublishedType = includeTypeKey(member.declaredTypeKey) || addedPublishedType;
                for (const frontend::MethodParameterSymbol& parameter : member.parameters)
                    addedPublishedType = includeTypeKey(parameter.typeKey) || addedPublishedType;
            }
        }
    }
    std::vector<std::size_t> rootTypeOrdinals(
        publishedRootTypes.begin(), publishedRootTypes.end());
    std::sort(rootTypeOrdinals.begin(), rootTypeOrdinals.end(), [&](std::size_t left, std::size_t right) {
        return types[left].identity.key() < types[right].identity.key();
    });

    for (const std::size_t typeOrdinal : rootTypeOrdinals)
    {
        const frontend::NamedTypeDeclaration& type = types[typeOrdinal];
        LibraryMetadataType record;
        record.kind = metadataKind(type.kind);
        record.accessibility = static_cast<std::uint8_t>(type.accessibility);
        record.flags = typeFlags(type);
        record.identityKey = type.identity.key();
        record.fullyQualifiedName = type.identity.fullyQualifiedName();
        const std::string canonicalTypeKey = typeKeyForOrdinal(types, typeOrdinal);
        record.stableTypeId = stableTypeId(canonicalTypeKey);

        if (!type.isStatic)
        {
            if (const frontend::ClassInheritanceInfo* inheritance =
                    compilation.inheritanceModel()->classInfo(typeOrdinal))
            {
                record.baseTypeKey = inheritance->baseTypeKey;
            }
            if (type.kind == frontend::NamedTypeDeclarationKind::Class)
            {
                for (const std::size_t interfaceOrdinal :
                     compilation.interfaceModel()->interfacesOfClass(typeOrdinal))
                {
                    record.interfaceTypeKeys.push_back(
                        typeKeyForOrdinal(types, interfaceOrdinal));
                }
            }
            else if (type.kind == frontend::NamedTypeDeclarationKind::Interface)
            {
                if (const frontend::InterfaceTypeInfo* interfaceInfo =
                        compilation.interfaceModel()->interfaceInfo(typeOrdinal))
                {
                    for (const std::size_t interfaceOrdinal :
                         interfaceInfo->directBaseInterfaceOrdinals)
                    {
                        record.interfaceTypeKeys.push_back(
                            typeKeyForOrdinal(types, interfaceOrdinal));
                    }
                }
            }
        }
        std::sort(record.interfaceTypeKeys.begin(), record.interfaceTypeKeys.end());

        const frontend::StructValueLayout* structLayout = compilation.objectLayoutModel()->structLayoutForType(typeOrdinal);
        const frontend::ClassObjectLayout* classLayout = compilation.objectLayoutModel()->layoutForType(typeOrdinal);
        if (structLayout != nullptr)
        {
            record.size = structLayout->size;
            record.alignment = structLayout->alignment;
            for (const frontend::ObjectFieldLayout& fieldLayout : structLayout->fields)
            {
                LibraryMetadataField field;
                if (fieldLayout.memberOrdinal < members.size())
                {
                    const frontend::ClassMemberSymbol& member = members[fieldLayout.memberOrdinal];
                    field.name = member.name;
                    field.flags = memberFlags(member);
                }
                field.typeKey = fieldLayout.typeKey;
                if (fieldLayout.memberOrdinal < members.size())
                {
                    field.ownership = metadataOwnership(members[fieldLayout.memberOrdinal].declaredOwnership);
                }
                field.offset = fieldLayout.offset;
                field.size = fieldLayout.size;
                field.alignment = fieldLayout.alignment;
                field.bitOffset = fieldLayout.bitOffset.value_or(0);
                field.bitWidth = fieldLayout.bitWidth.value_or(0);
                record.fields.push_back(std::move(field));
            }
        }
        else if (classLayout != nullptr)
        {
            record.size = classLayout->size;
            record.alignment = classLayout->alignment;
            record.headerSize = classLayout->headerSize;
            record.referenceSize = classLayout->referenceSize;
            record.referenceAlignment = classLayout->referenceAlignment;
            for (const frontend::ObjectFieldLayout& fieldLayout : classLayout->fields)
            {
                LibraryMetadataField field;
                if (fieldLayout.memberOrdinal < members.size())
                {
                    const frontend::ClassMemberSymbol& member = members[fieldLayout.memberOrdinal];
                    field.name = member.name;
                    field.flags = memberFlags(member);
                }
                field.typeKey = fieldLayout.typeKey;
                if (fieldLayout.memberOrdinal < members.size())
                {
                    field.ownership = metadataOwnership(members[fieldLayout.memberOrdinal].declaredOwnership);
                }
                field.offset = fieldLayout.offset;
                field.size = fieldLayout.size;
                field.alignment = fieldLayout.alignment;
                field.bitOffset = fieldLayout.bitOffset.value_or(0);
                field.bitWidth = fieldLayout.bitWidth.value_or(0);
                record.fields.push_back(std::move(field));
            }
        }
        else if (type.kind == frontend::NamedTypeDeclarationKind::Enum)
        {
            // EL TAMANO DEL ENUM SALE DEL MODELO DE LAYOUT, no de un 4 escrito aqui.
            //
            // Este era el SEPTIMO sitio con "un enum mide cuatro bytes" escrito a
            // mano, y el mas caro de los que quedaban: los metadatos alimentan a los
            // generadores de host --el enlace P/Invoke de C# y el de C++/CLI-- asi
            // que un 4/4 fijo se propaga a codigo que otro compilador genera y que ya
            // nadie relaciona con este fichero. Un `enum : byte` exportado hacia que
            // el host reservara cuatro bytes donde el modulo pone uno.
            //
            // `valueLayoutForTypeKey` es la misma consulta que usan el modelo MIR y
            // el HIR, asi que los metadatos no pueden discrepar del layout que el
            // propio modulo materializa.
            const std::optional<std::pair<std::uint64_t, std::uint64_t>> enumLayout =
                compilation.objectLayoutModel()->valueLayoutForTypeKey(
                    *compilation.nameLookup(), "type:" + type.identity.key());
            record.size = enumLayout.has_value() ? enumLayout->first : 4;
            record.alignment = enumLayout.has_value() ? enumLayout->second : 4;
            // EL SIGNO, que el tamano no lleva: `enum : byte` y `enum : sbyte` miden
            // lo mismo.
            record.enumUnderlyingTypeKey =
                std::string(frontend::enumUnderlyingTypeKey(type.syntax));
        }
        else if (type.kind == frontend::NamedTypeDeclarationKind::Interface)
        {
            const target::TypeLayout referenceLayout = compilation.objectLayoutModel()->classReferenceLayout();
            record.referenceSize = referenceLayout.size;
            record.referenceAlignment = referenceLayout.alignment;
        }
        else if (type.kind == frontend::NamedTypeDeclarationKind::Delegate && type.syntax != nullptr)
        {
            if (type.syntax->hasModifier(lexer::TokenKind::KwExtern)) record.delegateFlags |= 1U;
            if (type.syntax->hasModifier(lexer::TokenKind::KwVectorCall)) record.delegateFlags |= 2U;
            // Los dos delegates viajan en un registro LLVM `ptr`. El extern C apunta
            // al codigo; el ordinario apunta a la celda ABI versionada. La diferencia
            // queda en flags y en el contrato raiz, pero tamaño/alineacion de la
            // REFERENCIA son iguales.
            record.referenceSize = kPointerSize;
            record.referenceAlignment = kPointerAlignment;
            record.delegateReturnTypeKey =
                compilation.memberModel()->canonicalTypeKey(type.syntax->typeSyntax(), *compilation.nameLookup());
            for (const ast::AstNode* parameter : type.syntax->children())
            {
                if (parameter != nullptr && parameter->kind() == ast::AstNodeKind::ParameterDeclaration)
                {
                    record.delegateParameterTypeKeys.push_back(
                        compilation.memberModel()->canonicalTypeKey(parameter->typeSyntax(), *compilation.nameLookup()));
                }
            }
        }
        metadata.types.push_back(std::move(record));
    }

    std::unordered_set<std::size_t> rootTypes(rootTypeOrdinals.begin(), rootTypeOrdinals.end());
    std::unordered_map<std::size_t, const frontend::ProjectOopMirStaticField*> staticFields;
    for (const frontend::ProjectOopMirStaticField& field : compilation.oopMirModel()->staticFields())
    {
        staticFields.emplace(field.memberOrdinal, &field);
    }
    std::vector<std::size_t> memberOrdinals;
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
    {
        const frontend::ClassMemberSymbol& member = members[ordinal];
        const bool contractAccessibility =
            member.accessibility == frontend::ClassMemberAccessibility::Public ||
            member.accessibility == frontend::ClassMemberAccessibility::Protected;
        if (contractAccessibility &&
            isPubliclyReachableRootType(member.declaringTypeOrdinal))
        {
            memberOrdinals.push_back(ordinal);
        }
    }
    std::sort(memberOrdinals.begin(), memberOrdinals.end(), [&](std::size_t left, std::size_t right) {
        const frontend::ClassMemberSymbol& a = members[left];
        const frontend::ClassMemberSymbol& b = members[right];
        const std::string aType = typeKeyForOrdinal(types, a.declaringTypeOrdinal);
        const std::string bType = typeKeyForOrdinal(types, b.declaringTypeOrdinal);
        if (aType != bType) return aType < bType;
        if (a.signatureKey != b.signatureKey) return a.signatureKey < b.signatureKey;
        return left < right;
    });

    std::vector<std::string> missingPhysicalCallables;
    for (const std::size_t ordinal : memberOrdinals)
    {
        const frontend::ClassMemberSymbol& member = members[ordinal];
        LibraryMetadataMember record;
        record.kind = static_cast<std::uint8_t>(member.kind);
        record.accessibility = static_cast<std::uint8_t>(member.accessibility);
        record.flags = memberFlags(member);
        record.declaringTypeKey = typeKeyForOrdinal(types, member.declaringTypeOrdinal);
        record.name = member.name;
        record.signatureKey = member.signatureKey;
        record.declaredTypeKey = member.declaredTypeKey;
        record.declaredOwnership = metadataOwnership(member.declaredOwnership);
        record.cAbiName = member.cAbiName;
        record.importDllName = member.importDllName;
        const frontend::ProjectOopMirFunction* function =
            compilation.oopMirModel()->functionForMember(ordinal);
        if (function != nullptr)
        {
            record.mangledName = function->mangledName;
        }
        if (metadata.formatMinor >= 13 && member.kind == frontend::ClassMemberKind::Method &&
            member.syntax != nullptr && function != nullptr &&
            function->genericSpecializationKey.empty() &&
            (member.inliningPolicy == frontend::InliningPolicy::Preferred ||
             member.inliningPolicy == frontend::InliningPolicy::Required))
        {
            record.inlineSourceText =
                std::string(compilation.sourceManager().getText(member.syntax->range()));
        }
        const auto staticField = staticFields.find(ordinal);
        if (staticField != staticFields.end() && !staticField->second->isConst)
        {
            record.mangledName = staticField->second->accessorName;
        }
        const bool callableAccessibility =
            member.accessibility == frontend::ClassMemberAccessibility::Public ||
            member.accessibility == frontend::ClassMemberAccessibility::Protected;
        const bool nativeCallable =
            (member.kind == frontend::ClassMemberKind::Method ||
             member.kind == frontend::ClassMemberKind::Constructor) &&
            callableAccessibility && isPubliclyReachableRootType(member.declaringTypeOrdinal) &&
            !member.isAbstract && !member.isCompilerGenerated && !member.isImportedC &&
            (!member.isExternC || member.isExportedC);
        if (nativeCallable && record.mangledName.empty())
        {
            missingPhysicalCallables.push_back(
                record.declaringTypeKey + "." + record.signatureKey);
        }
        for (const frontend::MethodParameterSymbol& parameter : member.parameters)
        {
            LibraryMetadataParameter parameterRecord;
            parameterRecord.name = parameter.name;
            parameterRecord.typeKey = parameter.typeKey;
            parameterRecord.ownership = metadataOwnership(parameter.ownership);
            parameterRecord.passKind = metadataPassKind(parameter.passKind);
            parameterRecord.isParameterPack = parameter.isParameterPack;
            if (!metadataDefaultValue(parameter.defaultValueSyntax, parameterRecord, error))
            {
                error = "could not encode default value for parameter '" + parameter.name +
                    "' of member '" + member.name + "': " + error;
                return false;
            }
            record.parameters.push_back(std::move(parameterRecord));
        }
        if (staticField != staticFields.end() && staticField->second->isConst && staticField->second->hasConstantInitializer)
        {
            const frontend::ProjectOopMirStaticField& field = *staticField->second;
            if (field.typeKey == "builtin:bool")
            {
                record.constantKind = 3;
                record.constantBits = field.constantBooleanValue ? 1U : 0U;
            }
            else if (field.typeKey == "builtin:float")
            {
                record.constantKind = 4;
                record.constantBits = std::bit_cast<std::uint32_t>(static_cast<float>(field.constantFloatingValue));
            }
            else if (field.typeKey == "builtin:double")
            {
                record.constantKind = 5;
                record.constantBits = std::bit_cast<std::uint64_t>(field.constantFloatingValue);
            }
            else if (field.typeKey == "builtin:string")
            {
                record.constantKind = 6;
                record.constantText = field.constantStringValue;
            }
            else
            {
                record.constantKind = 1;
                record.constantBits = field.constantIntegerValue;
            }
        }
        metadata.members.push_back(std::move(record));
    }

    if (!missingPhysicalCallables.empty())
    {
        error = "public callables have no physical identity; refusing to emit unusable NSMETA";
        for (const std::string& callable : missingPhysicalCallables)
        {
            error += "\n  " + callable;
        }
        return false;
    }
    if (compilation.namespaceFunctionModel() != nullptr)
    {
        for (const frontend::NamespaceFunctionSymbol& function :
             compilation.namespaceFunctionModel()->functions())
        {
            if (function.projectOrdinal != *rootOrdinal ||
                function.accessibility != frontend::NamespaceFunctionAccessibility::Public ||
                (function.isExternC && !function.isExportedC))
            {
                continue;
            }
            if (function.mangledName.empty())
            {
                error = "public namespace function '" + function.namespaceName + "." +
                    function.signatureKey + "' has no physical identity";
                return false;
            }
            LibraryMetadataNamespaceFunction record;
            record.accessibility = 1U;
            if (function.isReferenceReturn) record.flags |= 1U;
            if (function.isVariadic) record.flags |= 2U;
            if (function.isExternC) record.flags |= 4U;
            if (function.isExportedC) record.flags |= 8U;
            record.namespaceName = function.namespaceName;
            record.name = function.name;
            record.signatureKey = function.signatureKey;
            record.declaredTypeKey = function.declaredTypeKey;
            record.declaredOwnership = metadataOwnership(function.declaredOwnership);
            record.mangledName = function.mangledName;
            for (const frontend::MethodParameterSymbol& parameter : function.parameters)
            {
                LibraryMetadataParameter parameterRecord;
                parameterRecord.name = parameter.name;
                parameterRecord.typeKey = parameter.typeKey;
                parameterRecord.ownership = metadataOwnership(parameter.ownership);
                parameterRecord.passKind = metadataPassKind(parameter.passKind);
                parameterRecord.isParameterPack = parameter.isParameterPack;
                if (!metadataDefaultValue(parameter.defaultValueSyntax, parameterRecord, error))
                {
                    error = "could not encode default value for parameter '" + parameter.name +
                        "' of namespace function '" + function.name + "': " + error;
                    return false;
                }
                record.parameters.push_back(std::move(parameterRecord));
            }
            metadata.namespaceFunctions.push_back(std::move(record));
        }
        std::sort(
            metadata.namespaceFunctions.begin(),
            metadata.namespaceFunctions.end(),
            [](const auto& left, const auto& right)
            {
                return std::tie(left.namespaceName, left.signatureKey) <
                    std::tie(right.namespaceName, right.signatureKey);
            });
    }
    if (compilation.namespaceFunctionModel() != nullptr)
    {
        for (const frontend::NamespaceStateSymbol& state :
             compilation.namespaceFunctionModel()->states())
        {
            if (state.projectOrdinal != *rootOrdinal ||
                state.accessibility != frontend::NamespaceFunctionAccessibility::Public ||
                state.isPrecompiled)
            {
                continue;
            }
            LibraryMetadataNamespaceState record;
            record.accessibility = 1U;
            if (state.isConst) record.flags |= 1U;
            record.initializerKind =
                static_cast<LibraryMetadataNamespaceStateInitializerKind>(
                    state.initializerKind);
            record.namespaceName = state.namespaceName;
            record.name = state.name;
            record.declaredTypeKey = state.declaredTypeKey;
            record.storageName = state.storageName;
            record.accessorName = "__nsc_get_" + state.storageName;
            if (state.initializerKind == frontend::NamespaceStateInitializerKind::Integer)
                record.initializerBits = state.integerValue;
            else if (state.initializerKind == frontend::NamespaceStateInitializerKind::Floating)
                record.initializerBits = std::bit_cast<std::uint64_t>(state.floatingValue);
            else if (state.initializerKind == frontend::NamespaceStateInitializerKind::Boolean)
                record.initializerBits = state.booleanValue ? 1U : 0U;
            else if (state.initializerKind == frontend::NamespaceStateInitializerKind::String)
                record.initializerText = state.stringValue;
            metadata.namespaceStates.push_back(std::move(record));
        }
        std::sort(
            metadata.namespaceStates.begin(), metadata.namespaceStates.end(),
            [](const auto& left, const auto& right)
            {
                return std::tie(left.namespaceName, left.name) <
                    std::tie(right.namespaceName, right.name);
            });
    }
    std::unordered_set<std::string> templateExternalTypeKeys;

    for (const frontend::ProjectOopMirFunction& function : compilation.oopMirModel()->functions())
    {
        if (!function.isSyntheticImplicitConstructor ||
            !rootTypes.contains(function.declaringTypeOrdinal) ||
            function.declaringTypeOrdinal >= types.size())
        {
            continue;
        }
        const frontend::NamedTypeDeclaration& type = types[function.declaringTypeOrdinal];
        if (type.accessibility != frontend::NamedTypeAccessibility::Public) continue;
        const std::string fullyQualifiedName = type.identity.fullyQualifiedName();
        const std::size_t separator = fullyQualifiedName.find_last_of(".+");
        const std::string typeName = separator == std::string::npos
            ? fullyQualifiedName
            : fullyQualifiedName.substr(separator + 1U);
        LibraryMetadataMember record;
        record.kind = static_cast<std::uint8_t>(frontend::ClassMemberKind::Constructor);
        record.accessibility = static_cast<std::uint8_t>(frontend::ClassMemberAccessibility::Public);
        record.declaringTypeKey = "type:" + type.identity.key();
        record.name = typeName;
        record.signatureKey = typeName + "()";
        record.declaredTypeKey = "builtin:void";
        record.mangledName = function.mangledName;
        metadata.members.push_back(std::move(record));
    }

    const auto genericTemplates = compilation.genericModel()->templates();
    std::unordered_map<std::string, const frontend::GenericTemplateRecord*> genericByKey;
    for (const frontend::GenericTemplateRecord& generic : genericTemplates)
        genericByKey.emplace(generic.canonicalKey, &generic);

    std::unordered_set<std::string> rootTemplateKeys;
    std::unordered_set<std::string> includedTemplateKeys;
    for (const frontend::GenericTemplateRecord& publicRoot : genericTemplates)
    {
        if (publicRoot.projectOrdinal != *rootOrdinal ||
            publicRoot.accessibility != frontend::GenericTemplateAccessibility::Public)
            continue;

        std::vector<std::string> chain;
        std::unordered_set<std::string> visiting;
        std::function<bool(const frontend::GenericTemplateRecord&)> includeClosure =
            [&](const frontend::GenericTemplateRecord& generic)
        {
            if (generic.projectOrdinal != *rootOrdinal) return true;
            if (includedTemplateKeys.contains(generic.canonicalKey)) return true;
            if (!visiting.insert(generic.canonicalKey).second) return true;
            chain.push_back(generic.canonicalKey);

            if (!generic.unresolvedDependencies.empty())
            {
                error = "reusable template closure is incomplete\n  public root: " +
                    publicRoot.canonicalKey + "\n  required template: " +
                    generic.unresolvedDependencies.front() + "\n  owner: " +
                    generic.moduleName + "::" + generic.ownerName +
                    "\n  dependency chain: ";
                for (std::size_t index = 0; index < chain.size(); ++index)
                {
                    if (index != 0) error += " -> ";
                    error += chain[index];
                }
                return false;
            }

            for (const std::string& dependencyKey : generic.dependencies)
            {
                const auto dependency = genericByKey.find(dependencyKey);
                if (dependency == genericByKey.end())
                {
                    error = "reusable template closure is incomplete\n  public root: " +
                        publicRoot.canonicalKey + "\n  required template: " + dependencyKey +
                        "\n  owner: " + generic.moduleName + "::" + generic.ownerName +
                        "\n  dependency chain: ";
                    for (const std::string& entry : chain) error += entry + " -> ";
                    error += dependencyKey;
                    return false;
                }
                if (!includeClosure(*dependency->second)) return false;
            }
            chain.pop_back();
            visiting.erase(generic.canonicalKey);
            includedTemplateKeys.insert(generic.canonicalKey);
            return true;
        };
        if (!includeClosure(publicRoot)) return false;
    }

    for (const frontend::GenericTemplateRecord& generic : genericTemplates)
    {
        if (!includedTemplateKeys.contains(generic.canonicalKey) || generic.syntax == nullptr)
            continue;
        LibraryMetadataGenericTemplate record;
        record.kind = static_cast<std::uint8_t>(generic.kind);
        record.accessibility = static_cast<std::uint8_t>(generic.accessibility);
        record.canonicalKey = generic.canonicalKey;
        record.moduleName = generic.moduleName;
        record.namespaceName = generic.namespaceName;
        record.ownerName = generic.ownerName;
        record.name = generic.name;
        record.parameters = generic.parameters;
        if (!materializeGenericTemplateSource(
                compilation, generic, record.sourceText, templateExternalTypeKeys, error))
        {
            return false;
        }
        record.dependencies = generic.dependencies;
        rootTemplateKeys.insert(record.canonicalKey);
        metadata.genericTemplates.push_back(std::move(record));
    }
    std::sort(metadata.genericTemplates.begin(), metadata.genericTemplates.end(), [](const auto& left, const auto& right) {
        return left.canonicalKey < right.canonicalKey;
    });

    // A closed type instantiated from another module is still part of this producer's contract
    // when one of the producer's declarations names it.  The old filter below kept only
    // specializations whose OPEN template belonged to the root project.  Consequently a field
    // such as List<ProducerValue> carried the generated nominal type key in `types`/`members`, but
    // NSMETA omitted the record that tells a consumer how to recreate that nominal declaration.
    std::unordered_set<std::string> referencedTypeKeys;
    const auto rememberType = [&](const std::string& typeKey) {
        const std::string base = metadataBaseTypeKey(typeKey);
        if (base.starts_with("type:")) referencedTypeKeys.insert(base);
    };
    for (const LibraryMetadataType& type : metadata.types)
    {
        rememberType(type.baseTypeKey);
        for (const std::string& interfaceTypeKey : type.interfaceTypeKeys) rememberType(interfaceTypeKey);
        for (const LibraryMetadataField& field : type.fields) rememberType(field.typeKey);
        rememberType(type.delegateReturnTypeKey);
        for (const std::string& parameterTypeKey : type.delegateParameterTypeKeys) rememberType(parameterTypeKey);
    }
    for (const LibraryMetadataMember& member : metadata.members)
    {
        rememberType(member.declaredTypeKey);
        for (const LibraryMetadataParameter& parameter : member.parameters) rememberType(parameter.typeKey);
    }

    for (const LibraryMetadataNamespaceFunction& function :
         metadata.namespaceFunctions)
    {
        rememberType(function.declaredTypeKey);
        for (const LibraryMetadataParameter& parameter : function.parameters)
            rememberType(parameter.typeKey);
    }
    for (const LibraryMetadataNamespaceState& state : metadata.namespaceStates)
        rememberType(state.declaredTypeKey);

    std::unordered_set<std::string> requiredSpecializationKeys;
    for (const frontend::GenericSpecializationRecord& specialization : compilation.genericModel()->specializations())
    {
        const std::optional<std::string> typeKey = specializedTypeKey(specialization);
        if (typeKey.has_value() && referencedTypeKeys.contains(*typeKey))
            requiredSpecializationKeys.insert(specialization.specializationKey);
    }
    // A closed generic can itself be a type argument.  Preserve that dependency transitively so
    // the consumer never receives an outer demand whose inner identity cannot be materialized.
    bool addedDependency = true;
    while (addedDependency)
    {
        addedDependency = false;
        for (const frontend::GenericSpecializationRecord& specialization : compilation.genericModel()->specializations())
        {
            if (!requiredSpecializationKeys.contains(specialization.specializationKey)) continue;
            for (const std::string& argument : specialization.typeArguments)
            {
                if (!argument.starts_with("generic:")) continue;
                const std::string dependencyKey = argument.substr(8U);
                if (requiredSpecializationKeys.insert(dependencyKey).second) addedDependency = true;
            }
        }
    }
    std::unordered_set<std::string> externalTypeKeys(
        templateExternalTypeKeys.begin(), templateExternalTypeKeys.end());
    std::unordered_set<std::string> emittedSpecializedTypeKeys;
    for (const frontend::GenericSpecializationRecord& specialization : compilation.genericModel()->specializations())
    {
        if (!rootTemplateKeys.contains(specialization.templateKey) &&
            !requiredSpecializationKeys.contains(specialization.specializationKey))
        {
            continue;
        }
        const std::optional<std::string> typeKey = specializedTypeKey(specialization);
        // A cache seed with no specialized syntax is code already supplied by a
        // LibraryReference.  Referencing it does not transfer ownership to this
        // producer.  Record the exact provider-owned nominal key instead of
        // duplicating its specialization declaration in this NSMETA.
        if (!rootTemplateKeys.contains(specialization.templateKey) &&
            specialization.specializedSyntax == nullptr && typeKey.has_value())
        {
            externalTypeKeys.insert(*typeKey);
            continue;
        }
        LibraryMetadataGenericSpecialization record;
        record.kind = static_cast<std::uint8_t>(specialization.kind);
        record.templateKey = specialization.templateKey;
        record.specializationKey = specialization.specializationKey;
        record.specializedName = specialization.specializedName;
        record.typeArguments = specialization.typeArguments;
        metadata.genericSpecializations.push_back(std::move(record));
        if (typeKey.has_value()) emittedSpecializedTypeKeys.insert(*typeKey);
    }
    std::sort(metadata.genericSpecializations.begin(), metadata.genericSpecializations.end(), [](const auto& left, const auto& right) {
        return left.specializationKey < right.specializationKey;
    });

    std::unordered_set<std::string> declaredTypeKeys;
    for (const LibraryMetadataType& type : metadata.types)
        declaredTypeKeys.insert("type:" + type.identityKey);
    for (const std::string& typeKey : referencedTypeKeys)
    {
        if (declaredTypeKeys.contains(typeKey) || emittedSpecializedTypeKeys.contains(typeKey)) continue;
        const std::optional<std::string> module = metadataTypeModule(typeKey);
        if (module.has_value() && *module != metadata.moduleName) externalTypeKeys.insert(typeKey);
    }
    // Type arguments are part of a specialization's public identity even when
    // no ordinary field or callable names them directly.
    for (const LibraryMetadataGenericSpecialization& specialization : metadata.genericSpecializations)
    {
        for (const std::string& argument : specialization.typeArguments)
        {
            if (!argument.starts_with("type:")) continue;
            const std::string typeKey = metadataBaseTypeKey(argument);
            if (declaredTypeKeys.contains(typeKey) || emittedSpecializedTypeKeys.contains(typeKey)) continue;
            const std::optional<std::string> module = metadataTypeModule(typeKey);
            if (module.has_value() && *module != metadata.moduleName) externalTypeKeys.insert(typeKey);
        }
    }
    for (const frontend::ProjectOopMirDispatchTable& table : compilation.oopMirModel()->dispatchTables())
    {
        if (!isPubliclyReachableRootType(table.typeOrdinal)) continue;
        LibraryMetadataDispatch record;
        record.typeKey = table.typeKey;
        record.stableTypeId = table.stableTypeId;
        record.descriptorName = table.descriptorName;
        record.descriptorAccessorName = table.descriptorAccessorName;
        record.vtableName = table.vtableName;
        record.destructionEntryName = table.destructionEntryName;
        for (std::size_t index = 0; index < table.slotMemberOrdinals.size(); ++index)
        {
            record.slotSignatures.push_back(signatureForMember(members, table.slotMemberOrdinals[index]));
            record.slotCalleeNames.push_back(index < table.slotCalleeNames.size() ? table.slotCalleeNames[index] : std::string{});
        }
        for (const frontend::ProjectOopMirDispatchTable::InterfaceTable& tableInterface : table.interfaceTables)
        {
            LibraryMetadataDispatchInterface interfaceRecord;
            interfaceRecord.interfaceTypeKey = tableInterface.interfaceTypeKey;
            interfaceRecord.stableInterfaceTypeId = tableInterface.stableInterfaceTypeId;
            interfaceRecord.methodTableName = tableInterface.methodTableName;
            for (const std::size_t memberOrdinal : tableInterface.interfaceMemberOrdinals)
            {
                interfaceRecord.interfaceSignatures.push_back(signatureForMember(members, memberOrdinal));
            }
            for (const std::size_t memberOrdinal : tableInterface.implementationMemberOrdinals)
            {
                interfaceRecord.implementationSignatures.push_back(signatureForMember(members, memberOrdinal));
            }
            interfaceRecord.calleeNames = tableInterface.calleeNames;
            record.interfaces.push_back(std::move(interfaceRecord));
        }
        std::sort(record.interfaces.begin(), record.interfaces.end(), [](const auto& left, const auto& right) {
            return left.interfaceTypeKey < right.interfaceTypeKey;
        });
        metadata.dispatchTables.push_back(std::move(record));
    }
    std::sort(metadata.dispatchTables.begin(), metadata.dispatchTables.end(), [](const auto& left, const auto& right) {
        return left.typeKey < right.typeKey;
    });

    for (const frontend::ProjectOopMirFunction& function : compilation.oopMirModel()->functions())
    {
        if (function.isLambda || !function.isExportedC ||
            !isPubliclyReachableRootType(function.declaringTypeOrdinal) ||
            function.memberOrdinal >= members.size())
        {
            continue;
        }
        const frontend::ClassMemberSymbol& member = members[function.memberOrdinal];
        LibraryMetadataExport record;
        record.cAbiName = member.cAbiName;
        record.declaringTypeKey = typeKeyForOrdinal(types, function.declaringTypeOrdinal);
        record.returnTypeKey = function.returnTypeKey;
        record.parameterTypeKeys = function.parameterTypeKeys;
        record.nativeMangledName = function.mangledName;
        metadata.exports.push_back(std::move(record));
    }
    std::sort(metadata.exports.begin(), metadata.exports.end(), [](const auto& left, const auto& right) {
        return left.cAbiName < right.cAbiName;
    });
    const auto rememberExternalType = [&](const std::string& writtenTypeKey) {
        const std::string typeKey = metadataBaseTypeKey(writtenTypeKey);
        if (declaredTypeKeys.contains(typeKey) || emittedSpecializedTypeKeys.contains(typeKey)) return;
        const std::optional<std::string> module = metadataTypeModule(typeKey);
        if (module.has_value() && *module != metadata.moduleName) externalTypeKeys.insert(typeKey);
    };
    for (const LibraryMetadataDispatch& dispatch : metadata.dispatchTables)
    {
        rememberExternalType(dispatch.typeKey);
        for (const LibraryMetadataDispatchInterface& interface : dispatch.interfaces)
            rememberExternalType(interface.interfaceTypeKey);
    }
    for (const LibraryMetadataExport& exportRecord : metadata.exports)
    {
        rememberExternalType(exportRecord.declaringTypeKey);
        rememberExternalType(exportRecord.returnTypeKey);
        for (const std::string& parameterTypeKey : exportRecord.parameterTypeKeys)
            rememberExternalType(parameterTypeKey);
    }
    metadata.externalTypeKeys.assign(externalTypeKeys.begin(), externalTypeKeys.end());
    std::sort(metadata.externalTypeKeys.begin(), metadata.externalTypeKeys.end());

    // NSMETA 1.25: retain the direct binary closure of the produced archive. Every referenced
    // metadata file carries its own direct edges, so consumers can expand this recursively without
    // restore or source enumeration. Store leaf artifact names and certify the exact ABI contract;
    // installation may relocate a package, but it may not substitute an incompatible module.
    if (compilation.projectGraph() != nullptr && compilation.projectGraph()->rootProject() != nullptr)
    {
        LibraryMetadataCodec codec;
        for (const compilation::LibraryReference& reference :
             compilation.projectGraph()->rootProject()->libraryReferences())
        {
            LibraryMetadata dependencyMetadata;
            std::string dependencyError;
            if (!codec.read(reference.metadataPath, dependencyMetadata, dependencyError) ||
                !codec.verifyCurrentCompiler(dependencyMetadata, dependencyError))
            {
                error = "cannot record binary dependency '" +
                    reference.metadataPath.generic_string() + "': " + dependencyError;
                return false;
            }
            LibraryMetadataBinaryDependency dependency;
            dependency.moduleName = dependencyMetadata.moduleName;
            dependency.libraryFileName = reference.libraryPath.filename().generic_string();
            dependency.metadataFileName = reference.metadataPath.filename().generic_string();
            dependency.runtimeFileName = reference.runtimePath.empty()
                ? std::string{} : reference.runtimePath.filename().generic_string();
            dependency.abiFingerprint = dependencyMetadata.abiFingerprint;
            metadata.binaryDependencies.push_back(std::move(dependency));
        }
        std::sort(metadata.binaryDependencies.begin(), metadata.binaryDependencies.end(),
            [](const auto& left, const auto& right) {
                return std::tie(left.moduleName, left.metadataFileName) <
                    std::tie(right.moduleName, right.metadataFileName);
            });
    }

    std::unordered_set<std::string> referencedCallees;
    const auto collectCallees = [&](const frontend::ProjectOopMirNode* node, auto&& self) -> void
    {
        if (node == nullptr) return;
        if (!node->calleeName.empty()) referencedCallees.insert(node->calleeName);
        for (const auto& child : node->children) self(child.get(), self);
    };
    for (const frontend::ProjectOopMirFunction& function : compilation.oopMirModel()->functions())
        collectCallees(function.body.get(), collectCallees);
    bool needsRuntimeBinding = false;
    for (const frontend::ProjectOopMirFunction& function : compilation.oopMirModel()->functions())
    {
        if (!function.isImportedC || function.importDllName.empty() ||
            !referencedCallees.contains(function.mangledName))
            continue;
        if (function.importRequiresRuntimeBinding)
        {
            needsRuntimeBinding = true;
            continue;
        }
        metadata.platformImports.push_back(
            LibraryMetadataPlatformImport{function.importDllName, function.cAbiName});
    }
    if (needsRuntimeBinding)
    {
        metadata.platformImports.push_back({"KERNEL32.dll", "LoadLibraryA"});
        metadata.platformImports.push_back({"KERNEL32.dll", "GetProcAddress"});
    }
    std::sort(metadata.platformImports.begin(), metadata.platformImports.end(),
        [](const auto& left, const auto& right) {
            return std::tie(left.dllName, left.symbolName) <
                std::tie(right.dllName, right.symbolName);
        });
    metadata.platformImports.erase(
        std::unique(metadata.platformImports.begin(), metadata.platformImports.end(),
            [](const auto& left, const auto& right) {
                return left.dllName == right.dllName && left.symbolName == right.symbolName;
            }),
        metadata.platformImports.end());
    return true;
}

void writeStringVector(ByteWriter& writer, const std::vector<std::string>& values)
{
    writer.u32(static_cast<std::uint32_t>(values.size()));
    for (const std::string& value : values) writer.text(value);
}

bool readStringVector(ByteReader& reader, std::vector<std::string>& values)
{
    std::uint32_t count = 0;
    if (!reader.u32(count) || count > kMaximumRecordCount) return false;
    values.clear();
    values.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        std::string value;
        if (!reader.text(value)) return false;
        values.push_back(std::move(value));
    }
    return true;
}

std::vector<std::uint8_t> encodePayload(const LibraryMetadata& metadata)
{
    ByteWriter writer;
    writer.text(metadata.producerVersion);
    writer.text(metadata.moduleName);
    writer.text(metadata.targetTriple);
    if (metadata.formatMinor >= 23) writer.u8(metadata.languageEdition);
    if (metadata.formatMinor >= 8)
    {
        writer.u16(metadata.nativeAbiMajor);
        writer.u16(metadata.nativeAbiMinor);
        writer.u16(metadata.runtimeAbiMajor);
        writer.u16(metadata.runtimeAbiMinor);
        writer.u16(metadata.manglingAbiVersion);
        writer.u16(metadata.objectAbiVersion);
        writer.u16(metadata.delegateAbiVersion);
        writer.u16(metadata.exceptionAbiVersion);
        writer.u32(metadata.managedDelegateCellSize);
    }

    writer.u32(static_cast<std::uint32_t>(metadata.types.size()));
    for (const LibraryMetadataType& type : metadata.types)
    {
        writer.u8(static_cast<std::uint8_t>(type.kind));
        writer.u8(type.accessibility);
        writer.u16(type.flags);
        writer.text(type.identityKey);
        writer.text(type.fullyQualifiedName);
        writer.text(type.baseTypeKey);
        writeStringVector(writer, type.interfaceTypeKeys);
        writer.u64(type.size);
        writer.u64(type.alignment);
        writer.u64(type.headerSize);
        writer.u64(type.referenceSize);
        writer.u64(type.referenceAlignment);
        writer.u64(type.stableTypeId);
        // Minor 2. Un lector de minor 1 no lo espera, asi que se escribe solo
        // cuando el formato que se esta produciendo lo incluye -- la misma
        // condicion que ya usa `ownership`.
        if (metadata.formatMinor >= 2) writer.text(type.enumUnderlyingTypeKey);
        if (metadata.formatMinor >= 5)
        {
            writer.u8(type.delegateFlags);
            writer.text(type.delegateReturnTypeKey);
            writeStringVector(writer, type.delegateParameterTypeKeys);
        }
        writer.u32(static_cast<std::uint32_t>(type.fields.size()));
        for (const LibraryMetadataField& field : type.fields)
        {
            writer.text(field.name);
            writer.text(field.typeKey);
            if (metadata.formatMinor >= 1)
            {
                writer.u8(static_cast<std::uint8_t>(field.ownership));
            }
            writer.u64(field.offset);
            writer.u64(field.size);
            writer.u64(field.alignment);
            writer.u32(field.flags);
            if (metadata.formatMinor >= 18)
            {
                writer.u64(field.bitOffset);
                writer.u64(field.bitWidth);
            }
        }
    }

    writer.u32(static_cast<std::uint32_t>(metadata.members.size()));
    for (const LibraryMetadataMember& member : metadata.members)
    {
        writer.u8(member.kind);
        writer.u8(member.accessibility);
        writer.u32(member.flags);
        writer.text(member.declaringTypeKey);
        writer.text(member.name);
        writer.text(member.signatureKey);
        writer.text(member.declaredTypeKey);
        if (metadata.formatMinor >= 1)
        {
            writer.u8(static_cast<std::uint8_t>(member.declaredOwnership));
        }
        writer.text(member.cAbiName);
        writer.text(member.mangledName);
        writer.u32(static_cast<std::uint32_t>(member.parameters.size()));
        for (const LibraryMetadataParameter& parameter : member.parameters)
        {
            writer.text(parameter.name);
            writer.text(parameter.typeKey);
            if (metadata.formatMinor >= 1)
            {
                writer.u8(static_cast<std::uint8_t>(parameter.ownership));
            }
            if (metadata.formatMinor >= 4)
            {
                writer.u8(static_cast<std::uint8_t>(parameter.passKind));
            }
            if (metadata.formatMinor >= 24)
            {
                writer.u8(parameter.isParameterPack ? 1U : 0U);
            }
            if (metadata.formatMinor >= 3)
            {
                writer.u8(static_cast<std::uint8_t>(parameter.defaultValueKind));
                writer.u8(parameter.defaultValueFlags);
                writer.u64(parameter.defaultValueBits);
                writer.text(parameter.defaultValueText);
            }
        }
        writer.u8(member.constantKind);
        writer.u64(member.constantBits);
        if (metadata.formatMinor >= 9) writer.text(member.constantText);
        if (metadata.formatMinor >= 10) writer.text(member.importDllName);
        if (metadata.formatMinor >= 13) writer.text(member.inlineSourceText);
    }

    writer.u32(static_cast<std::uint32_t>(metadata.genericTemplates.size()));
    for (const LibraryMetadataGenericTemplate& generic : metadata.genericTemplates)
    {
        writer.u8(generic.kind);
        if (metadata.formatMinor >= 7) writer.u8(generic.accessibility);
        writer.text(generic.canonicalKey);
        writer.text(generic.moduleName);
        writer.text(generic.namespaceName);
        writer.text(generic.ownerName);
        writer.text(generic.name);
        writeStringVector(writer, generic.parameters);
        if (metadata.formatMinor >= 7) writer.text(generic.sourceText);
        if (metadata.formatMinor >= 11) writeStringVector(writer, generic.dependencies);
    }

    writer.u32(static_cast<std::uint32_t>(metadata.genericSpecializations.size()));
    for (const LibraryMetadataGenericSpecialization& specialization : metadata.genericSpecializations)
    {
        writer.u8(specialization.kind);
        writer.text(specialization.templateKey);
        writer.text(specialization.specializationKey);
        writer.text(specialization.specializedName);
        writeStringVector(writer, specialization.typeArguments);
    }
    if (metadata.formatMinor >= 12) writeStringVector(writer, metadata.externalTypeKeys);

    writer.u32(static_cast<std::uint32_t>(metadata.dispatchTables.size()));
    for (const LibraryMetadataDispatch& dispatch : metadata.dispatchTables)
    {
        writer.text(dispatch.typeKey);
        writer.u64(dispatch.stableTypeId);
        writer.text(dispatch.descriptorName);
        if (metadata.formatMinor >= 6) writer.text(dispatch.descriptorAccessorName);
        writer.text(dispatch.vtableName);
        writer.text(dispatch.destructionEntryName);
        writeStringVector(writer, dispatch.slotSignatures);
        writeStringVector(writer, dispatch.slotCalleeNames);
        writer.u32(static_cast<std::uint32_t>(dispatch.interfaces.size()));
        for (const LibraryMetadataDispatchInterface& interfaceRecord : dispatch.interfaces)
        {
            writer.text(interfaceRecord.interfaceTypeKey);
            writer.u64(interfaceRecord.stableInterfaceTypeId);
            writer.text(interfaceRecord.methodTableName);
            writeStringVector(writer, interfaceRecord.interfaceSignatures);
            writeStringVector(writer, interfaceRecord.implementationSignatures);
            writeStringVector(writer, interfaceRecord.calleeNames);
        }
    }

    writer.u32(static_cast<std::uint32_t>(metadata.exports.size()));
    for (const LibraryMetadataExport& exportRecord : metadata.exports)
    {
        writer.text(exportRecord.cAbiName);
        writer.text(exportRecord.declaringTypeKey);
        writer.text(exportRecord.returnTypeKey);
        writeStringVector(writer, exportRecord.parameterTypeKeys);
        writer.text(exportRecord.nativeMangledName);
    }
    if (metadata.formatMinor >= 21)
    {
        writer.u32(static_cast<std::uint32_t>(metadata.namespaceFunctions.size()));
        for (const LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
        {
            writer.u8(function.accessibility);
            writer.u8(function.flags);
            writer.text(function.namespaceName);
            writer.text(function.name);
            writer.text(function.signatureKey);
            writer.text(function.declaredTypeKey);
            writer.u8(static_cast<std::uint8_t>(function.declaredOwnership));
            writer.text(function.mangledName);
            writer.u32(static_cast<std::uint32_t>(function.parameters.size()));
            for (const LibraryMetadataParameter& parameter : function.parameters)
            {
                writer.text(parameter.name);
                writer.text(parameter.typeKey);
                writer.u8(static_cast<std::uint8_t>(parameter.ownership));
                writer.u8(static_cast<std::uint8_t>(parameter.passKind));
                if (metadata.formatMinor >= 24)
                    writer.u8(parameter.isParameterPack ? 1U : 0U);
                writer.u8(static_cast<std::uint8_t>(parameter.defaultValueKind));
                writer.u8(parameter.defaultValueFlags);
                writer.u64(parameter.defaultValueBits);
                writer.text(parameter.defaultValueText);
            }
        }
    }
    if (metadata.formatMinor >= 22)
    {
        writer.u32(static_cast<std::uint32_t>(metadata.namespaceStates.size()));
        for (const LibraryMetadataNamespaceState& state : metadata.namespaceStates)
        {
            writer.u8(state.accessibility);
            writer.u8(state.flags);
            writer.u8(static_cast<std::uint8_t>(state.initializerKind));
            writer.text(state.namespaceName);
            writer.text(state.name);
            writer.text(state.declaredTypeKey);
            writer.text(state.storageName);
            writer.text(state.accessorName);
            writer.u64(state.initializerBits);
            writer.text(state.initializerText);
        }
    }
    if (metadata.formatMinor >= 25)
    {
        writer.u32(static_cast<std::uint32_t>(metadata.binaryDependencies.size()));
        for (const LibraryMetadataBinaryDependency& dependency : metadata.binaryDependencies)
        {
            writer.text(dependency.moduleName);
            writer.text(dependency.libraryFileName);
            writer.text(dependency.metadataFileName);
            writer.text(dependency.runtimeFileName);
            writer.raw(dependency.abiFingerprint);
        }
        writer.u32(static_cast<std::uint32_t>(metadata.platformImports.size()));
        for (const LibraryMetadataPlatformImport& importRecord : metadata.platformImports)
        {
            writer.text(importRecord.dllName);
            writer.text(importRecord.symbolName);
        }
    }
    return writer.bytes();
}

std::vector<std::uint8_t> encodeAbiProjection(const LibraryMetadata& metadata)
{
    LibraryMetadata projected = metadata;
    projected.producerVersion.clear();
    projected.languageEdition = 0;
    // Static closure and private imports describe how to link this particular archive; they do
    // not change the public NativeSharp ABI published by the module. Project through 1.24 so even
    // the two 1.25 section counts stay outside the public fingerprint.
    projected.binaryDependencies.clear();
    projected.platformImports.clear();
    if (projected.formatMinor == 25) projected.formatMinor = 24;
    return encodePayload(projected);
}

bool readCount(ByteReader& reader, std::uint32_t& count)
{
    return reader.u32(count) && count <= kMaximumRecordCount;
}

bool decodePayload(
    std::span<const std::uint8_t> payload,
    LibraryMetadata& metadata,
    std::string& error)
{
    ByteReader reader(payload);
    if (!reader.text(metadata.producerVersion) || !reader.text(metadata.moduleName) || !reader.text(metadata.targetTriple))
    {
        error = "truncated metadata identity payload";
        return false;
    }
    if (metadata.formatMinor >= 23 && !reader.u8(metadata.languageEdition))
    {
        error = "truncated metadata language edition";
        return false;
    }
    if (metadata.formatMinor >= 8 &&
        (!reader.u16(metadata.nativeAbiMajor) || !reader.u16(metadata.nativeAbiMinor) ||
         !reader.u16(metadata.runtimeAbiMajor) || !reader.u16(metadata.runtimeAbiMinor) ||
         !reader.u16(metadata.manglingAbiVersion) || !reader.u16(metadata.objectAbiVersion) ||
         !reader.u16(metadata.delegateAbiVersion) || !reader.u16(metadata.exceptionAbiVersion) ||
         !reader.u32(metadata.managedDelegateCellSize)))
    {
        error = "truncated metadata ABI contract";
        return false;
    }

    std::uint32_t count = 0;
    if (!readCount(reader, count)) { error = "invalid metadata type count"; return false; }
    metadata.types.clear();
    metadata.types.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataType type;
        std::uint8_t kind = 0;
        if (!reader.u8(kind) || kind > static_cast<std::uint8_t>(LibraryMetadataTypeKind::Delegate) ||
            !reader.u8(type.accessibility) || !reader.u16(type.flags) ||
            !reader.text(type.identityKey) || !reader.text(type.fullyQualifiedName) || !reader.text(type.baseTypeKey) ||
            !readStringVector(reader, type.interfaceTypeKeys) ||
            !reader.u64(type.size) || !reader.u64(type.alignment) || !reader.u64(type.headerSize) ||
            !reader.u64(type.referenceSize) || !reader.u64(type.referenceAlignment) || !reader.u64(type.stableTypeId))
        {
            error = "truncated or invalid metadata type record";
            return false;
        }
        type.kind = static_cast<LibraryMetadataTypeKind>(kind);
        if (metadata.formatMinor >= 2 && !reader.text(type.enumUnderlyingTypeKey))
        {
            error = "truncated or invalid metadata enum underlying type";
            return false;
        }
        if (metadata.formatMinor >= 5 &&
            (!reader.u8(type.delegateFlags) ||
             type.delegateFlags > (metadata.formatMinor >= 20 ? 3U : 1U) ||
             !reader.text(type.delegateReturnTypeKey) ||
             !readStringVector(reader, type.delegateParameterTypeKeys)))
        {
            error = "truncated or invalid metadata delegate signature";
            return false;
        }
        std::uint32_t fieldCount = 0;
        if (!readCount(reader, fieldCount)) { error = "invalid metadata field count"; return false; }
        type.fields.reserve(fieldCount);
        for (std::uint32_t fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex)
        {
            LibraryMetadataField field;
            if (!reader.text(field.name) || !reader.text(field.typeKey) ||
                !readOwnership(reader, metadata.formatMinor, field.ownership) ||
                !reader.u64(field.offset) || !reader.u64(field.size) ||
                !reader.u64(field.alignment) || !reader.u32(field.flags))
            {
                error = "truncated or invalid metadata field record";
                return false;
            }
            if (metadata.formatMinor >= 18 &&
                (!reader.u64(field.bitOffset) || !reader.u64(field.bitWidth)))
            {
                error = "truncated or invalid metadata bit-field record";
                return false;
            }
            type.fields.push_back(std::move(field));
        }
        metadata.types.push_back(std::move(type));
    }

    if (!readCount(reader, count)) { error = "invalid metadata member count"; return false; }
    metadata.members.clear();
    metadata.members.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataMember member;
        if (!reader.u8(member.kind) || !reader.u8(member.accessibility) || !reader.u32(member.flags) ||
            !reader.text(member.declaringTypeKey) || !reader.text(member.name) || !reader.text(member.signatureKey) ||
            !reader.text(member.declaredTypeKey) ||
            !readOwnership(reader, metadata.formatMinor, member.declaredOwnership) ||
            !reader.text(member.cAbiName) || !reader.text(member.mangledName))
        {
            error = "truncated or invalid metadata member record";
            return false;
        }
        std::uint32_t parameterCount = 0;
        if (!readCount(reader, parameterCount)) { error = "invalid metadata parameter count"; return false; }
        member.parameters.reserve(parameterCount);
        for (std::uint32_t parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex)
        {
            LibraryMetadataParameter parameter;
            if (!reader.text(parameter.name) || !reader.text(parameter.typeKey) ||
                !readOwnership(reader, metadata.formatMinor, parameter.ownership))
            {
                error = "truncated or invalid metadata parameter record";
                return false;
            }
            if (metadata.formatMinor >= 4)
            {
                std::uint8_t passKind = 0;
                if (!reader.u8(passKind) ||
                    passKind > static_cast<std::uint8_t>(LibraryMetadataParameterPassKind::In))
                {
                    error = "truncated or invalid metadata parameter pass kind";
                    return false;
                }
                parameter.passKind = static_cast<LibraryMetadataParameterPassKind>(passKind);
            }
            if (metadata.formatMinor >= 24)
            {
                std::uint8_t parameterPack = 0;
                if (!reader.u8(parameterPack) || parameterPack > 1U)
                {
                    error = "truncated or invalid metadata parameter-pack flag";
                    return false;
                }
                parameter.isParameterPack = parameterPack != 0U;
            }
            if (metadata.formatMinor >= 3)
            {
                std::uint8_t defaultKind = 0;
                if (!reader.u8(defaultKind) ||
                    defaultKind > static_cast<std::uint8_t>(LibraryMetadataDefaultValueKind::String) ||
                    !reader.u8(parameter.defaultValueFlags) || parameter.defaultValueFlags > 1U ||
                    !reader.u64(parameter.defaultValueBits) || !reader.text(parameter.defaultValueText))
                {
                    error = "truncated or invalid metadata parameter default value";
                    return false;
                }
                parameter.defaultValueKind = static_cast<LibraryMetadataDefaultValueKind>(defaultKind);
            }
            member.parameters.push_back(std::move(parameter));
        }
        if (!reader.u8(member.constantKind) || !reader.u64(member.constantBits) ||
            (metadata.formatMinor >= 9 && !reader.text(member.constantText)) ||
            (metadata.formatMinor >= 10 && !reader.text(member.importDllName)) ||
            (metadata.formatMinor >= 13 && !reader.text(member.inlineSourceText)))
        {
            error = "truncated metadata constant record";
            return false;
        }
        metadata.members.push_back(std::move(member));
    }

    if (!readCount(reader, count)) { error = "invalid generic-template count"; return false; }
    metadata.genericTemplates.clear();
    metadata.genericTemplates.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataGenericTemplate generic;
        if (!reader.u8(generic.kind) ||
            (metadata.formatMinor >= 7 && !reader.u8(generic.accessibility)) ||
            !reader.text(generic.canonicalKey) || !reader.text(generic.moduleName) ||
            !reader.text(generic.namespaceName) || !reader.text(generic.ownerName) || !reader.text(generic.name) ||
            !readStringVector(reader, generic.parameters) ||
            (metadata.formatMinor >= 7 && !reader.text(generic.sourceText)) ||
            (metadata.formatMinor >= 11 && !readStringVector(reader, generic.dependencies)))
        {
            error = "truncated generic-template metadata";
            return false;
        }
        metadata.genericTemplates.push_back(std::move(generic));
    }

    if (!readCount(reader, count)) { error = "invalid generic-specialization count"; return false; }
    metadata.genericSpecializations.clear();
    metadata.genericSpecializations.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataGenericSpecialization specialization;
        if (!reader.u8(specialization.kind) || !reader.text(specialization.templateKey) ||
            !reader.text(specialization.specializationKey) || !reader.text(specialization.specializedName) ||
            !readStringVector(reader, specialization.typeArguments))
        {
            error = "truncated generic-specialization metadata";
            return false;
        }
        metadata.genericSpecializations.push_back(std::move(specialization));
    }

    if (metadata.formatMinor >= 12 && !readStringVector(reader, metadata.externalTypeKeys))
    {
        error = "truncated external-type dependency metadata";
        return false;
    }

    if (!readCount(reader, count)) { error = "invalid dispatch-table count"; return false; }
    metadata.dispatchTables.clear();
    metadata.dispatchTables.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataDispatch dispatch;
        if (!reader.text(dispatch.typeKey) || !reader.u64(dispatch.stableTypeId) ||
            !reader.text(dispatch.descriptorName) ||
            (metadata.formatMinor >= 6 && !reader.text(dispatch.descriptorAccessorName)) ||
            !reader.text(dispatch.vtableName) ||
            !reader.text(dispatch.destructionEntryName) || !readStringVector(reader, dispatch.slotSignatures) ||
            !readStringVector(reader, dispatch.slotCalleeNames))
        {
            error = "truncated dispatch metadata";
            return false;
        }
        std::uint32_t interfaceCount = 0;
        if (!readCount(reader, interfaceCount)) { error = "invalid dispatch-interface count"; return false; }
        dispatch.interfaces.reserve(interfaceCount);
        for (std::uint32_t interfaceIndex = 0; interfaceIndex < interfaceCount; ++interfaceIndex)
        {
            LibraryMetadataDispatchInterface interfaceRecord;
            if (!reader.text(interfaceRecord.interfaceTypeKey) || !reader.u64(interfaceRecord.stableInterfaceTypeId) ||
                !reader.text(interfaceRecord.methodTableName) || !readStringVector(reader, interfaceRecord.interfaceSignatures) ||
                !readStringVector(reader, interfaceRecord.implementationSignatures) ||
                !readStringVector(reader, interfaceRecord.calleeNames))
            {
                error = "truncated dispatch-interface metadata";
                return false;
            }
            dispatch.interfaces.push_back(std::move(interfaceRecord));
        }
        metadata.dispatchTables.push_back(std::move(dispatch));
    }

    if (!readCount(reader, count)) { error = "invalid C-export count"; return false; }
    metadata.exports.clear();
    metadata.exports.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataExport exportRecord;
        if (!reader.text(exportRecord.cAbiName) || !reader.text(exportRecord.declaringTypeKey) ||
            !reader.text(exportRecord.returnTypeKey) || !readStringVector(reader, exportRecord.parameterTypeKeys) ||
            !reader.text(exportRecord.nativeMangledName))
        {
            error = "truncated C-export metadata";
            return false;
        }
        metadata.exports.push_back(std::move(exportRecord));
    }

    metadata.namespaceFunctions.clear();
    if (metadata.formatMinor >= 21)
    {
        if (!readCount(reader, count))
        {
            error = "invalid namespace-function count";
            return false;
        }
        metadata.namespaceFunctions.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            LibraryMetadataNamespaceFunction function;
            if (!reader.u8(function.accessibility) || !reader.u8(function.flags) ||
                function.accessibility > 1U || function.flags > 15U ||
                !reader.text(function.namespaceName) || !reader.text(function.name) ||
                !reader.text(function.signatureKey) || !reader.text(function.declaredTypeKey))
            {
                error = "truncated or invalid namespace-function metadata";
                return false;
            }
            std::uint8_t ownership = 0;
            if (!reader.u8(ownership) ||
                ownership > static_cast<std::uint8_t>(LibraryMetadataOwnershipKind::Borrowed) ||
                !reader.text(function.mangledName))
            {
                error = "truncated namespace-function identity";
                return false;
            }
            function.declaredOwnership =
                static_cast<LibraryMetadataOwnershipKind>(ownership);
            std::uint32_t parameterCount = 0;
            if (!readCount(reader, parameterCount))
            {
                error = "invalid namespace-function parameter count";
                return false;
            }
            function.parameters.reserve(parameterCount);
            for (std::uint32_t parameterIndex = 0;
                 parameterIndex < parameterCount;
                 ++parameterIndex)
            {
                LibraryMetadataParameter parameter;
                std::uint8_t parameterOwnership = 0;
                std::uint8_t passKind = 0;
                std::uint8_t parameterPack = 0;
                std::uint8_t defaultKind = 0;
                if (!reader.text(parameter.name) || !reader.text(parameter.typeKey) ||
                    !reader.u8(parameterOwnership) ||
                    parameterOwnership > static_cast<std::uint8_t>(
                        LibraryMetadataOwnershipKind::Borrowed) ||
                    !reader.u8(passKind) ||
                    passKind > static_cast<std::uint8_t>(
                        LibraryMetadataParameterPassKind::In) ||
                    (metadata.formatMinor >= 24 &&
                     (!reader.u8(parameterPack) || parameterPack > 1U)) ||
                    !reader.u8(defaultKind) ||
                    defaultKind > static_cast<std::uint8_t>(
                        LibraryMetadataDefaultValueKind::String) ||
                    !reader.u8(parameter.defaultValueFlags) ||
                    parameter.defaultValueFlags > 1U ||
                    !reader.u64(parameter.defaultValueBits) ||
                    !reader.text(parameter.defaultValueText))
                {
                    error = "truncated namespace-function parameter metadata";
                    return false;
                }
                parameter.ownership =
                    static_cast<LibraryMetadataOwnershipKind>(parameterOwnership);
                parameter.passKind =
                    static_cast<LibraryMetadataParameterPassKind>(passKind);
                parameter.isParameterPack = parameterPack != 0U;
                parameter.defaultValueKind =
                    static_cast<LibraryMetadataDefaultValueKind>(defaultKind);
                function.parameters.push_back(std::move(parameter));
            }
            metadata.namespaceFunctions.push_back(std::move(function));
        }
    }

    metadata.namespaceStates.clear();
    if (metadata.formatMinor >= 22)
    {
        if (!readCount(reader, count))
        {
            error = "invalid namespace-state count";
            return false;
        }
        metadata.namespaceStates.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            LibraryMetadataNamespaceState state;
            std::uint8_t initializerKind = 0;
            if (!reader.u8(state.accessibility) || state.accessibility > 1U ||
                !reader.u8(state.flags) || state.flags > 1U ||
                !reader.u8(initializerKind) ||
                initializerKind > static_cast<std::uint8_t>(
                    LibraryMetadataNamespaceStateInitializerKind::Null) ||
                !reader.text(state.namespaceName) || !reader.text(state.name) ||
                !reader.text(state.declaredTypeKey) ||
                !reader.text(state.storageName) || !reader.text(state.accessorName) ||
                !reader.u64(state.initializerBits) ||
                !reader.text(state.initializerText))
            {
                error = "truncated or invalid namespace-state metadata";
                return false;
            }
            state.initializerKind =
                static_cast<LibraryMetadataNamespaceStateInitializerKind>(
                    initializerKind);
            metadata.namespaceStates.push_back(std::move(state));
        }
    }
    metadata.binaryDependencies.clear();
    metadata.platformImports.clear();
    if (metadata.formatMinor >= 25)
    {
        if (!readCount(reader, count))
        {
            error = "invalid binary-dependency count";
            return false;
        }
        metadata.binaryDependencies.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            LibraryMetadataBinaryDependency dependency;
            if (!reader.text(dependency.moduleName) ||
                !reader.text(dependency.libraryFileName) ||
                !reader.text(dependency.metadataFileName) ||
                !reader.text(dependency.runtimeFileName) ||
                !reader.raw(dependency.abiFingerprint))
            {
                error = "truncated binary-dependency metadata";
                return false;
            }
            metadata.binaryDependencies.push_back(std::move(dependency));
        }
        if (!readCount(reader, count))
        {
            error = "invalid platform-import count";
            return false;
        }
        metadata.platformImports.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            LibraryMetadataPlatformImport importRecord;
            if (!reader.text(importRecord.dllName) || !reader.text(importRecord.symbolName))
            {
                error = "truncated platform-import metadata";
                return false;
            }
            metadata.platformImports.push_back(std::move(importRecord));
        }
    }
    if (!reader.atEnd())
    {
        error = "metadata payload has trailing bytes";
        return false;
    }

    return true;
}

bool sameFingerprint(
    const std::array<std::uint8_t, 32>& left,
    const std::array<std::uint8_t, 32>& right) noexcept
{
    std::uint8_t difference = 0;
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        difference = static_cast<std::uint8_t>(difference | (left[index] ^ right[index]));
    }
    return difference == 0;
}

bool writeBytesAtomically(
    const std::filesystem::path& destination,
    std::span<const std::uint8_t> bytes,
    std::string& error)
{
    return basic::writeFileTransactionally(destination, bytes, error);
}

std::string versionPair(std::uint16_t major, std::uint16_t minor)
{
    return std::to_string(major) + "." + std::to_string(minor);
}

// E5: una incompatibilidad ABI tiene que nombrar LA DECISION FISICA que
// difiere. El mensaje anterior agrupaba ocho campos bajo "ABI contract is not
// compatible"; era correcto para detener la compilacion, pero obligaba a abrir
// el fichero o depurar el lector para saber si habia cambiado el layout de
// objetos, la celda delegate o el transporte de excepciones. Esta funcion es la
// unica tabla de compatibilidad del contrato 1.0 y la comparten el escritor de
// modelos y el consumidor.
bool verifyFrozenAbiContract(const LibraryMetadata& metadata, std::string& error)
{
    if (metadata.nativeAbiMajor != kNativeAbiMajor || metadata.nativeAbiMinor > kNativeAbiMinor)
    {
        error = "NativeSharp ABI contract mismatch: native ABI expected " +
                versionPair(kNativeAbiMajor, kNativeAbiMinor) + ", found " +
                versionPair(metadata.nativeAbiMajor, metadata.nativeAbiMinor);
        return false;
    }
    if (metadata.runtimeAbiMajor != kRuntimeAbiMajor || metadata.runtimeAbiMinor > kRuntimeAbiMinor)
    {
        error = "NativeSharp ABI contract mismatch: runtime ABI expected " +
                versionPair(kRuntimeAbiMajor, kRuntimeAbiMinor) + ", found " +
                versionPair(metadata.runtimeAbiMajor, metadata.runtimeAbiMinor);
        return false;
    }
    const auto exactVersion = [&](std::string_view component, std::uint16_t found, std::uint16_t expected) {
        if (found == expected) return true;
        error = "NativeSharp ABI contract mismatch: " + std::string(component) + " expected " +
                std::to_string(expected) + ", found " + std::to_string(found);
        return false;
    };
    if (!exactVersion("mangling ABI", metadata.manglingAbiVersion, kManglingAbiVersion) ||
        !exactVersion("object ABI", metadata.objectAbiVersion, kObjectAbiVersion) ||
        !exactVersion("delegate ABI", metadata.delegateAbiVersion, kDelegateAbiVersion) ||
        !exactVersion("exception ABI", metadata.exceptionAbiVersion, kExceptionAbiVersion))
    {
        return false;
    }
    if (metadata.managedDelegateCellSize != kManagedDelegateCellSize)
    {
        error = "NativeSharp ABI contract mismatch: managed delegate cell size "
                "expected " +
                std::to_string(kManagedDelegateCellSize) + " bytes, found " +
                std::to_string(metadata.managedDelegateCellSize) + " bytes";
        return false;
    }
    return true;
}
}

bool LibraryMetadataCodec::write(
    const frontend::ProjectCompilation& compilation,
    const std::filesystem::path& destination,
    std::string& error) const
{
    LibraryMetadata ignored;
    return write(compilation, destination, ignored, error);
}

bool LibraryMetadataCodec::write(
    const frontend::ProjectCompilation& compilation,
    const std::filesystem::path& destination,
    LibraryMetadata& writtenModel,
    std::string& error) const
{
    error.clear();
    LibraryMetadata metadata;
    if (!buildMetadataModel(compilation, metadata, error)) return false;
    return writeModelImpl(metadata, destination, &writtenModel, error);
}

// Escribe un MODELO ya construido, en la version menor que el propio modelo
// dice.
//
// POR QUE EXISTE, y no es andamiaje de pruebas. La compatibilidad hacia atras
// de este formato solo se puede MEDIR si se puede producir un fichero de una
// version anterior: sin esto, "un lector de 1.2 lee un fichero de 1.1" es una
// afirmacion sobre codigo que nadie ejecuto nunca -- y este proyecto no cuenta
// esas. Cada minor futuro hereda la prueba sin escribir nada nuevo.
bool LibraryMetadataCodec::writeModel(
    const LibraryMetadata& model,
    const std::filesystem::path& destination,
    std::string& error) const
{
    return writeModelImpl(model, destination, nullptr, error);
}

bool LibraryMetadataCodec::writeModelImpl(
    const LibraryMetadata& model,
    const std::filesystem::path& destination,
    LibraryMetadata* writtenModel,
    std::string& error) const
{
    error.clear();
    LibraryMetadata metadata;
    const std::array<LibraryMetadata, 1> fragments = {model};
    LibraryMetadataMerger merger;
    if (!merger.merge(fragments, metadata, error)) return false;
    if (metadata.formatMajor != kFormatMajor || metadata.formatMinor > kFormatMinor)
    {
        error = "cannot write NativeSharp metadata format " +
            std::to_string(metadata.formatMajor) + "." + std::to_string(metadata.formatMinor);
        return false;
    }
    // EL OBJETIVO SE VALIDA, NO SE SUSTITUYE. Escribir la constante por encima de
    // lo que el modelo diga es exactamente lo que hacia el encabezado con el
    // minor: dos fuentes de verdad para el mismo campo. Y aqui seria peor, porque
    // esta funcion existe para MEDIR round-trips -- un campo que se sobrescribe
    // en silencio hace que la prueba diga "el objetivo sobrevivio" cuando lo que
    // sobrevivio es la constante.
    //
    // NativeSharp 2.0 solo tiene un objetivo, asi que lo unico correcto es
    // rechazar cualquier otro con su numero delante, y no producir un fichero que
    // el lector rechazaria despues.
    if (metadata.targetId != kWindowsX64Target || metadata.pointerSize != kPointerSize ||
        metadata.pointerAlignment != kPointerAlignment)
    {
        error = "cannot write NativeSharp metadata for target " + std::to_string(metadata.targetId) +
            " with pointer " + std::to_string(metadata.pointerSize) + "/" +
            std::to_string(metadata.pointerAlignment) +
            "; NativeSharp 2.0 targets Windows x64 only";
        return false;
    }
    if (metadata.formatMinor >= 8 && !verifyFrozenAbiContract(metadata, error)) return false;
    if (metadata.formatMinor >= 23 && metadata.languageEdition != 2U &&
        metadata.languageEdition != 3U)
    {
        error = "metadata language edition must be 2 or 3";
        return false;
    }
    if (metadata.formatMinor < 23 && metadata.languageEdition != 2U)
    {
        error = "NativeSharp metadata before 1.23 cannot represent language edition 3";
        return false;
    }

    // A down-level file must preserve the semantics of that historical minor, not merely omit
    // the bytes introduced later. NSMETA <= 1.10 contained public templates only; retaining a
    // private closure entry while dropping its 1.11 dependency edges would create a file that no
    // historical producer could have emitted and would silently widen what old readers hydrate.
    if (metadata.formatMinor < 11)
    {
        std::erase_if(metadata.genericTemplates, [](const LibraryMetadataGenericTemplate& generic) {
            return generic.accessibility != 3U;
        });
        std::unordered_set<std::string> publicTemplateKeys;
        for (const LibraryMetadataGenericTemplate& generic : metadata.genericTemplates)
            publicTemplateKeys.insert(generic.canonicalKey);
        std::erase_if(
            metadata.genericSpecializations,
            [&](const LibraryMetadataGenericSpecialization& specialization) {
                return !publicTemplateKeys.contains(specialization.templateKey);
            });
    }
    if (metadata.formatMinor < 13)
    {
        for (LibraryMetadataMember& member : metadata.members)
        {
            member.flags &= ~kMetadataMemberInliningMask;
            member.inlineSourceText.clear();
        }
    }
    if (metadata.formatMinor < 14)
    {
        for (LibraryMetadataMember& member : metadata.members)
            member.flags &= ~kMetadataMemberVolatile;
        for (LibraryMetadataType& type : metadata.types)
            for (LibraryMetadataField& field : type.fields)
                field.flags &= ~kMetadataMemberVolatile;
    }
    if (metadata.formatMinor < 15)
    {
        for (LibraryMetadataMember& member : metadata.members)
            member.flags &= ~kMetadataMemberReferenceReturn;
    }
    if (metadata.formatMinor < 16)
    {
        for (LibraryMetadataMember& member : metadata.members)
            member.flags &= ~kMetadataMemberNoHeap;
    }
    if (metadata.formatMinor < 17)
    {
        for (LibraryMetadataType& type : metadata.types)
            for (LibraryMetadataField& field : type.fields)
                field.flags &= ~kMetadataFieldAnonymousCarrier;
    }
    if (metadata.formatMinor < 18)
    {
        for (const LibraryMetadataType& type : metadata.types)
        {
            for (const LibraryMetadataField& field : type.fields)
            {
                if ((field.flags & kMetadataFieldBitField) != 0U)
                {
                    error = "NativeSharp metadata before 1.18 cannot represent bit-field layout";
                    return false;
                }
            }
        }
    }
    if (metadata.formatMinor < 19)
    {
        for (const LibraryMetadataMember& member : metadata.members)
        {
            if ((member.flags & kMetadataMemberVariadic) != 0U)
            {
                error = "NativeSharp metadata before 1.19 cannot represent a variadic C declaration";
                return false;
            }
        }
    }
    if (metadata.formatMinor < 20)
    {
        for (const LibraryMetadataType& type : metadata.types)
        {
            if ((type.delegateFlags & 2U) != 0U)
            {
                error = "NativeSharp metadata before 1.20 cannot represent vectorcall delegates";
                return false;
            }
        }
    }
    if (metadata.formatMinor < 24)
    {
        const auto hasParameterPack = [](const auto& callable)
        {
            return std::ranges::any_of(
                callable.parameters,
                [](const LibraryMetadataParameter& parameter)
                {
                    return parameter.isParameterPack;
                });
        };
        if (std::ranges::any_of(metadata.members, hasParameterPack) ||
            std::ranges::any_of(metadata.namespaceFunctions, hasParameterPack))
        {
            error = "NativeSharp metadata before 1.24 cannot represent a params "
                "ReadOnlySpan<T> declaration";
            return false;
        }
    }
    if (metadata.formatMinor < 25 &&
        (!metadata.binaryDependencies.empty() || !metadata.platformImports.empty()))
    {
        error = "NativeSharp metadata before 1.25 cannot represent binary closure or private "
            "platform imports";
        return false;
    }

    metadata.abiFingerprint = basic::sha256(encodeAbiProjection(metadata));
    const std::vector<std::uint8_t> payload = encodePayload(metadata);
    if (payload.size() > kMaximumPayloadSize)
    {
        error = "NativeSharp metadata payload exceeds 128 MiB format limit";
        return false;
    }
    const std::array<std::uint8_t, 32> payloadHash = basic::sha256(payload);

    ByteWriter file;
    file.raw(kMagic);
    // El encabezado escribe el minor DEL MODELO, no la constante. El cuerpo ya se
    // codifica segun `metadata.formatMinor` (`if (metadata.formatMinor >= N)`),
    // asi que tomar el numero de otro sitio serian dos fuentes de verdad para el
    // mismo campo dentro del mismo fichero -- y un encabezado que dijera 2 sobre
    // un cuerpo escrito como 1 es un fichero que no se puede leer.
    file.u16(metadata.formatMajor);
    file.u16(metadata.formatMinor);
    // El marcador de endianidad es una propiedad de la CODIFICACION, no del
    // modelo: no hay campo que pueda decir otra cosa, y por eso este si es una
    // constante. Los tres siguientes salen del modelo, ya validado arriba.
    file.u32(kEndianMarker);
    file.u32(metadata.targetId);
    file.u32(metadata.pointerSize);
    file.u32(metadata.pointerAlignment);
    file.u64(static_cast<std::uint64_t>(payload.size()));
    file.raw(metadata.abiFingerprint);
    file.raw(payloadHash);
    file.raw(payload);
    if (!writeBytesAtomically(destination, file.bytes(), error)) return false;
    if (writtenModel != nullptr) *writtenModel = std::move(metadata);
    return true;
}

bool LibraryMetadataCodec::read(
    const std::filesystem::path& source,
    LibraryMetadata& metadata,
    std::string& error) const
{
    error.clear();
    std::ifstream stream(source, std::ios::binary | std::ios::ate);
    if (!stream)
    {
        error = "cannot open NativeSharp metadata '" + source.string() + "'";
        return false;
    }
    const std::streampos end = stream.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) > kHeaderSize + kMaximumPayloadSize)
    {
        error = "NativeSharp metadata file has invalid size";
        return false;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    stream.seekg(0, std::ios::beg);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream && !bytes.empty())
    {
        error = "failed while reading NativeSharp metadata";
        return false;
    }
    if (bytes.size() < kHeaderSize)
    {
        error = "NativeSharp metadata is truncated before its fixed header";
        return false;
    }

    ByteReader reader(bytes);
    std::array<std::uint8_t, 8> magic{};
    if (!reader.raw(magic) || magic != kMagic)
    {
        error = "invalid NativeSharp metadata magic";
        return false;
    }
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
    std::uint32_t endianMarker = 0;
    std::uint32_t targetId = 0;
    std::uint32_t pointerSize = 0;
    std::uint32_t pointerAlignment = 0;
    std::uint64_t payloadSize = 0;
    std::array<std::uint8_t, 32> abiFingerprint{};
    std::array<std::uint8_t, 32> payloadHash{};
    if (!reader.u16(major) || !reader.u16(minor) || !reader.u32(endianMarker) ||
        !reader.u32(targetId) || !reader.u32(pointerSize) || !reader.u32(pointerAlignment) ||
        !reader.u64(payloadSize) || !reader.raw(abiFingerprint) || !reader.raw(payloadHash))
    {
        error = "truncated NativeSharp metadata header";
        return false;
    }
    if (major != kFormatMajor || minor > kFormatMinor)
    {
        error = "unsupported NativeSharp metadata format " + std::to_string(major) + "." + std::to_string(minor);
        return false;
    }
    if (endianMarker != kEndianMarker)
    {
        error = "NativeSharp metadata endian marker mismatch";
        return false;
    }
    if (payloadSize > kMaximumPayloadSize || payloadSize != reader.remaining())
    {
        error = "NativeSharp metadata payload length mismatch";
        return false;
    }
    const std::span<const std::uint8_t> payload(bytes.data() + kHeaderSize, static_cast<std::size_t>(payloadSize));
    if (!sameFingerprint(basic::sha256(payload), payloadHash))
    {
        error = "NativeSharp metadata payload checksum mismatch";
        return false;
    }

    LibraryMetadata decoded;
    decoded.formatMajor = major;
    decoded.formatMinor = minor;
    decoded.targetId = targetId;
    decoded.pointerSize = pointerSize;
    decoded.pointerAlignment = pointerAlignment;
    decoded.abiFingerprint = abiFingerprint;
    if (minor < 8)
    {
        decoded.nativeAbiMajor = 0;
        decoded.nativeAbiMinor = 0;
        decoded.runtimeAbiMajor = 0;
        decoded.runtimeAbiMinor = 0;
        decoded.manglingAbiVersion = 0;
        decoded.objectAbiVersion = 0;
        decoded.delegateAbiVersion = 0;
        decoded.exceptionAbiVersion = 0;
        decoded.managedDelegateCellSize = 0;
    }
    if (!decodePayload(payload, decoded, error)) return false;
    const std::array<std::uint8_t, 32> recomputedAbi = basic::sha256(encodeAbiProjection(decoded));
    if (!sameFingerprint(recomputedAbi, decoded.abiFingerprint))
    {
        error = "NativeSharp metadata ABI fingerprint mismatch";
        return false;
    }

    // Normalize lookup order only after authenticating the ABI projection in the exact order
    // encoded by the producer. Older compatible producers appended synthesized constructors
    // after their sorted member table; sorting before the fingerprint check changes their
    // historical projection and incorrectly rejects otherwise valid NSMETA files.
    std::stable_sort(decoded.types.begin(), decoded.types.end(), [](const auto& left, const auto& right) {
        return left.identityKey < right.identityKey;
    });
    std::stable_sort(decoded.members.begin(), decoded.members.end(), [](const auto& left, const auto& right) {
        return left.declaringTypeKey < right.declaringTypeKey;
    });
    metadata = std::move(decoded);
    return true;
}

bool LibraryMetadataCodec::verifyCurrentCompiler(
    const LibraryMetadata& metadata,
    std::string& error) const
{
    error.clear();
    if (metadata.formatMajor != kFormatMajor || metadata.formatMinor > kFormatMinor)
    {
        error = "metadata format expected NSMETA " + std::to_string(kFormatMajor) + ".0 through " +
                std::to_string(kFormatMajor) + "." + std::to_string(kFormatMinor) + ", found " +
                std::to_string(metadata.formatMajor) + "." + std::to_string(metadata.formatMinor);
        return false;
    }
    if (metadata.targetId != kWindowsX64Target)
    {
        error = "metadata target ABI mismatch: target id expected " + std::to_string(kWindowsX64Target) + ", found " +
                std::to_string(metadata.targetId);
        return false;
    }
    if (metadata.pointerSize != kPointerSize || metadata.pointerAlignment != kPointerAlignment)
    {
        error = "metadata target ABI mismatch: pointer size/alignment expected " + std::to_string(kPointerSize) + "/" +
                std::to_string(kPointerAlignment) + ", found " + std::to_string(metadata.pointerSize) + "/" +
                std::to_string(metadata.pointerAlignment);
        return false;
    }
    if (metadata.targetTriple != kTargetTriple)
    {
        error = "metadata target ABI mismatch: target triple expected '" + std::string(kTargetTriple) + "', found '" +
                metadata.targetTriple + "'";
        return false;
    }
    if (metadata.formatMinor >= 8 && !verifyFrozenAbiContract(metadata, error)) return false;
    if (metadata.formatMinor >= 23 && metadata.languageEdition != 2U &&
        metadata.languageEdition != 3U)
    {
        error = "metadata language edition must be 2 or 3";
        return false;
    }
    if (metadata.formatMinor >= 8 && metadata.moduleName.empty())
    {
        error = "metadata module identity is empty";
        return false;
    }

    // Un checksum valido demuestra integridad de bytes, no coherencia del modelo.
    // Un fichero puede estar firmado por su propio productor y aun publicar dos
    // identidades iguales o tablas paralelas de longitudes distintas. Esas formas
    // se rechazan aqui antes de materializar fuente sintetica o comparar simbolos
    // fisicos.
    std::unordered_set<std::string> typeIdentities;
    for (const LibraryMetadataType& type : metadata.types)
    {
        if (type.identityKey.empty() || type.fullyQualifiedName.empty())
        {
            error = "metadata contains a type with an empty canonical identity or name";
            return false;
        }
        if (!typeIdentities.insert(type.identityKey).second)
        {
            error = "metadata contains duplicate type identity '" + type.identityKey + "'";
            return false;
        }
    }
    for (const LibraryMetadataType& type : metadata.types)
    {
        if ((type.kind == LibraryMetadataTypeKind::Class || type.kind == LibraryMetadataTypeKind::Interface ||
             (metadata.formatMinor >= 8 && type.kind == LibraryMetadataTypeKind::Delegate)) &&
            (type.referenceSize != metadata.pointerSize ||
             type.referenceAlignment != metadata.pointerAlignment))
        {
            error = "metadata reference ABI for type '" + type.fullyQualifiedName + "' is " +
                std::to_string(type.referenceSize) + "/" + std::to_string(type.referenceAlignment) +
                ", but the target pointer ABI is " + std::to_string(metadata.pointerSize) + "/" +
                std::to_string(metadata.pointerAlignment);
            return false;
        }
        if (metadata.formatMinor >= 5 && type.kind == LibraryMetadataTypeKind::Delegate &&
            type.delegateReturnTypeKey.empty())
        {
            error = "metadata delegate '" + type.fullyQualifiedName +
                "' does not provide its callable signature";
            return false;
        }
    }
    if (metadata.formatMinor >= 6)
    {
        constexpr std::uint8_t kMethodKind = 2U;
        constexpr std::uint32_t kVirtual = 1U << 3U;
        constexpr std::uint32_t kOverride = 1U << 4U;
        constexpr std::uint32_t kAbstract = 1U << 5U;
        constexpr std::uint32_t kNewSlot = 1U << 19U;
        std::unordered_map<std::string, const LibraryMetadataType*> typesByKey;
        for (const LibraryMetadataType& type : metadata.types)
            typesByKey.emplace("type:" + type.identityKey, &type);
        std::unordered_map<std::string, const LibraryMetadataMember*> membersByCallee;
        for (const LibraryMetadataMember& member : metadata.members)
        {
            if (member.mangledName.empty()) continue;
            if (!membersByCallee.emplace(member.mangledName, &member).second)
            {
                error = "metadata contains duplicate NativeSharp callee '" + member.mangledName + "'";
                return false;
            }
        }

        std::unordered_set<std::string> dispatchTypes;
        for (const LibraryMetadataDispatch& dispatch : metadata.dispatchTables)
        {
            if (!dispatchTypes.insert(dispatch.typeKey).second)
            {
                error = "metadata contains duplicate dispatch contract for '" + dispatch.typeKey + "'";
                return false;
            }
            const auto dispatchedType = typesByKey.find(dispatch.typeKey);
            if (dispatchedType == typesByKey.end() ||
                dispatchedType->second->kind != LibraryMetadataTypeKind::Class)
            {
                error = "metadata dispatch contract refers to unknown or non-class type '" +
                    dispatch.typeKey + "'";
                return false;
            }
            if (dispatch.descriptorAccessorName.empty())
            {
                error = "metadata dispatch contract for '" + dispatch.typeKey +
                    "' does not provide its class-descriptor accessor";
                return false;
            }
            if (dispatch.destructionEntryName.empty())
            {
                error = "metadata dispatch contract for '" + dispatch.typeKey +
                    "' does not provide its producer-owned destruction entry";
                return false;
            }
            if (dispatch.slotSignatures.size() != dispatch.slotCalleeNames.size())
            {
                error = "metadata dispatch contract for '" + dispatch.typeKey + "' has " +
                        std::to_string(dispatch.slotSignatures.size()) + " slot signatures but " +
                        std::to_string(dispatch.slotCalleeNames.size()) + " callees";
                return false;
            }
            for (std::size_t slot = 0; slot < dispatch.slotSignatures.size(); ++slot)
            {
                if (dispatch.slotSignatures[slot].empty())
                {
                    error = "metadata dispatch contract for '" + dispatch.typeKey + "' has an empty signature at slot " +
                        std::to_string(slot);
                    return false;
                }
                const std::string& callee = dispatch.slotCalleeNames[slot];
                const auto localCallee = membersByCallee.find(callee);
                if (!callee.empty() && localCallee != membersByCallee.end() &&
                    localCallee->second->signatureKey != dispatch.slotSignatures[slot])
                {
                    error = "metadata dispatch slot " + std::to_string(slot) + " for '" + dispatch.typeKey +
                        "' names callee '" + callee + "' with signature '" +
                        localCallee->second->signatureKey + "', expected '" + dispatch.slotSignatures[slot] + "'";
                    return false;
                }
            }
            for (const LibraryMetadataDispatchInterface& interface : dispatch.interfaces)
            {
                if (interface.interfaceSignatures.size() != interface.implementationSignatures.size() ||
                    interface.interfaceSignatures.size() != interface.calleeNames.size())
                {
                    error = "metadata interface dispatch contract '" + interface.interfaceTypeKey + "' on '" +
                            dispatch.typeKey + "' has inconsistent slot counts";
                    return false;
                }
                for (std::size_t slot = 0; slot < interface.interfaceSignatures.size(); ++slot)
                {
                    if (interface.interfaceSignatures[slot].empty())
                    {
                        error = "metadata interface dispatch contract '" + interface.interfaceTypeKey + "' on '" +
                            dispatch.typeKey + "' has an empty interface signature at slot " + std::to_string(slot);
                        return false;
                    }
                    const std::string& callee = interface.calleeNames[slot];
                    const auto localCallee = membersByCallee.find(callee);
                    if (!callee.empty() && localCallee != membersByCallee.end() &&
                        localCallee->second->signatureKey != interface.implementationSignatures[slot])
                    {
                        error = "metadata interface dispatch slot " + std::to_string(slot) + " for '" +
                            interface.interfaceTypeKey + "' on '" + dispatch.typeKey + "' names callee '" + callee +
                            "' with signature '" + localCallee->second->signatureKey + "', expected '" +
                            interface.implementationSignatures[slot] + "'";
                        return false;
                    }
                }
            }
        }

        for (const LibraryMetadataMember& member : metadata.members)
        {
            if (member.kind != kMethodKind || (member.flags & (kVirtual | kOverride | kAbstract)) == 0U) continue;
            const auto declaringType = typesByKey.find(member.declaringTypeKey);
            if (declaringType == typesByKey.end() || declaringType->second->kind == LibraryMetadataTypeKind::Interface)
                continue;
            const auto dispatch = std::ranges::find_if(metadata.dispatchTables,
                [&](const LibraryMetadataDispatch& candidate) { return candidate.typeKey == member.declaringTypeKey; });
            if (dispatch == metadata.dispatchTables.end())
            {
                error = "metadata virtual member '" + member.declaringTypeKey + "." + member.signatureKey +
                    "' has no declaring-type dispatch contract";
                return false;
            }
            const auto signature = std::ranges::find_if(
                dispatch->slotSignatures, [&](const std::string& candidate) {
                    if (candidate != member.signatureKey) return false;
                    if ((member.flags & kNewSlot) == 0U) return true;
                    const std::size_t slot = static_cast<std::size_t>(
                        &candidate - dispatch->slotSignatures.data());
                    return slot < dispatch->slotCalleeNames.size() &&
                        dispatch->slotCalleeNames[slot] == member.mangledName;
                });
            if (signature == dispatch->slotSignatures.end())
            {
                error = "metadata virtual member '" + member.declaringTypeKey + "." + member.signatureKey +
                    "' has no matching dispatch slot";
                return false;
            }
            const std::size_t slot = static_cast<std::size_t>(signature - dispatch->slotSignatures.begin());
            const std::string& callee = dispatch->slotCalleeNames[slot];
            if ((member.flags & kAbstract) != 0U)
            {
                if (!callee.empty())
                {
                    error = "metadata abstract member '" + member.declaringTypeKey + "." + member.signatureKey +
                        "' unexpectedly owns dispatch callee '" + callee + "'";
                    return false;
                }
            }
            else if (member.mangledName.empty() || callee != member.mangledName)
            {
                error = "metadata concrete virtual member '" + member.declaringTypeKey + "." + member.signatureKey +
                    "' publishes callee '" + member.mangledName + "' but its dispatch slot names '" + callee + "'";
                return false;
            }
        }
    }
    if (metadata.formatMinor >= 7)
    {
        constexpr std::uint8_t kPublicAccessibility = 3U;
        std::unordered_set<std::string> templateKeys;
        std::unordered_map<std::string, const LibraryMetadataGenericTemplate*> templateByKey;
        for (const LibraryMetadataGenericTemplate& generic : metadata.genericTemplates)
        {
            if (generic.canonicalKey.empty() || !templateKeys.insert(generic.canonicalKey).second)
            {
                error = generic.canonicalKey.empty()
                    ? "metadata contains a generic template with an empty canonical key"
                    : "metadata contains duplicate generic template '" + generic.canonicalKey + "'";
                return false;
            }
            if (generic.accessibility > kPublicAccessibility || generic.sourceText.empty())
            {
                error = "metadata generic template '" + generic.canonicalKey +
                    "' does not provide a valid materializable declaration";
                return false;
            }
            templateByKey.emplace(generic.canonicalKey, &generic);
            if (metadata.formatMinor >= 11)
            {
                std::unordered_set<std::string> dependencyKeys;
                for (const std::string& dependency : generic.dependencies)
                {
                    if (dependency.empty() || !dependencyKeys.insert(dependency).second)
                    {
                        error = dependency.empty()
                            ? "metadata generic template '" + generic.canonicalKey +
                                "' contains an empty closure dependency"
                            : "metadata generic template '" + generic.canonicalKey +
                                "' contains duplicate closure dependency '" + dependency + "'";
                        return false;
                    }
                }
            }
        }

        if (metadata.formatMinor >= 11)
        {
            const std::string sameModulePrefix = metadata.moduleName + "::";
            for (const LibraryMetadataGenericTemplate& publicRoot : metadata.genericTemplates)
            {
                if (publicRoot.accessibility != kPublicAccessibility) continue;
                std::vector<std::string> chain;
                std::unordered_set<std::string> visiting;
                std::function<bool(const LibraryMetadataGenericTemplate&)> verifyClosure =
                    [&](const LibraryMetadataGenericTemplate& generic)
                {
                    if (!visiting.insert(generic.canonicalKey).second) return true;
                    chain.push_back(generic.canonicalKey);
                    for (const std::string& dependencyKey : generic.dependencies)
                    {
                        const auto dependency = templateByKey.find(dependencyKey);
                        if (dependency == templateByKey.end())
                        {
                            if (!dependencyKey.starts_with(sameModulePrefix)) continue;
                            error = "reusable template closure is incomplete\n  public root: " +
                                publicRoot.canonicalKey + "\n  required template: " +
                                dependencyKey + "\n  owner: " + metadata.moduleName +
                                "\n  dependency chain: ";
                            for (const std::string& entry : chain) error += entry + " -> ";
                            error += dependencyKey;
                            return false;
                        }
                        if (!verifyClosure(*dependency->second)) return false;
                    }
                    chain.pop_back();
                    visiting.erase(generic.canonicalKey);
                    return true;
                };
                if (!verifyClosure(publicRoot)) return false;
            }
        }
    }
    std::unordered_set<std::string> specializationKeys;
    std::unordered_set<std::string> specializedTypeKeys;
    for (const LibraryMetadataGenericSpecialization& specialization : metadata.genericSpecializations)
    {
        if (specialization.templateKey.empty() || specialization.specializationKey.empty() ||
            specialization.specializedName.empty() || specialization.typeArguments.empty() ||
            !specializationKeys.insert(specialization.specializationKey).second)
        {
            error = specialization.templateKey.empty() || specialization.specializationKey.empty() ||
                    specialization.specializedName.empty() || specialization.typeArguments.empty()
                ? "metadata contains an incomplete generic specialization declaration"
                : "metadata contains duplicate generic specialization '" +
                    specialization.specializationKey + "'";
            return false;
        }
        if (const std::optional<std::string> typeKey = specializedTypeKey(specialization);
            typeKey.has_value() && !specializedTypeKeys.insert(*typeKey).second)
        {
            error = "metadata contains duplicate closed generic type declaration '" + *typeKey + "'";
            return false;
        }
    }

    std::unordered_set<std::string> declaredTypeKeys;
    for (const LibraryMetadataType& type : metadata.types)
        declaredTypeKeys.insert("type:" + type.identityKey);

    std::unordered_set<std::string> externalTypeKeys;
    if (metadata.formatMinor >= 12)
    {
        for (const std::string& typeKey : metadata.externalTypeKeys)
        {
            const std::string base = metadataBaseTypeKey(typeKey);
            const std::optional<std::string> module = metadataTypeModule(base);
            if (base != typeKey || !module.has_value() || *module == metadata.moduleName ||
                !externalTypeKeys.insert(typeKey).second)
            {
                error = base != typeKey || !module.has_value()
                    ? "metadata contains malformed external type dependency '" + typeKey + "'"
                    : *module == metadata.moduleName
                        ? "metadata marks its own type '" + typeKey + "' as an external dependency"
                        : "metadata contains duplicate external type dependency '" + typeKey + "'";
                return false;
            }
            if (declaredTypeKeys.contains(typeKey) || specializedTypeKeys.contains(typeKey))
            {
                error = "metadata both declares and imports type '" + typeKey + "'";
                return false;
            }
        }
    }

    if (metadata.formatMinor >= 7)
    {
        const auto verifyTypeReference = [&](const std::string& writtenTypeKey,
                                             std::string_view owner) -> bool {
            const std::string typeKey = metadataBaseTypeKey(writtenTypeKey);
            if (!typeKey.starts_with("type:") || declaredTypeKeys.contains(typeKey) ||
                specializedTypeKeys.contains(typeKey) || externalTypeKeys.contains(typeKey))
            {
                return true;
            }
            if (metadata.formatMinor >= 12)
            {
                const std::optional<std::string> module = metadataTypeModule(typeKey);
                if (looksLikeCompilerSpecializedType(typeKey))
                {
                    error = "metadata " + std::string(owner) + " refers to closed generic type '" + typeKey +
                        "' but no generic specialization declares how to materialize it and no external "
                        "type dependency assigns it to a provider";
                }
                else if (module.has_value() && *module != metadata.moduleName)
                {
                    error = "metadata " + std::string(owner) + " refers to external type '" + typeKey +
                        "' but does not record that LibraryReference dependency";
                }
                else
                {
                    error = "metadata " + std::string(owner) + " refers to local type '" + typeKey +
                        "' but declares neither the type nor its closed specialization";
                }
                return false;
            }
            if (!looksLikeCompilerSpecializedType(typeKey)) return true;
            error = "metadata " + std::string(owner) + " refers to closed generic type '" + typeKey +
                "' but no generic specialization declares how to materialize it";
            return false;
        };
        for (const LibraryMetadataType& type : metadata.types)
        {
            const std::string owner = "type '" + type.identityKey + "'";
            if (!verifyTypeReference(type.baseTypeKey, owner)) return false;
            for (const std::string& interfaceTypeKey : type.interfaceTypeKeys)
                if (!verifyTypeReference(interfaceTypeKey, owner)) return false;
            for (const LibraryMetadataField& field : type.fields)
                if (!verifyTypeReference(field.typeKey, owner + " field '" + field.name + "'")) return false;
            if (!verifyTypeReference(type.delegateReturnTypeKey, owner)) return false;
            for (const std::string& parameterTypeKey : type.delegateParameterTypeKeys)
                if (!verifyTypeReference(parameterTypeKey, owner)) return false;
        }
        for (const LibraryMetadataMember& member : metadata.members)
        {
            const std::string owner = "member '" + member.declaringTypeKey + "." + member.signatureKey + "'";
            if (!verifyTypeReference(member.declaredTypeKey, owner)) return false;
            for (const LibraryMetadataParameter& parameter : member.parameters)
                if (!verifyTypeReference(parameter.typeKey, owner + " parameter '" + parameter.name + "'"))
                    return false;
        }
        for (const LibraryMetadataNamespaceFunction& function :
             metadata.namespaceFunctions)
        {
            const std::string owner = "namespace function '" +
                function.namespaceName + "." + function.signatureKey + "'";
            if (!verifyTypeReference(function.declaredTypeKey, owner)) return false;
            for (const LibraryMetadataParameter& parameter : function.parameters)
            {
                if (!verifyTypeReference(
                        parameter.typeKey,
                        owner + " parameter '" + parameter.name + "'"))
                {
                    return false;
                }
            }
        }
        for (const LibraryMetadataNamespaceState& state : metadata.namespaceStates)
        {
            const std::string owner = "namespace state '" +
                state.namespaceName + "." + state.name + "'";
            if (!verifyTypeReference(state.declaredTypeKey, owner)) return false;
        }
        for (const LibraryMetadataGenericSpecialization& specialization : metadata.genericSpecializations)
        {
            const std::string owner = "generic specialization '" + specialization.specializationKey + "'";
            for (const std::string& argument : specialization.typeArguments)
            {
                if (argument.starts_with("type:") && !verifyTypeReference(argument, owner)) return false;
            }
        }
        for (const LibraryMetadataExport& exportRecord : metadata.exports)
        {
            const std::string owner = "export '" + exportRecord.cAbiName + "'";
            if (!verifyTypeReference(exportRecord.returnTypeKey, owner)) return false;
            for (const std::string& parameterTypeKey : exportRecord.parameterTypeKeys)
                if (!verifyTypeReference(parameterTypeKey, owner)) return false;
        }
    }
    std::unordered_set<std::string> namespaceFunctionKeys;
    for (const LibraryMetadataNamespaceFunction& function :
         metadata.namespaceFunctions)
    {
        const std::string key = function.namespaceName + "" +
            function.signatureKey;
        if (function.namespaceName.empty() || function.name.empty() ||
            function.signatureKey.empty() || function.declaredTypeKey.empty() ||
            function.mangledName.empty() ||
            !namespaceFunctionKeys.insert(key).second)
        {
            error = function.mangledName.empty()
                ? "metadata namespace function has no physical identity"
                : "metadata contains an incomplete or duplicate namespace function '" +
                    function.namespaceName + "." + function.signatureKey + "'";
            return false;
        }
    }
    std::unordered_set<std::string> namespaceStateKeys;
    for (const LibraryMetadataNamespaceState& state : metadata.namespaceStates)
    {
        const std::string key = state.namespaceName + std::string(1, static_cast<char>(31)) +
            state.name;
        if (state.accessibility != 1U || state.namespaceName.empty() ||
            state.name.empty() || state.declaredTypeKey.empty() ||
            state.storageName.empty() || state.accessorName.empty() ||
            !namespaceStateKeys.insert(key).second)
        {
            error = "metadata contains an incomplete or duplicate namespace state '" +
                state.namespaceName + "." + state.name + "'";
            return false;
        }
    }
    std::unordered_set<std::string> exportNames;
    for (const LibraryMetadataExport& exportRecord : metadata.exports)
    {
        if (exportRecord.cAbiName.empty() || !exportNames.insert(exportRecord.cAbiName).second)
        {
            error = exportRecord.cAbiName.empty()
                ? "metadata contains a C ABI export with an empty symbol name"
                : "metadata contains duplicate C ABI export '" + exportRecord.cAbiName + "'";
            return false;
        }
    }
    if (metadata.formatMinor >= 25)
    {
        const auto isLeafName = [](const std::string& value) {
            if (value.empty()) return false;
            const std::filesystem::path path(value);
            return path == path.filename() && path.filename() != "." && path.filename() != "..";
        };
        std::unordered_set<std::string> dependencyModules;
        for (const LibraryMetadataBinaryDependency& dependency : metadata.binaryDependencies)
        {
            const bool validRuntime = dependency.runtimeFileName.empty() ||
                isLeafName(dependency.runtimeFileName);
            const bool emptyFingerprint = std::ranges::all_of(
                dependency.abiFingerprint, [](std::uint8_t byte) { return byte == 0U; });
            if (dependency.moduleName.empty() || dependency.moduleName == metadata.moduleName ||
                !isLeafName(dependency.libraryFileName) ||
                !isLeafName(dependency.metadataFileName) || !validRuntime || emptyFingerprint ||
                !dependencyModules.insert(dependency.moduleName).second)
            {
                error = "metadata contains an incomplete, recursive, or duplicate binary "
                    "dependency for module '" + dependency.moduleName + "'";
                return false;
            }
        }
        std::unordered_set<std::string> platformImportKeys;
        for (const LibraryMetadataPlatformImport& importRecord : metadata.platformImports)
        {
            const std::string key = foldedAscii(importRecord.dllName) + "\n" +
                importRecord.symbolName;
            if (importRecord.dllName.empty() || importRecord.symbolName.empty() ||
                !platformImportKeys.insert(key).second)
            {
                error = "metadata contains an incomplete or duplicate private platform import '" +
                    importRecord.dllName + "!" + importRecord.symbolName + "'";
                return false;
            }
        }
    }
    // El productor conserva la identidad exacta del ejecutable para poder
    // rastrear el artefacto, pero el canal `-dev` y `+build...` NO son una
    // frontera de compatibilidad. Sin separar ambas preguntas, cada build
    // incremental rechazaria las bibliotecas del build inmediatamente anterior
    // aunque compartieran lenguaje, formato y ABI.
    const std::string_view producerRelease = compilerReleasePart(metadata.producerVersion);
    if (producerRelease != kCompilerReleaseVersion &&
        producerRelease != kPreviousCompilerReleaseVersion)
    {
        error = "metadata producer version '" + metadata.producerVersion +
            "' is outside the supported producer window ['" +
            std::string(kPreviousCompilerReleaseVersion) + "', '" +
            std::string(kCompilerReleaseVersion) + "']";
        return false;
    }
    if (metadata.formatMinor >= 13)
    {
        constexpr std::uint8_t kMethodKind = 2U;
        constexpr std::uint32_t kImportedC = 1U << 20U;
        for (const LibraryMetadataMember& member : metadata.members)
        {
            const std::uint32_t policy =
                (member.flags & kMetadataMemberInliningMask) >> kMetadataMemberInliningShift;
            if (!member.inlineSourceText.empty() &&
                (member.kind != kMethodKind || (member.flags & kImportedC) != 0U ||
                 (policy != static_cast<std::uint32_t>(frontend::InliningPolicy::Preferred) &&
                  policy != static_cast<std::uint32_t>(frontend::InliningPolicy::Required))))
            {
                error = "metadata member '" + member.declaringTypeKey + "." +
                    member.signatureKey +
                    "' carries an inline body without a compatible inline policy";
                return false;
            }
        }
    }
    if (metadata.formatMinor >= 14)
    {
        constexpr std::uint8_t kFieldKind = 0U;
        for (const LibraryMetadataMember& member : metadata.members)
        {
            if ((member.flags & kMetadataMemberVolatile) != 0U && member.kind != kFieldKind)
            {
                error = "metadata member '" + member.declaringTypeKey + "." +
                    member.signatureKey + "' carries volatile on a non-field member";
                return false;
            }
        }
    }
    if (metadata.formatMinor >= 15)
    {
        constexpr std::uint8_t kMethodKind = 2U;
        constexpr std::uint32_t kPropertyAccessor = 1U << 12U;
        constexpr std::uint32_t kIndexerAccessor = 1U << 13U;
        constexpr std::uint32_t kGetter = 1U << 14U;
        constexpr std::uint32_t kSetter = 1U << 15U;
        for (const LibraryMetadataMember& member : metadata.members)
        {
            if ((member.flags & kMetadataMemberReferenceReturn) != 0U &&
                (member.kind != kMethodKind ||
                 (member.flags & kPropertyAccessor) != 0U ||
                 (member.flags & kSetter) != 0U ||
                 ((member.flags & kIndexerAccessor) != 0U &&
                  (member.flags & kGetter) == 0U)))
            {
                error = "metadata member '" + member.declaringTypeKey + "." +
                    member.signatureKey + "' carries ref-return outside an ordinary method or "
                    "indexer getter";
                return false;
            }
        }
    }
    if (metadata.formatMinor >= 16)
    {
        constexpr std::uint8_t kMethodKind = 2U;
        constexpr std::uint8_t kConstructorKind = 3U;
        for (const LibraryMetadataMember& member : metadata.members)
        {
            if ((member.flags & kMetadataMemberNoHeap) != 0U &&
                member.kind != kMethodKind && member.kind != kConstructorKind)
            {
                error = "metadata member '" + member.declaringTypeKey + "." +
                    member.signatureKey + "' carries noheap outside a method or constructor";
                return false;
            }
        }
    }
    if (metadata.formatMinor >= 19)
    {
        constexpr std::uint8_t kMethodKind = 2U;
        constexpr std::uint32_t kExternC = 1U << 8U;
        constexpr std::uint32_t kImportedC = 1U << 20U;
        for (const LibraryMetadataMember& member : metadata.members)
        {
            if ((member.flags & kMetadataMemberVariadic) != 0U &&
                (member.kind != kMethodKind ||
                 (member.flags & kExternC) == 0U ||
                 (member.flags & kImportedC) == 0U ||
                 member.parameters.empty()))
            {
                error = "metadata member '" + member.declaringTypeKey + "." +
                    member.signatureKey +
                    "' carries variadic outside an imported C method with fixed parameters";
                return false;
            }
        }
    }
    if (metadata.formatMinor >= 24)
    {
        const auto verifyParameterPacks = [&](const auto& callable, std::string_view owner) -> bool
        {
            for (std::size_t index = 0; index < callable.parameters.size(); ++index)
            {
                const LibraryMetadataParameter& parameter = callable.parameters[index];
                if (!parameter.isParameterPack) continue;
                if (index + 1U != callable.parameters.size() ||
                    parameter.passKind != LibraryMetadataParameterPassKind::Value ||
                    parameter.defaultValueKind != LibraryMetadataDefaultValueKind::None ||
                    parameter.typeKey.find("ReadOnlySpan__g1__") == std::string::npos)
                {
                    error = "metadata " + std::string(owner) + " parameter '" + parameter.name +
                        "' carries an invalid params ReadOnlySpan<T> contract";
                    return false;
                }
            }
            return true;
        };
        for (const LibraryMetadataMember& member : metadata.members)
        {
            if (!verifyParameterPacks(
                    member,
                    "member '" + member.declaringTypeKey + "." + member.signatureKey + "'"))
                return false;
        }
        for (const LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
        {
            if (!verifyParameterPacks(
                    function,
                    "namespace function '" + function.namespaceName + "." +
                        function.signatureKey + "'"))
                return false;
        }
    }
    return true;
}

std::string LibraryMetadataCodec::fingerprintHex(const std::array<std::uint8_t, 32>& fingerprint)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const std::uint8_t byte : fingerprint)
    {
        stream << std::setw(2) << static_cast<unsigned>(byte);
    }
    return stream.str();
}
}
