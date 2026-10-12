#include "SemaTestSupport.h"

#include "nativesharp/Sema/Symbol.h"

#include <array>
#include <string_view>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::sema;
using sema_support::AnalyzerFixture;
using sema_support::addSemaCase;
using sema_support::diagnosticCount;
using nativesharp::diagnostics::DiagnosticId;

Symbol* findGlobal(SemanticAnalyzer& analyzer, std::string_view name)
{
    for (Symbol* symbol : analyzer.globalSymbols())
        if (symbol->name() == name) return symbol;
    return nullptr;
}

void c102(TestContext& c)
{
    AnalyzerFixture fixture("struct Empty { }");
    const bool analyzed = fixture.analyze();
    Symbol* symbol = findGlobal(fixture.analyzer(), "Empty");
    c.expect(analyzed && symbol != nullptr && symbol->kind() == SymbolKind::Struct &&
            symbol->type()->isComplete() && symbol->members().empty(),
        "TS-SEMA-102 an empty struct produces a complete nominal type and empty layout");
}

void c103(TestContext& c)
{
    AnalyzerFixture fixture("struct Mixed { byte A; int B; short Tail; }");
    const bool analyzed = fixture.analyze();
    Symbol* symbol = findGlobal(fixture.analyzer(), "Mixed");
    const auto& members = symbol != nullptr ? symbol->members() : std::vector<Symbol*>{};
    c.expect(analyzed && members.size() == 3 && members[0]->fieldOffset() == 0 &&
            members[1]->fieldOffset() == 4 && members[2]->fieldOffset() == 8 &&
            members[0]->name() == "A" && members[1]->name() == "B" && members[2]->name() == "Tail",
        "TS-SEMA-103 scalar fields preserve source order, target padding, alignment, and offsets");
}

void c104(TestContext& c)
{
    AnalyzerFixture fixture(
        "struct Inner { byte A; int B; } struct Outer { byte Prefix; Inner Value; short Tail; }");
    const bool analyzed = fixture.analyze();
    Symbol* inner = findGlobal(fixture.analyzer(), "Inner");
    Symbol* outer = findGlobal(fixture.analyzer(), "Outer");
    c.expect(analyzed && inner != nullptr && outer != nullptr && inner->type()->isComplete() &&
            outer->type()->isComplete() && outer->members().size() == 3 &&
            outer->members()[0]->fieldOffset() == 0 && outer->members()[1]->fieldOffset() == 4 &&
            outer->members()[2]->fieldOffset() == 12,
        "TS-SEMA-104 nested complete structs use exact transitive target layout");
}

void c105(TestContext& c)
{
    AnalyzerFixture fixture("struct Duplicate { int Value; byte Value; }");
    fixture.analyze();
    Symbol* symbol = findGlobal(fixture.analyzer(), "Duplicate");
    c.expect(symbol != nullptr && symbol->members().size() == 1 &&
            symbol->members()[0]->type()->kind() == TypeKind::Int &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::DuplicateSymbol) == 1 &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::PreviousDeclaration) == 1,
        "TS-SEMA-105 a duplicate field diagnoses and retains only the first member");
}

void c106(TestContext& c)
{
    AnalyzerFixture voidField("struct InvalidVoidField { void Empty; }");
    AnalyzerFixture functionField("struct InvalidFunctionField { fn*()->int Callback; }");
    voidField.analyze();
    functionField.analyze();
    c.expect(diagnosticCount(voidField.diagnostics(), DiagnosticId::InvalidType) == 1 &&
            diagnosticCount(functionField.diagnostics(), DiagnosticId::InvalidType) == 1,
        "TS-SEMA-106 void and function fields must each report one InvalidType diagnostic");
}

void c107(TestContext& c)
{
    AnalyzerFixture fixture("struct Node { Node Next; }");
    fixture.analyze();
    c.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::IncompleteType) == 1,
        "TS-SEMA-107 a field containing its incomplete struct directly reports IncompleteType");
}

void c108(TestContext& c)
{
    AnalyzerFixture fixture("struct Unlayoutable { fn*()->int Callback; }");
    fixture.analyze();
    Symbol* symbol = findGlobal(fixture.analyzer(), "Unlayoutable");
    c.expect(symbol != nullptr && symbol->members().size() == 1 &&
            !symbol->members()[0]->hasFieldOffset(),
        "TS-SEMA-108 an unavailable field layout must not invent an offset");
}

void c109(TestContext& c)
{
    AnalyzerFixture fixture(
        "struct Overflow { byte[18446744073709551615] Payload; byte Tail; }");
    fixture.analyze();
    Symbol* symbol = findGlobal(fixture.analyzer(), "Overflow");
    const bool noInventedOffset = symbol != nullptr && symbol->members().size() == 2 &&
        !symbol->members()[0]->hasFieldOffset() && !symbol->members()[1]->hasFieldOffset();
    c.expect(noInventedOffset,
        "TS-SEMA-109 layout overflow preserves the current state without invented offsets");
}

void c110(TestContext& c)
{
    AnalyzerFixture direct("struct Direct { Direct Value; }");
    direct.analyze();
    AnalyzerFixture indirect("struct First { Second Value; } struct Second { First Value; }");
    indirect.analyze();
    c.expect(diagnosticCount(direct.diagnostics(), DiagnosticId::IncompleteType) == 1 &&
            diagnosticCount(indirect.diagnostics(), DiagnosticId::IncompleteType) >= 1,
        "TS-SEMA-110 PRODUCT-DEFECT: direct and indirect nominal recursion must both reject by incompleteness");
}

void c111(TestContext& c)
{
    AnalyzerFixture fixture("struct Broken { Broken Value; }");
    fixture.analyze();
    Symbol* symbol = findGlobal(fixture.analyzer(), "Broken");
    c.expect(symbol != nullptr,
        "TS-SEMA-111 self-recursive struct must still publish its nominal symbol");
    c.expect(symbol != nullptr && symbol->type()->isComplete(),
        "TS-SEMA-111 struct type is marked complete after semantic field traversal");
    c.expect(fixture.diagnostics().hasErrors(),
        "TS-SEMA-111 semantic layout traversal must diagnose the recursive by-value field");
}
}

void registerSemaStructLayoutModule(TestRegistry& registry)
{
    const std::array<TestFunction, 10> tests = {
        c102, c103, c104, c105, c106, c107, c108, c109, c110, c111};
    for (std::size_t index = 0; index < tests.size(); ++index)
    {
        const std::size_t ordinal = index + 102;
        const TestDisposition disposition = ordinal == 110
            ? TestDisposition::KnownProductDefect : TestDisposition::Required;
        std::vector<std::string> tags;
        if (disposition == TestDisposition::KnownProductDefect) tags = {"known-product-defect"};
        addSemaCase(registry, "TS-SEMA-" + std::to_string(ordinal), "CS-SEMA-013", "component",
            tests[index], "PR", disposition, std::move(tags));
    }
}
}
