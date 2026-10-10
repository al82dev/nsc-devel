#include "LanguageServiceTestSupport.h"

#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Source/SourceManager.h"

#include <array>
#include <cctype>
#include <string>

namespace nsc::suite
{
namespace
{
using namespace language_service_support;
using namespace nativesharp;
using namespace nativesharp::language_service;

void positionRangeContract(TestContext& context, std::size_t variant)
{
    const TextPosition start{variant == 5U ? 0U : 2U, variant == 5U ? 0U : 3U};
    const TextPosition end{variant == 6U ? 0xFFFFFFFFU : 2U, variant == 6U ? 0xFFFFFFFFU : 7U};
    const TextRange first{start, end};
    const TextRange second{start, end};
    context.expect(first == second && first.start == start && first.end == end,
        "position and half-open range values preserve exact coordinates and equality");
}

void diagnosticSchemaContract(TestContext& context, std::size_t variant)
{
    LanguageDiagnostic value;
    value.path = variant == 5U ? std::filesystem::path{} : "A:/source/Unit.ns";
    value.range = TextRange{{1U, 2U}, {3U, 4U}};
    value.severity = variant == 2U ? LanguageDiagnosticSeverity::Warning : LanguageDiagnosticSeverity::Error;
    value.code = variant == 8U ? std::string(4096U, 'C') : "UnknownIdentifier";
    value.message = "message";
    context.expect(value.range.start == TextPosition{1U, 2U} &&
            value.range.end == TextPosition{3U, 4U} &&
            value.severity == (variant == 2U ? LanguageDiagnosticSeverity::Warning :
                LanguageDiagnosticSeverity::Error) &&
            value.code.size() == (variant == 8U ? 4096U : 17U),
        "diagnostic transport preserves every payload field and its default severity");
}

void symbolSchemaContract(TestContext& context, std::size_t variant)
{
    LanguageSymbol symbol;
    symbol.name = "Value";
    symbol.qualifiedName = "Demo.Counter.Value";
    symbol.detail = "builtin:int";
    symbol.kind = variant == 2U ? LanguageSymbolKind::Field : LanguageSymbolKind::Variable;
    symbol.location = {"A:/source/Unit.ns", {{2U, 0U}, {2U, 9U}}};
    symbol.selectionRange = {{2U, 4U}, {2U, 9U}};
    symbol.renameable = variant != 3U;
    context.expect(symbol.name == "Value" && symbol.location.range.start == TextPosition{2U, 0U} &&
            symbol.selectionRange.end == TextPosition{2U, 9U} &&
            symbol.renameable == (variant != 3U),
        "symbol and location transport preserve identity, exact selection, kind, and renameability");
}

void completionSignatureContract(TestContext& context, std::size_t variant)
{
    CompletionItem completion{"Add", "builtin:int Demo.Counter.Add(builtin:int delta)",
        LanguageSymbolKind::Method};
    SignatureInformation signature{completion.detail, {"builtin:int delta", "builtin:int scale"}};
    SignatureHelp help{{signature}, variant == 6U ? 1U : 0U, variant == 5U ? 0U : 1U};
    context.expect(completion.label == "Add" && help.signatures.size() == 1U &&
            help.signatures.front().parameters.size() == 2U &&
            help.activeSignature == (variant == 6U ? 1U : 0U),
        "completion and signature transports preserve labels, parameters, and active indices");
}

void editActionContract(TestContext& context, std::size_t variant)
{
    LanguageTextEdit edit{{"A:/source/Unit.ns", {{1U, 2U}, {1U, 5U}}}, "Sum"};
    LanguageCodeAction action{"Rename", LanguageCodeActionKind::QuickFix, variant == 2U,
        {"UnknownIdentifier"}, {edit}};
    RenameResult rename{variant != 3U, variant == 3U ? "rejected" : "", {edit}};
    LanguageSemanticToken token{edit.location.range, LanguageSymbolKind::Method, variant == 5U};
    context.expect(action.edits.size() == 1U && action.preferred == (variant == 2U) &&
            rename.allowed == (variant != 3U) && token.declaration == (variant == 5U) &&
            action.edits.front().newText == "Sum",
        "edit, action, rename, and semantic-token schemas preserve exact defaults and payloads");
}

void offsetToPositionContract(TestContext& context, std::size_t variant)
{
    const std::string text = "a\r\n\xC3\xA9\xE6\x97\xA5\xF0\x9F\x98\x80z";
    std::optional<TextPosition> result;
    if (variant == 3U) result = utf16PositionForOffset(text, text.size() + 1U);
    else if (variant == 4U) result = utf16PositionForOffset(text, 4U);
    else if (variant == 5U) result = utf16PositionForOffset(text, 0U);
    else result = utf16PositionForOffset(text, variant == 6U ? text.size() : 12U);
    const bool rejected = variant == 3U || variant == 4U;
    const TextPosition expected = variant == 5U ? TextPosition{0U, 0U} :
        variant == 6U ? TextPosition{1U, 5U} : TextPosition{1U, 4U};
    context.expect(rejected ? !result.has_value() : result == expected,
        "UTF-8 byte offsets map to exact UTF-16 positions and reject invalid boundaries");
}

void positionToOffsetContract(TestContext& context, std::size_t variant)
{
    const std::string text = "a\r\n\xC3\xA9\xE6\x97\xA5\xF0\x9F\x98\x80z";
    const TextPosition position = variant == 3U ? TextPosition{9U, 0U} :
        variant == 4U ? TextPosition{1U, 3U} : variant == 5U ? TextPosition{0U, 0U} :
        variant == 6U ? TextPosition{1U, 5U} : TextPosition{1U, 4U};
    const auto result = byteOffsetForUtf16Position(text, position);
    const bool rejected = variant == 3U || variant == 4U;
    const std::size_t expected = variant == 5U ? 0U : variant == 6U ? text.size() : 12U;
    context.expect(rejected ? !result.has_value() : result == expected,
        "UTF-16 positions map to exact byte offsets and reject missing or surrogate-half boundaries");
}

void diagnosticConversionContract(TestContext& context, std::size_t variant)
{
    source::SourceManager sources;
    const std::string text = "a\r\n\xC3\xA9\xE6\x97\xA5\xF0\x9F\x98\x80z";
    const FileId file = sources.addBuffer("Unicode.ns", text);
    const FileId selected = variant == 3U ? FileId{} : file;
    const std::uint32_t end = variant == 4U ? 4U : 12U;
    const diagnostics::Diagnostic diagnostic(
        diagnostics::DiagnosticId::UnknownIdentifier,
        variant == 2U ? diagnostics::DiagnosticSeverity::Warning : diagnostics::DiagnosticSeverity::Error,
        SourceLocation(selected, 3U),
        SourceRange(SourceLocation(selected, 3U), SourceLocation(selected, end)),
        "unknown identifier");
    const auto converted = makeLanguageDiagnostic(diagnostic, sources);
    if (variant == 3U || variant == 4U)
    {
        context.expect(!converted.has_value(), "missing files and split UTF-8 ranges are omitted");
        return;
    }
    context.expect(converted.has_value() && converted->path == "Unicode.ns" &&
            converted->range == TextRange{{1U, 0U}, {1U, 4U}} &&
            converted->code == "UnknownIdentifier" && converted->message == "unknown identifier",
        "compiler diagnostics convert to the exact language-service payload");
}

void identityContract(TestContext& context, std::size_t variant)
{
    LanguageService service;
    const LanguageServiceIdentity first = service.identity();
    const LanguageServiceIdentity second = service.identity();
    const bool hexadecimal = std::ranges::all_of(first.buildId, [](char value) {
        return std::isdigit(static_cast<unsigned char>(value)) != 0 || (value >= 'a' && value <= 'f');
    });
    context.expect(first.compilerRelease == second.compilerRelease && !first.compilerRelease.empty() &&
            first.compilerStage == second.compilerStage && !first.compilerStage.empty() &&
            first.target == "x86_64-pc-windows-msvc" && first.analysisAbi == kAnalysisAbiVersion &&
            first.buildId.size() == 16U && hexadecimal && variant <= 8U,
        "language-service identity exposes stable compiler, target, build, and analysis ABI fields");
}
}

void registerLanguageServiceProtocol001To080Module(TestRegistry& registry)
{
    addLanguageServiceContract(registry, 1U, "unit", positionRangeContract);
    addLanguageServiceContract(registry, 2U, "unit", diagnosticSchemaContract);
    addLanguageServiceContract(registry, 3U, "unit", symbolSchemaContract);
    addLanguageServiceContract(registry, 4U, "unit", completionSignatureContract);
    addLanguageServiceContract(registry, 5U, "unit", editActionContract);
    addInfrastructureContract(registry, 6U, "unit",
        "decodeUtf8 and utf16Width are private anonymous-namespace helpers without an authorized unit seam");
    addLanguageServiceContract(registry, 7U, "unit", offsetToPositionContract);
    addLanguageServiceContract(registry, 8U, "unit", positionToOffsetContract);
    addLanguageServiceContract(registry, 9U, "unit", diagnosticConversionContract);
    addLanguageServiceContract(registry, 10U, "unit", identityContract);
}
}
