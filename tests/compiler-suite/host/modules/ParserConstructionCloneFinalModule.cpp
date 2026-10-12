#include "ParserFinalTestSupport.h"
#include "ParserConstructionCloneTestSupport.h"

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace parser_final_support;
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::lexer::TokenKind;

void testMakeHelpers(TestContext& context)
{
    ParserFixture fixture("");
    Parser& parser = fixture.parser();
    const nativesharp::SourceRange range = parser.current().range();
    AstNode* id = parser.makeIdentifierExpression(range, "item");
    AstNode* integer = parser.makeIntegerLiteralExpression(range, 7);
    AstNode* member = parser.makeMemberExpression(id, "Field");
    AstNode* call = parser.makeZeroArgCallExpression(member);
    AstNode* index = parser.makeIndexExpression(id, integer);
    AstNode* binary = parser.makeBinaryExpression(TokenKind::Plus, integer, integer);
    AstNode* assignment = parser.makeAssignmentExpression(id, integer);
    AstNode* statement = parser.makeExpressionStatement(call);
    TypeSyntax* type = parser.arena_.make<TypeSyntax>(range, TokenKind::KwInt, "int");
    AstNode* variable = parser.makeVariableDeclaration(type, "value", integer, false, true);
    AstNode* error = parser.makeErrorExpression();
    context.expect(id->kind() == AstNodeKind::IdentifierExpression && id->text() == "item" &&
            integer->kind() == AstNodeKind::IntegerLiteralExpression && integer->integerValue() == 7 &&
            member->kind() == AstNodeKind::MemberExpression && member->first() == id && member->text() == "Field" &&
            call->kind() == AstNodeKind::CallExpression && call->first() == member &&
            index->kind() == AstNodeKind::IndexExpression && index->first() == id && index->second() == integer &&
            binary->kind() == AstNodeKind::BinaryExpression && binary->tokenKind() == TokenKind::Plus &&
            assignment->kind() == AstNodeKind::AssignmentExpression && statement->first() == call &&
            variable->kind() == AstNodeKind::VariableDeclarationStatement && variable->typeSyntax() == type &&
            variable->first() == integer && variable->text() == "value" && variable->flag() &&
            error->text() == "<error>" && id->range().begin() == range.begin(),
        "TP-PAR-513 Parser make helpers must preserve exact kind, topology, payload, operator, and range");
}

