#include "nsc_suite/TestRegistry.h"

#include "nativesharp/AST/AstDumper.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::ast;

// Dumps a possibly-null root to an owned byte string.
std::string dumpTree(const AstNode* root)
{
    std::ostringstream output;
    AstDumper{}.dump(root, output);
    return output.str();
}

// Builds a source-location-independent node fixture with optional text identity.
std::unique_ptr<AstNode> makeNode(AstNodeKind kind, std::string text = {})
{
    auto node = std::make_unique<AstNode>(kind, nativesharp::SourceRange{});
    node->setText(std::move(text));
    return node;
}

// Registers one CA-AST-020 case with common component metadata.
void addTraversalCase(TestRegistry& registry, std::string id, std::string profile,
    TestFunction function)
{
    registry.add(TestCase{std::move(id), "CA-AST-020", "Host.AST", "robustness",
        std::move(profile), {"ast", "dumper", "traversal"}, std::move(function)});
}

// Freezes the null-root behavior as an empty byte sequence.
void testNullRoot(TestContext& context)
{
    context.expect(dumpTree(nullptr).empty(),
        "TA-AST-217 null root must produce no output bytes");
}

// Freezes preorder traversal and edge labels for the three dedicated slots.
void testThreeDedicatedSlots(TestContext& context)
{
    auto root = makeNode(AstNodeKind::CompilationUnit);
    auto first = makeNode(AstNodeKind::IdentifierExpression, "A");
    auto second = makeNode(AstNodeKind::IdentifierExpression, "B");
    auto third = makeNode(AstNodeKind::IdentifierExpression, "C");
    root->setFirst(first.get());
    root->setSecond(second.get());
    root->setThird(third.get());
    context.expect(dumpTree(root.get()) ==
            "CompilationUnit\n"
            "  first: IdentifierExpression \"A\"\n"
            "  second: IdentifierExpression \"B\"\n"
            "  third: IdentifierExpression \"C\"\n",
        "TA-AST-218 dedicated slots must dump in first, second, third preorder");
}

// Proves ordinary children follow all dedicated slots and retain insertion order.
void testChildrenAfterSlots(TestContext& context)
{
    auto root = makeNode(AstNodeKind::CompilationUnit);
    auto first = makeNode(AstNodeKind::IdentifierExpression, "slot");
    auto childA = makeNode(AstNodeKind::IdentifierExpression, "A");
    auto childB = makeNode(AstNodeKind::IdentifierExpression, "B");
    root->setFirst(first.get());
    root->addChild(childA.get());
    root->addChild(childB.get());
    context.expect(dumpTree(root.get()) ==
            "CompilationUnit\n"
            "  first: IdentifierExpression \"slot\"\n"
            "  child: IdentifierExpression \"A\"\n"
            "  child: IdentifierExpression \"B\"\n",
        "TA-AST-219 children must follow slots and preserve insertion order");
}

// Characterizes null pointers in both dedicated slots and the child vector as silent.
void testNullEdgesAreSilent(TestContext& context)
{
    auto root = makeNode(AstNodeKind::CompilationUnit);
    auto child = makeNode(AstNodeKind::IdentifierExpression, "live");
    root->setFirst(nullptr);
    root->setSecond(nullptr);
    root->setThird(nullptr);
    root->addChild(nullptr);
    root->addChild(child.get());
    root->addChild(nullptr);
    context.expect(dumpTree(root.get()) ==
            "CompilationUnit\n  child: IdentifierExpression \"live\"\n",
        "TA-AST-220 null slots and null children must not produce lines");
}

// Verifies zero, one, and multiple depths use exactly two spaces per level.
void testIndentationDepth(TestContext& context)
{
    auto root = makeNode(AstNodeKind::CompilationUnit);
    auto levelOne = makeNode(AstNodeKind::BlockStatement);
    auto levelTwo = makeNode(AstNodeKind::IdentifierExpression);
    root->setFirst(levelOne.get());
    levelOne->setFirst(levelTwo.get());
    context.expect(dumpTree(root.get()) ==
            "CompilationUnit\n"
            "  first: BlockStatement\n"
            "    first: IdentifierExpression\n",
        "TA-AST-221 indentation must add exactly two spaces per depth");
}

// Confirms the dumper is edge-based and emits a shared DAG node once per incoming edge.
void testRepeatedDagNode(TestContext& context)
{
    auto root = makeNode(AstNodeKind::CompilationUnit);
    auto shared = makeNode(AstNodeKind::IdentifierExpression, "shared");
    root->setFirst(shared.get());
    root->addChild(shared.get());
    context.expect(dumpTree(root.get()) ==
            "CompilationUnit\n"
            "  first: IdentifierExpression \"shared\"\n"
            "  child: IdentifierExpression \"shared\"\n",
        "TA-AST-222 shared DAG node must print once for each edge");
}

// Exercises a deep finite tree and validates every indentation and edge label byte.
void testDeepTreeOutput(TestContext& context)
{
    constexpr std::size_t Depth = 256;
    std::vector<std::unique_ptr<AstNode>> nodes;
    nodes.reserve(Depth + 1);
    for (std::size_t index = 0; index <= Depth; ++index)
    {
        nodes.push_back(makeNode(AstNodeKind::IdentifierExpression));
        if (index != 0) nodes[index - 1]->setFirst(nodes[index].get());
    }
    std::string expected;
    for (std::size_t depth = 0; depth <= Depth; ++depth)
    {
        expected.append(depth * 2, ' ');
        if (depth != 0) expected += "first: ";
        expected += "IdentifierExpression\n";
    }
    context.expect(dumpTree(nodes.front().get()) == expected,
        "TA-AST-225 deep finite tree must preserve complete recursive output");
}

