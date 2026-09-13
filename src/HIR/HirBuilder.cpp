#include "nativesharp/HIR/HirBuilder.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"
#include "nativesharp/Sema/Type.h"
#include "nativesharp/Sema/TypeContext.h"
#include "nativesharp/Sema/TypeKind.h"

namespace nativesharp::hir
{
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

    HirNode* root = arena_.make(HirNodeKind::CompilationUnit, compilationUnit->range());
    for (ast::AstNode* declaration : compilationUnit->children())
    {
        if (declaration->kind() == ast::AstNodeKind::ExternCDeclaration)
        {
            for (ast::AstNode* child : declaration->children())
            {
                HirNode* lowered = lowerFunction(child);
                if (lowered == nullptr)
                {
                    return nullptr;
                }
                root->addChild(lowered);
            }
            continue;
        }

        HirNode* lowered = lowerGlobal(declaration);
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
    switch (node->kind())
    {
        case ast::AstNodeKind::StructDeclaration:
            return lowerStruct(node);
        case ast::AstNodeKind::EnumDeclaration:
            return lowerEnum(node);
        case ast::AstNodeKind::FunctionDeclaration:
            return lowerFunction(node);
        case ast::AstNodeKind::GlobalVariableDeclaration:
            return lowerGlobalVariable(node);
        default:
            return fail("unsupported global AST node while building HIR");
    }
}

HirNode* HirBuilder::lowerStruct(ast::AstNode* node)
{
    sema::Symbol* symbol = boundSymbol(node);
    if (symbol == nullptr)
    {
        return nullptr;
    }

    HirNode* result = arena_.make(HirNodeKind::StructDeclaration, node->range());
    result->setSymbol(symbol);
    result->setType(symbol->type());

    for (ast::AstNode* fieldNode : node->children())
    {
        sema::Symbol* field = boundSymbol(fieldNode);
        if (field == nullptr)
        {
            return nullptr;
        }
        HirNode* fieldHir = arena_.make(HirNodeKind::FieldDeclaration, fieldNode->range());
        fieldHir->setSymbol(field);
        fieldHir->setType(field->type());
        if (field->hasFieldOffset())
        {
            fieldHir->setOffset(field->fieldOffset());
        }
        result->addChild(fieldHir);
    }
    return result;
}

HirNode* HirBuilder::lowerEnum(ast::AstNode* node)
{
    sema::Symbol* symbol = boundSymbol(node);
    if (symbol == nullptr)
    {
        return nullptr;
    }

    HirNode* result = arena_.make(HirNodeKind::EnumDeclaration, node->range());
    result->setSymbol(symbol);
    result->setType(symbol->type());

    for (ast::AstNode* memberNode : node->children())
    {
        sema::Symbol* member = boundSymbol(memberNode);
        if (member == nullptr)
        {
            return nullptr;
        }
        HirNode* memberHir = arena_.make(HirNodeKind::EnumMemberDeclaration, memberNode->range());
        memberHir->setSymbol(member);
        memberHir->setType(member->type());
        memberHir->setConstant(member->constantValue());
        result->addChild(memberHir);
    }
    return result;
}

HirNode* HirBuilder::lowerFunction(ast::AstNode* node)
{
    sema::Symbol* symbol = boundSymbol(node);
    if (symbol == nullptr)
    {
        return nullptr;
    }

    HirNode* result = arena_.make(HirNodeKind::FunctionDeclaration, node->range());
    result->setSymbol(symbol);
    result->setType(symbol->type());

    const std::vector<sema::Type*>& parameterTypes = symbol->type()->parameterTypes();
    for (std::size_t index = 0; index < node->children().size(); ++index)
    {
        ast::AstNode* parameterNode = node->children()[index];
        sema::Symbol* parameter = semanticModel_.symbol(parameterNode);
        if (parameter == nullptr && node->first() != nullptr)
        {
            return nullptr;
        }
        HirNode* parameterHir = arena_.make(HirNodeKind::ParameterDeclaration, parameterNode->range());
        parameterHir->setSymbol(parameter);
        parameterHir->setType(parameter != nullptr ? parameter->type() : parameterTypes[index]);
        result->addChild(parameterHir);
    }

    if (node->first() != nullptr)
    {
        HirNode* body = lowerBlock(node->first());
        if (body == nullptr)
        {
            return nullptr;
        }
        result->setFirst(body);
    }
    return result;
}

HirNode* HirBuilder::lowerGlobalVariable(ast::AstNode* node)
{
    sema::Symbol* symbol = boundSymbol(node);
    if (symbol == nullptr)
    {
        return nullptr;
    }

    HirNode* result = arena_.make(HirNodeKind::GlobalVariableDeclaration, node->range());
    result->setSymbol(symbol);
    result->setType(symbol->type());
    if (node->first() != nullptr)
    {
        HirNode* initializer = lowerConvertedOperand(node, node->first());
        if (initializer == nullptr)
        {
            return nullptr;
        }
        result->setFirst(initializer);
    }
    return result;
}

HirNode* HirBuilder::lowerBlock(ast::AstNode* node)
{
    HirNode* result = arena_.make(HirNodeKind::Block, node->range());
    for (ast::AstNode* statement : node->children())
    {
        HirNode* lowered = lowerStatement(statement);
        if (lowered == nullptr)
        {
            return nullptr;
        }
        result->addChild(lowered);
    }
    return result;
}

HirNode* HirBuilder::lowerStatement(ast::AstNode* node)
{
    switch (node->kind())
    {
        case ast::AstNodeKind::BlockStatement:
            return lowerBlock(node);

        case ast::AstNodeKind::VariableDeclarationStatement:
            return lowerLocalVariable(node);

        case ast::AstNodeKind::IfStatement:
        {
            HirNode* result = arena_.make(HirNodeKind::If, node->range());
            result->setFirst(lowerExpression(node->first()));
            result->setSecond(lowerBlock(node->second()));
            if (node->third() != nullptr)
            {
                result->setThird(
                    node->third()->kind() == ast::AstNodeKind::BlockStatement
                        ? lowerBlock(node->third())
                        : lowerStatement(node->third()));
            }
            return result->first() == nullptr || result->second() == nullptr ? nullptr : result;
        }

        case ast::AstNodeKind::WhileStatement:
        {
            HirNode* result = arena_.make(HirNodeKind::While, node->range());
            result->setFirst(lowerExpression(node->first()));
            result->setSecond(lowerBlock(node->second()));
            return result->first() == nullptr || result->second() == nullptr ? nullptr : result;
        }

        case ast::AstNodeKind::ForStatement:
        {
            HirNode* result = arena_.make(HirNodeKind::For, node->range());
            if (node->first() != nullptr)
            {
                result->setFirst(
                    node->first()->kind() == ast::AstNodeKind::VariableDeclarationStatement
                        ? lowerLocalVariable(node->first())
                        : lowerExpression(node->first(), false));
            }
            if (node->second() != nullptr)
            {
                result->setSecond(lowerExpression(node->second()));
            }
            if (node->third() != nullptr)
            {
                result->setThird(lowerExpression(node->third(), false));
            }
            if (!node->children().empty())
            {
                result->addChild(lowerBlock(node->children().front()));
            }
            if ((!node->children().empty() && result->children().front() == nullptr) ||
                (node->first() != nullptr && result->first() == nullptr) ||
                (node->second() != nullptr && result->second() == nullptr) ||
                (node->third() != nullptr && result->third() == nullptr))
            {
                return nullptr;
            }
            return result;
        }

        case ast::AstNodeKind::BreakStatement:
            return arena_.make(HirNodeKind::Break, node->range());

        case ast::AstNodeKind::ContinueStatement:
            return arena_.make(HirNodeKind::Continue, node->range());

        case ast::AstNodeKind::ReturnStatement:
        {
            HirNode* result = arena_.make(HirNodeKind::Return, node->range());
            if (node->first() != nullptr)
            {
                result->setFirst(lowerConvertedOperand(node, node->first()));
                if (result->first() == nullptr)
                {
                    return nullptr;
                }
                result->setType(result->first()->type());
            }
            else
            {
                result->setType(types_.builtin(sema::TypeKind::Void));
            }
            return result;
        }

        case ast::AstNodeKind::ExpressionStatement:
        {
            HirNode* result = arena_.make(HirNodeKind::ExpressionStatement, node->range());
            result->setFirst(lowerExpression(node->first(), false));
            return result->first() == nullptr ? nullptr : result;
        }

        case ast::AstNodeKind::StaticAssertStatement:
            return arena_.make(HirNodeKind::Block, node->range());

        default:
            return fail("unsupported statement AST node while building HIR");
    }
}

HirNode* HirBuilder::lowerLocalVariable(ast::AstNode* node)
{
    sema::Symbol* symbol = boundSymbol(node);
    if (symbol == nullptr)
    {
        return nullptr;
    }

    HirNode* result = arena_.make(HirNodeKind::LocalVariableDeclaration, node->range());
    result->setSymbol(symbol);
    result->setType(symbol->type());
    if (node->first() != nullptr)
    {
        result->setFirst(lowerConvertedOperand(node, node->first()));
        if (result->first() == nullptr)
        {
            return nullptr;
        }
    }
    return result;
}

HirNode* HirBuilder::lowerExpression(ast::AstNode* node, bool loadLValue)
{
    if (node == nullptr)
    {
        return fail("null expression reached HIR builder");
    }

    const sema::ExpressionInfo* info = expressionInfo(node);
    if (info == nullptr)
    {
        return nullptr;
    }

    if (info->category == sema::ValueCategory::LValue)
    {
        HirNode* address = lowerAddress(node);
        if (address == nullptr || !loadLValue)
        {
            return address;
        }

        HirNode* load = arena_.make(HirNodeKind::Load, node->range());
        load->setType(info->type);
        load->setFirst(address);
        return load;
    }

    if (info->symbol != nullptr && info->symbol->kind() == sema::SymbolKind::EnumMember)
    {
        HirNode* constant = arena_.make(HirNodeKind::Constant, node->range());
        constant->setType(info->type);
        constant->setConstant(info->constantValue);
        constant->setSymbol(info->symbol);
        return constant;
    }

    switch (node->kind())
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
            if (!info->isConstant)
            {
                return fail("constant HIR expression has no semantic constant value");
            }
            HirNode* constant = arena_.make(HirNodeKind::Constant, node->range());
            constant->setType(info->type);
            constant->setConstant(info->constantValue);
            return constant;
        }

