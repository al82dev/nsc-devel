#include "nativesharp/Lexer/Lexer.h"

#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"

#include <charconv>
#include <cctype>
#include <cstdlib>
#include <string_view>
#include <unordered_map>

namespace nativesharp::lexer
{
namespace
{
bool isIdentifierStart(char value) noexcept
{
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || value == '_';
}

bool isIdentifierPart(char value) noexcept
{
    return isIdentifierStart(value) || (value >= '0' && value <= '9');
}

bool isDecimalDigit(char value) noexcept
{
    return value >= '0' && value <= '9';
}

bool isHexDigit(char value) noexcept
{
    return isDecimalDigit(value) || (value >= 'A' && value <= 'F') || (value >= 'a' && value <= 'f');
}

int hexValue(char value) noexcept
{
    if (value >= '0' && value <= '9')
    {
        return value - '0';
    }

    if (value >= 'A' && value <= 'F')
    {
        return 10 + value - 'A';
    }

    return 10 + value - 'a';
}

TokenKind keywordKind(std::string_view text) noexcept
{
    static const std::unordered_map<std::string_view, TokenKind> keywords = {
        {"void", TokenKind::KwVoid},
        {"bool", TokenKind::KwBool},
        {"sbyte", TokenKind::KwSByte},
        {"byte", TokenKind::KwByte},
        {"char", TokenKind::KwChar},
        {"short", TokenKind::KwShort},
        {"ushort", TokenKind::KwUShort},
        {"int", TokenKind::KwInt},
        {"uint", TokenKind::KwUInt},
        {"long", TokenKind::KwLong},
        {"ulong", TokenKind::KwULong},
        {"nint", TokenKind::KwNInt},
        {"nuint", TokenKind::KwNUInt},
        {"float", TokenKind::KwFloat},
        {"double", TokenKind::KwDouble},
        {"string", TokenKind::KwString},
        {"const", TokenKind::KwConst},
        {"static", TokenKind::KwStatic},
        {"readonly", TokenKind::KwReadonly},
        {"volatile", TokenKind::KwVolatile},
        {"inline", TokenKind::KwInline},
        {"forceinline", TokenKind::KwForceInline},
        {"noinline", TokenKind::KwNoInline},
        {"noheap", TokenKind::KwNoHeap},
        {"public", TokenKind::KwPublic},
        {"internal", TokenKind::KwInternal},
        {"protected", TokenKind::KwProtected},
        {"private", TokenKind::KwPrivate},
        {"if", TokenKind::KwIf},
        {"else", TokenKind::KwElse},
        {"while", TokenKind::KwWhile},
        {"do", TokenKind::KwDo},
        {"for", TokenKind::KwFor},
        {"switch", TokenKind::KwSwitch},
        {"case", TokenKind::KwCase},
        {"break", TokenKind::KwBreak},
        {"continue", TokenKind::KwContinue},
        {"return", TokenKind::KwReturn},
        {"try", TokenKind::KwTry},
        {"catch", TokenKind::KwCatch},
        {"finally", TokenKind::KwFinally},
        {"throw", TokenKind::KwThrow},
        {"struct", TokenKind::KwStruct},
        {"class", TokenKind::KwClass},
        {"interface", TokenKind::KwInterface},
        {"enum", TokenKind::KwEnum},
        {"namespace", TokenKind::KwNamespace},
        {"using", TokenKind::KwUsing},
        {"new", TokenKind::KwNew},
        {"delete", TokenKind::KwDelete},
        {"destroy", TokenKind::KwDestroy},
        {"stackalloc", TokenKind::KwStackalloc},
        {"sizeof", TokenKind::KwSizeof},
        {"alignof", TokenKind::KwAlignof},
        {"offsetof", TokenKind::KwOffsetof},
        {"static_assert", TokenKind::KwStaticAssert},
        {"default", TokenKind::KwDefault},
        {"extern", TokenKind::KwExtern},
        {"export", TokenKind::KwExport},
        {"vectorcall", TokenKind::KwVectorCall},
        {"true", TokenKind::KwTrue},
        {"false", TokenKind::KwFalse},
        {"null", TokenKind::KwNull},
        {"this", TokenKind::KwThis},
        {"base", TokenKind::KwBase},
        {"virtual", TokenKind::KwVirtual},
        {"override", TokenKind::KwOverride},
        {"abstract", TokenKind::KwAbstract},
        {"sealed", TokenKind::KwSealed},
        {"get", TokenKind::KwGet},
        {"set", TokenKind::KwSet},
        {"operator", TokenKind::KwOperator},
        {"implicit", TokenKind::KwImplicit},
        {"explicit", TokenKind::KwExplicit},
        {"is", TokenKind::KwIs},
        {"as", TokenKind::KwAs},
        {"delegate", TokenKind::KwDelegate},
        {"event", TokenKind::KwEvent},
        {"raii", TokenKind::KwRaii},
        {"move", TokenKind::KwMove},
        {"owned", TokenKind::KwOwned},
        {"borrowed", TokenKind::KwBorrowed},
        {"var", TokenKind::KwVar},
        {"foreach", TokenKind::KwForeach},
        {"in", TokenKind::KwIn}
    };

    const auto iterator = keywords.find(text);
    return iterator == keywords.end() ? TokenKind::Identifier : iterator->second;
}
}

Lexer::Lexer(
    const source::SourceFile& sourceFile,
    diagnostics::DiagnosticEngine& diagnostics) noexcept
    : sourceFile_(sourceFile),
      diagnostics_(diagnostics),
      buffer_(sourceFile.buffer())
{
    if (buffer_.size() >= 3 &&
        static_cast<unsigned char>(buffer_[0]) == 0xEF &&
        static_cast<unsigned char>(buffer_[1]) == 0xBB &&
        static_cast<unsigned char>(buffer_[2]) == 0xBF)
    {
        offset_ = 3;
    }
}

std::vector<Token> Lexer::lexAll()
{
    std::vector<Token> tokens;

    while (true)
    {
        Token token = nextToken();
        const bool done = token.kind() == TokenKind::EndOfFile;
        tokens.push_back(std::move(token));
        if (done)
        {
            break;
        }
    }

    return tokens;
}

Token Lexer::nextToken()
{
    if (insideAssemblyBody_)
    {
        return lexAssemblyBodyToken();
    }

    skipTrivia();

    const std::uint32_t begin = offset_;
    if (isAtEnd())
    {
        awaitingAssemblyBrace_ = false;
        return Token(TokenKind::EndOfFile, range(begin, begin), {});
    }

    Token token = Token(TokenKind::Invalid, range(begin, begin), {});
    if (isIdentifierStart(current()))
    {
        token = lexIdentifierOrKeyword(begin);
    }
    else if (isDecimalDigit(current()))
    {
        token = lexNumber(begin);
    }
    else if (current() == '\'')
    {
        token = lexCharacter(begin);
    }
    else if (current() == '"')
    {
        token = lexString(begin);
    }
    else if (current() == '$' && peek() == '"')
    {
        token = lexInterpolatedString(begin);
    }
    else
    {
        token = lexOperatorOrPunctuation(begin);
    }

    if (awaitingAssemblyBrace_)
    {
        if (token.kind() == TokenKind::LeftBrace)
        {
            insideAssemblyBody_ = true;
            assemblyBraceDepth_ = 1;
        }
        awaitingAssemblyBrace_ = false;
    }
    else if (token.kind() == TokenKind::Identifier && token.lexeme() == "asm")
    {
        // `asm` remains contextual. Whitespace and comments may separate it from `{`, while any
        // real token other than `{` cancels assembly mode and leaves the identifier untouched.
        awaitingAssemblyBrace_ = true;
    }

    return token;
}

Token Lexer::lexAssemblyBodyToken()
{
    const std::uint32_t begin = offset_;
    if (isAtEnd())
    {
        return Token(TokenKind::EndOfFile, range(begin, begin), {});
    }

    if (current() == '\r' || current() == '\n')
    {
        const char first = advance();
        if (first == '\r' && current() == '\n') advance();
        return Token(TokenKind::AssemblyNewLine, range(begin, offset_), text(begin, offset_));
    }

    // Emit one exact physical-line fragment. Inner braces remain part of the raw text while their
    // depth protects the outer statement terminator. The sole outer `}` is returned as ordinary
    // punctuation so the parser keeps ownership of the grammatical boundary.
    while (!isAtEnd() && current() != '\r' && current() != '\n')
    {
        if (current() == '{')
        {
            ++assemblyBraceDepth_;
            advance();
            continue;
        }
        if (current() == '}')
        {
            if (assemblyBraceDepth_ == 1)
            {
                if (offset_ != begin)
                {
                    return Token(TokenKind::AssemblyText, range(begin, offset_), text(begin, offset_));
                }
                advance();
                insideAssemblyBody_ = false;
                assemblyBraceDepth_ = 0;
                return Token(TokenKind::RightBrace, range(begin, offset_), text(begin, offset_));
            }
            --assemblyBraceDepth_;
            advance();
            continue;
        }
        advance();
    }

    return Token(TokenKind::AssemblyText, range(begin, offset_), text(begin, offset_));
}

bool Lexer::isAtEnd() const noexcept
{
    return offset_ >= buffer_.size();
}

char Lexer::current() const noexcept
{
    return isAtEnd() ? '\0' : buffer_[offset_];
}

char Lexer::peek(std::uint32_t distance) const noexcept
{
    const std::size_t index = static_cast<std::size_t>(offset_) + distance;
    return index >= buffer_.size() ? '\0' : buffer_[index];
}

char Lexer::advance() noexcept
{
    const char value = current();
    if (!isAtEnd())
    {
        ++offset_;
    }
    return value;
}

bool Lexer::match(char expected) noexcept
{
    if (current() != expected)
    {
        return false;
    }

    ++offset_;
    return true;
}

SourceLocation Lexer::location(std::uint32_t offset) const noexcept
{
    return SourceLocation(sourceFile_.id(), offset);
}

SourceRange Lexer::range(std::uint32_t begin, std::uint32_t end) const noexcept
{
    return SourceRange(location(begin), location(end));
}

std::string_view Lexer::text(std::uint32_t begin, std::uint32_t end) const noexcept
{
    return buffer_.substr(begin, end - begin);
}

void Lexer::skipTrivia()
{
    while (!isAtEnd())
    {
        const char value = current();
        if (value == ' ' || value == '\t' || value == '\v' || value == '\f' || value == '\n' || value == '\r')
        {
            advance();
            continue;
        }

        if (value == '/' && peek() == '/')
        {
            skipLineComment();
            continue;
        }

        if (value == '/' && peek() == '*')
        {
            if (!skipBlockComment())
            {
                return;
            }
            continue;
        }

        return;
    }
}

void Lexer::skipLineComment()
{
    advance();
    advance();

    while (!isAtEnd() && current() != '\n' && current() != '\r')
    {
        advance();
    }
}

bool Lexer::skipBlockComment()
{
    const std::uint32_t begin = offset_;
    advance();
    advance();

    while (!isAtEnd())
    {
        if (current() == '*' && peek() == '/')
        {
            advance();
            advance();
            return true;
        }
        advance();
    }

    diagnostics_.report(
        diagnostics::DiagnosticId::UnterminatedBlockComment,
        diagnostics::DiagnosticSeverity::Error,
        range(begin, offset_),
        "unterminated block comment");
    return false;
}

Token Lexer::lexIdentifierOrKeyword(std::uint32_t begin)
{
    advance();
    while (isIdentifierPart(current()))
    {
        advance();
    }

    const std::string_view lexeme = text(begin, offset_);
    return Token(keywordKind(lexeme), range(begin, offset_), lexeme);
}

Token Lexer::lexNumber(std::uint32_t begin)
{
    int base = 10;
    std::uint32_t digitsBegin = begin;

    if (current() == '0' && (peek() == 'x' || peek() == 'X'))
    {
        base = 16;
        advance();
        advance();
        digitsBegin = offset_;
        while (isHexDigit(current()))
        {
            advance();
        }

        if (digitsBegin == offset_)
        {
            reportInvalid(
                begin,
                offset_,
                "hexadecimal literal requires at least one hexadecimal digit",
                diagnostics::DiagnosticId::InvalidNumericLiteral);
            return Token(TokenKind::Invalid, range(begin, offset_), text(begin, offset_));
        }
    }
    else if (current() == '0' && (peek() == 'b' || peek() == 'B'))
    {
        base = 2;
        advance();
        advance();
        digitsBegin = offset_;
        while (current() == '0' || current() == '1')
        {
            advance();
        }

        if (digitsBegin == offset_)
        {
            reportInvalid(
                begin,
                offset_,
                "binary literal requires at least one binary digit",
                diagnostics::DiagnosticId::InvalidNumericLiteral);
            return Token(TokenKind::Invalid, range(begin, offset_), text(begin, offset_));
        }
    }
    else
    {
        while (isDecimalDigit(current()))
        {
            advance();
        }

        bool floating = false;
        if (current() == '.' && isDecimalDigit(peek()))
        {
            floating = true;
            advance();
            while (isDecimalDigit(current()))
            {
                advance();
            }
        }

        if (current() == 'e' || current() == 'E')
        {
            floating = true;
            advance();
            if (current() == '+' || current() == '-')
            {
                advance();
            }

            const std::uint32_t exponentBegin = offset_;
            while (isDecimalDigit(current()))
            {
                advance();
            }

            if (exponentBegin == offset_)
            {
                reportInvalid(
                begin,
                offset_,
                "floating literal exponent requires decimal digits",
                diagnostics::DiagnosticId::InvalidNumericLiteral);
                return Token(TokenKind::Invalid, range(begin, offset_), text(begin, offset_));
            }
        }

        if (current() == 'f' || current() == 'F')
        {
            floating = true;
            advance();
        }

        if (floating)
        {
            const std::string lexeme(text(begin, offset_));
            char* end = nullptr;
            const double value = std::strtod(lexeme.c_str(), &end);
            if (end == nullptr || static_cast<std::size_t>(end - lexeme.c_str()) == 0)
            {
                reportInvalid(
                begin,
                offset_,
                "invalid floating-point literal",
                diagnostics::DiagnosticId::InvalidNumericLiteral);
                return Token(TokenKind::Invalid, range(begin, offset_), text(begin, offset_));
            }

            return Token(TokenKind::FloatingLiteral, range(begin, offset_), text(begin, offset_), value);
        }
    }

    const std::uint32_t digitsEnd = offset_;
    std::string suffix;
    while ((current() == 'u' || current() == 'U' || current() == 'l' || current() == 'L') && suffix.size() < 2)
    {
        suffix.push_back(current());
        advance();
    }

    bool suffixValid = suffix.empty() || suffix == "u" || suffix == "U" || suffix == "l" || suffix == "L" ||
        suffix == "ul" || suffix == "uL" || suffix == "Ul" || suffix == "UL" || suffix == "lu" ||
        suffix == "lU" || suffix == "Lu" || suffix == "LU";

    if (!suffixValid || current() == 'u' || current() == 'U' || current() == 'l' || current() == 'L')
    {
        while (current() == 'u' || current() == 'U' || current() == 'l' || current() == 'L')
        {
            advance();
        }
        reportInvalid(
                begin,
                offset_,
                "invalid integer literal suffix",
                diagnostics::DiagnosticId::InvalidNumericLiteral);
        return Token(TokenKind::Invalid, range(begin, offset_), text(begin, offset_));
    }

    std::uint64_t value = 0;
    const std::string_view digits = text(digitsBegin, digitsEnd);
    const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value, base);
    if (result.ec != std::errc() || result.ptr != digits.data() + digits.size())
    {
        reportInvalid(
                begin,
                offset_,
                "integer literal is not representable as an unsigned 64-bit value",
                diagnostics::DiagnosticId::InvalidNumericLiteral);
        return Token(TokenKind::Invalid, range(begin, offset_), text(begin, offset_));
    }