void testCloneTypeCompleteAndIndependent(TestContext& context)
{
    AstArena sourceArena;
    AstArena destinationArena;
    TypeSyntax* source = sourceArena.make<TypeSyntax>(
        nativesharp::SourceRange{}, TokenKind::Identifier, "CompleteType");
    source->setOwnershipQualifier(OwnershipQualifierSyntaxKind::Borrowed);
    source->setLifetimeName("borrowed_lifetime");
    source->setDynamicTrait();
    source->setOpaqueResult();
    source->setOpaqueResultTraitName("Display");
    source->setExplicitTraitImplementation();
    source->setPointeeConst();
    source->setTuple();
    TypeSyntax* tupleElement = sourceArena.make<TypeSyntax>(
        nativesharp::SourceRange{}, TokenKind::KwInt, "int");
    source->addTupleElement(tupleElement, "value");
    source->setFunction();
    source->setFunctionCarrier(FunctionTypeCarrierSyntaxKind::Reference);
    source->setFunctionCallingConvention(FunctionCallingConventionSyntaxKind::Win64);
    source->setFunctionReturnType(sourceArena.make<TypeSyntax>(
        nativesharp::SourceRange{}, TokenKind::KwLong, "long"));
    source->setFunctionErrorType(sourceArena.make<TypeSyntax>(
        nativesharp::SourceRange{}, TokenKind::Identifier, "Failure"));
    source->setFunctionVariadic();
    source->setFunctionUnsafe();
    source->setFunctionNoHeap();
    source->setFunctionAsync();
    source->setFunctionNullable();
    source->setFunctionLifetimeName("call_lifetime");
    AstNode* constExpression = sourceArena.make<AstNode>(
        AstNodeKind::IntegerLiteralExpression, nativesharp::SourceRange{});
    constExpression->setIntegerValue(17);
    source->setConstExpression(constExpression);
    source->setPackExpansion();
    AstNode* arrayCount = sourceArena.make<AstNode>(
        AstNodeKind::IntegerLiteralExpression, nativesharp::SourceRange{});
    arrayCount->setIntegerValue(4);
    TypeModifierSyntax modifier;
    modifier.kind = TypeModifierSyntaxKind::Array;
    modifier.arrayCount = arrayCount;
    modifier.arrayRank = 2;
    modifier.pointerNullability = PointerNullabilitySyntaxKind::NonNull;
    modifier.pointerHasProvenance = false;
    modifier.pointerKnownAlignment = 32;
    modifier.pointerAddressSpace = 7;
    source->addModifier(modifier);
    source->setOriginProjectOrdinal(11);
    source->setOriginNamespace("Origin.Space");

    const TypeSyntax* clone = parser_clone_test_support::cloneParsedType(destinationArena, source);
    context.expect(clone != nullptr && clone != source,
        "TP-PAR-514 cloneParsedType must allocate a distinct root in the destination arena");
    if (clone == nullptr) return;
    context.expect(clone->range().begin() == source->range().begin() &&
            clone->range().end() == source->range().end() &&
            clone->baseTokenKind() == source->baseTokenKind() &&
            clone->baseName() == source->baseName() &&
            clone->ownershipQualifier() == source->ownershipQualifier() &&
            clone->lifetimeName() == source->lifetimeName() &&
            clone->isDynamicTrait() == source->isDynamicTrait() &&
            clone->isOpaqueResult() == source->isOpaqueResult() &&
            clone->opaqueResultTraitName() == source->opaqueResultTraitName() &&
            clone->isExplicitTraitImplementation() == source->isExplicitTraitImplementation() &&
            clone->isPointeeConst() == source->isPointeeConst(),
        "TP-PAR-514 cloneParsedType must preserve every nominal, ownership, lifetime, and trait field");
    context.expect(clone->isTuple() && clone->tupleElementNames() == source->tupleElementNames() &&
            clone->typeArguments().size() == 1 && clone->typeArguments()[0] != tupleElement &&
            clone->typeArguments()[0]->baseName() == tupleElement->baseName(),
        "TP-PAR-514 tuple elements must preserve metadata and be recursively independent");
    context.expect(clone->isFunction() && clone->functionCarrier() == source->functionCarrier() &&
            clone->functionCallingConvention() == source->functionCallingConvention() &&
            clone->functionIsVariadic() && clone->functionIsUnsafe() && clone->functionIsNoHeap() &&
            clone->functionIsAsync() && clone->functionIsNullable() &&
            clone->functionLifetimeName() == source->functionLifetimeName() &&
            clone->functionReturnType() != source->functionReturnType() &&
            clone->functionReturnType() != nullptr &&
            clone->functionReturnType()->baseName() == "long" &&
            clone->functionErrorType() != source->functionErrorType() &&
            clone->functionErrorType() != nullptr &&
            clone->functionErrorType()->baseName() == "Failure",
        "TP-PAR-514 function-type payload and nested types must be copied exactly without aliasing");
    context.expect(clone->constExpression() != source->constExpression() &&
            clone->constExpression() != nullptr && clone->constExpression()->integerValue() == 17 &&
            clone->isPackExpansion() == source->isPackExpansion(),
        "TP-PAR-514 const-expression and pack state must survive as an independent subtree");
    context.expect(clone->modifiers().size() == 1 &&
            clone->modifiers()[0].kind == modifier.kind &&
            clone->modifiers()[0].arrayRank == modifier.arrayRank &&
            clone->modifiers()[0].pointerNullability == modifier.pointerNullability &&
            clone->modifiers()[0].pointerHasProvenance == modifier.pointerHasProvenance &&
            clone->modifiers()[0].pointerKnownAlignment == modifier.pointerKnownAlignment &&
            clone->modifiers()[0].pointerAddressSpace == modifier.pointerAddressSpace &&
            clone->modifiers()[0].arrayCount != modifier.arrayCount &&
            clone->modifiers()[0].arrayCount != nullptr &&
            clone->modifiers()[0].arrayCount->integerValue() == 4,
        "TP-PAR-514 modifiers and their expression payloads must be complete independent copies");
    context.expect(clone->originProjectOrdinal() == source->originProjectOrdinal() &&
            clone->originNamespace() == source->originNamespace(),
        "TP-PAR-514 origin project and namespace overrides must be preserved exactly");
}

