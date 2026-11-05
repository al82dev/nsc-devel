#pragma once

#include "nativesharp/Interop/CppSessionDiagnostic.h"
#include "nativesharp/Interop/CppSessionOptions.h"

#include <clang/Basic/Diagnostic.h>

#include <string>
#include <vector>

namespace nativesharp::interop::detail
{
std::vector<std::string> buildClangInvocationArguments(const CppSessionOptions& options);

class DiagnosticCollector final : public clang::DiagnosticConsumer
{
public:
    explicit DiagnosticCollector(std::vector<CppSessionDiagnostic>& diagnostics);
    void HandleDiagnostic(clang::DiagnosticsEngine::Level level, const clang::Diagnostic& info) override;

private:
    std::vector<CppSessionDiagnostic>& diagnostics_;
};
}
