#include "IterativeHirLowering.h"

#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"
#include "nativesharp/Sema/Type.h"
#include "nativesharp/Sema/TypeKind.h"

#include <utility>

namespace nativesharp::hir::internal
{
IterativeHirLowering::IterativeHirLowering(
    HirArena& arena,
    sema::TypeContext& types,
    const sema::SemanticModel& semanticModel,
    std::string& error)
    : arena_(arena), types_(types), semanticModel_(semanticModel), error_(error)
{
    frames_.reserve(128);
}

HirNode* IterativeHirLowering::lowerGlobal(ast::AstNode* node)
{
    return run({LoweringFrameKind::Global, node});
}

HirNode* IterativeHirLowering::lowerStruct(ast::AstNode* node)
{
    return run({LoweringFrameKind::Struct, node});
}

HirNode* IterativeHirLowering::lowerEnum(ast::AstNode* node)
{
    return run({LoweringFrameKind::Enum, node});
}

HirNode* IterativeHirLowering::lowerFunction(ast::AstNode* node)
{
    return run({LoweringFrameKind::Function, node});
}

HirNode* IterativeHirLowering::lowerGlobalVariable(ast::AstNode* node)
{
    return run({LoweringFrameKind::GlobalVariable, node});
}

HirNode* IterativeHirLowering::lowerBlock(ast::AstNode* node)
{
    return run({LoweringFrameKind::Block, node});
}

HirNode* IterativeHirLowering::lowerStatement(ast::AstNode* node)
{
    return run({LoweringFrameKind::Statement, node});
}

HirNode* IterativeHirLowering::lowerLocalVariable(ast::AstNode* node)
{
    return run({LoweringFrameKind::LocalVariable, node});
}

HirNode* IterativeHirLowering::lowerExpression(ast::AstNode* node, bool loadLValue)
{
    LoweringFrame frame{LoweringFrameKind::Expression, node};
    frame.loadLValue = loadLValue;
    return run(std::move(frame));
}

HirNode* IterativeHirLowering::lowerAddress(ast::AstNode* node)
{
    return run({LoweringFrameKind::Address, node});
}

HirNode* IterativeHirLowering::lowerConvertedOperand(
    const ast::AstNode* owner,
    ast::AstNode* operand,
    int operandIndex)
{
    LoweringFrame frame{LoweringFrameKind::ConvertedOperand, operand};
    frame.owner = owner;
    frame.operandIndex = operandIndex;
    return run(std::move(frame));
}

HirNode* IterativeHirLowering::run(LoweringFrame initial)
{
    std::vector<LoweringFrame>& frames = frames_;
    frames.clear();
    frames.push_back(std::move(initial));

    HirNode* completed = nullptr;
    bool hasCompleted = false;
    while (!frames.empty())
    {
        LoweringFrame& frame = frames.back();
        switch (frame.kind)
        {
            case LoweringFrameKind::Global:
                if (dispatchGlobalFrame(frame, frames, completed, hasCompleted))
                    continue;
                break;
            case LoweringFrameKind::Struct:
                if (dispatchStructFrame(frame, frames, completed, hasCompleted))
                    continue;
                break;
            case LoweringFrameKind::Enum:
                if (dispatchEnumFrame(frame, frames, completed, hasCompleted))
                    continue;
                break;
            case LoweringFrameKind::Function:
                if (dispatchFunctionFrame(frame, frames, completed, hasCompleted))
                    continue;
                break;
            case LoweringFrameKind::GlobalVariable:
                if (dispatchGlobalVariableFrame(frame, frames, completed, hasCompleted))
                    continue;
                break;
            case LoweringFrameKind::Block:
                if (dispatchBlockFrame(frame, frames, completed, hasCompleted))
                    continue;
                break;
            case LoweringFrameKind::Statement:
                if (dispatchStatementFrame(frame, frames, completed, hasCompleted))
                    continue;
                break;
            case LoweringFrameKind::LocalVariable:
                if (dispatchLocalVariableFrame(frame, frames, completed, hasCompleted))
                    continue;
                break;
            case LoweringFrameKind::Expression:
                if (dispatchExpressionFrame(frame, frames, completed, hasCompleted))
                    continue;
                break;
            case LoweringFrameKind::Address:
                if (dispatchAddressFrame(frame, frames, completed, hasCompleted))
                    continue;
                break;
            case LoweringFrameKind::ConvertedOperand:
                if (dispatchConvertedOperandFrame(frame, frames, completed, hasCompleted))
                    continue;
                break;
        }

        finishFrame(frames, completed, hasCompleted, nullptr);
    }
    return hasCompleted ? completed : nullptr;
}

void IterativeHirLowering::finishFrame(
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted,
    HirNode* result)
{
    frames.pop_back();
    completed = result;
    hasCompleted = true;
}

HirNode* IterativeHirLowering::takeCompletedFrame(
    HirNode*& completed,
    bool& hasCompleted)
{
    HirNode* result = completed;
    completed = nullptr;
    hasCompleted = false;
    return result;
}

void IterativeHirLowering::descendFrame(
    std::vector<LoweringFrame>& frames,
    bool& hasCompleted,
    LoweringFrame child)
{
    hasCompleted = false;
    frames.push_back(std::move(child));
}

bool IterativeHirLowering::dispatchGlobalFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    switch (frame.node->kind())
    {
        case ast::AstNodeKind::StructDeclaration:
            frame.kind = LoweringFrameKind::Struct;
            return true;
        case ast::AstNodeKind::EnumDeclaration:
            frame.kind = LoweringFrameKind::Enum;
            return true;
        case ast::AstNodeKind::FunctionDeclaration:
            frame.kind = LoweringFrameKind::Function;
            return true;
        case ast::AstNodeKind::GlobalVariableDeclaration:
            frame.kind = LoweringFrameKind::GlobalVariable;
            return true;
        default:
            fail("unsupported global AST node while building HIR");
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
    }
}

