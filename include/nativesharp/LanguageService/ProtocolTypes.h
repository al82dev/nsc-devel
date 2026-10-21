#pragma once

#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/LanguageService/CompletionItem.h"
#include "nativesharp/LanguageService/DocumentFormattingOptions.h"
#include "nativesharp/LanguageService/LanguageCodeAction.h"
#include "nativesharp/LanguageService/LanguageCodeActionKind.h"
#include "nativesharp/LanguageService/LanguageDiagnostic.h"
#include "nativesharp/LanguageService/LanguageDiagnosticSeverity.h"
#include "nativesharp/LanguageService/LanguageLocation.h"
#include "nativesharp/LanguageService/LanguageSemanticToken.h"
#include "nativesharp/LanguageService/LanguageSymbol.h"
#include "nativesharp/LanguageService/LanguageSymbolKind.h"
#include "nativesharp/LanguageService/LanguageTextEdit.h"
#include "nativesharp/LanguageService/MemberCompletionEntry.h"
#include "nativesharp/LanguageService/MemberCompletionEntryKind.h"
#include "nativesharp/LanguageService/MemberCompletionOverload.h"
#include "nativesharp/LanguageService/MemberCompletionRequest.h"
#include "nativesharp/LanguageService/MemberCompletionResult.h"
#include "nativesharp/LanguageService/MemberCompletionStatus.h"
#include "nativesharp/LanguageService/MemberCompletionTriggerKind.h"
#include "nativesharp/LanguageService/RenameResult.h"
#include "nativesharp/LanguageService/SignatureHelp.h"
#include "nativesharp/LanguageService/SignatureInformation.h"
#include "nativesharp/LanguageService/TextPosition.h"
#include "nativesharp/LanguageService/TextRange.h"
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