    return Token(TokenKind::IntegerLiteral, range(begin, offset_), text(begin, offset_), value);
}

Token Lexer::lexCharacter(std::uint32_t begin)
{
    advance();
    std::uint8_t value = 0;
    bool valid = true;

    if (isAtEnd() || current() == '\n' || current() == '\r' || current() == '\'')
    {
        valid = false;
    }
    else if (current() == '\\')
    {
        advance();
        valid = decodeEscapeByte(value, begin);
    }
    else
    {
        const unsigned char raw = static_cast<unsigned char>(advance());
        valid = raw >= 0x20 && raw <= 0x7E && raw != '\\' && raw != '\'';
        value = raw;
    }

    if (!match('\''))
    {
        valid = false;
        while (!isAtEnd() && current() != '\'' && current() != '\n' && current() != '\r')
        {
            advance();
        }
        match('\'');
    }

    if (!valid)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidCharacterLiteral,
            diagnostics::DiagnosticSeverity::Error,
            range(begin, offset_),
            "invalid character literal; exactly one ASCII char or supported escape is required");
        return Token(TokenKind::Invalid, range(begin, offset_), text(begin, offset_));
    }

    return Token(TokenKind::CharacterLiteral, range(begin, offset_), text(begin, offset_), value);
}

Token Lexer::lexString(std::uint32_t begin)
{
    if (current() == '"' && peek() == '"' && peek(2) == '"')
    {
        return lexRawString(begin);
    }

    advance();
    std::string decoded;
    bool valid = true;

    while (!isAtEnd() && current() != '"' && current() != '\n' && current() != '\r')
    {
        if (current() == '\\')
        {
            advance();
            if (!decodeEscape(decoded, begin))
            {
                valid = false;
            }
            continue;
        }

        const unsigned char raw = static_cast<unsigned char>(advance());
        if (raw < 0x20)
        {
            valid = false;
            decoded.push_back(static_cast<char>(raw));
            continue;
        }
        if (raw < 0x80)
        {
            decoded.push_back(static_cast<char>(raw));
            continue;
        }

        // Stage 41 source strings accept well-formed UTF-8 and preserve the encoded
        // bytes verbatim. `char` remains an 8-bit code unit; Unicode scalar-value
        // decoding is deliberately outside this stage.
        unsigned continuationCount = 0;
        unsigned char secondMinimum = 0x80;
        unsigned char secondMaximum = 0xBF;
        if (raw >= 0xC2 && raw <= 0xDF)
        {
            continuationCount = 1;
        }
        else if (raw >= 0xE0 && raw <= 0xEF)
        {
            continuationCount = 2;
            if (raw == 0xE0) secondMinimum = 0xA0;
            if (raw == 0xED) secondMaximum = 0x9F;
        }
        else if (raw >= 0xF0 && raw <= 0xF4)
        {
            continuationCount = 3;
            if (raw == 0xF0) secondMinimum = 0x90;
            if (raw == 0xF4) secondMaximum = 0x8F;
        }
        else
        {
            valid = false;
        }

        decoded.push_back(static_cast<char>(raw));
        for (unsigned index = 0; index < continuationCount; ++index)
        {
            if (isAtEnd())
            {
                valid = false;
                break;
            }
            const unsigned char continuation = static_cast<unsigned char>(current());
            const unsigned char minimum = index == 0 ? secondMinimum : 0x80;
            const unsigned char maximum = index == 0 ? secondMaximum : 0xBF;
            if (continuation < minimum || continuation > maximum)
            {
                valid = false;
                break;
            }
            decoded.push_back(advance());
        }
    }

    if (!match('"'))
    {
        valid = false;
    }

    if (!valid)
    {
        diagnostics_.report(
            diagnostics::DiagnosticId::InvalidStringLiteral,
            diagnostics::DiagnosticSeverity::Error,
            range(begin, offset_),
            "invalid or unterminated string literal");
        return Token(TokenKind::Invalid, range(begin, offset_), text(begin, offset_));
    }

    return Token(TokenKind::StringLiteral, range(begin, offset_), text(begin, offset_), std::move(decoded));
}