bool IterativeHirLowering::dispatchStructFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    sema::Symbol* symbol = boundSymbol(frame.node);
    if (symbol == nullptr)
    {
        finishFrame(frames, completed, hasCompleted, nullptr);
        return true;
    }
    HirNode* result = arena_.make(HirNodeKind::StructDeclaration, frame.node->range());
    result->setSymbol(symbol);
    result->setType(symbol->type());
    for (ast::AstNode* fieldNode : frame.node->children())
    {
        sema::Symbol* field = boundSymbol(fieldNode);
        if (field == nullptr)
        {
            result = nullptr;
            break;
        }
        HirNode* fieldHir = arena_.make(HirNodeKind::FieldDeclaration, fieldNode->range());
        fieldHir->setSymbol(field);
        fieldHir->setType(field->type());
        if (field->hasFieldOffset())
            fieldHir->setOffset(field->fieldOffset());
        result->addChild(fieldHir);
    }
    finishFrame(frames, completed, hasCompleted, result);
    return true;

}

bool IterativeHirLowering::dispatchEnumFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    sema::Symbol* symbol = boundSymbol(frame.node);
    if (symbol == nullptr)
    {
        finishFrame(frames, completed, hasCompleted, nullptr);
        return true;
    }
    HirNode* result = arena_.make(HirNodeKind::EnumDeclaration, frame.node->range());
    result->setSymbol(symbol);
    result->setType(symbol->type());
    for (ast::AstNode* memberNode : frame.node->children())
    {
        sema::Symbol* member = boundSymbol(memberNode);
        if (member == nullptr)
        {
            result = nullptr;
            break;
        }
        HirNode* memberHir = arena_.make(
            HirNodeKind::EnumMemberDeclaration, memberNode->range());
        memberHir->setSymbol(member);
        memberHir->setType(member->type());
        memberHir->setConstant(member->constantValue());
        result->addChild(memberHir);
    }
    finishFrame(frames, completed, hasCompleted, result);
    return true;

}

bool IterativeHirLowering::dispatchFunctionFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 0)
    {
        sema::Symbol* symbol = boundSymbol(frame.node);
        if (symbol == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        frame.result = arena_.make(
            HirNodeKind::FunctionDeclaration, frame.node->range());
        frame.result->setSymbol(symbol);
        frame.result->setType(symbol->type());
        const std::vector<sema::Type*>& parameterTypes =
            symbol->type()->parameterTypes();
        for (std::size_t index = 0; index < frame.node->children().size(); ++index)
        {
            ast::AstNode* parameterNode = frame.node->children()[index];
            sema::Symbol* parameter = semanticModel_.symbol(parameterNode);
            if (parameter == nullptr && frame.node->first() != nullptr)
            {
                frame.result = nullptr;
                break;
            }
            HirNode* parameterHir = arena_.make(
                HirNodeKind::ParameterDeclaration, parameterNode->range());
            parameterHir->setSymbol(parameter);
            parameterHir->setType(
                parameter != nullptr ? parameter->type() : parameterTypes[index]);
            frame.result->addChild(parameterHir);
        }
        if (frame.result == nullptr || frame.node->first() == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, frame.result);
            return true;
        }
        frame.state = 1;
        descendFrame(frames, hasCompleted, {LoweringFrameKind::Block, frame.node->first()});
        return true;
    }
    if (hasCompleted)
    {
        HirNode* body = takeCompletedFrame(completed, hasCompleted);
        if (body == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        frame.result->setFirst(body);
        finishFrame(frames, completed, hasCompleted, frame.result);
        return true;
    }
    return false;
}

bool IterativeHirLowering::dispatchGlobalVariableFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 0)
    {
        sema::Symbol* symbol = boundSymbol(frame.node);
        if (symbol == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        frame.result = arena_.make(
            HirNodeKind::GlobalVariableDeclaration, frame.node->range());
        frame.result->setSymbol(symbol);
        frame.result->setType(symbol->type());
        if (frame.node->first() == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, frame.result);
            return true;
        }
        frame.state = 1;
        LoweringFrame child{
            LoweringFrameKind::ConvertedOperand, frame.node->first()};
        child.owner = frame.node;
        descendFrame(frames, hasCompleted, std::move(child));
        return true;
    }
    if (hasCompleted)
    {
        HirNode* initializer = takeCompletedFrame(completed, hasCompleted);
        if (initializer == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        frame.result->setFirst(initializer);
        finishFrame(frames, completed, hasCompleted, frame.result);
        return true;
    }
    return false;
}

bool IterativeHirLowering::dispatchBlockFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 0)
    {
        frame.result = arena_.make(HirNodeKind::Block, frame.node->range());
        frame.state = 1;
    }
    if (hasCompleted)
    {
        HirNode* statement = takeCompletedFrame(completed, hasCompleted);
        if (statement == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        frame.result->addChild(statement);
        ++frame.index;
    }
    if (frame.index == frame.node->children().size())
    {
        finishFrame(frames, completed, hasCompleted, frame.result);
        return true;
    }
    descendFrame(frames, hasCompleted, {LoweringFrameKind::Statement, frame.node->children()[frame.index]});
    return true;
}

bool IterativeHirLowering::dispatchStatementFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (lowerStatementFrame(frame, frames, completed, hasCompleted))
        return true;
    return false;
}

bool IterativeHirLowering::dispatchLocalVariableFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 0)
    {
        sema::Symbol* symbol = boundSymbol(frame.node);
        if (symbol == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        frame.result = arena_.make(
            HirNodeKind::LocalVariableDeclaration, frame.node->range());
        frame.result->setSymbol(symbol);
        frame.result->setType(symbol->type());
        if (frame.node->first() == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, frame.result);
            return true;
        }
        frame.state = 1;
        LoweringFrame child{
            LoweringFrameKind::ConvertedOperand, frame.node->first()};
        child.owner = frame.node;
        descendFrame(frames, hasCompleted, std::move(child));
        return true;
    }
    if (hasCompleted)
    {
        HirNode* initializer = takeCompletedFrame(completed, hasCompleted);
        if (initializer == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        frame.result->setFirst(initializer);
        finishFrame(frames, completed, hasCompleted, frame.result);
        return true;
    }
    return false;
}