        case ast::AstNodeKind::IdentifierExpression:
            return fail("non-lvalue identifier reached HIR without enum-member or call context");

        case ast::AstNodeKind::UnaryExpression:
        case ast::AstNodeKind::PostfixExpression:
        {
            if (node->tokenKind() == lexer::TokenKind::Amp)
            {
                return lowerAddress(node->first());
            }

            if (node->tokenKind() == lexer::TokenKind::PlusPlus ||
                node->tokenKind() == lexer::TokenKind::MinusMinus)
            {
                HirNode* address = lowerAddress(node->first());
                const sema::ResolvedOperatorInfo* resolved = operatorInfo(node);
                if (address == nullptr || resolved == nullptr)
                {
                    return nullptr;
                }
                HirNode* increment = arena_.make(HirNodeKind::Increment, node->range());
                increment->setType(info->type);
                increment->setOperationType(resolved->operationType);
                increment->setOperation(node->tokenKind());
                increment->setPostfix(node->kind() == ast::AstNodeKind::PostfixExpression);
                increment->setFirst(address);
                return increment;
            }

            HirNode* operand = lowerConvertedOperand(node, node->first(), 0);
            const sema::ResolvedOperatorInfo* resolved = operatorInfo(node);
            if (operand == nullptr || resolved == nullptr)
            {
                return nullptr;
            }
            HirNode* unary = arena_.make(HirNodeKind::Unary, node->range());
            unary->setType(info->type);
            unary->setOperationType(resolved->operationType);
            unary->setOperation(node->tokenKind());
            unary->setFirst(operand);
            return unary;
        }

