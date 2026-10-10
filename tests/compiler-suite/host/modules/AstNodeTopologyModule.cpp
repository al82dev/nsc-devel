#include "nsc_suite/TestRegistry.h"

#include "nativesharp/AST/AstNode.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;

// Registers one AstNode topology case with canonical metadata. Complexity is O(1).
void addTopologyCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{
        std::move(id), "CA-AST-009", "Host.AST", "unit", std::move(profile), {"ast", "node", "topology"},
        std::move(function),
    });
}

// Registers independent null defaults for all three fixed edges. Complexity is O(1).
void registerSlotDefaultsCase(TestRegistry& registry)
{
    addTopologyCase(registry, "TA-AST-080", "PR", [](TestContext& context) {
        const AstNode node(AstNodeKind::CompilationUnit, {});
        context.expect(node.first() == nullptr, "TA-AST-080 first must default null");
        context.expect(node.second() == nullptr, "TA-AST-080 second must default null");
        context.expect(node.third() == nullptr, "TA-AST-080 third must default null");
    });
}

// Registers exact non-owning identity in all three fixed edges. Complexity is O(1).
void registerSlotIdentityCase(TestRegistry& registry)
{
    addTopologyCase(registry, "TA-AST-081", "PR", [](TestContext& context) {
        AstNode root(AstNodeKind::CompilationUnit, {});
        AstNode first(AstNodeKind::NamespaceDeclaration, {});
        AstNode second(AstNodeKind::FunctionDeclaration, {});
        AstNode third(AstNodeKind::BlockStatement, {});
        root.setFirst(&first);
        root.setSecond(&second);
        root.setThird(&third);
        context.expect(root.first() == &first, "TA-AST-081 first identity must be exact");
        context.expect(root.second() == &second, "TA-AST-081 second identity must be exact");
        context.expect(root.third() == &third, "TA-AST-081 third identity must be exact");
    });
}

// Registers resetting one fixed edge without mutating either sibling edge. Complexity is O(1).
void registerResetSlotCase(TestRegistry& registry)
{
    addTopologyCase(registry, "TA-AST-082", "PR", [](TestContext& context) {
        AstNode root(AstNodeKind::CompilationUnit, {});
        AstNode first(AstNodeKind::NamespaceDeclaration, {});
        AstNode second(AstNodeKind::FunctionDeclaration, {});
        AstNode third(AstNodeKind::BlockStatement, {});
        root.setFirst(&first);
        root.setSecond(&second);
        root.setThird(&third);
        root.setSecond(nullptr);
        context.expect(root.first() == &first && root.third() == &third,
            "TA-AST-082 sibling fixed edges must remain intact");
        context.expect(root.second() == nullptr, "TA-AST-082 selected fixed edge must reset null");
    });
}

// Registers that the variable child list preserves null entries. Complexity is O(1).
void registerNullChildCase(TestRegistry& registry)
{
    addTopologyCase(registry, "TA-AST-083", "PR", [](TestContext& context) {
        AstNode root(AstNodeKind::CompilationUnit, {});
        root.addChild(nullptr);
        context.expect(root.children().size() == 1 && root.children()[0] == nullptr,
            "TA-AST-083 null child must be preserved");
    });
}

// Registers that the variable child list preserves duplicate pointers. Complexity is O(1).
void registerDuplicateChildCase(TestRegistry& registry)
{
    addTopologyCase(registry, "TA-AST-084", "PR", [](TestContext& context) {
        AstNode root(AstNodeKind::CompilationUnit, {});
        AstNode child(AstNodeKind::BlockStatement, {});
        root.addChild(&child);
        root.addChild(&child);
        context.expect(root.children().size() == 2, "TA-AST-084 duplicate child must increase size");
        context.expect(root.children()[0] == &child && root.children()[1] == &child,
            "TA-AST-084 duplicate identities must be exact");
    });
}