// Characterizes the unsafe channel as intentionally absent from tree dumps.
void testUnsafeOmission(TestContext& context)
{
    auto baseline = makeNode(AstNodeKind::FunctionDeclaration);
    auto variant = makeNode(AstNodeKind::FunctionDeclaration);
    variant->setUnsafe(true);
    context.expect(dumpTree(baseline.get()) == dumpTree(variant.get()),
        "TA-AST-226 trees differing only in unsafe must dump identically");
}

// Characterizes integerValue as intentionally absent from tree dumps.
void testIntegerValueOmission(TestContext& context)
{
    auto baseline = makeNode(AstNodeKind::IntegerLiteralExpression);
    auto variant = makeNode(AstNodeKind::IntegerLiteralExpression);
    variant->setIntegerValue(0xDEADBEEF);
    context.expect(dumpTree(baseline.get()) == dumpTree(variant.get()),
        "TA-AST-227 trees differing only in integerValue must dump identically");
}

// Verifies repeated traversal of an unchanged tree is byte deterministic.
void testRepeatedDumpDeterminism(TestContext& context)
{
    auto root = makeNode(AstNodeKind::CompilationUnit);
    auto child = makeNode(AstNodeKind::IdentifierExpression, "stable");
    root->addChild(child.get());
    const std::string first = dumpTree(root.get());
    const std::string second = dumpTree(root.get());
    context.expect(first == second,
        "TA-AST-228 repeated dump of one tree must be byte deterministic");
}

// Proves child-vector order directly controls output-line order.
void testPermutedChildOrder(TestContext& context)
{
    auto firstRoot = makeNode(AstNodeKind::CompilationUnit);
    auto secondRoot = makeNode(AstNodeKind::CompilationUnit);
    auto childA = makeNode(AstNodeKind::IdentifierExpression, "A");
    auto childB = makeNode(AstNodeKind::IdentifierExpression, "B");
    firstRoot->addChild(childA.get());
    firstRoot->addChild(childB.get());
    secondRoot->addChild(childB.get());
    secondRoot->addChild(childA.get());
    const std::string first = dumpTree(firstRoot.get());
    const std::string second = dumpTree(secondRoot.get());
    context.expect(first ==
            "CompilationUnit\n"
            "  child: IdentifierExpression \"A\"\n"
            "  child: IdentifierExpression \"B\"\n" &&
            second ==
            "CompilationUnit\n"
            "  child: IdentifierExpression \"B\"\n"
            "  child: IdentifierExpression \"A\"\n" &&
            first != second,
        "TA-AST-229 permuting children must permute the corresponding output lines");
}

// Freezes a complete topology containing all edge kinds and nested descendants.
void testCompleteTopologyGoldenBytes(TestContext& context)
{
    auto root = makeNode(AstNodeKind::FunctionDeclaration, "Root");
    auto first = makeNode(AstNodeKind::IdentifierExpression, "first");
    auto second = makeNode(AstNodeKind::IdentifierExpression, "second");
    auto third = makeNode(AstNodeKind::IdentifierExpression, "third");
    auto childA = makeNode(AstNodeKind::BlockStatement, "A");
    auto childB = makeNode(AstNodeKind::ReturnStatement, "B");
    auto grandchild = makeNode(AstNodeKind::IntegerLiteralExpression, "value");
    root->setFirst(first.get());
    root->setSecond(second.get());
    root->setThird(third.get());
    root->addChild(childA.get());
    root->addChild(childB.get());
    childA->setSecond(grandchild.get());
    context.expect(dumpTree(root.get()) ==
            "FunctionDeclaration \"Root\"\n"
            "  first: IdentifierExpression \"first\"\n"
            "  second: IdentifierExpression \"second\"\n"
            "  third: IdentifierExpression \"third\"\n"
            "  child: BlockStatement \"A\"\n"
            "    second: IntegerLiteralExpression \"value\"\n"
            "  child: ReturnStatement \"B\"\n",
        "TA-AST-230 complete topology golden bytes, including final newline, must be exact");
}
}

// Adds all CA-AST-020 cases executable without watchdog-isolated cyclic traversal.
void registerAstDumperTraversalModule(TestRegistry& registry)
{
    addTraversalCase(registry, "TA-AST-217", "PR", testNullRoot);
    addTraversalCase(registry, "TA-AST-218", "PR", testThreeDedicatedSlots);
    addTraversalCase(registry, "TA-AST-219", "PR", testChildrenAfterSlots);
    addTraversalCase(registry, "TA-AST-220", "PR", testNullEdgesAreSilent);
    addTraversalCase(registry, "TA-AST-221", "PR", testIndentationDepth);
    addTraversalCase(registry, "TA-AST-222", "PR", testRepeatedDagNode);
    addTraversalCase(registry, "TA-AST-225", "NIGHTLY", testDeepTreeOutput);
    addTraversalCase(registry, "TA-AST-226", "PR", testUnsafeOmission);
    addTraversalCase(registry, "TA-AST-227", "PR", testIntegerValueOmission);
    addTraversalCase(registry, "TA-AST-228", "PR", testRepeatedDumpDeterminism);
    addTraversalCase(registry, "TA-AST-229", "PR", testPermutedChildOrder);
    addTraversalCase(registry, "TA-AST-230", "PR", testCompleteTopologyGoldenBytes);
}
}