Token Lexer::lexRawString(std::uint32_t begin)
{
    std::uint32_t delimiterWidth = 0;
    while (peek(delimiterWidth) == '"') ++delimiterWidth;
    for (std::uint32_t index = 0; index < delimiterWidth; ++index) advance();

    std::string decoded;
    bool valid = true;
    while (!isAtEnd())
    {
        if (current() == '"')
        {
            std::uint32_t quoteCount = 0;
            while (peek(quoteCount) == '"') ++quoteCount;
            if (quoteCount >= delimiterWidth)
            {
                for (std::uint32_t index = 0; index < delimiterWidth; ++index) advance();
                if (!valid)
                {
                    diagnostics_.report(
                        diagnostics::DiagnosticId::InvalidStringLiteral,
                        diagnostics::DiagnosticSeverity::Error,
                        range(begin, offset_),
                        "raw string literal contains an invalid control byte or UTF-8 sequence");
                    return Token(TokenKind::Invalid, range(begin, offset_), text(begin, offset_));
                }
                return Token(TokenKind::StringLiteral, range(begin, offset_), text(begin, offset_),
                    std::move(decoded));
            }
        }

        const unsigned char raw = static_cast<unsigned char>(advance());
        if (raw < 0x20)
        {
            if (raw != '\t' && raw != '\n' && raw != '\r') valid = false;
            decoded.push_back(static_cast<char>(raw));
            continue;
        }
        if (raw < 0x80)
        {
            decoded.push_back(static_cast<char>(raw));
            continue;
        }

        unsigned continuationCount = 0;
        unsigned char secondMinimum = 0x80;
        unsigned char secondMaximum = 0xBF;
        if (raw >= 0xC2 && raw <= 0xDF)
        {
            continuationCount = 1;
        }
        else if (raw >= 0xE0 && raw <= 0xEF)
        {
            continuationCount = 2;
            if (raw == 0xE0) secondMinimum = 0xA0;
            if (raw == 0xED) secondMaximum = 0x9F;
        }
        else if (raw >= 0xF0 && raw <= 0xF4)
        {
            continuationCount = 3;
            if (raw == 0xF0) secondMinimum = 0x90;
            if (raw == 0xF4) secondMaximum = 0x8F;
        }
        else
        {
            valid = false;
        }

        decoded.push_back(static_cast<char>(raw));
        for (unsigned index = 0; index < continuationCount; ++index)
        {
            if (isAtEnd())
            {
                valid = false;
                break;
            }
            const unsigned char continuation = static_cast<unsigned char>(current());
            const unsigned char minimum = index == 0 ? secondMinimum : 0x80;
            const unsigned char maximum = index == 0 ? secondMaximum : 0xBF;
            if (continuation < minimum || continuation > maximum)
            {
                valid = false;
                break;
            }
            decoded.push_back(advance());
        }
    }

    diagnostics_.report(
        diagnostics::DiagnosticId::InvalidStringLiteral,
        diagnostics::DiagnosticSeverity::Error,
        range(begin, offset_),
        "invalid or unterminated raw string literal");
    return Token(TokenKind::Invalid, range(begin, offset_), text(begin, offset_));
}

