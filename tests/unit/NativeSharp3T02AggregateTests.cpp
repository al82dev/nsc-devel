#include "nativesharp/AST/AstDumper.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Language/AggregateSemantics.h"
#include "nativesharp/Language/LanguageEdition.h"
#include "nativesharp/Lexer/Lexer.h"
#include "nativesharp/Parser/Parser.h"
#include "nativesharp/Source/SourceManager.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct Parsed final
{
    nativesharp::source::SourceManager sources;
    nativesharp::diagnostics::DiagnosticEngine diagnostics;
    std::vector<nativesharp::lexer::Token> tokens;
    nativesharp::ast::AstArena arena;
    nativesharp::ast::AstNode* root = nullptr;
};

void parse(Parsed& parsed, std::string source, nativesharp::language::LanguageEdition edition)
{
    const nativesharp::FileId id = parsed.sources.addBuffer("t02.ns", std::move(source));
    nativesharp::lexer::Lexer lexer(*parsed.sources.getFile(id), parsed.diagnostics);
    parsed.tokens = lexer.lexAll();
    nativesharp::parser::Parser parser(
        parsed.tokens, parsed.arena, parsed.diagnostics, {}, edition);
    parsed.root = parser.parseCompilationUnit();
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp;

    const std::array<language::TupleElement, 2> named = {{{"builtin:int", "x"}, {"builtin:long", "y"}}};
    const std::array<language::TupleElement, 2> renamed = {{{"builtin:int", "left"}, {"builtin:long", "right"}}};
    const std::array<language::TupleElement, 1> shorter = {{{"builtin:int", "x"}}};
    tests.expect(!language::tupleNamesAffectIdentity() &&
            language::tupleIdentity(named) == language::tupleIdentity(renamed) &&
            language::tupleIdentity(named) != language::tupleIdentity(shorter),
        "tuple identity depends on arity and element types, never element names");

    const std::array<std::string, 2> numericSource = {"builtin:int", "builtin:float"};
    const std::array<std::string, 2> numericTarget = {"builtin:long", "builtin:double"};
    const std::array<std::string, 1> wrongArity = {"builtin:long"};
    tests.expect(language::tupleConversionExists(numericSource, numericTarget) &&
            !language::tupleConversionExists(numericSource, wrongArity),
        "tuple conversions are element-wise and preserve arity");

    const std::array<language::AggregateFieldLayout, 3> fields = {{{1, 1}, {8, 8}, {4, 4}}};
    const auto layout = language::computeWindowsX64AggregateLayout(fields);
    tests.expect(layout.has_value() && layout->offsets == std::vector<std::size_t>({0, 8, 16}) &&
            layout->size == 24 && layout->alignment == 8,
        "aggregate layout follows Windows x64 alignment and tail-padding rules");
    const std::array<language::AggregateFieldLayout, 1> badField = {{{4, 3}}};
    tests.expect(!language::computeWindowsX64AggregateLayout(badField).has_value(),
        "aggregate layout rejects non-power-of-two alignment");
    tests.expect(language::classifyWindowsX64AggregateReturn(8) ==
            language::WindowsX64AggregateReturn::DirectInteger &&
            language::classifyWindowsX64AggregateReturn(12) ==
            language::WindowsX64AggregateReturn::IndirectSRet,
        "aggregate return classification distinguishes direct values from hidden sret");

    std::string error;
    const std::array<language::DeconstructionBinding, 2> duplicate = {{
        {"value", language::DeconstructionBindingMode::Value},
        {"value", language::DeconstructionBindingMode::Move}}};
    const std::array<language::DeconstructionBinding, 2> discards = {{
        {"_", language::DeconstructionBindingMode::Discard},
        {"_", language::DeconstructionBindingMode::Discard}}};
    tests.expect(!language::validateDeconstructionBindings(duplicate, error) &&
            language::validateDeconstructionBindings(discards, error),
        "deconstruction rejects duplicate bindings while permitting repeated discards");

    const language::RecordDeriveSet noDerives;
    const auto equality = noDerives.with(language::RecordDerive::Equality);
    tests.expect(!noDerives.contains(language::RecordDerive::Equality) &&
            equality.contains(language::RecordDerive::Equality) &&
            !equality.contains(language::RecordDerive::Clone),
        "records synthesize behavior only through explicit derives");

    const std::array<language::PromotedField, 2> promoted = {{{"x", "builtin:int"}, {"x", "builtin:int"}}};
    tests.expect(!language::validatePromotedFields(promoted, error),
        "promoted record fields reject duplicate names");
    const std::array<std::string, 2> duplicateFields = {"X", "X"};
    tests.expect(!language::validateAggregateSpread(2, {}, error) &&
            !language::validateAggregateSpread(1, duplicateFields, error),
        "aggregate spread accepts one source and unique explicit replacements");

    constexpr std::string_view source = R"ns(
record struct Pair { public int X; public int Y; }
int Demo(int a, int b)
{
    (x: a, y: b);
    (a, b) = (b, a);
    (a, ..(b, a));
    return 0;
}
)ns";
    Parsed edition3;
    parse(edition3, std::string(source), language::LanguageEdition::V3);
    tests.expect(edition3.root != nullptr && edition3.diagnostics.errorCount() == 0,
        "tuple, deconstruction, spread, and record syntax parse in edition 3");
    std::ostringstream dump;
    ast::AstDumper{}.dump(edition3.root, dump);
    const std::string tree = dump.str();
    tests.expect(tree.find("TupleExpression") != std::string::npos &&
            tree.find("SpreadElementExpression") != std::string::npos,
        "tuple and spread syntax survive in explicit AST nodes");
    tests.expect(tree.find("StructDeclaration \"Pair\"") != std::string::npos &&
            tree.find("aux=\"record\"") != std::string::npos,
        "record struct remains a value aggregate with an explicit semantic marker");

    Parsed edition2;
    parse(edition2, std::string(source), language::LanguageEdition::V2);
    const std::size_t editionErrors = std::ranges::count_if(
        edition2.diagnostics.diagnostics(), [](const diagnostics::Diagnostic& diagnostic)
        {
            return diagnostic.id() == diagnostics::DiagnosticId::NS3002FeatureRequiresLanguageEdition3;
        });
    tests.expect(editionErrors >= 4,
        "edition 2 rejects T02 surface syntax with stable NS3002 diagnostics");

    return tests.result();
}
