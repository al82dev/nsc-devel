#include "nsc_suite/TestRegistry.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/FileId.h"

#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::FileId;
using nativesharp::SourceLocation;
using nativesharp::SourceRange;
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;
using nativesharp::ast::GenericConstraintClause;
using nativesharp::ast::LifetimeOutlivesConstraint;
using nativesharp::ast::LifetimeParameterSyntax;
using nativesharp::ast::TypeSyntax;
using nativesharp::lexer::TokenKind;

// Builds an explicit range for lifetime and constraint record comparisons. Complexity is O(1).
constexpr SourceRange makeRange(std::uint32_t begin, std::uint32_t end)
{
    return SourceRange(SourceLocation(FileId{75}, begin), SourceLocation(FileId{75}, end));
}

// Compares all public coordinates of two ranges. Complexity is O(1).
constexpr bool sameRange(SourceRange left, SourceRange right)
{
    return left.begin() == right.begin() && left.end() == right.end();
}

// Registers one lifetime or generic-constraint case. Complexity is O(1).
void addLifetimeCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{
        std::move(id), "CA-AST-008", "Host.AST", "unit", "PR", {"ast", "node", "lifetime"},
        std::move(function),
    });
}

// Registers empty lifetime, outlives, constraint, and name state. Complexity is O(1).
void registerLifetimeDefaultsCase(TestRegistry& registry)
{
    addLifetimeCase(registry, "TA-AST-071", [](TestContext& context) {
        const AstNode node(AstNodeKind::CompilationUnit, {});
        context.expect(node.lifetimeParameters().empty(), "TA-AST-071 lifetime parameters must default empty");
        context.expect(node.lifetimeOutlivesConstraints().empty(),
            "TA-AST-071 outlives constraints must default empty");
        context.expect(node.genericConstraints().empty(), "TA-AST-071 generic constraints must default empty");
        context.expect(node.lifetimeName().empty(), "TA-AST-071 lifetime name must default empty");
    });
}

// Registers moved lifetime parameter ownership and insertion order. Complexity is O(1).
void registerLifetimeParametersCase(TestRegistry& registry)
{
    addLifetimeCase(registry, "TA-AST-072", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        const SourceRange firstRange = makeRange(1, 2);
        const SourceRange secondRange = makeRange(3, 5);
        node.addLifetimeParameter(LifetimeParameterSyntax{"a", firstRange});
        node.addLifetimeParameter(LifetimeParameterSyntax{"b", secondRange});
        const auto& values = node.lifetimeParameters();
        context.expect(values.size() == 2 && values[0].name == "a" && values[1].name == "b",
            "TA-AST-072 lifetime parameter order must be exact");
        context.expect(sameRange(values[0].range, firstRange) && sameRange(values[1].range, secondRange),
            "TA-AST-072 lifetime parameter ranges must be exact");
    });
}

// Registers outlives pair, range, and insertion-order preservation. Complexity is O(1).
void registerOutlivesCase(TestRegistry& registry)
{
    addLifetimeCase(registry, "TA-AST-073", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        const SourceRange firstRange = makeRange(6, 9);
        const SourceRange secondRange = makeRange(10, 13);
        node.addLifetimeOutlivesConstraint(LifetimeOutlivesConstraint{"a", "b", firstRange});
        node.addLifetimeOutlivesConstraint(LifetimeOutlivesConstraint{"b", "c", secondRange});
        const auto& values = node.lifetimeOutlivesConstraints();
        context.expect(values.size() == 2, "TA-AST-073 both outlives records must be present");
        context.expect(values[0].longer == "a" && values[0].shorter == "b",
            "TA-AST-073 first outlives pair must be exact");
        context.expect(values[1].longer == "b" && values[1].shorter == "c",
            "TA-AST-073 second outlives pair must be exact");
        context.expect(sameRange(values[0].range, firstRange) && sameRange(values[1].range, secondRange),
            "TA-AST-073 outlives ranges must be exact");
    });
}

// Registers owned lifetime-name storage from a temporary input. Complexity is O(n).
void registerLifetimeNameCase(TestRegistry& registry)
{
    addLifetimeCase(registry, "TA-AST-074", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.setLifetimeName(std::string("temporary-lifetime"));
        context.expect(node.lifetimeName() == "temporary-lifetime", "TA-AST-074 lifetime name must be owned");
    });
}