void testCloneAstTopology(TestContext& context)
{
    ParserFixture fixture(
        "match (source) { Left(var value) | Right(var value) when guard => flag ? F(a,b) : G(c,d), _ => zero }");
    AstNode* match = fixture.parser().parseExpression();
    std::vector<const AstNode*> arms;
    for (const AstNode* node : flatten(match))
    {
        if (node->kind() == AstNodeKind::MatchArmExpression && node->third() != nullptr &&
            node->third()->kind() == AstNodeKind::ConditionalExpression) arms.push_back(node);
    }
    const AstNode* first = arms.size() >= 2 ? arms[0] : nullptr;
    const AstNode* second = arms.size() >= 2 ? arms[1] : nullptr;
    context.expect(arms.size() >= 2, "TP-PAR-515 OR-pattern must expand to at least two arm nodes");
    context.expect(first != nullptr && second != nullptr && first != second,
        "TP-PAR-515 expanded alternatives must be distinct nodes");
    context.expect(first != nullptr && second != nullptr && first->second() != nullptr &&
            second->second() != nullptr && first->second() != second->second(),
        "TP-PAR-515 cloned guards must not alias");
    context.expect(first != nullptr && second != nullptr && first->third() != nullptr &&
            second->third() != nullptr && first->third() != second->third(),
        "TP-PAR-515 cloned values must not alias");
    context.expect(first != nullptr && second != nullptr && first->third() != nullptr &&
            second->third() != nullptr && first->third()->first() != second->third()->first(),
        "TP-PAR-515 first/second/third descendants must not alias across clones");
}

