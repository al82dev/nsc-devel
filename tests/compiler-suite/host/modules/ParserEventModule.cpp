#include "ParserFoundationTestSupport.h"

#include "nativesharp/AST/AstNodeKind.h"

#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;

// Returns a class member from a source whose first declaration is a delegate. Complexity is O(1).
AstNode* eventMember(ParserFixture& fixture, std::size_t ordinal = 0)
{
    AstNode* root = fixture.parse();
    AstNode* type = root != nullptr && root->children().size() > 1 ? root->children()[1] : nullptr;
    return type != nullptr && type->children().size() > ordinal ? type->children()[ordinal] : nullptr;
}

// Registers one event parser case. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CP-PAR-022", "Host.Parser", "component", "PR",
        {"parser", "events"}, std::move(function)});
}

// Verifies a minimal event retains declaration kind, delegate type, and name. Complexity is O(n).
void testMinimalEvent(TestContext& context)
{
    ParserFixture fixture("delegate void Changed(); class C { event Changed Signal; }");
    AstNode* event = eventMember(fixture);
    context.expect(event != nullptr && event->kind() == AstNodeKind::EventDeclaration &&
            event->text() == "Signal" && event->typeSyntax() != nullptr &&
            event->typeSyntax()->baseName() == "Changed",
        "TP-PAR-223 minimal event must preserve kind, delegate type, and name");
}

// Verifies event accessibility, static, and new markers propagate exactly. Complexity is O(n).
void testEventModifiers(TestContext& context)
{
    ParserFixture fixture("delegate void D(); class C { public static new event D A; private event D B; }");
    AstNode* root = fixture.parse();
    AstNode* type = root != nullptr && root->children().size() > 1 ? root->children()[1] : nullptr;
    AstNode* first = type != nullptr && !type->children().empty() ? type->children()[0] : nullptr;
    AstNode* second = type != nullptr && type->children().size() > 1 ? type->children()[1] : nullptr;
    context.expect(first != nullptr && first->hasModifier(TokenKind::KwPublic) &&
            first->hasModifier(TokenKind::KwStatic) && first->hasModifier(TokenKind::KwNew),
        "TP-PAR-224 public static new event must preserve every modifier");
    context.expect(second != nullptr && second->hasModifier(TokenKind::KwPrivate) &&
            !second->hasModifier(TokenKind::KwStatic),
        "TP-PAR-224 private instance event must remain independent");
}

// Verifies the current event contract synthesizes one bounded backing child, not accessors. Complexity is O(n).
void testEventStorageContract(TestContext& context)
{
    ParserFixture fixture("delegate void D(); class C { event D Changed; }");
    AstNode* event = eventMember(fixture);
    AstNode* backing = event != nullptr && event->children().size() == 1 ? event->children()[0] : nullptr;
    context.expect(backing != nullptr && backing->kind() == AstNodeKind::FieldDeclaration &&
            backing->text() == "__nsc_event_Changed$stage42" &&
            backing->auxiliaryText() == "event-backing:Changed",
        "TP-PAR-225 event grammar must expose its one exact backing-storage child");
    context.expect(event != nullptr && event->range().begin().offset() < event->range().end().offset(),
        "TP-PAR-225 event declaration must retain a non-empty complete range");
    ParserFixture accessors("delegate void D(); class C { event D Changed { add; remove; } }");
    accessors.parse();
    context.expect(diagnosticCount(accessors.diagnostics(), DiagnosticId::ExpectedToken) >= 1,
        "TP-PAR-225 event accessors are outside the current semicolon-only event grammar");
}

// Verifies missing event type or name diagnoses and terminates with progress. Complexity is O(n).
void testMalformedEvents(TestContext& context)
{
    ParserFixture missingType("class C { event ; int kept; }");
    ParserFixture missingName("delegate void D(); class C { event D ; int kept; }");
    missingType.parse();
    missingName.parse();
    context.expect(diagnosticCount(missingType.diagnostics(), DiagnosticId::ExpectedType) >= 1,
        "TP-PAR-226 missing event type must report ExpectedType");
    context.expect(diagnosticCount(missingName.diagnostics(), DiagnosticId::ExpectedToken) >= 1,
        "TP-PAR-226 missing event name must report ExpectedToken");
}

// Verifies modifiers outside the event subset remain explicit diagnostics. Complexity is O(n).
void testForbiddenEventModifier(TestContext& context)
{
    ParserFixture fixture("delegate void D(); class C { virtual event D Changed; }");
    fixture.parse();
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedDeclaration) >= 1,
        "TP-PAR-227 virtual event must remain rejected by the current event modifier contract");
}

// Verifies consecutive events remain separate and ordered. Complexity is O(n).
void testConsecutiveEvents(TestContext& context)
{
    ParserFixture fixture("delegate void D(); class C { event D First; event D Second; }");
    AstNode* root = fixture.parse();
    AstNode* type = root != nullptr && root->children().size() > 1 ? root->children()[1] : nullptr;
    AstNode* first = type != nullptr && !type->children().empty() ? type->children()[0] : nullptr;
    AstNode* second = type != nullptr && type->children().size() > 1 ? type->children()[1] : nullptr;
    context.expect(first != nullptr && second != nullptr && first != second &&
            first->text() == "First" && second->text() == "Second" &&
            first->range().end().offset() <= second->range().begin().offset(),
        "TP-PAR-228 consecutive events must remain independent and source ordered");
}
}

// Registers TP-PAR-223..228 for event declarations and recovery.
void registerParserEventModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-223", testMinimalEvent);
    addCase(registry, "TP-PAR-224", testEventModifiers);
    addCase(registry, "TP-PAR-225", testEventStorageContract);
    addCase(registry, "TP-PAR-226", testMalformedEvents);
    addCase(registry, "TP-PAR-227", testForbiddenEventModifier);
    addCase(registry, "TP-PAR-228", testConsecutiveEvents);
}
}
