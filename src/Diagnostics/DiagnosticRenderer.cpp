#include "nativesharp/Diagnostics/DiagnosticRenderer.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"

#include <algorithm>
#include <ostream>
#include <string>

namespace nativesharp::diagnostics
{
DiagnosticRenderer::DiagnosticRenderer(const source::SourceManager& sourceManager) noexcept
    : sourceManager_(sourceManager)
{
}

void DiagnosticRenderer::render(const Diagnostic& diagnostic, std::ostream& output) const
{
    const source::SourcePosition position = sourceManager_.getPosition(diagnostic.location());
    const std::string_view path = sourceManager_.getPath(diagnostic.location());

    output
        << (path.empty() ? "<source>" : path)
        << ':' << position.line
        << ':' << position.column
        << ": " << diagnosticSeverityName(diagnostic.severity())
        << ": " << diagnostic.message() << '\n';

    const std::string_view line = sourceManager_.getLineText(diagnostic.location());
    if (line.empty() && position.column != 1)
    {
        return;
    }

    output << "    " << line << '\n' << "    ";

    const std::size_t caretColumn = position.column > 0 ? position.column - 1 : 0;
    for (std::size_t index = 0; index < caretColumn; ++index)
    {
        output << (index < line.size() && line[index] == '\t' ? '\t' : ' ');
    }

    std::size_t markerLength = 1;
    if (diagnostic.range().has_value())
    {
        const SourceRange range = *diagnostic.range();
        const source::SourcePosition end = sourceManager_.getPosition(range.end());
        if (end.line == position.line && range.end().offset() > range.begin().offset())
        {
            markerLength = std::max<std::size_t>(1, end.column - position.column);
        }
    }

    output << '^';
    for (std::size_t index = 1; index < markerLength; ++index)
    {
        output << '~';
    }
    output << '\n';
}

void DiagnosticRenderer::renderAll(const DiagnosticEngine& engine, std::ostream& output) const
{
    for (const Diagnostic& diagnostic : engine.diagnostics())
    {
        render(diagnostic, output);
    }
}
}