Token Lexer::lexInterpolatedString(std::uint32_t begin)
{
    // Keep the complete decoded template in one token. Parsing the holes belongs to the T01
    // static-handler lowering pass, which needs brace nesting and format text as a unit.
    advance(); // '$'
    advance(); // opening quote
    std::string decoded;
    bool valid = true;
    std::uint32_t braceDepth = 0;
    while (!isAtEnd() && current() != '\n' && current() != '\r')
    {
        if (current() == '"' && braceDepth == 0)
        {
            advance();
            return valid
                ? Token(TokenKind::InterpolatedStringLiteral, range(begin, offset_),
                    text(begin, offset_), std::move(decoded))
                : Token(TokenKind::Invalid, range(begin, offset_), text(begin, offset_));
        }
        if (current() == '\\')
        {
            // Outside a hole this is an interpolated-string escape. Inside a hole it belongs
            // to the embedded expression (most importantly to an embedded string/char literal)
            // and must survive until that expression is lexed on its own by the parser.
            if (braceDepth != 0)
            {
                decoded.push_back(advance());
                if (!isAtEnd()) decoded.push_back(advance());
                else valid = false;
            }
            else
            {
                advance();
                if (!decodeEscape(decoded, begin)) valid = false;
            }
            continue;
        }
        if (current() == '{')
        {
            if (peek() == '{')
            {
                advance();
                advance();
                decoded += "{{";
                continue;
            }
            ++braceDepth;
            decoded.push_back(advance());
            continue;
        }
        if (current() == '}')
        {
            if (peek() == '}' && braceDepth == 0)
            {
                advance();
                advance();
                decoded += "}}";
                continue;
            }
            if (braceDepth == 0)
            {
                valid = false;
            }
            else
            {
                --braceDepth;
            }
            decoded.push_back(advance());
            continue;
        }
        decoded.push_back(advance());
    }
    valid = false;
    diagnostics_.report(
        diagnostics::DiagnosticId::InvalidStringLiteral,
        diagnostics::DiagnosticSeverity::Error,
        range(begin, offset_),
        "invalid or unterminated interpolated string literal");
    return Token(TokenKind::Invalid, range(begin, offset_), text(begin, offset_));
}

