#include "nativesharp/HIR/HirVerifier.h"

#include "Internal/VerificationTask.h"

#include "nativesharp/HIR/HirNode.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"
#include "nativesharp/Sema/Type.h"
#include "nativesharp/Sema/TypeKind.h"

#include <cstddef>
#include <vector>

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

using internal::VerificationTask;

// Appends children in reverse so the explicit LIFO worklist preserves the recursive left-to-right DFS order.
void appendChildren(
    std::vector<VerificationTask>& tasks,
    const HirNode* node,
    sema::Type* functionReturnType,
    int loopDepth)
{
    for (auto child = node->children().rbegin(); child != node->children().rend(); ++child)
    {
        tasks.push_back({VerificationTask::Kind::Node, *child, functionReturnType, loopDepth, 0});
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
    std::vector<VerificationTask> tasks;
    tasks.push_back({VerificationTask::Kind::Node, node, functionReturnType, loopDepth, 0});

    while (!tasks.empty())
    {
        const VerificationTask task = tasks.back();
        tasks.pop_back();
        node = task.node;
        functionReturnType = task.functionReturnType;
        loopDepth = task.loopDepth;

        if (task.kind == VerificationTask::Kind::VariableInitializerType)
        {
            if (node->first()->type() != node->type())
            {
                return fail(node, "variable initializer must be an expression of the declaration type");
            }
            continue;
        }
        if (task.kind == VerificationTask::Kind::IfElseBranch)
        {
            if (node->third() != nullptr &&
                node->third()->kind() != HirNodeKind::Block && node->third()->kind() != HirNodeKind::If)
            {
                return fail(node, "else branch must be a HIR Block or If");
            }
            if (node->third() != nullptr)
            {
                tasks.push_back({VerificationTask::Kind::Node, node->third(), functionReturnType, loopDepth, 0});
            }
            continue;
        }
        if (task.kind == VerificationTask::Kind::ReturnType)
        {
            if (node->first()->type() != functionReturnType || node->type() != functionReturnType)
            {
                return fail(node, "HIR return value must exactly match function return type");
            }
            continue;
        }
        if (task.kind == VerificationTask::Kind::CallArgument)
        {
            if (task.childIndex >= node->children().size())
            {
                continue;
            }
            const HirNode* argument = node->children()[task.childIndex];
            const std::vector<sema::Type*>& parameters = node->symbol()->type()->parameterTypes();
            if (argument == nullptr || !isExpressionKind(argument->kind()) ||
                argument->type() != parameters[task.childIndex])
            {
                return fail(node, "call argument type does not match resolved parameter type");
            }
            tasks.push_back({VerificationTask::Kind::CallArgument, node, functionReturnType,
                loopDepth, task.childIndex + 1});
            tasks.push_back({VerificationTask::Kind::Node, argument, functionReturnType, loopDepth, 0});
            continue;
        }

        if (node == nullptr)
        {
            error_ = "HIR contains a null node";
            return false;
        }

        if (!verifyNodeKind(node, tasks, functionReturnType, loopDepth))
        {
            return false;
        }
    }

    return true;
}

bool HirVerifier::verifyNodeKind(
    const HirNode* node,
    std::vector<VerificationTask>& tasks,
    sema::Type* functionReturnType,
    int loopDepth)
{
    switch (node->kind())
    {
        case HirNodeKind::CompilationUnit:
        case HirNodeKind::StructDeclaration:
        case HirNodeKind::FieldDeclaration:
        case HirNodeKind::EnumDeclaration:
        case HirNodeKind::EnumMemberDeclaration:
            return verifyGlobalDeclarationNode(node, tasks, functionReturnType, loopDepth);
        case HirNodeKind::GlobalVariableDeclaration:
        case HirNodeKind::LocalVariableDeclaration:
        case HirNodeKind::FunctionDeclaration:
        case HirNodeKind::ParameterDeclaration:
            return verifyCallableDeclarationNode(node, tasks, functionReturnType, loopDepth);
        case HirNodeKind::Block:
        case HirNodeKind::If:
        case HirNodeKind::While:
        case HirNodeKind::For:
            return verifyControlFlowNode(node, tasks, functionReturnType, loopDepth);
        case HirNodeKind::Break:
        case HirNodeKind::Continue:
        case HirNodeKind::Return:
        case HirNodeKind::ExpressionStatement:
            return verifyExitOrStatementNode(node, tasks, functionReturnType, loopDepth);
        case HirNodeKind::Constant:
        case HirNodeKind::SymbolAddress:
        case HirNodeKind::Load:
        case HirNodeKind::Store:
        case HirNodeKind::Conversion:
        case HirNodeKind::Unary:
        case HirNodeKind::Binary:
        case HirNodeKind::Conditional:
            return verifyValueNode(node, tasks, functionReturnType, loopDepth);
        case HirNodeKind::Call:
        case HirNodeKind::IndexAddress:
        case HirNodeKind::FieldAddress:
        case HirNodeKind::DereferenceAddress:
        case HirNodeKind::ExtractField:
            return verifyAccessNode(node, tasks, functionReturnType, loopDepth);
        case HirNodeKind::CompoundAssignment:
        case HirNodeKind::Increment:
        case HirNodeKind::Allocation:
        case HirNodeKind::Deallocation:
        case HirNodeKind::StackAllocation:
            return verifyMutationOrAllocationNode(node, tasks, functionReturnType, loopDepth);
        default:
            return fail(node, "unhandled HIR node kind in verifier");
    }
}

bool HirVerifier::verifyGlobalDeclarationNode(
    const HirNode* node,
    std::vector<VerificationTask>& tasks,
    sema::Type* functionReturnType,
    int loopDepth)
{
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
                appendChildren(tasks, node, nullptr, 0);
                return true;

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
                appendChildren(tasks, node, functionReturnType, loopDepth);
                return true;

            case HirNodeKind::FieldDeclaration:
                if (node->symbol() == nullptr || node->symbol()->kind() != sema::SymbolKind::Field ||
                    node->type() != node->symbol()->type() || !node->hasOffset() || !hasNoEdges(node))
                {
                    return fail(node,
                        "field declaration must have resolved field symbol, type, offset and no children");
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
                appendChildren(tasks, node, functionReturnType, loopDepth);
                return true;

            case HirNodeKind::EnumMemberDeclaration:
                if (node->symbol() == nullptr || node->symbol()->kind() != sema::SymbolKind::EnumMember ||
                    node->type() != node->symbol()->type() || !node->hasConstant() || !hasNoEdges(node))
                {
                    return fail(node, "enum member must have resolved symbol, enum type, constant and no children");
                }
                return true;
        default:
            return false;
    }
}

bool HirVerifier::verifyCallableDeclarationNode(
    const HirNode* node,
    std::vector<VerificationTask>& tasks,
    sema::Type* functionReturnType,
    int loopDepth)
{
    switch (node->kind())
    {
            case HirNodeKind::GlobalVariableDeclaration:
            case HirNodeKind::LocalVariableDeclaration:
                return verifyVariableDeclarationNode(
                    node, tasks, functionReturnType, loopDepth);

            case HirNodeKind::FunctionDeclaration:
                return verifyFunctionDeclarationNode(node, tasks);

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
                if (!hasNoEdges(node))
                {
                    return fail(node, "parameter declaration cannot contain HIR child nodes");
                }
                return true;
        default:
            return false;
    }
}

bool HirVerifier::verifyVariableDeclarationNode(
    const HirNode* node,
    std::vector<VerificationTask>& tasks,
    sema::Type* functionReturnType,
    int loopDepth)
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
        if (!isExpressionKind(node->first()->kind()))
        {
            return fail(node, "variable initializer must be an expression of the declaration type");
        }
        tasks.push_back({VerificationTask::Kind::VariableInitializerType, node,
            functionReturnType, loopDepth, 0});
        tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
    }
    return true;
}

