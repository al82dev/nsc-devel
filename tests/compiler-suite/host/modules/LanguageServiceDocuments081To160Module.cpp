#include "LanguageServiceTestSupport.h"

#include <array>
#include <cctype>
#include <string>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace language_service_support;
using namespace nativesharp::language_service;

std::filesystem::path documentPath(std::size_t contract, std::size_t variant)
{
    return std::filesystem::path("A:/compiler-suite/language-service") /
        ("contract-" + std::to_string(contract) + "-variant-" + std::to_string(variant) + ".ns");
}

void pathKeyContract(TestContext& context, std::size_t variant)
{
    LanguageService service;
    std::string error;
    const auto path = documentPath(11U, variant);
    const bool invalid = variant == 3U || variant == 4U;
    const bool opened = service.openDocument(invalid ? std::filesystem::path{} : path, "value", 1, error);
    if (invalid)
    {
        context.expect(!opened && !error.empty(), "empty paths fail canonicalization without mutation");
        return;
    }
    std::string spelling = path.generic_string();
    if (variant == 2U || variant == 7U)
    {
        std::ranges::transform(spelling, spelling.begin(), [](const unsigned char value) {
            return static_cast<char>(std::toupper(value));
        });
    }
    const auto snapshot = service.document(std::filesystem::path(spelling).parent_path() / "." /
        std::filesystem::path(spelling).filename());
    context.expect(opened && snapshot.has_value() && snapshot->text == "value",
        "equivalent normalized case-insensitive path spellings share one document key");
}

void neutralSnapshotContract(TestContext& context, std::size_t variant)
{
    AnalysisSnapshot snapshot;
    const auto renamed = snapshot.rename("A:/missing.ns", {0U, 0U}, variant == 4U ? "class" : "Name");
    context.expect(!snapshot.available() && snapshot.generation() == 0U && snapshot.projectPath().empty() &&
            !snapshot.currentAnalysisSucceeded() && !snapshot.semanticModelAvailable() &&
            !snapshot.usedPreviousSemanticModel() && !snapshot.cancelled() && snapshot.error().empty() &&
            snapshot.diagnostics().empty() && snapshot.documentSymbols("A:/missing.ns").empty() &&
            !renamed.allowed && !renamed.error.empty(),
        "empty snapshots expose only neutral values and retain no stale storage");
}

void openDocumentContract(TestContext& context, std::size_t variant)
{
    LanguageService service;
    std::string error;
    const auto path = documentPath(13U, variant);
    if (variant == 3U || variant == 4U)
    {
        const bool opened = service.openDocument(variant == 3U ? std::filesystem::path{} : path,
            "invalid", variant == 4U ? -1 : 1, error);
        context.expect(!opened && !error.empty() && service.documentCount() == 0U,
            "invalid path and negative versions are rejected without admission");
        return;
    }
    const bool opened = service.openDocument(path, variant == 8U ? std::string(16384U, 'x') : "first", 0, error);
    const bool duplicate = service.openDocument(path, "duplicate", 1, error);
    const auto snapshot = service.document(path);
    context.expect(opened && !duplicate && snapshot.has_value() && snapshot->version == 0 &&
            service.documentCount() == 1U && !error.empty(),
        "document admission is atomic and duplicate opens preserve the first snapshot");
}

void updateDocumentContract(TestContext& context, std::size_t variant)
{
    LanguageService service;
    std::string error;
    const auto path = documentPath(14U, variant);
    if (variant != 3U) service.openDocument(path, "first", 1, error);
    const std::int64_t version = variant == 4U ? -1 : variant == 5U ? 1 : 2;
    const bool updated = service.updateDocument(path, "second", version, error);
    const auto snapshot = service.document(path);
    if (variant == 3U || variant == 4U || variant == 5U)
    {
        context.expect(!updated && !error.empty() &&
                (variant == 3U ? !snapshot.has_value() : snapshot->text == "first"),
            "closed, negative, and stale updates fail without partial mutation");
        return;
    }
    context.expect(updated && error.empty() && snapshot.has_value() && snapshot->text == "second" &&
            snapshot->version == 2,
        "strictly newer document versions replace text and version together");
}