void testCloneGenericConstraintTypesIntoDestination(TestContext& context)
{
    AstArena sourceArena;
    AstArena destinationArena;
    AstNode* source = sourceArena.make<AstNode>(
        AstNodeKind::ClassDeclaration, nativesharp::SourceRange{});
    GenericParameterSyntax parameter;
    parameter.name = "N";
    parameter.kind = GenericParameterSyntaxKind::Constant;
    parameter.constantType = sourceArena.make<TypeSyntax>(
        nativesharp::SourceRange{}, TokenKind::KwInt, "int");
    source->addGenericParameter(parameter);
    GenericConstraintClause clause;
    clause.parameterName = "T";
    clause.requiresClass = true;
    clause.requiresValueType = true;
    clause.requiresDefaultConstructor = true;
    clause.requiresCopy = true;
    clause.requiresMove = true;
    clause.requiresTrackedLifetime = true;
    clause.requiresUnmanaged = true;
    clause.requiresNotNull = true;
    clause.equalityConstraint = sourceArena.make<TypeSyntax>(
        nativesharp::SourceRange{}, TokenKind::Identifier, "EqualType");
    clause.associatedProjection = sourceArena.make<TypeSyntax>(
        nativesharp::SourceRange{}, TokenKind::Identifier, "T.Item");
    clause.substitutedArgument = sourceArena.make<TypeSyntax>(
        nativesharp::SourceRange{}, TokenKind::Identifier, "Concrete");
    clause.typeConstraints.push_back(sourceArena.make<TypeSyntax>(
        nativesharp::SourceRange{}, TokenKind::Identifier, "Base"));
    clause.typeConstraints.push_back(sourceArena.make<TypeSyntax>(
        nativesharp::SourceRange{}, TokenKind::Identifier, "Interface"));
    source->addGenericConstraint(clause);

    const AstNode* clone = parser_clone_test_support::cloneParsedNode(destinationArena, source);
    context.expect(clone != nullptr && clone != source,
        "TP-PAR-516 cloneParsedNode must allocate the declaration in the destination arena");
    if (clone == nullptr || clone->genericParameterSyntaxes().size() != 1 ||
        clone->genericConstraints().size() != 1)
    {
        context.expect(false,
            "TP-PAR-516 clone must preserve one generic parameter and one constraint clause");
        return;
    }
    const auto& clonedParameter = clone->genericParameterSyntaxes()[0];
    const auto& clonedClause = clone->genericConstraints()[0];
    context.expect(clonedParameter.name == parameter.name && clonedParameter.kind == parameter.kind &&
            clonedParameter.constantType != parameter.constantType &&
            clonedParameter.constantType != nullptr && clonedParameter.constantType->baseName() == "int",
        "TP-PAR-516 const-parameter types must be reallocated in the destination arena");
    context.expect(clonedClause.parameterName == clause.parameterName && clonedClause.requiresClass &&
            clonedClause.requiresValueType && clonedClause.requiresDefaultConstructor &&
            clonedClause.requiresCopy && clonedClause.requiresMove &&
            clonedClause.requiresTrackedLifetime && clonedClause.requiresUnmanaged &&
            clonedClause.requiresNotNull,
        "TP-PAR-516 constraint scalar payload must remain identical");
    context.expect(clonedClause.equalityConstraint != clause.equalityConstraint &&
            clonedClause.associatedProjection != clause.associatedProjection &&
            clonedClause.substitutedArgument != clause.substitutedArgument &&
            clonedClause.equalityConstraint != nullptr && clonedClause.associatedProjection != nullptr &&
            clonedClause.substitutedArgument != nullptr &&
            clonedClause.equalityConstraint->baseName() == "EqualType" &&
            clonedClause.associatedProjection->baseName() == "T.Item" &&
            clonedClause.substitutedArgument->baseName() == "Concrete",
        "TP-PAR-516 equality, projection, and substituted types must be distinct destination nodes");
    context.expect(clonedClause.typeConstraints.size() == clause.typeConstraints.size() &&
            clonedClause.typeConstraints[0] != clause.typeConstraints[0] &&
            clonedClause.typeConstraints[1] != clause.typeConstraints[1] &&
            clonedClause.typeConstraints[0]->baseName() == "Base" &&
            clonedClause.typeConstraints[1]->baseName() == "Interface",
        "TP-PAR-516 every ordered type constraint must be reallocated without aliasing");
}

void testCloneNullThroughOrPattern(TestContext& context)
{
    ParserFixture fixture("match (source) { Left(var value) | Right(var value) => value, _ => zero }");
    AstNode* match = fixture.parser().parseExpression();
    std::vector<const AstNode*> alternatives;
    for (const AstNode* node : flatten(match))
    {
        if (node->kind() == AstNodeKind::MatchArmExpression && node->third() != nullptr &&
            node->third()->text() == "value") alternatives.push_back(node);
    }
    context.expect(alternatives.size() == 2 && alternatives[0]->second() == nullptr &&
            alternatives[1]->second() == nullptr,
        "TP-PAR-517 cloning a null optional guard must preserve null on every expanded alternative");
}

