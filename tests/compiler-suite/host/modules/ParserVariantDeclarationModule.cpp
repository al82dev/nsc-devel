#include "ParserDeclarationTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace parser_declaration_support;

void testPayloadlessVariant(TestContext& context)
{
    DeclarationFixture fixture("variant Maybe { None }");
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && hasMarker(node, "variant") &&
            childNamed(node, "__nsc_variant_tag") != nullptr && childNamed(node, "None") != nullptr &&
            childNamed(node, "IsNone") != nullptr && childNamed(node, "CaseTag") != nullptr &&
            node->children().size() == 4,
        "TP-PAR-165 payloadless variant must synthesize tag, factory, test, and CaseTag");
}

void testUnaryPayloadVariant(TestContext& context)
{
    DeclarationFixture fixture("variant Maybe { Some(int) }");
    AstNode* const node = declaration(fixture);
    AstNode* const payloadUnion = childNamed(node, "__nsc_variant_payload_Maybe");
    AstNode* const factory = childNamed(node, "Some");
    context.expect(node != nullptr && payloadUnion != nullptr && hasMarker(payloadUnion, "union") &&
            childNamed(payloadUnion, "__nsc_case_Some") != nullptr &&
            childNamed(node, "__nsc_variant_payload") != nullptr && factory != nullptr &&
            factory->children().size() == 1 && factory->children()[0]->typeSyntax() != nullptr &&
            factory->children()[0]->typeSyntax()->baseName() == "int" &&
            childNamed(node, "__nsc_variant_extract_Some") != nullptr,
        "TP-PAR-166 unary payload must align union field, factory parameter, and extractor");
}

void testTuplePayloadVariant(TestContext& context)
{
    DeclarationFixture fixture("variant PairValue { Pair((int left, long right)) }");
    AstNode* const node = declaration(fixture);
    AstNode* const payloadUnion = childNamed(node, "__nsc_variant_payload_PairValue");
    AstNode* const payload = childNamed(payloadUnion, "__nsc_case_Pair");
    AstNode* const factory = childNamed(node, "Pair");
    const auto* const type = payload == nullptr ? nullptr : payload->typeSyntax();
    context.expect(type != nullptr && type->isTuple() && type->typeArguments().size() == 2 &&
            type->tupleElementNames() == std::vector<std::string>({"left", "right"}) &&
            factory != nullptr && factory->children().size() == 1 &&
            factory->children()[0]->typeSyntax() == type,
        "TP-PAR-167 tuple payload fields and factory parameters must share exact syntax");
}

void testVariantOrderAndTags(TestContext& context)
{
    DeclarationFixture fixture("variant Choice { First, Second(int), Third }");
    AstNode* const node = declaration(fixture);
    AstNode* const first = childNamed(node, "First");
    AstNode* const second = childNamed(node, "Second");
    AstNode* const third = childNamed(node, "Third");
    context.expect(node != nullptr && hasMarker(node, "cases=First|Second:payload|Third") &&
            hasMarker(first, "variant-case-factory:First:0") &&
            hasMarker(second, "variant-case-factory:Second:1") &&
            hasMarker(third, "variant-case-factory:Third:2"),
        "TP-PAR-168 variant alternatives must preserve declaration order and monotonic tags");
}

void testExplicitDiscriminantRejected(TestContext& context)
{
    DeclarationFixture fixture("variant Code { Zero = 4, One }");
    AstNode* const node = declaration(fixture);
    AstNode* const zero = childNamed(node, "Zero");
    AstNode* const one = childNamed(node, "One");
    context.expect(node != nullptr && !fixture.diagnostics().empty() &&
            hasMarker(zero, "variant-case-factory:Zero:0") &&
            hasMarker(one, "variant-case-factory:One:1"),
        "TP-PAR-169 explicit discriminants must diagnose and recover with ordinal tags");
}

