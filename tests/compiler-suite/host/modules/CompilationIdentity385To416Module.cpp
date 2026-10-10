#include "CompilationTestSupport.h"

#include <array>
#include <cctype>
#include <set>

namespace nsc::suite
{
namespace
{
using namespace compilation_support;

void typeIdentityContract(TestContext& context, std::size_t variant)
{
    TypeIdentity identity = variant == 3 ? TypeIdentity("", {}, {}, "Player") :
        variant == 4 ? TypeIdentity("Game", {"bad.namespace"}, {}, "Player") :
        variant == 5 ? TypeIdentity("Game", {}, {}, "9Player") :
        variant == 6 ? TypeIdentity("Game", {"Ns"}, {{"Outer", 1U}}, "Item", 2U) :
        sampleIdentity();
    const bool expected = variant != 3 && variant != 4 && variant != 5;
    bool exact = identity.isValid() == expected;
    if (identity.isValid())
    {
        const TypeIdentity copy(
            identity.moduleName(), identity.namespaceSegments(), identity.containingTypes(),
            identity.name(), identity.arity());
        exact = exact && identity == copy && identity.key() == identity.moduleName() + "::" +
            identity.fullyQualifiedName();
    }
    else
    {
        exact = exact && !identity.validationError().empty();
    }
    context.expect(exact, "CF-COMPILATION-049 identity validates every segment and preserves value equality/accessors");
}

void qualifiedNameContract(TestContext& context, std::size_t variant)
{
    const TypeIdentity identity = variant == 1 ? TypeIdentity("M", {}, {}, "T") :
        variant == 2 ? TypeIdentity("M", {"N"}, {{"Outer", 1U}}, "Item", 2U) :
        variant == 3 ? TypeIdentity("M", {}, {}, "T", 0U) :
        variant == 4 ? TypeIdentity("M", {"N", "Outer"}, {}, "Item", 2U) :
        variant == 5 ? TypeIdentity("M", {}, {{"Outer", 0U}}, "Item", 0U) :
        variant == 6 ? TypeIdentity("Other", {"N"}, {{"Outer", 1U}}, "Item", 2U) :
        TypeIdentity("M", {"N"}, {{"Outer", 1U}}, "Item", 2U);
    const std::string expectedName = variant == 1 || variant == 3 ? "T" :
        variant == 4 ? "N.Outer.Item`2" : variant == 5 ? "Outer+Item" : "N.Outer`1+Item`2";
    context.expect(identity.fullyQualifiedName() == expectedName &&
            identity.key() == identity.moduleName() + "::" + expectedName,
        "CF-COMPILATION-050 namespace, nesting, generic arity, and module format without collisions");
}

void symbolFragmentContract(TestContext& context, std::size_t variant)
{
    const std::array<std::string, 8> inputs = {
        "A", "M::N.T", "", "A+B", "A_B", "M::Outer`2+Item", "A.B:C", std::string(256U, ':')};
    const std::string encoded = symbolFragmentForIdentityKey(inputs[variant - 1U]);
    bool exact = true;
    for (const char character : encoded)
    {
        exact = exact && (std::isalnum(static_cast<unsigned char>(character)) != 0 ||
            character == '_' || character == '$');
    }
    if (variant == 1) exact = exact && encoded == "A";
    if (variant == 2) exact = exact && encoded == "M$3A$3AN$2ET";
    if (variant == 3) exact = exact && encoded.empty();
    if (variant == 4) exact = exact && encoded == "A$2BB";
    if (variant == 5) exact = exact && encoded == "A_B";
    if (variant == 6) exact = exact && contains(encoded, "$60") && contains(encoded, "$2B");
    std::set<std::string> distinct;
    for (const std::string& value : std::array<std::string, 6>{"A.B", "A_B", "A+B", "A:B", "A$B", "A-B"})
    {
        distinct.insert(symbolFragmentForIdentityKey(value));
    }
    context.expect(exact && distinct.size() == 6U,
        "CF-COMPILATION-051 byte escaping is locale-independent, reversible in shape, and collision-free");
}

void irSymbolContract(TestContext& context, std::size_t variant)
{
    const std::array<std::string, 8> symbols = {
        "name", "__nsc_method_M$3A$3AN$2ET", "", "0leading",
        "with space", "alpha-beta.gamma_1", "$", std::string(512U, 'a') + "$"};
    const std::string& symbol = symbols[variant - 1U];
    const bool expectedQuotes = variant == 2 || variant == 3 || variant == 4 ||
        variant == 5 || variant == 7 || variant == 8;
    const std::string reference = irTextSymbolReference(symbol);
    const bool exact = symbolNeedsIrTextQuotes(symbol) == expectedQuotes && reference.starts_with('@') &&
        (expectedQuotes ? reference.size() >= 3U && reference[1] == '"' && reference.back() == '"' :
            reference == "@" + symbol) &&
        irTextIdentitySymbolPrefix("__nsc_method_") == "@\"__nsc_method_";
    context.expect(exact,
        "CF-COMPILATION-052 LLVM text quoting, complete references, and identity prefixes follow one rule");
}
}

void registerCompilationIdentity385To416Module(TestRegistry& registry)
{
    addCompilationContract(registry, 49U, "unit", typeIdentityContract);
    addCompilationContract(registry, 50U, "unit", qualifiedNameContract);
    addCompilationContract(registry, 51U, "unit", symbolFragmentContract);
    addCompilationContract(registry, 52U, "unit", irSymbolContract);
}
}