void testFloatingPointPropagation(TestContext& context)
{
    ParserFixture fixture("double F() fp(strict) { return 1.0 + 2.0; }");
    AstNode* root = fixture.parse();
    const AstNode* function = firstKind(root, AstNodeKind::FunctionDeclaration);
    bool allStrict = function != nullptr && function->first() != nullptr;
    for (const AstNode* node : flatten(function != nullptr ? function->first() : nullptr))
    {
        allStrict = allStrict && node->floatingPointMode() == FloatingPointModeSyntaxKind::Strict;
    }
    context.expect(allStrict && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-518 callable FP mode must propagate to every Inherit descendant");
}

void testFloatingPointOverrideBoundary(TestContext& context)
{
    ParserFixture fixture(
        "double F() fp(strict) { fp(fast) { return 1.0 + 2.0; } return 3.0; }");
    AstNode* root = fixture.parse();
    const AstNode* function = firstKind(root, AstNodeKind::FunctionDeclaration);
    const AstNode* fastBlock = nullptr;
    for (const AstNode* node : flatten(function != nullptr ? function->first() : nullptr))
    {
        if (node->auxiliaryText() == "fp-scope:fast") fastBlock = node;
    }
    bool fastBranch = fastBlock != nullptr;
    for (const AstNode* node : flatten(fastBlock))
    {
        fastBranch = fastBranch && node->floatingPointMode() == FloatingPointModeSyntaxKind::Fast;
    }
    context.expect(fastBranch,
        "TP-PAR-519 an explicit child FP mode must form an override boundary and retain its branch mode");
}

void testSignedIntegerNonnegative(TestContext& context)
{
    ParserFixture zero("0");
    ParserFixture maximum("9223372036854775807");
    const Parser::Pattern first = zero.parser().parsePattern(false);
    const Parser::Pattern second = maximum.parser().parsePattern(false);
    context.expect(first.firstInteger == std::optional<std::int64_t>(0) &&
            second.firstInteger == std::optional<std::int64_t>(std::numeric_limits<std::int64_t>::max()),
        "TP-PAR-520 zero and INT64_MAX literals must retain exact signed values");
}

void testSignedIntegerNegative(TestContext& context)
{
    ParserFixture negative("-1");
    ParserFixture minimum("-9223372036854775808");
    const Parser::Pattern first = negative.parser().parsePattern(false);
    const Parser::Pattern second = minimum.parser().parsePattern(false);
    context.expect(first.firstInteger == std::optional<std::int64_t>(-1) &&
            second.firstInteger == std::optional<std::int64_t>(std::numeric_limits<std::int64_t>::min()),
        "TP-PAR-521 unary minus and INT64_MIN must retain exact signed values without overflow");
}

void testSignedIntegerOverflow(TestContext& context)
{
    ParserFixture positive("9223372036854775808");
    ParserFixture negative("-9223372036854775809");
    const Parser::Pattern first = positive.parser().parsePattern(false);
    const Parser::Pattern second = negative.parser().parsePattern(false);
    context.expect(!first.firstInteger.has_value() && !second.firstInteger.has_value(),
        "TP-PAR-522 magnitudes outside int64 must not publish a signed constant");
}

void testSignedIntegerNonLiteral(TestContext& context)
{
    ParserFixture fixture("value");
    const Parser::Pattern pattern = fixture.parser().parsePattern(false);
    context.expect(!pattern.firstInteger.has_value(),
        "TP-PAR-523 a non-literal pattern must not publish a signed integer constant");
}

void testOperatorSpellingAndNameMatrix(TestContext& context)
{
    struct Row final { TokenKind kind; std::size_t arity; std::string_view spelling; std::string_view name; };
    constexpr std::array<Row, 22> rows{{
        {TokenKind::Plus, 1, "+", "op_UnaryPlus"},
        {TokenKind::Minus, 1, "-", "op_UnaryNegation"},
        {TokenKind::Bang, 1, "!", "op_LogicalNot"},
        {TokenKind::Tilde, 1, "~", "op_OnesComplement"},
        {TokenKind::Plus, 2, "+", "op_Addition"},
        {TokenKind::Minus, 2, "-", "op_Subtraction"},
        {TokenKind::Star, 2, "*", "op_Multiply"},
        {TokenKind::Slash, 2, "/", "op_Division"},
        {TokenKind::Percent, 2, "%", "op_Modulus"},
        {TokenKind::Amp, 2, "&", "op_BitwiseAnd"},
        {TokenKind::Pipe, 2, "|", "op_BitwiseOr"},
        {TokenKind::Caret, 2, "^", "op_ExclusiveOr"},
        {TokenKind::LessLess, 2, "<<", "op_LeftShift"},
        {TokenKind::GreaterGreater, 2, ">>", "op_RightShift"},
        {TokenKind::EqualEqual, 2, "==", "op_Equality"},
        {TokenKind::BangEqual, 2, "!=", "op_Inequality"},
        {TokenKind::Less, 2, "<", "op_LessThan"},
        {TokenKind::LessEqual, 2, "<=", "op_LessThanOrEqual"},
        {TokenKind::Greater, 2, ">", "op_GreaterThan"},
        {TokenKind::GreaterEqual, 2, ">=", "op_GreaterThanOrEqual"},
        {TokenKind::PlusPlus, 1, "", "op_Increment"},
        {TokenKind::MinusMinus, 1, "", "op_Decrement"},
    }};
    for (const Row& row : rows)
    {
        context.expect(parser_clone_test_support::operatorSpelling(row.kind) == row.spelling &&
                parser_clone_test_support::operatorMethodName(row.kind, row.arity) == row.name,
            "TP-PAR-524 every supported token kind and arity must preserve exact spelling and metadata name");
    }
    constexpr std::array<Row, 7> invalid{{
        {TokenKind::EndOfFile, 1, "", "op_Invalid"},
        {TokenKind::Identifier, 2, "", "op_Invalid"},
        {TokenKind::Plus, 0, "+", "op_Invalid"},
        {TokenKind::Plus, 3, "+", "op_Invalid"},
        {TokenKind::AmpAmp, 2, "&&", "op_Invalid"},
        {TokenKind::PlusPlus, 2, "", "op_Invalid"},
        {TokenKind::MinusMinus, 0, "", "op_Invalid"},
    }};
    for (const Row& row : invalid)
    {
        context.expect(parser_clone_test_support::operatorSpelling(row.kind) == row.spelling &&
                parser_clone_test_support::operatorMethodName(row.kind, row.arity) == row.name,
            "TP-PAR-524 unsupported kind/arity pairs must return the exact op_Invalid fallback");
    }
}
}

