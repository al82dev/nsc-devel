#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "nativesharp/Language/DiagnosticArea.h"
#include "nativesharp/Language/DiagnosticFixIt.h"
#include "nativesharp/Language/DiagnosticSpan.h"

namespace nativesharp::language
{
struct DiagnosticQuality final
{
    std::string code;
    std::string english;
    std::string spanish;
    bool precisePrimarySpan = false;
    bool fixItPreservesSemantics = false;
    std::size_t cascadeCount = 0;
    DiagnosticArea area = DiagnosticArea::Borrow;
    DiagnosticSpan primarySpan;
    std::optional<DiagnosticFixIt> fixIt;
    std::vector<std::string> macroBacktrace;
};
}