bool HirVerifier::verifyFunctionDeclarationNode(
    const HirNode* node,
    std::vector<VerificationTask>& tasks)
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
        if (node->first()->kind() != HirNodeKind::Block)
        {
            return fail(node, "function body must be a valid HIR block");
        }
        tasks.push_back({VerificationTask::Kind::Node, node->first(),
            node->type()->returnType(), 0, 0});
    }
    return true;
}

bool HirVerifier::verifyControlFlowNode(
    const HirNode* node,
    std::vector<VerificationTask>& tasks,
    sema::Type* functionReturnType,
    int loopDepth)
{
    switch (node->kind())
    {
            case HirNodeKind::Block:
                for (const HirNode* child : node->children())
                {
                    if (child == nullptr || !isStatementKind(child->kind()))
                    {
                        return fail(node, "Block may contain only HIR statements");
                    }
                }
                appendChildren(tasks, node, functionReturnType, loopDepth);
                return true;

            case HirNodeKind::If:
                if (node->first() == nullptr || !isExpressionKind(node->first()->kind()) ||
                    node->first()->type() == nullptr || node->first()->type()->kind() != sema::TypeKind::Bool ||
                    node->second() == nullptr || node->second()->kind() != HirNodeKind::Block)
                {
                    return fail(node, "if requires bool condition and block body");
                }
                tasks.push_back({VerificationTask::Kind::IfElseBranch, node,
                    functionReturnType, loopDepth, 0});
                tasks.push_back({VerificationTask::Kind::Node, node->second(), functionReturnType, loopDepth, 0});
                tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
                return true;

            case HirNodeKind::While:
                if (node->first() == nullptr || !isExpressionKind(node->first()->kind()) ||
                    node->first()->type() == nullptr || node->first()->type()->kind() != sema::TypeKind::Bool ||
                    node->second() == nullptr || node->second()->kind() != HirNodeKind::Block)
                {
                    return fail(node, "while requires bool condition and block body");
                }
                tasks.push_back({VerificationTask::Kind::Node, node->second(),
                    functionReturnType, loopDepth + 1, 0});
                tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
                return true;

            case HirNodeKind::For:
                if (node->first() != nullptr && node->first()->kind() != HirNodeKind::Block &&
                    node->first()->kind() != HirNodeKind::LocalVariableDeclaration &&
                    !isExpressionKind(node->first()->kind()))
                {
                    return fail(node,
                        "for initializer must be a local declaration, declaration block, or HIR expression");
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
                tasks.push_back({VerificationTask::Kind::Node, node->children().front(),
                    functionReturnType, loopDepth + 1, 0});
                if (node->third() != nullptr)
                {
                    tasks.push_back({VerificationTask::Kind::Node, node->third(),
                        functionReturnType, loopDepth, 0});
                }
                if (node->second() != nullptr)
                {
                    tasks.push_back({VerificationTask::Kind::Node, node->second(),
                        functionReturnType, loopDepth, 0});
                }
                if (node->first() != nullptr)
                {
                    tasks.push_back({VerificationTask::Kind::Node, node->first(),
                        functionReturnType, loopDepth, 0});
                }
                return true;
        default:
            return false;
    }
}

bool HirVerifier::verifyExitOrStatementNode(
    const HirNode* node,
    std::vector<VerificationTask>& tasks,
    sema::Type* functionReturnType,
    int loopDepth)
{
    switch (node->kind())
    {
            case HirNodeKind::Break:
            case HirNodeKind::Continue:
                if (!hasNoEdges(node))
                {
                    return fail(node, "break/continue cannot contain HIR child nodes");
                }
                if (loopDepth <= 0)
                {
                    return fail(node, "break/continue must be nested inside a HIR loop");
                }
                return true;

            case HirNodeKind::Return:
                if (functionReturnType == nullptr)
                {
                    return fail(node, "return appears outside a HIR function body");
                }
                if (functionReturnType->kind() == sema::TypeKind::Void)
                {
                    if (node->first() != nullptr)
                    {
                        return fail(node, "void HIR function cannot return a value");
                    }
                    return true;
                }
                if (node->first() == nullptr || !isExpressionKind(node->first()->kind()))
                {
                    return fail(node, "HIR return value must exactly match function return type");
                }
                tasks.push_back({VerificationTask::Kind::ReturnType, node,
                    functionReturnType, loopDepth, 0});
                tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
                return true;

            case HirNodeKind::ExpressionStatement:
                if (node->first() == nullptr || !isExpressionKind(node->first()->kind()))
                {
                    return fail(node, "expression statement must contain a valid HIR expression");
                }
                tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
                return true;
        default:
            return false;
    }
}

bool HirVerifier::verifyValueNode(
    const HirNode* node,
    std::vector<VerificationTask>& tasks,
    sema::Type* functionReturnType,
    int loopDepth)
{
    switch (node->kind())
    {
            case HirNodeKind::Constant:
                if (node->type() == nullptr || !node->hasConstant() || !hasNoEdges(node))
                {
                    return fail(node, "constant must have semantic type, value and no child nodes");
                }
                return true;

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
                tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
                return true;

            case HirNodeKind::Store:
                if (node->first() == nullptr || node->second() == nullptr || node->type() == nullptr ||
                    !verifyPointerTo(node->first(), node->type()) || node->second()->type() != node->type())
                {
                    return fail(node, "store requires pointer-to-T destination and T value");
                }
                tasks.push_back({VerificationTask::Kind::Node, node->second(), functionReturnType, loopDepth, 0});
                tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
                return true;

            case HirNodeKind::Conversion:
                if (node->first() == nullptr || node->type() == nullptr || !node->hasConversion())
                {
                    return fail(node, "conversion requires source value, destination type and conversion kind");
                }
                tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
                return true;

            case HirNodeKind::Unary:
                if (node->first() == nullptr || node->type() == nullptr || node->operationType() == nullptr ||
                    node->operation() == lexer::TokenKind::Invalid)
                {
                    return fail(node, "unary operation is missing resolved semantic information");
                }
                tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
                return true;

            case HirNodeKind::Binary:
                if (node->first() == nullptr || node->second() == nullptr || node->type() == nullptr ||
                    node->operationType() == nullptr || node->operation() == lexer::TokenKind::Invalid)
                {
                    return fail(node, "binary operation is missing resolved semantic information");
                }
                tasks.push_back({VerificationTask::Kind::Node, node->second(), functionReturnType, loopDepth, 0});
                tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
                return true;

            case HirNodeKind::Conditional:
                if (node->first() == nullptr || node->second() == nullptr || node->third() == nullptr ||
                    node->type() == nullptr || !isExpressionKind(node->first()->kind()) ||
                    !isExpressionKind(node->second()->kind()) || !isExpressionKind(node->third()->kind()) ||
                    node->first()->type() == nullptr || node->first()->type()->kind() != sema::TypeKind::Bool ||
                    node->second()->type() != node->type() || node->third()->type() != node->type())
                {
                    return fail(node, "conditional requires bool condition and two branches of its result type");
                }
                tasks.push_back({VerificationTask::Kind::Node, node->third(), functionReturnType, loopDepth, 0});
                tasks.push_back({VerificationTask::Kind::Node, node->second(), functionReturnType, loopDepth, 0});
                tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
                return true;
        default:
            return false;
    }
}

bool HirVerifier::verifyAccessNode(
    const HirNode* node,
    std::vector<VerificationTask>& tasks,
    sema::Type* functionReturnType,
    int loopDepth)
{
    switch (node->kind())
    {
            case HirNodeKind::Call:
                return verifyCallNode(node, tasks, functionReturnType, loopDepth);

            case HirNodeKind::IndexAddress:
                return verifyIndexAddressNode(node, tasks, functionReturnType, loopDepth);

            case HirNodeKind::FieldAddress:
                if (node->first() == nullptr || node->symbol() == nullptr ||
                    node->symbol()->kind() != sema::SymbolKind::Field || !node->hasOffset() ||
                    !verifyPointerTo(node, node->symbol()->type()))
                {
                    return fail(node, "field address requires resolved field, offset and pointer-to-field type");
                }
                tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
                return true;

            case HirNodeKind::DereferenceAddress:
                if (node->first() == nullptr || node->type() == nullptr ||
                    node->type()->kind() != sema::TypeKind::Pointer || node->first()->type() != node->type())
                {
                    return fail(node, "dereference address must preserve the evaluated pointer type");
                }
                tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
                return true;

            case HirNodeKind::ExtractField:
                if (node->first() == nullptr || !isExpressionKind(node->first()->kind()) ||
                    node->first()->type() == nullptr || node->first()->type()->kind() != sema::TypeKind::Struct ||
                    node->symbol() == nullptr || node->symbol()->kind() != sema::SymbolKind::Field ||
                    node->type() != node->symbol()->type() || !node->hasOffset())
                {
                    return fail(node, "field extraction requires struct value, resolved field and offset");
                }
                tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
                return true;
        default:
            return false;
    }
}

bool HirVerifier::verifyCallNode(
    const HirNode* node,
    std::vector<VerificationTask>& tasks,
    sema::Type* functionReturnType,
    int loopDepth)
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
    if (!node->children().empty())
    {
        tasks.push_back({VerificationTask::Kind::CallArgument, node,
            functionReturnType, loopDepth, 0});
    }
    return true;
}

