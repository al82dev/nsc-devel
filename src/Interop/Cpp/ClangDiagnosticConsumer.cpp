#include "ClangSessionSupport.h"

#include <clang/Basic/DiagnosticIDs.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/ADT/SmallString.h>

#include <filesystem>
#include <string_view>
#include <utility>

namespace nativesharp::interop::detail
{
namespace
{
std::filesystem::path pathFromUtf8(const char* text)
{
    const std::string_view bytes(text);
    return std::filesystem::path(std::u8string(
        reinterpret_cast<const char8_t*>(bytes.data()), bytes.size()));
}
}

DiagnosticCollector::DiagnosticCollector(std::vector<CppSessionDiagnostic>& diagnostics)
    : diagnostics_(diagnostics) {}

void DiagnosticCollector::HandleDiagnostic(clang::DiagnosticsEngine::Level level, const clang::Diagnostic& info)
{
    clang::DiagnosticConsumer::HandleDiagnostic(level, info);
    if (level == clang::DiagnosticsEngine::Ignored) return;
    CppSessionDiagnostic diagnostic;
    switch (level)
    {
    case clang::DiagnosticsEngine::Note: diagnostic.severity = CppSessionDiagnosticSeverity::Note; break;
    case clang::DiagnosticsEngine::Remark: diagnostic.severity = CppSessionDiagnosticSeverity::Remark; break;
    case clang::DiagnosticsEngine::Warning: diagnostic.severity = CppSessionDiagnosticSeverity::Warning; break;
    case clang::DiagnosticsEngine::Fatal: diagnostic.severity = CppSessionDiagnosticSeverity::Fatal; break;
    default: diagnostic.severity = CppSessionDiagnosticSeverity::Error; break;
    }
    llvm::SmallString<256> message;
    info.FormatDiagnostic(message);
    diagnostic.message = std::string(message.str());
    diagnostic.originalId = info.getID();
    // EN: Item 1.2. Keep the Clang option that controls the diagnostic and its fix-its, as Clang prints them.
    // ES: Item 1.2. Conservar la opcion de Clang que controla el diagnostico y sus fix-its, como los imprime Clang.
    if (level == clang::DiagnosticsEngine::Warning || level == clang::DiagnosticsEngine::Error)
    {
        const auto option = info.getDiags()->getDiagnosticIDs()->getWarningOptionForDiag(info.getID());
        if (!option.empty()) diagnostic.message += " [-W" + option.str() + "]";
    }
    if (info.hasSourceManager())
        for (const auto& fix : info.getFixItHints())
        {
            if (fix.RemoveRange.isInvalid()) continue;
            const auto at = info.getSourceManager().getPresumedLoc(fix.RemoveRange.getBegin());
            CppSessionDiagnostic hint;
            hint.severity = CppSessionDiagnosticSeverity::Note;
            hint.message = fix.CodeToInsert.empty() ? "fix-it: remove this text"
                                                    : "fix-it: insert or replace with '" + fix.CodeToInsert + "'";
            if (at.isValid())
                hint.location = CppSourcePoint{pathFromUtf8(at.getFilename()), at.getLine(), at.getColumn()};
            diagnostic.notes.push_back(std::move(hint));
        }
    if (info.hasSourceManager() && info.getLocation().isValid())
    {
        const auto location = info.getSourceManager().getPresumedLoc(info.getLocation());
        if (location.isValid())
            diagnostic.location = CppSourcePoint{
                pathFromUtf8(location.getFilename()), location.getLine(), location.getColumn()};
    }
    if (level == clang::DiagnosticsEngine::Note && !diagnostics_.empty())
        diagnostics_.back().notes.push_back(std::move(diagnostic));
    else
        diagnostics_.push_back(std::move(diagnostic));
}
}
