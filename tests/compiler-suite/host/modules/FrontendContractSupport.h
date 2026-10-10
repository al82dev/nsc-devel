#pragma once

#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Diagnostics/DiagnosticRenderer.h"
#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/Frontend/ProjectCompilation.h"

#include <array>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite::frontend_contract_support
{
using nativesharp::frontend::FrontendCompilation;
using nativesharp::frontend::ProjectCompilation;
using MatrixOracle = std::function<void(TestContext&, std::size_t)>;

inline std::string caseId(std::size_t ordinal)
{
    std::array<char, 20> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "TF-FRONT-%03zu", ordinal);
    return buffer.data();
}

inline std::string contractId(std::size_t ordinal)
{
    std::array<char, 20> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "CF-FRONT-%03zu", ordinal);
    return buffer.data();
}

inline void addMatrix(TestRegistry& registry, std::size_t contract, std::string layer, MatrixOracle oracle)
{
    const std::size_t first = ((contract - 1) * 8) + 1;
    for (std::size_t variant = 0; variant < 8; ++variant)
    {
        registry.add(TestCase{
            caseId(first + variant),
            contractId(contract),
            "Host.Frontend",
            layer,
            variant == 7 ? "NIGHTLY" : "PR",
            {"frontend", "phase2", "matrix-" + std::to_string(variant + 1)},
            [variant, oracle](TestContext& context) { oracle(context, variant); },
        });
    }
}

enum class ProjectStage
{
    Syntax,
    Names,
    Generics,
    Members,
    Inheritance,
    Interfaces,
    GenericConstraints,
    MethodBodies,
    VirtualDispatch,
    Destruction,
    Constructions,
    ObjectLayouts,
    Hir,
    Mir,
};

class TemporaryProject final
{
public:
    TemporaryProject(std::string_view label, std::string source)
    {
        static std::atomic<std::uint64_t> sequence{0};
        const std::uint64_t ordinal = sequence.fetch_add(1, std::memory_order_relaxed);
        directory_ = std::filesystem::current_path() /
            ("nsc-frontend-suite-" + std::string(label) + "-" + std::to_string(ordinal));
        std::filesystem::create_directories(directory_);
        sourceFile_ = directory_ / "Main.ns";
        projectFile_ = directory_ / "Suite.nsproj";
        write(sourceFile_, std::move(source));
        write(projectFile_,
            "<Project>\n"
            "  <PropertyGroup><Name>Suite</Name><OutputType>Library</OutputType></PropertyGroup>\n"
            "  <ItemGroup><Compile Include=\"Main.ns\" /></ItemGroup>\n"
            "</Project>\n");
    }

    TemporaryProject(const TemporaryProject&) = delete;
    TemporaryProject& operator=(const TemporaryProject&) = delete;

    ~TemporaryProject()
    {
        std::error_code ignored;
        std::filesystem::remove(sourceFile_, ignored);
        std::filesystem::remove(projectFile_, ignored);
        std::filesystem::remove(directory_, ignored);
    }

    const std::filesystem::path& projectFile() const noexcept { return projectFile_; }

private:
    static void write(const std::filesystem::path& path, std::string text)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    std::filesystem::path directory_;
    std::filesystem::path sourceFile_;
    std::filesystem::path projectFile_;
};

inline bool compileTo(
    ProjectCompilation& compilation, ProjectStage stage,
    const std::filesystem::path& projectFile, std::string& error)
{
    switch (stage)
    {
        case ProjectStage::Syntax: return compilation.compileProjectSyntax(projectFile, error);
        case ProjectStage::Names: return compilation.compileProjectNames(projectFile, error);
        case ProjectStage::Generics: return compilation.compileProjectGenerics(projectFile, error);
        case ProjectStage::Members: return compilation.compileProjectMembers(projectFile, error);
        case ProjectStage::Inheritance: return compilation.compileProjectInheritance(projectFile, error);
        case ProjectStage::Interfaces: return compilation.compileProjectInterfaces(projectFile, error);
        case ProjectStage::GenericConstraints:
            return compilation.compileProjectGenericConstraints(projectFile, error);
        case ProjectStage::MethodBodies: return compilation.compileProjectMethodBodies(projectFile, error);
        case ProjectStage::VirtualDispatch:
            return compilation.compileProjectVirtualDispatch(projectFile, error);
        case ProjectStage::Destruction: return compilation.compileProjectDestruction(projectFile, error);
        case ProjectStage::Constructions: return compilation.compileProjectConstructions(projectFile, error);
        case ProjectStage::ObjectLayouts: return compilation.compileProjectObjectLayouts(projectFile, error);
        case ProjectStage::Hir: return compilation.compileProjectOopHir(projectFile, error);
        case ProjectStage::Mir: return compilation.compileProjectOopMir(projectFile, error);
    }
    error = "unknown project stage";
    return false;
}

