#include "HirLoweringSupport.h"

#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectGenericConstraintModel.h"
#include "nativesharp/Frontend/ProjectMetaprogrammingModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
enum class StaticAssertConstantKind
{
    Integer,
    Floating,
    Boolean
};

struct StaticAssertConstant final
{
    StaticAssertConstantKind kind = StaticAssertConstantKind::Integer;
    std::string typeKey;
    std::uint64_t integerValue = 0;
    double floatingValue = 0.0;
    bool booleanValue = false;
};

std::optional<unsigned> staticAssertIntegerWidth(std::string_view typeKey) noexcept
{
    if (typeKey == "builtin:sbyte" || typeKey == "builtin:byte") return 8U;
    if (typeKey == "builtin:char" || typeKey == "builtin:short" || typeKey == "builtin:ushort") return 16U;
    if (typeKey == "builtin:int" || typeKey == "builtin:uint") return 32U;
    if (typeKey == "builtin:long" || typeKey == "builtin:ulong" ||
        typeKey == "builtin:nint" || typeKey == "builtin:nuint") return 64U;
    if (typeKey == "builtin:i128" || typeKey == "builtin:u128") return 128U;
    return std::nullopt;
}

bool staticAssertSignedInteger(std::string_view typeKey) noexcept
{
    return typeKey == "builtin:sbyte" || typeKey == "builtin:short" ||
        typeKey == "builtin:int" || typeKey == "builtin:long" || typeKey == "builtin:nint" ||
        typeKey == "builtin:i128";
}

std::uint64_t staticAssertMask(unsigned width) noexcept
{
    return width == 64U ? ~std::uint64_t{0} : ((std::uint64_t{1} << width) - 1U);
}

std::uint64_t staticAssertNormalize(std::uint64_t value, std::string_view typeKey) noexcept
{
    const std::optional<unsigned> width = staticAssertIntegerWidth(typeKey);
    return width.has_value() ? value & staticAssertMask(*width) : value;
}

std::int64_t staticAssertSignedValue(std::uint64_t value, std::string_view typeKey) noexcept
{
    const unsigned width = staticAssertIntegerWidth(typeKey).value_or(64U);
    value &= staticAssertMask(width);
    if (width == 64U) return std::bit_cast<std::int64_t>(value);
    const std::uint64_t sign = std::uint64_t{1} << (width - 1U);
    if ((value & sign) != 0U) value |= ~staticAssertMask(width);
    return std::bit_cast<std::int64_t>(value);
}

