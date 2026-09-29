#pragma once

#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Source/SourceManager.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language_service
{
struct TextPosition final
{
    std::uint32_t line = 0;
    std::uint32_t character = 0;

    friend constexpr bool operator==(TextPosition, TextPosition) noexcept = default;
};

struct TextRange final
{
    TextPosition start;
    TextPosition end;

    friend constexpr bool operator==(TextRange, TextRange) noexcept = default;
};

enum class LanguageDiagnosticSeverity
{
    Note,
    Warning,
    Error
};

struct LanguageDiagnostic final
{
    std::filesystem::path path;
    TextRange range;
    LanguageDiagnosticSeverity severity = LanguageDiagnosticSeverity::Error;
    std::string code;
    std::string message;
};

enum class LanguageSymbolKind
{
    Class,
    Interface,
    Struct,
    Enum,
    Delegate,
    Function,
    Method,
    Constructor,
    Destructor,
    Field,
    Event,
    Variable,
    Parameter
};

struct LanguageLocation final
{
    std::filesystem::path path;
    TextRange range;
};

struct LanguageSymbol final
{
    std::string name;
    std::string qualifiedName;
    std::string detail;
    LanguageSymbolKind kind = LanguageSymbolKind::Variable;
    LanguageLocation location;
    TextRange selectionRange;
    bool renameable = false;
};

struct CompletionItem final
{
    std::string label;
    std::string detail;
    LanguageSymbolKind kind = LanguageSymbolKind::Variable;
};

struct SignatureInformation final
{
    std::string label;
    std::vector<std::string> parameters;
};

struct SignatureHelp final
{
    std::vector<SignatureInformation> signatures;
    std::size_t activeSignature = 0;
    std::size_t activeParameter = 0;
};

struct LanguageTextEdit final
{
    LanguageLocation location;
    std::string newText;
};

struct DocumentFormattingOptions final
{
    std::uint32_t tabSize = 4;
    bool insertSpaces = true;
};

enum class LanguageCodeActionKind
{
    QuickFix,
    SourceFixAll
};

struct LanguageCodeAction final
{
    std::string title;
    LanguageCodeActionKind kind = LanguageCodeActionKind::QuickFix;
    bool preferred = false;
    std::vector<std::string> diagnosticCodes;
    std::vector<LanguageTextEdit> edits;
};

struct RenameResult final
{
    bool allowed = false;
    std::string error;
    std::vector<LanguageTextEdit> edits;
};

struct LanguageSemanticToken final
{
    TextRange range;
    LanguageSymbolKind kind = LanguageSymbolKind::Variable;
    bool declaration = false;
};

std::optional<TextPosition> utf16PositionForOffset(
    std::string_view utf8,
    std::size_t byteOffset) noexcept;
std::optional<std::size_t> byteOffsetForUtf16Position(
    std::string_view utf8,
    TextPosition position) noexcept;
std::optional<LanguageDiagnostic> makeLanguageDiagnostic(
    const diagnostics::Diagnostic& diagnostic,
    const source::SourceManager& sourceManager);
}