        case ast::AstNodeKind::BinaryExpression:
        {
            HirNode* left = lowerConvertedOperand(node, node->first(), 0);
            HirNode* right = lowerConvertedOperand(node, node->second(), 1);
            const sema::ResolvedOperatorInfo* resolved = operatorInfo(node);
            if (left == nullptr || right == nullptr || resolved == nullptr)
            {
                return nullptr;
            }
            HirNode* binary = arena_.make(HirNodeKind::Binary, node->range());
            binary->setType(info->type);
            binary->setOperationType(resolved->operationType);
            binary->setOperation(node->tokenKind());
            binary->setFirst(left);
            binary->setSecond(right);
            return binary;
        }

        case ast::AstNodeKind::AssignmentExpression:
        {
            HirNode* address = lowerAddress(node->first());
            if (address == nullptr)
            {
                return nullptr;
            }

            if (node->tokenKind() == lexer::TokenKind::Equal)
            {
                HirNode* value = lowerConvertedOperand(node, node->second(), 1);
                if (value == nullptr)
                {
                    return nullptr;
                }
                HirNode* store = arena_.make(HirNodeKind::Store, node->range());
                store->setType(info->type);
                store->setFirst(address);
                store->setSecond(value);
                return store;
            }

            HirNode* right = lowerConvertedOperand(node, node->second(), 1);
            const sema::ResolvedOperatorInfo* resolved = operatorInfo(node);
            if (right == nullptr || resolved == nullptr)
            {
                return nullptr;
            }

            HirNode* compound = arena_.make(HirNodeKind::CompoundAssignment, node->range());
            compound->setType(info->type);
            compound->setOperationType(resolved->operationType);
            compound->setOperation(resolved->tokenKind);
            compound->setFirst(address);
            compound->setSecond(right);

            const sema::ConversionInfo* read = operandConversion(node, node->first());
            if (read != nullptr)
            {
                compound->setReadConversionKind(read->kind);
            }
            const sema::ConversionInfo* result = resultConversion(node);
            if (result != nullptr)
            {
                compound->setResultConversionKind(result->kind);
            }
            return compound;
        }