bool IterativeHirLowering::dispatchExpressionFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (lowerExpressionFrame(frame, frames, completed, hasCompleted))
        return true;
    return false;
}

bool IterativeHirLowering::dispatchAddressFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (lowerAddressFrame(frame, frames, completed, hasCompleted))
        return true;
    return false;
}

bool IterativeHirLowering::dispatchConvertedOperandFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 0)
    {
        frame.state = 1;
        descendFrame(frames, hasCompleted, {LoweringFrameKind::Expression, frame.node});
        return true;
    }
    if (hasCompleted)
    {
        HirNode* value = takeCompletedFrame(completed, hasCompleted);
        if (value == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        for (const sema::ConversionInfo& conversion :
            semanticModel_.conversions(frame.owner))
        {
            if (conversion.role == sema::ConversionRole::Result ||
                conversion.operand != frame.node)
            {
                continue;
            }
            if (frame.operandIndex >= 0 && conversion.operandIndex >= 0 &&
                conversion.operandIndex != frame.operandIndex)
            {
                continue;
            }
            value = wrapConversion(frame.owner->range(), value, conversion);
            if (value == nullptr)
                break;
        }
        finishFrame(frames, completed, hasCompleted, value);
        return true;
    }
    return false;
}
bool IterativeHirLowering::lowerStatementFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 0)
        return beginStatementFrame(frame, frames, completed, hasCompleted);
    if (frame.state >= 10 && frame.state <= 12)
        return continueConditionalStatementFrame(frame, frames, completed, hasCompleted);
    if (frame.state >= 20 && frame.state <= 21)
        return continueLoopStatementFrame(frame, frames, completed, hasCompleted);
    if (frame.state >= 30 && frame.state <= 33)
        return continueForStatementFrame(frame, frames, completed, hasCompleted);
    return finishValueStatementFrame(frame, frames, completed, hasCompleted);
}

bool IterativeHirLowering::beginStatementFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 0)
    {
        switch (frame.node->kind())
        {
            case ast::AstNodeKind::BlockStatement:
                frame.kind = LoweringFrameKind::Block;
                return true;
            case ast::AstNodeKind::VariableDeclarationStatement:
                frame.kind = LoweringFrameKind::LocalVariable;
                return true;
            case ast::AstNodeKind::IfStatement:
                frame.result = arena_.make(HirNodeKind::If, frame.node->range());
                frame.state = 10;
                descendFrame(frames, hasCompleted, {LoweringFrameKind::Expression, frame.node->first()});
                return true;
            case ast::AstNodeKind::WhileStatement:
                frame.result = arena_.make(HirNodeKind::While, frame.node->range());
                frame.state = 20;
                descendFrame(frames, hasCompleted, {LoweringFrameKind::Expression, frame.node->first()});
                return true;
            case ast::AstNodeKind::ForStatement:
                frame.result = arena_.make(HirNodeKind::For, frame.node->range());
                frame.state = 30;
                if (frame.node->first() != nullptr)
                {
                    const ast::AstNodeKind kind = frame.node->first()->kind();
                    LoweringFrame child{
                        kind == ast::AstNodeKind::BlockStatement
                            ? LoweringFrameKind::Block
                            : kind == ast::AstNodeKind::VariableDeclarationStatement
                                ? LoweringFrameKind::LocalVariable
                                : LoweringFrameKind::Expression,
                        frame.node->first()};
                    if (child.kind == LoweringFrameKind::Expression)
                        child.loadLValue = false;
                    descendFrame(frames, hasCompleted, std::move(child));
                }
                return true;
            case ast::AstNodeKind::BreakStatement:
                finishFrame(frames, completed, hasCompleted, arena_.make(HirNodeKind::Break, frame.node->range()));
                return true;
            case ast::AstNodeKind::ContinueStatement:
                finishFrame(frames, completed, hasCompleted, arena_.make(HirNodeKind::Continue, frame.node->range()));
                return true;
            case ast::AstNodeKind::ReturnStatement:
                frame.result = arena_.make(HirNodeKind::Return, frame.node->range());
                if (frame.node->first() == nullptr)
                {
                    frame.result->setType(types_.builtin(sema::TypeKind::Void));
                    finishFrame(frames, completed, hasCompleted, frame.result);
                }
                else
                {
                    frame.state = 40;
                    LoweringFrame child{
                        LoweringFrameKind::ConvertedOperand, frame.node->first()};
                    child.owner = frame.node;
                    descendFrame(frames, hasCompleted, std::move(child));
                }
                return true;
            case ast::AstNodeKind::ExpressionStatement:
            {
                frame.result = arena_.make(
                    HirNodeKind::ExpressionStatement, frame.node->range());
                frame.state = 50;
                LoweringFrame child{LoweringFrameKind::Expression, frame.node->first()};
                child.loadLValue = false;
                descendFrame(frames, hasCompleted, std::move(child));
                return true;
            }
            case ast::AstNodeKind::StaticAssertStatement:
                finishFrame(frames, completed, hasCompleted, arena_.make(HirNodeKind::Block, frame.node->range()));
                return true;
            default:
                fail("unsupported statement AST node while building HIR");
                finishFrame(frames, completed, hasCompleted, nullptr);
                return true;
        }
    }
    return false;
}

bool IterativeHirLowering::continueConditionalStatementFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 10 && hasCompleted)
    {
        frame.result->setFirst(takeCompletedFrame(completed, hasCompleted));
        frame.state = 11;
        descendFrame(frames, hasCompleted, {LoweringFrameKind::Block, frame.node->second()});
        return true;
    }
    if (frame.state == 11 && hasCompleted)
    {
        frame.result->setSecond(takeCompletedFrame(completed, hasCompleted));
        if (frame.node->third() == nullptr)
        {
            finishFrame(
                frames,
                completed,
                hasCompleted,
                frame.result->first() == nullptr || frame.result->second() == nullptr
                    ? nullptr : frame.result);
            return true;
        }
        frame.state = 12;
        descendFrame(frames, hasCompleted, {
            frame.node->third()->kind() == ast::AstNodeKind::BlockStatement
                ? LoweringFrameKind::Block : LoweringFrameKind::Statement,
            frame.node->third()});
        return true;
    }
    if (frame.state == 12 && hasCompleted)
    {
        frame.result->setThird(takeCompletedFrame(completed, hasCompleted));
        finishFrame(
            frames,
            completed,
            hasCompleted,
            frame.result->first() == nullptr || frame.result->second() == nullptr
                ? nullptr : frame.result);
        return true;
    }
    return false;
}

