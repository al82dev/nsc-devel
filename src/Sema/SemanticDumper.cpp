#include "nativesharp/Sema/SemanticDumper.h"

#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Lexer/TokenKind.h"
#include "nativesharp/Sema/ConversionKind.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/Type.h"

#include <ostream>

namespace nativesharp::sema
{
void SemanticDumper::dump(
    const ast::AstNode* root,
    const SemanticModel& model,
    std::ostream& output) const
{
    dumpNode(root, model, output, 0, "");
}

void SemanticDumper::dumpNode(
    const ast::AstNode* node,
    const SemanticModel& model,
    std::ostream& output,
    int depth,
    const char* edge) const
{
    if (node == nullptr)
    {
        return;
    }

    const ExpressionInfo* expression = model.get(node);
    const auto conversions = model.conversions(node);
    const ResolvedOperatorInfo* operation = model.getOperator(node);

    if (expression != nullptr || !conversions.empty() || operation != nullptr)
    {
        for (int index = 0; index < depth; ++index)
        {
            output << "  ";
        }

        output << edge << ast::astNodeKindName(node->kind());
        if (!node->text().empty())
        {
            output << " \"" << node->text() << '"';
        }

        if (expression != nullptr)
        {
            output << " : " << expression->type->displayName();
            output << " " << valueCategoryName(expression->category);
            if (expression->modifiable)
            {
                output << " modifiable";
            }
            if (expression->addressable)
            {
                output << " addressable";
            }
            if (expression->requiresBoundsCheck)
            {
                output << " bounds-check";
            }
            if (expression->symbol != nullptr)
            {
                output << " symbol=" << expression->symbol->name();
            }
            if (expression->isConstant)
            {
                output << " constant=";
                dumpConstant(expression->constantValue, output);
            }
        }
        output << '\n';

        if (operation != nullptr)
        {
            for (int index = 0; index <= depth; ++index)
            {
                output << "  ";
            }
            output << "operator " << lexer::tokenKindName(operation->tokenKind)
                   << " operation=" << operation->operationType->displayName()
                   << " result=" << operation->resultType->displayName() << '\n';
        }

        for (const ConversionInfo& conversion : conversions)
        {
            for (int index = 0; index <= depth; ++index)
            {
                output << "  ";
            }
            output << "conversion " << ToString(conversion.kind)
                   << ' ' << conversion.sourceType->displayName()
                   << " -> " << conversion.destinationType->displayName();
            if (conversion.role == ConversionRole::Operand)
            {
                output << " operand=" << conversion.operandIndex;
            }
            else if (conversion.role == ConversionRole::Result)
            {
                output << " result";
            }
            else
            {
                output << " contextual";
            }
            output << '\n';
        }
    }

    dumpNode(node->first(), model, output, depth + 1, "first: ");
    dumpNode(node->second(), model, output, depth + 1, "second: ");
    dumpNode(node->third(), model, output, depth + 1, "third: ");
    for (const ast::AstNode* child : node->children())
    {
        dumpNode(child, model, output, depth + 1, "child: ");
    }
}

void SemanticDumper::dumpConstant(const ConstantValue& value, std::ostream& output)
{
    switch (value.kind())
    {
        case ConstantKind::SignedInteger:
            output << value.signedInteger();
            break;
        case ConstantKind::UnsignedInteger:
            output << value.unsignedInteger();
            break;
        case ConstantKind::Floating:
            output << value.floating();
            break;
        case ConstantKind::Boolean:
            output << (value.boolean() ? "true" : "false");
            break;
        case ConstantKind::Null:
            output << "null";
            break;
        case ConstantKind::String:
            output << '"' << value.string() << '"';
            break;
        case ConstantKind::None:
            output << "<none>";
            break;
    }
}
}