// Registers preservation of every observable field in a complete generic constraint. Complexity is O(1).
void registerCompleteConstraintCase(TestRegistry& registry)
{
    addLifetimeCase(registry, "TA-AST-075", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        TypeSyntax equality({}, TokenKind::Identifier, "Equality");
        TypeSyntax projection({}, TokenKind::Identifier, "Projection");
        TypeSyntax base({}, TokenKind::Identifier, "Base");
        TypeSyntax substituted({}, TokenKind::Identifier, "Substituted");
        GenericConstraintClause clause;
        clause.parameterName = "T";
        clause.parameterRange = makeRange(1, 2);
        clause.requiresClass = clause.requiresCopy = clause.requiresNotNull = true;
        clause.equalityConstraint = &equality;
        clause.associatedProjection = &projection;
        clause.typeConstraints = {&base};
        clause.range = makeRange(0, 20);
        clause.substitutedArgument = &substituted;
        node.addGenericConstraint(std::move(clause));
        const auto& value = node.genericConstraints()[0];
        context.expect(value.parameterName == "T" && value.requiresClass && value.requiresCopy &&
                value.requiresNotNull,
            "TA-AST-075 names and capability fields must be preserved");
        context.expect(value.equalityConstraint == &equality && value.associatedProjection == &projection &&
                value.substitutedArgument == &substituted,
            "TA-AST-075 pointer identities must be preserved");
        context.expect(value.typeConstraints.size() == 1 && value.typeConstraints[0] == &base,
            "TA-AST-075 type constraints must be preserved");
        context.expect(sameRange(value.parameterRange, makeRange(1, 2)) &&
                sameRange(value.range, makeRange(0, 20)),
            "TA-AST-075 both ranges must be preserved");
    });
}

// Registers insertion order and duplicates for generic constraints. Complexity is O(1).
void registerConstraintSequenceCase(TestRegistry& registry)
{
    addLifetimeCase(registry, "TA-AST-076", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        GenericConstraintClause first;
        first.parameterName = "T";
        GenericConstraintClause second;
        second.parameterName = "U";
        node.addGenericConstraint(first);
        node.addGenericConstraint(second);
        node.addGenericConstraint(first);
        const auto& values = node.genericConstraints();
        context.expect(values.size() == 3, "TA-AST-076 duplicate constraint must be retained");
        context.expect(values[0].parameterName == "T" && values[1].parameterName == "U" &&
                values[2].parameterName == "T",
            "TA-AST-076 constraint order must be exact");
    });
}

// Registers that clearing constraints leaves all lifetime state intact. Complexity is O(n).
void registerClearConstraintsCase(TestRegistry& registry)
{
    addLifetimeCase(registry, "TA-AST-077", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.addLifetimeParameter({"a", makeRange(1, 2)});
        node.addLifetimeOutlivesConstraint({"a", "b", makeRange(3, 4)});
        node.setLifetimeName("a");
        node.addGenericConstraint(GenericConstraintClause{});
        node.clearGenericConstraints();
        context.expect(node.genericConstraints().empty(), "TA-AST-077 generic constraints must clear");
        context.expect(node.lifetimeParameters().size() == 1,
            "TA-AST-077 lifetime parameters must survive constraint clear");
        context.expect(node.lifetimeOutlivesConstraints().size() == 1,
            "TA-AST-077 outlives constraints must survive constraint clear");
        context.expect(node.lifetimeName() == "a", "TA-AST-077 lifetime name must survive constraint clear");
    });
}

// Registers that exposed lifetime state remains accessible because no clear API exists. Complexity is O(1).
void registerPersistentLifetimeCase(TestRegistry& registry)
{
    addLifetimeCase(registry, "TA-AST-078", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        node.addLifetimeParameter({"a", makeRange(1, 2)});
        node.addLifetimeOutlivesConstraint({"a", "b", makeRange(3, 4)});
        node.clearGenericConstraints();
        context.expect(node.lifetimeParameters()[0].name == "a",
            "TA-AST-078 lifetime parameter must remain accessible");
        context.expect(node.lifetimeOutlivesConstraints()[0].shorter == "b",
            "TA-AST-078 outlives record must remain accessible");
    });
}

// Registers non-owning identity of every pointer held by a constraint. Complexity is O(1).
void registerNonOwningPointersCase(TestRegistry& registry)
{
    addLifetimeCase(registry, "TA-AST-079", [](TestContext& context) {
        AstNode node(AstNodeKind::CompilationUnit, {});
        TypeSyntax one({}, TokenKind::Identifier, "One");
        TypeSyntax two({}, TokenKind::Identifier, "Two");
        GenericConstraintClause clause;
        clause.equalityConstraint = &one;
        clause.associatedProjection = &two;
        clause.typeConstraints = {&one, &two};
        clause.substitutedArgument = &one;
        node.addGenericConstraint(std::move(clause));
        const auto& stored = node.genericConstraints()[0];
        context.expect(stored.equalityConstraint == &one && stored.associatedProjection == &two,
            "TA-AST-079 equality and projection pointers must not be cloned");
        context.expect(stored.typeConstraints[0] == &one && stored.typeConstraints[1] == &two,
            "TA-AST-079 type-constraint pointers must not be cloned");
        context.expect(stored.substitutedArgument == &one,
            "TA-AST-079 substituted pointer must remain non-owning identity");
    });
}
}

// Registers CA-AST-008 lifetime, outlives, and generic-constraint contracts.
void registerAstNodeLifetimeModule(TestRegistry& registry)
{
    registerLifetimeDefaultsCase(registry);
    registerLifetimeParametersCase(registry);
    registerOutlivesCase(registry);
    registerLifetimeNameCase(registry);
    registerCompleteConstraintCase(registry);
    registerConstraintSequenceCase(registry);
    registerClearConstraintsCase(registry);
    registerPersistentLifetimeCase(registry);
    registerNonOwningPointersCase(registry);
}
}