void registerParserConstructionCloneFinalModule(TestRegistry& registry)
{
    addFinalParserCase(registry, "TP-PAR-513", "CP-PAR-041", testMakeHelpers);
    addFinalParserCase(registry, "TP-PAR-514", "CP-PAR-041", testCloneTypeCompleteAndIndependent,
        "PR", TestDisposition::Required, {"clone", "deep-copy"});
    addFinalParserCase(registry, "TP-PAR-515", "CP-PAR-041", testCloneAstTopology);
    addFinalParserCase(registry, "TP-PAR-516", "CP-PAR-041", testCloneGenericConstraintTypesIntoDestination);
    addFinalParserCase(registry, "TP-PAR-517", "CP-PAR-041", testCloneNullThroughOrPattern);
    addFinalParserCase(registry, "TP-PAR-518", "CP-PAR-041", testFloatingPointPropagation);
    addFinalParserCase(registry, "TP-PAR-519", "CP-PAR-041", testFloatingPointOverrideBoundary);
    addFinalParserCase(registry, "TP-PAR-520", "CP-PAR-041", testSignedIntegerNonnegative);
    addFinalParserCase(registry, "TP-PAR-521", "CP-PAR-041", testSignedIntegerNegative);
    addFinalParserCase(registry, "TP-PAR-522", "CP-PAR-041", testSignedIntegerOverflow);
    addFinalParserCase(registry, "TP-PAR-523", "CP-PAR-041", testSignedIntegerNonLiteral);
    addFinalParserCase(registry, "TP-PAR-524", "CP-PAR-041", testOperatorSpellingAndNameMatrix,
        "PR", TestDisposition::Required, {"operator", "mapping"});
}
}
