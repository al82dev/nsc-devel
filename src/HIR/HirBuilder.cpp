#include "nativesharp/HIR/HirBuilder.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"
#include "nativesharp/Sema/Type.h"
#include "nativesharp/Sema/TypeContext.h"
#include "nativesharp/Sema/TypeKind.h"

#include <utility>
#include <vector>

namespace nativesharp::hir
{
namespace
{
enum class LoweringFrameKind
{
    Global,
    Struct,
    Enum,
    Function,
    GlobalVariable,
    Block,
    Statement,
    LocalVariable,
    Expression,
    Address,
    ConvertedOperand
};

struct LoweringFrame final
{
    LoweringFrameKind kind;
    ast::AstNode* node;
    const ast::AstNode* owner = nullptr;
    int operandIndex = -1;
    bool loadLValue = true;
    std::size_t state = 0;
    std::size_t index = 0;
    HirNode* result = nullptr;
    HirNode* first = nullptr;
    HirNode* second = nullptr;
    const sema::ExpressionInfo* info = nullptr;
    sema::Type* dimensionType = nullptr;
};

/// Executes the base AST-to-HIR lowering with heap-backed continuation frames.
class IterativeHirLowering final
{
public:
    IterativeHirLowering(
        HirArena& arena,
        sema::TypeContext& types,
        const sema::SemanticModel& semanticModel,
        std::string& error)
        : arena_(arena), types_(types), semanticModel_(semanticModel), error_(error)
    {
        frames_.reserve(128);
    }

    HirNode* lowerGlobal(ast::AstNode* node)
    {
        return run({LoweringFrameKind::Global, node});
    }

    HirNode* lowerStruct(ast::AstNode* node)
    {
        return run({LoweringFrameKind::Struct, node});
    }

    HirNode* lowerEnum(ast::AstNode* node)
    {
        return run({LoweringFrameKind::Enum, node});
    }

    HirNode* lowerFunction(ast::AstNode* node)
    {
        return run({LoweringFrameKind::Function, node});
    }

    HirNode* lowerGlobalVariable(ast::AstNode* node)
    {
        return run({LoweringFrameKind::GlobalVariable, node});
    }

    HirNode* lowerBlock(ast::AstNode* node)
    {
        return run({LoweringFrameKind::Block, node});
    }

    HirNode* lowerStatement(ast::AstNode* node)
    {
        return run({LoweringFrameKind::Statement, node});
    }

    HirNode* lowerLocalVariable(ast::AstNode* node)
    {
        return run({LoweringFrameKind::LocalVariable, node});
    }

    HirNode* lowerExpression(ast::AstNode* node, bool loadLValue)
    {
        LoweringFrame frame{LoweringFrameKind::Expression, node};
        frame.loadLValue = loadLValue;
        return run(std::move(frame));
    }

    HirNode* lowerAddress(ast::AstNode* node)
    {
        return run({LoweringFrameKind::Address, node});
    }

