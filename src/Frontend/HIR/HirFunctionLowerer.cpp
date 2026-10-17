#include "HirLoweringSupport.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <vector>

namespace nativesharp::frontend
{
std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerBlock(
    const ast::AstNode* block,
    std::size_t memberOrdinal,
    std::string& error)
{
    if (block == nullptr || block->kind() != ast::AstNodeKind::BlockStatement)
    {
        error = "Stage 34 OOP HIR requires block statement body";
        return nullptr;
    }

    struct BlockFrame final
    {
        const ast::AstNode* syntax = nullptr;
        std::unique_ptr<ProjectOopHirNode> result;
        std::size_t cleanupDepthAtEntry = 0;
        std::size_t statementIndex = 0;
    };
    const auto makeFrame = [&](const ast::AstNode* syntax)
    {
        BlockFrame frame;
        frame.syntax = syntax;
        frame.result = makeNode(ProjectOopHirNodeKind::Block, syntax, "builtin:void");
        frame.cleanupDepthAtEntry = activeHirCleanupRegions_.size();
        return frame;
    };

    const std::size_t rootCleanupDepth = activeHirCleanupRegions_.size();
    std::vector<BlockFrame> pending;
    pending.push_back(makeFrame(block));
    while (!pending.empty())
    {
        BlockFrame& frame = pending.back();
        if (frame.statementIndex >= frame.syntax->children().size())
        {
            activeHirCleanupRegions_.resize(frame.cleanupDepthAtEntry);
            std::unique_ptr<ProjectOopHirNode> completed = std::move(frame.result);
            pending.pop_back();
            if (pending.empty()) return completed;
            pending.back().result->children.push_back(std::move(completed));
            continue;
        }

        const ast::AstNode* statement =
            frame.syntax->children()[frame.statementIndex++];
        if (statement != nullptr && statement->kind() == ast::AstNodeKind::BlockStatement)
        {
            pending.push_back(makeFrame(statement));
            continue;
        }

        std::unique_ptr<ProjectOopHirNode> lowered =
            lowerStatement(statement, memberOrdinal, error);
        if (lowered == nullptr)
        {
            activeHirCleanupRegions_.resize(rootCleanupDepth);
            return nullptr;
        }
        if (lowered->kind == ProjectOopHirNodeKind::LocalDeclaration &&
            lowered->cleanupRegionOrdinal.has_value())
        {
            activeHirCleanupRegions_.push_back(*lowered->cleanupRegionOrdinal);
        }
        frame.result->children.push_back(std::move(lowered));
    }
    return nullptr;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerReceiver(
    const ast::AstNode* receiverSyntax,
    std::string_view receiverTypeKey,
    std::size_t memberOrdinal,
    std::string& error)
{
    if (receiverTypeKey.empty())
    {
        error = "Stage 40 receiver has no semantic class/interface/struct type";
        return nullptr;
    }
    if (receiverSyntax == nullptr || receiverSyntax->kind() == ast::AstNodeKind::BaseExpression)
    {
        if (currentLambda_ != nullptr)
        {
            const std::optional<std::size_t> capture = currentLambdaThisCapture();
            if (!capture.has_value())
            {
                error = "Stage 43 implicit instance receiver has no captured this";
                return nullptr;
            }
            auto node = makeNode(
                isStructValueType(receiverTypeKey)
                    ? ProjectOopHirNodeKind::ClosureCaptureAddress
                    : ProjectOopHirNodeKind::ClosureCaptureValue,
                receiverSyntax,
                std::string(receiverTypeKey));
            node->integerValue = *capture;
            return node;
        }
        return makeNode(ProjectOopHirNodeKind::ThisValue, receiverSyntax, std::string(receiverTypeKey));
    }
    // Struct instance methods receive the address of existing value storage.  Unlike a class
    // receiver, lowering a struct receiver as a value here would copy it and mutations of `this`
    // would be lost.  Stage 40 therefore requires an addressable receiver and preserves that
    // address explicitly into MIR/LLVM.
    //
    // C6 "struct members": ... and a struct temporary has no address until one is made for it.
    // Before C6 that did not come up much, because a struct expression was almost always a local
    // or a field. Operators change that completely -- `(a + b).x`, `Cross(u, v).z`,
    // `(-v).LengthSquared`, `new Vector3(1,2,3)[0]` are ordinary math, and every one of them
    // reaches a member of a value that lives nowhere. Measured before this: the operator path had
    // no addressability check at all and produced IR the verifier rejected ("GEP base pointer is
    // not a vector", GEPing into a loaded `[12 x i8]`), while the call path reported "temporary
    // struct values are not addressable".
    //
    // So a temporary receiver is MATERIALIZED: given a home, and the member reached through it.
    // Anything that is naturally addressable keeps its own address, unchanged -- this adds a slot
    // only where there was none to begin with.
    if (isStructValueType(receiverTypeKey) && !isNaturallyAddressable(receiverSyntax))
    {
        auto value = lowerExpression(receiverSyntax, memberOrdinal, false, error);
        if (value == nullptr) return nullptr;
        auto node = makeNode(
            ProjectOopHirNodeKind::TemporaryAddress, receiverSyntax, std::string(receiverTypeKey));
        node->elementTypeKey = std::string(receiverTypeKey);
        node->children.push_back(std::move(value));
        return node;
    }
    return lowerExpression(receiverSyntax, memberOrdinal, isStructValueType(receiverTypeKey), error);
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerDefaultArgument(
    const ast::AstNode* syntax,
    std::string_view parameterTypeKey,
    std::string& error)
{
    if (syntax == nullptr)
    {
        error = "NSC-LIMIT-020 selected optional parameter has no default expression";
        return nullptr;
    }
    const std::string target(parameterTypeKey);
    std::vector<const ast::AstNode*> unaryFrames;
    const ast::AstNode* current = syntax;
    while (current != nullptr && current->kind() == ast::AstNodeKind::UnaryExpression)
    {
        unaryFrames.push_back(current);
        current = current->first();
    }
    if (current == nullptr)
    {
        error = "NSC-LIMIT-020 selected optional parameter has no default expression";
        return nullptr;
    }

    std::unique_ptr<ProjectOopHirNode> value;
    switch (current->kind())
    {
        case ast::AstNodeKind::IntegerLiteralExpression:
        case ast::AstNodeKind::CharacterLiteralExpression:
            if (target == "builtin:float" || target == "builtin:double")
            {
                value = makeNode(ProjectOopHirNodeKind::FloatingConstant, current, target);
                value->floatingValue = static_cast<double>(current->integerValue());
            }
            else
            {
                value = makeNode(ProjectOopHirNodeKind::IntegerConstant, current, target);
                value->integerValue = current->integerValue();
            }
            break;
        case ast::AstNodeKind::FloatingLiteralExpression:
            value = makeNode(ProjectOopHirNodeKind::FloatingConstant, current, target);
            value->floatingValue = current->floatingValue();
            break;
        case ast::AstNodeKind::BooleanLiteralExpression:
            value = makeNode(ProjectOopHirNodeKind::BooleanConstant, current, "builtin:bool");
            value->booleanValue = current->tokenKind() == lexer::TokenKind::KwTrue;
            break;
        case ast::AstNodeKind::StringLiteralExpression:
        {
            value = makeNode(ProjectOopHirNodeKind::StringLiteral, current, "string-literal");
            value->stringValue = current->text();
            const ProjectImplicitConversion conversion =
                classifyImplicit("string-literal", target, std::nullopt);
            if (!conversion.exists())
            {
                error = "NSC-LIMIT-020 string default cannot convert to parameter type '" +
                    target + "'";
                return nullptr;
            }
            if (conversion.kind != ProjectConversionKind::Identity)
            {
                value = wrapImplicitConversion(std::move(value), conversion, current);
            }
            break;
        }
        case ast::AstNodeKind::NullLiteralExpression:
            value = makeNode(ProjectOopHirNodeKind::NullConstant, current, target);
            break;
        default:
            error = "NSC-LIMIT-020 default argument is not a supported literal constant";
            return nullptr;
    }

    for (auto frame = unaryFrames.rbegin(); frame != unaryFrames.rend(); ++frame)
    {
        auto unary = makeNode(ProjectOopHirNodeKind::Unary, *frame, target);
        unary->operation = (*frame)->tokenKind();
        unary->children.push_back(std::move(value));
        value = std::move(unary);
    }
    return value;
}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerInstanceFieldInitialization(
    std::size_t fieldMemberOrdinal,
    std::optional<std::size_t> callerMemberOrdinal,
    std::string& error)
{
    if (memberModel_ == nullptr || inheritanceModel_ == nullptr ||
        fieldMemberOrdinal >= memberModel_->members().size())
    {
        error = "Stage 37 cannot lower an invalid instance field initializer";
        return nullptr;
    }
    const ClassMemberSymbol& field = memberModel_->members()[fieldMemberOrdinal];
    if (field.kind != ClassMemberKind::Field || field.isStatic || field.syntax == nullptr ||
        field.syntax->first() == nullptr)
    {
        error = "Stage 37 instance initializer helper received a non-instance field";
        return nullptr;
    }

    auto target = makeNode(ProjectOopHirNodeKind::FieldAddress, field.syntax, field.declaredTypeKey);
    target->memberOrdinal = fieldMemberOrdinal;
    target->volatileAccess = field.isVolatile;
    target->children.push_back(makeNode(
        ProjectOopHirNodeKind::ThisValue,
        field.syntax,
        inheritanceModel_->typeKeyForOrdinal(field.declaringTypeOrdinal)));

    auto value = lowerExpression(
        field.syntax->first(), callerMemberOrdinal.value_or(fieldMemberOrdinal), false, error);
    if (value == nullptr)
    {
        return nullptr;
    }
    const MethodExpressionTypeBinding* sourceType = expressionType(field.syntax->first());
    const ProjectImplicitConversion conversion = classifyImplicit(
        value->typeKey,
        field.declaredTypeKey,
        sourceType != nullptr ? sourceType->integerConstant : std::nullopt);
    if (!conversion.exists())
    {
        error = "Stage 37 field initializer lost its validated implicit conversion";
        return nullptr;
    }
    if (conversion.kind != ProjectConversionKind::Identity)
    {
        value = wrapImplicitConversion(std::move(value), conversion, field.syntax->first());
    }

    auto assignment = makeNode(ProjectOopHirNodeKind::Assignment, field.syntax, field.declaredTypeKey);
    assignment->operation = lexer::TokenKind::Equal;
    assignment->children.push_back(std::move(target));
    assignment->children.push_back(std::move(value));
    return assignment;
}

void ProjectOopHirModel::appendInstanceFieldInitializers(
    std::size_t typeOrdinal,
    std::vector<std::unique_ptr<ProjectOopHirNode>>& output,
    std::string& error,
    std::optional<std::size_t> callerMemberOrdinal)
{
    if (memberModel_ == nullptr)
    {
        error = "Stage 37 instance initializer lowering has no member model";
        return;
    }
    for (const std::size_t memberOrdinal : memberModel_->membersOfType(typeOrdinal))
    {
        if (memberOrdinal >= memberModel_->members().size())
        {
            continue;
        }
        const ClassMemberSymbol& field = memberModel_->members()[memberOrdinal];
        if (field.kind != ClassMemberKind::Field || field.isStatic || field.syntax == nullptr ||
            field.syntax->first() == nullptr)
        {
            continue;
        }
        auto initializer = lowerInstanceFieldInitialization(
            memberOrdinal, callerMemberOrdinal, error);
        if (initializer == nullptr)
        {
            return;
        }
        output.push_back(std::move(initializer));
    }
}

std::vector<std::size_t> ProjectOopHirModel::implicitInitializerTypes(
    std::size_t targetTypeOrdinal,
    std::optional<std::size_t> entryConstructorMemberOrdinal) const
{
    std::vector<std::size_t> reversePath;
    if (inheritanceModel_ == nullptr || memberModel_ == nullptr)
    {
        return reversePath;
    }

    std::optional<std::size_t> stopType;
    if (entryConstructorMemberOrdinal.has_value() &&
        *entryConstructorMemberOrdinal < memberModel_->members().size())
    {
        stopType = memberModel_->members()[*entryConstructorMemberOrdinal].declaringTypeOrdinal;
    }

    std::optional<std::size_t> current = targetTypeOrdinal;
    while (current.has_value() && (!stopType.has_value() || *current != *stopType))
    {
        reversePath.push_back(*current);
        current = inheritanceModel_->baseTypeOrdinal(*current);
    }
    std::reverse(reversePath.begin(), reversePath.end());
    return reversePath;
}
}
