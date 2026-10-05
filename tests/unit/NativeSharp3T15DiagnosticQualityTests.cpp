#include "nativesharp/Language/IntegrationQuality.h"

#include <array>
#include <iostream>
#include <string>

using namespace nativesharp::language;

namespace
{
int checks = 0;
int failures = 0;

void check(bool condition, const char* message)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

DiagnosticQuality diagnostic(std::string code, DiagnosticArea area)
{
    DiagnosticQuality result;
    result.code = std::move(code);
    result.english = "the source construct violates its semantic contract";
    result.spanish = "la construccion fuente viola su contrato semantico";
    result.precisePrimarySpan = true;
    result.fixItPreservesSemantics = true;
    result.area = area;
    result.primarySpan = {"Main.ns", 4, 9, 4, 17};
    result.fixIt = DiagnosticFixIt{"safe_replacement", true, true};
    result.macroBacktrace = {"outer! at Main.ns:2", "inner! at Main.ns:4"};
    return result;
}
}

int main()
{
    std::array catalog = {
        diagnostic("NS3B001", DiagnosticArea::Borrow),
        diagnostic("NS3T001", DiagnosticArea::Trait),
        diagnostic("NS3M001", DiagnosticArea::Match),
        diagnostic("NS3A001", DiagnosticArea::Async),
        diagnostic("NS3D001", DiagnosticArea::Module)};
    std::string error;
    check(validateDiagnosticQualityCatalog(catalog, 80, error), "complete bilingual catalog passes");
    check(renderDiagnosticForTerminal(catalog[0], false, 40).size() == 40,
        "English rendering obeys terminal width");
    check(renderDiagnosticForTerminal(catalog[0], true, 44).size() <= 44,
        "Spanish rendering obeys terminal width");

    auto unsafe = catalog;
    unsafe[0].fixIt->preservesSemantics = false;
    check(!validateDiagnosticQualityCatalog(unsafe, 80, error), "unsafe fix-it is rejected");
    auto imprecise = catalog;
    imprecise[1].primarySpan.endColumn = 1;
    check(!validateDiagnosticQualityCatalog(imprecise, 80, error), "invalid span is rejected");
    auto cascade = catalog;
    cascade[2].cascadeCount = 3;
    check(!validateDiagnosticQualityCatalog(cascade, 80, error), "unbounded cascade is rejected");
    auto verboseMacro = catalog;
    verboseMacro[3].macroBacktrace.resize(5, "frame");
    check(!validateDiagnosticQualityCatalog(verboseMacro, 80, error), "verbose macro backtrace is rejected");
    auto duplicate = catalog;
    duplicate[4].code = duplicate[0].code;
    check(!validateDiagnosticQualityCatalog(duplicate, 80, error), "duplicate diagnostic code is rejected");
    auto missingArea = catalog;
    missingArea[4].area = DiagnosticArea::Borrow;
    check(!validateDiagnosticQualityCatalog(missingArea, 80, error), "missing diagnostic family is rejected");
    check(!validateDiagnosticQualityCatalog(catalog, 20, error), "unreadable terminal width is rejected");

    if (failures != 0)
    {
        std::cerr << failures << " of " << checks << " checks failed\n";
        return 1;
    }
    std::cout << "PASS: " << checks << " diagnostic quality checks\n";
    return 0;
}