bool IterativeHirLowering::continueLoopStatementFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 20 && hasCompleted)
    {
        frame.result->setFirst(takeCompletedFrame(completed, hasCompleted));
        frame.state = 21;
        descendFrame(frames, hasCompleted, {LoweringFrameKind::Block, frame.node->second()});
        return true;
    }
    if (frame.state == 21 && hasCompleted)
    {
        frame.result->setSecond(takeCompletedFrame(completed, hasCompleted));
        finishFrame(
            frames,
            completed,
            hasCompleted,
            frame.result->first() == nullptr || frame.result->second() == nullptr
                ? nullptr : frame.result);
        return true;
    }
    return false;
}

bool IterativeHirLowering::continueForStatementFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 30)
    {
        if (frame.node->first() != nullptr)
        {
            if (!hasCompleted)
                return false;
            frame.result->setFirst(takeCompletedFrame(completed, hasCompleted));
        }
        frame.state = 31;
        if (frame.node->second() != nullptr)
            descendFrame(frames, hasCompleted, {LoweringFrameKind::Expression, frame.node->second()});
        return true;
    }
    if (frame.state == 31)
    {
        if (frame.node->second() != nullptr)
        {
            if (!hasCompleted)
                return false;
            frame.result->setSecond(takeCompletedFrame(completed, hasCompleted));
        }
        frame.state = 32;
        if (frame.node->third() != nullptr)
        {
            LoweringFrame child{LoweringFrameKind::Expression, frame.node->third()};
            child.loadLValue = false;
            descendFrame(frames, hasCompleted, std::move(child));
        }
        return true;
    }
    if (frame.state == 32)
    {
        if (frame.node->third() != nullptr)
        {
            if (!hasCompleted)
                return false;
            frame.result->setThird(takeCompletedFrame(completed, hasCompleted));
        }
        frame.state = 33;
        if (!frame.node->children().empty())
            descendFrame(frames, hasCompleted, {LoweringFrameKind::Block, frame.node->children().front()});
        return true;
    }
    if (frame.state == 33)
    {
        if (!frame.node->children().empty())
        {
            if (!hasCompleted)
                return false;
            frame.result->addChild(takeCompletedFrame(completed, hasCompleted));
        }
        const bool failed =
            (!frame.node->children().empty() && frame.result->children().front() == nullptr) ||
            (frame.node->first() != nullptr && frame.result->first() == nullptr) ||
            (frame.node->second() != nullptr && frame.result->second() == nullptr) ||
            (frame.node->third() != nullptr && frame.result->third() == nullptr);
        finishFrame(frames, completed, hasCompleted, failed ? nullptr : frame.result);
        return true;
    }
    return false;
}

bool IterativeHirLowering::finishValueStatementFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 40 && hasCompleted)
    {
        HirNode* value = takeCompletedFrame(completed, hasCompleted);
        if (value == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        frame.result->setFirst(value);
        frame.result->setType(value->type());
        finishFrame(frames, completed, hasCompleted, frame.result);
        return true;
    }
    if (frame.state == 50 && hasCompleted)
    {
        frame.result->setFirst(takeCompletedFrame(completed, hasCompleted));
        finishFrame(frames, completed, hasCompleted, frame.result->first() == nullptr ? nullptr : frame.result);
        return true;
    }
    return false;
}

bool IterativeHirLowering::lowerExpressionFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 0)
        return beginExpressionFrame(frame, frames, completed, hasCompleted);
    if (frame.state >= 1 && frame.state <= 11)
        return finishLoadOrUnaryFrame(frame, frames, completed, hasCompleted);
    if (frame.state >= 20 && frame.state <= 21)
        return finishBinaryFrame(frame, frames, completed, hasCompleted);
    if (frame.state >= 30 && frame.state <= 32)
        return finishConditionalFrame(frame, frames, completed, hasCompleted);
    if (frame.state >= 40 && frame.state <= 42)
        return finishAssignmentFrame(frame, frames, completed, hasCompleted);
    return finishCallMemberOrAllocationFrame(frame, frames, completed, hasCompleted);
}

