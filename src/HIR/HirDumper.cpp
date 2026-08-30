#include "nativesharp/HIR/HirDumper.h"

#include "nativesharp/HIR/HirNode.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Sema/ConstantValue.h"
#include "nativesharp/Sema/ConversionKind.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"
#include "nativesharp/Sema/Type.h"

#include <iomanip>
#include <ostream>

namespace nativesharp::hir
{
namespace
{
void dumpEscapedString(const std::string& value, std::ostream& output)
{
    output << '"';
    for (char rawCharacter : value)
    {
        const unsigned char character = static_cast<unsigned char>(rawCharacter);
        switch (character)
        {
            case '\\': output << "\\\\"; break;
            case '"': output << "\\\""; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            case '\0': output << "\\0"; break;
            default:
                if (character >= 0x20 && character <= 0x7E)
                {
                    output << static_cast<char>(character);
                }
                else
                {
                    const std::ios_base::fmtflags flags = output.flags();
                    const char fill = output.fill();
                    output << "\\x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                           << static_cast<unsigned int>(character);
                    output.flags(flags);
                    output.fill(fill);
                }
                break;
        }
    }
    output << '"';
}

void dumpConstant(const sema::ConstantValue& value, std::ostream& output)
{
    switch (value.kind())
    {
        case sema::ConstantKind::None:
            output << "<none>";
            break;
        case sema::ConstantKind::SignedInteger:
            output << value.signedInteger();
            break;
        case sema::ConstantKind::UnsignedInteger:
            output << value.unsignedInteger() << 'u';
            break;
        case sema::ConstantKind::Floating:
            output << std::setprecision(17) << value.floating();
            break;
        case sema::ConstantKind::Boolean:
            output << (value.boolean() ? "true" : "false");
            break;
        case sema::ConstantKind::Null:
            output << "null";
            break;
        case sema::ConstantKind::String:
            dumpEscapedString(value.string(), output);
            break;
    }
}
}

void HirDumper::dump(const HirNode* node, std::ostream& output) const
{
    dumpNode(node, output, 0, "");
}

void HirDumper::dumpNode(
    const HirNode* node,
    std::ostream& output,
    int depth,
    const char* edge) const
{
    if (node == nullptr)
    {
        return;
    }

    for (int index = 0; index < depth; ++index)
    {
        output << "  ";
    }

    output << edge << hirNodeKindName(node->kind());
    if (node->symbol() != nullptr)
    {
        output << " symbol=" << sema::symbolKindName(node->symbol()->kind())
               << ':' << node->symbol()->name();
        if (node->symbol()->isConst())
        {
            output << " const";
        }
        if (node->symbol()->isExternC())
        {
            output << " extern-C";
        }
    }
    if (node->type() != nullptr)
    {
        output << " type=" << node->type()->displayName();
    }
    if (node->operationType() != nullptr)
    {
        output << " operation-type=" << node->operationType()->displayName();
    }
    if (node->operation() != lexer::TokenKind::Invalid)
    {
        output << " op=" << lexer::tokenKindName(node->operation());
    }
    if (node->hasConstant())
    {
        output << " constant=";
        dumpConstant(node->constant(), output);
    }
    if (node->hasConversion())
    {
        output << " conversion=" << sema::ToString(node->conversionKind());
    }
    if (node->hasReadConversion())
    {
        output << " read-conversion=" << sema::ToString(node->readConversionKind());
    }
    if (node->hasResultConversion())
    {
        output << " result-conversion=" << sema::ToString(node->resultConversionKind());
    }
    if (node->requiresBoundsCheck())
    {
        output << " bounds-check";
    }
    if (node->arrayForm())
    {
        output << " array-form";
    }
    if (node->postfix())
    {
        output << " postfix";
    }
    if (node->hasOffset())
    {
        output << " offset=" << node->offset();
    }
    output << '\n';

    dumpNode(node->first(), output, depth + 1, "first: ");
    dumpNode(node->second(), output, depth + 1, "second: ");
    dumpNode(node->third(), output, depth + 1, "third: ");
    for (const HirNode* child : node->children())
    {
        dumpNode(child, output, depth + 1, "child: ");
    }
}
}