bool HirVerifier::verifyIndexAddressNode(
    const HirNode* node,
    std::vector<VerificationTask>& tasks,
    sema::Type* functionReturnType,
    int loopDepth)
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
        return fail(node,
            "index address base/result types do not match resolved array or pointer type");
    }
    tasks.push_back({VerificationTask::Kind::Node, node->second(), functionReturnType, loopDepth, 0});
    tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
    return true;
}

bool HirVerifier::verifyMutationOrAllocationNode(
    const HirNode* node,
    std::vector<VerificationTask>& tasks,
    sema::Type* functionReturnType,
    int loopDepth)
{
    switch (node->kind())
    {
            case HirNodeKind::CompoundAssignment:
                if (node->first() == nullptr || node->second() == nullptr || node->type() == nullptr ||
                    node->operationType() == nullptr || node->operation() == lexer::TokenKind::Invalid ||
                    !verifyPointerTo(node->first(), node->type()))
                {
                    return fail(node,
                        "compound assignment is missing address or resolved operation information");
                }
                tasks.push_back({VerificationTask::Kind::Node, node->second(), functionReturnType, loopDepth, 0});
                tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
                return true;

            case HirNodeKind::Increment:
                if (node->first() == nullptr || node->type() == nullptr || node->operationType() == nullptr ||
                    (node->operation() != lexer::TokenKind::PlusPlus &&
                     node->operation() != lexer::TokenKind::MinusMinus) ||
                    !verifyPointerTo(node->first(), node->type()))
                {
                    return fail(node, "increment/decrement requires ++/-- and pointer-to-value storage address");
                }
                tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
                return true;

            case HirNodeKind::Allocation:
                if (node->type() == nullptr || node->type()->kind() != sema::TypeKind::Pointer ||
                    node->operationType() != node->type()->elementType() ||
                    (node->arrayForm() && (node->first() == nullptr || node->first()->type() == nullptr ||
                                           !isIntegerType(node->first()->type()))) ||
                    (!node->arrayForm() && node->first() != nullptr))
                {
                    return fail(node, "heap allocation must identify element type and array/scalar form");
                }
                if (node->first() != nullptr)
                {
                    tasks.push_back({VerificationTask::Kind::Node, node->first(),
                        functionReturnType, loopDepth, 0});
                }
                return true;

            case HirNodeKind::Deallocation:
                if (node->type() == nullptr || node->type()->kind() != sema::TypeKind::Void ||
                    node->first() == nullptr || node->first()->type() == nullptr ||
                    (node->first()->type()->kind() != sema::TypeKind::Pointer &&
                     node->first()->type()->kind() != sema::TypeKind::NullLiteral))
                {
                    return fail(node, "deallocation requires void result and pointer or null operand");
                }
                tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
                return true;

            case HirNodeKind::StackAllocation:
                if (node->type() == nullptr || node->type()->kind() != sema::TypeKind::Pointer ||
                    node->operationType() != node->type()->elementType() || node->first() == nullptr ||
                    node->first()->type() == nullptr || !isIntegerType(node->first()->type()))
                {
                    return fail(node, "stack allocation must identify element type and count expression");
                }
                tasks.push_back({VerificationTask::Kind::Node, node->first(), functionReturnType, loopDepth, 0});
                return true;
        default:
            return false;
    }
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