bool IterativeHirLowering::beginExpressionFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 0)
    {
        if (frame.node == nullptr)
        {
            fail("null expression reached HIR builder");
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        frame.info = expressionInfo(frame.node);
        if (frame.info == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        if (!sema::isUsableValueCategory(frame.info->category))
        {
            fail("NS3020: moved-from expression reached HIR lowering");
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        if (sema::isPlaceCategory(frame.info->category))
        {
            frame.state = 1;
            descendFrame(frames, hasCompleted, {LoweringFrameKind::Address, frame.node});
            return true;
        }
        if (frame.info->symbol != nullptr &&
            frame.info->symbol->kind() == sema::SymbolKind::EnumMember)
        {
            HirNode* constant = arena_.make(HirNodeKind::Constant, frame.node->range());
            constant->setType(frame.info->type);
            constant->setConstant(frame.info->constantValue);
            constant->setSymbol(frame.info->symbol);
            finishFrame(frames, completed, hasCompleted, constant);
            return true;
        }

        switch (frame.node->kind())
        {
            case ast::AstNodeKind::IntegerLiteralExpression:
            case ast::AstNodeKind::FloatingLiteralExpression:
            case ast::AstNodeKind::CharacterLiteralExpression:
            case ast::AstNodeKind::StringLiteralExpression:
            case ast::AstNodeKind::BooleanLiteralExpression:
            case ast::AstNodeKind::NullLiteralExpression:
            case ast::AstNodeKind::SizeofTypeExpression:
            case ast::AstNodeKind::SizeofExpression:
            case ast::AstNodeKind::AlignofTypeExpression:
            case ast::AstNodeKind::AlignofExpression:
            case ast::AstNodeKind::OffsetofExpression:
            case ast::AstNodeKind::IdentifierExpression:
            case ast::AstNodeKind::UnaryExpression:
            case ast::AstNodeKind::PostfixExpression:
            case ast::AstNodeKind::BinaryExpression:
            case ast::AstNodeKind::ConditionalExpression:
            case ast::AstNodeKind::AssignmentExpression:
                return beginBasicExpressionFrame(frame, frames, completed, hasCompleted);
            case ast::AstNodeKind::CallExpression:
            case ast::AstNodeKind::MemberExpression:
            case ast::AstNodeKind::PointerMemberExpression:
            case ast::AstNodeKind::CastExpression:
                return beginCallOrMemberExpressionFrame(
                    frame, frames, completed, hasCompleted);
            case ast::AstNodeKind::NewExpression:
            case ast::AstNodeKind::DeleteExpression:
            case ast::AstNodeKind::StackallocExpression:
                return beginAllocationExpressionFrame(frame, frames, completed, hasCompleted);
            default:
                fail("unsupported expression AST node while building HIR");
                finishFrame(frames, completed, hasCompleted, nullptr);
                return true;
        }
    }
    return false;
}

bool IterativeHirLowering::beginBasicExpressionFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    switch (frame.node->kind())
    {
            case ast::AstNodeKind::IntegerLiteralExpression:
            case ast::AstNodeKind::FloatingLiteralExpression:
            case ast::AstNodeKind::CharacterLiteralExpression:
            case ast::AstNodeKind::StringLiteralExpression:
            case ast::AstNodeKind::BooleanLiteralExpression:
            case ast::AstNodeKind::NullLiteralExpression:
            case ast::AstNodeKind::SizeofTypeExpression:
            case ast::AstNodeKind::SizeofExpression:
            case ast::AstNodeKind::AlignofTypeExpression:
            case ast::AstNodeKind::AlignofExpression:
            case ast::AstNodeKind::OffsetofExpression:
            {
                if (!frame.info->isConstant)
                {
                    fail("constant HIR expression has no semantic constant value");
                    finishFrame(frames, completed, hasCompleted, nullptr);
                    return true;
                }
                HirNode* constant = arena_.make(HirNodeKind::Constant, frame.node->range());
                constant->setType(frame.info->type);
                constant->setConstant(frame.info->constantValue);
                finishFrame(frames, completed, hasCompleted, constant);
                return true;
            }
            case ast::AstNodeKind::IdentifierExpression:
                fail("non-lvalue identifier reached HIR without enum-member or call context");
                finishFrame(frames, completed, hasCompleted, nullptr);
                return true;
            case ast::AstNodeKind::UnaryExpression:
            case ast::AstNodeKind::PostfixExpression:
                if (frame.node->tokenKind() == lexer::TokenKind::Amp)
                {
                    frame.kind = LoweringFrameKind::Address;
                    frame.node = frame.node->first();
                    frame.state = 0;
                    return true;
                }
                if (frame.node->tokenKind() == lexer::TokenKind::PlusPlus ||
                    frame.node->tokenKind() == lexer::TokenKind::MinusMinus)
                {
                    frame.state = 10;
                    descendFrame(frames, hasCompleted, {LoweringFrameKind::Address, frame.node->first()});
                }
                else
                {
                    frame.state = 11;
                    LoweringFrame child{
                        LoweringFrameKind::ConvertedOperand, frame.node->first()};
                    child.owner = frame.node;
                    child.operandIndex = 0;
                    descendFrame(frames, hasCompleted, std::move(child));
                }
                return true;
            case ast::AstNodeKind::BinaryExpression:
            {
                frame.state = 20;
                LoweringFrame child{
                    LoweringFrameKind::ConvertedOperand, frame.node->first()};
                child.owner = frame.node;
                child.operandIndex = 0;
                descendFrame(frames, hasCompleted, std::move(child));
                return true;
            }
            case ast::AstNodeKind::ConditionalExpression:
                frame.state = 30;
                descendFrame(frames, hasCompleted, {LoweringFrameKind::Expression, frame.node->first()});
                return true;
            case ast::AstNodeKind::AssignmentExpression:
                frame.state = 40;
                descendFrame(frames, hasCompleted, {LoweringFrameKind::Address, frame.node->first()});
                return true;
        default:
            return false;
    }
}

bool IterativeHirLowering::beginCallOrMemberExpressionFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    switch (frame.node->kind())
    {
            case ast::AstNodeKind::CallExpression:
            {
                const sema::ExpressionInfo* calleeInfo = expressionInfo(frame.node->first());
                if (calleeInfo == nullptr || calleeInfo->symbol == nullptr)
                {
                    fail("call expression has no resolved function symbol");
                    finishFrame(frames, completed, hasCompleted, nullptr);
                    return true;
                }
                frame.result = arena_.make(HirNodeKind::Call, frame.node->range());
                frame.result->setType(frame.info->type);
                frame.result->setSymbol(calleeInfo->symbol);
                frame.state = 50;
                if (frame.node->children().empty())
                {
                    finishFrame(frames, completed, hasCompleted, frame.result);
                    return true;
                }
                LoweringFrame child{
                    LoweringFrameKind::ConvertedOperand, frame.node->children().front()};
                child.owner = frame.node;
                child.operandIndex = 0;
                descendFrame(frames, hasCompleted, std::move(child));
                return true;
            }
            case ast::AstNodeKind::MemberExpression:
            case ast::AstNodeKind::PointerMemberExpression:
                if (!sema::isPlaceCategory(frame.info->category))
                {
                    frame.state = 60;
                    descendFrame(frames, hasCompleted, {LoweringFrameKind::Expression, frame.node->first()});
                }
                else
                {
                    fail("lvalue member unexpectedly bypassed generic HIR lvalue lowering");
                    finishFrame(frames, completed, hasCompleted, nullptr);
                }
                return true;
            case ast::AstNodeKind::CastExpression:
                frame.kind = LoweringFrameKind::ConvertedOperand;
                frame.owner = frame.node;
                frame.node = frame.node->first();
                frame.operandIndex = 0;
                frame.state = 0;
                return true;
        default:
            return false;
    }
}

