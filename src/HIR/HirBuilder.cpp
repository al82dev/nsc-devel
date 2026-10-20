#include "nativesharp/HIR/HirBuilder.h"

#include "Internal/LoweringFrame.h"
#include "Internal/IterativeHirLowering.h"

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
using internal::LoweringFrame;
using internal::LoweringFrameKind;

using internal::IterativeHirLowering;
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
