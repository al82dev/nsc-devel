#include "nsc_suite/TestRegistry.h"

#include "nativesharp/AST/AstNodeKind.h"

#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::ast::AstNodeKind;

struct ExpectedAstKind final
{
    AstNodeKind kind;
    std::string_view name;
};

// Independent CA-AST-003 oracle: order and spelling are intentionally explicit so a product-side
// reorder, insertion, or rename cannot update the expected values implicitly.
constexpr std::array<ExpectedAstKind, 103> ExpectedAstKinds = {{
    {AstNodeKind::CompilationUnit, "CompilationUnit"},
    {AstNodeKind::NamespaceDeclaration, "NamespaceDeclaration"},
    {AstNodeKind::UsingNamespaceDirective, "UsingNamespaceDirective"},
    {AstNodeKind::UsingAliasDirective, "UsingAliasDirective"},
    {AstNodeKind::FunctionDeclaration, "FunctionDeclaration"},
    {AstNodeKind::ParameterDeclaration, "ParameterDeclaration"},
    {AstNodeKind::GlobalVariableDeclaration, "GlobalVariableDeclaration"},
    {AstNodeKind::StructDeclaration, "StructDeclaration"},
    {AstNodeKind::ClassDeclaration, "ClassDeclaration"},
    {AstNodeKind::InterfaceDeclaration, "InterfaceDeclaration"},
    {AstNodeKind::DelegateDeclaration, "DelegateDeclaration"},
    {AstNodeKind::FieldDeclaration, "FieldDeclaration"},
    {AstNodeKind::MethodDeclaration, "MethodDeclaration"},
    {AstNodeKind::ConstructorDeclaration, "ConstructorDeclaration"},
    {AstNodeKind::DestructorDeclaration, "DestructorDeclaration"},
    {AstNodeKind::PropertyDeclaration, "PropertyDeclaration"},
    {AstNodeKind::IndexerDeclaration, "IndexerDeclaration"},
    {AstNodeKind::EventDeclaration, "EventDeclaration"},
    {AstNodeKind::BaseConstructorInitializer, "BaseConstructorInitializer"},
    {AstNodeKind::ThisConstructorInitializer, "ThisConstructorInitializer"},
    {AstNodeKind::EnumDeclaration, "EnumDeclaration"},
    {AstNodeKind::EnumMemberDeclaration, "EnumMemberDeclaration"},
    {AstNodeKind::ExternCDeclaration, "ExternCDeclaration"},
    {AstNodeKind::BlockStatement, "BlockStatement"},
    {AstNodeKind::VariableDeclarationStatement, "VariableDeclarationStatement"},
    {AstNodeKind::IfStatement, "IfStatement"},
    {AstNodeKind::WhileStatement, "WhileStatement"},
    {AstNodeKind::DoWhileStatement, "DoWhileStatement"},
    {AstNodeKind::ForStatement, "ForStatement"},
    {AstNodeKind::ForeachStatement, "ForeachStatement"},
    {AstNodeKind::SwitchStatement, "SwitchStatement"},
    {AstNodeKind::SwitchSection, "SwitchSection"},
    {AstNodeKind::SwitchLabel, "SwitchLabel"},
    {AstNodeKind::BreakStatement, "BreakStatement"},
    {AstNodeKind::ContinueStatement, "ContinueStatement"},
    {AstNodeKind::ReturnStatement, "ReturnStatement"},
    {AstNodeKind::TryStatement, "TryStatement"},
    {AstNodeKind::CatchClause, "CatchClause"},
    {AstNodeKind::ThrowStatement, "ThrowStatement"},
    {AstNodeKind::UsingStatement, "UsingStatement"},
    {AstNodeKind::StaticAssertStatement, "StaticAssertStatement"},
    {AstNodeKind::InlineAssemblyStatement, "InlineAssemblyStatement"},
    {AstNodeKind::InlineAssemblyLine, "InlineAssemblyLine"},
    {AstNodeKind::InlineAssemblyInputOperand, "InlineAssemblyInputOperand"},
    {AstNodeKind::InlineAssemblyOutputOperand, "InlineAssemblyOutputOperand"},
    {AstNodeKind::InlineAssemblyClobber, "InlineAssemblyClobber"},
    {AstNodeKind::ExpressionStatement, "ExpressionStatement"},
    {AstNodeKind::IdentifierExpression, "IdentifierExpression"},
    {AstNodeKind::IntegerLiteralExpression, "IntegerLiteralExpression"},
    {AstNodeKind::FloatingLiteralExpression, "FloatingLiteralExpression"},
    {AstNodeKind::CharacterLiteralExpression, "CharacterLiteralExpression"},
    {AstNodeKind::StringLiteralExpression, "StringLiteralExpression"},
    {AstNodeKind::BooleanLiteralExpression, "BooleanLiteralExpression"},
    {AstNodeKind::NullLiteralExpression, "NullLiteralExpression"},
    {AstNodeKind::ThisExpression, "ThisExpression"},
    {AstNodeKind::BaseExpression, "BaseExpression"},
    {AstNodeKind::UnaryExpression, "UnaryExpression"},
    {AstNodeKind::BinaryExpression, "BinaryExpression"},
    {AstNodeKind::ConditionalExpression, "ConditionalExpression"},
    {AstNodeKind::AssignmentExpression, "AssignmentExpression"},
    {AstNodeKind::CallExpression, "CallExpression"},
    {AstNodeKind::IndexExpression, "IndexExpression"},
    {AstNodeKind::MemberExpression, "MemberExpression"},
    {AstNodeKind::PointerMemberExpression, "PointerMemberExpression"},
    {AstNodeKind::PostfixExpression, "PostfixExpression"},
    {AstNodeKind::CastExpression, "CastExpression"},
    {AstNodeKind::TypeTestExpression, "TypeTestExpression"},
    {AstNodeKind::SafeCastExpression, "SafeCastExpression"},
    {AstNodeKind::LambdaExpression, "LambdaExpression"},
    {AstNodeKind::SizeofTypeExpression, "SizeofTypeExpression"},
    {AstNodeKind::SizeofExpression, "SizeofExpression"},
    {AstNodeKind::AlignofTypeExpression, "AlignofTypeExpression"},
    {AstNodeKind::AlignofExpression, "AlignofExpression"},
    {AstNodeKind::OffsetofExpression, "OffsetofExpression"},
    {AstNodeKind::DefaultValueExpression, "DefaultValueExpression"},
    {AstNodeKind::NewExpression, "NewExpression"},
    {AstNodeKind::ObjectCreationExpression, "ObjectCreationExpression"},
    {AstNodeKind::PlacementConstructionExpression, "PlacementConstructionExpression"},
    {AstNodeKind::DeleteExpression, "DeleteExpression"},
    {AstNodeKind::DestroyExpression, "DestroyExpression"},
    {AstNodeKind::StackallocExpression, "StackallocExpression"},
    {AstNodeKind::MoveExpression, "MoveExpression"},
    {AstNodeKind::AggregateInitializerExpression, "AggregateInitializerExpression"},
    {AstNodeKind::NullCoalescingExpression, "NullCoalescingExpression"},
    {AstNodeKind::NullConditionalMemberExpression, "NullConditionalMemberExpression"},
    {AstNodeKind::RangeExpression, "RangeExpression"},
    {AstNodeKind::FromEndExpression, "FromEndExpression"},
    {AstNodeKind::SpreadElementExpression, "SpreadElementExpression"},
    {AstNodeKind::InterpolatedStringExpression, "InterpolatedStringExpression"},
    {AstNodeKind::WithExpression, "WithExpression"},
    {AstNodeKind::TupleExpression, "TupleExpression"},
    {AstNodeKind::DeconstructionDeclarationStatement, "DeconstructionDeclarationStatement"},
    {AstNodeKind::AssociatedTypeDeclaration, "AssociatedTypeDeclaration"},
    {AstNodeKind::AssociatedConstantDeclaration, "AssociatedConstantDeclaration"},
    {AstNodeKind::ExtensionDeclaration, "ExtensionDeclaration"},
    {AstNodeKind::ResultPropagationExpression, "ResultPropagationExpression"},
    {AstNodeKind::MatchExpression, "MatchExpression"},
    {AstNodeKind::MatchArmExpression, "MatchArmExpression"},
    {AstNodeKind::MatchValueExpression, "MatchValueExpression"},
    {AstNodeKind::LoopExpression, "LoopExpression"},
    {AstNodeKind::AwaitExpression, "AwaitExpression"},
    {AstNodeKind::YieldReturnStatement, "YieldReturnStatement"},
    {AstNodeKind::YieldBreakStatement, "YieldBreakStatement"},
}};

