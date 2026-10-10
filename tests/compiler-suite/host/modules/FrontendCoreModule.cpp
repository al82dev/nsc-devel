#include "FrontendContractSupport.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Frontend/BuiltinIntegerTypes.h"
#include "nativesharp/Frontend/CompilationStage.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/NamespaceSymbol.h"
#include "nativesharp/Frontend/QualifiedNameLadder.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace frontend_contract_support;
using namespace nativesharp::frontend;

void integerOracle(TestContext& context, std::size_t variant)
{
    using namespace nativesharp::frontend::builtins;
    constexpr std::array<std::string_view, 8> keys = {
        "builtin:sbyte", "builtin:byte", "builtin:char", "builtin:short",
        "builtin:int", "builtin:uint", "builtin:i128", "builtin:u128",
    };
    const IntegerTypeInfo* descriptor = findIntegerType(keys[variant]);
    context.expect(descriptor != nullptr && descriptor->key == keys[variant],
        "integer descriptor lookup must preserve the exact builtin key");
    context.expect(!isIntegerTypeKey("builtin:bool") && findIntegerType("unknown") == nullptr,
        "noninteger and unknown keys must not acquire an integer descriptor");
    constexpr std::array<std::uint64_t, 8> values = {
        7, 7, 7, 2147483647ULL, 2147483648ULL,
        0x7FFFFFFFFFFFFFFFULL, 0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL,
    };
    constexpr std::array<std::string_view, 8> texts = {
        "7", "7u", "7L", "2147483647", "2147483648", "9223372036854775807",
        "9223372036854775808", "18446744073709551615",
    };
    const std::string_view selected = integerLiteralTypeKey(texts[variant], values[variant], variant == 6);
    context.expect(!selected.empty() && findIntegerType(selected) != nullptr,
        "every accepted integer literal must select one canonical builtin integer key");
    const IntegerLiteralSuffix suffix = integerLiteralSuffix(texts[variant]);
    context.expect(suffix.isUnsigned == (texts[variant].find_first_of("uU") != std::string_view::npos),
        "literal suffix parser must preserve unsigned spelling");
}

const nativesharp::ast::AstNode* findFirstEnum(const nativesharp::ast::AstNode* root)
{
    if (root == nullptr) return nullptr;
    std::vector<const nativesharp::ast::AstNode*> pending{root};
    while (!pending.empty())
    {
        const nativesharp::ast::AstNode* node = pending.back();
        pending.pop_back();
        if (node->kind() == nativesharp::ast::AstNodeKind::EnumDeclaration) return node;
        for (const nativesharp::ast::AstNode* child : node->children())
        {
            if (child != nullptr) pending.push_back(child);
        }
    }
    return nullptr;
}

void enumOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> spellings = {
        "", "byte", "sbyte", "short", "ushort", "int", "uint", "long",
    };
    std::string source = "enum E";
    if (!spellings[variant].empty()) source += " : " + std::string(spellings[variant]);
    source += " { A = 0 } int Main() { return 0; }";
    FrontendCompilation compilation;
    compilation.compileBuffer("frontend-enum.ns", source, false);
    const nativesharp::ast::AstNode* syntax = findFirstEnum(compilation.syntaxTree());
    context.expect(syntax != nullptr, "enum fixture must retain its enum syntax node");
    if (syntax == nullptr) return;
    const std::string_view effective = enumUnderlyingTypeKey(syntax);
    const std::string expected = spellings[variant].empty() ?
        "builtin:int" : "builtin:" + std::string(spellings[variant]);
    context.expect(effective == expected,
        "enum underlying helper must return the written integer key or int default");
    if (spellings[variant].empty())
    {
        context.expect(declaredEnumUnderlyingTypeKey(syntax).empty(),
            "enum without a written underlying type must preserve declaration absence");
    }
}