Token Lexer::lexOperatorOrPunctuation(std::uint32_t begin)
{
    const char first = advance();

    switch (first)
    {
        case '(':
            return makeToken(TokenKind::LeftParen, begin);
        case ')':
            return makeToken(TokenKind::RightParen, begin);
        case '{':
            return makeToken(TokenKind::LeftBrace, begin);
        case '}':
            return makeToken(TokenKind::RightBrace, begin);
        case '[':
            return makeToken(TokenKind::LeftBracket, begin);
        case ']':
            return makeToken(TokenKind::RightBracket, begin);
        case ';':
            return makeToken(TokenKind::Semicolon, begin);
        case ',':
            return makeToken(TokenKind::Comma, begin);
        case ':':
            return makeToken(TokenKind::Colon, begin);
        case '?':
            if (match('?'))
            {
                if (match('=')) return makeToken(TokenKind::QuestionQuestionEqual, begin);
                return makeToken(TokenKind::QuestionQuestion, begin);
            }
            if (match('.')) return makeToken(TokenKind::QuestionDot, begin);
            return makeToken(TokenKind::Question, begin);
        case '.':
            if (current() == '.' && peek() == '.')
            {
                advance();
                advance();
                return makeToken(TokenKind::Ellipsis, begin);
            }
            if (match('.')) return makeToken(TokenKind::DotDot, begin);
            return makeToken(TokenKind::Dot, begin);
        case '~':
            return makeToken(TokenKind::Tilde, begin);

        case '+':
            if (match('+')) return makeToken(TokenKind::PlusPlus, begin);
            if (match('=')) return makeToken(TokenKind::PlusEqual, begin);
            return makeToken(TokenKind::Plus, begin);

        case '-':
            if (match('-')) return makeToken(TokenKind::MinusMinus, begin);
            if (match('>')) return makeToken(TokenKind::Arrow, begin);
            if (match('=')) return makeToken(TokenKind::MinusEqual, begin);
            return makeToken(TokenKind::Minus, begin);

        case '*':
            if (match('=')) return makeToken(TokenKind::StarEqual, begin);
            return makeToken(TokenKind::Star, begin);

        case '/':
            if (match('=')) return makeToken(TokenKind::SlashEqual, begin);
            return makeToken(TokenKind::Slash, begin);

        case '%':
            if (match('=')) return makeToken(TokenKind::PercentEqual, begin);
            return makeToken(TokenKind::Percent, begin);

        case '=':
            if (match('=')) return makeToken(TokenKind::EqualEqual, begin);
            if (match('>')) return makeToken(TokenKind::FatArrow, begin);
            return makeToken(TokenKind::Equal, begin);

        case '!':
            if (match('=')) return makeToken(TokenKind::BangEqual, begin);
            return makeToken(TokenKind::Bang, begin);

        case '&':
            if (match('&')) return makeToken(TokenKind::AmpAmp, begin);
            if (match('=')) return makeToken(TokenKind::AmpEqual, begin);
            return makeToken(TokenKind::Amp, begin);

        case '|':
            if (match('|')) return makeToken(TokenKind::PipePipe, begin);
            if (match('=')) return makeToken(TokenKind::PipeEqual, begin);
            return makeToken(TokenKind::Pipe, begin);

        case '^':
            if (match('=')) return makeToken(TokenKind::CaretEqual, begin);
            return makeToken(TokenKind::Caret, begin);

        case '<':
            if (match('<'))
            {
                if (match('=')) return makeToken(TokenKind::LessLessEqual, begin);
                return makeToken(TokenKind::LessLess, begin);
            }
            if (match('=')) return makeToken(TokenKind::LessEqual, begin);
            return makeToken(TokenKind::Less, begin);

        case '>':
            if (match('>'))
            {
                if (match('=')) return makeToken(TokenKind::GreaterGreaterEqual, begin);
                return makeToken(TokenKind::GreaterGreater, begin);
            }
            if (match('=')) return makeToken(TokenKind::GreaterEqual, begin);
            return makeToken(TokenKind::Greater, begin);

        default:
            break;
    }

    reportInvalid(begin, offset_, "unexpected character in source");
    return Token(TokenKind::Invalid, range(begin, offset_), text(begin, offset_));
}

