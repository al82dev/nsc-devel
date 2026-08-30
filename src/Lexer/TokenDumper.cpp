#include "nativesharp/Lexer/TokenDumper.h"

#include "nativesharp/Lexer/TokenKind.h"

#include <iomanip>
#include <ostream>

namespace nativesharp::lexer
{
namespace
{
void writeEscaped(std::string_view text, std::ostream& output)
{
    output << '"';
    for (char rawValue : text)
    {
        const unsigned char value = static_cast<unsigned char>(rawValue);
        switch (value)
        {
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            case '\\': output << "\\\\"; break;
            case '"': output << "\\\""; break;
            default:
                if (value >= 0x20 && value <= 0x7E)
                {
                    output << static_cast<char>(value);
                }
                else
                {
                    output << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                           << static_cast<unsigned>(value) << std::dec << std::setfill(' ');
                }
                break;
        }
    }
    output << '"';
}
}

void TokenDumper::dump(std::span<const Token> tokens, std::ostream& output) const
{
    for (const Token& token : tokens)
    {
        output << tokenKindName(token.kind()) << ' ';
        writeEscaped(token.lexeme(), output);
        output << " @" << token.range().begin().offset() << ':' << token.range().end().offset() << '\n';
    }
}
}
