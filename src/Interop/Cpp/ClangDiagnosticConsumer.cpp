#include "ClangSessionSupport.h"

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