        case ast::AstNodeKind::CallExpression:
        {
            const sema::ExpressionInfo* calleeInfo = expressionInfo(node->first());
            if (calleeInfo == nullptr || calleeInfo->symbol == nullptr)
            {
                return fail("call expression has no resolved function symbol");
            }
            HirNode* call = arena_.make(HirNodeKind::Call, node->range());
            call->setType(info->type);
            call->setSymbol(calleeInfo->symbol);
            for (std::size_t index = 0; index < node->children().size(); ++index)
            {
                HirNode* argument = lowerConvertedOperand(
                    node,
                    node->children()[index],
                    static_cast<int>(index));
                if (argument == nullptr)
                {
                    return nullptr;
                }
                call->addChild(argument);
            }
            return call;
        }

        case ast::AstNodeKind::MemberExpression:
        case ast::AstNodeKind::PointerMemberExpression:
        {
            if (info->category == sema::ValueCategory::RValue)
            {
                HirNode* base = lowerExpression(node->first());
                if (base == nullptr || info->symbol == nullptr)
                {
                    return fail("rvalue member access has no resolved field");
                }
                HirNode* extract = arena_.make(HirNodeKind::ExtractField, node->range());
                extract->setType(info->type);
                extract->setSymbol(info->symbol);
                if (info->symbol->hasFieldOffset())
                {
                    extract->setOffset(info->symbol->fieldOffset());
                }
                extract->setFirst(base);
                return extract;
            }
            return fail("lvalue member unexpectedly bypassed generic HIR lvalue lowering");
        }

        case ast::AstNodeKind::CastExpression:
            return lowerConvertedOperand(node, node->first(), 0);

        case ast::AstNodeKind::NewExpression:
        {
            HirNode* allocation = arena_.make(HirNodeKind::Allocation, node->range());
            allocation->setType(info->type);
            allocation->setOperationType(info->type->elementType());
            allocation->setArrayForm(node->flag());
            if (node->first() != nullptr)
            {
                allocation->setFirst(lowerExpression(node->first()));
                if (allocation->first() == nullptr)
                {
                    return nullptr;
                }
            }
            return allocation;
        }

        case ast::AstNodeKind::DeleteExpression:
        {
            HirNode* deallocation = arena_.make(HirNodeKind::Deallocation, node->range());
            deallocation->setType(info->type);
            deallocation->setArrayForm(node->flag());
            deallocation->setFirst(lowerExpression(node->first()));
            return deallocation->first() == nullptr ? nullptr : deallocation;
        }

        case ast::AstNodeKind::StackallocExpression:
        {
            HirNode* allocation = arena_.make(HirNodeKind::StackAllocation, node->range());
            allocation->setType(info->type);
            allocation->setOperationType(info->type->elementType());
            allocation->setFirst(lowerExpression(node->first()));
            return allocation->first() == nullptr ? nullptr : allocation;
        }

        default:
            return fail("unsupported expression AST node while building HIR");
    }
}