std::optional<StaticAssertConstant> evaluateStaticAssertConstant(const ProjectOopHirNode& node)
{
    const auto integer = [&](std::uint64_t value, std::string typeKey)
    {
        StaticAssertConstant result;
        result.kind = StaticAssertConstantKind::Integer;
        result.integerValue = staticAssertNormalize(value, typeKey);
        result.typeKey = std::move(typeKey);
        return std::optional<StaticAssertConstant>(std::move(result));
    };
    const auto floating = [&](double value, std::string typeKey)
    {
        StaticAssertConstant result;
        result.kind = StaticAssertConstantKind::Floating;
        result.floatingValue = value;
        result.typeKey = std::move(typeKey);
        return std::optional<StaticAssertConstant>(std::move(result));
    };
    const auto boolean = [&](bool value)
    {
        StaticAssertConstant result;
        result.kind = StaticAssertConstantKind::Boolean;
        result.booleanValue = value;
        result.typeKey = "builtin:bool";
        return std::optional<StaticAssertConstant>(std::move(result));
    };

    switch (node.kind)
    {
        case ProjectOopHirNodeKind::IntegerConstant:
            return integer(node.integerValue, node.typeKey);
        case ProjectOopHirNodeKind::FloatingConstant:
            return floating(node.floatingValue, node.typeKey);
        case ProjectOopHirNodeKind::BooleanConstant:
            return boolean(node.booleanValue);
        case ProjectOopHirNodeKind::Conversion:
        {
            if (node.children.size() != 1U) return std::nullopt;
            const std::optional<StaticAssertConstant> value =
                evaluateStaticAssertConstant(*node.children.front());
            if (!value.has_value()) return std::nullopt;
            if (node.typeKey == "builtin:bool")
                return value->kind == StaticAssertConstantKind::Boolean
                    ? boolean(value->booleanValue) : std::nullopt;
            if (node.typeKey == "builtin:float" || node.typeKey == "builtin:double")
            {
                if (value->kind == StaticAssertConstantKind::Floating)
                    return floating(
                        node.typeKey == "builtin:float"
                            ? static_cast<double>(static_cast<float>(value->floatingValue))
                            : value->floatingValue,
                        node.typeKey);
                if (value->kind != StaticAssertConstantKind::Integer) return std::nullopt;
                const double converted = staticAssertSignedInteger(value->typeKey)
                    ? static_cast<double>(staticAssertSignedValue(value->integerValue, value->typeKey))
                    : static_cast<double>(value->integerValue);
                return floating(
                    node.typeKey == "builtin:float"
                        ? static_cast<double>(static_cast<float>(converted)) : converted,
                    node.typeKey);
            }
            if (!staticAssertIntegerWidth(node.typeKey).has_value()) return std::nullopt;
            if (value->kind == StaticAssertConstantKind::Integer)
                return integer(value->integerValue, node.typeKey);
            if (value->kind != StaticAssertConstantKind::Floating ||
                !std::isfinite(value->floatingValue)) return std::nullopt;
            const long double converted = std::trunc(static_cast<long double>(value->floatingValue));
            if (staticAssertSignedInteger(node.typeKey))
            {
                const unsigned width = *staticAssertIntegerWidth(node.typeKey);
                const long double minimum = width == 64U
                    ? static_cast<long double>(std::numeric_limits<std::int64_t>::min())
                    : -static_cast<long double>(std::uint64_t{1} << (width - 1U));
                const long double maximum = width == 64U
                    ? static_cast<long double>(std::numeric_limits<std::int64_t>::max())
                    : static_cast<long double>((std::uint64_t{1} << (width - 1U)) - 1U);
                if (converted < minimum || converted > maximum) return std::nullopt;
                return integer(
                    std::bit_cast<std::uint64_t>(static_cast<std::int64_t>(converted)), node.typeKey);
            }
            const unsigned width = *staticAssertIntegerWidth(node.typeKey);
            const long double maximum = width == 64U
                ? static_cast<long double>(std::numeric_limits<std::uint64_t>::max())
                : static_cast<long double>(staticAssertMask(width));
            if (converted < 0.0L || converted > maximum) return std::nullopt;
            return integer(static_cast<std::uint64_t>(converted), node.typeKey);
        }
        case ProjectOopHirNodeKind::Unary:
        {
            if (node.children.size() != 1U) return std::nullopt;
            const std::optional<StaticAssertConstant> value =
                evaluateStaticAssertConstant(*node.children.front());
            if (!value.has_value()) return std::nullopt;
            switch (node.operation)
            {
                case lexer::TokenKind::Plus:
                    return value;
                case lexer::TokenKind::Minus:
                    if (value->kind == StaticAssertConstantKind::Floating)
                        return floating(-value->floatingValue, node.typeKey);
                    if (value->kind == StaticAssertConstantKind::Integer)
                        return integer(std::uint64_t{0} - value->integerValue, node.typeKey);
                    return std::nullopt;
                case lexer::TokenKind::Tilde:
                    return value->kind == StaticAssertConstantKind::Integer
                        ? integer(~value->integerValue, node.typeKey) : std::nullopt;
                case lexer::TokenKind::Bang:
                    return value->kind == StaticAssertConstantKind::Boolean
                        ? boolean(!value->booleanValue) : std::nullopt;
                default:
                    return std::nullopt;
            }
        }
        case ProjectOopHirNodeKind::Binary:
        {
            if (node.children.size() != 2U) return std::nullopt;
            const std::optional<StaticAssertConstant> left =
                evaluateStaticAssertConstant(*node.children[0]);
            if (!left.has_value()) return std::nullopt;
            if (node.operation == lexer::TokenKind::AmpAmp)
            {
                if (left->kind != StaticAssertConstantKind::Boolean) return std::nullopt;
                if (!left->booleanValue) return boolean(false);
            }
            if (node.operation == lexer::TokenKind::PipePipe)
            {
                if (left->kind != StaticAssertConstantKind::Boolean) return std::nullopt;
                if (left->booleanValue) return boolean(true);
            }
            const std::optional<StaticAssertConstant> right =
                evaluateStaticAssertConstant(*node.children[1]);
            if (!right.has_value()) return std::nullopt;
            if (node.operation == lexer::TokenKind::AmpAmp || node.operation == lexer::TokenKind::PipePipe)
            {
                if (right->kind != StaticAssertConstantKind::Boolean) return std::nullopt;
                return boolean(right->booleanValue);
            }

            const bool floatingOperands = left->kind == StaticAssertConstantKind::Floating &&
                right->kind == StaticAssertConstantKind::Floating;
            const bool integerOperands = left->kind == StaticAssertConstantKind::Integer &&
                right->kind == StaticAssertConstantKind::Integer;
            const bool booleanOperands = left->kind == StaticAssertConstantKind::Boolean &&
                right->kind == StaticAssertConstantKind::Boolean;
            if (node.operation == lexer::TokenKind::EqualEqual ||
                node.operation == lexer::TokenKind::BangEqual)
            {
                bool equal = false;
                if (floatingOperands) equal = left->floatingValue == right->floatingValue;
                else if (integerOperands) equal = left->integerValue == right->integerValue;
                else if (booleanOperands) equal = left->booleanValue == right->booleanValue;
                else return std::nullopt;
                return boolean(node.operation == lexer::TokenKind::EqualEqual ? equal : !equal);
            }
            if (floatingOperands)
            {
                switch (node.operation)
                {
                    case lexer::TokenKind::Less: return boolean(left->floatingValue < right->floatingValue);
                    case lexer::TokenKind::LessEqual: return boolean(left->floatingValue <= right->floatingValue);
                    case lexer::TokenKind::Greater: return boolean(left->floatingValue > right->floatingValue);
                    case lexer::TokenKind::GreaterEqual: return boolean(left->floatingValue >= right->floatingValue);
                    case lexer::TokenKind::Plus: return floating(left->floatingValue + right->floatingValue, node.typeKey);
                    case lexer::TokenKind::Minus: return floating(left->floatingValue - right->floatingValue, node.typeKey);
                    case lexer::TokenKind::Star: return floating(left->floatingValue * right->floatingValue, node.typeKey);
                    case lexer::TokenKind::Slash: return floating(left->floatingValue / right->floatingValue, node.typeKey);
                    case lexer::TokenKind::Percent: return floating(std::fmod(left->floatingValue, right->floatingValue), node.typeKey);
                    default: return std::nullopt;
                }
            }
            if (!integerOperands) return std::nullopt;
            const unsigned width = staticAssertIntegerWidth(left->typeKey).value_or(64U);
            const bool signedOperands = staticAssertSignedInteger(left->typeKey);
            const std::int64_t signedLeft = staticAssertSignedValue(left->integerValue, left->typeKey);
            const std::int64_t signedRight = staticAssertSignedValue(right->integerValue, right->typeKey);
            switch (node.operation)
            {
                case lexer::TokenKind::Less:
                    return boolean(signedOperands ? signedLeft < signedRight
                                                  : left->integerValue < right->integerValue);
                case lexer::TokenKind::LessEqual:
                    return boolean(signedOperands ? signedLeft <= signedRight
                                                  : left->integerValue <= right->integerValue);
                case lexer::TokenKind::Greater:
                    return boolean(signedOperands ? signedLeft > signedRight
                                                  : left->integerValue > right->integerValue);
                case lexer::TokenKind::GreaterEqual:
                    return boolean(signedOperands ? signedLeft >= signedRight
                                                  : left->integerValue >= right->integerValue);
                case lexer::TokenKind::Plus:
                    return integer(left->integerValue + right->integerValue, node.typeKey);
                case lexer::TokenKind::Minus:
                    return integer(left->integerValue - right->integerValue, node.typeKey);
                case lexer::TokenKind::Star:
                    return integer(left->integerValue * right->integerValue, node.typeKey);
                case lexer::TokenKind::Amp:
                    return integer(left->integerValue & right->integerValue, node.typeKey);
                case lexer::TokenKind::Pipe:
                    return integer(left->integerValue | right->integerValue, node.typeKey);
                case lexer::TokenKind::Caret:
                    return integer(left->integerValue ^ right->integerValue, node.typeKey);
                case lexer::TokenKind::LessLess:
                    if (right->integerValue >= width) return std::nullopt;
                    return integer(left->integerValue << right->integerValue, node.typeKey);
                case lexer::TokenKind::GreaterGreater:
                    if (right->integerValue >= width) return std::nullopt;
                    return signedOperands
                        ? integer(
                            std::bit_cast<std::uint64_t>(signedLeft >> right->integerValue), node.typeKey)
                        : integer(left->integerValue >> right->integerValue, node.typeKey);
                case lexer::TokenKind::Slash:
                case lexer::TokenKind::Percent:
                    if (right->integerValue == 0U) return std::nullopt;
                    if (!signedOperands)
                        return integer(
                            node.operation == lexer::TokenKind::Slash
                                ? left->integerValue / right->integerValue
                                : left->integerValue % right->integerValue,
                            node.typeKey);
                    if (signedLeft == std::numeric_limits<std::int64_t>::min() && signedRight == -1)
                        return integer(
                            node.operation == lexer::TokenKind::Slash
                                ? left->integerValue : std::uint64_t{0},
                            node.typeKey);
                    return integer(
                        std::bit_cast<std::uint64_t>(
                            node.operation == lexer::TokenKind::Slash
                                ? signedLeft / signedRight : signedLeft % signedRight),
                        node.typeKey);
                default:
                    return std::nullopt;
            }
        }
        case ProjectOopHirNodeKind::Conditional:
        {
            if (node.children.size() != 3U) return std::nullopt;
            const std::optional<StaticAssertConstant> condition =
                evaluateStaticAssertConstant(*node.children[0]);
            if (!condition.has_value() || condition->kind != StaticAssertConstantKind::Boolean)
                return std::nullopt;
            return evaluateStaticAssertConstant(
                *node.children[condition->booleanValue ? 1U : 2U]);
        }
        default:
            return std::nullopt;
    }
}


}