    HirNode* lowerConvertedOperand(
        const ast::AstNode* owner,
        ast::AstNode* operand,
        int operandIndex)
    {
        LoweringFrame frame{LoweringFrameKind::ConvertedOperand, operand};
        frame.owner = owner;
        frame.operandIndex = operandIndex;
        return run(std::move(frame));
    }

private:
    HirNode* run(LoweringFrame initial)
    {
        std::vector<LoweringFrame>& frames = frames_;
        frames.clear();
        frames.push_back(std::move(initial));

        HirNode* completed = nullptr;
        bool hasCompleted = false;
        auto finish = [&](HirNode* result)
        {
            frames.pop_back();
            completed = result;
            hasCompleted = true;
        };
        auto takeCompleted = [&]()
        {
            HirNode* result = completed;
            completed = nullptr;
            hasCompleted = false;
            return result;
        };
        auto descend = [&](LoweringFrame child)
        {
            hasCompleted = false;
            frames.push_back(std::move(child));
        };

        while (!frames.empty())
        {
            LoweringFrame& frame = frames.back();
            switch (frame.kind)
            {
                case LoweringFrameKind::Global:
                    switch (frame.node->kind())
                    {
                        case ast::AstNodeKind::StructDeclaration:
                            frame.kind = LoweringFrameKind::Struct;
                            continue;
                        case ast::AstNodeKind::EnumDeclaration:
                            frame.kind = LoweringFrameKind::Enum;
                            continue;
                        case ast::AstNodeKind::FunctionDeclaration:
                            frame.kind = LoweringFrameKind::Function;
                            continue;
                        case ast::AstNodeKind::GlobalVariableDeclaration:
                            frame.kind = LoweringFrameKind::GlobalVariable;
                            continue;
                        default:
                            fail("unsupported global AST node while building HIR");
                            finish(nullptr);
                            continue;
                    }

                case LoweringFrameKind::Struct:
                {
                    sema::Symbol* symbol = boundSymbol(frame.node);
                    if (symbol == nullptr)
                    {
                        finish(nullptr);
                        continue;
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
                    finish(result);
                    continue;
                }

                case LoweringFrameKind::Enum:
                {
                    sema::Symbol* symbol = boundSymbol(frame.node);
                    if (symbol == nullptr)
                    {
                        finish(nullptr);
                        continue;
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
                    finish(result);
                    continue;
                }

                case LoweringFrameKind::Function:
                    if (frame.state == 0)
                    {
                        sema::Symbol* symbol = boundSymbol(frame.node);
                        if (symbol == nullptr)
                        {
                            finish(nullptr);
                            continue;
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
                            finish(frame.result);
                            continue;
                        }
                        frame.state = 1;
                        descend({LoweringFrameKind::Block, frame.node->first()});
                        continue;
                    }
                    if (hasCompleted)
                    {
                        HirNode* body = takeCompleted();
                        if (body == nullptr)
                        {
                            finish(nullptr);
                            continue;
                        }
                        frame.result->setFirst(body);
                        finish(frame.result);
                        continue;
                    }
                    break;

                case LoweringFrameKind::GlobalVariable:
                    if (frame.state == 0)
                    {
                        sema::Symbol* symbol = boundSymbol(frame.node);
                        if (symbol == nullptr)
                        {
                            finish(nullptr);
                            continue;
                        }
                        frame.result = arena_.make(
                            HirNodeKind::GlobalVariableDeclaration, frame.node->range());
                        frame.result->setSymbol(symbol);
                        frame.result->setType(symbol->type());
                        if (frame.node->first() == nullptr)
                        {
                            finish(frame.result);
                            continue;
                        }
                        frame.state = 1;
                        LoweringFrame child{
                            LoweringFrameKind::ConvertedOperand, frame.node->first()};
                        child.owner = frame.node;
                        descend(std::move(child));
                        continue;
                    }
                    if (hasCompleted)
                    {
                        HirNode* initializer = takeCompleted();
                        if (initializer == nullptr)
                        {
                            finish(nullptr);
                            continue;
                        }
                        frame.result->setFirst(initializer);
                        finish(frame.result);
                        continue;
                    }
                    break;

                case LoweringFrameKind::Block:
                    if (frame.state == 0)
                    {
                        frame.result = arena_.make(HirNodeKind::Block, frame.node->range());
                        frame.state = 1;
                    }
                    if (hasCompleted)
                    {
                        HirNode* statement = takeCompleted();
                        if (statement == nullptr)
                        {
                            finish(nullptr);
                            continue;
                        }
                        frame.result->addChild(statement);
                        ++frame.index;
                    }
                    if (frame.index == frame.node->children().size())
                    {
                        finish(frame.result);
                        continue;
                    }
                    descend({LoweringFrameKind::Statement, frame.node->children()[frame.index]});
                    continue;

                case LoweringFrameKind::Statement:
                    if (lowerStatementFrame(frame, frames, completed, hasCompleted))
                        continue;
                    break;

                case LoweringFrameKind::LocalVariable:
                    if (frame.state == 0)
                    {
                        sema::Symbol* symbol = boundSymbol(frame.node);
                        if (symbol == nullptr)
                        {
                            finish(nullptr);
                            continue;
                        }
                        frame.result = arena_.make(
                            HirNodeKind::LocalVariableDeclaration, frame.node->range());
                        frame.result->setSymbol(symbol);
                        frame.result->setType(symbol->type());
                        if (frame.node->first() == nullptr)
                        {
                            finish(frame.result);
                            continue;
                        }
                        frame.state = 1;
                        LoweringFrame child{
                            LoweringFrameKind::ConvertedOperand, frame.node->first()};
                        child.owner = frame.node;
                        descend(std::move(child));
                        continue;
                    }
                    if (hasCompleted)
                    {
                        HirNode* initializer = takeCompleted();
                        if (initializer == nullptr)
                        {
                            finish(nullptr);
                            continue;
                        }
                        frame.result->setFirst(initializer);
                        finish(frame.result);
                        continue;
                    }
                    break;

                case LoweringFrameKind::Expression:
                    if (lowerExpressionFrame(frame, frames, completed, hasCompleted))
                        continue;
                    break;

                case LoweringFrameKind::Address:
                    if (lowerAddressFrame(frame, frames, completed, hasCompleted))
                        continue;
                    break;

                case LoweringFrameKind::ConvertedOperand:
                    if (frame.state == 0)
                    {
                        frame.state = 1;
                        descend({LoweringFrameKind::Expression, frame.node});
                        continue;
                    }
                    if (hasCompleted)
                    {
                        HirNode* value = takeCompleted();
                        if (value == nullptr)
                        {
                            finish(nullptr);
                            continue;
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
                        finish(value);
                        continue;
                    }
                    break;
            }

            finish(nullptr);
        }
        return hasCompleted ? completed : nullptr;
    }

    bool lowerStatementFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted)
    {
        auto takeCompleted = [&]()
        {
            HirNode* result = completed;
            completed = nullptr;
            hasCompleted = false;
            return result;
        };
        auto finish = [&](HirNode* result)
        {
            frames.pop_back();
            completed = result;
            hasCompleted = true;
        };
        auto descend = [&](LoweringFrame child)
        {
            hasCompleted = false;
            frames.push_back(std::move(child));
        };

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
                    descend({LoweringFrameKind::Expression, frame.node->first()});
                    return true;
                case ast::AstNodeKind::WhileStatement:
                    frame.result = arena_.make(HirNodeKind::While, frame.node->range());
                    frame.state = 20;
                    descend({LoweringFrameKind::Expression, frame.node->first()});
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
                        descend(std::move(child));
                    }
                    return true;
                case ast::AstNodeKind::BreakStatement:
                    finish(arena_.make(HirNodeKind::Break, frame.node->range()));
                    return true;
                case ast::AstNodeKind::ContinueStatement:
                    finish(arena_.make(HirNodeKind::Continue, frame.node->range()));
                    return true;
                case ast::AstNodeKind::ReturnStatement:
                    frame.result = arena_.make(HirNodeKind::Return, frame.node->range());
                    if (frame.node->first() == nullptr)
                    {
                        frame.result->setType(types_.builtin(sema::TypeKind::Void));
                        finish(frame.result);
                    }
                    else
                    {
                        frame.state = 40;
                        LoweringFrame child{
                            LoweringFrameKind::ConvertedOperand, frame.node->first()};
                        child.owner = frame.node;
                        descend(std::move(child));
                    }
                    return true;
                case ast::AstNodeKind::ExpressionStatement:
                {
                    frame.result = arena_.make(
                        HirNodeKind::ExpressionStatement, frame.node->range());
                    frame.state = 50;
                    LoweringFrame child{LoweringFrameKind::Expression, frame.node->first()};
                    child.loadLValue = false;
                    descend(std::move(child));
                    return true;
                }
                case ast::AstNodeKind::StaticAssertStatement:
                    finish(arena_.make(HirNodeKind::Block, frame.node->range()));
                    return true;
                default:
                    fail("unsupported statement AST node while building HIR");
                    finish(nullptr);
                    return true;
            }
        }

