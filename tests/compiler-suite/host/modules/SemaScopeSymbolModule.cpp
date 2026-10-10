#include "SemaTestSupport.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Sema/Scope.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/TypeContext.h"

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::sema;
using sema_support::addSemaCase;

Symbol makeSymbol(std::string name, Type* type = nullptr)
{
    return Symbol(SymbolKind::Variable, std::move(name), type, nullptr);
}

void c036(TestContext& c)
{
    const Scope scope(nullptr);
    c.expect(scope.parent() == nullptr && scope.lookupLocal("x") == nullptr && scope.lookup("x") == nullptr,
        "TS-SEMA-036 an empty root scope has no parent and no lookup hits");
}

void c037(TestContext& c)
{
    Scope scope(nullptr);
    Symbol symbol = makeSymbol("x");
    c.expect(scope.declare(&symbol) && scope.lookupLocal("x") == &symbol && scope.lookup("x") == &symbol,
        "TS-SEMA-037 a unique declaration is visible locally and hierarchically");
}

void c038(TestContext& c)
{
    Scope scope(nullptr);
    Symbol first = makeSymbol("x");
    Symbol second = makeSymbol("x");
    c.expect(scope.declare(&first) && !scope.declare(&second) && scope.lookup("x") == &first,
        "TS-SEMA-038 a duplicate declaration fails and retains the first symbol");
}

void c039(TestContext& c)
{
    Scope parent(nullptr);
    Scope child(&parent);
    Symbol symbol = makeSymbol("x");
    parent.declare(&symbol);
    c.expect(child.lookup("x") == &symbol,
        "TS-SEMA-039 child lookup finds a symbol declared in its parent");
}

void c040(TestContext& c)
{
    Scope parent(nullptr);
    Scope child(&parent);
    Symbol outer = makeSymbol("x");
    Symbol inner = makeSymbol("x");
    parent.declare(&outer);
    child.declare(&inner);
    c.expect(child.lookup("x") == &inner && parent.lookup("x") == &outer,
        "TS-SEMA-040 local shadowing wins without mutating the parent");
}

void c041(TestContext& c)
{
    Scope parent(nullptr);
    Scope child(&parent);
    Symbol symbol = makeSymbol("x");
    parent.declare(&symbol);
    c.expect(child.lookupLocal("x") == nullptr && child.lookup("x") == &symbol,
        "TS-SEMA-041 lookupLocal never climbs to a parent");
}

void c042(TestContext& c)
{
    Scope scope(nullptr);
    Symbol empty = makeSymbol("");
    Symbol utf8 = makeSymbol("caf\xC3\xA9");
    Symbol upper = makeSymbol("Name");
    Symbol lower = makeSymbol("name");
    const bool declared = scope.declare(&empty) && scope.declare(&utf8) &&
        scope.declare(&upper) && scope.declare(&lower);
    c.expect(declared && scope.lookup("") == &empty && scope.lookup("caf\xC3\xA9") == &utf8 &&
            scope.lookup("Name") == &upper && scope.lookup("name") == &lower,
        "TS-SEMA-042 scope names compare exact bytes including empty, UTF-8, and case variants");
}

void c043(TestContext& c)
{
    constexpr std::size_t depth = 1024;
    std::vector<std::unique_ptr<Scope>> scopes;
    scopes.reserve(depth);
    scopes.push_back(std::make_unique<Scope>(nullptr));
    Symbol symbol = makeSymbol("root");
    scopes.front()->declare(&symbol);
    for (std::size_t index = 1; index < depth; ++index)
        scopes.push_back(std::make_unique<Scope>(scopes.back().get()));
    c.expect(scopes.back()->lookup("root") == &symbol && scopes.back()->lookup("missing") == nullptr,
        "TS-SEMA-043 deep iterative scope lookup preserves hit and miss behavior");
}

void c044(TestContext& c)
{
    const std::array<std::pair<SymbolKind, std::string_view>, 8> rows = {{
        {SymbolKind::Variable, "Variable"}, {SymbolKind::GlobalVariable, "GlobalVariable"},
        {SymbolKind::Parameter, "Parameter"}, {SymbolKind::Function, "Function"},
        {SymbolKind::Struct, "Struct"}, {SymbolKind::Enum, "Enum"},
        {SymbolKind::Field, "Field"}, {SymbolKind::EnumMember, "EnumMember"}}};
    for (const auto& [kind, name] : rows)
        c.expect(symbolKindName(kind) == name, "TS-SEMA-044 all eight symbol kind names are exact");
}

void c045(TestContext& c)
{
    c.expect(symbolKindName(static_cast<SymbolKind>(99)) == "Symbol",
        "TS-SEMA-045 an external SymbolKind uses the Symbol fallback");
}

