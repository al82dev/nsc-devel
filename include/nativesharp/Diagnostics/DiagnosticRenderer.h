#pragma once

#include "nativesharp/Diagnostics/Diagnostic.h"
#include "nativesharp/Source/SourceManager.h"

#include <iosfwd>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;

class DiagnosticRenderer final
{
public:
    explicit DiagnosticRenderer(const source::SourceManager& sourceManager) noexcept;

    void render(const Diagnostic& diagnostic, std::ostream& output) const;
    void renderAll(const DiagnosticEngine& engine, std::ostream& output) const;

private:
    const source::SourceManager& sourceManager_;
};
}