        if (frame.state == 10 && hasCompleted)
        {
            frame.result->setFirst(takeCompleted());
            frame.state = 11;
            descend({LoweringFrameKind::Block, frame.node->second()});
            return true;
        }
        if (frame.state == 11 && hasCompleted)
        {
            frame.result->setSecond(takeCompleted());
            if (frame.node->third() == nullptr)
            {
                finish(frame.result->first() == nullptr || frame.result->second() == nullptr
                    ? nullptr : frame.result);
                return true;
            }
            frame.state = 12;
            descend({
                frame.node->third()->kind() == ast::AstNodeKind::BlockStatement
                    ? LoweringFrameKind::Block : LoweringFrameKind::Statement,
                frame.node->third()});
            return true;
        }
        if (frame.state == 12 && hasCompleted)
        {
            frame.result->setThird(takeCompleted());
            finish(frame.result->first() == nullptr || frame.result->second() == nullptr
                ? nullptr : frame.result);
            return true;
        }

        if (frame.state == 20 && hasCompleted)
        {
            frame.result->setFirst(takeCompleted());
            frame.state = 21;
            descend({LoweringFrameKind::Block, frame.node->second()});
            return true;
        }
        if (frame.state == 21 && hasCompleted)
        {
            frame.result->setSecond(takeCompleted());
            finish(frame.result->first() == nullptr || frame.result->second() == nullptr
                ? nullptr : frame.result);
            return true;
        }

        if (frame.state == 30)
        {
            if (frame.node->first() != nullptr)
            {
                if (!hasCompleted)
                    return false;
                frame.result->setFirst(takeCompleted());
            }
            frame.state = 31;
            if (frame.node->second() != nullptr)
                descend({LoweringFrameKind::Expression, frame.node->second()});
            return true;
        }
        if (frame.state == 31)
        {
            if (frame.node->second() != nullptr)
            {
                if (!hasCompleted)
                    return false;
                frame.result->setSecond(takeCompleted());
            }
            frame.state = 32;
            if (frame.node->third() != nullptr)
            {
                LoweringFrame child{LoweringFrameKind::Expression, frame.node->third()};
                child.loadLValue = false;
                descend(std::move(child));
            }
            return true;
        }
        if (frame.state == 32)
        {
            if (frame.node->third() != nullptr)
            {
                if (!hasCompleted)
                    return false;
                frame.result->setThird(takeCompleted());
            }
            frame.state = 33;
            if (!frame.node->children().empty())
                descend({LoweringFrameKind::Block, frame.node->children().front()});
            return true;
        }
        if (frame.state == 33)
        {
            if (!frame.node->children().empty())
            {
                if (!hasCompleted)
                    return false;
                frame.result->addChild(takeCompleted());
            }
            const bool failed =
                (!frame.node->children().empty() && frame.result->children().front() == nullptr) ||
                (frame.node->first() != nullptr && frame.result->first() == nullptr) ||
                (frame.node->second() != nullptr && frame.result->second() == nullptr) ||
                (frame.node->third() != nullptr && frame.result->third() == nullptr);
            finish(failed ? nullptr : frame.result);
            return true;
        }

