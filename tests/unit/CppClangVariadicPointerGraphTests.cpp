#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Frontend/CppImportedTypeAlias.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "nativesharp/Interop/CppProjectSessions.h"
#include "nativesharp/Interop/CppSemanticFunction.h"
#include "nativesharp/Interop/CppSemanticRecord.h"
#include "nativesharp/Interop/CppVariadicAbiReconciliation.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <variant>
#include <vector>

int main(int argc, char** argv)
{
    using namespace nativesharp;
    using namespace nativesharp::interop;
    if (argc != 3) return 1;
    CppClangSessionFactory factory;
    CppSessionOptions options;
    options.language = CppLanguageMode::C;
    options.languageStandard = "c11";
    options.targetTriple = "x86_64-pc-windows-msvc";
    options.driverReferencePath = "clang.exe";
    options.resourceDirectory = argv[1];
    options.workingDirectory = argv[2];
    options.input = CppBufferInput{"variadic-pointer-graph.c",
        "typedef struct Mixed { double x; int y; } Mixed; "
        "typedef int (__attribute__((sysv_abi)) *Callback)(int, ...);"};
    auto opened = factory.open(std::move(options), {});
    auto* holder = std::get_if<std::unique_ptr<CppInteropSession>>(&opened);
    if (!holder || !*holder) return 2;
    auto session = std::move(*holder);
    const auto records = session->recordsSnapshot();
    const auto aliases = session->typeAliasesSnapshot();
    if (!records || !aliases) return 3;
    const CppTypeAliasDeclaration* foundAlias = nullptr;
    for (const auto& alias : *aliases) if (alias.name == "Callback") foundAlias = &alias;
    if (!foundAlias || !foundAlias->clangUsr) return 4;
    const auto directory = std::filesystem::path(argv[2]) /
        ("varptr-graph-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code error;
    if (!std::filesystem::create_directory(directory, error) || error) return 5;
    const auto projectPath = directory / "graph.nsproj";
    { std::ofstream file(projectPath); if (!file) return 6; file << "<Project/>"; }
    std::vector<compilation::Project> projects;
    projects.emplace_back(projectPath, "VarProbe", compilation::ProjectOutputKind::Library,
        compilation::ProjectSourceSet(std::filesystem::path(argv[2])), std::vector<std::filesystem::path>{},
        std::vector<compilation::LibraryReference>{}, std::vector<compilation::SdkManifest>{});
    compilation::ProjectGraph graph(projectPath, std::move(projects));
    std::vector<CppSemanticRecordEntry> entries;
    for (const auto& record : *records)
        entries.push_back({{}, 0, 0, "x86_64-pc-windows-msvc", record,
                           projectCppRecordIdentity(record, "VarProbe"), std::nullopt});
    const auto groups = groupCppSemanticRecords(entries);
    auto preparedResult = prepareCppRecordObservation(entries, groups);
    auto* prepared = std::get_if<CppRecordObservationInputs>(&preparedResult);
    if (!prepared) return 7;
    frontend::ProjectNameLookup lookup;
    if (!lookup.setCppImportedTypes(std::move(prepared->types))) return 8;
    diagnostics::DiagnosticEngine diagnostics;
    std::vector<std::unique_ptr<frontend::ProjectSyntaxUnit>> units;
    if (!lookup.build(graph, units, diagnostics) || diagnostics.hasErrors()) return 9;
    std::string recordKey;
    for (const auto ordinal : lookup.typeOrdinalsForName("Mixed"))
    {
        const auto* imported = lookup.cppImportedType(ordinal);
        if (imported) recordKey = "type:" + imported->identity.key();
    }
    if (recordKey.empty()) return 10;
    frontend::CppImportedTypeAlias alias{0, projectPath, 0, "x86_64-pc-windows-msvc", *foundAlias, {}};
    const auto mapped = projectCppStoredFunctionPointer(alias.declaration.underlyingType, 0, "VarProbe", &lookup);
    if (!mapped) return 11;
    std::vector<CppProjectSessions> sessions(1);
    sessions[0].projectFile = projectPath;
    sessions[0].sessions.push_back(std::move(session));
    const std::vector<std::string> optional{recordKey};
    const auto plan = planCppVariadicFunctionPointerForAlias(graph, sessions, lookup, alias, mapped->nativeSharpTypeKey, optional);
    if (!plan || plan->arguments.size() != 2 || plan->arguments[1].physicalCount != 2 ||
        plan->physicalParameters.size() != 3 || plan->physicalParameters[1].type.kind != abi_plan::TypeKind::Double)
        return 12;
    auto bad = alias;
    bad.referenceIndex = 1;
    if (planCppVariadicFunctionPointerForAlias(graph, sessions, lookup, bad, mapped->nativeSharpTypeKey, optional) ||
        planCppVariadicFunctionPointerForAlias(graph, sessions, lookup, alias, "wrong", optional)) return 13;
    bad = alias; bad.declaration.clangUsr = "wrong";
    if (planCppVariadicFunctionPointerForAlias(graph, sessions, lookup, bad, mapped->nativeSharpTypeKey, optional)) return 14;
    bad = alias; bad.fromPrecompiledCatalog = true;
    if (planCppVariadicFunctionPointerForAlias(graph, sessions, lookup, bad, mapped->nativeSharpTypeKey, optional)) return 15;
    bad = alias; bad.projectFile = directory / "other.nsproj";
    if (planCppVariadicFunctionPointerForAlias(graph, sessions, lookup, bad, mapped->nativeSharpTypeKey, optional)) return 16;
    const std::vector<std::string> unknown{"type:VarProbe::Missing"};
    if (planCppVariadicFunctionPointerForAlias(graph, sessions, lookup, alias, mapped->nativeSharpTypeKey, unknown)) return 17;
    sessions.emplace_back(); sessions.back().projectFile = projectPath;
    if (planCppVariadicFunctionPointerForAlias(graph, sessions, lookup, alias, mapped->nativeSharpTypeKey, optional)) return 18;
    return 0;
}
