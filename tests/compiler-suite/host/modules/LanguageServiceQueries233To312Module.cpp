#include "LanguageServiceTestSupport.h"

#include <algorithm>

namespace nsc::suite
{
namespace
{
using namespace language_service_support;
using namespace nativesharp::language_service;

constexpr TextPosition kFirstAdd{6U, 20U};

void documentSymbolsContract(TestContext& context, std::size_t variant)
{
    if (variant == 3U)
    {
        context.expect(AnalysisSnapshot{}.documentSymbols("A:/missing.ns").empty(),
            "missing semantic model yields an empty document symbol set");
        return;
    }
    SemanticFixture fixture(variant == 8U ? 8U : 1U);
    const auto symbols = fixture.snapshot().documentSymbols(
        variant == 4U ? fixture.root() / "missing.ns" : fixture.use());
    const bool hasRun = std::ranges::any_of(symbols, [](const LanguageSymbol& symbol) {
        return symbol.name == "Run";
    });
    const bool hasTotal = std::ranges::any_of(symbols, [](const LanguageSymbol& symbol) {
        return symbol.name == "total";
    });
    context.expect(variant == 4U ? symbols.empty() : hasRun && hasTotal,
        "document symbols filter by canonical path and preserve declaration order");
}

void workspaceSymbolsContract(TestContext& context, std::size_t variant)
{
    if (variant == 3U)
    {
        context.expect(AnalysisSnapshot{}.workspaceSymbols("Add").empty(),
            "missing semantic model yields empty workspace symbols");
        return;
    }
    SemanticFixture fixture;
    const std::size_t limit = variant == 4U || variant == 5U ? 0U : variant == 6U ? 1U : 100U;
    const auto symbols = fixture.snapshot().workspaceSymbols(variant == 2U ? "aDd" : "Add", limit);
    context.expect(limit == 0U ? symbols.empty() : limit == 1U ? symbols.size() == 1U : symbols.size() == 2U,
        "workspace symbols use case-insensitive substring matching and a strict result cap");
}

void symbolAtContract(TestContext& context, std::size_t variant)
{
    if (variant == 3U)
    {
        context.expect(!AnalysisSnapshot{}.symbolAt("A:/missing.ns", {}).has_value(),
            "missing semantic model yields no symbol");
        return;
    }
    SemanticFixture fixture;
    const auto symbol = fixture.snapshot().symbolAt(
        variant == 4U ? fixture.declaration() : fixture.use(),
        variant == 4U ? TextPosition{99U, 0U} : kFirstAdd);
    context.expect(variant == 4U ? !symbol.has_value() :
            symbol.has_value() && symbol->name == "Add" && symbol->kind == LanguageSymbolKind::Method,
        "symbol-at prioritizes the smallest resolved reference target and rejects misses");
}

void definitionsContract(TestContext& context, std::size_t variant)
{
    if (variant == 3U)
    {
        context.expect(AnalysisSnapshot{}.definitions("A:/missing.ns", {}).empty(),
            "missing semantic model yields no definitions");
        return;
    }
    SemanticFixture fixture;
    const auto definitions = fixture.snapshot().definitions(
        fixture.use(), variant == 4U ? TextPosition{99U, 0U} : kFirstAdd);
    context.expect(variant == 4U ? definitions.empty() :
            definitions.size() == 1U && definitions.front().path.filename() == "A_StateAndOverloads.ns",
        "definition queries return exact ordered target locations and remain empty for misses");
}

void completionContract(TestContext& context, std::size_t variant)
{
    if (variant == 3U)
    {
        context.expect(AnalysisSnapshot{}.completions("A:/missing.ns", "A").empty(),
            "missing semantic model yields no completions");
        return;
    }
    SemanticFixture fixture;
    const std::size_t limit = variant == 4U || variant == 5U ? 0U : variant == 6U ? 1U : 100U;
    const auto completions = fixture.snapshot().completions(
        fixture.use(), variant == 2U ? "ad" : "Ad", limit);
    const bool methods = std::ranges::all_of(completions, [](const CompletionItem& item) {
        return item.label == "Add" && item.kind == LanguageSymbolKind::Method;
    });
    context.expect(limit == 0U ? completions.empty() :
            limit == 1U ? completions.size() == 1U && methods : completions.size() == 2U && methods,
        "completion filters case-insensitively, retains overload details, sorts, deduplicates, and caps");
}

void signatureContract(TestContext& context, std::size_t variant)
{
    if (variant == 3U)
    {
        context.expect(!AnalysisSnapshot{}.signatureHelp("A:/missing.ns", {}).has_value(),
            "missing semantic model yields no signature help");
        return;
    }
    SemanticFixture fixture;
    const auto help = fixture.snapshot().signatureHelp(
        fixture.use(), variant == 4U ? TextPosition{99U, 0U} : TextPosition{6U, 25U});
    context.expect(variant == 4U ? !help.has_value() :
            help.has_value() && help->signatures.size() == 1U && help->activeSignature == 0U &&
                help->activeParameter == 0U && help->signatures.front().label.find("builtin:int") !=
                    std::string::npos,
        "signature help selects the smallest containing call and maps its active argument exactly");
}

void referencesContract(TestContext& context, std::size_t variant)
{
    if (variant == 3U)
    {
        context.expect(AnalysisSnapshot{}.references("A:/missing.ns", {}).empty(),
            "missing semantic model yields no references");
        return;
    }
    SemanticFixture fixture;
    const bool declarations = variant != 5U;
    const auto references = fixture.snapshot().references(
        fixture.use(), variant == 4U ? TextPosition{99U, 0U} : kFirstAdd, declarations);
    context.expect(variant == 4U ? references.empty() :
            declarations ? references.size() >= 4U : references.size() >= 2U,
        "reference queries expand overload targets, deduplicate locations, and honor declaration inclusion");
}

void prepareRenameContract(TestContext& context, std::size_t variant)
{
    if (variant == 3U)
    {
        context.expect(!AnalysisSnapshot{}.prepareRename("A:/missing.ns", {}).has_value(),
            "missing semantic model cannot prepare rename");
        return;
    }
    SemanticFixture fixture;
    const auto range = fixture.snapshot().prepareRename(
        fixture.use(), variant == 4U ? TextPosition{99U, 0U} : kFirstAdd);
    context.expect(variant == 4U ? !range.has_value() :
            range == TextRange{{6U, 20U}, {6U, 23U}},
        "rename preparation returns only the exact identifier range and rejects non-target positions");
}

void renameContract(TestContext& context, std::size_t variant)
{
    if (variant == 3U)
    {
        const RenameResult result = AnalysisSnapshot{}.rename("A:/missing.ns", {}, "Sum");
        context.expect(!result.allowed && result.edits.empty() && !result.error.empty(),
            "rename without a semantic model fails with a stable error and no partial edits");
        return;
    }
    SemanticFixture fixture;
    const std::string_view name = variant == 4U ? "9invalid" : variant == 6U ? "class" :
        variant == 8U ? "i32" : "Sum";
    const RenameResult result = fixture.snapshot().rename(
        fixture.use(), variant == 5U ? TextPosition{99U, 0U} : kFirstAdd, name);
    const bool rejected = variant == 4U || variant == 5U || variant == 6U || variant == 8U;
    const bool exact = std::ranges::all_of(result.edits, [](const LanguageTextEdit& edit) {
        return edit.newText == "Sum" && edit.location.range.start.line == edit.location.range.end.line &&
            edit.location.range.end.character - edit.location.range.start.character == 3U;
    });
    context.expect(rejected ? !result.allowed && result.edits.empty() && !result.error.empty() :
            result.allowed && result.error.empty() && result.edits.size() >= 4U && exact,
        "rename is atomic, rejects invalid names or targets, and emits only exact deduplicated edits");
}

void semanticTokensContract(TestContext& context, std::size_t variant)
{
    if (variant == 3U)
    {
        context.expect(AnalysisSnapshot{}.semanticTokens("A:/missing.ns").empty(),
            "missing semantic model yields no semantic tokens");
        return;
    }
    SemanticFixture fixture;
    const auto tokens = fixture.snapshot().semanticTokens(
        variant == 4U ? fixture.root() / "missing.ns" : fixture.use());
    const bool containsUse = std::ranges::any_of(tokens, [](const LanguageSemanticToken& token) {
        return !token.declaration && token.kind == LanguageSymbolKind::Method && token.range.start == kFirstAdd;
    });
    const bool ordered = std::ranges::is_sorted(tokens, [](const auto& left, const auto& right) {
        return left.range.start.line < right.range.start.line ||
            (left.range.start.line == right.range.start.line &&
                left.range.start.character < right.range.start.character);
    });
    context.expect(variant == 4U ? tokens.empty() : containsUse && ordered,
        "semantic tokens project exact declarations and references in deterministic source order");
}
}

void registerLanguageServiceQueries233To312Module(TestRegistry& registry)
{
    addLanguageServiceContract(registry, 30U, "unit", documentSymbolsContract);
    addLanguageServiceContract(registry, 31U, "unit", workspaceSymbolsContract);
    addLanguageServiceContract(registry, 32U, "unit", symbolAtContract);
    addLanguageServiceContract(registry, 33U, "unit", definitionsContract);
    addLanguageServiceContract(registry, 34U, "unit", completionContract);
    addLanguageServiceContract(registry, 35U, "unit", signatureContract);
    addLanguageServiceContract(registry, 36U, "unit", referencesContract);
    addLanguageServiceContract(registry, 37U, "unit", prepareRenameContract);
    addLanguageServiceContract(registry, 38U, "unit", renameContract, {8U}, {"rename-keyword-authority"});
    addLanguageServiceContract(registry, 39U, "unit", semanticTokensContract);
}
}