        if (frame.state == 40 && hasCompleted)
        {
            HirNode* value = takeCompleted();
            if (value == nullptr)
            {
                finish(nullptr);
                return true;
            }
            frame.result->setFirst(value);
            frame.result->setType(value->type());
            finish(frame.result);
            return true;
        }
        if (frame.state == 50 && hasCompleted)
        {
            frame.result->setFirst(takeCompleted());
            finish(frame.result->first() == nullptr ? nullptr : frame.result);
            return true;
        }
        return false;
    }

    bool lowerExpressionFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted)
    {
        auto takeCompleted = [&]()
        {
            HirNode* result = completed;
            completed = nullptr;
            hasCompleted = false;
            return result;
        };
        auto finish = [&](HirNode* result)
        {
            frames.pop_back();
            completed = result;
            hasCompleted = true;
        };
        auto descend = [&](LoweringFrame child)
        {
            hasCompleted = false;
            frames.push_back(std::move(child));
        };

        if (frame.state == 0)
        {
            if (frame.node == nullptr)
            {
                fail("null expression reached HIR builder");
                finish(nullptr);
                return true;
            }
            frame.info = expressionInfo(frame.node);
            if (frame.info == nullptr)
            {
                finish(nullptr);
                return true;
            }
            if (!sema::isUsableValueCategory(frame.info->category))
            {
                fail("NS3020: moved-from expression reached HIR lowering");
                finish(nullptr);
                return true;
            }
            if (sema::isPlaceCategory(frame.info->category))
            {
                frame.state = 1;
                descend({LoweringFrameKind::Address, frame.node});
                return true;
            }
            if (frame.info->symbol != nullptr &&
                frame.info->symbol->kind() == sema::SymbolKind::EnumMember)
            {
                HirNode* constant = arena_.make(HirNodeKind::Constant, frame.node->range());
                constant->setType(frame.info->type);
                constant->setConstant(frame.info->constantValue);
                constant->setSymbol(frame.info->symbol);
                finish(constant);
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
                {
                    if (!frame.info->isConstant)
                    {
                        fail("constant HIR expression has no semantic constant value");
                        finish(nullptr);
                        return true;
                    }
                    HirNode* constant = arena_.make(HirNodeKind::Constant, frame.node->range());
                    constant->setType(frame.info->type);
                    constant->setConstant(frame.info->constantValue);
                    finish(constant);
                    return true;
                }
                case ast::AstNodeKind::IdentifierExpression:
                    fail("non-lvalue identifier reached HIR without enum-member or call context");
                    finish(nullptr);
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
                        descend({LoweringFrameKind::Address, frame.node->first()});
                    }
                    else
                    {
                        frame.state = 11;
                        LoweringFrame child{
                            LoweringFrameKind::ConvertedOperand, frame.node->first()};
                        child.owner = frame.node;
                        child.operandIndex = 0;
                        descend(std::move(child));
                    }
                    return true;
                case ast::AstNodeKind::BinaryExpression:
                {
                    frame.state = 20;
                    LoweringFrame child{
                        LoweringFrameKind::ConvertedOperand, frame.node->first()};
                    child.owner = frame.node;
                    child.operandIndex = 0;
                    descend(std::move(child));
                    return true;
                }
                case ast::AstNodeKind::ConditionalExpression:
                    frame.state = 30;
                    descend({LoweringFrameKind::Expression, frame.node->first()});
                    return true;
                case ast::AstNodeKind::AssignmentExpression:
                    frame.state = 40;
                    descend({LoweringFrameKind::Address, frame.node->first()});
                    return true;
                case ast::AstNodeKind::CallExpression:
                {
                    const sema::ExpressionInfo* calleeInfo = expressionInfo(frame.node->first());
                    if (calleeInfo == nullptr || calleeInfo->symbol == nullptr)
                    {
                        fail("call expression has no resolved function symbol");
                        finish(nullptr);
                        return true;
                    }
                    frame.result = arena_.make(HirNodeKind::Call, frame.node->range());
                    frame.result->setType(frame.info->type);
                    frame.result->setSymbol(calleeInfo->symbol);
                    frame.state = 50;
                    if (frame.node->children().empty())
                    {
                        finish(frame.result);
                        return true;
                    }
                    LoweringFrame child{
                        LoweringFrameKind::ConvertedOperand, frame.node->children().front()};
                    child.owner = frame.node;
                    child.operandIndex = 0;
                    descend(std::move(child));
                    return true;
                }
                case ast::AstNodeKind::MemberExpression:
                case ast::AstNodeKind::PointerMemberExpression:
                    if (!sema::isPlaceCategory(frame.info->category))
                    {
                        frame.state = 60;
                        descend({LoweringFrameKind::Expression, frame.node->first()});
                    }
                    else
                    {
                        fail("lvalue member unexpectedly bypassed generic HIR lvalue lowering");
                        finish(nullptr);
                    }
                    return true;
                case ast::AstNodeKind::CastExpression:
                    frame.kind = LoweringFrameKind::ConvertedOperand;
                    frame.owner = frame.node;
                    frame.node = frame.node->first();
                    frame.operandIndex = 0;
                    frame.state = 0;
                    return true;
                case ast::AstNodeKind::NewExpression:
                    frame.result = arena_.make(HirNodeKind::Allocation, frame.node->range());
                    frame.result->setType(frame.info->type);
                    frame.result->setOperationType(frame.info->type->elementType());
                    frame.result->setArrayForm(frame.node->flag());
                    if (frame.node->first() == nullptr)
                    {
                        finish(frame.result);
                    }
                    else
                    {
                        frame.state = 70;
                        descend({LoweringFrameKind::Expression, frame.node->first()});
                    }
                    return true;
                case ast::AstNodeKind::DeleteExpression:
                    frame.result = arena_.make(HirNodeKind::Deallocation, frame.node->range());
                    frame.result->setType(frame.info->type);
                    frame.result->setArrayForm(frame.node->flag());
                    frame.state = 71;
                    descend({LoweringFrameKind::Expression, frame.node->first()});
                    return true;
                case ast::AstNodeKind::StackallocExpression:
                    frame.result = arena_.make(HirNodeKind::StackAllocation, frame.node->range());
                    frame.result->setType(frame.info->type);
                    frame.result->setOperationType(frame.info->type->elementType());
                    frame.state = 72;
                    descend({LoweringFrameKind::Expression, frame.node->first()});
                    return true;
                default:
                    fail("unsupported expression AST node while building HIR");
                    finish(nullptr);
                    return true;
            }
        }