void stagesOracle(TestContext& context, std::size_t variant)
{
    const std::span<const CompilationStageContract> contracts = compilationStageContracts();
    context.expect(contracts.size() == 7, "frontend must publish all seven ordered stage contracts");
    const CompilationStageContract& selected = contracts[variant % contracts.size()];
    context.expect(compilationStageContract(selected.stage).name == selected.name,
        "stage lookup must return the exact canonical contract");
    context.expect(containsStageInformation(informationAvailableAfter(selected.stage), selected.produces),
        "information available after a stage must contain everything it produces");
    CompilationStageTracker tracker;
    std::string error;
    context.expect(tracker.enter(CompilationStage::Parse, error), "Parse must be a legal first stage");
    if (variant >= 4)
    {
        context.expect(!tracker.enter(CompilationStage::Codegen, error),
            "tracker must reject a stage whose required information is unavailable");
        context.expect(!error.empty(), "illegal stage transition must retain an exact diagnostic");
    }
}

void historicalOracle(TestContext& context, std::size_t variant)
{
    expectHistoricalCompilation(context, variant, "int Main() { int x = 1; return x + 2; }",
        [](TestContext& test, const FrontendCompilation& compilation)
        {
            test.expect(compilation.syntaxTree() != nullptr, "historical pipeline must retain syntax");
            test.expect(compilation.semanticAnalyzer() != nullptr, "historical pipeline must retain semantics");
            test.expect(compilation.hirRoot() != nullptr && compilation.mirModule() != nullptr,
                "historical pipeline must retain HIR and MIR products");
            test.expect(compilation.diagnostics().errorCount() == 0,
                "successful historical pipeline must retain no error diagnostics");
        });
}

void syntaxUnitOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 5, variant, ProjectStage::Syntax,
        "namespace Suite { class C { public static int F() { return 1; } } }",
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            test.expect(!compilation.units().empty(), "syntax stage must publish at least one project unit");
            const auto& unit = compilation.units().front();
            test.expect(unit != nullptr, "project syntax unit must be owned by the compilation");
            test.expect(compilation.diagnostics().errorCount() == 0,
                "valid syntax unit must retain no error diagnostics");
        });
}

void namespaceOracle(TestContext& context, std::size_t variant)
{
    GlobalNamespace namespaces;
    if (variant == 2)
    {
        const std::array<std::string, 0> emptySegments = {};
        NamespaceSymbol& root = namespaces.ensureNamespace(emptySegments);
        context.expect(&root == &namespaces.root() && root.children().empty(),
            "empty namespace input must construct a model without namespace symbols");

        const QualifiedNameSite emptySite{};
        context.expect(qualifiedNameLadder(emptySite, "").empty(),
            "empty qualified name input must publish no fallback candidates");
        return;
    }

    const std::array<std::string, 3> segments = {"A", "B", "C"};
    NamespaceSymbol& leaf = namespaces.ensureNamespace(segments);
    leaf.addTypeOrdinal(variant);
    leaf.addFunctionOrdinal(variant + 1);
    context.expect(leaf.fullName() == "A.B.C" && leaf.parent() != nullptr,
        "namespace tree must preserve full name and parent identity");
    context.expect(namespaces.findNamespace(segments) == &leaf,
        "namespace lookup must round-trip the ensured segment sequence");
    context.expect(leaf.typeOrdinals().front() == variant && leaf.functionOrdinals().front() == variant + 1,
        "namespace symbols must preserve type and function insertion order");

    const std::array<std::string, 2> lexical = {"A", "B"};
    const std::array<std::string, 1> enclosing = {"A.B.Container"};
    const std::array<std::string, 1> imports = {"Imported"};
    std::unordered_map<std::string, std::string> aliases{{"Alias", "A.B.C"}};
    QualifiedNameSite site{lexical, enclosing, &aliases, imports};
    const std::vector<QualifiedNameRung> ladder = qualifiedNameLadder(site, "Thing");
    if (ladder.empty())
    {
        context.expect(false, "qualified name ladder must publish an ordered fallback sequence");
        return;
    }
    context.expect(ladder.front().tier == QualifiedNameTier::EnclosingType,
        "enclosing type must be the highest-priority name lookup tier");
}
}

void registerFrontendCoreModule(TestRegistry& registry)
{
    addMatrix(registry, 1, "unit", integerOracle);
    addMatrix(registry, 2, "unit", enumOracle);
    addMatrix(registry, 3, "unit", stagesOracle);
    addMatrix(registry, 4, "component", historicalOracle);
    addMatrix(registry, 5, "component", syntaxUnitOracle);
    addMatrix(registry, 6, "unit", namespaceOracle);
}
}
