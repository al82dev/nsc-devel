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
    if (!node->argumentName().empty())
    {
        output << " name=\"" << node->argumentName() << '"';
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
    if (!node->lifetimeParameters().empty())
    {
        output << " lifetime=<";
        for (std::size_t index = 0; index < node->lifetimeParameters().size(); ++index)
        {
            if (index != 0) output << ',';
            output << node->lifetimeParameters()[index].name;
        }
        output << '>';
    }
    for (const LifetimeOutlivesConstraint& constraint : node->lifetimeOutlivesConstraints())
        output << " outlives=" << constraint.longer << ':' << constraint.shorter;
    if (!node->lifetimeName().empty()) output << " lifetime-name=" << node->lifetimeName();

    // NSC-LIMIT-007: `where T : class, IFoo` se vuelca junto a los parametros, porque es parte
    // de la firma generica y no de los hijos. Sin esto, una clausula perdida en el parser no se
    // veria en ningun volcado.
    for (const GenericConstraintClause& clause : node->genericConstraints())
    {
        output << " where " << clause.parameterName;
        if (clause.equalityConstraint != nullptr)
        {
            output << "==";
            dumpType(clause.equalityConstraint, output);
            continue;
        }
        output << ':';
        bool first = true;
        const auto capability = [&](bool enabled, std::string_view spelling)
        {
            if (!enabled) return;
            if (!first) output << ',';
            output << spelling;
            first = false;
        };
        capability(clause.requiresClass, "class");
        capability(clause.requiresValueType, "struct");
        capability(clause.requiresDefaultConstructor, "new()");
        capability(clause.requiresCopy, "copy");
        capability(clause.requiresMove, "move");
        capability(clause.requiresTrackedLifetime, "lifetime");
        capability(clause.requiresUnmanaged, "unmanaged");
        capability(clause.requiresNotNull, "notnull");
        capability(clause.requiresRefLike, "ref-like");
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
    if (node->throwsTypeSyntax() != nullptr)
    {
        output << " throws=";
        dumpType(node->throwsTypeSyntax(), output);
    }

    if (node->flag())
    {
        output << " flag=true";
    }
    if (node->isReference())
    {
        output << " ref=true";
    }
    if (node->isVariadic())
    {
        output << " variadic=true";
    }
    switch (node->floatingPointMode())
    {
        case FloatingPointModeSyntaxKind::Inherit: break;
        case FloatingPointModeSyntaxKind::Strict: output << " fp=strict"; break;
        case FloatingPointModeSyntaxKind::Precise: output << " fp=precise"; break;
        case FloatingPointModeSyntaxKind::Fast: output << " fp=fast"; break;
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
    if (typeSyntax == nullptr)
    {
        output << "<missing>";
        return;
    }

    if (typeSyntax->ownershipQualifier() != OwnershipQualifierSyntaxKind::None)
    {
        output << ownershipQualifierSyntaxKindName(typeSyntax->ownershipQualifier());
        if (!typeSyntax->lifetimeName().empty()) output << '[' << typeSyntax->lifetimeName() << ']';
        output << ' ';
    }
    if (typeSyntax->isDynamicTrait()) output << "dyn ";
    if (typeSyntax->isFunction())
    {
        output << "fn" << (typeSyntax->functionCarrier() ==
            FunctionTypeCarrierSyntaxKind::Pointer ? '*' : '&');
        if (typeSyntax->functionIsNullable()) output << '?';
        if (!typeSyntax->functionLifetimeName().empty())
            output << '[' << typeSyntax->functionLifetimeName() << ']';
        switch (typeSyntax->functionCallingConvention())
        {
            case FunctionCallingConventionSyntaxKind::NativeSharp: output << " native"; break;
            case FunctionCallingConventionSyntaxKind::C: output << " C"; break;
            case FunctionCallingConventionSyntaxKind::StdCall: output << " stdcall"; break;
            case FunctionCallingConventionSyntaxKind::FastCall: output << " fastcall"; break;
            case FunctionCallingConventionSyntaxKind::VectorCall: output << " vectorcall"; break;
            case FunctionCallingConventionSyntaxKind::Win64: output << " win64"; break;
        }
        output << '(';
        for (std::size_t index = 0; index < typeSyntax->typeArguments().size(); ++index)
        {
            if (index != 0) output << ',';
            dumpType(typeSyntax->typeArguments()[index], output);
        }
        if (typeSyntax->functionIsVariadic())
            output << (typeSyntax->typeArguments().empty() ? "..." : ",...");
        output << ")->";
        if (typeSyntax->functionReturnType() != nullptr)
            dumpType(typeSyntax->functionReturnType(), output);
        else output << "<missing>";
        if (typeSyntax->functionIsUnsafe()) output << " unsafe";
        if (typeSyntax->functionIsNoHeap()) output << " noheap";
        if (typeSyntax->functionIsAsync()) output << " async";
        if (typeSyntax->functionErrorType() != nullptr)
        {
            output << " throws ";
            dumpType(typeSyntax->functionErrorType(), output);
        }
    }
    else if (typeSyntax->isTuple())
    {
        output << '(';
        for (std::size_t index = 0; index < typeSyntax->typeArguments().size(); ++index)
        {
            if (index != 0) output << ',';
            dumpType(typeSyntax->typeArguments()[index], output);
            if (index < typeSyntax->tupleElementNames().size() &&
                !typeSyntax->tupleElementNames()[index].empty())
                output << ' ' << typeSyntax->tupleElementNames()[index];
        }
        output << ')';
    }
    else output << typeSyntax->baseName();
    if (!typeSyntax->isFunction() && !typeSyntax->isTuple() &&
        !typeSyntax->typeArguments().empty())
    {
        output << '<';
        for (std::size_t index = 0; index < typeSyntax->typeArguments().size(); ++index)
        {
            if (index != 0) output << ',';
            dumpType(typeSyntax->typeArguments()[index], output);
        }
        output << '>';
    }
    if (typeSyntax->isPointeeConst()) output << " const";
    for (const TypeModifierSyntax& modifier : typeSyntax->modifiers())
    {
        if (modifier.kind == TypeModifierSyntaxKind::Pointer)
        {
            output << '*';
            if (modifier.pointerNullability == PointerNullabilitySyntaxKind::Nullable)
                output << '?';
            else if (modifier.pointerNullability == PointerNullabilitySyntaxKind::NonNull)
                output << '!';
            if (modifier.pointerKnownAlignment != 1)
                output << " align(" << modifier.pointerKnownAlignment << ')';
            if (modifier.pointerAddressSpace != 0)
                output << " addrspace(" << modifier.pointerAddressSpace << ')';
            if (!modifier.pointerHasProvenance)
                output << " provenance(none)";
        }
        else if (modifier.kind == TypeModifierSyntaxKind::DynamicArray)
        {
            output << "[]";
        }
        else if (modifier.kind == TypeModifierSyntaxKind::RangeArray)
        {
            output << '[' << std::string(
                modifier.arrayRank > 0 ? modifier.arrayRank - 1 : 0, ',') << ']';
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
