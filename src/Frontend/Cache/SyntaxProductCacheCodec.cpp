#include "SyntaxProductCacheCodec.h"

#include "CachePayloadReader.h"
#include "CachePayloadWriter.h"
#include "../Generics/Cache/TemplateCatalogCodec.h"

#include <cstring>
#include <limits>
#include <span>
#include <utility>

namespace nativesharp::frontend::internal
{
namespace
{
constexpr std::string_view syntaxUnitPayloadSchema = "nsc-syntax-product-unit-v1";

void writeSourceRange(std::string& out, SourceRange range)
{
    CachePayloadWriter::u32(out, range.begin().fileId().value());
    CachePayloadWriter::u32(out, range.begin().offset());
    CachePayloadWriter::u32(out, range.end().fileId().value());
    CachePayloadWriter::u32(out, range.end().offset());
}

bool readSourceRange(
    CachePayloadReader& reader,
    SourceRange& range,
    FileId relocatedFileId)
{
    std::uint32_t beginFile = 0;
    std::uint32_t beginOffset = 0;
    std::uint32_t endFile = 0;
    std::uint32_t endOffset = 0;
    if (!reader.u32(beginFile) || !reader.u32(beginOffset) ||
        !reader.u32(endFile) || !reader.u32(endOffset))
    {
        return false;
    }
    FileId beginFileId{beginFile};
    FileId endFileId{endFile};
    if (relocatedFileId.isValid() && (beginFileId.isValid() || endFileId.isValid()))
    {
        if (!beginFileId.isValid() || !endFileId.isValid() || beginFileId != endFileId)
        {
            return false;
        }
        beginFileId = relocatedFileId;
        endFileId = relocatedFileId;
    }
    range = SourceRange{
        SourceLocation{beginFileId, beginOffset},
        SourceLocation{endFileId, endOffset}};
    return true;
}

void serializeTokenValue(std::string& out, const lexer::TokenValue& value)
{
    CachePayloadWriter::byte(out, static_cast<std::uint8_t>(value.index()));
    if (const std::uint64_t* integer = std::get_if<std::uint64_t>(&value))
    {
        CachePayloadWriter::u64(out, *integer);
    }
    else if (const double* floating = std::get_if<double>(&value))
    {
        std::uint64_t bits = 0;
        std::memcpy(&bits, floating, sizeof(bits));
        CachePayloadWriter::u64(out, bits);
    }
    else if (const std::uint8_t* byte = std::get_if<std::uint8_t>(&value))
    {
        CachePayloadWriter::byte(out, *byte);
    }
    else if (const std::string* text = std::get_if<std::string>(&value))
    {
        CachePayloadWriter::string(out, *text);
    }
}

bool deserializeTokenValue(CachePayloadReader& reader, lexer::TokenValue& value)
{
    std::uint8_t kind = 0;
    if (!reader.byte(kind)) return false;
    if (kind == 0U)
    {
        value = std::monostate{};
        return true;
    }
    if (kind == 1U)
    {
        std::uint64_t integer = 0;
        if (!reader.u64(integer)) return false;
        value = integer;
        return true;
    }
    if (kind == 2U)
    {
        std::uint64_t bits = 0;
        if (!reader.u64(bits)) return false;
        double floating = 0.0;
        std::memcpy(&floating, &bits, sizeof(floating));
        value = floating;
        return true;
    }
    if (kind == 3U)
    {
        std::uint8_t byte = 0;
        if (!reader.byte(byte)) return false;
        value = byte;
        return true;
    }
    if (kind == 4U)
    {
        std::string text;
        if (!reader.string(text)) return false;
        value = std::move(text);
        return true;
    }
    return false;
}

bool deserializeToken(
    CachePayloadReader& reader,
    const source::SourceFile& sourceFile,
    FileId relocatedFileId,
    lexer::Token& token)
{
    std::uint32_t kind = 0;
    SourceRange range;
    std::string storedLexeme;
    lexer::TokenValue value;
    if (!reader.u32(kind) || !readSourceRange(reader, range, relocatedFileId) ||
        !reader.string(storedLexeme) || !deserializeTokenValue(reader, value))
    {
        return false;
    }
    if (range.begin().fileId() != sourceFile.id() ||
        range.end().fileId() != sourceFile.id() ||
        range.end().offset() < range.begin().offset() ||
        range.end().offset() > sourceFile.buffer().size())
    {
        return false;
    }
    const std::string_view lexeme = sourceFile.buffer().substr(
        range.begin().offset(),
        range.end().offset() - range.begin().offset());
    if (lexeme != storedLexeme) return false;
    token = lexer::Token(static_cast<lexer::TokenKind>(kind), range, lexeme, std::move(value));
    return true;
}

// Restores the same payload in exact-cache mode or relocates every valid range for publication.
bool deserializeSyntaxProductUnitImpl(
    const source::SourceFile& sourceFile,
    std::string_view payload,
    FileId relocatedFileId,
    std::vector<lexer::Token>& tokens,
    std::unique_ptr<ast::AstArena>& arena,
    ast::AstNode*& syntaxTree)
{
    CachePayloadReader reader(payload);
    std::string schema;
    std::uint32_t tokenCount = 0;
    if (!reader.string(schema) || schema != syntaxUnitPayloadSchema ||
        !reader.u32(tokenCount) || tokenCount > sourceFile.buffer().size() + 1U)
    {
        return false;
    }
    std::vector<lexer::Token> restoredTokens;
    restoredTokens.reserve(tokenCount);
    for (std::uint32_t index = 0; index < tokenCount; ++index)
    {
        lexer::Token token(lexer::TokenKind::Invalid, SourceRange{}, {});
        if (!deserializeToken(reader, sourceFile, relocatedFileId, token)) return false;
        restoredTokens.push_back(std::move(token));
    }
    std::string treePayload;
    if (!reader.string(treePayload) || !reader.remaining().empty()) return false;
    auto restoredArena = std::make_unique<ast::AstArena>();
    CachePayloadReader treeReader(treePayload);
    ast::AstNode* restoredTree = relocatedFileId.isValid()
        ? deserializeAstNodeTree(*restoredArena, treeReader, relocatedFileId)
        : deserializeAstNodeTree(*restoredArena, treeReader);
    if (restoredTree == nullptr || !treeReader.remaining().empty()) return false;
    tokens = std::move(restoredTokens);
    arena = std::move(restoredArena);
    syntaxTree = restoredTree;
    return true;
}
}

// Serializes the complete lexer and parser product of one unit. Token lexemes are persisted only
// for validation; restored string_views point back into the current SourceFile buffer.
std::string serializeSyntaxProductUnit(const ProjectSyntaxUnit& unit)
{
    std::string out;
    CachePayloadWriter::string(out, syntaxUnitPayloadSchema);
    CachePayloadWriter::u32(out, static_cast<std::uint32_t>(unit.tokens().size()));
    for (const lexer::Token& token : unit.tokens())
    {
        CachePayloadWriter::u32(out, static_cast<std::uint32_t>(token.kind()));
        writeSourceRange(out, token.range());
        CachePayloadWriter::string(out, token.lexeme());
        serializeTokenValue(out, token.value());
    }
    CachePayloadWriter::string(out, serializeAstNodeTree(unit.syntaxTree()));
    return out;
}

// Deserializes into detached staging storage. The caller installs it only after every unit in the
// corpus has passed schema, token, identity and trailing-byte validation, preventing partial hits.
bool deserializeSyntaxProductUnit(
    const source::SourceFile& sourceFile,
    std::string_view payload,
    std::vector<lexer::Token>& tokens,
    std::unique_ptr<ast::AstArena>& arena,
    ast::AstNode*& syntaxTree)
{
    return deserializeSyntaxProductUnitImpl(
        sourceFile, payload, FileId{}, tokens, arena, syntaxTree);
}

bool deserializeRelocatableSyntaxProductUnit(
    const source::SourceFile& sourceFile,
    std::string_view payload,
    std::vector<lexer::Token>& tokens,
    std::unique_ptr<ast::AstArena>& arena,
    ast::AstNode*& syntaxTree)
{
    return deserializeSyntaxProductUnitImpl(
        sourceFile, payload, sourceFile.id(), tokens, arena, syntaxTree);
}
}
