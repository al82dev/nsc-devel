#include "nativesharp/Compilation/ProjectDumper.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticRenderer.h"
#include "nativesharp/Frontend/CompilationStage.h"
#include "nativesharp/Frontend/FrontendCompilation.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Language/LanguageEdition.h"
#include "nativesharp/Sema/EffectContext.h"
#include "nativesharp/Sema/ExpressionInfo.h"
#include "nativesharp/Source/SourceManager.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
void writeText(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::filesystem::path writeProject(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view edition,
    std::string_view references = {})
{
    const std::filesystem::path directory = root / name;
    writeText(directory / "Main.ns", "int Answer() { return 42; }\n");
    std::string project = "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><OutputType>Library</OutputType><Language>" + std::string(edition) +
        "</Language></PropertyGroup><ItemGroup><Compile Include=\"Main.ns\"/>";
    project += references;
    project += "</ItemGroup></Project>\n";
    const std::filesystem::path path = directory / (std::string(name) + ".nsproj");
    writeText(path, project);
    return path;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp;

    const std::filesystem::path root =
        nativesharp::tests::evidenceRoot("nativesharp_ns3_t00_constitution");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    using language::LanguageEdition;
    tests.expect(language::parseLanguageEdition("2") == LanguageEdition::V2,
        "language edition 2 is selectable");
    tests.expect(language::parseLanguageEdition("3") == LanguageEdition::V3,
        "language edition 3 is selectable");
    tests.expect(!language::parseLanguageEdition("latest").has_value(),
        "implicit latest edition is rejected");
    tests.expect(language::languageEditionFingerprint(LanguageEdition::V2) !=
            language::languageEditionFingerprint(LanguageEdition::V3),
        "changing edition changes the incremental identity");

    const auto edition2Project = writeProject(root, "Edition2", "2");
    const auto edition3Project = writeProject(root, "Edition3", "3");
    std::string error;
    compilation::ProjectLoader loader;
    const auto edition2 = loader.load(edition2Project, error);
    const auto edition3 = loader.load(edition3Project, error);
    tests.expect(edition2.has_value() && edition2->languageEdition() == LanguageEdition::V2,
        "project loader publishes edition 2");
    tests.expect(edition3.has_value() && edition3->languageEdition() == LanguageEdition::V3,
        "project loader publishes edition 3");
    if (edition3.has_value())
    {
        std::ostringstream dump;
        compilation::ProjectDumper().dump(*edition3, dump);
        tests.expect(dump.str().find("language: 3") != std::string::npos,
            "project dump exposes the selected edition");
    }

    const auto invalidProject = writeProject(root, "InvalidEdition", "4");
    tests.expect(!loader.load(invalidProject, error).has_value() &&
            error.find("Language must be '2' or '3'") != std::string::npos,
        "unknown language edition is diagnostic");

    const auto dependency = writeProject(root, "Dependency", "2");
    const std::string reference =
        "<ProjectReference Include=\"../Dependency/Dependency.nsproj\"/>";
    const auto mixedRoot = writeProject(root, "MixedRoot", "3", reference);
    tests.expect(!compilation::ProjectGraphLoader().load(mixedRoot, error).has_value() &&
            error.find("NS3001") != std::string::npos,
        "project graph rejects mixed editions with stable NS3001");
    const auto compatibleRoot = writeProject(root, "CompatibleRoot", "2", reference);
    tests.expect(compilation::ProjectGraphLoader().load(compatibleRoot, error).has_value(),
        "project graph accepts one explicit edition");

    auto mixedMetadataGraph = compilation::ProjectGraphLoader().load(edition3Project, error);
    const std::array mixedMetadata = {
        compilation::PrecompiledProjectDescriptor{
            edition3Project,
            root / "LegacyEdition2.nsmeta",
            "LegacyEdition2",
            LanguageEdition::V2}};
    tests.expect(mixedMetadataGraph.has_value() &&
            !mixedMetadataGraph->addPrecompiledProjects(mixedMetadata, error) &&
            error.find("NS3001") != std::string::npos,
        "project graph rejects precompiled metadata from another edition with stable NS3001");
    auto alignedMetadataGraph = compilation::ProjectGraphLoader().load(edition3Project, error);
    const std::array alignedMetadata = {
        compilation::PrecompiledProjectDescriptor{
            edition3Project,
            root / "Edition3.nsmeta",
            "Edition3Metadata",
            LanguageEdition::V3}};
    tests.expect(alignedMetadataGraph.has_value() &&
            alignedMetadataGraph->addPrecompiledProjects(alignedMetadata, error) &&
            alignedMetadataGraph->projects().front().languageEdition() == LanguageEdition::V3,
        "precompiled project nodes preserve their published language edition");

    frontend::FrontendCompilation frontend2(LanguageEdition::V2);
    frontend::FrontendCompilation frontend3(LanguageEdition::V3);
    tests.expect(frontend2.compileBuffer("same-v2.ns", "int Answer() { return 42; }", false),
        "the same source compiles under edition 2");
    tests.expect(frontend3.compileBuffer("same-v3.ns", "int Answer() { return 42; }", false),
        "the same source compiles under edition 3");
    tests.expect(frontend2.languageEdition() == LanguageEdition::V2 &&
            frontend3.languageEdition() == LanguageEdition::V3,
        "frontend compilation unit retains its edition");

    frontend::ProjectCompilation productivePipeline;
    productivePipeline.setCompilationJobs(2);
    error.clear();
    const bool productiveMir = productivePipeline.compileProjectOopMir(edition3Project, error);
    constexpr std::array productiveStages = {
        frontend::CompilationStage::Parse,
        frontend::CompilationStage::Binding,
        frontend::CompilationStage::TypeChecking,
        frontend::CompilationStage::Comptime,
        frontend::CompilationStage::Mir,
        frontend::CompilationStage::Monomorphization,
    };
    tests.expect(productiveMir &&
            std::ranges::equal(productivePipeline.completedStages(), productiveStages),
        "the production project pipeline records every frontend stage in normative order: " + error);
    error.clear();
    tests.expect(productiveMir && productivePipeline.completeCodeGeneration(error) &&
            productivePipeline.completedStages().size() == productiveStages.size() + 1U,
        "the production codegen boundary completes the normative stage sequence: " + error);

    interop::LibraryMetadataCodec metadataCodec;
    interop::LibraryMetadata writtenMetadata;
    const std::filesystem::path metadataPath = root / "Edition3.nsmeta";
    error.clear();
    const bool metadataWritten = productiveMir && metadataCodec.write(
        productivePipeline, metadataPath, writtenMetadata, error);
    interop::LibraryMetadata decodedMetadata;
    const bool metadataRead = metadataWritten &&
        metadataCodec.read(metadataPath, decodedMetadata, error);
    tests.expect(metadataRead && writtenMetadata.languageEdition == 3U &&
            decodedMetadata.languageEdition == 3U,
        "NSMETA round-trip preserves language edition 3: " + error);

    frontend::CompilationStageTracker stages;
    tests.expect(!stages.enter(frontend::CompilationStage::Binding, error) &&
            error.find("NS3010") != std::string::npos,
        "stage model rejects a cycle or missing producer before backend work");
    constexpr std::array stageOrder = {
        frontend::CompilationStage::Parse,
        frontend::CompilationStage::Binding,
        frontend::CompilationStage::TypeChecking,
        frontend::CompilationStage::Comptime,
        frontend::CompilationStage::Mir,
        frontend::CompilationStage::Monomorphization,
        frontend::CompilationStage::Codegen,
    };
    bool orderedStages = true;
    for (const auto stage : stageOrder) orderedStages = stages.enter(stage, error) && orderedStages;
    tests.expect(orderedStages && stages.completed().size() == stageOrder.size(),
        "all normative stages execute in producer order");
    tests.expect(frontend::containsStageInformation(
            stages.availableInformation(), frontend::StageInformation::MachineCode),
        "codegen publishes machine-code information");
    tests.expect(!frontend::compilationStageContract(frontend::CompilationStage::TypeChecking)
                      .diagnosticsRequireBackend,
        "type diagnostics are independent of codegen");

    tests.expect(sema::ValueCategory::LValue == sema::ValueCategory::Place &&
            sema::ValueCategory::RValue == sema::ValueCategory::Value,
        "2.0 lvalue/rvalue spellings map to the 3.0 categories");
    tests.expect(sema::isPlaceCategory(sema::ValueCategory::BorrowedValue),
        "borrowed values preserve place semantics");
    tests.expect(sema::requiresMaterialization(sema::ValueCategory::Temporary),
        "temporaries require materialization");
    tests.expect(!sema::isUsableValueCategory(sema::ValueCategory::MovedFrom),
        "moved-from expressions cannot be reused");

    const sema::EffectContext outer(
        sema::EffectSet(sema::Effect::Unsafe).with(sema::Effect::Throws));
    const sema::EffectContext inner(sema::EffectSet(sema::Effect::NoHeap), &outer);
    tests.expect(inner.allows(sema::Effect::Unsafe) && inner.allows(sema::Effect::NoHeap),
        "nested effect contexts propagate explicitly");
    tests.expect(inner.require(sema::Effect::Async) == sema::Effect::Async,
        "missing effect is reported by identity");
    tests.expect(sema::effectParticipatesInFunctionType(sema::Effect::Throws) &&
            !sema::effectParticipatesInFunctionType(sema::Effect::Unsafe),
        "effect type participation is explicit, not implicit");

    source::SourceManager sources;
    const FileId file = sources.addBuffer("effects.ns", "call();\nsource();\n");
    diagnostics::DiagnosticEngine diagnostics;
    diagnostics.reportStructured(
        diagnostics::DiagnosticId::NS3030MissingEffectContext,
        diagnostics::DiagnosticSeverity::Lint,
        SourceRange(SourceLocation(file, 0), SourceLocation(file, 4)),
        "operation requires async context",
        {{SourceRange(SourceLocation(file, 8), SourceLocation(file, 14)), "effect originates here"}},
        {"add async to the containing function"});
    tests.expect(diagnostics.lintCount() == 1 && diagnostics.warningCount() == 0 &&
            diagnostics.errorCount() == 0,
        "lint, warning, and error accounting are separate");
    std::ostringstream rendered;
    diagnostics::DiagnosticRenderer(sources).renderAll(diagnostics, rendered);
    tests.expect(rendered.str().find("lint[NS3030]") != std::string::npos &&
            rendered.str().find("effect originates here") != std::string::npos &&
            rendered.str().find("add async") != std::string::npos,
        "structured diagnostic renders code, secondary span, and note");
    tests.expect(diagnostics::isNativeSharp3Diagnostic(
            diagnostics::DiagnosticId::NS3049DeferredLanguageSurface) &&
            diagnostics::diagnosticIdName(
                diagnostics::DiagnosticId::NS3049DeferredLanguageSurface) == "NS3049" &&
            !diagnostics::isNativeSharp3Diagnostic(diagnostics::DiagnosticId::InvalidOperand),
        "the stable NS3 diagnostic catalog is explicit and does not classify legacy IDs by text");

    return tests.resultKeeping(root);
}