void c046(TestContext& c)
{
    TypeContext types;
    nativesharp::ast::AstNode declaration(nativesharp::ast::AstNodeKind::CompilationUnit, {});
    const std::array<SymbolKind, 8> kinds = {SymbolKind::Variable, SymbolKind::GlobalVariable,
        SymbolKind::Parameter, SymbolKind::Function, SymbolKind::Struct, SymbolKind::Enum,
        SymbolKind::Field, SymbolKind::EnumMember};
    for (const SymbolKind kind : kinds)
    {
        Symbol symbol(kind, "name", types.builtin(TypeKind::Int), &declaration);
        c.expect(symbol.kind() == kind && symbol.name() == "name" &&
                symbol.type() == types.builtin(TypeKind::Int) && symbol.declaration() == &declaration,
            "TS-SEMA-046 every SymbolKind construction preserves identity, name, type, and declaration");
    }
}

void c047(TestContext& c)
{
    Symbol symbol = makeSymbol("x");
    const bool defaults = !symbol.isConst() && !symbol.isDefined() && !symbol.isExternC();
    symbol.setConst(true); symbol.setDefined(true); symbol.setExternC(true);
    const bool enabled = symbol.isConst() && symbol.isDefined() && symbol.isExternC();
    symbol.setConst(false); symbol.setDefined(false); symbol.setExternC(false);
    c.expect(defaults && enabled && !symbol.isConst() && !symbol.isDefined() && !symbol.isExternC(),
        "TS-SEMA-047 symbol flags transition independently false-true-false");
}

void c048(TestContext& c)
{
    Symbol symbol = makeSymbol("e");
    bool exact = true;
    for (const std::int64_t value : {std::numeric_limits<std::int64_t>::min(), std::int64_t{0},
             std::numeric_limits<std::int64_t>::max()})
    {
        symbol.setEnumValue(value);
        exact = exact && symbol.enumValue() == value;
    }
    c.expect(exact, "TS-SEMA-048 enum value setter/getter preserves min, zero, and max");
}

void c049(TestContext& c)
{
    const Symbol symbol = makeSymbol("field");
    c.expect(!symbol.hasFieldOffset() && symbol.fieldOffset() == 0,
        "TS-SEMA-049 absent field offset is distinguishable while its getter returns zero");
}

void c050(TestContext& c)
{
    Symbol zero = makeSymbol("zero");
    Symbol maximum = makeSymbol("maximum");
    zero.setFieldOffset(0);
    maximum.setFieldOffset(std::numeric_limits<std::uint64_t>::max());
    c.expect(zero.hasFieldOffset() && zero.fieldOffset() == 0 && maximum.hasFieldOffset() &&
            maximum.fieldOffset() == std::numeric_limits<std::uint64_t>::max(),
        "TS-SEMA-050 present zero and maximum field offsets remain distinct and exact");
}

void c051(TestContext& c)
{
    Symbol symbol = makeSymbol("constant");
    const bool absent = !symbol.hasConstantValue();
    symbol.setConstantValue(ConstantValue::signedInteger(-7));
    c.expect(absent && symbol.hasConstantValue() &&
            symbol.constantValue().kind() == ConstantKind::SignedInteger &&
            symbol.constantValue().signedInteger() == -7,
        "TS-SEMA-051 constant-value presence and payload are exact");
}

void c052(TestContext& c)
{
    Symbol owner = makeSymbol("owner");
    Symbol a = makeSymbol("A");
    Symbol b = makeSymbol("B");
    owner.addMember(&a); owner.addMember(&b); owner.addMember(&a);
    c.expect(owner.members().size() == 3 && owner.members()[0] == &a &&
            owner.members()[1] == &b && owner.members()[2] == &a,
        "TS-SEMA-052 members preserve insertion order and duplicates");
}

void c053(TestContext& c)
{
    Symbol owner = makeSymbol("owner");
    Symbol first = makeSymbol("x");
    Symbol second = makeSymbol("x");
    owner.addMember(&first); owner.addMember(&second);
    c.expect(owner.findMember("x") == &first && owner.findMember("missing") == nullptr,
        "TS-SEMA-053 member lookup returns the first match or null");
}
}

void registerSemaScopeSymbolModule(TestRegistry& registry)
{
    const std::array<TestFunction, 18> tests = {c036, c037, c038, c039, c040, c041, c042, c043,
        c044, c045, c046, c047, c048, c049, c050, c051, c052, c053};
    for (std::size_t index = 0; index < tests.size(); ++index)
    {
        const std::size_t ordinal = index + 36;
        const std::string id = "TS-SEMA-0" + std::to_string(ordinal);
        addSemaCase(registry, id, ordinal <= 43 ? "CS-SEMA-006" : "CS-SEMA-007", "unit",
            tests[index], ordinal == 43 ? "NIGHTLY" : "PR");
    }
}
}
