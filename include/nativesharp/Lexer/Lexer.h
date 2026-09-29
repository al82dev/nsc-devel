#pragma once

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Lexer/Token.h"
#include "nativesharp/Source/SourceFile.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nativesharp::lexer
{
class Lexer final
{
public:
    Lexer(const source::SourceFile& sourceFile, diagnostics::DiagnosticEngine& diagnostics) noexcept;

    std::vector<Token> lexAll();
    Token nextToken();

private:
    bool isAtEnd() const noexcept;
    char current() const noexcept;
    char peek(std::uint32_t distance = 1) const noexcept;
    char advance() noexcept;
    bool match(char expected) noexcept;

    SourceLocation location(std::uint32_t offset) const noexcept;
    SourceRange range(std::uint32_t begin, std::uint32_t end) const noexcept;
    std::string_view text(std::uint32_t begin, std::uint32_t end) const noexcept;

    void skipTrivia();
    void skipLineComment();
    bool skipBlockComment();

    Token lexIdentifierOrKeyword(std::uint32_t begin);
    Token lexNumber(std::uint32_t begin);
    Token lexCharacter(std::uint32_t begin);
    Token lexString(std::uint32_t begin);
    Token lexInterpolatedString(std::uint32_t begin);
    Token lexOperatorOrPunctuation(std::uint32_t begin);
    Token lexAssemblyBodyToken();

    Token makeToken(TokenKind kind, std::uint32_t begin) const;

    bool decodeEscape(std::string& output, std::uint32_t literalBegin);
    bool decodeEscapeByte(std::uint8_t& output, std::uint32_t literalBegin);
    void reportInvalid(
        std::uint32_t begin,
        std::uint32_t end,
        std::string message,
        diagnostics::DiagnosticId id = diagnostics::DiagnosticId::UnexpectedCharacter);

    const source::SourceFile& sourceFile_;
    diagnostics::DiagnosticEngine& diagnostics_;
    std::string_view buffer_;
    std::uint32_t offset_ = 0;
    bool awaitingAssemblyBrace_ = false;
    bool insideAssemblyBody_ = false;
    std::uint32_t assemblyBraceDepth_ = 0;
};
}
