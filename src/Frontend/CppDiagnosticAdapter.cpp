#include "nativesharp/Frontend/CppDiagnosticAdapter.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"

#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
diagnostics::DiagnosticSeverity severity(interop::CppSessionDiagnosticSeverity value)
{
    using C = interop::CppSessionDiagnosticSeverity;
    using N = diagnostics::DiagnosticSeverity;
    switch (value)
    {
    case C::Note: case C::Remark: return N::Note;
    case C::Warning: return N::Warning;
    case C::Error: case C::Fatal: return N::Error;
    }
    return N::Error;
}

std::string locationText(const interop::CppSourcePoint& point)
{
    return point.file.generic_string() + ":" + std::to_string(point.line) + ":" +
        std::to_string(point.column);
}

void collectNotes(const interop::CppSessionDiagnostic& diagnostic, std::vector<std::string>& notes)
{
    for (const auto& note : diagnostic.notes)
    {
        std::string text = note.location ? locationText(*note.location) + ": " : std::string{};
        text += note.message;
        notes.push_back(std::move(text));
        collectNotes(note, notes);
    }
}
}

void appendCppDiagnostics(
    std::span<const interop::CppSessionDiagnostic> source,
    diagnostics::DiagnosticEngine& destination,
    const CppDiagnosticLocationResolver& resolve)
{
    for (const auto& diagnostic : source)
    {
        std::optional<SourceRange> range;
        if (diagnostic.location && resolve) range = resolve(*diagnostic.location);
        if (range && !range->isValid()) range.reset();
        std::string message;
        if (diagnostic.location && !range) message = locationText(*diagnostic.location) + ": ";
        if (diagnostic.severity == interop::CppSessionDiagnosticSeverity::Fatal) message += "fatal: ";
        if (diagnostic.severity == interop::CppSessionDiagnosticSeverity::Remark) message += "remark: ";
        message += diagnostic.message;
        std::vector<std::string> notes;
        collectNotes(diagnostic, notes);
        destination.reportStructured(diagnostics::DiagnosticId::CppInteropDiagnostic,
            severity(diagnostic.severity), range.value_or(SourceRange{}), std::move(message), {},
            std::move(notes));
    }
}
}