bool IterativeHirLowering::beginAllocationExpressionFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    switch (frame.node->kind())
    {
            case ast::AstNodeKind::NewExpression:
                frame.result = arena_.make(HirNodeKind::Allocation, frame.node->range());
                frame.result->setType(frame.info->type);
                frame.result->setOperationType(frame.info->type->elementType());
                frame.result->setArrayForm(frame.node->flag());
                if (frame.node->first() == nullptr)
                {
                    finishFrame(frames, completed, hasCompleted, frame.result);
                }
                else
                {
                    frame.state = 70;
                    descendFrame(frames, hasCompleted, {LoweringFrameKind::Expression, frame.node->first()});
                }
                return true;
            case ast::AstNodeKind::DeleteExpression:
                frame.result = arena_.make(HirNodeKind::Deallocation, frame.node->range());
                frame.result->setType(frame.info->type);
                frame.result->setArrayForm(frame.node->flag());
                frame.state = 71;
                descendFrame(frames, hasCompleted, {LoweringFrameKind::Expression, frame.node->first()});
                return true;
            case ast::AstNodeKind::StackallocExpression:
                frame.result = arena_.make(HirNodeKind::StackAllocation, frame.node->range());
                frame.result->setType(frame.info->type);
                frame.result->setOperationType(frame.info->type->elementType());
                frame.state = 72;
                descendFrame(frames, hasCompleted, {LoweringFrameKind::Expression, frame.node->first()});
                return true;
        default:
            return false;
    }
}

bool IterativeHirLowering::finishLoadOrUnaryFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 1 && hasCompleted)
    {
        HirNode* address = takeCompletedFrame(completed, hasCompleted);
        if (address == nullptr || !frame.loadLValue)
        {
            finishFrame(frames, completed, hasCompleted, address);
            return true;
        }
        HirNode* load = arena_.make(HirNodeKind::Load, frame.node->range());
        load->setType(frame.info->type);
        load->setFirst(address);
        finishFrame(frames, completed, hasCompleted, load);
        return true;
    }
    if ((frame.state == 10 || frame.state == 11) && hasCompleted)
    {
        HirNode* operand = takeCompletedFrame(completed, hasCompleted);
        const sema::ResolvedOperatorInfo* resolved = operatorInfo(frame.node);
        if (operand == nullptr || resolved == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        HirNode* result = arena_.make(
            frame.state == 10 ? HirNodeKind::Increment : HirNodeKind::Unary,
            frame.node->range());
        result->setType(frame.info->type);
        result->setOperationType(resolved->operationType);
        result->setOperation(frame.node->tokenKind());
        if (frame.state == 10)
            result->setPostfix(frame.node->kind() == ast::AstNodeKind::PostfixExpression);
        result->setFirst(operand);
        finishFrame(frames, completed, hasCompleted, result);
        return true;
    }
    return false;
}

bool IterativeHirLowering::finishBinaryFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 20 && hasCompleted)
    {
        frame.first = takeCompletedFrame(completed, hasCompleted);
        frame.state = 21;
        LoweringFrame child{
            LoweringFrameKind::ConvertedOperand, frame.node->second()};
        child.owner = frame.node;
        child.operandIndex = 1;
        descendFrame(frames, hasCompleted, std::move(child));
        return true;
    }
    if (frame.state == 21 && hasCompleted)
    {
        frame.second = takeCompletedFrame(completed, hasCompleted);
        const sema::ResolvedOperatorInfo* resolved = operatorInfo(frame.node);
        if (frame.first == nullptr || frame.second == nullptr || resolved == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        HirNode* binary = arena_.make(HirNodeKind::Binary, frame.node->range());
        binary->setType(frame.info->type);
        binary->setOperationType(resolved->operationType);
        binary->setOperation(frame.node->tokenKind());
        binary->setFirst(frame.first);
        binary->setSecond(frame.second);
        finishFrame(frames, completed, hasCompleted, binary);
        return true;
    }
    return false;
}

bool IterativeHirLowering::finishConditionalFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 30 && hasCompleted)
    {
        frame.result = takeCompletedFrame(completed, hasCompleted);
        frame.state = 31;
        LoweringFrame child{
            LoweringFrameKind::ConvertedOperand, frame.node->second()};
        child.owner = frame.node;
        child.operandIndex = 1;
        descendFrame(frames, hasCompleted, std::move(child));
        return true;
    }
    if (frame.state == 31 && hasCompleted)
    {
        frame.first = takeCompletedFrame(completed, hasCompleted);
        frame.state = 32;
        LoweringFrame child{
            LoweringFrameKind::ConvertedOperand, frame.node->third()};
        child.owner = frame.node;
        child.operandIndex = 2;
        descendFrame(frames, hasCompleted, std::move(child));
        return true;
    }
    if (frame.state == 32 && hasCompleted)
    {
        frame.second = takeCompletedFrame(completed, hasCompleted);
        if (frame.result == nullptr || frame.first == nullptr || frame.second == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        HirNode* conditional = arena_.make(HirNodeKind::Conditional, frame.node->range());
        conditional->setType(frame.info->type);
        conditional->setFirst(frame.result);
        conditional->setSecond(frame.first);
        conditional->setThird(frame.second);
        finishFrame(frames, completed, hasCompleted, conditional);
        return true;
    }
    return false;
}

