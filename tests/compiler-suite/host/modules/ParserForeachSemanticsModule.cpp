#include "ParserFoundationTestSupport.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;

void addForeachCase(
    TestRegistry& registry,
    std::string id,
    TestFunction function,
    TestDisposition disposition = TestDisposition::Required)
{
    registry.add(TestCase{std::move(id), "CP-PAR-034", "Host.Parser", "component", "PR",
        {"parser", "foreach"}, std::move(function), disposition});
}

std::vector<const AstNode*> descendants(const AstNode* root)
{
    std::vector<const AstNode*> result;
    std::vector<const AstNode*> pending;
    if (root != nullptr) pending.push_back(root);
    while (!pending.empty())
    {
        const AstNode* const node = pending.back();
        pending.pop_back();
        result.push_back(node);
        if (node->first() != nullptr) pending.push_back(node->first());
        if (node->second() != nullptr) pending.push_back(node->second());
        if (node->third() != nullptr) pending.push_back(node->third());
        for (const AstNode* child : node->children())
        {
            if (child != nullptr) pending.push_back(child);
        }
    }
    return result;
}

std::size_t countText(const AstNode* root, std::string_view text)
{
    std::size_t count = 0;
    for (const AstNode* node : descendants(root)) count += node->text() == text ? 1U : 0U;
    return count;
}

std::size_t countToken(const AstNode* root, TokenKind kind)
{
    std::size_t count = 0;
    for (const AstNode* node : descendants(root)) count += node->tokenKind() == kind ? 1U : 0U;
    return count;
}

const AstNode* firstKindText(
    const AstNode* root,
    AstNodeKind kind,
    std::string_view text)
{
    for (const AstNode* node : descendants(root))
    {
        if (node->kind() == kind && node->text() == text) return node;
    }
    return nullptr;
}

void testExclusiveAndInclusiveRanges(TestContext& context)
{
    ParserFixture exclusive("foreach (var item in 1..4) {}");
    ParserFixture inclusive("foreach (var item in 1..=4) {}");
    AstNode* const first = exclusive.parser().parseStatement();
    AstNode* const second = inclusive.parser().parseStatement();
    context.expect(first != nullptr && second != nullptr &&
            first->kind() == AstNodeKind::BlockStatement &&
            second->kind() == AstNodeKind::BlockStatement &&
            first->auxiliaryText() == "foreach:integer-range-exclusive" &&
            second->auxiliaryText() == "foreach:integer-range-inclusive" &&
            countToken(first, TokenKind::Less) >= 2 &&
            countToken(second, TokenKind::LessEqual) >= 1 &&
            exclusive.diagnostics().diagnostics().empty() &&
            inclusive.diagnostics().diagnostics().empty(),
        "TP-PAR-395 exclusive/inclusive ranges must lower distinct loop bounds");
}

void testArrayCandidate(TestContext& context)
{
    ParserFixture fixture("foreach (var item in array) { consume(item); }");
    AstNode* const node = fixture.parser().parseStatement();
    context.expect(node != nullptr && node->kind() == AstNodeKind::ForeachStatement &&
            node->first() != nullptr && node->first()->text() == "$__foreach_0_source" &&
            node->second() != nullptr &&
            countText(node->second(), "$__foreach_0_index") >= 4 &&
            countText(node->second(), "Length") == 1 &&
            countText(node->second(), "item") >= 1,
        "TP-PAR-396 foreach array candidate must own source, length, index, and element local");
}

void testSpanCandidate(TestContext& context)
{
    ParserFixture fixture("foreach (var item in span) { consume(item); }");
    AstNode* const node = fixture.parser().parseStatement();
    const AstNode* const candidate =
        node != nullptr && node->children().size() >= 2 ? node->children()[1] : nullptr;
    context.expect(candidate != nullptr && countText(candidate, "Length") == 1 &&
            countText(candidate, "Data") == 1 &&
            countText(candidate, "$__foreach_0_index") >= 4 && countText(candidate, "item") >= 1,
        "TP-PAR-397 foreach span candidate must use Length and Data/index protocol");
}

void testEnumeratorCandidate(TestContext& context)
{
    ParserFixture fixture("foreach (var item in sequence) { consume(item); }");
    AstNode* const node = fixture.parser().parseStatement();
    const AstNode* const candidate = node != nullptr ? node->third() : nullptr;
    context.expect(candidate != nullptr && countText(candidate, "GetEnumerator") == 1 &&
            countText(candidate, "MoveNext") == 1 && countText(candidate, "Current") == 1 &&
            countText(candidate, "$__foreach_0_enumerator") >= 3 &&
            countText(candidate, "item") >= 1,
        "TP-PAR-398 enumerator candidate must use GetEnumerator/MoveNext/Current exactly");
}

void testBindingModes(TestContext& context)
{
    ParserFixture value("foreach (var item in source) {}");
    ParserFixture reference("foreach (ref var item in source) {}");
    ParserFixture mutableReference("foreach (ref mut var item in source) {}");
    ParserFixture owned("foreach (raii var item in source) {}");
    AstNode* const valueNode = value.parser().parseStatement();
    AstNode* const refNode = reference.parser().parseStatement();
    AstNode* const mutNode = mutableReference.parser().parseStatement();
    AstNode* const ownedNode = owned.parser().parseStatement();
    const AstNodeKind variable = AstNodeKind::VariableDeclarationStatement;
    const AstNode* const valueDecl = firstKindText(valueNode, variable, "item");
    const AstNode* const refDecl = firstKindText(refNode, variable, "item");
    const AstNode* const mutDecl = firstKindText(mutNode, variable, "item");
    const AstNode* const ownedDecl = firstKindText(ownedNode, variable, "item");
    context.expect(valueNode != nullptr && refNode != nullptr && mutNode != nullptr &&
            ownedNode != nullptr && valueNode->auxiliaryText() == "foreach:value" &&
            refNode->auxiliaryText() == "foreach:ref" &&
            mutNode->auxiliaryText() == "foreach:ref-mut" && valueDecl != nullptr &&
            refDecl != nullptr && refDecl->auxiliaryText() == "local:foreach-ref" &&
            mutDecl != nullptr && mutDecl->auxiliaryText() == "local:foreach-ref-mut" &&
            ownedDecl != nullptr && ownedDecl->hasModifier(TokenKind::KwRaii),
        "TP-PAR-399 value, borrowed ref, mutable ref, and owned bindings must retain flags");
}

void testDeconstructionBinding(TestContext& context)
{
    ParserFixture fixture("foreach (var (left, right) in pairs) { consume(left, right); }");
    AstNode* const node = fixture.parser().parseStatement();
    context.expect(node != nullptr && node->kind() == AstNodeKind::ForeachStatement &&
            countText(node, "left") >= 1 && countText(node, "right") >= 1 &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-400 foreach deconstruction must lower both pattern bindings exactly");
}
}

void registerParserForeachSemanticsModule(TestRegistry& registry)
{
    addForeachCase(registry, "TP-PAR-395", testExclusiveAndInclusiveRanges);
    addForeachCase(registry, "TP-PAR-396", testArrayCandidate);
    addForeachCase(registry, "TP-PAR-397", testSpanCandidate);
    addForeachCase(registry, "TP-PAR-398", testEnumeratorCandidate);
    addForeachCase(registry, "TP-PAR-399", testBindingModes);
    addForeachCase(registry, "TP-PAR-400", testDeconstructionBinding);
}
}
