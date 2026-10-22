#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Interop/LibraryMetadataMerger.h"

#include "nativesharp/Basic/ArtifactTransaction.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Compilation/Project.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include "Internal/TemplateSourceReplacement.h"
#include "Internal/TemplateSourcePiece.h"
#include "Internal/ByteReader.h"
#include "Internal/ByteWriter.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Internal/LibraryMetadataCodecSupport.h"

namespace nativesharp::interop::metadata_detail
{

std::string foldedAscii(std::string value)
{
    std::ranges::transform(value, value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}
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

std::uint8_t metadataAccessibility(frontend::NamedTypeAccessibility accessibility) noexcept
{
    // NSMETA accessibility values predate package/module visibility and are shared with member
    // accessibility. Keep that stable wire contract instead of casting the expanded frontend enum.
    switch (accessibility)
    {
        case frontend::NamedTypeAccessibility::Private: return 0U;
        case frontend::NamedTypeAccessibility::Protected: return 1U;
        case frontend::NamedTypeAccessibility::Internal:
        case frontend::NamedTypeAccessibility::Package: return 2U;
        case frontend::NamedTypeAccessibility::Public: return 3U;
    }
    return 0U;
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

LibraryMetadataEscapeClass metadataEscapeClass(language::EscapeClass escapeClass) noexcept
{
    return static_cast<LibraryMetadataEscapeClass>(escapeClass);
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
    if (type.kind == frontend::NamedTypeDeclarationKind::Interface && type.syntax != nullptr)
    {
        const std::string_view metadata = type.syntax->auxiliaryText();
        if (metadata == "ns3-trait" ||
            metadata.find("nsc-meta:ns3-trait") != std::string_view::npos)
            flags |= kMetadataTypeTrait;
    }
    if (type.syntax != nullptr)
    {
        const std::string_view metadata = type.syntax->auxiliaryText();
        const auto hasListItem = [](std::string_view list, std::string_view item) noexcept
        {
            std::size_t cursor = 0;
            while (cursor <= list.size())
            {
                const std::size_t separator = list.find('|', cursor);
                if (list.substr(
                        cursor,
                        separator == std::string_view::npos ? std::string_view::npos
                                                            : separator - cursor) == item)
                {
                    return true;
                }
                if (separator == std::string_view::npos) break;
                cursor = separator + 1;
            }
            return false;
        };
        const auto hasMarker = [](std::string_view text, std::string_view marker) noexcept
        {
            std::size_t position = text.find(marker);
            while (position != std::string_view::npos)
            {
                const bool beginsItem = position == 0 || text[position - 1] == ',' ||
                    text[position - 1] == ':';
                const std::size_t end = position + marker.size();
                const bool endsItem = end == text.size() || text[end] == ',' || text[end] == ';';
                if (beginsItem && endsItem) return true;
                position = text.find(marker, position + 1);
            }
            return false;
        };
        const bool isRecord = hasMarker(metadata, "record");
        if (isRecord)
        {
            flags |= kMetadataTypeRecord;
            const std::size_t marker = metadata.find("derive=");
            if (marker != std::string_view::npos)
            {
                const std::size_t begin = marker + std::string_view("derive=").size();
                const std::size_t end = metadata.find_first_of(",;", begin);
                const std::string_view derives = metadata.substr(
                    begin, (end == std::string_view::npos ? metadata.size() : end) - begin);
                if (hasListItem(derives, "Equality")) flags |= kMetadataTypeDeriveEquality;
                if (hasListItem(derives, "Hash")) flags |= kMetadataTypeDeriveHash;
                if (hasListItem(derives, "Print")) flags |= kMetadataTypeDerivePrint;
                if (hasListItem(derives, "Clone")) flags |= kMetadataTypeDeriveClone;
            }
        }
        if (hasMarker(metadata, "variant")) flags |= kMetadataTypeVariant;
        if (metadata.starts_with("opaque-result|")) flags |= kMetadataTypeOpaqueResult;
        if (hasMarker(metadata, "copy")) flags |= kMetadataTypeCopy;
    }
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
    if (member.declaredTypeSyntax != nullptr && member.declaredTypeSyntax->isDynamicTrait())
        flags |= kMetadataMemberDynamicTrait;
    return flags;
}

std::string signatureForMember(
    std::span<const frontend::ClassMemberSymbol> members,
    std::optional<std::size_t> ordinal)
{
    if (!ordinal.has_value() || *ordinal >= members.size()) return {};
    return members[*ordinal].signatureKey;
}

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

void initializeGenericTemplateSource(
    const frontend::ProjectCompilation& compilation,
    const frontend::GenericTemplateRecord& generic,
    std::string& source,
    std::vector<TemplateSourcePiece>& pieces)
{
    const SourceRange rootRange = generic.syntax->range();
    source = std::string(compilation.sourceManager().getText(rootRange));
    pieces.push_back(TemplateSourcePiece{rootRange, 0});
    for (const ast::AstNode* extension : generic.extensionSyntaxes)
    {
        if (extension == nullptr || !extension->range().isValid()) continue;
        source.push_back('\n');
        const std::size_t outputBegin = source.size();
        source += compilation.sourceManager().getText(extension->range());
        pieces.push_back(TemplateSourcePiece{extension->range(), outputBegin});
    }
}

bool appendTemplateSourceReplacement(
    SourceRange range,
    std::size_t localBegin,
    std::size_t localEnd,
    std::string text,
    std::span<const TemplateSourcePiece> pieces,
    std::vector<TemplateSourceReplacement>& replacements,
    const frontend::GenericTemplateRecord& generic,
    std::string& error)
{
    const auto piece = std::ranges::find_if(pieces, [&](const TemplateSourcePiece& candidate)
    {
        return rangeIsInside(range, candidate.range);
    });
    if (piece == pieces.end() || localBegin >= localEnd ||
        localEnd > range.end().offset() - range.begin().offset())
    {
        error = "cannot canonicalize an external type reference in reusable template '" +
            generic.canonicalKey + "'";
        return false;
    }
    replacements.push_back({
        piece->outputBegin +
            static_cast<std::size_t>(range.begin().offset() - piece->range.begin().offset()) +
            localBegin,
        piece->outputBegin +
            static_cast<std::size_t>(range.begin().offset() - piece->range.begin().offset()) +
            localEnd,
        std::move(text)});
    return true;
}

bool collectTemplateSourceReplacements(
    const frontend::ProjectCompilation& compilation,
    const frontend::GenericTemplateRecord& generic,
    std::span<const TemplateSourcePiece> pieces,
    std::vector<TemplateSourceReplacement>& replacements,
    std::unordered_set<std::string>& externalTypeKeys,
    std::string& error)
{
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
        if (!appendTemplateSourceReplacement(
                typeRange,
                baseBegin,
                baseEnd,
                sourceType,
                pieces,
                replacements,
                generic,
                error))
        {
            return false;
        }
    }
    return true;
}

bool applyTemplateSourceReplacements(
    const frontend::GenericTemplateRecord& generic,
    std::vector<TemplateSourceReplacement>& replacements,
    std::string& source,
    std::string& error)
{
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

bool materializeGenericTemplateSource(
    const frontend::ProjectCompilation& compilation,
    const frontend::GenericTemplateRecord& generic,
    std::string& source,
    std::unordered_set<std::string>& externalTypeKeys,
    std::string& error)
{
    std::vector<TemplateSourcePiece> pieces;
    initializeGenericTemplateSource(compilation, generic, source, pieces);
    std::vector<TemplateSourceReplacement> replacements;
    return collectTemplateSourceReplacements(
               compilation, generic, pieces, replacements, externalTypeKeys, error) &&
        applyTemplateSourceReplacements(generic, replacements, source, error);
}

bool binaryDependencyLess(
    const LibraryMetadataBinaryDependency& left,
    const LibraryMetadataBinaryDependency& right)
{
    return std::tie(left.moduleName, left.metadataFileName) <
        std::tie(right.moduleName, right.metadataFileName);
}

bool platformImportLess(
    const LibraryMetadataPlatformImport& left,
    const LibraryMetadataPlatformImport& right)
{
    return std::tie(left.dllName, left.symbolName) <
        std::tie(right.dllName, right.symbolName);
}

bool samePlatformImport(
    const LibraryMetadataPlatformImport& left,
    const LibraryMetadataPlatformImport& right)
{
    return left.dllName == right.dllName && left.symbolName == right.symbolName;
}

bool dispatchInterfaceLess(
    const LibraryMetadataDispatchInterface& left,
    const LibraryMetadataDispatchInterface& right)
{
    return left.interfaceTypeKey < right.interfaceTypeKey;
}

bool dispatchMetadataLess(
    const LibraryMetadataDispatch& left,
    const LibraryMetadataDispatch& right)
{
    return left.typeKey < right.typeKey;
}

bool exportMetadataLess(
    const LibraryMetadataExport& left,
    const LibraryMetadataExport& right)
{
    return left.cAbiName < right.cAbiName;
}

bool genericTemplateMetadataLess(
    const LibraryMetadataGenericTemplate& left,
    const LibraryMetadataGenericTemplate& right)
{
    return left.canonicalKey < right.canonicalKey;
}

bool genericSpecializationMetadataLess(
    const LibraryMetadataGenericSpecialization& left,
    const LibraryMetadataGenericSpecialization& right)
{
    return left.specializationKey < right.specializationKey;
}

bool metadataTypeOrdinalLess(
    std::span<const frontend::NamedTypeDeclaration> types,
    std::size_t left,
    std::size_t right)
{
    return types[left].identity.key() < types[right].identity.key();
}

bool isParameterDeclaration(const ast::AstNode* candidate)
{
    return candidate != nullptr &&
        candidate->kind() == ast::AstNodeKind::ParameterDeclaration;
}

bool appendBinaryDependencies(
    const frontend::ProjectCompilation& compilation,
    LibraryMetadata& metadata,
    std::string& error)
{
    // NSMETA 1.25: retain the direct binary closure of a produced static archive. A static archive
    // has not consumed its link dependencies yet, so its final consumer must expand these edges.
    // A DLL has already linked its static inputs and exposes dynamic dependencies through its PE
    // import table; publishing those inputs here would make a DLL consumer relink private modules
    // and incorrectly require their .lib/.nsmeta sidecars beside the DLL.
    if (compilation.projectGraph() == nullptr ||
        compilation.projectGraph()->rootProject() == nullptr ||
        compilation.projectGraph()->rootProject()->outputKind() !=
            compilation::ProjectOutputKind::Library)
    {
        return true;
    }

    LibraryMetadataCodec codec;
    for (const compilation::LibraryReference& reference :
         compilation.projectGraph()->rootProject()->libraryReferences())
    {
        std::string dependencyError;
        std::shared_ptr<const LibraryMetadata> decoded =
            compilation.libraryMetadataRepository().decode(
            reference.metadataPath,
            [&codec](
                std::span<const std::uint8_t> bytes,
                LibraryMetadata& result,
                std::string& decodeError) {
                return codec.read(bytes, result, decodeError);
            },
            dependencyError);
        if (decoded == nullptr)
        {
            error = "cannot record binary dependency '" +
                reference.metadataPath.generic_string() + "': " + dependencyError;
            return false;
        }
        if (!compilation.libraryMetadataRepository().verify(
                reference.metadataPath,
                [&codec](const LibraryMetadata& candidate, std::string& verificationError) {
                    return codec.verifyCurrentCompiler(candidate, verificationError);
                },
                dependencyError))
        {
            error = "cannot record binary dependency '" +
                reference.metadataPath.generic_string() + "': " + dependencyError;
            return false;
        }
        const LibraryMetadata& dependencyMetadata = *decoded;
        LibraryMetadataBinaryDependency dependency;
        dependency.moduleName = dependencyMetadata.moduleName;
        dependency.libraryFileName = reference.libraryPath.filename().generic_string();
        dependency.metadataFileName = reference.metadataPath.filename().generic_string();
        dependency.runtimeFileName = reference.runtimePath.empty()
            ? std::string{} : reference.runtimePath.filename().generic_string();
        dependency.abiFingerprint = dependencyMetadata.abiFingerprint;
        metadata.binaryDependencies.push_back(std::move(dependency));
    }
    std::sort(
        metadata.binaryDependencies.begin(),
        metadata.binaryDependencies.end(),
        binaryDependencyLess);
    return true;
}

void collectReferencedCallees(
    const frontend::ProjectOopMirNode* node,
    std::unordered_set<std::string>& referencedCallees)
{
    if (node == nullptr) return;
    if (!node->calleeName.empty()) referencedCallees.insert(node->calleeName);
    for (const auto& child : node->children)
    {
        collectReferencedCallees(child.get(), referencedCallees);
    }
}

void appendPlatformImports(
    const frontend::ProjectCompilation& compilation,
    LibraryMetadata& metadata)
{
    std::unordered_set<std::string> referencedCallees;
    for (const frontend::ProjectOopMirFunction& function :
         compilation.oopMirModel()->functions())
    {
        collectReferencedCallees(function.body.get(), referencedCallees);
    }
    bool needsRuntimeBinding = false;
    for (const frontend::ProjectOopMirFunction& function :
         compilation.oopMirModel()->functions())
    {
        if (!function.isImportedC || function.importDllName.empty() ||
            !referencedCallees.contains(function.mangledName))
        {
            continue;
        }
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
    std::sort(
        metadata.platformImports.begin(),
        metadata.platformImports.end(),
        platformImportLess);
    metadata.platformImports.erase(
        std::unique(metadata.platformImports.begin(), metadata.platformImports.end(),
            samePlatformImport),
        metadata.platformImports.end());
}

bool isPubliclyReachableRootType(
    std::span<const frontend::NamedTypeDeclaration> types,
    std::size_t rootProjectOrdinal,
    std::size_t typeOrdinal)
{
    while (typeOrdinal < types.size())
    {
        const frontend::NamedTypeDeclaration& type = types[typeOrdinal];
        if (type.projectOrdinal != rootProjectOrdinal ||
            type.accessibility != frontend::NamedTypeAccessibility::Public)
        {
            return false;
        }
        if (!type.containingTypeOrdinal.has_value()) return true;
        typeOrdinal = *type.containingTypeOrdinal;
    }
    return false;
}

void appendMetadataDispatchTables(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    std::span<const frontend::ClassMemberSymbol> members,
    std::size_t rootProjectOrdinal,
    LibraryMetadata& metadata)
{
    for (const frontend::ProjectOopMirDispatchTable& table :
         compilation.oopMirModel()->dispatchTables())
    {
        if (!isPubliclyReachableRootType(types, rootProjectOrdinal, table.typeOrdinal)) continue;
        LibraryMetadataDispatch record;
        record.typeKey = table.typeKey;
        record.stableTypeId = table.stableTypeId;
        record.descriptorName = table.descriptorName;
        record.descriptorAccessorName = table.descriptorAccessorName;
        record.vtableName = table.vtableName;
        record.destructionEntryName = table.destructionEntryName;
        for (std::size_t index = 0; index < table.slotMemberOrdinals.size(); ++index)
        {
            record.slotSignatures.push_back(
                signatureForMember(members, table.slotMemberOrdinals[index]));
            record.slotCalleeNames.push_back(
                index < table.slotCalleeNames.size()
                    ? table.slotCalleeNames[index]
                    : std::string{});
        }
        for (const frontend::ProjectOopMirDispatchTable::InterfaceTable& tableInterface :
             table.interfaceTables)
        {
            LibraryMetadataDispatchInterface interfaceRecord;
            interfaceRecord.interfaceTypeKey = tableInterface.interfaceTypeKey;
            interfaceRecord.stableInterfaceTypeId = tableInterface.stableInterfaceTypeId;
            interfaceRecord.methodTableName = tableInterface.methodTableName;
            for (const std::size_t memberOrdinal : tableInterface.interfaceMemberOrdinals)
                interfaceRecord.interfaceSignatures.push_back(
                    signatureForMember(members, memberOrdinal));
            for (const std::size_t memberOrdinal : tableInterface.implementationMemberOrdinals)
                interfaceRecord.implementationSignatures.push_back(
                    signatureForMember(members, memberOrdinal));
            interfaceRecord.calleeNames = tableInterface.calleeNames;
            record.interfaces.push_back(std::move(interfaceRecord));
        }
        std::sort(
            record.interfaces.begin(), record.interfaces.end(), dispatchInterfaceLess);
        metadata.dispatchTables.push_back(std::move(record));
    }
    std::sort(
        metadata.dispatchTables.begin(), metadata.dispatchTables.end(), dispatchMetadataLess);
}

void appendMetadataExports(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    std::span<const frontend::ClassMemberSymbol> members,
    std::size_t rootProjectOrdinal,
    LibraryMetadata& metadata)
{
    for (const frontend::ProjectOopMirFunction& function :
         compilation.oopMirModel()->functions())
    {
        if (function.isLambda || !function.isExportedC ||
            !isPubliclyReachableRootType(
                types, rootProjectOrdinal, function.declaringTypeOrdinal) ||
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
    std::sort(metadata.exports.begin(), metadata.exports.end(), exportMetadataLess);
}

void rememberExternalMetadataType(
    const std::string& writtenTypeKey,
    const LibraryMetadata& metadata,
    const std::unordered_set<std::string>& declaredTypeKeys,
    const std::unordered_set<std::string>& emittedSpecializedTypeKeys,
    std::unordered_set<std::string>& externalTypeKeys)
{
    const std::string typeKey = metadataBaseTypeKey(writtenTypeKey);
    if (declaredTypeKeys.contains(typeKey) || emittedSpecializedTypeKeys.contains(typeKey)) return;
    const std::optional<std::string> module = metadataTypeModule(typeKey);
    if (module.has_value() && *module != metadata.moduleName) externalTypeKeys.insert(typeKey);
}

void finalizeExternalMetadataTypes(
    LibraryMetadata& metadata,
    const std::unordered_set<std::string>& declaredTypeKeys,
    const std::unordered_set<std::string>& emittedSpecializedTypeKeys,
    std::unordered_set<std::string>& externalTypeKeys)
{
    for (const LibraryMetadataDispatch& dispatch : metadata.dispatchTables)
    {
        rememberExternalMetadataType(
            dispatch.typeKey, metadata, declaredTypeKeys, emittedSpecializedTypeKeys, externalTypeKeys);
        for (const LibraryMetadataDispatchInterface& interface : dispatch.interfaces)
            rememberExternalMetadataType(
                interface.interfaceTypeKey,
                metadata,
                declaredTypeKeys,
                emittedSpecializedTypeKeys,
                externalTypeKeys);
    }
    for (const LibraryMetadataExport& exportRecord : metadata.exports)
    {
        rememberExternalMetadataType(
            exportRecord.declaringTypeKey,
            metadata,
            declaredTypeKeys,
            emittedSpecializedTypeKeys,
            externalTypeKeys);
        rememberExternalMetadataType(
            exportRecord.returnTypeKey,
            metadata,
            declaredTypeKeys,
            emittedSpecializedTypeKeys,
            externalTypeKeys);
        for (const std::string& parameterTypeKey : exportRecord.parameterTypeKeys)
            rememberExternalMetadataType(
                parameterTypeKey,
                metadata,
                declaredTypeKeys,
                emittedSpecializedTypeKeys,
                externalTypeKeys);
    }
    metadata.externalTypeKeys.assign(externalTypeKeys.begin(), externalTypeKeys.end());
    std::sort(metadata.externalTypeKeys.begin(), metadata.externalTypeKeys.end());
}

void appendSyntheticImplicitConstructors(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    const std::unordered_set<std::size_t>& rootTypes,
    LibraryMetadata& metadata)
{
    for (const frontend::ProjectOopMirFunction& function :
         compilation.oopMirModel()->functions())
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
        record.accessibility =
            static_cast<std::uint8_t>(frontend::ClassMemberAccessibility::Public);
        record.declaringTypeKey = "type:" + type.identity.key();
        record.name = typeName;
        record.signatureKey = typeName + "()";
        record.declaredTypeKey = "builtin:void";
        record.mangledName = function.mangledName;
        metadata.members.push_back(std::move(record));
    }
}

bool includeGenericTemplateClosure(
    const frontend::GenericTemplateRecord& generic,
    const frontend::GenericTemplateRecord& publicRoot,
    std::size_t rootProjectOrdinal,
    const std::unordered_map<std::string, const frontend::GenericTemplateRecord*>& genericByKey,
    std::unordered_set<std::string>& includedTemplateKeys,
    std::unordered_set<std::string>& visiting,
    std::vector<std::string>& chain,
    std::string& error)
{
    if (generic.projectOrdinal != rootProjectOrdinal) return true;
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
        if (!includeGenericTemplateClosure(
                *dependency->second,
                publicRoot,
                rootProjectOrdinal,
                genericByKey,
                includedTemplateKeys,
                visiting,
                chain,
                error))
        {
            return false;
        }
    }
    chain.pop_back();
    visiting.erase(generic.canonicalKey);
    includedTemplateKeys.insert(generic.canonicalKey);
    return true;
}

bool collectIncludedGenericTemplateKeys(
    std::span<const frontend::GenericTemplateRecord> genericTemplates,
    std::size_t rootProjectOrdinal,
    std::unordered_set<std::string>& includedTemplateKeys,
    std::string& error)
{
    std::unordered_map<std::string, const frontend::GenericTemplateRecord*> genericByKey;
    for (const frontend::GenericTemplateRecord& generic : genericTemplates)
        genericByKey.emplace(generic.canonicalKey, &generic);

    for (const frontend::GenericTemplateRecord& publicRoot : genericTemplates)
    {
        if (publicRoot.projectOrdinal != rootProjectOrdinal ||
            publicRoot.accessibility != frontend::GenericTemplateAccessibility::Public)
        {
            continue;
        }
        std::vector<std::string> chain;
        std::unordered_set<std::string> visiting;
        if (!includeGenericTemplateClosure(
                publicRoot,
                publicRoot,
                rootProjectOrdinal,
                genericByKey,
                includedTemplateKeys,
                visiting,
                chain,
                error))
        {
            return false;
        }
    }
    return true;
}

std::string canonicalMetadataType(
    const frontend::ProjectCompilation& compilation,
    const ast::TypeSyntax* syntax)
{
    return syntax == nullptr
        ? std::string{}
        : compilation.memberModel()->canonicalTypeKey(syntax, *compilation.nameLookup());
}

LibraryMetadataGenericTemplate::ParameterContract buildGenericParameterContract(
    const frontend::ProjectCompilation& compilation,
    const frontend::GenericTemplateRecord& generic,
    const ast::GenericParameterSyntax& parameter)
{
    LibraryMetadataGenericTemplate::ParameterContract contract;
    contract.name = parameter.name;
    contract.kind = static_cast<std::uint8_t>(parameter.kind);
    contract.constantTypeKey = canonicalMetadataType(compilation, parameter.constantType);
    const auto clause = std::ranges::find(
        generic.constraints, parameter.name, &ast::GenericConstraintClause::parameterName);
    if (clause == generic.constraints.end()) return contract;

    if (clause->requiresClass) contract.flags |= kMetadataGenericRequiresClass;
    if (clause->requiresValueType) contract.flags |= kMetadataGenericRequiresValueType;
    if (clause->requiresDefaultConstructor)
        contract.flags |= kMetadataGenericRequiresDefaultConstructor;
    if (clause->requiresCopy) contract.flags |= kMetadataGenericRequiresCopy;
    if (clause->requiresMove) contract.flags |= kMetadataGenericRequiresMove;
    if (clause->requiresTrackedLifetime) contract.flags |= kMetadataGenericRequiresDrop;
    if (clause->requiresUnmanaged) contract.flags |= kMetadataGenericRequiresUnmanaged;
    if (clause->requiresNotNull) contract.flags |= kMetadataGenericRequiresNotNull;
    contract.equalityTypeKey = canonicalMetadataType(compilation, clause->equalityConstraint);
    contract.associatedProjectionKey =
        canonicalMetadataType(compilation, clause->associatedProjection);
    for (const ast::TypeSyntax* constraint : clause->typeConstraints)
        contract.typeConstraintKeys.push_back(canonicalMetadataType(compilation, constraint));
    std::ranges::sort(contract.typeConstraintKeys);
    return contract;
}

bool buildGenericTemplateMetadata(
    const frontend::ProjectCompilation& compilation,
    const frontend::GenericTemplateRecord& generic,
    LibraryMetadataGenericTemplate& record,
    std::unordered_set<std::string>& templateExternalTypeKeys,
    std::string& error)
{
    record.kind = static_cast<std::uint8_t>(generic.kind);
    record.accessibility = static_cast<std::uint8_t>(generic.accessibility);
    record.canonicalKey = generic.canonicalKey;
    record.moduleName = generic.moduleName;
    record.namespaceName = generic.namespaceName;
    record.ownerName = generic.ownerName;
    record.name = generic.name;
    record.parameters = generic.parameters;
    record.parameterContracts.reserve(generic.parameterSyntaxes.size());
    for (const ast::GenericParameterSyntax& parameter : generic.parameterSyntaxes)
        record.parameterContracts.push_back(
            buildGenericParameterContract(compilation, generic, parameter));
    for (const ast::LifetimeParameterSyntax& lifetime : generic.syntax->lifetimeParameters())
        record.lifetimeParameters.push_back(lifetime.name);
    for (const ast::LifetimeOutlivesConstraint& edge :
         generic.syntax->lifetimeOutlivesConstraints())
    {
        record.lifetimeOutlivesConstraints.emplace_back(edge.longer, edge.shorter);
    }
    if (!materializeGenericTemplateSource(
            compilation, generic, record.sourceText, templateExternalTypeKeys, error))
    {
        return false;
    }
    record.dependencies = generic.dependencies;
    return true;
}

bool appendGenericTemplates(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::GenericTemplateRecord> genericTemplates,
    const std::unordered_set<std::string>& includedTemplateKeys,
    LibraryMetadata& metadata,
    std::unordered_set<std::string>& rootTemplateKeys,
    std::unordered_set<std::string>& templateExternalTypeKeys,
    std::string& error)
{
    for (const frontend::GenericTemplateRecord& generic : genericTemplates)
    {
        if (!includedTemplateKeys.contains(generic.canonicalKey) || generic.syntax == nullptr)
            continue;
        LibraryMetadataGenericTemplate record;
        if (!buildGenericTemplateMetadata(
                compilation, generic, record, templateExternalTypeKeys, error))
        {
            return false;
        }
        rootTemplateKeys.insert(record.canonicalKey);
        metadata.genericTemplates.push_back(std::move(record));
    }
    std::sort(
        metadata.genericTemplates.begin(),
        metadata.genericTemplates.end(),
        genericTemplateMetadataLess);
    return true;
}

void rememberReferencedMetadataType(
    const std::string& typeKey,
    std::unordered_set<std::string>& referencedTypeKeys)
{
    if (const std::optional<language::FunctionType> functionType =
            language::parseFunctionTypeIdentity(typeKey))
    {
        for (const std::string& parameter : functionType->parameters)
            rememberReferencedMetadataType(parameter, referencedTypeKeys);
        rememberReferencedMetadataType(functionType->result, referencedTypeKeys);
        rememberReferencedMetadataType(functionType->errorType, referencedTypeKeys);
        return;
    }
    const std::string base = metadataBaseTypeKey(typeKey);
    if (base.starts_with("type:")) referencedTypeKeys.insert(base);
}

void collectReferencedMetadataTypes(
    const LibraryMetadata& metadata,
    std::unordered_set<std::string>& referencedTypeKeys)
{
    for (const LibraryMetadataType& type : metadata.types)
    {
        rememberReferencedMetadataType(type.baseTypeKey, referencedTypeKeys);
        for (const std::string& interfaceTypeKey : type.interfaceTypeKeys)
            rememberReferencedMetadataType(interfaceTypeKey, referencedTypeKeys);
        for (const LibraryMetadataField& field : type.fields)
            rememberReferencedMetadataType(field.typeKey, referencedTypeKeys);
        rememberReferencedMetadataType(type.delegateReturnTypeKey, referencedTypeKeys);
        rememberReferencedMetadataType(type.delegateErrorTypeKey, referencedTypeKeys);
        for (const std::string& parameterTypeKey : type.delegateParameterTypeKeys)
            rememberReferencedMetadataType(parameterTypeKey, referencedTypeKeys);
        for (const LibraryMetadataVariantCase& variantCase : type.variantCases)
            rememberReferencedMetadataType(variantCase.payloadTypeKey, referencedTypeKeys);
        for (const LibraryMetadataAssociatedItem& item : type.associatedItems)
            rememberReferencedMetadataType(item.typeKey, referencedTypeKeys);
    }
    for (const LibraryMetadataMember& member : metadata.members)
    {
        rememberReferencedMetadataType(member.declaredTypeKey, referencedTypeKeys);
        rememberReferencedMetadataType(member.declaredErrorTypeKey, referencedTypeKeys);
        for (const LibraryMetadataParameter& parameter : member.parameters)
            rememberReferencedMetadataType(parameter.typeKey, referencedTypeKeys);
    }
    for (const LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
    {
        rememberReferencedMetadataType(function.declaredTypeKey, referencedTypeKeys);
        rememberReferencedMetadataType(function.declaredErrorTypeKey, referencedTypeKeys);
        for (const LibraryMetadataParameter& parameter : function.parameters)
            rememberReferencedMetadataType(parameter.typeKey, referencedTypeKeys);
    }
    for (const LibraryMetadataNamespaceState& state : metadata.namespaceStates)
        rememberReferencedMetadataType(state.declaredTypeKey, referencedTypeKeys);
}

void collectRequiredSpecializationKeys(
    std::span<const frontend::GenericSpecializationRecord> specializations,
    const std::unordered_set<std::string>& referencedTypeKeys,
    std::unordered_set<std::string>& requiredSpecializationKeys)
{
    for (const frontend::GenericSpecializationRecord& specialization : specializations)
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
        for (const frontend::GenericSpecializationRecord& specialization : specializations)
        {
            if (!requiredSpecializationKeys.contains(specialization.specializationKey)) continue;
            for (const std::string& argument : specialization.typeArguments)
            {
                if (!argument.starts_with("generic:")) continue;
                const std::string dependencyKey = argument.substr(8U);
                if (requiredSpecializationKeys.insert(dependencyKey).second)
                    addedDependency = true;
            }
        }
    }
}

void appendGenericSpecializations(
    std::span<const frontend::GenericSpecializationRecord> specializations,
    const std::unordered_set<std::string>& rootTemplateKeys,
    const std::unordered_set<std::string>& requiredSpecializationKeys,
    LibraryMetadata& metadata,
    std::unordered_set<std::string>& emittedSpecializedTypeKeys,
    std::unordered_set<std::string>& externalTypeKeys)
{
    for (const frontend::GenericSpecializationRecord& specialization : specializations)
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
    std::sort(
        metadata.genericSpecializations.begin(),
        metadata.genericSpecializations.end(),
        genericSpecializationMetadataLess);
}

void collectExternalGenericTypes(
    const LibraryMetadata& metadata,
    const std::unordered_set<std::string>& referencedTypeKeys,
    const std::unordered_set<std::string>& emittedSpecializedTypeKeys,
    std::unordered_set<std::string>& declaredTypeKeys,
    std::unordered_set<std::string>& externalTypeKeys)
{
    for (const LibraryMetadataType& type : metadata.types)
        declaredTypeKeys.insert("type:" + type.identityKey);
    for (const std::string& typeKey : referencedTypeKeys)
    {
        if (declaredTypeKeys.contains(typeKey) ||
            emittedSpecializedTypeKeys.contains(typeKey))
        {
            continue;
        }
        const std::optional<std::string> module = metadataTypeModule(typeKey);
        if (module.has_value() && *module != metadata.moduleName) externalTypeKeys.insert(typeKey);
    }
    // Type arguments are part of a specialization's public identity even when
    // no ordinary field or callable names them directly.
    for (const LibraryMetadataGenericSpecialization& specialization :
         metadata.genericSpecializations)
    {
        for (const std::string& argument : specialization.typeArguments)
        {
            if (!argument.starts_with("type:")) continue;
            const std::string typeKey = metadataBaseTypeKey(argument);
            if (declaredTypeKeys.contains(typeKey) ||
                emittedSpecializedTypeKeys.contains(typeKey))
            {
                continue;
            }
            const std::optional<std::string> module = metadataTypeModule(typeKey);
            if (module.has_value() && *module != metadata.moduleName)
                externalTypeKeys.insert(typeKey);
        }
    }
}

bool buildGenericMetadata(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    const std::unordered_set<std::size_t>& rootTypes,
    std::size_t rootProjectOrdinal,
    LibraryMetadata& metadata,
    std::unordered_set<std::string>& declaredTypeKeys,
    std::unordered_set<std::string>& emittedSpecializedTypeKeys,
    std::unordered_set<std::string>& externalTypeKeys,
    std::string& error)
{
    appendSyntheticImplicitConstructors(compilation, types, rootTypes, metadata);
    const auto genericTemplates = compilation.genericModel()->templates();
    std::unordered_set<std::string> includedTemplateKeys;
    if (!collectIncludedGenericTemplateKeys(
            genericTemplates, rootProjectOrdinal, includedTemplateKeys, error))
    {
        return false;
    }
    std::unordered_set<std::string> rootTemplateKeys;
    std::unordered_set<std::string> templateExternalTypeKeys;
    if (!appendGenericTemplates(
            compilation,
            genericTemplates,
            includedTemplateKeys,
            metadata,
            rootTemplateKeys,
            templateExternalTypeKeys,
            error))
    {
        return false;
    }
    std::unordered_set<std::string> referencedTypeKeys;
    collectReferencedMetadataTypes(metadata, referencedTypeKeys);
    std::unordered_set<std::string> requiredSpecializationKeys;
    const auto specializations = compilation.genericModel()->specializations();
    collectRequiredSpecializationKeys(
        specializations, referencedTypeKeys, requiredSpecializationKeys);
    externalTypeKeys.insert(
        templateExternalTypeKeys.begin(), templateExternalTypeKeys.end());
    appendGenericSpecializations(
        specializations,
        rootTemplateKeys,
        requiredSpecializationKeys,
        metadata,
        emittedSpecializedTypeKeys,
        externalTypeKeys);
    collectExternalGenericTypes(
        metadata,
        referencedTypeKeys,
        emittedSpecializedTypeKeys,
        declaredTypeKeys,
        externalTypeKeys);
    return true;
}

bool publishRootTypeKey(
    std::string typeKey,
    const std::unordered_map<std::string, std::size_t>& rootTypeByKey,
    std::unordered_set<std::size_t>& publishedRootTypes)
{
    typeKey = metadataBaseTypeKey(std::move(typeKey));
    const auto found = rootTypeByKey.find(typeKey);
    return found != rootTypeByKey.end() &&
        publishedRootTypes.insert(found->second).second;
}

void appendPublishedInheritanceEdges(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    const std::unordered_set<std::size_t>& rootProjectTypes,
    const std::unordered_map<std::string, std::size_t>& rootTypeByKey,
    std::unordered_set<std::size_t>& publishedRootTypes,
    std::size_t typeOrdinal,
    bool& addedPublishedType)
{
    if (const frontend::ClassInheritanceInfo* inheritance =
            compilation.inheritanceModel()->classInfo(typeOrdinal))
    {
        if (publishRootTypeKey(
                inheritance->baseTypeKey, rootTypeByKey, publishedRootTypes))
        {
            addedPublishedType = true;
        }
    }
    if (typeOrdinal < types.size() &&
        types[typeOrdinal].kind == frontend::NamedTypeDeclarationKind::Class)
    {
        for (const std::size_t interfaceOrdinal :
             compilation.interfaceModel()->interfacesOfClass(typeOrdinal))
        {
            if (rootProjectTypes.contains(interfaceOrdinal) &&
                publishedRootTypes.insert(interfaceOrdinal).second)
            {
                addedPublishedType = true;
            }
        }
    }
    else if (const frontend::InterfaceTypeInfo* interfaceInfo =
                 compilation.interfaceModel()->interfaceInfo(typeOrdinal))
    {
        for (const std::size_t interfaceOrdinal :
             interfaceInfo->directBaseInterfaceOrdinals)
        {
            if (rootProjectTypes.contains(interfaceOrdinal) &&
                publishedRootTypes.insert(interfaceOrdinal).second)
            {
                addedPublishedType = true;
            }
        }
    }
}

void appendPublishedAssociatedItemEdges(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    const std::unordered_map<std::string, std::size_t>& rootTypeByKey,
    std::unordered_set<std::size_t>& publishedRootTypes,
    std::size_t typeOrdinal,
    bool& addedPublishedType)
{
    if (typeOrdinal >= types.size()) return;
    for (const frontend::NamedTypeDeclarationPart& part : types[typeOrdinal].parts)
    {
        if (part.syntax == nullptr) continue;
        for (const ast::AstNode* item : part.syntax->children())
        {
            if (item == nullptr ||
                (item->kind() != ast::AstNodeKind::AssociatedTypeDeclaration &&
                 item->kind() != ast::AstNodeKind::AssociatedConstantDeclaration) ||
                item->typeSyntax() == nullptr)
            {
                continue;
            }
            if (publishRootTypeKey(
                    compilation.memberModel()->canonicalTypeKey(
                        item->typeSyntax(), *compilation.nameLookup()),
                    rootTypeByKey,
                    publishedRootTypes))
            {
                addedPublishedType = true;
            }
        }
    }
}

void appendPublishedDelegateEdges(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    const std::unordered_map<std::string, std::size_t>& rootTypeByKey,
    std::unordered_set<std::size_t>& publishedRootTypes,
    std::size_t typeOrdinal,
    bool& addedPublishedType)
{
    if (typeOrdinal >= types.size() ||
        types[typeOrdinal].kind != frontend::NamedTypeDeclarationKind::Delegate ||
        types[typeOrdinal].syntax == nullptr)
    {
        return;
    }
    // A delegate is a nominal type whose signature owns additional type edges.  Those
    // edges are not ClassMemberSymbols and do not appear in an object layout, so walking
    // only members and fields stops one node too early.  Preserve the complete transitive
    // contract even when a delegate parameter or return type is itself a non-root helper.
    const ast::AstNode* delegateSyntax = types[typeOrdinal].syntax;
    if (publishRootTypeKey(
            compilation.memberModel()->canonicalTypeKey(
                delegateSyntax->typeSyntax(), *compilation.nameLookup()),
            rootTypeByKey,
            publishedRootTypes))
    {
        addedPublishedType = true;
    }
    if (delegateSyntax->throwsTypeSyntax() != nullptr &&
        publishRootTypeKey(
            compilation.memberModel()->canonicalTypeKey(
                delegateSyntax->throwsTypeSyntax(), *compilation.nameLookup()),
            rootTypeByKey,
            publishedRootTypes))
    {
        addedPublishedType = true;
    }
    for (const ast::AstNode* parameter : delegateSyntax->children())
    {
        if (parameter != nullptr &&
            parameter->kind() == ast::AstNodeKind::ParameterDeclaration &&
            publishRootTypeKey(
                compilation.memberModel()->canonicalTypeKey(
                    parameter->typeSyntax(), *compilation.nameLookup()),
                rootTypeByKey,
                publishedRootTypes))
        {
            addedPublishedType = true;
        }
    }
}

void appendPublishedLayoutFields(
    std::span<const frontend::ObjectFieldLayout> fields,
    const std::unordered_map<std::string, std::size_t>& rootTypeByKey,
    std::unordered_set<std::size_t>& publishedRootTypes,
    bool& addedPublishedType)
{
    for (const frontend::ObjectFieldLayout& field : fields)
    {
        if (publishRootTypeKey(field.typeKey, rootTypeByKey, publishedRootTypes))
            addedPublishedType = true;
    }
}

void appendPublishedLayoutEdges(
    const frontend::ProjectCompilation& compilation,
    const std::unordered_map<std::string, std::size_t>& rootTypeByKey,
    std::unordered_set<std::size_t>& publishedRootTypes,
    std::size_t typeOrdinal,
    bool& addedPublishedType)
{
    const frontend::StructValueLayout* structLayout =
        compilation.objectLayoutModel()->structLayoutForType(typeOrdinal);
    const frontend::ClassObjectLayout* classLayout =
        compilation.objectLayoutModel()->layoutForType(typeOrdinal);
    if (structLayout != nullptr)
        appendPublishedLayoutFields(
            structLayout->fields,
            rootTypeByKey,
            publishedRootTypes,
            addedPublishedType);
    if (classLayout != nullptr)
        appendPublishedLayoutFields(
            classLayout->fields,
            rootTypeByKey,
            publishedRootTypes,
            addedPublishedType);
}

void appendPublishedMemberEdges(
    std::span<const frontend::ClassMemberSymbol> members,
    const std::unordered_map<std::string, std::size_t>& rootTypeByKey,
    std::unordered_set<std::size_t>& publishedRootTypes,
    std::size_t typeOrdinal,
    bool& addedPublishedType)
{
    for (const frontend::ClassMemberSymbol& member : members)
    {
        if (member.declaringTypeOrdinal != typeOrdinal ||
            (member.accessibility != frontend::ClassMemberAccessibility::Public &&
             member.accessibility != frontend::ClassMemberAccessibility::Protected))
        {
            continue;
        }
        if (publishRootTypeKey(
                member.declaredTypeKey, rootTypeByKey, publishedRootTypes))
        {
            addedPublishedType = true;
        }
        if (publishRootTypeKey(
                member.declaredErrorTypeKey, rootTypeByKey, publishedRootTypes))
        {
            addedPublishedType = true;
        }
        for (const frontend::MethodParameterSymbol& parameter : member.parameters)
        {
            if (publishRootTypeKey(
                    parameter.typeKey, rootTypeByKey, publishedRootTypes))
            {
                addedPublishedType = true;
            }
        }
    }
}

void expandPublishedRootType(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    std::span<const frontend::ClassMemberSymbol> members,
    const std::unordered_set<std::size_t>& rootProjectTypes,
    const std::unordered_map<std::string, std::size_t>& rootTypeByKey,
    std::unordered_set<std::size_t>& publishedRootTypes,
    std::size_t typeOrdinal,
    bool& addedPublishedType)
{
    appendPublishedInheritanceEdges(
        compilation,
        types,
        rootProjectTypes,
        rootTypeByKey,
        publishedRootTypes,
        typeOrdinal,
        addedPublishedType);
    appendPublishedAssociatedItemEdges(
        compilation,
        types,
        rootTypeByKey,
        publishedRootTypes,
        typeOrdinal,
        addedPublishedType);
    appendPublishedDelegateEdges(
        compilation,
        types,
        rootTypeByKey,
        publishedRootTypes,
        typeOrdinal,
        addedPublishedType);
    appendPublishedLayoutEdges(
        compilation,
        rootTypeByKey,
        publishedRootTypes,
        typeOrdinal,
        addedPublishedType);
    appendPublishedMemberEdges(
        members,
        rootTypeByKey,
        publishedRootTypes,
        typeOrdinal,
        addedPublishedType);
}

void collectPublishedRootTypeOrdinals(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    std::span<const frontend::ClassMemberSymbol> members,
    std::size_t rootProjectOrdinal,
    std::vector<std::size_t>& rootTypeOrdinals)
{
    std::unordered_set<std::size_t> rootProjectTypes;
    std::unordered_map<std::string, std::size_t> rootTypeByKey;
    for (std::size_t ordinal = 0; ordinal < types.size(); ++ordinal)
    {
        if (types[ordinal].projectOrdinal != rootProjectOrdinal) continue;
        rootProjectTypes.insert(ordinal);
        rootTypeByKey.emplace(typeKeyForOrdinal(types, ordinal), ordinal);
    }
    std::unordered_set<std::size_t> publishedRootTypes;
    for (const std::size_t ordinal : rootProjectTypes)
    {
        if (isPubliclyReachableRootType(types, rootProjectOrdinal, ordinal))
            publishedRootTypes.insert(ordinal);
    }

    // Public signatures can expose a root-module helper whose declaration has no source-level
    // public modifier (the canonical example is an `extern C delegate`). Such a type is part of
    // the contract because a public record already names it, while an unrelated internal type is
    // not. Compute that closure before serializing records instead of publishing every root type.
    bool addedPublishedType = true;
    while (addedPublishedType)
    {
        addedPublishedType = false;
        const std::vector<std::size_t> current(
            publishedRootTypes.begin(), publishedRootTypes.end());
        for (const std::size_t typeOrdinal : current)
            expandPublishedRootType(
                compilation,
                types,
                members,
                rootProjectTypes,
                rootTypeByKey,
                publishedRootTypes,
                typeOrdinal,
                addedPublishedType);
    }
    rootTypeOrdinals.assign(publishedRootTypes.begin(), publishedRootTypes.end());
    std::sort(
        rootTypeOrdinals.begin(),
        rootTypeOrdinals.end(),
        std::bind_front(metadataTypeOrdinalLess, types));
}

void initializeMetadataTypeRecord(
    std::span<const frontend::NamedTypeDeclaration> types,
    std::size_t typeOrdinal,
    LibraryMetadataType& record)
{
    const frontend::NamedTypeDeclaration& type = types[typeOrdinal];
    record.kind = metadataKind(type.kind);
    record.accessibility = metadataAccessibility(type.accessibility);
    record.flags = typeFlags(type);
    record.identityKey = type.identity.key();
    record.fullyQualifiedName = type.identity.fullyQualifiedName();
    record.stableTypeId = stableTypeId(typeKeyForOrdinal(types, typeOrdinal));
}

void appendTypeLifetimeMetadata(
    const frontend::NamedTypeDeclaration& type,
    bool publishLifetimeContracts,
    LibraryMetadataType& record)
{
    if (!publishLifetimeContracts || type.syntax == nullptr) return;
    for (const ast::LifetimeParameterSyntax& lifetime : type.syntax->lifetimeParameters())
        record.lifetimeParameters.push_back(lifetime.name);
    for (const ast::LifetimeOutlivesConstraint& constraint :
         type.syntax->lifetimeOutlivesConstraints())
    {
        record.lifetimeOutlivesConstraints.emplace_back(
            constraint.longer, constraint.shorter);
    }
}

void appendTraitObjectMetadata(
    const frontend::ProjectCompilation& compilation,
    std::size_t typeOrdinal,
    LibraryMetadataType& record)
{
    if ((record.flags & kMetadataTypeTrait) == 0U) return;
    const frontend::TraitObjectContract* object =
        compilation.interfaceModel()->traitObjectContract(typeOrdinal);
    if (object == nullptr) return;
    if (object->objectSafe) record.traitObjectFlags |= kMetadataTraitObjectObjectSafe;
    if (object->descriptorInterfaceMap)
        record.traitObjectFlags |= kMetadataTraitObjectDescriptorMap;
    if (object->nullIsZero) record.traitObjectFlags |= kMetadataTraitObjectNullIsZero;
    if (!object->conversionAllocates)
        record.traitObjectFlags |= kMetadataTraitObjectNoConversionAllocation;
    record.traitObjectWordCount = static_cast<std::uint8_t>(object->size / 8U);
}

void appendDeclaredTypeInterfaces(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    std::span<const frontend::ClassMemberSymbol> members,
    const frontend::NamedTypeDeclaration& type,
    std::size_t typeOrdinal,
    LibraryMetadataType& record)
{
    if (type.kind == frontend::NamedTypeDeclarationKind::Class ||
        type.kind == frontend::NamedTypeDeclarationKind::Struct)
    {
        for (const std::size_t interfaceOrdinal :
             compilation.interfaceModel()->interfacesOfClass(typeOrdinal))
        {
            record.interfaceTypeKeys.push_back(typeKeyForOrdinal(types, interfaceOrdinal));
        }
        return;
    }
    if (type.kind != frontend::NamedTypeDeclarationKind::Interface) return;
    const frontend::InterfaceTypeInfo* interfaceInfo =
        compilation.interfaceModel()->interfaceInfo(typeOrdinal);
    if (interfaceInfo == nullptr) return;
    for (const std::size_t interfaceOrdinal : interfaceInfo->directBaseInterfaceOrdinals)
        record.interfaceTypeKeys.push_back(typeKeyForOrdinal(types, interfaceOrdinal));
    if (!interfaceInfo->isTrait) return;
    for (const std::size_t memberOrdinal : interfaceInfo->effectiveMethodMemberOrdinals)
        record.traitMethodSignatures.push_back(signatureForMember(members, memberOrdinal));
}

void appendOpaqueResultInterfaces(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    const frontend::NamedTypeDeclaration& type,
    LibraryMetadataType& record)
{
    // Opaque results are compiler-owned value structs. Keep a direct-syntax fallback for
    // metadata-hydrated or partial compilations whose value-trait relation did not produce
    // a class-style interface set; silently dropping the trait would erase the abstraction
    // at the module boundary. Inherited bases are rebuilt from the trait metadata.
    if ((record.flags & kMetadataTypeOpaqueResult) == 0U ||
        !record.interfaceTypeKeys.empty())
    {
        return;
    }
    for (const frontend::NamedTypeDeclarationPart& part : type.parts)
    {
        if (part.syntax == nullptr) continue;
        for (const ast::TypeSyntax* specification : part.syntax->typeSyntaxes())
        {
            const std::optional<std::size_t> interfaceOrdinal =
                compilation.nameLookup()->resolvedTypeOrdinal(specification);
            if (!interfaceOrdinal.has_value() || *interfaceOrdinal >= types.size() ||
                types[*interfaceOrdinal].kind != frontend::NamedTypeDeclarationKind::Interface)
            {
                continue;
            }
            record.interfaceTypeKeys.push_back(typeKeyForOrdinal(types, *interfaceOrdinal));
        }
    }
}

void appendTypeInheritanceMetadata(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    std::span<const frontend::ClassMemberSymbol> members,
    const frontend::NamedTypeDeclaration& type,
    std::size_t typeOrdinal,
    LibraryMetadataType& record)
{
    if (type.isStatic) return;
    if (const frontend::ClassInheritanceInfo* inheritance =
            compilation.inheritanceModel()->classInfo(typeOrdinal))
    {
        record.baseTypeKey = inheritance->baseTypeKey;
    }
    appendDeclaredTypeInterfaces(
        compilation, types, members, type, typeOrdinal, record);
    appendOpaqueResultInterfaces(compilation, types, type, record);
}

bool buildAssociatedItemMetadata(
    const frontend::ProjectCompilation& compilation,
    const frontend::NamedTypeDeclaration& type,
    const ast::AstNode& item,
    LibraryMetadataAssociatedItem& associated,
    std::string& error)
{
    associated.name = item.text();
    if (item.kind() == ast::AstNodeKind::AssociatedTypeDeclaration)
    {
        associated.kind = LibraryMetadataAssociatedItemKind::Type;
        associated.hasDefault = item.typeSyntax() != nullptr;
        if (associated.hasDefault)
            associated.typeKey = compilation.memberModel()->canonicalTypeKey(
                item.typeSyntax(), *compilation.nameLookup());
        return true;
    }
    associated.kind = LibraryMetadataAssociatedItemKind::Constant;
    associated.typeKey = compilation.memberModel()->canonicalTypeKey(
        item.typeSyntax(), *compilation.nameLookup());
    associated.hasDefault = item.first() != nullptr;
    if (!associated.hasDefault) return true;
    const std::optional<frontend::ProjectIntegerValue> value =
        frontend::ProjectIntegerEvaluator(nullptr).evaluate(item.first());
    if (!value.has_value())
    {
        error = "associated constant '" + type.identity.key() + "." +
            item.text() + "' is not an integral compile-time value";
        return false;
    }
    associated.valueBits = value->bits;
    return true;
}

bool appendAssociatedItemMetadata(
    const frontend::ProjectCompilation& compilation,
    const frontend::NamedTypeDeclaration& type,
    LibraryMetadataType& record,
    std::string& error)
{
    for (const frontend::NamedTypeDeclarationPart& part : type.parts)
    {
        if (part.syntax == nullptr) continue;
        for (const ast::AstNode* item : part.syntax->children())
        {
            if (item == nullptr ||
                (item->kind() != ast::AstNodeKind::AssociatedTypeDeclaration &&
                 item->kind() != ast::AstNodeKind::AssociatedConstantDeclaration))
            {
                continue;
            }
            LibraryMetadataAssociatedItem associated;
            if (!buildAssociatedItemMetadata(
                    compilation, type, *item, associated, error))
            {
                return false;
            }
            record.associatedItems.push_back(std::move(associated));
        }
    }
    return true;
}

LibraryMetadataField buildFieldMetadata(
    std::span<const frontend::ClassMemberSymbol> members,
    const frontend::ObjectFieldLayout& fieldLayout,
    bool publishLifetimeContracts)
{
    LibraryMetadataField field;
    if (fieldLayout.memberOrdinal < members.size())
    {
        const frontend::ClassMemberSymbol& member = members[fieldLayout.memberOrdinal];
        field.name = member.name;
        field.flags = memberFlags(member);
        if (publishLifetimeContracts && member.declaredTypeSyntax != nullptr)
            field.lifetimeName = member.declaredTypeSyntax->lifetimeName();
        field.ownership = metadataOwnership(member.declaredOwnership);
    }
    field.typeKey = fieldLayout.typeKey;
    field.offset = fieldLayout.offset;
    field.size = fieldLayout.size;
    field.alignment = fieldLayout.alignment;
    field.bitOffset = fieldLayout.bitOffset.value_or(0);
    field.bitWidth = fieldLayout.bitWidth.value_or(0);
    return field;
}

void appendStructLayoutMetadata(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::ClassMemberSymbol> members,
    const frontend::NamedTypeDeclaration& type,
    std::size_t typeOrdinal,
    const frontend::StructValueLayout& structLayout,
    bool publishLifetimeContracts,
    LibraryMetadataType& record)
{
    record.size = structLayout.size;
    record.alignment = structLayout.alignment;
    for (const frontend::ObjectFieldLayout& fieldLayout : structLayout.fields)
        record.fields.push_back(
            buildFieldMetadata(members, fieldLayout, publishLifetimeContracts));
    if (!compilation.memberModel()->typeRequiresValueDrop("type:" + type.identity.key()))
        return;
    for (const frontend::ProjectOopMirStructLayout& mirLayout :
         compilation.oopMirModel()->structLayouts())
    {
        if (mirLayout.typeOrdinal != typeOrdinal) continue;
        record.dropGlueEntryName = mirLayout.dropGlueEntryName;
        break;
    }
}

void appendClassLayoutMetadata(
    std::span<const frontend::ClassMemberSymbol> members,
    const frontend::ClassObjectLayout& classLayout,
    bool publishLifetimeContracts,
    LibraryMetadataType& record)
{
    record.size = classLayout.size;
    record.alignment = classLayout.alignment;
    record.headerSize = classLayout.headerSize;
    record.referenceSize = classLayout.referenceSize;
    record.referenceAlignment = classLayout.referenceAlignment;
    for (const frontend::ObjectFieldLayout& fieldLayout : classLayout.fields)
        record.fields.push_back(
            buildFieldMetadata(members, fieldLayout, publishLifetimeContracts));
}

void appendEnumLayoutMetadata(
    const frontend::ProjectCompilation& compilation,
    const frontend::NamedTypeDeclaration& type,
    LibraryMetadataType& record)
{
    // EL TAMANO DEL ENUM SALE DEL MODELO DE LAYOUT, no de un 4 escrito aqui.
    //
    // Este era el SEPTIMO sitio con "un enum mide cuatro bytes" escrito a
    // mano, y el mas caro de los que quedaban: los metadatos alimentan a los
    // consumidores del ABI C publico, asi que un 4/4 fijo se propaga a codigo
    // externo que ya nadie relaciona con este fichero. Un `enum : byte`
    // exportado hacia que el host reservara cuatro bytes donde el modulo pone uno.
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

void appendDelegateParameterMetadata(
    const frontend::ProjectCompilation& compilation,
    const ast::AstNode& parameter,
    bool publishLifetimeContracts,
    LibraryMetadataType& record)
{
    record.delegateParameterTypeKeys.push_back(
        compilation.memberModel()->canonicalTypeKey(
            parameter.typeSyntax(), *compilation.nameLookup()));
    record.delegateParameterDynamicTraits.push_back(
        parameter.typeSyntax() != nullptr && parameter.typeSyntax()->isDynamicTrait()
            ? 1U
            : 0U);
    if (!publishLifetimeContracts) return;
    LibraryMetadataParameterPassKind passKind = LibraryMetadataParameterPassKind::Value;
    if (parameter.auxiliaryText() == "pass:ref")
        passKind = LibraryMetadataParameterPassKind::Ref;
    else if (parameter.auxiliaryText() == "pass:out")
        passKind = LibraryMetadataParameterPassKind::Out;
    else if (parameter.auxiliaryText() == "pass:in")
        passKind = LibraryMetadataParameterPassKind::In;
    record.delegateParameterPassKinds.push_back(passKind);
    record.delegateParameterLifetimeNames.push_back(parameter.lifetimeName());
}

void appendDelegateMetadata(
    const frontend::ProjectCompilation& compilation,
    const frontend::NamedTypeDeclaration& type,
    bool publishLifetimeContracts,
    LibraryMetadataType& record)
{
    if (type.kind != frontend::NamedTypeDeclarationKind::Delegate || type.syntax == nullptr)
        return;
    if (type.syntax->hasModifier(lexer::TokenKind::KwExtern)) record.delegateFlags |= 1U;
    if (type.syntax->hasModifier(lexer::TokenKind::KwVectorCall)) record.delegateFlags |= 2U;
    // Los dos delegates viajan en un registro LLVM `ptr`. El extern C apunta
    // al codigo; el ordinario apunta a la celda ABI versionada. La diferencia
    // queda en flags y en el contrato raiz, pero tamaño/alineacion de la
    // REFERENCIA son iguales.
    record.referenceSize = kPointerSize;
    record.referenceAlignment = kPointerAlignment;
    record.delegateReturnTypeKey = compilation.memberModel()->canonicalTypeKey(
        type.syntax->typeSyntax(), *compilation.nameLookup());
    record.delegateErrorTypeKey = type.syntax->throwsTypeSyntax() == nullptr
        ? std::string{}
        : compilation.memberModel()->canonicalTypeKey(
            type.syntax->throwsTypeSyntax(), *compilation.nameLookup());
    record.delegateReturnDynamicTrait =
        type.syntax->typeSyntax() != nullptr && type.syntax->typeSyntax()->isDynamicTrait();
    if (publishLifetimeContracts)
    {
        record.delegateReferenceReturn = type.syntax->isReference();
        record.delegateReturnLifetimeName = type.syntax->lifetimeName();
    }
    for (const ast::AstNode* parameter : type.syntax->children())
    {
        if (parameter != nullptr &&
            parameter->kind() == ast::AstNodeKind::ParameterDeclaration)
        {
            appendDelegateParameterMetadata(
                compilation, *parameter, publishLifetimeContracts, record);
        }
    }
}

void appendTypeLayoutMetadata(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::ClassMemberSymbol> members,
    const frontend::NamedTypeDeclaration& type,
    std::size_t typeOrdinal,
    bool publishLifetimeContracts,
    LibraryMetadataType& record)
{
    const frontend::StructValueLayout* structLayout =
        compilation.objectLayoutModel()->structLayoutForType(typeOrdinal);
    const frontend::ClassObjectLayout* classLayout =
        compilation.objectLayoutModel()->layoutForType(typeOrdinal);
    if (structLayout != nullptr)
    {
        appendStructLayoutMetadata(
            compilation,
            members,
            type,
            typeOrdinal,
            *structLayout,
            publishLifetimeContracts,
            record);
    }
    else if (classLayout != nullptr)
    {
        appendClassLayoutMetadata(
            members, *classLayout, publishLifetimeContracts, record);
    }
    else if (type.kind == frontend::NamedTypeDeclarationKind::Enum)
    {
        appendEnumLayoutMetadata(compilation, type, record);
    }
    else if (type.kind == frontend::NamedTypeDeclarationKind::Interface)
    {
        const target::TypeLayout referenceLayout =
            compilation.objectLayoutModel()->classReferenceLayout();
        record.referenceSize = referenceLayout.size;
        record.referenceAlignment = referenceLayout.alignment;
    }
    else
    {
        appendDelegateMetadata(compilation, type, publishLifetimeContracts, record);
    }
}

void appendVariantCaseMetadata(
    const frontend::ProjectCompilation& compilation,
    const ast::AstNode& child,
    std::vector<std::pair<std::size_t, LibraryMetadataVariantCase>>& orderedCases)
{
    const std::string_view marker = child.auxiliaryText();
    constexpr std::string_view prefix = "variant-case-factory:";
    if (!marker.starts_with(prefix)) return;
    const std::size_t separator = marker.find(':', prefix.size());
    if (separator == std::string_view::npos) return;
    std::size_t caseIndex = 0;
    const std::string_view indexText = marker.substr(separator + 1U);
    const auto parsed = std::from_chars(
        indexText.data(), indexText.data() + indexText.size(), caseIndex);
    if (parsed.ec != std::errc{} || parsed.ptr != indexText.data() + indexText.size())
        return;
    LibraryMetadataVariantCase variantCase;
    variantCase.name = std::string(marker.substr(prefix.size(), separator - prefix.size()));
    const auto parameter = std::ranges::find_if(
        child.children(), isParameterDeclaration);
    if (parameter != child.children().end())
        variantCase.payloadTypeKey = compilation.memberModel()->canonicalTypeKey(
            (*parameter)->typeSyntax(), *compilation.nameLookup());
    orderedCases.emplace_back(caseIndex, std::move(variantCase));
}

void appendVariantMetadata(
    const frontend::ProjectCompilation& compilation,
    const frontend::NamedTypeDeclaration& type,
    LibraryMetadataType& record)
{
    if ((record.flags & kMetadataTypeVariant) == 0U || type.syntax == nullptr) return;
    using IndexedVariantCase = std::pair<std::size_t, LibraryMetadataVariantCase>;
    std::vector<IndexedVariantCase> orderedCases;
    for (const ast::AstNode* child : type.syntax->children())
    {
        if (child == nullptr || child->kind() != ast::AstNodeKind::MethodDeclaration)
            continue;
        appendVariantCaseMetadata(compilation, *child, orderedCases);
    }
    std::ranges::sort(orderedCases, {}, &IndexedVariantCase::first);
    for (IndexedVariantCase& item : orderedCases)
        record.variantCases.push_back(std::move(item.second));
}

bool buildMetadataType(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    std::span<const frontend::ClassMemberSymbol> members,
    std::size_t typeOrdinal,
    bool publishLifetimeContracts,
    LibraryMetadataType& record,
    std::string& error)
{
    const frontend::NamedTypeDeclaration& type = types[typeOrdinal];
    initializeMetadataTypeRecord(types, typeOrdinal, record);
    appendTypeLifetimeMetadata(type, publishLifetimeContracts, record);
    appendTraitObjectMetadata(compilation, typeOrdinal, record);
    appendTypeInheritanceMetadata(
        compilation, types, members, type, typeOrdinal, record);
    if (!appendAssociatedItemMetadata(compilation, type, record, error)) return false;
    std::sort(record.interfaceTypeKeys.begin(), record.interfaceTypeKeys.end());
    appendTypeLayoutMetadata(
        compilation,
        members,
        type,
        typeOrdinal,
        publishLifetimeContracts,
        record);
    appendVariantMetadata(compilation, type, record);
    return true;
}

bool appendMetadataTypes(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    std::span<const frontend::ClassMemberSymbol> members,
    const std::vector<std::size_t>& rootTypeOrdinals,
    bool publishLifetimeContracts,
    LibraryMetadata& metadata,
    std::string& error)
{
    for (const std::size_t typeOrdinal : rootTypeOrdinals)
    {
        LibraryMetadataType record;
        if (!buildMetadataType(
                compilation,
                types,
                members,
                typeOrdinal,
                publishLifetimeContracts,
                record,
                error))
        {
            return false;
        }
        metadata.types.push_back(std::move(record));
    }
    return true;
}

bool metadataMemberOrdinalLess(
    std::span<const frontend::NamedTypeDeclaration> types,
    std::span<const frontend::ClassMemberSymbol> members,
    std::size_t left,
    std::size_t right)
{
    const frontend::ClassMemberSymbol& a = members[left];
    const frontend::ClassMemberSymbol& b = members[right];
    const std::string aType = typeKeyForOrdinal(types, a.declaringTypeOrdinal);
    const std::string bType = typeKeyForOrdinal(types, b.declaringTypeOrdinal);
    if (aType != bType) return aType < bType;
    if (a.signatureKey != b.signatureKey) return a.signatureKey < b.signatureKey;
    return left < right;
}

void collectMetadataMemberOrdinals(
    std::span<const frontend::NamedTypeDeclaration> types,
    std::span<const frontend::ClassMemberSymbol> members,
    const std::unordered_set<std::size_t>& rootTypes,
    std::vector<std::size_t>& memberOrdinals)
{
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
    {
        const frontend::ClassMemberSymbol& member = members[ordinal];
        const bool contractAccessibility =
            member.accessibility == frontend::ClassMemberAccessibility::Public ||
            member.accessibility == frontend::ClassMemberAccessibility::Protected;
        if (contractAccessibility &&
            !member.isControlledSpecializationFamily &&
            rootTypes.contains(member.declaringTypeOrdinal))
        {
            memberOrdinals.push_back(ordinal);
        }
    }
    std::sort(
        memberOrdinals.begin(),
        memberOrdinals.end(),
        std::bind_front(metadataMemberOrdinalLess, types, members));
}

void initializeMetadataMemberRecord(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    const frontend::ClassMemberSymbol& member,
    std::size_t ordinal,
    bool publishLifetimeContracts,
    LibraryMetadataMember& record)
{
    record = LibraryMetadataMember{};
    record.kind = static_cast<std::uint8_t>(member.kind);
    record.accessibility = static_cast<std::uint8_t>(member.accessibility);
    record.flags = memberFlags(member);
    record.declaringTypeKey = typeKeyForOrdinal(types, member.declaringTypeOrdinal);
    record.name = member.name;
    record.signatureKey = member.signatureKey;
    record.declaredTypeKey = member.declaredTypeKey;
    record.declaredErrorTypeKey = member.declaredErrorTypeKey;
    record.declaredOwnership = metadataOwnership(member.declaredOwnership);
    if (publishLifetimeContracts)
    {
        record.returnLifetimeName = member.returnLifetimeName;
        record.lifetimeParameters = member.lifetimeParameters;
        record.lifetimeOutlivesConstraints = member.lifetimeOutlivesConstraints;
    }
    for (const language::EscapeClass escapeClass :
         compilation.methodBodyModel()->parameterEscapeSummary(ordinal))
    {
        record.parameterEscapeSummary.push_back(metadataEscapeClass(escapeClass));
    }
    record.cAbiName = member.cAbiName;
    record.importDllName = member.importDllName;
}

const frontend::ProjectOopMirFunction* resolveMemberPhysicalIdentity(
    const frontend::ProjectCompilation& compilation,
    std::size_t ordinal,
    LibraryMetadataMember& record)
{
    const frontend::ProjectOopMirFunction* function =
        compilation.oopMirModel()->functionForMember(ordinal);
    if (function != nullptr) record.mangledName = function->mangledName;
    // NS304-T07-F08: an async/generator callable's physical entry is its CONSTRUCTOR (the
    // original symbol). The $moveNext and $drop functions share the memberOrdinal and must
    // never become the published callable identity — a consumer that imports this member must
    // call the constructor, not the drop glue.
    if (function == nullptr || !function->isGeneratorConstructor)
    {
        for (const frontend::ProjectOopMirFunction& candidate :
             compilation.oopMirModel()->functions())
        {
            if (candidate.memberOrdinal == ordinal && candidate.isGeneratorConstructor)
            {
                record.mangledName = candidate.mangledName;
                break;
            }
        }
    }
    return function;
}

void appendInlineMemberSource(
    const frontend::ProjectCompilation& compilation,
    const frontend::ClassMemberSymbol& member,
    const frontend::ProjectOopMirFunction* function,
    const LibraryMetadata& metadata,
    LibraryMetadataMember& record)
{
    if (metadata.formatMinor >= 13 &&
        member.kind == frontend::ClassMemberKind::Method &&
        member.syntax != nullptr && function != nullptr &&
        function->genericSpecializationKey.empty() &&
        (member.inliningPolicy == frontend::InliningPolicy::Preferred ||
         member.inliningPolicy == frontend::InliningPolicy::Required))
    {
        record.inlineSourceText =
            std::string(compilation.sourceManager().getText(member.syntax->range()));
    }
}

void validateMemberPhysicalIdentity(
    std::span<const frontend::NamedTypeDeclaration> types,
    const frontend::ClassMemberSymbol& member,
    std::size_t rootProjectOrdinal,
    const LibraryMetadataMember& record,
    std::vector<std::string>& missingPhysicalCallables)
{
    const bool callableAccessibility =
        member.accessibility == frontend::ClassMemberAccessibility::Public ||
        member.accessibility == frontend::ClassMemberAccessibility::Protected;
    const bool nativeCallable =
        (member.kind == frontend::ClassMemberKind::Method ||
         member.kind == frontend::ClassMemberKind::Constructor) &&
        callableAccessibility &&
        isPubliclyReachableRootType(types, rootProjectOrdinal, member.declaringTypeOrdinal) &&
        !member.isAbstract && !member.isCompilerGenerated && !member.isImportedC &&
        (!member.isExternC || member.isExportedC);
    if (nativeCallable && record.mangledName.empty())
    {
        missingPhysicalCallables.push_back(
            record.declaringTypeKey + "." + record.signatureKey);
    }
}

bool buildMemberParameterMetadata(
    const frontend::ClassMemberSymbol& member,
    const frontend::MethodParameterSymbol& parameter,
    bool publishLifetimeContracts,
    LibraryMetadataParameter& parameterRecord,
    std::string& error)
{
    parameterRecord.name = parameter.name;
    parameterRecord.typeKey = parameter.typeKey;
    parameterRecord.ownership = metadataOwnership(parameter.ownership);
    parameterRecord.passKind = metadataPassKind(parameter.passKind);
    parameterRecord.isParameterPack = parameter.isParameterPack;
    parameterRecord.isDynamicTrait =
        parameter.typeSyntax != nullptr && parameter.typeSyntax->isDynamicTrait();
    if (parameter.ffiLengthParameterOrdinal.has_value())
        parameterRecord.ffiLengthParameter =
            static_cast<std::uint32_t>(*parameter.ffiLengthParameterOrdinal);
    parameterRecord.ffiLengthParameterName = parameter.ffiLengthParameterName;
    parameterRecord.ffiCallbackLifetime = parameter.ffiCallback
        ? static_cast<std::uint8_t>(parameter.ffiCallbackMayEscape ? 2U : 1U)
        : 0U;
    if (publishLifetimeContracts)
        parameterRecord.lifetimeName = parameter.lifetimeName;
    if (metadataDefaultValue(parameter.defaultValueSyntax, parameterRecord, error))
        return true;
    error = "could not encode default value for parameter '" + parameter.name +
        "' of member '" + member.name + "': " + error;
    return false;
}

bool appendMemberParameterMetadata(
    const frontend::ClassMemberSymbol& member,
    bool publishLifetimeContracts,
    LibraryMetadataMember& record,
    std::string& error)
{
    for (const frontend::MethodParameterSymbol& parameter : member.parameters)
    {
        LibraryMetadataParameter parameterRecord;
        if (!buildMemberParameterMetadata(
                member,
                parameter,
                publishLifetimeContracts,
                parameterRecord,
                error))
        {
            return false;
        }
        record.parameters.push_back(std::move(parameterRecord));
    }
    return true;
}

void appendMemberConstantMetadata(
    const frontend::ProjectOopMirStaticField* field,
    LibraryMetadataMember& record)
{
    if (field == nullptr || !field->isConst || !field->hasConstantInitializer) return;
    if (field->typeKey == "builtin:bool")
    {
        record.constantKind = 3;
        record.constantBits = field->constantBooleanValue ? 1U : 0U;
    }
    else if (field->typeKey == "builtin:float")
    {
        record.constantKind = 4;
        record.constantBits =
            std::bit_cast<std::uint32_t>(static_cast<float>(field->constantFloatingValue));
    }
    else if (field->typeKey == "builtin:double")
    {
        record.constantKind = 5;
        record.constantBits = std::bit_cast<std::uint64_t>(field->constantFloatingValue);
    }
    else if (field->typeKey == "builtin:string")
    {
        record.constantKind = 6;
        record.constantText = field->constantStringValue;
    }
    else
    {
        record.constantKind = 1;
        record.constantBits = field->constantIntegerValue;
    }
}

bool buildMetadataMember(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    std::span<const frontend::ClassMemberSymbol> members,
    const std::unordered_map<std::size_t, const frontend::ProjectOopMirStaticField*>&
        staticFields,
    std::size_t rootProjectOrdinal,
    std::size_t ordinal,
    bool publishLifetimeContracts,
    LibraryMetadata& metadata,
    LibraryMetadataMember& record,
    std::vector<std::string>& missingPhysicalCallables,
    std::string& error)
{
    const frontend::ClassMemberSymbol& member = members[ordinal];
    initializeMetadataMemberRecord(
        compilation, types, member, ordinal, publishLifetimeContracts, record);
    const frontend::ProjectOopMirFunction* function =
        resolveMemberPhysicalIdentity(compilation, ordinal, record);
    appendInlineMemberSource(compilation, member, function, metadata, record);
    const auto staticField = staticFields.find(ordinal);
    const frontend::ProjectOopMirStaticField* field =
        staticField == staticFields.end() ? nullptr : staticField->second;
    if (field != nullptr && !field->isConst) record.mangledName = field->accessorName;
    validateMemberPhysicalIdentity(
        types, member, rootProjectOrdinal, record, missingPhysicalCallables);
    if (!appendMemberParameterMetadata(
            member, publishLifetimeContracts, record, error))
    {
        return false;
    }
    record.ffiUnwindPolicy = static_cast<std::uint8_t>(member.ffiUnwindPolicy);
    appendMemberConstantMetadata(field, record);
    return true;
}

bool appendMetadataMembers(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    std::span<const frontend::ClassMemberSymbol> members,
    const std::vector<std::size_t>& memberOrdinals,
    const std::unordered_map<std::size_t, const frontend::ProjectOopMirStaticField*>&
        staticFields,
    std::size_t rootProjectOrdinal,
    bool publishLifetimeContracts,
    LibraryMetadata& metadata,
    std::vector<std::string>& missingPhysicalCallables,
    std::string& error)
{
    for (const std::size_t ordinal : memberOrdinals)
    {
        LibraryMetadataMember record;
        if (!buildMetadataMember(
                compilation,
                types,
                members,
                staticFields,
                rootProjectOrdinal,
                ordinal,
                publishLifetimeContracts,
                metadata,
                record,
                missingPhysicalCallables,
                error))
        {
            return false;
        }
        metadata.members.push_back(std::move(record));
    }
    return true;
}

bool appendExternalTraitImplMetadata(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    std::span<const frontend::ClassMemberSymbol> members,
    const std::unordered_map<std::size_t, const frontend::ProjectOopMirStaticField*>&
        staticFields,
    std::size_t rootProjectOrdinal,
    bool publishLifetimeContracts,
    LibraryMetadata& metadata,
    std::vector<std::string>& missingPhysicalCallables,
    std::string& error)
{
    // NS304-T08-F05: publish external trait conformances (the module owns the trait, not the type)
    // so a consumer rebuilds the same impl without the producer's sources.
    for (const frontend::NamedTypeDeclarationPart& part :
         compilation.nameLookup()->externalTraitImplParts())
    {
        if (part.syntax == nullptr || part.syntax->typeSyntax() == nullptr) continue;
        const std::optional<std::size_t> traitOrdinal =
            compilation.nameLookup()->resolvedTypeOrdinal(part.syntax->typeSyntax());
        if (!traitOrdinal.has_value()) continue;
        std::optional<std::size_t> targetOrdinal;
        for (std::size_t ordinal = 0; ordinal < types.size(); ++ordinal)
        {
            if (types[ordinal].identity.name() == part.syntax->text())
            {
                targetOrdinal = ordinal;
                break;
            }
        }
        if (!targetOrdinal.has_value()) continue;
        LibraryMetadataTraitImpl traitImpl;
        traitImpl.targetTypeKey = typeKeyForOrdinal(types, *targetOrdinal);
        traitImpl.traitTypeKey = typeKeyForOrdinal(types, *traitOrdinal);
        for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
        {
            if (members[ordinal].declaringTypeOrdinal != *targetOrdinal) continue;
            LibraryMetadataMember record;
            if (!buildMetadataMember(
                    compilation,
                    types,
                    members,
                    staticFields,
                    rootProjectOrdinal,
                    ordinal,
                    publishLifetimeContracts,
                    metadata,
                    record,
                    missingPhysicalCallables,
                    error))
            {
                return false;
            }
            traitImpl.methods.push_back(std::move(record));
        }
        if (!traitImpl.methods.empty()) metadata.traitImpls.push_back(std::move(traitImpl));
    }
    return true;
}

bool validatePhysicalMetadataCallables(
    const std::vector<std::string>& missingPhysicalCallables,
    std::string& error)
{
    if (missingPhysicalCallables.empty()) return true;
    error = "public callables have no physical identity; refusing to emit unusable NSMETA";
    for (const std::string& callable : missingPhysicalCallables)
        error += "\n  " + callable;
    return false;
}

bool buildMemberMetadata(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::NamedTypeDeclaration> types,
    std::span<const frontend::ClassMemberSymbol> members,
    const std::vector<std::size_t>& rootTypeOrdinals,
    std::size_t rootProjectOrdinal,
    bool publishLifetimeContracts,
    LibraryMetadata& metadata,
    std::unordered_set<std::size_t>& rootTypes,
    std::string& error)
{
    rootTypes.insert(rootTypeOrdinals.begin(), rootTypeOrdinals.end());
    std::unordered_map<std::size_t, const frontend::ProjectOopMirStaticField*> staticFields;
    for (const frontend::ProjectOopMirStaticField& field :
         compilation.oopMirModel()->staticFields())
    {
        staticFields.emplace(field.memberOrdinal, &field);
    }
    std::vector<std::size_t> memberOrdinals;
    collectMetadataMemberOrdinals(types, members, rootTypes, memberOrdinals);
    std::vector<std::string> missingPhysicalCallables;
    if (!appendMetadataMembers(
            compilation,
            types,
            members,
            memberOrdinals,
            staticFields,
            rootProjectOrdinal,
            publishLifetimeContracts,
            metadata,
            missingPhysicalCallables,
            error))
    {
        return false;
    }
    if (!appendExternalTraitImplMetadata(
            compilation,
            types,
            members,
            staticFields,
            rootProjectOrdinal,
            publishLifetimeContracts,
            metadata,
            missingPhysicalCallables,
            error))
    {
        return false;
    }
    return validatePhysicalMetadataCallables(missingPhysicalCallables, error);
}

bool initializeMetadataModel(
    const frontend::ProjectCompilation& compilation,
    LibraryMetadata& metadata,
    std::size_t& rootProjectOrdinalValue,
    std::string& error)
{
    if (compilation.projectGraph() == nullptr ||
        compilation.projectGraph()->rootProject() == nullptr ||
        compilation.nameLookup() == nullptr || compilation.genericModel() == nullptr ||
        compilation.memberModel() == nullptr || compilation.inheritanceModel() == nullptr ||
        compilation.interfaceModel() == nullptr ||
        compilation.objectLayoutModel() == nullptr ||
        compilation.methodBodyModel() == nullptr || compilation.oopMirModel() == nullptr)
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
    rootProjectOrdinalValue = *rootOrdinal;
    metadata = {};
    metadata.formatMajor = kFormatMajor;
    metadata.formatMinor = kFormatMinor;
    metadata.targetId = kWindowsX64Target;
    metadata.pointerSize = kPointerSize;
    metadata.pointerAlignment = kPointerAlignment;
    metadata.languageFeatureFlags = kMetadataLanguageFeatureMask;
    metadata.producerVersion = std::string(kCompilerVersion);
    metadata.moduleName = compilation.projectGraph()->rootProject()->moduleName();
    metadata.targetTriple = std::string(kTargetTriple);
    return true;
}

bool buildNamespaceFunctionParameterMetadata(
    const frontend::NamespaceFunctionSymbol& function,
    const frontend::MethodParameterSymbol& parameter,
    bool publishLifetimeContracts,
    LibraryMetadataParameter& parameterRecord,
    std::string& error)
{
    parameterRecord.name = parameter.name;
    parameterRecord.typeKey = parameter.typeKey;
    parameterRecord.ownership = metadataOwnership(parameter.ownership);
    parameterRecord.passKind = metadataPassKind(parameter.passKind);
    parameterRecord.isParameterPack = parameter.isParameterPack;
    parameterRecord.isDynamicTrait =
        parameter.typeSyntax != nullptr && parameter.typeSyntax->isDynamicTrait();
    if (publishLifetimeContracts)
        parameterRecord.lifetimeName = parameter.lifetimeName;
    if (metadataDefaultValue(parameter.defaultValueSyntax, parameterRecord, error))
        return true;
    error = "could not encode default value for parameter '" + parameter.name +
        "' of namespace function '" + function.name + "': " + error;
    return false;
}

void initializeNamespaceFunctionMetadata(
    const frontend::NamespaceFunctionSymbol& function,
    bool publishLifetimeContracts,
    LibraryMetadataNamespaceFunction& record)
{
    record.accessibility = 1U;
    if (function.isReferenceReturn) record.flags |= 1U;
    if (function.isVariadic) record.flags |= 2U;
    if (function.isExternC) record.flags |= 4U;
    if (function.isExportedC) record.flags |= 8U;
    if (function.declaredTypeSyntax != nullptr &&
        function.declaredTypeSyntax->isDynamicTrait())
    {
        record.flags |= 16U;
    }
    if (function.isExtension) record.flags |= kMetadataNamespaceFunctionExtension;
    if (function.isStaticExtension)
        record.flags |= kMetadataNamespaceFunctionStaticExtension;
    record.namespaceName = function.namespaceName;
    record.name = function.name;
    record.signatureKey = function.signatureKey;
    record.declaredTypeKey = function.declaredTypeKey;
    record.declaredErrorTypeKey = function.declaredErrorTypeKey;
    record.declaredOwnership = metadataOwnership(function.declaredOwnership);
    record.mangledName = function.mangledName;
    if (publishLifetimeContracts)
    {
        record.returnLifetimeName = function.returnLifetimeName;
        record.lifetimeParameters = function.lifetimeParameters;
        record.lifetimeOutlivesConstraints = function.lifetimeOutlivesConstraints;
    }
}

bool buildNamespaceFunctionMetadata(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::ClassMemberSymbol> members,
    const frontend::NamespaceFunctionSymbol& function,
    bool publishLifetimeContracts,
    LibraryMetadataNamespaceFunction& record,
    std::string& error)
{
    if (function.mangledName.empty())
    {
        error = "public namespace function '" + function.namespaceName + "." +
            function.signatureKey + "' has no physical identity";
        return false;
    }
    initializeNamespaceFunctionMetadata(function, publishLifetimeContracts, record);
    const std::size_t callableOrdinal = members.size() +
        static_cast<std::size_t>(
            &function - compilation.namespaceFunctionModel()->functions().data());
    for (const language::EscapeClass escapeClass :
         compilation.methodBodyModel()->parameterEscapeSummary(callableOrdinal))
    {
        record.parameterEscapeSummary.push_back(metadataEscapeClass(escapeClass));
    }
    for (const frontend::MethodParameterSymbol& parameter : function.parameters)
    {
        LibraryMetadataParameter parameterRecord;
        if (!buildNamespaceFunctionParameterMetadata(
                function,
                parameter,
                publishLifetimeContracts,
                parameterRecord,
                error))
        {
            return false;
        }
        record.parameters.push_back(std::move(parameterRecord));
    }
    return true;
}

bool namespaceFunctionMetadataLess(
    const LibraryMetadataNamespaceFunction& left,
    const LibraryMetadataNamespaceFunction& right)
{
    return std::tie(left.namespaceName, left.signatureKey) <
        std::tie(right.namespaceName, right.signatureKey);
}

bool appendNamespaceFunctionMetadata(
    const frontend::ProjectCompilation& compilation,
    std::span<const frontend::ClassMemberSymbol> members,
    std::size_t rootProjectOrdinal,
    bool publishLifetimeContracts,
    LibraryMetadata& metadata,
    std::string& error)
{
    if (compilation.namespaceFunctionModel() == nullptr) return true;
    for (const frontend::NamespaceFunctionSymbol& function :
         compilation.namespaceFunctionModel()->functions())
    {
        if (function.projectOrdinal != rootProjectOrdinal ||
            function.accessibility != frontend::NamespaceFunctionAccessibility::Public ||
            (function.isExternC && !function.isExportedC))
        {
            continue;
        }
        LibraryMetadataNamespaceFunction record;
        if (!buildNamespaceFunctionMetadata(
                compilation,
                members,
                function,
                publishLifetimeContracts,
                record,
                error))
        {
            return false;
        }
        metadata.namespaceFunctions.push_back(std::move(record));
    }
    std::sort(
        metadata.namespaceFunctions.begin(),
        metadata.namespaceFunctions.end(),
        namespaceFunctionMetadataLess);
    return true;
}

LibraryMetadataNamespaceState buildNamespaceStateMetadata(
    const frontend::NamespaceStateSymbol& state)
{
    LibraryMetadataNamespaceState record;
    record.accessibility = 1U;
    if (state.isConst) record.flags |= 1U;
    if (state.declaredTypeSyntax != nullptr &&
        state.declaredTypeSyntax->isDynamicTrait())
    {
        record.flags |= 2U;
    }
    record.initializerKind =
        static_cast<LibraryMetadataNamespaceStateInitializerKind>(state.initializerKind);
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
    return record;
}

bool namespaceStateMetadataLess(
    const LibraryMetadataNamespaceState& left,
    const LibraryMetadataNamespaceState& right)
{
    return std::tie(left.namespaceName, left.name) <
        std::tie(right.namespaceName, right.name);
}

void appendNamespaceStateMetadata(
    const frontend::ProjectCompilation& compilation,
    std::size_t rootProjectOrdinal,
    LibraryMetadata& metadata)
{
    if (compilation.namespaceFunctionModel() == nullptr) return;
    for (const frontend::NamespaceStateSymbol& state :
         compilation.namespaceFunctionModel()->states())
    {
        if (state.projectOrdinal != rootProjectOrdinal ||
            state.accessibility != frontend::NamespaceFunctionAccessibility::Public ||
            state.isPrecompiled)
        {
            continue;
        }
        metadata.namespaceStates.push_back(buildNamespaceStateMetadata(state));
    }
    std::sort(
        metadata.namespaceStates.begin(),
        metadata.namespaceStates.end(),
        namespaceStateMetadataLess);
}

bool buildMetadataModel(
    const frontend::ProjectCompilation& compilation,
    LibraryMetadata& metadata,
    std::string& error)
{
    std::size_t rootProjectOrdinalValue = 0;
    if (!initializeMetadataModel(
            compilation, metadata, rootProjectOrdinalValue, error))
    {
        return false;
    }
    constexpr bool publishLifetimeContracts = true;
    const auto types = compilation.nameLookup()->namedTypes();
    const auto members = compilation.memberModel()->members();
    std::vector<std::size_t> rootTypeOrdinals;
    collectPublishedRootTypeOrdinals(
        compilation, types, members, rootProjectOrdinalValue, rootTypeOrdinals);
    if (!appendMetadataTypes(
            compilation,
            types,
            members,
            rootTypeOrdinals,
            publishLifetimeContracts,
            metadata,
            error))
    {
        return false;
    }
    std::unordered_set<std::size_t> rootTypes;
    if (!buildMemberMetadata(
            compilation,
            types,
            members,
            rootTypeOrdinals,
            rootProjectOrdinalValue,
            publishLifetimeContracts,
            metadata,
            rootTypes,
            error))
    {
        return false;
    }
    if (!appendNamespaceFunctionMetadata(
            compilation,
            members,
            rootProjectOrdinalValue,
            publishLifetimeContracts,
            metadata,
            error))
    {
        return false;
    }
    appendNamespaceStateMetadata(compilation, rootProjectOrdinalValue, metadata);
    std::unordered_set<std::string> declaredTypeKeys;
    std::unordered_set<std::string> emittedSpecializedTypeKeys;
    std::unordered_set<std::string> externalTypeKeys;
    if (!buildGenericMetadata(
            compilation,
            types,
            rootTypes,
            rootProjectOrdinalValue,
            metadata,
            declaredTypeKeys,
            emittedSpecializedTypeKeys,
            externalTypeKeys,
            error))
    {
        return false;
    }
    appendMetadataDispatchTables(
        compilation, types, members, rootProjectOrdinalValue, metadata);
    appendMetadataExports(
        compilation, types, members, rootProjectOrdinalValue, metadata);
    finalizeExternalMetadataTypes(
        metadata, declaredTypeKeys, emittedSpecializedTypeKeys, externalTypeKeys);
    if (!appendBinaryDependencies(compilation, metadata, error)) return false;
    appendPlatformImports(compilation, metadata);
    return true;
}

}