bool IterativeHirLowering::finishAssignmentFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 40 && hasCompleted)
    {
        frame.first = takeCompletedFrame(completed, hasCompleted);
        if (frame.first == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        frame.state = frame.node->tokenKind() == lexer::TokenKind::Equal ? 41 : 42;
        LoweringFrame child{
            LoweringFrameKind::ConvertedOperand, frame.node->second()};
        child.owner = frame.node;
        child.operandIndex = 1;
        descendFrame(frames, hasCompleted, std::move(child));
        return true;
    }
    if ((frame.state == 41 || frame.state == 42) && hasCompleted)
    {
        HirNode* right = takeCompletedFrame(completed, hasCompleted);
        if (frame.state == 41)
        {
            if (right == nullptr)
            {
                finishFrame(frames, completed, hasCompleted, nullptr);
                return true;
            }
            HirNode* store = arena_.make(HirNodeKind::Store, frame.node->range());
            store->setType(frame.info->type);
            store->setFirst(frame.first);
            store->setSecond(right);
            finishFrame(frames, completed, hasCompleted, store);
            return true;
        }
        const sema::ResolvedOperatorInfo* resolved = operatorInfo(frame.node);
        if (right == nullptr || resolved == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        HirNode* compound = arena_.make(
            HirNodeKind::CompoundAssignment, frame.node->range());
        compound->setType(frame.info->type);
        compound->setOperationType(resolved->operationType);
        compound->setOperation(resolved->tokenKind);
        compound->setFirst(frame.first);
        compound->setSecond(right);
        const sema::ConversionInfo* read = operandConversion(
            frame.node, frame.node->first());
        if (read != nullptr)
            compound->setReadConversionKind(read->kind);
        const sema::ConversionInfo* result = resultConversion(frame.node);
        if (result != nullptr)
            compound->setResultConversionKind(result->kind);
        finishFrame(frames, completed, hasCompleted, compound);
        return true;
    }
    return false;
}

bool IterativeHirLowering::finishCallMemberOrAllocationFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 50 && hasCompleted)
    {
        HirNode* argument = takeCompletedFrame(completed, hasCompleted);
        if (argument == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        frame.result->addChild(argument);
        ++frame.index;
        if (frame.index == frame.node->children().size())
        {
            finishFrame(frames, completed, hasCompleted, frame.result);
            return true;
        }
        LoweringFrame child{
            LoweringFrameKind::ConvertedOperand, frame.node->children()[frame.index]};
        child.owner = frame.node;
        child.operandIndex = static_cast<int>(frame.index);
        descendFrame(frames, hasCompleted, std::move(child));
        return true;
    }

    if (frame.state == 60 && hasCompleted)
    {
        HirNode* base = takeCompletedFrame(completed, hasCompleted);
        if (base == nullptr || frame.info->symbol == nullptr)
        {
            fail("rvalue member access has no resolved field");
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        HirNode* extract = arena_.make(HirNodeKind::ExtractField, frame.node->range());
        extract->setType(frame.info->type);
        extract->setSymbol(frame.info->symbol);
        if (frame.info->symbol->hasFieldOffset())
            extract->setOffset(frame.info->symbol->fieldOffset());
        extract->setFirst(base);
        finishFrame(frames, completed, hasCompleted, extract);
        return true;
    }

    if ((frame.state == 70 || frame.state == 71 || frame.state == 72) && hasCompleted)
    {
        frame.result->setFirst(takeCompletedFrame(completed, hasCompleted));
        finishFrame(frames, completed, hasCompleted, frame.result->first() == nullptr ? nullptr : frame.result);
        return true;
    }
    return false;
}

bool IterativeHirLowering::lowerAddressFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 0)
        return beginAddressFrame(frame, frames, completed, hasCompleted);
    if (frame.state == 10)
        return finishDereferenceAddressFrame(frame, frames, completed, hasCompleted);
    if (frame.state >= 20 && frame.state <= 21)
        return finishIndexAddressFrame(frame, frames, completed, hasCompleted);
    return finishMemberAddressFrame(frame, frames, completed, hasCompleted);
}

bool IterativeHirLowering::beginAddressFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 0)
    {
        frame.info = expressionInfo(frame.node);
        if (frame.info == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        switch (frame.node->kind())
        {
            case ast::AstNodeKind::IdentifierExpression:
            {
                if (frame.info->symbol == nullptr)
                {
                    fail("addressable identifier has no resolved symbol");
                    finishFrame(frames, completed, hasCompleted, nullptr);
                    return true;
                }
                HirNode* address = arena_.make(
                    HirNodeKind::SymbolAddress, frame.node->range());
                address->setType(types_.pointerTo(frame.info->type));
                address->setSymbol(frame.info->symbol);
                finishFrame(frames, completed, hasCompleted, address);
                return true;
            }
            case ast::AstNodeKind::UnaryExpression:
                if (frame.node->tokenKind() == lexer::TokenKind::Star)
                {
                    frame.state = 10;
                    descendFrame(frames, hasCompleted, {LoweringFrameKind::Expression, frame.node->first()});
                    return true;
                }
                break;
            case ast::AstNodeKind::IndexExpression:
            {
                const sema::ExpressionInfo* baseInfo = expressionInfo(frame.node->first());
                if (baseInfo == nullptr)
                {
                    finishFrame(frames, completed, hasCompleted, nullptr);
                    return true;
                }
                frame.dimensionType = baseInfo->type;
                frame.state = 20;
                descendFrame(frames, hasCompleted, {
                    baseInfo->type->kind() == sema::TypeKind::Pointer ||
                        baseInfo->type->kind() == sema::TypeKind::StringLiteral
                        ? LoweringFrameKind::Expression : LoweringFrameKind::Address,
                    frame.node->first()});
                return true;
            }
            case ast::AstNodeKind::MemberExpression:
            case ast::AstNodeKind::PointerMemberExpression:
                if (frame.info->symbol == nullptr ||
                    frame.info->symbol->kind() != sema::SymbolKind::Field)
                {
                    fail("addressable member expression has no resolved field symbol");
                    finishFrame(frames, completed, hasCompleted, nullptr);
                    return true;
                }
                frame.state = 30;
                descendFrame(frames, hasCompleted, {
                    frame.node->kind() == ast::AstNodeKind::PointerMemberExpression
                        ? LoweringFrameKind::Expression : LoweringFrameKind::Address,
                    frame.node->first()});
                return true;
            default:
                break;
        }
        fail("expression classified as lvalue has no HIR address lowering");
        finishFrame(frames, completed, hasCompleted, nullptr);
        return true;
    }
    return false;
}