constexpr std::size_t HistoricalKindCount = 83;

// Registers one CA-AST-003 case without duplicating registry metadata.
void addAstNodeKindCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CA-AST-003", "Host.AST", "unit", "PR",
        {"ast", "node-kind"}, std::move(function)});
}

// Compares all one hundred valid values against the independently frozen public spelling.
void testExactNames(TestContext& context)
{
    for (std::size_t index = 0; index < ExpectedAstKinds.size(); ++index)
    {
        context.expect(nativesharp::ast::astNodeKindName(ExpectedAstKinds[index].kind) ==
                ExpectedAstKinds[index].name,
            "TA-AST-019 name differs at ordinal " + std::to_string(index));
    }
}

// Rejects any duplicate public name across the complete valid catalog.
void testUniqueNames(TestContext& context)
{
    std::unordered_set<std::string_view> uniqueNames;
    for (const ExpectedAstKind& expected : ExpectedAstKinds)
    {
        context.expect(uniqueNames.insert(nativesharp::ast::astNodeKindName(expected.kind)).second,
            "TA-AST-020 every valid AST node name must be unique");
    }
}

// Freezes the contiguous ordinal identity and declared order of the complete catalog.
void testContiguousOrdinals(TestContext& context)
{
    using Underlying = std::underlying_type_t<AstNodeKind>;
    for (std::size_t index = 0; index < ExpectedAstKinds.size(); ++index)
    {
        context.expect(static_cast<Underlying>(ExpectedAstKinds[index].kind) ==
                static_cast<Underlying>(index),
            "TA-AST-021 ordinal differs at catalog position " + std::to_string(index));
    }
}