        if (frame.state == 1 && hasCompleted)
        {
            HirNode* address = takeCompleted();
            if (address == nullptr || !frame.loadLValue)
            {
                finish(address);
                return true;
            }
            HirNode* load = arena_.make(HirNodeKind::Load, frame.node->range());
            load->setType(frame.info->type);
            load->setFirst(address);
            finish(load);
            return true;
        }
        if ((frame.state == 10 || frame.state == 11) && hasCompleted)
        {
            HirNode* operand = takeCompleted();
            const sema::ResolvedOperatorInfo* resolved = operatorInfo(frame.node);
            if (operand == nullptr || resolved == nullptr)
            {
                finish(nullptr);
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
            finish(result);
            return true;
        }

        if (frame.state == 20 && hasCompleted)
        {
            frame.first = takeCompleted();
            frame.state = 21;
            LoweringFrame child{
                LoweringFrameKind::ConvertedOperand, frame.node->second()};
            child.owner = frame.node;
            child.operandIndex = 1;
            descend(std::move(child));
            return true;
        }
        if (frame.state == 21 && hasCompleted)
        {
            frame.second = takeCompleted();
            const sema::ResolvedOperatorInfo* resolved = operatorInfo(frame.node);
            if (frame.first == nullptr || frame.second == nullptr || resolved == nullptr)
            {
                finish(nullptr);
                return true;
            }
            HirNode* binary = arena_.make(HirNodeKind::Binary, frame.node->range());
            binary->setType(frame.info->type);
            binary->setOperationType(resolved->operationType);
            binary->setOperation(frame.node->tokenKind());
            binary->setFirst(frame.first);
            binary->setSecond(frame.second);
            finish(binary);
            return true;
        }

        if (frame.state == 30 && hasCompleted)
        {
            frame.result = takeCompleted();
            frame.state = 31;
            LoweringFrame child{
                LoweringFrameKind::ConvertedOperand, frame.node->second()};
            child.owner = frame.node;
            child.operandIndex = 1;
            descend(std::move(child));
            return true;
        }
        if (frame.state == 31 && hasCompleted)
        {
            frame.first = takeCompleted();
            frame.state = 32;
            LoweringFrame child{
                LoweringFrameKind::ConvertedOperand, frame.node->third()};
            child.owner = frame.node;
            child.operandIndex = 2;
            descend(std::move(child));
            return true;
        }
        if (frame.state == 32 && hasCompleted)
        {
            frame.second = takeCompleted();
            if (frame.result == nullptr || frame.first == nullptr || frame.second == nullptr)
            {
                finish(nullptr);
                return true;
            }
            HirNode* conditional = arena_.make(HirNodeKind::Conditional, frame.node->range());
            conditional->setType(frame.info->type);
            conditional->setFirst(frame.result);
            conditional->setSecond(frame.first);
            conditional->setThird(frame.second);
            finish(conditional);
            return true;
        }

        if (frame.state == 40 && hasCompleted)
        {
            frame.first = takeCompleted();
            if (frame.first == nullptr)
            {
                finish(nullptr);
                return true;
            }
            frame.state = frame.node->tokenKind() == lexer::TokenKind::Equal ? 41 : 42;
            LoweringFrame child{
                LoweringFrameKind::ConvertedOperand, frame.node->second()};
            child.owner = frame.node;
            child.operandIndex = 1;
            descend(std::move(child));
            return true;
        }
        if ((frame.state == 41 || frame.state == 42) && hasCompleted)
        {
            HirNode* right = takeCompleted();
            if (frame.state == 41)
            {
                if (right == nullptr)
                {
                    finish(nullptr);
                    return true;
                }
                HirNode* store = arena_.make(HirNodeKind::Store, frame.node->range());
                store->setType(frame.info->type);
                store->setFirst(frame.first);
                store->setSecond(right);
                finish(store);
                return true;
            }
            const sema::ResolvedOperatorInfo* resolved = operatorInfo(frame.node);
            if (right == nullptr || resolved == nullptr)
            {
                finish(nullptr);
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
            finish(compound);
            return true;
        }

        if (frame.state == 50 && hasCompleted)
        {
            HirNode* argument = takeCompleted();
            if (argument == nullptr)
            {
                finish(nullptr);
                return true;
            }
            frame.result->addChild(argument);
            ++frame.index;
            if (frame.index == frame.node->children().size())
            {
                finish(frame.result);
                return true;
            }
            LoweringFrame child{
                LoweringFrameKind::ConvertedOperand, frame.node->children()[frame.index]};
            child.owner = frame.node;
            child.operandIndex = static_cast<int>(frame.index);
            descend(std::move(child));
            return true;
        }

        if (frame.state == 60 && hasCompleted)
        {
            HirNode* base = takeCompleted();
            if (base == nullptr || frame.info->symbol == nullptr)
            {
                fail("rvalue member access has no resolved field");
                finish(nullptr);
                return true;
            }
            HirNode* extract = arena_.make(HirNodeKind::ExtractField, frame.node->range());
            extract->setType(frame.info->type);
            extract->setSymbol(frame.info->symbol);
            if (frame.info->symbol->hasFieldOffset())
                extract->setOffset(frame.info->symbol->fieldOffset());
            extract->setFirst(base);
            finish(extract);
            return true;
        }

        if ((frame.state == 70 || frame.state == 71 || frame.state == 72) && hasCompleted)
        {
            frame.result->setFirst(takeCompleted());
            finish(frame.result->first() == nullptr ? nullptr : frame.result);
            return true;
        }
        return false;
    }

    bool lowerAddressFrame(
        LoweringFrame& frame,
        std::vector<LoweringFrame>& frames,
        HirNode*& completed,
        bool& hasCompleted)
    {
        auto takeCompleted = [&]()
        {
            HirNode* result = completed;
            completed = nullptr;
            hasCompleted = false;
            return result;
        };
        auto finish = [&](HirNode* result)
        {
            frames.pop_back();
            completed = result;
            hasCompleted = true;
        };
        auto descend = [&](LoweringFrame child)
        {
            hasCompleted = false;
            frames.push_back(std::move(child));
        };

        if (frame.state == 0)
        {
            frame.info = expressionInfo(frame.node);
            if (frame.info == nullptr)
            {
                finish(nullptr);
                return true;
            }
            switch (frame.node->kind())
            {
                case ast::AstNodeKind::IdentifierExpression:
                {
                    if (frame.info->symbol == nullptr)
                    {
                        fail("addressable identifier has no resolved symbol");
                        finish(nullptr);
                        return true;
                    }
                    HirNode* address = arena_.make(
                        HirNodeKind::SymbolAddress, frame.node->range());
                    address->setType(types_.pointerTo(frame.info->type));
                    address->setSymbol(frame.info->symbol);
                    finish(address);
                    return true;
                }
                case ast::AstNodeKind::UnaryExpression:
                    if (frame.node->tokenKind() == lexer::TokenKind::Star)
                    {
                        frame.state = 10;
                        descend({LoweringFrameKind::Expression, frame.node->first()});
                        return true;
                    }
                    break;
                case ast::AstNodeKind::IndexExpression:
                {
                    const sema::ExpressionInfo* baseInfo = expressionInfo(frame.node->first());
                    if (baseInfo == nullptr)
                    {
                        finish(nullptr);
                        return true;
                    }
                    frame.dimensionType = baseInfo->type;
                    frame.state = 20;
                    descend({
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
                        finish(nullptr);
                        return true;
                    }
                    frame.state = 30;
                    descend({
                        frame.node->kind() == ast::AstNodeKind::PointerMemberExpression
                            ? LoweringFrameKind::Expression : LoweringFrameKind::Address,
                        frame.node->first()});
                    return true;
                default:
                    break;
            }
            fail("expression classified as lvalue has no HIR address lowering");
            finish(nullptr);
            return true;
        }

        if (frame.state == 10 && hasCompleted)
        {
            HirNode* pointer = takeCompleted();
            if (pointer == nullptr)
            {
                finish(nullptr);
                return true;
            }
            HirNode* address = arena_.make(
                HirNodeKind::DereferenceAddress, frame.node->range());
            address->setType(pointer->type());
            address->setFirst(pointer);
            finish(address);
            return true;
        }

        if (frame.state == 20 && hasCompleted)
        {
            frame.result = takeCompleted();
            const std::size_t indexCount = !frame.node->children().empty()
                ? frame.node->children().size() : frame.node->second() != nullptr ? 1U : 0U;
            if (frame.result == nullptr || indexCount == 0)
            {
                finish(nullptr);
                return true;
            }
            frame.state = 21;
            ast::AstNode* indexSyntax = !frame.node->children().empty()
                ? frame.node->children().front() : frame.node->second();
            descend({LoweringFrameKind::Expression, indexSyntax});
            return true;
        }
        if (frame.state == 21 && hasCompleted)
        {
            HirNode* index = takeCompleted();
            if (index == nullptr || frame.dimensionType == nullptr ||
                (frame.dimensionType->kind() != sema::TypeKind::Array &&
                 frame.dimensionType->kind() != sema::TypeKind::Pointer &&
                 frame.dimensionType->kind() != sema::TypeKind::StringLiteral))
            {
                finish(nullptr);
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
                finish(frame.result);
                return true;
            }
            ast::AstNode* indexSyntax = frame.node->children()[frame.index];
            descend({LoweringFrameKind::Expression, indexSyntax});
            return true;
        }

        if (frame.state == 30 && hasCompleted)
        {
            HirNode* base = takeCompleted();
            if (base == nullptr)
            {
                finish(nullptr);
                return true;
            }
            HirNode* address = arena_.make(HirNodeKind::FieldAddress, frame.node->range());
            address->setType(types_.pointerTo(frame.info->type));
            address->setSymbol(frame.info->symbol);
            if (frame.info->symbol->hasFieldOffset())
                address->setOffset(frame.info->symbol->fieldOffset());
            address->setFirst(base);
            finish(address);
            return true;
        }
        return false;
    }

    const sema::ExpressionInfo* expressionInfo(const ast::AstNode* node)
    {
        const sema::ExpressionInfo* info = semanticModel_.get(node);
        if (info == nullptr || info->type == nullptr || info->type->kind() == sema::TypeKind::Error)
        {
            fail("HIR builder could not find valid semantic expression information");
            return nullptr;
        }
        return info;
    }

    sema::Symbol* boundSymbol(const ast::AstNode* node)
    {
        sema::Symbol* symbol = semanticModel_.symbol(node);
        if (symbol == nullptr)
            fail("HIR builder could not find the symbol bound to a declaration");
        return symbol;
    }

    const sema::ResolvedOperatorInfo* operatorInfo(const ast::AstNode* node)
    {
        const sema::ResolvedOperatorInfo* info = semanticModel_.getOperator(node);
        if (info == nullptr)
            fail("HIR builder could not find resolved operator information");
        return info;
    }

    const sema::ConversionInfo* operandConversion(
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

    const sema::ConversionInfo* resultConversion(const ast::AstNode* owner) const noexcept
    {
        for (const sema::ConversionInfo& conversion : semanticModel_.conversions(owner))
        {
            if (conversion.role == sema::ConversionRole::Result)
                return &conversion;
        }
        return nullptr;
    }

    HirNode* wrapConversion(
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

    HirNode* fail(std::string message)
    {
        if (error_.empty())
            error_ = std::move(message);
        return nullptr;
    }

    HirArena& arena_;
    sema::TypeContext& types_;
    const sema::SemanticModel& semanticModel_;
    std::string& error_;
    std::vector<LoweringFrame> frames_;
};
}

HirBuilder::HirBuilder(
    HirArena& arena,
    sema::TypeContext& types,
    const sema::SemanticModel& semanticModel)
    : arena_(arena), types_(types), semanticModel_(semanticModel)
{
}

HirNode* HirBuilder::build(ast::AstNode* compilationUnit)
{
    if (compilationUnit == nullptr || compilationUnit->kind() != ast::AstNodeKind::CompilationUnit)
    {
        return fail("HIR builder requires a CompilationUnit AST root");
    }

    IterativeHirLowering lowering(arena_, types_, semanticModel_, error_);
    HirNode* root = arena_.make(HirNodeKind::CompilationUnit, compilationUnit->range());
    for (ast::AstNode* declaration : compilationUnit->children())
    {
        if (declaration->kind() == ast::AstNodeKind::ExternCDeclaration)
        {
            for (ast::AstNode* child : declaration->children())
            {
                HirNode* lowered = lowering.lowerFunction(child);
                if (lowered == nullptr)
                {
                    return nullptr;
                }
                root->addChild(lowered);
            }
            continue;
        }

        HirNode* lowered = lowering.lowerGlobal(declaration);
        if (lowered == nullptr)
        {
            return nullptr;
        }
        root->addChild(lowered);
    }
    return root;
}

bool HirBuilder::succeeded() const noexcept
{
    return error_.empty();
}

const std::string& HirBuilder::error() const noexcept
{
    return error_;
}

HirNode* HirBuilder::lowerGlobal(ast::AstNode* node)
{
    return IterativeHirLowering(arena_, types_, semanticModel_, error_).lowerGlobal(node);
}

HirNode* HirBuilder::lowerStruct(ast::AstNode* node)
{
    return IterativeHirLowering(arena_, types_, semanticModel_, error_).lowerStruct(node);
}

HirNode* HirBuilder::lowerEnum(ast::AstNode* node)
{
    return IterativeHirLowering(arena_, types_, semanticModel_, error_).lowerEnum(node);
}

HirNode* HirBuilder::lowerFunction(ast::AstNode* node)
{
    return IterativeHirLowering(arena_, types_, semanticModel_, error_).lowerFunction(node);
}

HirNode* HirBuilder::lowerGlobalVariable(ast::AstNode* node)
{
    return IterativeHirLowering(arena_, types_, semanticModel_, error_).lowerGlobalVariable(node);
}

HirNode* HirBuilder::lowerBlock(ast::AstNode* node)
{
    return IterativeHirLowering(arena_, types_, semanticModel_, error_).lowerBlock(node);
}

HirNode* HirBuilder::lowerStatement(ast::AstNode* node)
{
    return IterativeHirLowering(arena_, types_, semanticModel_, error_).lowerStatement(node);
}

HirNode* HirBuilder::lowerLocalVariable(ast::AstNode* node)
{
    return IterativeHirLowering(arena_, types_, semanticModel_, error_).lowerLocalVariable(node);
}

HirNode* HirBuilder::lowerExpression(ast::AstNode* node, bool loadLValue)
{
    return IterativeHirLowering(arena_, types_, semanticModel_, error_)
        .lowerExpression(node, loadLValue);
}

HirNode* HirBuilder::lowerAddress(ast::AstNode* node)
{
    return IterativeHirLowering(arena_, types_, semanticModel_, error_).lowerAddress(node);
}

HirNode* HirBuilder::lowerConvertedOperand(
    const ast::AstNode* owner,
    ast::AstNode* operand,
    int operandIndex)
{
    return IterativeHirLowering(arena_, types_, semanticModel_, error_)
        .lowerConvertedOperand(owner, operand, operandIndex);
}

HirNode* HirBuilder::wrapConversion(
    SourceRange range,
    HirNode* value,
    const sema::ConversionInfo& conversion)
{
    if (value == nullptr || conversion.destinationType == nullptr)
    {
        return fail("invalid semantic conversion while building HIR");
    }
    if (conversion.sourceType != nullptr && value->type() != conversion.sourceType)
    {
        return fail("semantic conversion source type does not match lowered HIR operand type");
    }

    HirNode* result = arena_.make(HirNodeKind::Conversion, range);
    result->setType(conversion.destinationType);
    result->setConversionKind(conversion.kind);
    result->setFirst(value);
    return result;
}

const sema::ExpressionInfo* HirBuilder::expressionInfo(const ast::AstNode* node)
{
    const sema::ExpressionInfo* info = semanticModel_.get(node);
    if (info == nullptr || info->type == nullptr || info->type->kind() == sema::TypeKind::Error)
    {
        fail("HIR builder could not find valid semantic expression information");
        return nullptr;
    }
    return info;
}

sema::Symbol* HirBuilder::boundSymbol(const ast::AstNode* node)
{
    sema::Symbol* symbol = semanticModel_.symbol(node);
    if (symbol == nullptr)
    {
        fail("HIR builder could not find the symbol bound to a declaration");
    }
    return symbol;
}

const sema::ResolvedOperatorInfo* HirBuilder::operatorInfo(const ast::AstNode* node)
{
    const sema::ResolvedOperatorInfo* info = semanticModel_.getOperator(node);
    if (info == nullptr)
    {
        fail("HIR builder could not find resolved operator information");
    }
    return info;
}

const sema::ConversionInfo* HirBuilder::operandConversion(
    const ast::AstNode* owner,
    const ast::AstNode* operand) const noexcept
{
    for (const sema::ConversionInfo& conversion : semanticModel_.conversions(owner))
    {
        if (conversion.role == sema::ConversionRole::Operand && conversion.operand == operand)
        {
            return &conversion;
        }
    }
    return nullptr;
}

const sema::ConversionInfo* HirBuilder::resultConversion(const ast::AstNode* owner) const noexcept
{
    for (const sema::ConversionInfo& conversion : semanticModel_.conversions(owner))
    {
        if (conversion.role == sema::ConversionRole::Result)
        {
            return &conversion;
        }
    }
    return nullptr;
}

HirNode* HirBuilder::fail(std::string message)
{
    if (error_.empty())
    {
        error_ = std::move(message);
    }
    return nullptr;
}
}
