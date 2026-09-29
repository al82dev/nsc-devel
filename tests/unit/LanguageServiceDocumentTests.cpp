#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/LanguageService/LanguageService.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace
{
int failures = 0;

void expect(bool condition, std::string_view message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
}

int main()
{
    using nativesharp::language_service::LanguageService;
    LanguageService service;
    std::string error;
    const std::filesystem::path source =
        std::filesystem::path(NSC_SOURCE_DIR) / "examples" / "project_stage22" / "Main.ns";

    expect(service.openDocument(source, "first", 1, error), "document opens in memory");
    expect(service.documentCount() == 1, "open document is counted");
    expect(!service.openDocument(source, "duplicate", 2, error), "duplicate open is rejected");
    expect(!service.updateDocument(source, "stale", 1, error), "stale update is rejected");
    expect(service.updateDocument(source, "second", 2, error), "newer update is accepted");
    const auto snapshot = service.document(source.parent_path() / "." / "MAIN.NS");
    expect(snapshot.has_value(), "Windows path lookup is normalized and case-insensitive");
    expect(snapshot.has_value() && snapshot->text == "second", "snapshot contains current text");
    expect(snapshot.has_value() && snapshot->version == 2, "snapshot contains current version");

    nativesharp::frontend::ProjectCompilation compilation;
    bool providerCalled = false;
    compilation.setSourceBufferProvider([&](const std::filesystem::path& path)
        -> std::optional<std::string> {
        if (path.filename() == "Main.ns")
        {
            providerCalled = true;
            return std::string{"\"unterminated overlay"};
        }
        return std::nullopt;
    });
    const std::filesystem::path project = source.parent_path() / "Hello.nsproj";
    const bool compiled = compilation.compileProjectSyntax(project, error);
    expect(providerCalled, "project compilation consults the in-memory provider");
    expect(!compiled, "overlay text replaces the valid source on disk");
    expect(compilation.diagnostics().hasErrors(), "overlay syntax errors are diagnosed");

    expect(service.closeDocument(source), "open document closes");
    expect(service.documentCount() == 0, "closed document is removed");
    expect(!service.document(source).has_value(), "closed document has no snapshot");

    if (failures == 0)
    {
        std::cout << "PASS: versioned in-memory documents and project overlay\n";
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