// Checks the lower out-of-domain boundary explicitly.
void testNegativeUnknownValue(TestContext& context)
{
    context.expect(nativesharp::ast::astNodeKindName(static_cast<AstNodeKind>(-1)) ==
            "UnknownAstNode",
        "TA-AST-022 ordinal -1 must map to UnknownAstNode");
}

// Checks both the immediate upper boundary and the maximum underlying representation.
void testUpperUnknownValues(TestContext& context)
{
    using Underlying = std::underlying_type_t<AstNodeKind>;
    const auto immediateUpper = static_cast<AstNodeKind>(ExpectedAstKinds.size());
    const auto maximumValue = static_cast<AstNodeKind>(std::numeric_limits<Underlying>::max());
    context.expect(nativesharp::ast::astNodeKindName(immediateUpper) == "UnknownAstNode" &&
            nativesharp::ast::astNodeKindName(maximumValue) == "UnknownAstNode",
        "TA-AST-023 the first out-of-domain ordinal and underlying maximum must map to UnknownAstNode");
}

// Separately freezes every pre-null-coalescing identity preserved from the historical catalog.
void testHistoricalOrdinals(TestContext& context)
{
    using Underlying = std::underlying_type_t<AstNodeKind>;
    for (std::size_t index = 0; index < HistoricalKindCount; ++index)
    {
        context.expect(static_cast<Underlying>(ExpectedAstKinds[index].kind) ==
                static_cast<Underlying>(index),
            "TA-AST-024 historical ordinal differs at position " + std::to_string(index));
    }
    context.expect(ExpectedAstKinds.front().kind == AstNodeKind::CompilationUnit &&
            ExpectedAstKinds[HistoricalKindCount - 1].kind == AstNodeKind::AggregateInitializerExpression,
        "TA-AST-024 historical range must remain CompilationUnit through AggregateInitializerExpression");
}

// Freezes the exact relative order of every kind appended from null-coalescing through loop.
void testAppendedKindOrder(TestContext& context)
{
    using Underlying = std::underlying_type_t<AstNodeKind>;
    context.expect(ExpectedAstKinds[HistoricalKindCount].kind == AstNodeKind::NullCoalescingExpression &&
            ExpectedAstKinds.back().kind == AstNodeKind::YieldBreakStatement,
        "TA-AST-025 appended range must remain NullCoalescingExpression through YieldBreakStatement");
    for (std::size_t index = HistoricalKindCount; index < ExpectedAstKinds.size(); ++index)
    {
        context.expect(static_cast<Underlying>(ExpectedAstKinds[index].kind) ==
                static_cast<Underlying>(index),
            "TA-AST-025 appended relative order differs at position " + std::to_string(index));
    }
}

// Uses stable backing pointers across repeated calls as the observable no-allocation contract.
void testStableRepeatedNames(TestContext& context)
{
    static_assert(noexcept(nativesharp::ast::astNodeKindName(AstNodeKind::CompilationUnit)));
    for (const ExpectedAstKind& expected : ExpectedAstKinds)
    {
        const std::string_view baseline = nativesharp::ast::astNodeKindName(expected.kind);
        for (int repetition = 0; repetition < 128; ++repetition)
        {
            const std::string_view repeated = nativesharp::ast::astNodeKindName(expected.kind);
            context.expect(repeated == baseline && repeated.data() == baseline.data(),
                "TA-AST-026 repeated calls must return stable non-owning string storage");
        }
    }
}
}

// Adds the complete CA-AST-003 catalog contract to the consolidated host registry.
void registerAstNodeKindModule(TestRegistry& registry)
{
    addAstNodeKindCase(registry, "TA-AST-019", testExactNames);
    addAstNodeKindCase(registry, "TA-AST-020", testUniqueNames);
    addAstNodeKindCase(registry, "TA-AST-021", testContiguousOrdinals);
    addAstNodeKindCase(registry, "TA-AST-022", testNegativeUnknownValue);
    addAstNodeKindCase(registry, "TA-AST-023", testUpperUnknownValues);
    addAstNodeKindCase(registry, "TA-AST-024", testHistoricalOrdinals);
    addAstNodeKindCase(registry, "TA-AST-025", testAppendedKindOrder);
    addAstNodeKindCase(registry, "TA-AST-026", testStableRepeatedNames);
}
}
