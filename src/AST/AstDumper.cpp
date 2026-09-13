#include "nativesharp/AST/AstDumper.h"

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <ostream>

namespace nativesharp::ast
{
void AstDumper::dump(const AstNode* node, std::ostream& output) const
{
    dumpNode(node, output, 0, "");
}

void AstDumper::dumpNode(const AstNode* node, std::ostream& output, int depth, const char* edge) const
{
    if (node == nullptr)
    {
        return;
    }

    for (int index = 0; index < depth; ++index)
    {
        output << "  ";
    }

    output << edge << astNodeKindName(node->kind());
    if (!node->text().empty())
    {
        output << " \"" << node->text() << '"';
    }
    if (!node->auxiliaryText().empty())
    {
        output << " aux=\"" << node->auxiliaryText() << '"';
    }
    if (!node->genericParameters().empty())
    {
        output << " generic=<";
        for (std::size_t index = 0; index < node->genericParameters().size(); ++index)
        {
            if (index != 0) output << ',';
            output << node->genericParameters()[index];
        }
        output << '>';
    }

    // NSC-LIMIT-007: `where T : class, IFoo` se vuelca junto a los parametros, porque es parte
    // de la firma generica y no de los hijos. Sin esto, una clausula perdida en el parser no se
    // veria en ningun volcado.
    for (const GenericConstraintClause& clause : node->genericConstraints())
    {
        output << " where " << clause.parameterName << ':';
        bool first = true;
        if (clause.requiresClass)
        {
            output << "class";
            first = false;
        }
        for (const TypeSyntax* constraint : clause.typeConstraints)
        {
            if (!first) output << ',';
            first = false;
            dumpType(constraint, output);
        }
    }

    if (node->tokenKind() != lexer::TokenKind::Invalid)
    {
        output << " op=" << lexer::tokenKindName(node->tokenKind());
    }

    if (node->typeSyntax() != nullptr)
    {
        output << " type=";
        dumpType(node->typeSyntax(), output);
    }

    if (node->flag())
    {
        output << " flag=true";
    }

    output << '\n';

    dumpNode(node->first(), output, depth + 1, "first: ");
    dumpNode(node->second(), output, depth + 1, "second: ");
    dumpNode(node->third(), output, depth + 1, "third: ");

    for (AstNode* child : node->children())
    {
        dumpNode(child, output, depth + 1, "child: ");
    }
}

void AstDumper::dumpType(const TypeSyntax* typeSyntax, std::ostream& output) const
{
    if (typeSyntax->ownershipQualifier() != OwnershipQualifierSyntaxKind::None)
    {
        output << ownershipQualifierSyntaxKindName(typeSyntax->ownershipQualifier()) << ' ';
    }
    output << typeSyntax->baseName();
    if (!typeSyntax->typeArguments().empty())
    {
        output << '<';
        for (std::size_t index = 0; index < typeSyntax->typeArguments().size(); ++index)
        {
            if (index != 0) output << ',';
            dumpType(typeSyntax->typeArguments()[index], output);
        }
        output << '>';
    }
    for (const TypeModifierSyntax& modifier : typeSyntax->modifiers())
    {
        if (modifier.kind == TypeModifierSyntaxKind::Pointer)
        {
            output << '*';
        }
        else if (modifier.kind == TypeModifierSyntaxKind::DynamicArray)
        {
            output << "[]";
        }
        else if (modifier.kind == TypeModifierSyntaxKind::FlexibleArray)
        {
            output << "[*]";
        }
        else
        {
            output << "[expr]";
        }
    }
}
}