Token Lexer::makeToken(TokenKind kind, std::uint32_t begin) const
{
    return Token(kind, range(begin, offset_), text(begin, offset_));
}

bool Lexer::decodeEscape(std::string& output, std::uint32_t literalBegin)
{
    std::uint8_t value = 0;
    if (!decodeEscapeByte(value, literalBegin))
    {
        return false;
    }

    output.push_back(static_cast<char>(value));
    return true;
}

bool Lexer::decodeEscapeByte(std::uint8_t& output, std::uint32_t literalBegin)
{
    if (isAtEnd())
    {
        return false;
    }

    const char escape = advance();
    switch (escape)
    {
        case '0': output = 0; return true;
        case 'a': output = 7; return true;
        case 'b': output = 8; return true;
        case 'f': output = 12; return true;
        case 'n': output = 10; return true;
        case 'r': output = 13; return true;
        case 't': output = 9; return true;
        case 'v': output = 11; return true;
        case '\\': output = '\\'; return true;
        case '\'': output = '\''; return true;
        case '"': output = '"'; return true;
        case 'x':
        {
            if (!isHexDigit(current()) || !isHexDigit(peek()))
            {
                diagnostics_.report(
                    diagnostics::DiagnosticId::InvalidEscapeSequence,
                    diagnostics::DiagnosticSeverity::Error,
                    location(literalBegin),
                    "\\x escape requires exactly two hexadecimal digits");
                return false;
            }

            const int high = hexValue(advance());
            const int low = hexValue(advance());
            output = static_cast<std::uint8_t>((high << 4) | low);
            return true;
        }
        default:
            diagnostics_.report(
                diagnostics::DiagnosticId::InvalidEscapeSequence,
                diagnostics::DiagnosticSeverity::Error,
                range(offset_ - 1, offset_),
                "unsupported escape sequence");
            return false;
    }
}

void Lexer::reportInvalid(
    std::uint32_t begin,
    std::uint32_t end,
    std::string message,
    diagnostics::DiagnosticId id)
{
    diagnostics_.report(
        id,
        diagnostics::DiagnosticSeverity::Error,
        range(begin, end),
        std::move(message));
}
}