void testDuplicateVariantName(TestContext& context)
{
    DeclarationFixture fixture("variant Bad { Same, Same }");
    context.expect(fixture.diagnosticCount(DiagnosticId::DuplicateSymbol) == 1,
        "TP-PAR-170 duplicate variant names must report DuplicateSymbol");
}

void testInvalidPayloadRecovery(TestContext& context)
{
    DeclarationFixture fixture("variant Recovered { Bad(void), Good(int) }");
    AstNode* const node = declaration(fixture);
    context.expect(node != nullptr && fixture.diagnosticCount(DiagnosticId::InvalidType) == 1 &&
            childNamed(node, "Good") != nullptr && childNamed(node, "IsGood") != nullptr,
        "TP-PAR-171 invalid payload must diagnose and recover to the following alternative");
}

void testGenericVariantSynthesis(TestContext& context)
{
    DeclarationFixture fixture("variant Maybe<T> where T : class { None, Some(T) }");
    AstNode* const node = declaration(fixture);
    AstNode* const some = childNamed(node, "Some");
    context.expect(node != nullptr && node->genericParameters() == std::vector<std::string>{"T"} &&
            node->genericConstraints().size() == 1 && node->genericConstraints()[0].requiresClass &&
            some != nullptr && some->typeSyntax() != nullptr && some->typeSyntax()->baseName() == "Maybe" &&
            some->typeSyntax()->typeArguments().size() == 1 &&
            some->typeSyntax()->typeArguments()[0]->baseName() == "T",
        "TP-PAR-172 generic variant synthesis must reference declaration parameters exactly");
}

void testCopyVariantMarker(TestContext& context)
{
    DeclarationFixture fixture("copy variant Copyable { Value(int) } variant MoveOnly { Value(int) }");
    AstNode* const copyable = declaration(fixture);
    AstNode* const moveOnly = declaration(fixture, 1);
    context.expect(hasMarker(copyable, "copy") && !hasMarker(moveOnly, "copy") &&
            hasMarker(copyable, "variant") && hasMarker(moveOnly, "variant"),
        "TP-PAR-173 copy marker must appear only when explicitly written");
}

void testEmptyAndUnclosedVariants(TestContext& context)
{
    DeclarationFixture empty("variant Empty { }");
    DeclarationFixture unclosed("variant Open { One");
    AstNode* const emptyNode = declaration(empty);
    AstNode* const openNode = declaration(unclosed);
    context.expect(emptyNode != nullptr && !empty.diagnostics().empty() && openNode != nullptr &&
            !unclosed.diagnostics().empty() &&
            openNode->range().begin().offset() <= openNode->range().end().offset(),
        "TP-PAR-174 empty and unclosed variants must diagnose and terminate with finite progress");
}
}

void registerParserVariantDeclarationModule(TestRegistry& registry)
{
    addDeclarationCase(registry, "TP-PAR-165", "CP-PAR-017", "PR", testPayloadlessVariant);
    addDeclarationCase(registry, "TP-PAR-166", "CP-PAR-017", "PR", testUnaryPayloadVariant);
    addDeclarationCase(registry, "TP-PAR-167", "CP-PAR-017", "PR", testTuplePayloadVariant);
    addDeclarationCase(registry, "TP-PAR-168", "CP-PAR-017", "PR", testVariantOrderAndTags);
    addDeclarationCase(
        registry,
        "TP-PAR-169",
        "CP-PAR-017",
        "PR",
        testExplicitDiscriminantRejected);
    addDeclarationCase(registry, "TP-PAR-170", "CP-PAR-017", "PR", testDuplicateVariantName);
    addDeclarationCase(registry, "TP-PAR-171", "CP-PAR-017", "PR", testInvalidPayloadRecovery);
    addDeclarationCase(registry, "TP-PAR-172", "CP-PAR-017", "PR", testGenericVariantSynthesis);
    addDeclarationCase(registry, "TP-PAR-173", "CP-PAR-017", "PR", testCopyVariantMarker);
    addDeclarationCase(registry, "TP-PAR-174", "CP-PAR-017", "PR", testEmptyAndUnclosedVariants);
}
}