void closeQueryContract(TestContext& context, std::size_t variant)
{
    LanguageService service;
    std::string error;
    const auto path = documentPath(15U, variant);
    if (variant != 3U) service.openDocument(path, "value", 1, error);
    const auto first = service.document(path);
    const bool closed = service.closeDocument(variant == 4U ? std::filesystem::path{} : path);
    const bool secondClose = service.closeDocument(path);
    context.expect(variant == 3U || variant == 4U ? !closed && !first.has_value() == (variant == 3U) :
            first.has_value() && closed && !secondClose && service.documentCount() == 0U &&
                !service.document(path).has_value(),
        "document queries return copies and close removes exactly one open document");
}

void braceShieldContract(TestContext& context, std::size_t variant)
{
    LanguageService service;
    std::string error;
    const auto path = documentPath(16U, variant);
    const std::string source = variant == 3U ? "void Run() { // }\nreturn;\n}\n" :
        variant == 4U ? "void Run() {\nstring text = \"}\";\nreturn;\n}\n" :
        variant == 5U ? "void Run() { /* }\n{ */ return;\n}\n" :
        "void Run() {\nreturn;\n}\n";
    service.openDocument(path, source, 1, error);
    const auto edit = service.formatDocument(path, {2U, true});
    const std::string expectedLine = variant == 5U ? "  { */ return;" : "  return;";
    context.expect(edit.has_value() && edit->newText.find(expectedLine) != std::string::npos,
        "brace scanning ignores comments and literals while preserving structural indentation");
}

void formatterContract(TestContext& context, std::size_t variant)
{
    LanguageService service;
    std::string error;
    const auto path = documentPath(17U, variant);
    const bool crlf = variant == 2U || variant == 6U;
    const std::string newline = crlf ? "\r\n" : "\n";
    const std::string source = "namespace Demo {  " + newline + "class Item {" + newline +
        "void Run() {" + newline + "return;   " + newline + "}" + newline + "}" + newline + "}" + newline;
    service.openDocument(path, source, 1, error);
    DocumentFormattingOptions options{variant == 5U ? 0U : variant == 6U ? 99U : 2U, variant != 7U};
    const auto edit = service.formatDocument(path, options);
    const std::string indent = variant == 7U ? "\t\t\treturn;" :
        variant == 5U ? "   return;" : variant == 6U ? std::string(48U, ' ') + "return;" : "      return;";
    context.expect(edit.has_value() && edit->newText.find(indent) != std::string::npos &&
            (crlf ? edit->newText.find("\r\n") != std::string::npos :
                edit->newText.find("\r\n") == std::string::npos),
        "formatter clamps tab size, preserves newline style, removes trailing whitespace, and indents exactly");
}

void inlineAssemblyContract(TestContext& context, std::size_t variant)
{
    LanguageService service;
    std::string error;
    const auto path = documentPath(18U, variant);
    const bool shielded = variant != 3U && variant != 4U;
    const std::string source = variant == 3U ? "// asm {\nvoid Run() {\nreturn;\n}\n" :
        variant == 4U ? "string text = \"asm {\";\nvoid Run() {\nreturn;\n}\n" :
        "void Raw() {\n    asm {\nmov rax, {input}\n    }\n}\n";
    service.openDocument(path, source, 1, error);
    const auto edit = service.formatDocument(path);
    const std::string expected = variant == 3U ?
        "// asm {\nvoid Run() {\n    return;\n}\n" :
        "string text = \"asm {\";\nvoid Run() {\n    return;\n}\n";
    context.expect(shielded ? !edit.has_value() : edit.has_value() && edit->newText == expected,
        "real inline assembly is a byte-identical formatting no-op while comments and strings do not shield");
}

