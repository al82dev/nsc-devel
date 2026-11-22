#include "nativesharp/Diagnostics/DiagnosticRenderer.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
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
    if (diagnostic.id() == DiagnosticId::CppInteropDiagnostic && !diagnostic.location().isValid())
    {
        output << diagnosticSeverityName(diagnostic.severity()) << ": " << diagnostic.message() << '\n';
        // EN: Item 1.10. A C/C++ location shows its header line and a caret, like a NativeSharp one.
        // ES: Item 1.10. Una ubicacion C/C++ muestra su linea del encabezado y el subrayado, como una NativeSharp.
        if (const auto& external = diagnostic.externalLocation(); external && external->line > 0)
        {
            std::ifstream file(external->file, std::ios::binary);
            std::string text;
            for (std::uint32_t current = 1; std::getline(file, text); ++current)
                if (current == external->line)
                {
                    if (!text.empty() && text.back() == '\r') text.pop_back();
                    output << "    " << text << "\n    ";
                    for (std::size_t index = 0; index + 1 < external->column && index < text.size(); ++index)
                        output << (text[index] == '\t' ? '\t' : ' ');
                    output << "^\n";
                    break;
                }
        }
        for (const std::string& note : diagnostic.notes()) output << "note: " << note << '\n';
        return;
    }
    const source::SourcePosition position = sourceManager_.getPosition(diagnostic.location());
    const std::string_view path = sourceManager_.getPath(diagnostic.location());

    output
        << (path.empty() ? "<source>" : path)
        << ':' << position.line
        << ':' << position.column
        << ": " << diagnosticSeverityName(diagnostic.severity());
    const std::string_view code = diagnosticIdName(diagnostic.id());
    if (isNativeSharpDiagnostic(diagnostic.id())) output << '[' << code << ']';
    output << ": " << diagnostic.message() << '\n';

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

    for (const DiagnosticLabel& label : diagnostic.secondaryLabels())
    {
        const source::SourcePosition labelPosition = sourceManager_.getPosition(label.range.begin());
        const std::string_view labelPath = sourceManager_.getPath(label.range.begin());
        output << (labelPath.empty() ? "<source>" : labelPath)
               << ':' << labelPosition.line << ':' << labelPosition.column
               << ": note: " << label.message << '\n';
    }
    for (const std::string& note : diagnostic.notes()) output << "note: " << note << '\n';
}

void DiagnosticRenderer::renderAll(const DiagnosticEngine& engine, std::ostream& output) const
{
    for (const Diagnostic& diagnostic : engine.diagnostics())
    {
        render(diagnostic, output);
    }

    if (engine.suppressedDiagnosticCount() != 0)
    {
        output << "nsc: error: diagnostic limit reached after " << engine.errorCount()
               << " errors; " << engine.suppressedDiagnosticCount()
               << " additional diagnostics suppressed\n";
    }
}
}