HirNode* HirBuilder::lowerAddress(ast::AstNode* node)
{
    const sema::ExpressionInfo* info = expressionInfo(node);
    if (info == nullptr)
    {
        return nullptr;
    }

    switch (node->kind())
    {
        case ast::AstNodeKind::IdentifierExpression:
        {
            if (info->symbol == nullptr)
            {
                return fail("addressable identifier has no resolved symbol");
            }
            HirNode* address = arena_.make(HirNodeKind::SymbolAddress, node->range());
            address->setType(types_.pointerTo(info->type));
            address->setSymbol(info->symbol);
            return address;
        }

        case ast::AstNodeKind::UnaryExpression:
            if (node->tokenKind() == lexer::TokenKind::Star)
            {
                HirNode* pointer = lowerExpression(node->first());
                if (pointer == nullptr)
                {
                    return nullptr;
                }
                HirNode* address = arena_.make(HirNodeKind::DereferenceAddress, node->range());
                address->setType(pointer->type());
                address->setFirst(pointer);
                return address;
            }
            break;

        case ast::AstNodeKind::IndexExpression:
        {
            const sema::ExpressionInfo* baseInfo = expressionInfo(node->first());
            if (baseInfo == nullptr)
            {
                return nullptr;
            }

            std::vector<ast::AstNode*> indices;
            if (!node->children().empty())
                indices.assign(node->children().begin(), node->children().end());
            else if (node->second() != nullptr)
                indices.push_back(node->second());

            HirNode* base = nullptr;
            if (baseInfo->type->kind() == sema::TypeKind::Pointer ||
                baseInfo->type->kind() == sema::TypeKind::StringLiteral)
            {
                base = lowerExpression(node->first());
            }
            else
            {
                base = lowerAddress(node->first());
            }

            if (base == nullptr || indices.empty())
            {
                return nullptr;
            }

            sema::Type* dimensionType = baseInfo->type;
            HirNode* address = nullptr;
            for (ast::AstNode* indexSyntax : indices)
            {
                HirNode* index = lowerExpression(indexSyntax);
                if (index == nullptr || dimensionType == nullptr ||
                    (dimensionType->kind() != sema::TypeKind::Array &&
                     dimensionType->kind() != sema::TypeKind::Pointer &&
                     dimensionType->kind() != sema::TypeKind::StringLiteral))
                {
                    return nullptr;
                }
                sema::Type* elementType = dimensionType->kind() == sema::TypeKind::StringLiteral
                    ? info->type : dimensionType->elementType();
                address = arena_.make(HirNodeKind::IndexAddress, node->range());
                address->setType(types_.pointerTo(elementType));
                address->setOperationType(dimensionType);
                address->setRequiresBoundsCheck(
                    dimensionType->kind() == sema::TypeKind::Array ||
                    dimensionType->kind() == sema::TypeKind::StringLiteral);
                address->setFirst(base);
                address->setSecond(index);
                base = address;
                dimensionType = elementType;
            }
            return address;
        }

        case ast::AstNodeKind::MemberExpression:
        case ast::AstNodeKind::PointerMemberExpression:
        {
            if (info->symbol == nullptr || info->symbol->kind() != sema::SymbolKind::Field)
            {
                return fail("addressable member expression has no resolved field symbol");
            }

            HirNode* base = node->kind() == ast::AstNodeKind::PointerMemberExpression
                ? lowerExpression(node->first())
                : lowerAddress(node->first());
            if (base == nullptr)
            {
                return nullptr;
            }

            HirNode* address = arena_.make(HirNodeKind::FieldAddress, node->range());
            address->setType(types_.pointerTo(info->type));
            address->setSymbol(info->symbol);
            if (info->symbol->hasFieldOffset())
            {
                address->setOffset(info->symbol->fieldOffset());
            }
            address->setFirst(base);
            return address;
        }

        default:
            break;
    }

    return fail("expression classified as lvalue has no HIR address lowering");
}

HirNode* HirBuilder::lowerConvertedOperand(
    const ast::AstNode* owner,
    ast::AstNode* operand,
    int operandIndex)
{
    HirNode* value = lowerExpression(operand);
    if (value == nullptr)
    {
        return nullptr;
    }

    for (const sema::ConversionInfo& conversion : semanticModel_.conversions(owner))
    {
        if (conversion.role == sema::ConversionRole::Result || conversion.operand != operand)
        {
            continue;
        }
        if (operandIndex >= 0 && conversion.operandIndex >= 0 && conversion.operandIndex != operandIndex)
        {
            continue;
        }
        value = wrapConversion(owner->range(), value, conversion);
        if (value == nullptr)
        {
            return nullptr;
        }
    }
    return value;
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