std::unique_ptr<ProjectOopHirNode> ProjectOopHirModel::lowerStatement(
    const ast::AstNode* statement,
    std::size_t memberOrdinal,
    std::string& error)
{
    if (statement == nullptr)
    {
        error = "null statement reached Stage 34 OOP HIR";
        return nullptr;
    }

    const auto cleanupOrdinalFor = [&](const MethodCleanupRegion* region) -> std::optional<std::size_t>
    {
        if (region == nullptr || methodBodyModel_ == nullptr) return std::nullopt;
        const auto regions = methodBodyModel_->cleanupRegions();
        if (regions.empty() || region < regions.data() || region >= regions.data() + regions.size())
            return std::nullopt;
        return static_cast<std::size_t>(region - regions.data());
    };

    const auto attachStructuredExit = [&](ProjectOopHirNode& node)
    {
        applyStructuredExitBinding(node, statement);
    };

    switch (statement->kind())
    {
        case ast::AstNodeKind::BlockStatement:
            return lowerBlock(statement, memberOrdinal, error);

        case ast::AstNodeKind::VariableDeclarationStatement:
        {
            const MethodLocalSymbol* local = localSymbol(statement);
            if (local == nullptr)
            {
                error = "Stage 34 cannot map local declaration to Stage 27 local symbol";
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::LocalDeclaration, statement, local->typeKey);
            node->stringValue = local->name;
            node->referenceLocal = local->isReference;
            node->referenceMutable = local->referenceMutable;
            node->staticLocal = local->isStatic;
            node->pinnedStorage = local->isPinned;
            node->ownership = local->ownership;
            node->automaticDestruction = local->isRaii || local->isScope || local->requiresValueDrop;
            node->objectLifetimeOwnership = local->objectLifetimeOwnership;
            node->storageOwnership = local->storageOwnership;
            const auto localIndex = static_cast<std::size_t>(local - methodBodyModel_->locals().data());
            node->localOrdinal = localIndex;
            if (!local->isStatic && (local->isRaii || local->isScope || local->requiresValueDrop))
            {
                const MethodCleanupRegion* region = cleanupRegion(statement);
                if (region == nullptr)
                {
                    error = "drop-bearing local is missing its lexical cleanup region";
                    return nullptr;
                }
                node->cleanupRegionOrdinal = cleanupOrdinalFor(region);
                if (!node->cleanupRegionOrdinal.has_value())
                {
                    error = "drop-bearing local cleanup region has no stable ordinal";
                    return nullptr;
                }
            }
            if (local->isVariableLengthArray)
            {
                if (local->variableLengthArrayCountSyntax == nullptr ||
                    local->variableLengthArrayElementTypeKey.empty())
                {
                    error = "NativeSharp 2.0 VLA local lost its element type or runtime extent";
                    return nullptr;
                }
                node->variableLengthArray = true;
                node->variableLengthArrayRank =
                    std::max<std::size_t>(1, local->variableLengthArrayExtentSyntaxes.size());
                node->elementTypeKey = local->variableLengthArrayElementTypeKey;
                std::unique_ptr<ProjectOopHirNode> count;
                std::vector<std::unique_ptr<ProjectOopHirNode>> allocationCounts;
                std::unique_ptr<ProjectOopHirNode> vlaBlock;
                if (local->variableLengthArrayExtentSyntaxes.size() > 1)
                {
                    if (local->variableLengthArrayExtentLocalOrdinals.size() !=
                        local->variableLengthArrayExtentSyntaxes.size())
                    {
                        error = "multidimensional VLA lost its captured extent locals";
                        return nullptr;
                    }
                    vlaBlock = makeNode(ProjectOopHirNodeKind::Block, statement, "builtin:void");
                    for (std::size_t dimension = 0;
                         dimension < local->variableLengthArrayExtentSyntaxes.size(); ++dimension)
                    {
                        auto extent = lowerExpression(
                            local->variableLengthArrayExtentSyntaxes[dimension],
                            memberOrdinal,
                            false,
                            error);
                        if (extent == nullptr) return nullptr;
                        const std::size_t extentOrdinal =
                            local->variableLengthArrayExtentLocalOrdinals[dimension];
                        const std::string& extentType =
                            methodBodyModel_->locals()[extentOrdinal].typeKey;
                        auto extentDeclaration = makeNode(
                            ProjectOopHirNodeKind::LocalDeclaration,
                            local->variableLengthArrayExtentSyntaxes[dimension],
                            extentType);
                        extentDeclaration->stringValue =
                            methodBodyModel_->locals()[extentOrdinal].name;
                        extentDeclaration->debugDisplayName = local->name + ".Length[" +
                            std::to_string(dimension) + "]";
                        extentDeclaration->localOrdinal = extentOrdinal;
                        extentDeclaration->children.push_back(std::move(extent));
                        vlaBlock->children.push_back(std::move(extentDeclaration));

                        auto extentValue = makeNode(
                            ProjectOopHirNodeKind::LocalValue,
                            local->variableLengthArrayExtentSyntaxes[dimension],
                            extentType);
                        extentValue->localOrdinal = extentOrdinal;
                        allocationCounts.push_back(std::move(extentValue));
                    }
                }
                else
                {
                    count = lowerExpression(
                        local->variableLengthArrayCountSyntax, memberOrdinal, false, error);
                    if (count == nullptr) return nullptr;
                }

                auto allocation = makeNode(
                    ProjectOopHirNodeKind::StackAllocation, statement, local->typeKey);
                allocation->elementTypeKey = local->variableLengthArrayElementTypeKey;
                if (!allocationCounts.empty())
                {
                    for (auto& extent : allocationCounts)
                        allocation->children.push_back(std::move(extent));
                }
                else
                {
                    allocation->children.push_back(std::move(count));
                }
                node->children.push_back(std::move(allocation));
                if (vlaBlock != nullptr)
                {
                    vlaBlock->children.push_back(std::move(node));
                    return vlaBlock;
                }
            }
            else if (statement->first() != nullptr)
            {
                std::unique_ptr<ProjectOopHirNode> initializer = lowerExpression(
                    statement->first(), memberOrdinal, local->isReference, error);
                if (initializer == nullptr)
                {
                    return nullptr;
                }
                node->placementConstructed =
                    initializer->kind == ProjectOopHirNodeKind::PlacementConstruction;
                const ProjectImplicitConversion conversion = local->isReference
                    ? ProjectImplicitConversion{
                        ProjectConversionKind::Identity, 0, local->typeKey, local->typeKey,
                        std::nullopt}
                    : classifyImplicit(
                        initializer->typeKey,
                        local->typeKey,
                        expressionType(statement->first()) != nullptr
                            ? expressionType(statement->first())->integerConstant
                            : std::nullopt);
                if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
                {
                    initializer = wrapImplicitConversion(std::move(initializer), conversion, statement->first());
                }
                if (local->ownership == sema::OwnershipKind::Borrowed &&
                    local->localBorrowBindingOrdinal.has_value() &&
                    *local->localBorrowBindingOrdinal < methodBodyModel_->localBorrowBindings().size())
                {
                    const MethodLocalBorrowBinding& borrow =
                        methodBodyModel_->localBorrowBindings()[*local->localBorrowBindingOrdinal];
                    auto borrowed = makeNode(ProjectOopHirNodeKind::BorrowValue, statement->first(), local->typeKey);
                    borrowed->localBorrowSourceKind = borrow.sourceKind;
                    borrowed->localOrdinal = borrow.sourceLocalOrdinal;
                    borrowed->parameterOrdinal = borrow.sourceParameterOrdinal;
                    borrowed->children.push_back(std::move(initializer));
                    initializer = std::move(borrowed);
                }
                node->children.push_back(std::move(initializer));
            }
            return node;
        }

        case ast::AstNodeKind::DeconstructionDeclarationStatement:
        {
            if (objectLayoutModel_ == nullptr || nameLookup_ == nullptr)
            {
                error = "T02-F03 deconstruction declaration requires value layouts";
                return nullptr;
            }
            const MethodDeconstructionBinding* binding =
                methodBodyModel_->deconstructionBinding(statement);
            if (binding == nullptr)
            {
                error = "T02-F03 deconstruction declaration has no semantic binding";
                return nullptr;
            }
            auto source = lowerDeconstructionSource(
                statement, statement->first(), memberOrdinal, error);
            if (source == nullptr) return nullptr;
            const std::optional<std::vector<std::string>> elements =
                ProjectTypeConversion::tupleElementTypeKeys(binding->tupleTypeKey);
            const std::optional<language::AggregateLayout> layout =
                objectLayoutModel_->tupleLayoutForTypeKey(*nameLookup_, binding->tupleTypeKey);
            if (!elements.has_value() || !layout.has_value() ||
                elements->size() != statement->children().size() ||
                layout->offsets.size() != elements->size())
            {
                error = "T02-F03 deconstruction declaration lost its tuple shape or layout";
                return nullptr;
            }
            auto block = makeNode(ProjectOopHirNodeKind::Block, statement, "builtin:void");
            auto deconstruct = makeNode(
                ProjectOopHirNodeKind::TupleDeconstruct, statement, binding->tupleTypeKey);
            deconstruct->children.push_back(std::move(source));
            std::function<bool(const ast::AstNode*, const std::string&, std::uint64_t)>
                appendBinding;
            appendBinding = [&](const ast::AstNode* bindingSyntax, const std::string& typeKey,
                                std::uint64_t byteOffset) -> bool
            {
                if (bindingSyntax == nullptr) return false;
                if (bindingSyntax->kind() == ast::AstNodeKind::TupleExpression)
                {
                    const std::optional<std::vector<std::string>> nestedElements =
                        ProjectTypeConversion::tupleElementTypeKeys(typeKey);
                    const std::optional<language::AggregateLayout> nestedLayout =
                        objectLayoutModel_->tupleLayoutForTypeKey(*nameLookup_, typeKey);
                    if (!nestedElements.has_value() || !nestedLayout.has_value() ||
                        nestedElements->size() != bindingSyntax->children().size() ||
                        nestedLayout->offsets.size() != nestedElements->size())
                    {
                        error = "T02-F03 nested declaration pattern lost its tuple layout";
                        return false;
                    }
                    for (std::size_t index = 0; index < nestedElements->size(); ++index)
                        if (!appendBinding(
                                bindingSyntax->children()[index], (*nestedElements)[index],
                                byteOffset + nestedLayout->offsets[index])) return false;
                    return true;
                }
                if (bindingSyntax->kind() == ast::AstNodeKind::IdentifierExpression &&
                    bindingSyntax->text() == "_") return true;
                const MethodLocalSymbol* local = localSymbol(bindingSyntax);
                if (local == nullptr)
                {
                    error = "T02-F03 deconstruction binding has no local symbol";
                    return false;
                }
                const std::size_t localOrdinal = static_cast<std::size_t>(
                    local - methodBodyModel_->locals().data());
                auto declaration = makeNode(
                    ProjectOopHirNodeKind::LocalDeclaration, bindingSyntax, local->typeKey);
                declaration->stringValue = local->name;
                declaration->localOrdinal = localOrdinal;
                block->children.push_back(std::move(declaration));

                auto address = makeNode(
                    ProjectOopHirNodeKind::LocalAddress, bindingSyntax, local->typeKey);
                address->localOrdinal = localOrdinal;
                auto element = makeNode(
                    ProjectOopHirNodeKind::TupleDeconstructElement, bindingSyntax, local->typeKey);
                element->elementTypeKey = typeKey;
                element->integerValue = byteOffset;
                element->conversion = ProjectTypeConversion::classifyImplicit(
                    typeKey, local->typeKey);
                element->children.push_back(std::move(address));
                deconstruct->children.push_back(std::move(element));
                return true;
            };
            for (std::size_t index = 0; index < statement->children().size(); ++index)
            {
                if (!appendBinding(
                        statement->children()[index], (*elements)[index],
                        static_cast<std::uint64_t>(layout->offsets[index]))) return nullptr;
            }
            block->children.push_back(std::move(deconstruct));
            return block;
        }

        case ast::AstNodeKind::ExpressionStatement:
        {
            auto node = makeNode(ProjectOopHirNodeKind::ExpressionStatement, statement, "builtin:void");
            std::unique_ptr<ProjectOopHirNode> expression = lowerExpression(
                statement->first(), memberOrdinal, false, error);
            if (expression == nullptr)
            {
                return nullptr;
            }
            node->children.push_back(std::move(expression));
            return node;
        }

        case ast::AstNodeKind::InlineAssemblyStatement:
        {
            auto assembly = makeNode(ProjectOopHirNodeKind::InlineAssembly, statement, "builtin:void");
            assembly->stringValue = statement->text();
            for (const ast::AstNode* child : statement->children())
            {
                if (child == nullptr)
                {
                    error = "integrated assembly contains a null contract record";
                    return nullptr;
                }

                if (child->kind() == ast::AstNodeKind::InlineAssemblyLine)
                {
                    auto line = makeNode(
                        ProjectOopHirNodeKind::InlineAssemblyLine, child, "builtin:void");
                    line->stringValue = child->text();
                    line->elementTypeKey = child->auxiliaryText();
                    assembly->children.push_back(std::move(line));
                    continue;
                }

                if (child->kind() == ast::AstNodeKind::InlineAssemblyClobber)
                {
                    auto clobber = makeNode(
                        ProjectOopHirNodeKind::InlineAssemblyClobber, child, "builtin:void");
                    clobber->stringValue = child->text();
                    clobber->elementTypeKey = child->auxiliaryText();
                    clobber->integerValue = child->integerValue();
                    clobber->booleanValue = child->flag();
                    assembly->children.push_back(std::move(clobber));
                    continue;
                }

                const bool input = child->kind() == ast::AstNodeKind::InlineAssemblyInputOperand;
                const bool output = child->kind() == ast::AstNodeKind::InlineAssemblyOutputOperand;
                if (!input && !output)
                {
                    error = "integrated assembly contains an unknown contract record";
                    return nullptr;
                }
                if (child->first() == nullptr)
                {
                    error = "integrated assembly operand lost its NativeSharp expression";
                    return nullptr;
                }

                const bool memoryInput = input && child->auxiliaryText() == "m";
                auto value = lowerExpression(
                    child->first(), memberOrdinal, output || memoryInput, error);
                if (value == nullptr) return nullptr;
                const MethodExpressionTypeBinding* operandType = expressionType(child->first());
                if (operandType == nullptr || operandType->typeKey.empty())
                {
                    error = "integrated assembly operand lost its resolved NativeSharp type";
                    return nullptr;
                }

                auto operand = makeNode(
                    input ? ProjectOopHirNodeKind::InlineAssemblyInput
                          : ProjectOopHirNodeKind::InlineAssemblyOutput,
                    child,
                    operandType->typeKey);
                operand->stringValue = child->auxiliaryText();
                operand->integerValue = child->integerValue();
                operand->booleanValue = child->flag();
                operand->children.push_back(std::move(value));
                assembly->children.push_back(std::move(operand));
            }
            return assembly;
        }

        case ast::AstNodeKind::StaticAssertStatement:
        {
            std::unique_ptr<ProjectOopHirNode> condition = lowerExpression(
                statement->first(), memberOrdinal, false, error);
            if (condition == nullptr) return nullptr;
            const std::optional<StaticAssertConstant> value =
                evaluateStaticAssertConstant(*condition);
            if (!value.has_value() || value->kind != StaticAssertConstantKind::Boolean)
            {
                error = "static_assert condition is not a compile-time bool constant";
                return nullptr;
            }
            if (!value->booleanValue)
            {
                error = "static assertion failed";
                if (!statement->text().empty()) error += ": " + statement->text();
                return nullptr;
            }
            return makeNode(ProjectOopHirNodeKind::Block, statement, "builtin:void");
        }

        case ast::AstNodeKind::IfStatement:
        {
            auto node = makeNode(ProjectOopHirNodeKind::If, statement, "builtin:void");
            auto condition = lowerExpression(statement->first(), memberOrdinal, false, error);
            auto thenBlock = lowerBlock(statement->second(), memberOrdinal, error);
            if (condition == nullptr || thenBlock == nullptr)
            {
                return nullptr;
            }
            node->children.push_back(std::move(condition));
            node->children.push_back(std::move(thenBlock));
            if (statement->third() != nullptr)
            {
                std::unique_ptr<ProjectOopHirNode> elseNode = statement->third()->kind() == ast::AstNodeKind::BlockStatement
                    ? lowerBlock(statement->third(), memberOrdinal, error)
                    : lowerStatement(statement->third(), memberOrdinal, error);
                if (elseNode == nullptr)
                {
                    return nullptr;
                }
                node->children.push_back(std::move(elseNode));
            }
            return node;
        }

        case ast::AstNodeKind::DoWhileStatement:
        case ast::AstNodeKind::WhileStatement:
        {
            auto node = makeNode(
                statement->kind() == ast::AstNodeKind::DoWhileStatement
                    ? ProjectOopHirNodeKind::DoWhile
                    : ProjectOopHirNodeKind::While,
                statement,
                "builtin:void");
            auto condition = lowerExpression(statement->first(), memberOrdinal, false, error);
            auto body = lowerBlock(statement->second(), memberOrdinal, error);
            if (condition == nullptr || body == nullptr)
            {
                return nullptr;
            }
            node->children.push_back(std::move(condition));
            node->children.push_back(std::move(body));
            return node;
        }

        // `switch`. The label EXPRESSIONS are not lowered at all: the body model folded them to
        // integers, checked them against the governing type and rejected duplicates, and left the
        // answer in a MethodSwitchBinding. So this walk copies numbers and lowers statements --
        // there is exactly one place in the compiler that knows how to evaluate a case label, and
        // it is not here.
        case ast::AstNodeKind::SwitchStatement:
        {
            const MethodSwitchBinding* binding = switchBinding(statement);
            if (binding == nullptr)
            {
                error = "Stage 34 OOP HIR reached a switch statement with no semantic binding";
                return nullptr;
            }

            auto node = makeNode(ProjectOopHirNodeKind::Switch, statement, "builtin:void");
            auto governing = lowerExpression(statement->first(), memberOrdinal, false, error);
            if (governing == nullptr)
            {
                return nullptr;
            }
            node->children.push_back(std::move(governing));

            std::size_t sectionIndex = 0;
            for (const ast::AstNode* section : statement->children())
            {
                if (section == nullptr || section->kind() != ast::AstNodeKind::SwitchSection)
                {
                    continue;
                }
                if (sectionIndex >= binding->sections.size())
                {
                    error = "Stage 34 OOP HIR found more switch sections than its semantic binding";
                    return nullptr;
                }
                const MethodSwitchSectionBinding& sectionBinding = binding->sections[sectionIndex];
                ++sectionIndex;

                auto sectionNode =
                    makeNode(ProjectOopHirNodeKind::SwitchSection, section, "builtin:void");
                sectionNode->switchLabels = sectionBinding.labelValues;
                sectionNode->switchHasDefault = sectionBinding.hasDefault;

                // The section's statements become an ordinary Block, so every consumer downstream
                // sees a shape it already handles. Built here rather than through lowerBlock()
                // because a section is not a BlockStatement -- its child list starts with labels.
                auto body = makeNode(ProjectOopHirNodeKind::Block, section, "builtin:void");
                const std::size_t sectionCleanupDepth = activeHirCleanupRegions_.size();
                for (const ast::AstNode* child : section->children())
                {
                    if (child == nullptr || child->kind() == ast::AstNodeKind::SwitchLabel)
                    {
                        continue;
                    }
                    std::unique_ptr<ProjectOopHirNode> lowered =
                        lowerStatement(child, memberOrdinal, error);
                    if (lowered == nullptr)
                    {
                        activeHirCleanupRegions_.resize(sectionCleanupDepth);
                        return nullptr;
                    }
                    if (lowered->kind == ProjectOopHirNodeKind::LocalDeclaration &&
                        lowered->cleanupRegionOrdinal.has_value())
                    {
                        activeHirCleanupRegions_.push_back(*lowered->cleanupRegionOrdinal);
                    }
                    body->children.push_back(std::move(lowered));
                }
                activeHirCleanupRegions_.resize(sectionCleanupDepth);

                sectionNode->children.push_back(std::move(body));
                node->children.push_back(std::move(sectionNode));
            }
            return node;
        }

        case ast::AstNodeKind::ForStatement:
        {
            auto node = makeNode(ProjectOopHirNodeKind::For, statement, "builtin:void");
            const std::size_t cleanupDepthAtEntry = activeHirCleanupRegions_.size();
            if (statement->first() != nullptr)
            {
                std::unique_ptr<ProjectOopHirNode> init;
                if (statement->first()->kind() == ast::AstNodeKind::BlockStatement)
                    init = lowerBlock(statement->first(), memberOrdinal, error);
                else if (statement->first()->kind() == ast::AstNodeKind::VariableDeclarationStatement)
                    init = lowerStatement(statement->first(), memberOrdinal, error);
                else
                    init = lowerExpression(statement->first(), memberOrdinal, false, error);
                if (init == nullptr)
                {
                    activeHirCleanupRegions_.resize(cleanupDepthAtEntry);
                    return nullptr;
                }
                if (init->kind == ProjectOopHirNodeKind::LocalDeclaration &&
                    init->cleanupRegionOrdinal.has_value())
                {
                    activeHirCleanupRegions_.push_back(*init->cleanupRegionOrdinal);
                }
                node->children.push_back(std::move(init));
            }
            else
            {
                node->children.push_back(nullptr);
            }
            if (statement->second() != nullptr)
            {
                auto condition = lowerExpression(statement->second(), memberOrdinal, false, error);
                if (condition == nullptr)
                {
                    activeHirCleanupRegions_.resize(cleanupDepthAtEntry);
                    return nullptr;
                }
                node->children.push_back(std::move(condition));
            }
            else
            {
                node->children.push_back(nullptr);
            }
            if (statement->third() != nullptr)
            {
                std::unique_ptr<ProjectOopHirNode> increment =
                    statement->third()->kind() == ast::AstNodeKind::BlockStatement
                        ? lowerBlock(statement->third(), memberOrdinal, error)
                        : lowerExpression(statement->third(), memberOrdinal, false, error);
                if (increment == nullptr)
                {
                    activeHirCleanupRegions_.resize(cleanupDepthAtEntry);
                    return nullptr;
                }
                node->children.push_back(std::move(increment));
            }
            else
            {
                node->children.push_back(nullptr);
            }
            if (statement->children().empty())
            {
                error = "Stage 34 for statement is missing a body";
                activeHirCleanupRegions_.resize(cleanupDepthAtEntry);
                return nullptr;
            }
            auto body = lowerBlock(statement->children().front(), memberOrdinal, error);
            if (body == nullptr)
            {
                activeHirCleanupRegions_.resize(cleanupDepthAtEntry);
                return nullptr;
            }
            node->children.push_back(std::move(body));
            activeHirCleanupRegions_.resize(cleanupDepthAtEntry);
            return node;
        }

        case ast::AstNodeKind::ForeachStatement:
        {
            // Foreach was desugared at parse time into two unconditionally-built candidate loops
            // (see the design comment atop Parser::parseForeachStatement and
            // ProjectMethodBodyModel::analyzeForeach). ProjectMethodBodyModel::analyzeForeach
            // already picked the "real" candidate and ran ordinary semantic analysis on ONLY it --
            // but the raw AST this function walks still carries the original ForeachStatement
            // node, with BOTH candidate subtrees still attached, as the statement in the enclosing
            // block. There is no side-channel recording which candidate Stage 27 chose, so HIR
            // lowering re-derives the SAME array-vs-enumerator decision independently, the same
            // way: resolve __foreach_source's already-analyzed typeKey via localSymbol() (populated
            // from MethodLocalSymbol::syntax, see localBySyntax_) and check for the "[]" array
            // suffix (see ProjectGenericModel::canonicalTypeKey for that representation). Both
            // passes read the identical resolved type, so they always agree.
            //
            // No new HIR node kind is introduced: __foreach_source's declaration lowers through
            // the ordinary VariableDeclarationStatement case above, and the chosen candidate --
            // itself just a BlockStatement holding an index/enumerator VariableDeclarationStatement
            // plus a WhileStatement -- lowers through the ordinary lowerBlock/WhileStatement paths.
            // The two are wrapped in one outer Block so the foreach statement lowers to exactly one
            // HIR node, mirroring the single Foreach semantic scope analyzeForeach created.
            const MethodLocalSymbol* sourceLocal = localSymbol(statement->first());
            if (sourceLocal == nullptr)
            {
                error = "Stage 34 foreach source local is missing its Stage 27 semantic binding";
                return nullptr;
            }
            const bool isArray = sourceLocal->typeKey.size() > 2 &&
                sourceLocal->typeKey.compare(sourceLocal->typeKey.size() - 2, 2, "[]") == 0;
            const bool isSpan =
                sourceLocal->typeKey.starts_with("type:NativeSharp.Core.Span<") ||
                sourceLocal->typeKey.starts_with("type:NativeSharp.Core.Span__g") ||
                sourceLocal->typeKey.find("::NativeSharp.Core.Span__g") != std::string::npos ||
                sourceLocal->typeKey.starts_with("type:NativeSharp.Core.ReadOnlySpan<") ||
                sourceLocal->typeKey.starts_with("type:NativeSharp.Core.ReadOnlySpan__g") ||
                sourceLocal->typeKey.find("::NativeSharp.Core.ReadOnlySpan__g") != std::string::npos;
            const ast::AstNode* chosenCandidate = isArray
                ? statement->second()
                : (isSpan && statement->children().size() > 1U
                    ? statement->children()[1]
                    : statement->third());

            auto node = makeNode(ProjectOopHirNodeKind::Block, statement, "builtin:void");
            std::unique_ptr<ProjectOopHirNode> sourceDeclHir =
                lowerStatement(statement->first(), memberOrdinal, error);
            if (sourceDeclHir == nullptr)
            {
                return nullptr;
            }
            node->children.push_back(std::move(sourceDeclHir));

            std::unique_ptr<ProjectOopHirNode> candidateHir = lowerBlock(chosenCandidate, memberOrdinal, error);
            if (candidateHir == nullptr)
            {
                return nullptr;
            }
            node->children.push_back(std::move(candidateHir));
            return node;
        }

        case ast::AstNodeKind::BreakStatement:
        {
            auto node = makeNode(ProjectOopHirNodeKind::Break, statement, "builtin:void");
            attachStructuredExit(*node);
            node->stringValue = statement->text();
            if (statement->first() != nullptr)
            {
                const MethodLoopBreakBinding* binding = methodBodyModel_ != nullptr
                    ? methodBodyModel_->loopBreakBinding(statement) : nullptr;
                if (binding == nullptr)
                {
                    error = "T07-F05 break value has no loop-expression semantic binding";
                    return nullptr;
                }
                auto value = lowerExpression(statement->first(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;
                if (binding->conversion.exists() &&
                    binding->conversion.kind != ProjectConversionKind::Identity)
                {
                    value = wrapImplicitConversion(
                        std::move(value), binding->conversion, statement->first());
                }
                node->children.push_back(std::move(value));
                node->elementTypeKey = binding->resultTypeKey;
            }
            return node;
        }
        case ast::AstNodeKind::ContinueStatement:
        {
            auto node = makeNode(ProjectOopHirNodeKind::Continue, statement, "builtin:void");
            attachStructuredExit(*node);
            node->stringValue = statement->text();
            return node;
        }

        case ast::AstNodeKind::SuspendStatement:
        case ast::AstNodeKind::YieldReturnStatement:
        case ast::AstNodeKind::YieldBreakStatement:
            return lowerCoroutineStatement(statement, memberOrdinal, error);
        case ast::AstNodeKind::ReturnStatement:
        {
            auto node = makeNode(ProjectOopHirNodeKind::Return, statement, "builtin:void");
            attachStructuredExit(*node);
            if (statement->first() != nullptr)
            {
                bool referenceReturn = false;
                bool valueReturningCallable = false;
                std::string declaredTypeKey;
                sema::OwnershipKind declaredOwnership = sema::OwnershipKind::None;
                if (memberModel_ != nullptr && memberOrdinal < memberModel_->members().size())
                {
                    const ClassMemberSymbol& member = memberModel_->members()[memberOrdinal];
                    referenceReturn = member.isReferenceReturn;
                    valueReturningCallable = member.kind == ClassMemberKind::Method;
                    declaredTypeKey = member.declaredTypeKey;
                    declaredOwnership = member.declaredOwnership;
                }
                else
                {
                    const std::size_t callableBase =
                        memberModel_ != nullptr ? memberModel_->members().size() : 0;
                    const std::size_t functionOrdinal =
                        memberOrdinal >= callableBase ? memberOrdinal - callableBase :
                        std::numeric_limits<std::size_t>::max();
                    if (namespaceFunctionModel_ == nullptr ||
                        functionOrdinal >= namespaceFunctionModel_->functions().size())
                    {
                        error = "Stage 34 return refers to invalid callable";
                        return nullptr;
                    }
                    const NamespaceFunctionSymbol& function =
                        namespaceFunctionModel_->functions()[functionOrdinal];
                    referenceReturn = function.isReferenceReturn;
                    valueReturningCallable = true;
                    declaredTypeKey = function.declaredTypeKey;
                    declaredOwnership = function.declaredOwnership;
                }

                auto value = lowerExpression(
                    statement->first(), memberOrdinal, referenceReturn, error);
                if (value == nullptr)
                {
                    return nullptr;
                }
                if (valueReturningCallable && declaredTypeKey != "builtin:void" && !referenceReturn)
                {
                    const MethodExpressionTypeBinding* type = expressionType(statement->first());
                    const ProjectImplicitConversion conversion = classifyImplicit(
                        value->typeKey,
                        declaredTypeKey,
                        type != nullptr ? type->integerConstant : std::nullopt);
                    if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
                    {
                        value = wrapImplicitConversion(std::move(value), conversion, statement->first());
                    }
                    if (declaredOwnership == sema::OwnershipKind::Borrowed)
                    {
                        const MethodBorrowBinding* borrow = borrowBinding(statement->first());
                        if (borrow == nullptr)
                        {
                            error = "Stage 50.8 borrowed return has no frozen borrow binding";
                            return nullptr;
                        }
                        auto borrowed = makeNode(
                            ProjectOopHirNodeKind::BorrowValue,
                            statement->first(),
                            declaredTypeKey);
                        borrowed->borrowKind = borrow->kind;
                        borrowed->localOrdinal = borrow->sourceLocalOrdinal;
                        borrowed->parameterOrdinal = borrow->sourceParameterOrdinal;
                        borrowed->children.push_back(std::move(value));
                        value = std::move(borrowed);
                    }
                }
                if (statement->flag())
                {
                    switch (value->kind)
                    {
                        case ProjectOopHirNodeKind::InstanceCall:
                        case ProjectOopHirNodeKind::StaticCall:
                            value->mustTailCall = true;
                            break;
                        default:
                            error = "T08-F06 validated must_tail return did not lower to a direct call";
                            return nullptr;
                    }
                }
                node->children.push_back(std::move(value));
            }
            return node;
        }
        case ast::AstNodeKind::ThrowStatement:
        {
            const MethodExceptionBinding* binding = exceptionBinding(statement);
            if (binding == nullptr)
            {
                error = "Stage 45.6 throw statement has no Stage 45.2 exception binding";
                return nullptr;
            }
            auto node = makeNode(
                binding->kind == MethodExceptionOperationKind::Rethrow
                    ? ProjectOopHirNodeKind::Rethrow
                    : ProjectOopHirNodeKind::Throw,
                statement, "builtin:void");
            node->typeKey = binding->exceptionTypeKey.empty() ? "builtin:void" : binding->exceptionTypeKey;
            node->exceptionTypeOrdinal = binding->exceptionTypeOrdinal;
            node->cleanupContinuationKind = MethodCleanupContinuationKind::Exception;

            const std::size_t retainedDepth = activeHirExceptionHandlers_.empty()
                ? 0
                : activeHirExceptionHandlers_.back().retainedCleanupDepth;
            for (std::size_t index = activeHirCleanupRegions_.size(); index > retainedDepth; --index)
                node->cleanupRegionOrdinals.push_back(activeHirCleanupRegions_[index - 1]);
            if (!activeHirExceptionHandlers_.empty())
                node->exceptionTargetRegionOrdinal = activeHirExceptionHandlers_.back().hirRegionOrdinal;

            if (statement->first() != nullptr)
            {
                auto value = lowerExpression(statement->first(), memberOrdinal, false, error);
                if (value == nullptr) return nullptr;
                node->children.push_back(std::move(value));
            }
            return node;
        }

        case ast::AstNodeKind::UsingStatement:
        {
            const MethodUsingBinding* binding = usingBinding(statement);
            const MethodCleanupRegion* region = cleanupRegion(statement);
            const std::optional<std::size_t> cleanupOrdinal = cleanupOrdinalFor(region);
            if (binding == nullptr || !cleanupOrdinal.has_value())
            {
                error = "Stage 45.6 using statement has no frozen using/cleanup binding";
                return nullptr;
            }
            auto node = makeNode(ProjectOopHirNodeKind::UsingRegion, statement, "builtin:void");
            node->localOrdinal = binding->resourceLocalOrdinal;
            node->elementTypeKey = binding->resourceTypeKey;
            if (binding->resourceLocalOrdinal < methodBodyModel_->locals().size())
                node->stringValue = methodBodyModel_->locals()[binding->resourceLocalOrdinal].name;
            node->cleanupRegionOrdinal = cleanupOrdinal;
            node->disposeContractMemberOrdinal = binding->disposeContractMemberOrdinal;
            node->disposeImplementationMemberOrdinal = binding->disposeImplementationMemberOrdinal;
            node->interfaceCleanupDispatch = binding->interfaceDispatch;

            auto initializer = lowerExpression(statement->first(), memberOrdinal, false, error);
            if (initializer == nullptr) return nullptr;
            const MethodExpressionTypeBinding* sourceType = expressionType(statement->first());
            const ProjectImplicitConversion conversion = classifyImplicit(
                initializer->typeKey, binding->resourceTypeKey,
                sourceType != nullptr ? sourceType->integerConstant : std::nullopt);
            if (conversion.exists() && conversion.kind != ProjectConversionKind::Identity)
                initializer = wrapImplicitConversion(std::move(initializer), conversion, statement->first());
            node->children.push_back(std::move(initializer));

            activeHirCleanupRegions_.push_back(*cleanupOrdinal);
            auto body = lowerBlock(statement->second(), memberOrdinal, error);
            activeHirCleanupRegions_.pop_back();
            if (body == nullptr) return nullptr;
            node->children.push_back(std::move(body));

            auto continuation = makeNode(ProjectOopHirNodeKind::CleanupContinuation, statement, "builtin:void");
            continuation->cleanupRegionOrdinal = cleanupOrdinal;
            continuation->cleanupContinuationKind = MethodCleanupContinuationKind::Normal;
            node->children.push_back(std::move(continuation));
            return node;
        }

        case ast::AstNodeKind::TryStatement:
        {
            auto node = makeNode(ProjectOopHirNodeKind::TryRegion, statement, "builtin:void");
            node->hirRegionOrdinal = nextHirRegionOrdinal_++;
            const MethodCleanupRegion* region = cleanupRegion(statement);
            const std::optional<std::size_t> cleanupOrdinal = cleanupOrdinalFor(region);
            if (cleanupOrdinal.has_value())
            {
                node->cleanupRegionOrdinal = cleanupOrdinal;
                activeHirCleanupRegions_.push_back(*cleanupOrdinal);
            }

            const bool hasCatches = !statement->children().empty();
            if (hasCatches)
            {
                activeHirExceptionHandlers_.push_back(HirExceptionHandlerContext{
                    *node->hirRegionOrdinal, activeHirCleanupRegions_.size()});
            }
            auto protectedBlock = lowerBlock(statement->first(), memberOrdinal, error);
            if (hasCatches) activeHirExceptionHandlers_.pop_back();
            if (protectedBlock == nullptr)
            {
                if (cleanupOrdinal.has_value()) activeHirCleanupRegions_.pop_back();
                return nullptr;
            }
            node->children.push_back(std::move(protectedBlock));

            for (const ast::AstNode* catchClause : statement->children())
            {
                const MethodExceptionBinding* catchBinding = exceptionBinding(catchClause);
                if (catchBinding == nullptr || catchBinding->kind != MethodExceptionOperationKind::Catch)
                {
                    if (cleanupOrdinal.has_value()) activeHirCleanupRegions_.pop_back();
                    error = "Stage 45.6 catch clause has no Stage 45.2 catch binding";
                    return nullptr;
                }
                auto catchNode = makeNode(ProjectOopHirNodeKind::CatchRegion, catchClause, "builtin:void");
                catchNode->elementTypeKey = catchBinding->exceptionTypeKey;
                catchNode->exceptionTypeOrdinal = catchBinding->exceptionTypeOrdinal;
                catchNode->catchAll = catchBinding->catchAll;
                catchNode->catchHasFilter = catchBinding->hasFilter;
                catchNode->catchExhaustive = catchBinding->exhaustive;
                catchNode->catchMatchedErrorTypeKeys = catchBinding->matchedErrorTypeKeys;
                if (const MethodLocalSymbol* local = localSymbol(catchClause); local != nullptr)
                {
                    catchNode->localOrdinal = static_cast<std::size_t>(
                        local - methodBodyModel_->locals().data());
                    catchNode->stringValue = local->name;
                }
                if (catchBinding->hasFilter)
                {
                    auto catchFilter = lowerExpression(
                        catchClause->second(), memberOrdinal, false, error);
                    if (catchFilter == nullptr)
                    {
                        if (cleanupOrdinal.has_value()) activeHirCleanupRegions_.pop_back();
                        return nullptr;
                    }
                    catchNode->children.push_back(std::move(catchFilter));
                }
                auto catchBody = lowerBlock(catchClause->first(), memberOrdinal, error);
                if (catchBody == nullptr)
                {
                    if (cleanupOrdinal.has_value()) activeHirCleanupRegions_.pop_back();
                    return nullptr;
                }
                catchNode->children.push_back(std::move(catchBody));
                node->children.push_back(std::move(catchNode));
            }

            if (cleanupOrdinal.has_value()) activeHirCleanupRegions_.pop_back();

            if (statement->second() != nullptr)
            {
                if (!cleanupOrdinal.has_value())
                {
                    error = "Stage 45.6 finally block has no cleanup region";
                    return nullptr;
                }
                auto finallyNode = makeNode(ProjectOopHirNodeKind::FinallyRegion, statement->second(), "builtin:void");
                finallyNode->cleanupRegionOrdinal = cleanupOrdinal;
                auto finallyBody = lowerBlock(statement->second(), memberOrdinal, error);
                if (finallyBody == nullptr) return nullptr;
                finallyNode->children.push_back(std::move(finallyBody));
                auto continuation = makeNode(ProjectOopHirNodeKind::CleanupContinuation, statement, "builtin:void");
                continuation->cleanupRegionOrdinal = cleanupOrdinal;
                continuation->cleanupContinuationKind = MethodCleanupContinuationKind::Normal;
                finallyNode->children.push_back(std::move(continuation));
                node->children.push_back(std::move(finallyNode));
            }
            return node;
        }

        default:
            error = "unsupported Stage 34 OOP statement kind: " +
                std::string(ast::astNodeKindName(statement->kind()));
            return nullptr;
    }
}

// Ver el comentario de la declaracion en ProjectOopHirModel.h: este envoltorio existe para que
// los 125 sitios que ponen un error dentro de lowerExpressionImpl no tengan que adjuntar la
// ubicacion cada uno.

}
