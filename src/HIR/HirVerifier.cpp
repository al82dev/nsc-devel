#include "nativesharp/HIR/HirVerifier.h"

#include "nativesharp/HIR/HirNode.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"
#include "nativesharp/Sema/Type.h"
#include "nativesharp/Sema/TypeKind.h"

namespace nativesharp::hir
{
namespace
{
bool isGlobalKind(HirNodeKind kind)
{
    return kind == HirNodeKind::StructDeclaration ||
        kind == HirNodeKind::EnumDeclaration ||
        kind == HirNodeKind::GlobalVariableDeclaration ||
        kind == HirNodeKind::FunctionDeclaration;
}

bool isStatementKind(HirNodeKind kind)
{
    return kind == HirNodeKind::Block ||
        kind == HirNodeKind::LocalVariableDeclaration ||
        kind == HirNodeKind::If ||
        kind == HirNodeKind::While ||
        kind == HirNodeKind::For ||
        kind == HirNodeKind::Break ||
        kind == HirNodeKind::Continue ||
        kind == HirNodeKind::Return ||
        kind == HirNodeKind::ExpressionStatement;
}

bool isExpressionKind(HirNodeKind kind)
{
    return kind == HirNodeKind::Constant ||
        kind == HirNodeKind::SymbolAddress ||
        kind == HirNodeKind::Load ||
        kind == HirNodeKind::Store ||
        kind == HirNodeKind::Conversion ||
        kind == HirNodeKind::Unary ||
        kind == HirNodeKind::Binary ||
        kind == HirNodeKind::Conditional ||
        kind == HirNodeKind::Call ||
        kind == HirNodeKind::IndexAddress ||
        kind == HirNodeKind::FieldAddress ||
        kind == HirNodeKind::DereferenceAddress ||
        kind == HirNodeKind::ExtractField ||
        kind == HirNodeKind::CompoundAssignment ||
        kind == HirNodeKind::Increment ||
        kind == HirNodeKind::Allocation ||
        kind == HirNodeKind::Deallocation ||
        kind == HirNodeKind::StackAllocation;
}

bool hasNoEdges(const HirNode* node)
{
    return node->first() == nullptr && node->second() == nullptr && node->third() == nullptr &&
        node->children().empty();
}

bool isRuntimeStorageSymbol(sema::SymbolKind kind)
{
    return kind == sema::SymbolKind::Variable ||
        kind == sema::SymbolKind::GlobalVariable ||
        kind == sema::SymbolKind::Parameter;
}

bool isIntegerType(const sema::Type* type)
{
    if (type == nullptr)
    {
        return false;
    }
    switch (type->kind())
    {
        case sema::TypeKind::Byte:
        case sema::TypeKind::Char:
        case sema::TypeKind::Short:
        case sema::TypeKind::UShort:
        case sema::TypeKind::Int:
        case sema::TypeKind::UInt:
        case sema::TypeKind::Long:
        case sema::TypeKind::ULong:
        case sema::TypeKind::NInt:
        case sema::TypeKind::NUInt:
            return true;
        default:
            return false;
    }
}
}

bool HirVerifier::verify(const HirNode* root)
{
    error_.clear();
    if (root == nullptr)
    {
        error_ = "HIR root is null";
        return false;
    }
    if (root->kind() != HirNodeKind::CompilationUnit)
    {
        error_ = "HIR root must be CompilationUnit";
        return false;
    }
    return verifyNode(root, nullptr, 0);
}

const std::string& HirVerifier::error() const noexcept
{
    return error_;
}

bool HirVerifier::verifyNode(const HirNode* node, sema::Type* functionReturnType, int loopDepth)
{
    if (node == nullptr)
    {
        error_ = "HIR contains a null node";
        return false;
    }

    switch (node->kind())
    {
        case HirNodeKind::CompilationUnit:
            for (const HirNode* child : node->children())
            {
                if (child == nullptr || !isGlobalKind(child->kind()))
                {
                    return fail(node, "CompilationUnit may contain only global declarations");
                }
            }
            return verifyChildren(node, nullptr, 0);

        case HirNodeKind::StructDeclaration:
            if (node->symbol() == nullptr || node->symbol()->kind() != sema::SymbolKind::Struct ||
                node->type() != node->symbol()->type())
            {
                return fail(node, "struct declaration must reference its resolved struct symbol and type");
            }
            for (const HirNode* child : node->children())
            {
                if (child == nullptr || child->kind() != HirNodeKind::FieldDeclaration)
                {
                    return fail(node, "StructDeclaration may contain only FieldDeclaration children");
                }
            }
            return verifyChildren(node, functionReturnType, loopDepth);

        case HirNodeKind::FieldDeclaration:
            if (node->symbol() == nullptr || node->symbol()->kind() != sema::SymbolKind::Field ||
                node->type() != node->symbol()->type() || !node->hasOffset() || !hasNoEdges(node))
            {
                return fail(node, "field declaration must have resolved field symbol, type, offset and no children");
            }
            return true;

        case HirNodeKind::EnumDeclaration:
            if (node->symbol() == nullptr || node->symbol()->kind() != sema::SymbolKind::Enum ||
                node->type() != node->symbol()->type())
            {
                return fail(node, "enum declaration must reference its resolved enum symbol and type");
            }
            for (const HirNode* child : node->children())
            {
                if (child == nullptr || child->kind() != HirNodeKind::EnumMemberDeclaration)
                {
                    return fail(node, "EnumDeclaration may contain only EnumMemberDeclaration children");
                }
            }
            return verifyChildren(node, functionReturnType, loopDepth);

        case HirNodeKind::EnumMemberDeclaration:
            if (node->symbol() == nullptr || node->symbol()->kind() != sema::SymbolKind::EnumMember ||
                node->type() != node->symbol()->type() || !node->hasConstant() || !hasNoEdges(node))
            {
                return fail(node, "enum member must have resolved symbol, enum type, constant and no children");
            }
            return true;

        case HirNodeKind::GlobalVariableDeclaration:
        case HirNodeKind::LocalVariableDeclaration:
        {
            const sema::SymbolKind expected = node->kind() == HirNodeKind::GlobalVariableDeclaration
                ? sema::SymbolKind::GlobalVariable
                : sema::SymbolKind::Variable;
            if (node->symbol() == nullptr || node->symbol()->kind() != expected ||
                node->type() != node->symbol()->type())
            {
                return fail(node, "variable declaration must reference the exact resolved symbol and type");
            }
            if (node->first() != nullptr)
            {
                if (!isExpressionKind(node->first()->kind()) ||
                    !verifyNode(node->first(), functionReturnType, loopDepth) || node->first()->type() != node->type())
                {
                    return fail(node, "variable initializer must be an expression of the declaration type");
                }
            }
            return true;
        }

        case HirNodeKind::FunctionDeclaration:
        {
            if (node->symbol() == nullptr || node->symbol()->kind() != sema::SymbolKind::Function ||
                node->type() == nullptr || node->type()->kind() != sema::TypeKind::Function ||
                node->type() != node->symbol()->type())
            {
                return fail(node, "function declaration must reference its resolved function symbol and type");
            }
            const std::vector<sema::Type*>& parameters = node->type()->parameterTypes();
            if (parameters.size() != node->children().size())
            {
                return fail(node, "HIR function parameter count does not match its semantic function type");
            }
            for (std::size_t index = 0; index < node->children().size(); ++index)
            {
                const HirNode* parameter = node->children()[index];
                if (parameter == nullptr || parameter->kind() != HirNodeKind::ParameterDeclaration ||
                    parameter->type() != parameters[index])
                {
                    return fail(node, "HIR function parameter does not match semantic parameter type");
                }
                if (node->first() != nullptr &&
                    (parameter->symbol() == nullptr ||
                     parameter->symbol()->kind() != sema::SymbolKind::Parameter))
                {
                    return fail(node, "defined HIR function parameters must be bound runtime symbols");
                }
            }
            if (node->first() != nullptr)
            {
                if (node->first()->kind() != HirNodeKind::Block ||
                    !verifyNode(node->first(), node->type()->returnType(), 0))
                {
                    return fail(node, "function body must be a valid HIR block");
                }
            }
            return true;
        }

        case HirNodeKind::ParameterDeclaration:
            if (node->type() == nullptr)
            {
                return fail(node, "parameter declaration requires a semantic type");
            }
            if (node->symbol() != nullptr &&
                (node->symbol()->kind() != sema::SymbolKind::Parameter ||
                 node->type() != node->symbol()->type()))
            {
                return fail(node, "bound parameter declaration must match its runtime parameter symbol");
            }
            return hasNoEdges(node)
                ? true
                : fail(node, "parameter declaration cannot contain HIR child nodes");

        case HirNodeKind::Block:
            for (const HirNode* child : node->children())
            {
                if (child == nullptr || !isStatementKind(child->kind()))
                {
                    return fail(node, "Block may contain only HIR statements");
                }
            }
            return verifyChildren(node, functionReturnType, loopDepth);

        case HirNodeKind::If:
            if (node->first() == nullptr || !isExpressionKind(node->first()->kind()) ||
                node->first()->type() == nullptr || node->first()->type()->kind() != sema::TypeKind::Bool ||
                node->second() == nullptr || node->second()->kind() != HirNodeKind::Block)
            {
                return fail(node, "if requires bool condition and block body");
            }
            if (!verifyNode(node->first(), functionReturnType, loopDepth) ||
                !verifyNode(node->second(), functionReturnType, loopDepth))
            {
                return false;
            }
            if (node->third() != nullptr &&
                node->third()->kind() != HirNodeKind::Block && node->third()->kind() != HirNodeKind::If)
            {
                return fail(node, "else branch must be a HIR Block or If");
            }
            if (node->third() != nullptr && !verifyNode(node->third(), functionReturnType, loopDepth))
            {
                return false;
            }
            return true;

        case HirNodeKind::While:
            if (node->first() == nullptr || !isExpressionKind(node->first()->kind()) ||
                node->first()->type() == nullptr || node->first()->type()->kind() != sema::TypeKind::Bool ||
                node->second() == nullptr || node->second()->kind() != HirNodeKind::Block)
            {
                return fail(node, "while requires bool condition and block body");
            }
            return verifyNode(node->first(), functionReturnType, loopDepth) &&
                verifyNode(node->second(), functionReturnType, loopDepth + 1);

        case HirNodeKind::For:
            if (node->first() != nullptr && node->first()->kind() != HirNodeKind::LocalVariableDeclaration &&
                !isExpressionKind(node->first()->kind()))
            {
                return fail(node, "for initializer must be a local declaration or HIR expression");
            }
            if (node->second() != nullptr &&
                (!isExpressionKind(node->second()->kind()) || node->second()->type() == nullptr ||
                 node->second()->type()->kind() != sema::TypeKind::Bool))
            {
                return fail(node, "for condition must be a bool HIR expression when present");
            }
            if (node->third() != nullptr && !isExpressionKind(node->third()->kind()))
            {
                return fail(node, "for increment must be a HIR expression when present");
            }
            if (node->children().size() != 1 || node->children().front() == nullptr ||
                node->children().front()->kind() != HirNodeKind::Block)
            {
                return fail(node, "for must contain exactly one HIR block body");
            }
            if (node->first() != nullptr && !verifyNode(node->first(), functionReturnType, loopDepth)) return false;
            if (node->second() != nullptr && !verifyNode(node->second(), functionReturnType, loopDepth)) return false;
            if (node->third() != nullptr && !verifyNode(node->third(), functionReturnType, loopDepth))
            {
                return false;
            }
            return verifyNode(node->children().front(), functionReturnType, loopDepth + 1);

        case HirNodeKind::Break:
        case HirNodeKind::Continue:
            if (!hasNoEdges(node))
            {
                return fail(node, "break/continue cannot contain HIR child nodes");
            }
            return loopDepth > 0
                ? true
                : fail(node, "break/continue must be nested inside a HIR loop");

        case HirNodeKind::Return:
            if (functionReturnType == nullptr)
            {
                return fail(node, "return appears outside a HIR function body");
            }
            if (functionReturnType->kind() == sema::TypeKind::Void)
            {
                return node->first() == nullptr
                    ? true
                    : fail(node, "void HIR function cannot return a value");
            }
            if (node->first() == nullptr || !isExpressionKind(node->first()->kind()) ||
                !verifyNode(node->first(), functionReturnType, loopDepth) ||
                node->first()->type() != functionReturnType || node->type() != functionReturnType)
            {
                return fail(node, "HIR return value must exactly match function return type");
            }
            return true;

        case HirNodeKind::ExpressionStatement:
            return node->first() != nullptr && isExpressionKind(node->first()->kind()) &&
                    verifyNode(node->first(), functionReturnType, loopDepth)
                ? true
                : fail(node, "expression statement must contain a valid HIR expression");

        case HirNodeKind::Constant:
            return node->type() != nullptr && node->hasConstant() && hasNoEdges(node)
                ? true
                : fail(node, "constant must have semantic type, value and no child nodes");

        case HirNodeKind::SymbolAddress:
            if (node->symbol() == nullptr || !isRuntimeStorageSymbol(node->symbol()->kind()) ||
                node->type() == nullptr || node->type()->kind() != sema::TypeKind::Pointer ||
                node->type()->elementType() != node->symbol()->type() || !hasNoEdges(node))
            {
                return fail(node, "symbol address must reference runtime storage with pointer-to-symbol type");
            }
            return true;

        case HirNodeKind::Load:
            if (node->first() == nullptr || node->type() == nullptr ||
                !verifyPointerTo(node->first(), node->type()))
            {
                return fail(node, "load requires pointer-to-result-type address");
            }
            return verifyNode(node->first(), functionReturnType, loopDepth);

        case HirNodeKind::Store:
            if (node->first() == nullptr || node->second() == nullptr || node->type() == nullptr ||
                !verifyPointerTo(node->first(), node->type()) || node->second()->type() != node->type())
            {
                return fail(node, "store requires pointer-to-T destination and T value");
            }
            return verifyNode(node->first(), functionReturnType, loopDepth) &&
                verifyNode(node->second(), functionReturnType, loopDepth);

        case HirNodeKind::Conversion:
            if (node->first() == nullptr || node->type() == nullptr || !node->hasConversion())
            {
                return fail(node, "conversion requires source value, destination type and conversion kind");
            }
            return verifyNode(node->first(), functionReturnType, loopDepth);

        case HirNodeKind::Unary:
            if (node->first() == nullptr || node->type() == nullptr || node->operationType() == nullptr ||
                node->operation() == lexer::TokenKind::Invalid)
            {
                return fail(node, "unary operation is missing resolved semantic information");
            }
            return verifyNode(node->first(), functionReturnType, loopDepth);

        case HirNodeKind::Binary:
            if (node->first() == nullptr || node->second() == nullptr || node->type() == nullptr ||
                node->operationType() == nullptr || node->operation() == lexer::TokenKind::Invalid)
            {
                return fail(node, "binary operation is missing resolved semantic information");
            }
            return verifyNode(node->first(), functionReturnType, loopDepth) &&
                verifyNode(node->second(), functionReturnType, loopDepth);

        case HirNodeKind::Conditional:
            if (node->first() == nullptr || node->second() == nullptr || node->third() == nullptr ||
                node->type() == nullptr || !isExpressionKind(node->first()->kind()) ||
                !isExpressionKind(node->second()->kind()) || !isExpressionKind(node->third()->kind()) ||
                node->first()->type() == nullptr || node->first()->type()->kind() != sema::TypeKind::Bool ||
                node->second()->type() != node->type() || node->third()->type() != node->type())
            {
                return fail(node, "conditional requires bool condition and two branches of its result type");
            }
            return verifyNode(node->first(), functionReturnType, loopDepth) &&
                verifyNode(node->second(), functionReturnType, loopDepth) &&
                verifyNode(node->third(), functionReturnType, loopDepth);

        case HirNodeKind::Call:
        {
            if (node->symbol() == nullptr || node->symbol()->kind() != sema::SymbolKind::Function ||
                node->symbol()->type()->kind() != sema::TypeKind::Function ||
                node->type() != node->symbol()->type()->returnType())
            {
                return fail(node, "call must target a resolved function and have its return type");
            }
            const std::vector<sema::Type*>& parameters = node->symbol()->type()->parameterTypes();
            if (parameters.size() != node->children().size())
            {
                return fail(node, "call argument count does not match resolved function type");
            }
            for (std::size_t index = 0; index < node->children().size(); ++index)
            {
                if (node->children()[index] == nullptr ||
                    !isExpressionKind(node->children()[index]->kind()) ||
                    node->children()[index]->type() != parameters[index] ||
                    !verifyNode(node->children()[index], functionReturnType, loopDepth))
                {
                    return fail(node, "call argument type does not match resolved parameter type");
                }
            }
            return true;
        }

        case HirNodeKind::IndexAddress:
        {
            if (node->first() == nullptr || node->second() == nullptr ||
                !isExpressionKind(node->first()->kind()) || !isExpressionKind(node->second()->kind()) ||
                node->type() == nullptr || node->type()->kind() != sema::TypeKind::Pointer ||
                node->operationType() == nullptr || node->second()->type() == nullptr ||
                !isIntegerType(node->second()->type()))
            {
                return fail(node, "index address requires base, integer index, base type and pointer result");
            }

            const sema::Type* baseType = node->operationType();
            bool relationIsValid = false;
            if (baseType->kind() == sema::TypeKind::Array)
            {
                relationIsValid = verifyPointerTo(node->first(), baseType) &&
                    node->type()->elementType() == baseType->elementType();
            }
            else if (baseType->kind() == sema::TypeKind::Pointer)
            {
                relationIsValid = node->first()->type() == baseType && node->type() == baseType;
            }
            else if (baseType->kind() == sema::TypeKind::StringLiteral)
            {
                relationIsValid = node->first()->type() == baseType &&
                    node->type()->elementType() == baseType->elementType();
            }

            if (!relationIsValid)
            {
                return fail(node, "index address base/result types do not match resolved array or pointer type");
            }
            return verifyNode(node->first(), functionReturnType, loopDepth) &&
                verifyNode(node->second(), functionReturnType, loopDepth);
        }

        case HirNodeKind::FieldAddress:
            if (node->first() == nullptr || node->symbol() == nullptr ||
                node->symbol()->kind() != sema::SymbolKind::Field || !node->hasOffset() ||
                !verifyPointerTo(node, node->symbol()->type()))
            {
                return fail(node, "field address requires resolved field, offset and pointer-to-field type");
            }
            return verifyNode(node->first(), functionReturnType, loopDepth);

        case HirNodeKind::DereferenceAddress:
            if (node->first() == nullptr || node->type() == nullptr ||
                node->type()->kind() != sema::TypeKind::Pointer || node->first()->type() != node->type())
            {
                return fail(node, "dereference address must preserve the evaluated pointer type");
            }
            return verifyNode(node->first(), functionReturnType, loopDepth);

        case HirNodeKind::ExtractField:
            if (node->first() == nullptr || !isExpressionKind(node->first()->kind()) ||
                node->first()->type() == nullptr || node->first()->type()->kind() != sema::TypeKind::Struct ||
                node->symbol() == nullptr || node->symbol()->kind() != sema::SymbolKind::Field ||
                node->type() != node->symbol()->type() || !node->hasOffset())
            {
                return fail(node, "field extraction requires struct value, resolved field and offset");
            }
            return verifyNode(node->first(), functionReturnType, loopDepth);

        case HirNodeKind::CompoundAssignment:
            if (node->first() == nullptr || node->second() == nullptr || node->type() == nullptr ||
                node->operationType() == nullptr || node->operation() == lexer::TokenKind::Invalid ||
                !verifyPointerTo(node->first(), node->type()))
            {
                return fail(node, "compound assignment is missing address or resolved operation information");
            }
            return verifyNode(node->first(), functionReturnType, loopDepth) &&
                verifyNode(node->second(), functionReturnType, loopDepth);

        case HirNodeKind::Increment:
            if (node->first() == nullptr || node->type() == nullptr || node->operationType() == nullptr ||
                (node->operation() != lexer::TokenKind::PlusPlus &&
                 node->operation() != lexer::TokenKind::MinusMinus) ||
                !verifyPointerTo(node->first(), node->type()))
            {
                return fail(node, "increment/decrement requires ++/-- and pointer-to-value storage address");
            }
            return verifyNode(node->first(), functionReturnType, loopDepth);

        case HirNodeKind::Allocation:
            if (node->type() == nullptr || node->type()->kind() != sema::TypeKind::Pointer ||
                node->operationType() != node->type()->elementType() ||
                (node->arrayForm() && (node->first() == nullptr || node->first()->type() == nullptr ||
                                       !isIntegerType(node->first()->type()))) ||
                (!node->arrayForm() && node->first() != nullptr))
            {
                return fail(node, "heap allocation must identify element type and array/scalar form");
            }
            return node->first() == nullptr || verifyNode(node->first(), functionReturnType, loopDepth);

        case HirNodeKind::Deallocation:
            if (node->type() == nullptr || node->type()->kind() != sema::TypeKind::Void ||
                node->first() == nullptr || node->first()->type() == nullptr ||
                (node->first()->type()->kind() != sema::TypeKind::Pointer &&
                 node->first()->type()->kind() != sema::TypeKind::NullLiteral))
            {
                return fail(node, "deallocation requires void result and pointer or null operand");
            }
            return verifyNode(node->first(), functionReturnType, loopDepth);

        case HirNodeKind::StackAllocation:
            if (node->type() == nullptr || node->type()->kind() != sema::TypeKind::Pointer ||
                node->operationType() != node->type()->elementType() || node->first() == nullptr ||
                node->first()->type() == nullptr || !isIntegerType(node->first()->type()))
            {
                return fail(node, "stack allocation must identify element type and count expression");
            }
            return verifyNode(node->first(), functionReturnType, loopDepth);
    }

    return fail(node, "unhandled HIR node kind in verifier");
}

bool HirVerifier::verifyChildren(const HirNode* node, sema::Type* functionReturnType, int loopDepth)
{
    for (const HirNode* child : node->children())
    {
        if (child == nullptr || !verifyNode(child, functionReturnType, loopDepth))
        {
            return false;
        }
    }
    return true;
}

bool HirVerifier::verifyPointerTo(const HirNode* address, const sema::Type* pointee)
{
    return address != nullptr && address->type() != nullptr &&
        address->type()->kind() == sema::TypeKind::Pointer &&
        address->type()->elementType() == pointee;
}

bool HirVerifier::fail(const HirNode* node, std::string message)
{
    if (error_.empty())
    {
        error_ = std::string(hirNodeKindName(node->kind())) + ": " + message;
    }
    return false;
}
}