inline std::string scaledSource(std::string base, std::size_t variant)
{
    if (variant != 7) return base;
    std::string result = std::move(base);
    for (std::size_t index = 0; index < 32; ++index)
    {
        result += "\nint Scale" + std::to_string(index) + "() { return " + std::to_string(index) + "; }";
    }
    return result;
}

inline std::string projectFailureEvidence(
    const ProjectCompilation& compilation, const std::string& loadError)
{
    std::ostringstream output;
    output << "load_error=" << (loadError.empty() ? "<empty>" : loadError)
           << "; error_count=" << compilation.diagnostics().errorCount()
           << "; diagnostic_count=" << compilation.diagnostics().diagnostics().size();
    if (!compilation.diagnostics().diagnostics().empty())
    {
        std::ostringstream rendered;
        nativesharp::diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
        renderer.renderAll(compilation.diagnostics(), rendered);
        output << "; diagnostics=" << rendered.str();
    }
    return output.str();
}

inline std::string frontendFailureEvidence(const FrontendCompilation& compilation)
{
    std::ostringstream output;
    output << "error_count=" << compilation.diagnostics().errorCount()
           << "; diagnostic_count=" << compilation.diagnostics().diagnostics().size();
    if (!compilation.diagnostics().diagnostics().empty())
    {
        std::ostringstream rendered;
        nativesharp::diagnostics::DiagnosticRenderer renderer(compilation.sourceManager());
        renderer.renderAll(compilation.diagnostics(), rendered);
        output << "; diagnostics=" << rendered.str();
    }
    return output.str();
}

inline void expectProjectStage(
    TestContext& context, std::size_t contract, std::size_t variant,
    ProjectStage stage, std::string source,
    const std::function<void(TestContext&, const ProjectCompilation&)>& oracle)
{
    const std::string label = "front-" + std::to_string(contract) + "-" + std::to_string(variant);
    if (variant == 5)
    {
        ProjectCompilation missing;
        std::string error;
        const std::filesystem::path path = std::filesystem::current_path() /
            (label + "-missing.nsproj");
        context.expect(!compileTo(missing, stage, path, error), "missing project dependency must fail");
        context.expect(!error.empty(), "missing project dependency must publish a causal load error");
        return;
    }
    if (variant == 4) source = "int Broken( {";
    if (variant == 2) source.clear();
    TemporaryProject fixture(label, scaledSource(std::move(source), variant));
    ProjectCompilation compilation;
    compilation.setCompilationJobs(variant == 7 ? 4 : 1);
    std::string error;
    const bool completed = compileTo(compilation, stage, fixture.projectFile(), error);
    if (variant == 4)
    {
        context.expect(!completed, "invalid syntax must fail before publishing the requested frontend model");
        context.expect(!error.empty() || compilation.diagnostics().errorCount() != 0,
            "invalid syntax must retain a diagnostic or load error");
        return;
    }
    const std::string evidence = projectFailureEvidence(compilation, error);
    context.expect(completed,
        "valid project fixture must reach its requested frontend stage; " + evidence);
    context.expect(error.empty(),
        "successful project stage must not retain a load error; " + evidence);
    if (completed) oracle(context, compilation);
}

inline void expectHistoricalCompilation(
    TestContext& context, std::size_t variant, std::string source,
    const std::function<void(TestContext&, const FrontendCompilation&)>& oracle)
{
    FrontendCompilation compilation;
    if (variant == 4) source = "int Broken( {";
    if (variant == 2) source.clear();
    const bool completed = compilation.compileBuffer(
        std::filesystem::path("frontend-historical.ns"), scaledSource(std::move(source), variant), false);
    if (variant == 4)
    {
        context.expect(!completed, "invalid source must fail historical frontend compilation");
        context.expect(compilation.diagnostics().errorCount() != 0,
            "historical frontend failure must preserve diagnostics");
        return;
    }
    context.expect(completed,
        "valid source must complete historical frontend compilation; " +
            frontendFailureEvidence(compilation));
    if (completed) oracle(context, compilation);
}
}