void formattingEditContract(TestContext& context, std::size_t variant)
{
    LanguageService service;
    std::string error;
    const auto path = documentPath(19U, variant);
    if (variant == 3U)
    {
        context.expect(!service.formatDocument(path).has_value(), "closed documents produce no formatting edit");
        return;
    }
    std::string source;
    std::string expected;
    TextPosition expectedEnd{};
    if (variant == 4U) source = "void Run()\n";
    else if (variant == 5U) source.clear();
    else if (variant == 6U) { source = "// \xF0\x9F\x98\x80   "; expected = "// \xF0\x9F\x98\x80"; expectedEnd = {0U, 8U}; }
    else if (variant == 8U) {
        constexpr std::size_t blockCount = 256U;
        for (std::size_t ordinal = 0; ordinal < blockCount; ++ordinal) {
            const std::string name = "Run" + std::to_string(ordinal);
            source += "void " + name + "() {\nreturn;\n}\n";
            expected += "void " + name + "() {\n  return;\n}\n";
        }
        expectedEnd = {static_cast<std::uint32_t>(blockCount * 3U), 0U};
    } else {
        const std::string newline = variant == 2U ? "\r\n" : "\n";
        source = "void Run() {" + newline + "return;" + newline + "}" + newline;
        expected = "void Run() {" + newline + "  return;" + newline + "}" + newline;
        expectedEnd = {3U, 0U};
    }
    service.openDocument(path, source, 1, error);
    const auto edit = service.formatDocument(path, {2U, true});
    if (variant == 4U || variant == 5U) { context.expect(!edit.has_value(), "formatting emits no edit for unchanged or empty documents"); return; }
    const bool exactEdit = edit.has_value() && edit->location.range.start == TextPosition{0U, 0U} && edit->location.range.end == expectedEnd && edit->newText == expected;
    const auto repeated = variant == 7U ? service.formatDocument(path, {2U, true}) : std::optional<LanguageTextEdit>{};
    const bool deterministic = variant != 7U || (edit.has_value() && repeated.has_value() && repeated->location.range == edit->location.range && repeated->newText == edit->newText);
    context.expect(exactEdit && deterministic, "formatting emits one exact whole-document edit only when text changes");
}

void codeActionContract(TestContext& context, std::size_t variant)
{
    LanguageService service;
    std::string error;
    const auto path = documentPath(20U, variant);
    if (variant != 3U) service.openDocument(path, "namespace Demo {\n@int value;   \n}\n", 1, error);
    const std::vector<std::string> codes = variant == 4U ?
        std::vector<std::string>{"UnknownIdentifier"} : std::vector<std::string>{"UnexpectedCharacter"};
    const TextRange range = variant == 5U ? TextRange{{1U, 0U}, {1U, 0U}} :
        variant == 6U ? TextRange{{99U, 0U}, {99U, 1U}} : TextRange{{1U, 0U}, {1U, 1U}};
    const auto actions = service.codeActions(path, range, codes, {2U, true});
    const bool hasQuickFix = std::ranges::any_of(actions, [](const LanguageCodeAction& action) {
        return action.kind == LanguageCodeActionKind::QuickFix;
    });
    const bool hasFormat = std::ranges::any_of(actions, [](const LanguageCodeAction& action) {
        return action.kind == LanguageCodeActionKind::SourceFixAll;
    });
    context.expect(variant == 3U ? actions.empty() : variant == 4U || variant == 5U || variant == 6U ?
            !hasQuickFix && hasFormat : hasQuickFix && hasFormat,
        "code actions emit only valid unexpected-character deletion and deterministic source fix-all payloads");
}
}

void registerLanguageServiceDocuments081To160Module(TestRegistry& registry)
{
    addLanguageServiceContract(registry, 11U, "unit", pathKeyContract);
    addLanguageServiceContract(registry, 12U, "unit", neutralSnapshotContract);
    addLanguageServiceContract(registry, 13U, "unit", openDocumentContract);
    addLanguageServiceContract(registry, 14U, "unit", updateDocumentContract);
    addLanguageServiceContract(registry, 15U, "unit", closeQueryContract);
    addLanguageServiceContract(registry, 16U, "unit", braceShieldContract);
    addLanguageServiceContract(registry, 17U, "unit", formatterContract);
    addLanguageServiceContract(registry, 18U, "unit", inlineAssemblyContract);
    addLanguageServiceContract(registry, 19U, "unit", formattingEditContract);
    addLanguageServiceContract(registry, 20U, "unit", codeActionContract);
}
}