bool IterativeHirLowering::finishDereferenceAddressFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 10 && hasCompleted)
    {
        HirNode* pointer = takeCompletedFrame(completed, hasCompleted);
        if (pointer == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        HirNode* address = arena_.make(
            HirNodeKind::DereferenceAddress, frame.node->range());
        address->setType(pointer->type());
        address->setFirst(pointer);
        finishFrame(frames, completed, hasCompleted, address);
        return true;
    }
    return false;
}

bool IterativeHirLowering::finishIndexAddressFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 20 && hasCompleted)
    {
        frame.result = takeCompletedFrame(completed, hasCompleted);
        const std::size_t indexCount = !frame.node->children().empty()
            ? frame.node->children().size() : frame.node->second() != nullptr ? 1U : 0U;
        if (frame.result == nullptr || indexCount == 0)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        frame.state = 21;
        ast::AstNode* indexSyntax = !frame.node->children().empty()
            ? frame.node->children().front() : frame.node->second();
        descendFrame(frames, hasCompleted, {LoweringFrameKind::Expression, indexSyntax});
        return true;
    }
    if (frame.state == 21 && hasCompleted)
    {
        HirNode* index = takeCompletedFrame(completed, hasCompleted);
        if (index == nullptr || frame.dimensionType == nullptr ||
            (frame.dimensionType->kind() != sema::TypeKind::Array &&
             frame.dimensionType->kind() != sema::TypeKind::Pointer &&
             frame.dimensionType->kind() != sema::TypeKind::StringLiteral))
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        sema::Type* elementType =
            frame.dimensionType->kind() == sema::TypeKind::StringLiteral
                ? frame.info->type : frame.dimensionType->elementType();
        HirNode* address = arena_.make(HirNodeKind::IndexAddress, frame.node->range());
        address->setType(types_.pointerTo(elementType));
        address->setOperationType(frame.dimensionType);
        address->setRequiresBoundsCheck(
            frame.dimensionType->kind() == sema::TypeKind::Array ||
            frame.dimensionType->kind() == sema::TypeKind::StringLiteral);
        address->setFirst(frame.result);
        address->setSecond(index);
        frame.result = address;
        frame.dimensionType = elementType;
        ++frame.index;
        const std::size_t indexCount = !frame.node->children().empty()
            ? frame.node->children().size() : 1U;
        if (frame.index == indexCount)
        {
            finishFrame(frames, completed, hasCompleted, frame.result);
            return true;
        }
        ast::AstNode* indexSyntax = frame.node->children()[frame.index];
        descendFrame(frames, hasCompleted, {LoweringFrameKind::Expression, indexSyntax});
        return true;
    }
    return false;
}

bool IterativeHirLowering::finishMemberAddressFrame(
    LoweringFrame& frame,
    std::vector<LoweringFrame>& frames,
    HirNode*& completed,
    bool& hasCompleted)
{
    if (frame.state == 30 && hasCompleted)
    {
        HirNode* base = takeCompletedFrame(completed, hasCompleted);
        if (base == nullptr)
        {
            finishFrame(frames, completed, hasCompleted, nullptr);
            return true;
        }
        HirNode* address = arena_.make(HirNodeKind::FieldAddress, frame.node->range());
        address->setType(types_.pointerTo(frame.info->type));
        address->setSymbol(frame.info->symbol);
        if (frame.info->symbol->hasFieldOffset())
            address->setOffset(frame.info->symbol->fieldOffset());
        address->setFirst(base);
        finishFrame(frames, completed, hasCompleted, address);
        return true;
    }
    return false;
}

const sema::ExpressionInfo* IterativeHirLowering::expressionInfo(const ast::AstNode* node)
{
    const sema::ExpressionInfo* info = semanticModel_.get(node);
    if (info == nullptr || info->type == nullptr || info->type->kind() == sema::TypeKind::Error)
    {
        fail("HIR builder could not find valid semantic expression information");
        return nullptr;
    }
    return info;
}

sema::Symbol* IterativeHirLowering::boundSymbol(const ast::AstNode* node)
{
    sema::Symbol* symbol = semanticModel_.symbol(node);
    if (symbol == nullptr)
        fail("HIR builder could not find the symbol bound to a declaration");
    return symbol;
}

const sema::ResolvedOperatorInfo* IterativeHirLowering::operatorInfo(const ast::AstNode* node)
{
    const sema::ResolvedOperatorInfo* info = semanticModel_.getOperator(node);
    if (info == nullptr)
        fail("HIR builder could not find resolved operator information");
    return info;
}

const sema::ConversionInfo* IterativeHirLowering::operandConversion(
    const ast::AstNode* owner,
    const ast::AstNode* operand) const noexcept
{
    for (const sema::ConversionInfo& conversion : semanticModel_.conversions(owner))
    {
        if (conversion.role == sema::ConversionRole::Operand && conversion.operand == operand)
            return &conversion;
    }
    return nullptr;
}

const sema::ConversionInfo* IterativeHirLowering::resultConversion(const ast::AstNode* owner) const noexcept
{
    for (const sema::ConversionInfo& conversion : semanticModel_.conversions(owner))
    {
        if (conversion.role == sema::ConversionRole::Result)
            return &conversion;
    }
    return nullptr;
}

HirNode* IterativeHirLowering::wrapConversion(
    SourceRange range,
    HirNode* value,
    const sema::ConversionInfo& conversion)
{
    if (value == nullptr || conversion.destinationType == nullptr)
        return fail("invalid semantic conversion while building HIR");
    if (conversion.sourceType != nullptr && value->type() != conversion.sourceType)
    {
        return fail(
            "semantic conversion source type does not match lowered HIR operand type");
    }
    HirNode* result = arena_.make(HirNodeKind::Conversion, range);
    result->setType(conversion.destinationType);
    result->setConversionKind(conversion.kind);
    result->setFirst(value);
    return result;
}

HirNode* IterativeHirLowering::fail(std::string message)
{
    if (error_.empty())
        error_ = std::move(message);
    return nullptr;
}
}