// Registers prepend before an existing append sequence. Complexity is O(n).
void registerPrependOrderCase(TestRegistry& registry)
{
    addTopologyCase(registry, "TA-AST-085", "PR", [](TestContext& context) {
        AstNode root(AstNodeKind::CompilationUnit, {});
        AstNode a(AstNodeKind::NamespaceDeclaration, {});
        AstNode b(AstNodeKind::FunctionDeclaration, {});
        AstNode c(AstNodeKind::BlockStatement, {});
        root.addChild(&a);
        root.addChild(&b);
        root.prependChild(&c);
        const auto& children = root.children();
        context.expect(children.size() == 3, "TA-AST-085 child count must be exact");
        context.expect(children[0] == &c && children[1] == &a && children[2] == &b,
            "TA-AST-085 prepend and append order must be C A B");
    });
}

// Registers repeated prepend semantics where each new element becomes first. Complexity is O(n squared).
void registerRepeatedPrependCase(TestRegistry& registry)
{
    addTopologyCase(registry, "TA-AST-086", "PR", [](TestContext& context) {
        AstNode root(AstNodeKind::CompilationUnit, {});
        AstNode a(AstNodeKind::NamespaceDeclaration, {});
        AstNode b(AstNodeKind::FunctionDeclaration, {});
        AstNode c(AstNodeKind::BlockStatement, {});
        root.prependChild(&a);
        root.prependChild(&b);
        root.prependChild(&c);
        const auto& children = root.children();
        context.expect(children[0] == &c && children[1] == &b && children[2] == &a,
            "TA-AST-086 each prepended child must become first");
    });
}

// Registers the same non-owning pointer in a fixed edge and in children. Complexity is O(1).
void registerSharedEdgeCase(TestRegistry& registry)
{
    addTopologyCase(registry, "TA-AST-087", "PR", [](TestContext& context) {
        AstNode root(AstNodeKind::CompilationUnit, {});
        AstNode child(AstNodeKind::BlockStatement, {});
        root.setFirst(&child);
        root.addChild(&child);
        context.expect(root.first() == &child && root.children()[0] == &child,
            "TA-AST-087 fixed and variable edges may share identity");
    });
}

// Registers that topology storage accepts a direct self-cycle without validation. Complexity is O(1).
void registerSelfCycleCase(TestRegistry& registry)
{
    addTopologyCase(registry, "TA-AST-088", "PR", [](TestContext& context) {
        AstNode root(AstNodeKind::CompilationUnit, {});
        root.setFirst(&root);
        root.addChild(&root);
        context.expect(root.first() == &root, "TA-AST-088 fixed self-cycle must be retained");
        context.expect(root.children().size() == 1 && root.children()[0] == &root,
            "TA-AST-088 child self-cycle must be retained");
    });
}

// Registers exact order after many prepends, characterizing the current O(n) operation. Complexity is O(n squared).
void registerPrependScaleCase(TestRegistry& registry)
{
    addTopologyCase(registry, "TA-AST-089", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t count = 2048;
        AstNode root(AstNodeKind::CompilationUnit, {});
        std::vector<AstNode> nodes;
        nodes.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            nodes.emplace_back(AstNodeKind::BlockStatement, nativesharp::SourceRange{});
            root.prependChild(&nodes.back());
        }
        const auto& children = root.children();
        context.expect(children.size() == count, "TA-AST-089 every prepended child must be retained");
        for (std::size_t index = 0; index < count; ++index)
        {
            context.expect(children[index] == &nodes[count - index - 1],
                "TA-AST-089 large prepend order must be exact reverse insertion order");
        }
    });
}
}

// Registers CA-AST-009 fixed-edge and variable-child topology contracts.
void registerAstNodeTopologyModule(TestRegistry& registry)
{
    registerSlotDefaultsCase(registry);
    registerSlotIdentityCase(registry);
    registerResetSlotCase(registry);
    registerNullChildCase(registry);
    registerDuplicateChildCase(registry);
    registerPrependOrderCase(registry);
    registerRepeatedPrependCase(registry);
    registerSharedEdgeCase(registry);
    registerSelfCycleCase(registry);
    registerPrependScaleCase(registry);
}
}
