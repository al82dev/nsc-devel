#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Compilation/PrecompiledProjectPreparation.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ArrayReferenceFlowState.h"
#include "nativesharp/Frontend/ArrayReferenceLatePathProof.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/MetadataHeaderControl.h"
#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using nativesharp::frontend::ProjectCompilation;
using nativesharp::language::EscapeClass;
using nativesharp::tests::TestContext;

std::optional<std::string> environmentValue(const char* name)
{
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) return std::nullopt;
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value != nullptr ? std::optional<std::string>(value) : std::nullopt;
#endif
}

nativesharp::ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const nativesharp::ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::filesystem::path writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source,
    std::string_view outputType = "Exe",
    std::string_view extraItems = {})
{
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><OutputType>" + std::string(outputType) +
        "</OutputType></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" />" + std::string(extraItems) +
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return project;
}

std::string diagnosticsText(const ProjectCompilation& compilation)
{
    std::string result;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (!result.empty()) result += " | ";
        result += diagnostic.message();
    }
    return result;
}

std::vector<EscapeClass> summaryFor(
    const ProjectCompilation& compilation,
    std::string_view memberName)
{
    if (compilation.memberModel() == nullptr || compilation.methodBodyModel() == nullptr)
        return {};
    const auto members = compilation.memberModel()->members();
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
    {
        if (members[ordinal].name != memberName) continue;
        const auto summary = compilation.methodBodyModel()->parameterEscapeSummary(ordinal);
        return {summary.begin(), summary.end()};
    }
    return {};
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t06_escape_summaries";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    const auto lateProject = writeProject(root / "late-path", "LatePath", R"NS(
namespace NativeSharp.Core { public class Exception { } }
class Resource { public int Read() { return 1; } public int Use(int value) { return value; } }
class FieldResource
{
    private int value;
    public FieldResource(int initial) { value = initial; }
    public int Read() { return value; }
}
class NontrivialFieldResource
{
    private int value = SpinBeforeEntry();
    private static int SpinBeforeEntry() { while (true) { } return 0; }
    public NontrivialFieldResource(int initial) { value = initial; }
    public int Read() { return value; }
}
class Conversion
{
    public static implicit operator int(Conversion value) { while (true) { } return 0; }
}
class BlockedArgument
{
    private static int state = SpinBeforeEntry();
    private static int SpinBeforeEntry() { while (true) { } return 0; }
    public static int Get() { return 1; }
    public static void Touch() { }
}
class NonreturningConstructor { public NonreturningConstructor() { while (true) {} } public int Read() { return 1; } }
class NonreturningDestructor { ~NonreturningDestructor() { while (true) {} } public int Read() { return 1; } }
class BaseDisposer { public virtual void Dispose(owned Resource value) { delete value; } public virtual void Touch() { } }
class DerivedDisposer : BaseDisposer { public override void Dispose(owned Resource value) { delete value; } }
class BlockedCaller
{
    private static int state = SpinBeforeEntry();
    private static int SpinBeforeEntry() { while (true) { } return 0; }
    public static int BeforeStaticInit(bool dispose)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Flow.Store(owner, refs); if (dispose) { Flow.Dispose(move(owner)); }
        int value = refs[0].Read(); delete[] refs; return value;
    }
}
class Flow
{
    public static void Store(Resource value, Resource[] refs) { refs[0] = value; }
    public static void Dispose(owned Resource value) { delete value; }
    public static void Spin() { while (true) { } }
    public static void NoOp() { }
    public static void StoreConstructor(NonreturningConstructor value, NonreturningConstructor[] refs) { refs[0] = value; }
    public static void DropConstructor(owned NonreturningConstructor value) { delete value; }
    public static void StoreDestructor(NonreturningDestructor value, NonreturningDestructor[] refs) { refs[0] = value; }
    public static void DropDestructor(owned NonreturningDestructor value) { delete value; }
    public static void StoreField(FieldResource value, FieldResource[] refs) { refs[0] = value; }
    public static void DropField(owned FieldResource value) { delete value; }
    public static void StoreNontrivial(NontrivialFieldResource value, NontrivialFieldResource[] refs) { refs[0] = value; }
    public static void DropNontrivial(owned NontrivialFieldResource value) { delete value; }
    public static int Branch(bool dispose)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Store(owner, refs); if (dispose) { Dispose(move(owner)); }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int NonReturning(bool dispose)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Store(owner, refs); Spin(); if (dispose) { Dispose(move(owner)); }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int ArgumentBlocks(bool dispose)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Store(owner, refs); if (dispose) { Dispose(move(owner)); }
        int value = refs[0].Use(BlockedArgument.Get()); delete[] refs; return value;
    }
    public static int OversizedArray(bool dispose)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[2147483648];
        Store(owner, refs); if (dispose) { Dispose(move(owner)); }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int ImplicitConversion(bool dispose, Conversion source)
    {
        int scalar = source;
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Store(owner, refs); if (dispose) { Dispose(move(owner)); }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int Exiting(bool dispose)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Store(owner, refs); if (dispose) { Dispose(move(owner)); delete[] refs; return 0; }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int ConstructorStops(bool dispose)
    {
        NonreturningConstructor owner = new NonreturningConstructor();
        NonreturningConstructor[] refs = new NonreturningConstructor[1];
        StoreConstructor(owner, refs); if (dispose) { DropConstructor(move(owner)); }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int FieldConstructor(bool dispose)
    {
        FieldResource owner = new FieldResource(40); FieldResource[] refs = new FieldResource[1];
        StoreField(owner, refs); if (dispose) { DropField(move(owner)); }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int InitializerStops(bool dispose)
    {
        NontrivialFieldResource owner = new NontrivialFieldResource(40);
        NontrivialFieldResource[] refs = new NontrivialFieldResource[1];
        StoreNontrivial(owner, refs); if (dispose) { DropNontrivial(move(owner)); }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int DestructorStops(bool dispose)
    {
        NonreturningDestructor owner = new NonreturningDestructor();
        NonreturningDestructor[] refs = new NonreturningDestructor[1];
        StoreDestructor(owner, refs); if (dispose) { DropDestructor(move(owner)); }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int Override(bool dispose)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        DerivedDisposer target = new DerivedDisposer(); Store(owner, refs);
        if (dispose) { target.Dispose(move(owner)); }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int Alias(bool dispose)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Resource[] alias = refs; Store(owner, refs); if (dispose) { Dispose(move(owner)); }
        int value = alias[0].Read(); delete[] refs; return value;
    }
    public static int Overwritten(bool dispose)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Store(owner, refs); if (dispose) { Dispose(move(owner)); }
        refs[0] = null; int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int Finally(bool dispose)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Store(owner, refs); try { if (dispose) { Dispose(move(owner)); } } finally { }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int ChangedCondition(bool dispose)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Store(owner, refs); dispose = false; if (dispose) { Dispose(move(owner)); }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int LoopFirst()
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs);
        for (int i = 0; i < 1; i++) { Dispose(move(owner)); break; }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int LoopZero()
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs);
        for (int i = 0; i < 0; i++) { Dispose(move(owner)); break; }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int LoopStops()
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs);
        for (int i = 0; i < 1; i++) { Spin(); Dispose(move(owner)); break; }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int LoopPrefixStops()
    {
        Spin();
        Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs);
        for (int i = 0; i < 1; i++) { Dispose(move(owner)); break; }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int LoopPrefixConstructor()
    {
        NonreturningConstructor side = new NonreturningConstructor();
        Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs);
        for (int i = 0; i < 1; i++) { Dispose(move(owner)); break; }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int LoopPrefixVirtual(BaseDisposer target)
    {
        target.Touch();
        Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs);
        for (int i = 0; i < 1; i++) { Dispose(move(owner)); break; }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int LoopPrefixAlias(Resource input)
    {
        Resource alias = input;
        Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs);
        for (int i = 0; i < 1; i++) { Dispose(move(owner)); break; }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int LoopPrefixThrow(bool fail) throws NativeSharp.Core.Exception
    {
        if (fail) { throw new NativeSharp.Core.Exception(); }
        Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs);
        for (int i = 0; i < 1; i++) { Dispose(move(owner)); break; }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int LoopSuffixStops()
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs);
        for (int i = 0; i < 1; i++) { Dispose(move(owner)); break; }
        Spin(); int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int LoopSuffixNoOp()
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs);
        for (int i = 0; i < 1; i++) { Dispose(move(owner)); break; }
        NoOp(); int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int LoopSuffixOverwrite()
    {
        Resource owner = new Resource(); Resource survivor = new Resource();
        Resource[] refs = new Resource[1]; Store(owner, refs);
        for (int i = 0; i < 1; i++) { Dispose(move(owner)); break; }
        Store(survivor, refs); int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int LoopSuffixAlias()
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs);
        for (int i = 0; i < 1; i++) { Dispose(move(owner)); break; }
        Resource[] alias = refs; int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int LoopSuffixStaticInit()
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs);
        for (int i = 0; i < 1; i++) { Dispose(move(owner)); break; }
        BlockedArgument.Touch(); int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int LoopSuffixVirtual(BaseDisposer target)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs);
        for (int i = 0; i < 1; i++) { Dispose(move(owner)); break; }
        target.Touch(); int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int LoopSuffixConstructor()
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs);
        for (int i = 0; i < 1; i++) { Dispose(move(owner)); break; }
        NonreturningConstructor side = new NonreturningConstructor();
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int LoopSuffixFinally()
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs);
        for (int i = 0; i < 1; i++) { Dispose(move(owner)); break; }
        try { int marker = 1; } finally { int cleanup = 0; }
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int LoopConditional(bool stop)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs);
        for (int i = 0; i < 1; i++) { if (stop) { break; } Dispose(move(owner)); break; }
        int value = refs[0].Read(); delete[] refs; return value;
    }
}
)NS");
    ProjectCompilation lateCompilation;
    std::string lateError;
    const bool lateBuilt = lateCompilation.compileProjectConstructions(lateProject, lateError);
    tests.expect(!lateBuilt && lateCompilation.constructionModel() && lateCompilation.methodBodyModel() &&
            diagnosticsText(lateCompilation).find("reference to a destroyed object") != std::string::npos,
        "construction rejects the feasible branch with a late diagnostic (" + lateError + ")");
    if (lateCompilation.constructionModel() && lateCompilation.methodBodyModel() && lateCompilation.memberModel())
    {
        using nativesharp::frontend::ArrayReferenceLatePathResult;
        const auto members = lateCompilation.memberModel()->members();
        const auto* model = lateCompilation.methodBodyModel();
        const auto proved = [&](std::string_view name)
        {
            for (const auto& observation : model->arrayReferenceReplayObservations())
                if (observation.callableOrdinal < members.size() &&
                    members[observation.callableOrdinal].name == name &&
                    nativesharp::frontend::evaluateArrayReferenceLatePath(lateCompilation, observation) ==
                        ArrayReferenceLatePathResult::ProvenDeadPath) return true;
            return false;
        };
        tests.expect(proved("Branch"), "late evaluator proves the feasible branch using real bindings");
        tests.expect(proved("Alias"), "late evaluator follows the array alias on a feasible branch");
        tests.expect(proved("FieldConstructor"), "late evaluator proves a scalar field and identity constructor");
        for (const std::string_view name : {"BeforeStaticInit", "ArgumentBlocks", "OversizedArray", "ImplicitConversion"})
        {
            bool unknownObserved = false;
            for (const auto& observation : model->arrayReferenceReplayObservations())
                if (observation.callableOrdinal < members.size() &&
                    members[observation.callableOrdinal].name == name && observation.deadPathEvidence &&
                    nativesharp::frontend::evaluateArrayReferenceLatePath(lateCompilation, observation) ==
                        ArrayReferenceLatePathResult::Unknown) unknownObserved = true;
            tests.expect(unknownObserved, "late evaluator rejects reproduced false witness: " + std::string(name));
        }
        tests.expect(!proved("NonReturning"), "late evaluator refuses an unproven nonreturning call");
        tests.expect(!proved("Exiting"), "late evaluator does not use a branch that exits before the read");
        for (const std::string_view name : {"ConstructorStops", "DestructorStops", "InitializerStops", "Override",
                "Overwritten", "Finally", "ChangedCondition"})
        {
            std::optional<std::size_t> ordinal;
            for (std::size_t i = 0; i < members.size(); ++i)
                if (members[i].name == name) ordinal = i;
            bool observed = false;
            if (ordinal)
                for (const auto& observation : model->arrayReferenceReplayObservations())
                    if (observation.callableOrdinal == *ordinal)
                    {
                        observed = true;
                        tests.expect(nativesharp::frontend::evaluateArrayReferenceLatePath(lateCompilation, observation) ==
                                ArrayReferenceLatePathResult::Unknown,
                            "late evaluator rejects unsupported observation: " + std::string(name));
                    }
            tests.expect(ordinal.has_value() && (observed || !model->arrayReferenceLinearPublicationCoverage(*ordinal)),
                "unsupported case was observed or excluded by the replay coverage gate: " + std::string(name));
        }
    }

    if (lateCompilation.nameLookup() && lateCompilation.memberModel() && lateCompilation.namespaceFunctionModel() &&
        lateCompilation.inheritanceModel() && lateCompilation.interfaceModel() && lateCompilation.destructionModel())
    {
        nativesharp::frontend::ProjectMethodBodyModel loopReplay;
        nativesharp::diagnostics::DiagnosticEngine loopDiagnostics;
        nativesharp::frontend::ProjectMethodBodyBuildOptions loopOptions;
        loopOptions.experimentalArrayReferenceReplay = true;
        loopOptions.checkArrayReferenceDeadReceivers = false;
        const bool replayReady = loopReplay.build(*lateCompilation.nameLookup(), *lateCompilation.memberModel(),
            *lateCompilation.namespaceFunctionModel(), *lateCompilation.inheritanceModel(),
            *lateCompilation.interfaceModel(), lateCompilation.genericModel(), loopDiagnostics,
            lateCompilation.genericConstraintModel(), std::nullopt, loopOptions);
        tests.expect(replayReady, "experimental loop break paths replay with resolved bindings");
        const auto members = lateCompilation.memberModel()->members();
        const auto checkPath = [&](std::string_view name, bool expected, bool expectedPrefix)
        {
            bool found = false;
            std::string detail = "observation absent";
            for (const auto& observation : loopReplay.arrayReferenceForObservations())
                if (observation.callableOrdinal < members.size() && members[observation.callableOrdinal].name == name)
                {
                    const auto path = nativesharp::frontend::inspectExperimentalForBreakPath(lateCompilation, observation);
                    detail = "identity=" + std::to_string(observation.identityComplete) +
                        " path=" + std::to_string(path.reachesAfterLoopFromEntry) +
                        " prefix=" + std::to_string(path.preLoopReachabilityProven) +
                        " destroyed=" + std::to_string(path.destroyedOwners.size());
                    found = observation.identityComplete && path.reachesAfterLoopFromEntry == expected &&
                        path.preLoopReachabilityProven == expectedPrefix &&
                        (expected ? path.destroyedOwners.size() == 1 && path.destroyedOwners[0] != nullptr &&
                            path.destroyedOwners[0]->text() == "owner" : path.destroyedOwners.empty());
                }
            tests.expect(found, "experimental loop break path and rebound identity: " + std::string(name) + " (" + detail + ")");
        };
        if (replayReady)
        {
            checkPath("LoopFirst", true, true);
            checkPath("LoopZero", false, false);
            checkPath("LoopStops", false, false);
            checkPath("LoopPrefixStops", true, false);
            checkPath("LoopPrefixConstructor", true, false);
            checkPath("LoopPrefixVirtual", true, false);
            checkPath("LoopPrefixAlias", true, false);
            checkPath("LoopPrefixThrow", true, false);
            checkPath("LoopConditional", false, false);
            const auto checkRead = [&](std::string_view name, bool expected)
            {
                bool found = false;
                for (const auto& observation : loopReplay.arrayReferenceForObservations())
                    if (observation.callableOrdinal < members.size() && members[observation.callableOrdinal].name == name)
                    {
                        const auto proof = nativesharp::frontend::inspectExperimentalForReadPath(lateCompilation, observation);
                        found = proof.provenFromCallableEntry == expected &&
                            (expected ? proof.receiver != nullptr && proof.receiver->kind() == nativesharp::ast::AstNodeKind::IndexExpression
                                      : proof.receiver == nullptr);
                    }
                tests.expect(found, "experimental path from callable entry to loop read: " + std::string(name));
            };
            checkRead("LoopFirst", true);
            checkRead("LoopSuffixNoOp", true);
            checkRead("LoopZero", false);
            checkRead("LoopPrefixStops", false);
            checkRead("LoopSuffixStops", false);
            checkRead("LoopSuffixOverwrite", false);
            checkRead("LoopSuffixAlias", true);
            checkRead("LoopSuffixStaticInit", false);
            checkRead("LoopSuffixVirtual", false);
            checkRead("LoopSuffixConstructor", false);
            checkRead("LoopSuffixFinally", false);
        }
    }

    // Same source as path-proof-fixtures/branch/Main.ns, kept as a separate
    // project so the test exercises its actual constructor and call bindings.
    const auto originalBranchProject = writeProject(root / "late-original-branch", "OriginalBranch", R"NS(class Resource { public Resource(int n) {} public int Read() { return 1; } }
class Holder { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } }
class Case { public static int Run(bool flag) { Resource owner = new Resource(40); Resource[] refs = new Resource[1]; Holder.Store(owner, refs); if (flag) { Holder.Drop(move(owner)); } int value = refs[0].Read(); delete[] refs; return value; } }
)NS");
    ProjectCompilation originalBranchCompilation;
    std::string originalBranchError;
    const bool originalBranchBuilt = originalBranchCompilation.compileProjectConstructions(originalBranchProject, originalBranchError);
    bool originalBranchProved = false;
    if (originalBranchCompilation.constructionModel() && originalBranchCompilation.methodBodyModel())
        for (const auto& observation : originalBranchCompilation.methodBodyModel()->arrayReferenceReplayObservations())
            if (nativesharp::frontend::evaluateArrayReferenceLatePath(originalBranchCompilation, observation) ==
                nativesharp::frontend::ArrayReferenceLatePathResult::ProvenDeadPath) originalBranchProved = true;
    tests.expect(!originalBranchBuilt && originalBranchProved &&
            diagnosticsText(originalBranchCompilation).find("reference to a destroyed object") != std::string::npos,
        "construction rejects the original Resource(int) branch with real bindings (" + originalBranchError +
            diagnosticsText(originalBranchCompilation) + ")");
    if (originalBranchCompilation.constructionModel() && originalBranchCompilation.methodBodyModel())
    {
        using nativesharp::frontend::ArrayReferenceLatePathResult;
        std::vector<nativesharp::frontend::ArrayReferenceReplayObservation> observations;
        for (const auto& observation : originalBranchCompilation.methodBodyModel()->arrayReferenceReplayObservations())
            if (nativesharp::frontend::evaluateArrayReferenceLatePath(originalBranchCompilation, observation) ==
                ArrayReferenceLatePathResult::ProvenDeadPath)
            {
                auto unknown = observation;
                unknown.deadPathEvidence.reset();
                observations.push_back(std::move(unknown));
                observations.push_back(observation);
                observations.push_back(observation);
                break;
            }
        nativesharp::diagnostics::DiagnosticEngine stagedDiagnostics;
        nativesharp::frontend::reportArrayReferenceLatePaths(originalBranchCompilation, observations, stagedDiagnostics);
        tests.expect(observations.size() == 3 && stagedDiagnostics.errorCount() == 1,
            "staged reporter skips unknown, reports the proven path and deduplicates callable/syntax");
        ProjectCompilation bodiesOnly;
        std::string bodiesOnlyError;
        tests.expect(bodiesOnly.compileProjectMethodBodies(originalBranchProject, bodiesOnlyError) &&
                !bodiesOnly.diagnostics().hasErrors(),
            "method-bodies stops before the late construction validation (" + bodiesOnlyError + ")");
    }

    const auto referenceFieldProject = writeProject(root / "late-reference-field", "ReferenceField", R"NS(
class ReferenceField { private ref int value; public ReferenceField(int input) { value = input; } public int Read() { return value; } }
class Holder { public static void Store(ReferenceField value, ReferenceField[] refs) { refs[0] = value; } public static void Drop(owned ReferenceField value) { delete value; } }
class Case { public static int Run(bool flag) { ReferenceField owner = new ReferenceField(40); ReferenceField[] refs = new ReferenceField[1]; Holder.Store(owner, refs); if (flag) { Holder.Drop(move(owner)); } int value = refs[0].Read(); delete[] refs; return value; } }
)NS");
    ProjectCompilation referenceFieldCompilation;
    std::string referenceFieldError;
    const bool referenceFieldBuilt = referenceFieldCompilation.compileProjectConstructions(referenceFieldProject, referenceFieldError);
    // EN: Contract replaced by the user's decision of 2026-11-13 (docs/3.0/main-fixes/04): `Drop` runs only
    //     when `flag` is true, so the later read is unproven and the conservative rule rejects it. The
    //     witness checks below still inspect the analysed model.
    // ES: Contrato reemplazado por la decision del usuario del 2026-11-13 (docs/3.0/main-fixes/04):
    //     `Drop` corre solo si `flag` es verdadero, asi la lectura posterior no esta demostrada y la regla
    //     conservadora la rechaza. Las comprobaciones de testigo de abajo siguen inspeccionando el modelo.
    tests.expect(!referenceFieldBuilt && diagnosticsText(referenceFieldCompilation).find(
            "cannot prove that the referenced object is still alive") != std::string::npos,
        "reference field fixture analysis (" + referenceFieldError + diagnosticsText(referenceFieldCompilation) + ")");
    if (referenceFieldCompilation.memberModel() && referenceFieldCompilation.methodBodyModel())
    {
        bool fieldIsReference = false, unknownReferenceObservation = false;
        for (const auto& member : referenceFieldCompilation.memberModel()->members())
            if (member.kind == nativesharp::frontend::ClassMemberKind::Field && member.name == "value" &&
                member.isReferenceReturn && member.syntax != nullptr && member.syntax->isReference())
                fieldIsReference = true;
        for (const auto& observation : referenceFieldCompilation.methodBodyModel()->arrayReferenceReplayObservations())
            if (observation.deadPathEvidence &&
                nativesharp::frontend::evaluateArrayReferenceLatePath(referenceFieldCompilation, observation) ==
                    nativesharp::frontend::ArrayReferenceLatePathResult::Unknown)
                unknownReferenceObservation = true;
        tests.expect(fieldIsReference, "fixture binds the field with a reference qualifier");
        tests.expect(unknownReferenceObservation, "reference-field witness remains unknown");
    }

    const std::filesystem::path arrayEffectsProject = writeProject(
        root / "array-direct-effects", "ArrayDirectEffects", R"NS(
namespace NativeSharp.Core { public class Exception { } }
class Resource { public int Value; }
class Marker
{
    private static Marker saved = null;
    public void PublishThis() { saved = this; }
    public int Read() { return 1; }
}
class HasConstructor { public HasConstructor() { } }
class DerivedMarker : Marker { }
class Api
{
    private int scalar;
    private static Api savedApi = null;
    private static Resource savedResource = null;
    private static Marker savedMarker = null;
    private static Marker[] savedArray = null;
    public static void Exact(Resource value, Resource[] refs) { refs[0] = value; refs[0] = value; }
    public static void Dynamic(Resource[] refs, int index, Resource value) { refs[index] = value; }
    public static void NoWrite(Resource value, Resource[] refs) { }
    public static void ExactSingle(Resource value, Resource[] refs) { refs[0] = value; }
    public static void StoreMarker(Marker value, Marker[] refs) { refs[0] = value; }
    public static void StoreUpcast(DerivedMarker value, Marker[] refs) { refs[0] = value; }
    public static owned Marker ForwardUpcast(owned DerivedMarker value) { return move(value); }
    public static void PublishMarker(Marker value) { savedMarker = value; }
    public static void PublishArray(Marker[] values) { savedArray = values; }
    public static void Clear(Marker[] values) { values[0] = null; }
    public static void DisposeMarker(owned Marker value) { delete value; }
    public static void MaybeStore(Marker value, Marker[] refs, bool store)
    {
        if (store) { refs[0] = value; }
    }
    public static void ThrowOwned(owned Marker value) throws NativeSharp.Core.Exception
    {
        throw new NativeSharp.Core.Exception();
    }
    public void ScalarSet(int value) { scalar = value; }
    public int ScalarGet() { return scalar; }
    public void PublishThis() { savedApi = this; }
    public static void PublishArgument(Resource value) { savedResource = value; }
    public static void Chain(Resource value, Resource[] refs) { Exact(value, refs); }
    public static void Reordered(Resource[] refs, Resource value) { Chain(value, refs); }
    public static void CycleA(Resource value, Resource[] refs, bool turn)
    {
        if (turn) { CycleB(value, refs, false); }
    }
    public static void CycleB(Resource value, Resource[] refs, bool turn)
    {
        if (turn) { CycleA(value, refs, false); }
        Exact(value, refs);
    }
    public virtual void VirtualStore(Resource value, Resource[] refs) { refs[0] = value; }
    public virtual void VirtualMarker(Marker value, Marker[] refs) { refs[0] = value; }
    public virtual void UnknownScalar(int value) { }
    public static void UnknownDispatch(Api target, Resource value, Resource[] refs)
    {
        target.VirtualStore(value, refs);
    }
    public static void UnknownAlias(Resource value, Resource[] refs)
    {
        Resource copy = value;
        refs[0] = copy;
    }
    public static void Dispose(owned Resource value) { delete value; }
    public static void Consume(owned Resource value) { }
    public static owned Resource Forward(owned Resource value) { return move(value); }
    public static void MaybeDispose(owned Resource value, bool dispose)
    {
        if (dispose) { delete value; }
    }
    public static int Replay()
    {
        Resource owner = new Resource();
        Resource[] refs = new Resource[1];
        ExactSingle(owner, refs);
        int before = refs[0].Value;
        Dispose(move(owner));
        int after = refs[0].Value;
        delete[] refs;
        return before + after;
    }
    public static int ReplayForward()
    {
        Resource owner = new Resource();
        Resource[] refs = new Resource[1];
        ExactSingle(owner, refs);
        raii Resource next = Forward(move(owner));
        int value = refs[0].Value + next.Value;
        delete[] refs;
        return value;
    }
    public static int ReplayImplicitCleanup()
    {
        Resource owner = new Resource();
        Resource[] refs = new Resource[1];
        ExactSingle(owner, refs);
        Consume(move(owner));
        int value = refs[0].Value;
        delete[] refs;
        return value;
    }
    public static int ReplayBranch(bool dispose)
    {
        Resource owner = new Resource();
        Resource[] refs = new Resource[1];
        ExactSingle(owner, refs);
        if (dispose) { Dispose(move(owner)); }
        int value = refs[0].Value;
        delete[] refs;
        return value;
    }
    public static int ReplayExitingBranch(bool dispose)
    {
        Resource owner = new Resource();
        Resource[] refs = new Resource[1];
        ExactSingle(owner, refs);
        if (dispose) { Dispose(move(owner)); delete[] refs; return 0; }
        int value = refs[0].Value;
        delete[] refs;
        return value;
    }
    public static int ReplayOverwrite()
    {
        Resource first = new Resource();
        Resource second = new Resource();
        Resource[] refs = new Resource[1];
        ExactSingle(first, refs);
        Dispose(move(first));
        ExactSingle(second, refs);
        int value = refs[0].Value;
        delete[] refs;
        return value;
    }
    public static int ReplayDynamic(int index)
    {
        Resource owner = new Resource();
        Resource[] refs = new Resource[1];
        Dynamic(refs, index, owner);
        int value = refs[0].Value;
        delete[] refs;
        return value;
    }
    public static int ReplayUnknown(bool dispose)
    {
        Resource owner = new Resource();
        Resource[] refs = new Resource[1];
        ExactSingle(owner, refs);
        MaybeDispose(move(owner), dispose);
        int value = refs[0].Value;
        delete[] refs;
        return value;
    }
    public static int ReplayLoop(bool repeat)
    {
        Resource owner = new Resource();
        Resource[] refs = new Resource[1];
        ExactSingle(owner, refs);
        while (repeat) { int value = refs[0].Value; break; }
        delete[] refs;
        return 0;
    }
    public static int ReplayForFirst()
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        ExactSingle(owner, refs);
        for (int i = 0; i < 1; i++) { Dispose(move(owner)); break; }
        int value = refs[0].Value; delete[] refs; return value;
    }
    public static int ReplayForZero()
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        ExactSingle(owner, refs);
        for (int i = 0; i < 0; i++) { Dispose(move(owner)); break; }
        int value = refs[0].Value; delete[] refs; return value;
    }
    public static int ReplayForUnknown(int limit)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        ExactSingle(owner, refs);
        for (int i = 0; i < limit; i++) { Dispose(move(owner)); break; }
        int value = refs[0].Value; delete[] refs; return value;
    }
    public static int ReplayScalarBefore(Api target)
    {
        target.UnknownScalar(1);
        Resource owner = new Resource();
        Resource[] refs = new Resource[1];
        ExactSingle(owner, refs);
        int value = refs[0].Value;
        delete[] refs;
        return value;
    }
    public static int ReplayUnknownAccessible(Api target)
    {
        Resource owner = new Resource();
        Resource[] refs = new Resource[1];
        ExactSingle(owner, refs);
        target.VirtualStore(owner, refs);
        int value = refs[0].Value;
        delete[] refs;
        return value;
    }
    public static int ReplayPrivateMarker()
    {
        Marker owner = new Marker();
        Marker[] refs = new Marker[1];
        StoreMarker(owner, refs);
        int value = refs[0].Read();
        delete[] refs;
        return value;
    }
    public static int ReplayPublishedArgument()
    {
        Marker owner = new Marker();
        Marker[] refs = new Marker[1];
        StoreMarker(owner, refs);
        PublishMarker(owner);
        int value = refs[0].Read();
        delete[] refs;
        return value;
    }
    public static int ReplayPublishedReceiver()
    {
        Marker owner = new Marker();
        Marker[] refs = new Marker[1];
        StoreMarker(owner, refs);
        owner.PublishThis();
        int value = refs[0].Read();
        delete[] refs;
        return value;
    }
    public static int ReplayPrivateScalarAfter(Api target)
    {
        Marker owner = new Marker();
        Marker[] refs = new Marker[1];
        StoreMarker(owner, refs);
        target.UnknownScalar(1);
        int value = refs[0].Read();
        delete[] refs;
        return value;
    }
    public static int ReplayPublishedScalarAfter(Api target)
    {
        Marker owner = new Marker();
        Marker[] refs = new Marker[1];
        StoreMarker(owner, refs);
        PublishMarker(owner);
        target.UnknownScalar(1);
        int value = refs[0].Read();
        delete[] refs;
        return value;
    }
    public static int ReplayReceiverScalarAfter(Api target)
    {
        Marker owner = new Marker();
        Marker[] refs = new Marker[1];
        StoreMarker(owner, refs);
        owner.PublishThis();
        target.UnknownScalar(1);
        int value = refs[0].Read();
        delete[] refs;
        return value;
    }
    public static int ReplayAliasScalarAfter(Api target)
    {
        Marker owner = new Marker();
        Marker[] refs = new Marker[1];
        Marker[] alias = refs;
        StoreMarker(owner, refs);
        target.UnknownScalar(1);
        int value = refs[0].Read();
        delete[] refs;
        return value;
    }
    public static int ReplayAccessibleMarker(Api target)
    {
        Marker owner = new Marker();
        Marker[] refs = new Marker[1];
        StoreMarker(owner, refs);
        target.VirtualMarker(owner, refs);
        int value = refs[0].Read();
        delete[] refs;
        return value;
    }
    public static int ReplayAccessibleAlias(Api target)
    {
        Marker owner = new Marker();
        Marker[] refs = new Marker[1];
        Marker[] alias = refs;
        StoreMarker(owner, refs);
        target.VirtualMarker(owner, alias);
        int value = refs[0].Read();
        delete[] refs;
        return value;
    }
    public static int ReplayPublishedArrayBeforeStore(Api target)
    {
        Marker[] refs = new Marker[1];
        PublishArray(refs);
        Marker owner = new Marker();
        StoreMarker(owner, refs);
        target.UnknownScalar(1);
        int value = refs[0].Read();
        delete[] refs;
        return value;
    }
    public static int ReplayIndependentArrays(Api target)
    {
        Marker first = new Marker();
        Marker second = new Marker();
        Marker[] exposed = new Marker[1];
        Marker[] privateRefs = new Marker[1];
        PublishArray(exposed);
        StoreMarker(first, exposed);
        StoreMarker(second, privateRefs);
        target.UnknownScalar(1);
        int a = exposed[0].Read();
        int b = privateRefs[0].Read();
        delete[] exposed;
        delete[] privateRefs;
        return b;
    }
    public static int ReplayConditionalOverwrite(bool store)
    {
        Marker first = new Marker();
        Marker second = new Marker();
        Marker[] refs = new Marker[1];
        StoreMarker(first, refs);
        DisposeMarker(move(first));
        MaybeStore(second, refs, store);
        int value = refs[0].Read();
        delete[] refs;
        return value;
    }
    public static int ReplayException()
    {
        Marker owner = new Marker();
        Marker[] refs = new Marker[1];
        StoreMarker(owner, refs);
        try { ThrowOwned(move(owner)); }
        catch (NativeSharp.Core.Exception error) { }
        int value = refs[0].Read();
        delete[] refs;
        return value;
    }
    public static int ReplayDoubleWrite()
    {
        Resource owner = new Resource();
        Resource[] refs = new Resource[1];
        Exact(owner, refs);
        int value = refs[0].Value;
        delete[] refs;
        return value;
    }
    public static int ReplayNullOverwrite()
    {
        Marker owner = new Marker();
        Marker[] refs = new Marker[1];
        StoreMarker(owner, refs);
        DisposeMarker(move(owner));
        refs[0] = null;
        bool isNull = refs[0] == null;
        delete[] refs;
        return 0;
    }
    public static int ReplayCallNullOverwrite()
    {
        Marker owner = new Marker();
        Marker[] refs = new Marker[1];
        StoreMarker(owner, refs);
        DisposeMarker(move(owner));
        Clear(refs);
        bool isNull = refs[0] == null;
        delete[] refs;
        return 0;
    }
    public static int ReplayAliasOverwrite()
    {
        Marker owner = new Marker();
        Marker[] refs = new Marker[1];
        StoreMarker(owner, refs);
        DisposeMarker(move(owner));
        Marker[] other = refs;
        other[0] = null;
        bool isNull = refs[0] == null;
        delete[] refs;
        return 0;
    }
    public static int ReplayArrayRebind()
    {
        Marker owner = new Marker();
        Marker[] refs = new Marker[1];
        StoreMarker(owner, refs);
        DisposeMarker(move(owner));
        refs = new Marker[1];
        bool isNull = refs[0] == null;
        delete[] refs;
        return 0;
    }
    public static int ReplayUnknownIndex(int index)
    {
        Marker owner = new Marker();
        Marker[] refs = new Marker[2];
        StoreMarker(owner, refs);
        DisposeMarker(move(owner));
        bool isNull = refs[index] == null;
        delete[] refs;
        return 0;
    }
    public static int ReplayUnevaluated()
    {
        Marker owner = new Marker();
        Marker[] refs = new Marker[1];
        StoreMarker(owner, refs);
        DisposeMarker(move(owner));
        nuint size = sizeof(refs[0]);
        delete[] refs;
        return 0;
    }
    public static int ReplayUnreachable()
    {
        Marker owner = new Marker();
        Marker[] refs = new Marker[1];
        StoreMarker(owner, refs);
        DisposeMarker(move(owner));
        return 0;
        int unreachable = refs[0].Read();
    }
}
)NS", "StaticLibrary");
    ProjectCompilation arrayEffects;
    std::string arrayEffectsError;
    const bool arrayEffectsCompiled = arrayEffects.compileProjectMethodBodies(
        arrayEffectsProject, arrayEffectsError);
    tests.expect(arrayEffectsCompiled, "direct array-write summary fixture compiles (" +
        arrayEffectsError + diagnosticsText(arrayEffects) + ")");
    const auto directEffectsFor = [&](std::string_view name)
    {
        using nativesharp::frontend::MethodDirectArrayReferenceWrite;
        std::vector<MethodDirectArrayReferenceWrite> result;
        if (arrayEffects.memberModel() == nullptr || arrayEffects.methodBodyModel() == nullptr)
            return result;
        const auto members = arrayEffects.memberModel()->members();
        for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
        {
            if (members[ordinal].name != name) continue;
            const auto writes = arrayEffects.methodBodyModel()->directArrayReferenceWrites(ordinal);
            result.assign(writes.begin(), writes.end());
            break;
        }
        return result;
    };
    const auto exactEffects = directEffectsFor("Exact");
    tests.expect(exactEffects.size() == 1 && exactEffects[0].sourceParameterOrdinal == 0 &&
            exactEffects[0].arrayParameterOrdinal == 1 && exactEffects[0].index == 0,
        "direct array summary records and deduplicates an exact slot");
    const auto dynamicEffects = directEffectsFor("Dynamic");
    tests.expect(dynamicEffects.size() == 1 && dynamicEffects[0].sourceParameterOrdinal == 2 &&
            dynamicEffects[0].arrayParameterOrdinal == 0 && !dynamicEffects[0].index.has_value(),
        "direct array summary retains a dynamic index as unknown");
    tests.expect(directEffectsFor("NoWrite").empty(),
        "a callable with no array write has no direct write effect");
    const auto propagatedEffectsFor = [&](std::string_view name)
    {
        using nativesharp::frontend::MethodDirectArrayReferenceWrite;
        std::vector<MethodDirectArrayReferenceWrite> result;
        bool unknown = true;
        if (arrayEffects.memberModel() == nullptr || arrayEffects.methodBodyModel() == nullptr)
            return std::pair{result, unknown};
        const auto members = arrayEffects.memberModel()->members();
        for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
        {
            if (members[ordinal].name != name) continue;
            const auto writes = arrayEffects.methodBodyModel()->arrayReferenceWriteSummary(ordinal);
            result.assign(writes.begin(), writes.end());
            unknown = arrayEffects.methodBodyModel()->arrayReferenceWritesUnknown(ordinal);
            break;
        }
        return std::pair{result, unknown};
    };
    const auto [chainEffects, chainUnknown] = propagatedEffectsFor("Chain");
    tests.expect(!chainUnknown && chainEffects.size() == 1 &&
            chainEffects[0].sourceParameterOrdinal == 0 &&
            chainEffects[0].arrayParameterOrdinal == 1 && chainEffects[0].index == 0,
        "array write propagates through a direct call");
    const auto [reorderedEffects, reorderedUnknown] = propagatedEffectsFor("Reordered");
    tests.expect(!reorderedUnknown && reorderedEffects.size() == 1 &&
            reorderedEffects[0].sourceParameterOrdinal == 1 &&
            reorderedEffects[0].arrayParameterOrdinal == 0 && reorderedEffects[0].index == 0,
        "array write follows parameter bindings when arguments are reordered");
    const auto [cycleEffects, cycleUnknown] = propagatedEffectsFor("CycleA");
    tests.expect(!cycleUnknown && cycleEffects.size() == 1 &&
            cycleEffects[0].sourceParameterOrdinal == 0 &&
            cycleEffects[0].arrayParameterOrdinal == 1 && cycleEffects[0].index == 0,
        "array write reaches a fixed point across recursive calls");
    tests.expect(propagatedEffectsFor("UnknownDispatch").second,
        "virtual dispatch keeps array write effects unknown");
    tests.expect(propagatedEffectsFor("UnknownAlias").second,
        "an unmapped local alias keeps array write effects unknown");
    tests.expect(propagatedEffectsFor("Clear").second,
        "a null write to a class-reference array remains an unknown write effect");
    tests.expect(directEffectsFor("StoreUpcast").empty() &&
            propagatedEffectsFor("StoreUpcast").second,
        "a nonidentity class conversion remains an unknown array write");
    const auto onlyModeledPublication = [&](std::string_view name)
    {
        if (arrayEffects.memberModel() == nullptr || arrayEffects.methodBodyModel() == nullptr)
            return false;
        const auto members = arrayEffects.memberModel()->members();
        for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
            if (members[ordinal].name == name)
                return arrayEffects.methodBodyModel()->arrayReferenceHasOnlyModeledPublication(ordinal);
        return false;
    };
    tests.expect(onlyModeledPublication("ExactSingle") && onlyModeledPublication("ScalarSet") &&
            onlyModeledPublication("ScalarGet"),
        "trivial scalar members and the modeled direct array write have no other publication");
    tests.expect(!onlyModeledPublication("PublishThis") &&
            !onlyModeledPublication("PublishArgument") && !onlyModeledPublication("Chain"),
        "receiver or argument publication and unproven call chains remain unknown");
    const auto privateConstruction = [&](std::string_view name)
    {
        if (arrayEffects.nameLookup() == nullptr || arrayEffects.methodBodyModel() == nullptr)
            return false;
        const auto types = arrayEffects.nameLookup()->namedTypes();
        for (std::size_t ordinal = 0; ordinal < types.size(); ++ordinal)
            if (types[ordinal].identity.name() == name)
                return arrayEffects.methodBodyModel()->arrayReferencePrivateDefaultConstruction(ordinal);
        return false;
    };
    tests.expect(privateConstruction("Marker") && !privateConstruction("Resource") &&
            !privateConstruction("HasConstructor") && !privateConstruction("DerivedMarker"),
        "private default construction requires a source class without fields, bases or custom constructors");
    const auto ownedEffectFor = [&](std::string_view name)
    {
        if (arrayEffects.memberModel() == nullptr || arrayEffects.methodBodyModel() == nullptr)
            return nativesharp::frontend::ArrayReferenceOwnedEffect::Unknown;
        const auto members = arrayEffects.memberModel()->members();
        for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
            if (members[ordinal].name == name)
                return arrayEffects.methodBodyModel()->arrayReferenceOwnedEffect(ordinal, 0);
        return nativesharp::frontend::ArrayReferenceOwnedEffect::Unknown;
    };
    using nativesharp::frontend::ArrayReferenceOwnedEffect;
    tests.expect(ownedEffectFor("Dispose") == ArrayReferenceOwnedEffect::Destroyed &&
            ownedEffectFor("Consume") == ArrayReferenceOwnedEffect::Destroyed,
        "simple explicit deletion and implicit owned cleanup summarize destruction");
    tests.expect(ownedEffectFor("Forward") == ArrayReferenceOwnedEffect::ReturnedSame,
        "direct owned return summarizes the preserved object identity");
    tests.expect(ownedEffectFor("MaybeDispose") == ArrayReferenceOwnedEffect::Destroyed,
        "conditional delete and normal owned cleanup both destroy the parameter");
    tests.expect(ownedEffectFor("ForwardUpcast") == ArrayReferenceOwnedEffect::Unknown,
        "an owned return with a nonidentity type remains unknown");
    tests.expect(!nativesharp::frontend::ProjectMethodBodyBuildOptions{}.experimentalArrayReferenceReplay,
        "unrestricted experimental array replay is disabled by default");
    nativesharp::frontend::ProjectMethodBodyModel replayModel;
    nativesharp::diagnostics::DiagnosticEngine replayDiagnostics;
    nativesharp::frontend::ProjectMethodBodyBuildOptions replayOptions;
    replayOptions.experimentalArrayReferenceReplay = true;
    replayOptions.checkArrayReferenceDeadReceivers = false;
    const bool replayBuilt = arrayEffects.nameLookup() != nullptr &&
        arrayEffects.memberModel() != nullptr && arrayEffects.namespaceFunctionModel() != nullptr &&
        arrayEffects.inheritanceModel() != nullptr && arrayEffects.interfaceModel() != nullptr &&
        replayModel.build(*arrayEffects.nameLookup(), *arrayEffects.memberModel(),
            *arrayEffects.namespaceFunctionModel(), *arrayEffects.inheritanceModel(),
            *arrayEffects.interfaceModel(), arrayEffects.genericModel(), replayDiagnostics,
            arrayEffects.genericConstraintModel(), std::nullopt, replayOptions);
    tests.expect(replayBuilt, "experimental replay reanalyzes callable bodies in fresh shards");
    std::optional<std::size_t> replayOrdinal;
    if (arrayEffects.memberModel() != nullptr)
    {
        const auto members = arrayEffects.memberModel()->members();
        for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
            if (members[ordinal].name == "Replay") replayOrdinal = ordinal;
    }
    std::vector<nativesharp::frontend::ArrayReferenceRead> replayReads;
    for (const auto& observation : replayModel.arrayReferenceReplayObservations())
        if (replayOrdinal.has_value() && observation.callableOrdinal == *replayOrdinal)
            replayReads.push_back(observation.state);
    tests.expect(replayReads.size() == 2 &&
            replayReads[0] == nativesharp::frontend::ArrayReferenceRead::Alive &&
            replayReads[1] == nativesharp::frontend::ArrayReferenceRead::Dead,
        "experimental replay observes explicit destruction after a live read");
    const auto readsFor = [&](std::string_view name)
    {
        std::vector<nativesharp::frontend::ArrayReferenceRead> result;
        if (arrayEffects.memberModel() == nullptr) return result;
        const auto members = arrayEffects.memberModel()->members();
        for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
            if (members[ordinal].name == name)
                for (const auto& observation : replayModel.arrayReferenceReplayObservations())
                    if (observation.callableOrdinal == ordinal) result.push_back(observation.state);
        return result;
    };
    const auto forwardReads = readsFor("ReplayForward");
    tests.expect(forwardReads.size() == 1 &&
            forwardReads[0] == nativesharp::frontend::ArrayReferenceRead::Alive,
        "experimental replay preserves aliases when an owned return transfers the object");
    const auto cleanupReads = readsFor("ReplayImplicitCleanup");
    tests.expect(cleanupReads.size() == 1 &&
            cleanupReads[0] == nativesharp::frontend::ArrayReferenceRead::Dead,
        "experimental replay applies simple implicit owned cleanup");
    const auto branchReads = readsFor("ReplayBranch");
    tests.expect(branchReads.size() == 1 &&
            branchReads[0] == nativesharp::frontend::ArrayReferenceRead::MaybeDead,
        "experimental replay joins a possible destruction");
    bool branchProvenance = false;
    if (arrayEffects.memberModel() != nullptr)
    {
        const auto members = arrayEffects.memberModel()->members();
        for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
            if (members[ordinal].name == "ReplayBranch")
                for (const auto& observation : replayModel.arrayReferenceReplayObservations())
                    if (observation.callableOrdinal == ordinal && observation.deadPathEvidence.has_value())
                    {
                        const auto& evidence = *observation.deadPathEvidence;
                        bool branch = false, write = false, destroy = false;
                        for (const auto& event : evidence.events)
                        {
                            branch = branch || (event.kind == nativesharp::frontend::ArrayReferenceTraceKind::Branch &&
                                event.syntax != nullptr && event.branch.has_value());
                            write = write || (event.kind == nativesharp::frontend::ArrayReferenceTraceKind::Write &&
                                event.syntax != nullptr && event.source != nullptr && event.destination != nullptr &&
                                event.index == 0);
                            destroy = destroy || (event.kind == nativesharp::frontend::ArrayReferenceTraceKind::Destruction &&
                                event.syntax != nullptr && event.source != nullptr);
                        }
                        branchProvenance = !evidence.complete && branch && write && destroy;
                    }
    }
    tests.expect(branchProvenance,
        "replay exports branch, source/array slot and destruction syntax without claiming a complete proof");
    const auto exitingReads = readsFor("ReplayExitingBranch");
    tests.expect(exitingReads.size() == 1 &&
            exitingReads[0] == nativesharp::frontend::ArrayReferenceRead::Alive,
        "an exiting branch does not contaminate a reachable read");
    const auto overwriteReads = readsFor("ReplayOverwrite");
    tests.expect(overwriteReads.size() == 1 &&
            overwriteReads[0] == nativesharp::frontend::ArrayReferenceRead::Alive,
        "overwriting the exact slot removes its old object reference");
    const auto dynamicReads = readsFor("ReplayDynamic");
    tests.expect(dynamicReads.size() == 1 &&
            dynamicReads[0] == nativesharp::frontend::ArrayReferenceRead::Unknown,
        "a dynamic write does not prove which slot was assigned");
    const auto unknownReads = readsFor("ReplayUnknown");
    tests.expect(unknownReads.size() == 1 &&
            unknownReads[0] == nativesharp::frontend::ArrayReferenceRead::Dead,
        "a conditional delete or implicit owned cleanup leaves the subsequent read dead");
    const auto loopReads = readsFor("ReplayLoop");
    tests.expect(loopReads.size() == 1 &&
            loopReads[0] == nativesharp::frontend::ArrayReferenceRead::Unknown,
        "loop reads stay unknown until back-edge analysis reaches a fixed point");
    const auto checkForFacts = [&](std::string_view name, nativesharp::frontend::ExperimentalForFirstCondition expected)
    {
        bool found = false;
        const auto members = arrayEffects.memberModel()->members();
        for (const auto& observation : replayModel.arrayReferenceForObservations())
            if (observation.callableOrdinal < members.size() && members[observation.callableOrdinal].name == name)
            {
                const auto facts = nativesharp::frontend::inspectExperimentalForFirstEvaluation(arrayEffects, observation);
                found = facts.firstCondition == expected && !facts.terminationProven &&
                    !observation.beforeLoop.unknown && observation.breakStates.size() == 1 &&
                    observation.breakStates[0].unknown && !observation.truncated &&
                    (expected == nativesharp::frontend::ExperimentalForFirstCondition::Unknown || facts.boundBreakCount == 1);
            }
        tests.expect(found, "experimental for facts keep condition, break snapshot and termination distinct: " + std::string(name));
    };
    if (arrayEffects.memberModel() != nullptr)
    {
        checkForFacts("ReplayForFirst", nativesharp::frontend::ExperimentalForFirstCondition::True);
        checkForFacts("ReplayForZero", nativesharp::frontend::ExperimentalForFirstCondition::False);
        checkForFacts("ReplayForUnknown", nativesharp::frontend::ExperimentalForFirstCondition::Unknown);
    }
    const auto scalarBeforeReads = readsFor("ReplayScalarBefore");
    tests.expect(scalarBeforeReads.size() == 1 &&
            scalarBeforeReads[0] == nativesharp::frontend::ArrayReferenceRead::Alive,
        "an unknown scalar call before object creation does not taint later fresh identities");
    const auto accessibleReads = readsFor("ReplayUnknownAccessible");
    tests.expect(accessibleReads.size() == 1 &&
            accessibleReads[0] == nativesharp::frontend::ArrayReferenceRead::Unknown,
        "an unknown call that can access a tracked alias remains unknown");
    const auto publicationFor = [&](std::string_view name)
    {
        std::vector<bool> result;
        if (arrayEffects.memberModel() == nullptr) return result;
        const auto members = arrayEffects.memberModel()->members();
        for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
            if (members[ordinal].name == name)
                for (const auto& observation : replayModel.arrayReferenceReplayObservations())
                    if (observation.callableOrdinal == ordinal)
                        result.push_back(observation.publicationUnknown);
        return result;
    };
    const auto privatePublication = publicationFor("ReplayPrivateMarker");
    tests.expect(privatePublication.size() == 1 && !privatePublication[0],
        "fresh fieldless source objects passed only to modeled local storage remain unpublished");
    const auto argumentPublication = publicationFor("ReplayPublishedArgument");
    tests.expect(argumentPublication.size() == 1 && argumentPublication[0],
        "passing an object to an unmodeled publishing method marks publication unknown");
    const auto receiverPublication = publicationFor("ReplayPublishedReceiver");
    tests.expect(receiverPublication.size() == 1 && receiverPublication[0],
        "a receiver method that can publish this marks publication unknown");
    const auto privateScalarReads = readsFor("ReplayPrivateScalarAfter");
    tests.expect(privateScalarReads.size() == 1 &&
            privateScalarReads[0] == nativesharp::frontend::ArrayReferenceRead::Alive,
        "a scalar-only unknown call cannot reach a proven private object");
    const auto linearCoverageFor = [&](std::string_view name)
    {
        if (arrayEffects.memberModel() == nullptr || arrayEffects.methodBodyModel() == nullptr)
            return false;
        const auto members = arrayEffects.memberModel()->members();
        for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
            if (members[ordinal].name == name)
                return arrayEffects.methodBodyModel()->arrayReferenceLinearPublicationCoverage(ordinal);
        return false;
    };
    tests.expect(linearCoverageFor("ReplayPrivateScalarAfter") &&
            linearCoverageFor("ReplayAliasScalarAfter") && !linearCoverageFor("ReplayLoop"),
        "only fully recognized linear caller syntax gets publication coverage");
    const auto aliasScalarReads = readsFor("ReplayAliasScalarAfter");
    tests.expect(aliasScalarReads.size() == 1 &&
            aliasScalarReads[0] == nativesharp::frontend::ArrayReferenceRead::Alive,
        "a private array alias remains live after a scalar-only call");
    for (std::string_view name : {"ReplayPublishedScalarAfter", "ReplayReceiverScalarAfter",
                                  "ReplayAccessibleMarker", "ReplayAccessibleAlias",
                                  "ReplayPublishedArrayBeforeStore"})
    {
        const auto reads = readsFor(name);
        tests.expect(reads.size() == 1 && reads[0] == nativesharp::frontend::ArrayReferenceRead::Unknown,
            std::string(name) + " retains unknown when publication or access is possible");
    }
    const auto independentReads = readsFor("ReplayIndependentArrays");
    tests.expect(independentReads.size() == 2 &&
            independentReads[0] == nativesharp::frontend::ArrayReferenceRead::Unknown &&
            independentReads[1] == nativesharp::frontend::ArrayReferenceRead::Alive,
        "an escaped array does not taint an independent private array or object");
    const auto conditionalReads = readsFor("ReplayConditionalOverwrite");
    tests.expect(conditionalReads.size() == 1 &&
            conditionalReads[0] == nativesharp::frontend::ArrayReferenceRead::Unknown,
        "a conditional unmodeled overwrite cannot prove a slot identity");
    const auto exceptionReads = readsFor("ReplayException");
    tests.expect(exceptionReads.size() == 1 &&
            exceptionReads[0] == nativesharp::frontend::ArrayReferenceRead::Unknown &&
            !linearCoverageFor("ReplayException"),
        "exceptional paths remain unknown and outside the linear coverage proof");
    for (std::string_view name : {"ReplayDoubleWrite", "ReplayNullOverwrite", "ReplayCallNullOverwrite",
                                  "ReplayAliasOverwrite", "ReplayArrayRebind", "ReplayUnknownIndex"})
    {
        const auto reads = readsFor(name);
        tests.expect(reads.size() == 1 && reads[0] == nativesharp::frontend::ArrayReferenceRead::Unknown,
            std::string(name) + " does not infer a dead object from an unproven slot");
    }
    tests.expect(readsFor("ReplayUnevaluated").empty(),
        "sizeof operand does not record a runtime read");
    tests.expect(readsFor("ReplayUnreachable").empty(),
        "unreachable statements do not retain replay observations");
    tests.expect(!linearCoverageFor("ReplayNullOverwrite") &&
            !linearCoverageFor("ReplayAliasOverwrite") &&
            !linearCoverageFor("ReplayArrayRebind") &&
            !linearCoverageFor("ReplayUnknownIndex") &&
            !linearCoverageFor("ReplayUnreachable"),
        "unsafe or unsupported forms do not receive linear diagnostic coverage");
    const auto receiverFlagsFor = [&](std::string_view name)
    {
        std::vector<bool> result;
        if (arrayEffects.memberModel() == nullptr) return result;
        const auto members = arrayEffects.memberModel()->members();
        for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
            if (members[ordinal].name == name)
                for (const auto& observation : replayModel.arrayReferenceReplayObservations())
                    if (observation.callableOrdinal == ordinal)
                        result.push_back(observation.instanceMethodReceiver);
        return result;
    };
    const auto comparisonFlags = receiverFlagsFor("ReplayNullOverwrite");
    const auto methodFlags = receiverFlagsFor("ReplayPrivateMarker");
    tests.expect(comparisonFlags.size() == 1 && !comparisonFlags[0] &&
            methodFlags.size() == 1 && methodFlags[0],
        "comparison is distinguished from using the slot as an instance method receiver");

    using nativesharp::frontend::ArrayReferenceFlowState;
    using nativesharp::frontend::ArrayReferenceRead;
    const nativesharp::frontend::ArrayReferenceSlot firstSlot{100, 0};
    ArrayReferenceFlowState live;
    live.create(1, 10);
    live.arrays.insert(100);
    live.write(firstSlot, {10});
    tests.expect(live.read(firstSlot) == ArrayReferenceRead::Alive,
        "flow model records a live object in an exact slot");
    auto aliasLeft = live;
    aliasLeft.arrayAliases[101] = 100;
    tests.expect(aliasLeft.arrayForLocal(101) == 100,
        "local array aliases resolve to the storage identity");
    tests.expect(!ArrayReferenceFlowState::join(aliasLeft, live).arrayForLocal(101).has_value(),
        "a branch-local alias is not assumed present after a join");
    tests.expect(ArrayReferenceFlowState::join(aliasLeft, aliasLeft).arrayForLocal(101) == 100,
        "an identical alias in both branches retains its storage identity");
    ArrayReferenceFlowState transferred = live;
    transferred.transfer(1, 2);
    tests.expect(transferred.read(firstSlot) == ArrayReferenceRead::Alive,
        "flow model preserves object identity across owner transfer");
    tests.expect(ArrayReferenceFlowState::join(live, transferred).read(firstSlot) ==
            ArrayReferenceRead::Alive,
        "joining branches with different live owners preserves the object");
    ArrayReferenceFlowState cleaned = transferred;
    cleaned.cleanup(2);
    tests.expect(cleaned.read(firstSlot) == ArrayReferenceRead::Dead,
        "flow model marks references after owner cleanup as possibly dead");
    tests.expect(ArrayReferenceFlowState::join(live, cleaned).read(firstSlot) ==
            ArrayReferenceRead::MaybeDead,
        "join preserves a possibly destroyed object");
    ArrayReferenceFlowState conditional = cleaned;
    conditional.create(3, 11);
    conditional.writeMaybe(firstSlot, {11});
    tests.expect(conditional.read(firstSlot) == ArrayReferenceRead::MaybeDead,
        "weak update joins old and new object identities");
    cleaned.create(3, 11);
    cleaned.write(firstSlot, {11});
    tests.expect(cleaned.read(firstSlot) == ArrayReferenceRead::Alive,
        "exact overwrite replaces the old slot fact");
    ArrayReferenceFlowState wildcard = live;
    wildcard.write({100, std::nullopt}, {10});
    tests.expect(wildcard.read(firstSlot) == ArrayReferenceRead::Unknown,
        "an unknown index does not prove the identity of a slot");
    ArrayReferenceFlowState unknown = live;
    unknown.markUnknown();
    tests.expect(unknown.read(firstSlot) == ArrayReferenceRead::Unknown &&
            ArrayReferenceFlowState::join(live, unknown).read(firstSlot) == ArrayReferenceRead::Unknown,
        "unknown effects remain unknown through joins");
    ArrayReferenceFlowState absentSlot = live;
    absentSlot.slots.clear();
    tests.expect(ArrayReferenceFlowState::join(live, absentSlot).read(firstSlot) ==
            ArrayReferenceRead::Unknown,
        "a slot missing on one branch is not assumed initialized");

    std::string denseSource = R"NS(
class DenseResource { }
class DenseApi
{
    public static void Seed(DenseResource value, DenseResource[] refs) { refs[0] = value; }
)NS";
    constexpr int denseWidth = 16;
    for (int i = 0; i < denseWidth; ++i)
    {
        denseSource += "public static void F" + std::to_string(i) +
            "(DenseResource value, DenseResource[] refs) { Seed(value, refs); ";
        for (int j = 0; j < i; ++j)
            denseSource += "F" + std::to_string(j) + "(value, refs); ";
        denseSource += "}\n";
    }
    denseSource += "}\n";
    const auto denseProject = writeProject(root / "dense-array-effects", "DenseArrayEffects",
        denseSource, "StaticLibrary");
    ProjectCompilation denseCompilation;
    std::string denseError;
    const bool denseBuilt = denseCompilation.compileProjectMethodBodies(denseProject, denseError);
    bool denseSummaryCorrect = false;
    if (denseBuilt && denseCompilation.memberModel() != nullptr && denseCompilation.methodBodyModel() != nullptr)
    {
        const auto members = denseCompilation.memberModel()->members();
        for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
            if (members[ordinal].name == "F15")
            {
                const auto effects = denseCompilation.methodBodyModel()->arrayReferenceWriteSummary(ordinal);
                denseSummaryCorrect = effects.size() == 1 && effects[0].sourceParameterOrdinal == 0 &&
                    effects[0].arrayParameterOrdinal == 1 && effects[0].index == 0 &&
                    !denseCompilation.methodBodyModel()->arrayReferenceWritesUnknown(ordinal);
            }
    }
    tests.expect(denseBuilt && denseSummaryCorrect,
        "dense call graph converges to one deduplicated array effect (" + denseError +
            diagnosticsText(denseCompilation) + ")");

    const std::string deadFixture = R"NS(
class Resource
{
    public Resource() { }
    public Resource(int value) { }
    public int Read() { return 1; }
}
class Holder
{
    public static void Store(Resource value, Resource[] refs) { refs[0] = value; }
    public static void Drop(owned Resource value) { delete value; }
    public static owned Resource Forward(owned Resource value) { return move(value); }
    public static void Clear(Resource[] refs) { refs[0] = null; }
    public static void MaybeDrop(owned Resource value, bool drop) { if (drop) { delete value; } }
    public static void MaybeDropUnproven(owned Resource value, bool drop) { if (drop) { delete value; } else { while (true) {} } }
}
)NS";
    const auto deadProject = writeProject(root / "dead-receiver", "DeadReceiver", deadFixture + R"NS(
class Case
{
    public static int Run()
    {
        Resource owner = new Resource(40);
        Resource[] refs = new Resource[1];
        Holder.Store(owner, refs);
        int before = refs[0].Read();
        Holder.Drop(move(owner));
        int after = refs[0].Read();
        delete[] refs;
        return before + after;
    }
}
)NS", "StaticLibrary");
    ProjectCompilation deadCompilation;
    std::string deadError;
    const bool deadAccepted = deadCompilation.compileProjectMethodBodies(deadProject, deadError);
    tests.expect(!deadAccepted && diagnosticsText(deadCompilation).find("reference to a destroyed object") !=
            std::string::npos,
        "public analysis rejects a proven dead instance receiver (" + deadError +
            diagnosticsText(deadCompilation) + ")");

    const auto branchDeadProject = writeProject(root / "dead-in-branch", "DeadInBranch", deadFixture + R"NS(
class Case
{
    public static int Run(bool flag)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Holder.Store(owner, refs);
        if (flag) { Holder.Drop(move(owner)); refs[0].Read(); }
        else { Holder.Drop(move(owner)); }
        delete[] refs; return 0;
    }
}
)NS", "StaticLibrary");
    ProjectCompilation branchDeadCompilation;
    std::string branchDeadError;
    const bool branchDeadAccepted = branchDeadCompilation.compileProjectMethodBodies(branchDeadProject, branchDeadError);
    tests.expect(!branchDeadAccepted && diagnosticsText(branchDeadCompilation).find("reference to a destroyed object") !=
            std::string::npos,
        "public analysis rejects a receiver destroyed on its branch (" + branchDeadError +
            diagnosticsText(branchDeadCompilation) + ")");

    const auto joinDeadProject = writeProject(root / "dead-after-branch", "DeadAfterBranch", deadFixture + R"NS(
class Case
{
    public static int Run(bool flag)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Holder.Store(owner, refs);
        if (flag) { Holder.Drop(move(owner)); } else { Holder.Drop(move(owner)); }
        int value = refs[0].Read(); delete[] refs; return value;
    }
}
)NS", "StaticLibrary");
    ProjectCompilation joinDeadCompilation;
    std::string joinDeadError;
    const bool joinDeadAccepted = joinDeadCompilation.compileProjectMethodBodies(joinDeadProject, joinDeadError);
    tests.expect(!joinDeadAccepted && diagnosticsText(joinDeadCompilation).find("reference to a destroyed object") !=
            std::string::npos,
        "public analysis rejects a receiver destroyed on both branches (" + joinDeadError +
            diagnosticsText(joinDeadCompilation) + ")");

    const auto branchControlsProject = writeProject(root / "branch-controls", "BranchControls", deadFixture + R"NS(
class Case
{
    public static int Alive(bool flag)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Holder.Store(owner, refs);
        if (flag) { refs[0].Read(); } else { refs[0].Read(); }
        Holder.Drop(move(owner)); delete[] refs; return 0;
    }
    public static int Constant()
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Holder.Store(owner, refs);
        if (false) { Holder.Drop(move(owner)); refs[0].Read(); }
        delete[] refs; return 0;
    }
}
)NS", "StaticLibrary");
    ProjectCompilation branchControlsCompilation;
    std::string branchControlsError;
    branchControlsCompilation.compileProjectMethodBodies(branchControlsProject, branchControlsError);
    tests.expect(diagnosticsText(branchControlsCompilation).find("reference to a destroyed object") == std::string::npos,
        "branch controls never report alive or unsupported constant paths as dead (" +
            branchControlsError + diagnosticsText(branchControlsCompilation) + ")");

    const auto witnessedProject = writeProject(root / "dead-path-witness", "DeadPathWitness", deadFixture + R"NS(
class Case
{
    public static int Run(bool flag)
    {
        Resource owner = new Resource(40); Resource[] refs = new Resource[1];
        Holder.Store(owner, refs);
        if (flag) { Holder.Drop(move(owner)); }
        int value = refs[0].Read(); delete[] refs; return value;
    }
}
)NS", "StaticLibrary");
    ProjectCompilation witnessedCompilation;
    std::string witnessedError;
    const bool witnessedAccepted = witnessedCompilation.compileProjectMethodBodies(witnessedProject, witnessedError);
    tests.expect(witnessedAccepted && diagnosticsText(witnessedCompilation).find(
            "reference to a destroyed object") == std::string::npos,
        "possible destruction after a join remains undiagnosed without a complete path proof (" + witnessedError +
            diagnosticsText(witnessedCompilation) + ")");

    const auto returnControlProject = writeProject(root / "dead-path-return", "DeadPathReturn", deadFixture + R"NS(
class Case
{
    public static int Run(bool flag)
    {
        Resource owner = new Resource(40); Resource[] refs = new Resource[1];
        Holder.Store(owner, refs);
        if (flag) { Holder.Drop(move(owner)); delete[] refs; return 0; }
        int value = refs[0].Read(); delete[] refs; return value;
    }
}
)NS", "StaticLibrary");
    ProjectCompilation returnControlCompilation;
    std::string returnControlError;
    const bool returnControlAccepted = returnControlCompilation.compileProjectMethodBodies(
        returnControlProject, returnControlError);
    tests.expect(returnControlAccepted && diagnosticsText(returnControlCompilation).find(
            "reference to a destroyed object") == std::string::npos,
        "a destroyed path that returns cannot witness a later receiver (" + returnControlError +
            diagnosticsText(returnControlCompilation) + ")");

    const auto spinControlProject = writeProject(root / "dead-path-spin", "DeadPathSpin", deadFixture + R"NS(
class Spinner { public static void Spin() { while (true) { } } }
class Case
{
    public static int Run(bool flag)
    {
        Resource owner = new Resource(40); Resource[] refs = new Resource[1];
        Holder.Store(owner, refs);
        if (flag) { Holder.Drop(move(owner)); Spinner.Spin(); }
        int value = refs[0].Read(); delete[] refs; return value;
    }
}
)NS", "StaticLibrary");
    ProjectCompilation spinControlCompilation;
    std::string spinControlError;
    const bool spinControlAccepted = spinControlCompilation.compileProjectMethodBodies(
        spinControlProject, spinControlError);
    tests.expect(spinControlAccepted && diagnosticsText(spinControlCompilation).find(
            "reference to a destroyed object") == std::string::npos,
        "a non-returning call after destruction cannot witness a later read (" +
            spinControlError + diagnosticsText(spinControlCompilation) + ")");

    const auto destructorControlProject = writeProject(root / "dead-path-destructor-spin", "DeadPathDestructorSpin", R"NS(
class Resource { public int Read() { return 1; } ~Resource() { while (true) { } } }
class Holder {
    public static void Store(Resource value, Resource[] refs) { refs[0] = value; }
    public static void Drop(owned Resource value) { delete value; }
}
class Case {
    public static int Run(bool flag) {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Holder.Store(owner, refs);
        if (flag) { Holder.Drop(move(owner)); }
        int value = refs[0].Read(); delete[] refs; return value;
    }
}
)NS", "StaticLibrary");
    ProjectCompilation destructorControlCompilation;
    std::string destructorControlError;
    const bool destructorControlAccepted = destructorControlCompilation.compileProjectMethodBodies(
        destructorControlProject, destructorControlError);
    tests.expect(destructorControlAccepted && diagnosticsText(destructorControlCompilation).find(
            "reference to a destroyed object") == std::string::npos,
        "a potentially non-returning destructor cannot witness a later read (" +
            destructorControlError + diagnosticsText(destructorControlCompilation) + ")");

    const auto overrideControlProject = writeProject(root / "override-owner", "OverrideOwner", R"NS(
class Resource { public int Read() { return 1; } }
class Holder { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } }
class Base { public virtual void Dispose(owned Resource value) { delete value; } }
class Middle : Base { public override void Dispose(owned Resource value) { delete value; } }
class Derived : Middle {
    owned Resource saved;
    public override void Dispose(owned Resource value) { saved = move(value); }
}
class Case {
    public static int Run() {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Holder.Store(owner, refs);
        Middle target = new Derived(); target.Dispose(move(owner));
        int value = refs[0].Read(); delete[] refs; return value;
    }
}
)NS", "StaticLibrary");
    ProjectCompilation overrideControlCompilation;
    std::string overrideControlError;
    const bool overrideControlAccepted = overrideControlCompilation.compileProjectMethodBodies(
        overrideControlProject, overrideControlError);
    tests.expect(overrideControlAccepted && diagnosticsText(overrideControlCompilation).find(
            "reference to a destroyed object") == std::string::npos,
        "an override may preserve ownership through later dynamic dispatch (" +
            overrideControlError + diagnosticsText(overrideControlCompilation) + ")");

    {
        ArrayReferenceFlowState alive;
        alive.create(1, 1); alive.write({2, 0}, {1});
        auto dead = alive; dead.cleanup(1);
        auto joined = ArrayReferenceFlowState::join(dead, alive);
        tests.expect(joined.hasDeadPathWitness({2, 0}) &&
                !joined.hasDeadPathWitness({2, std::nullopt}),
            "a precise join witnesses only the exact slot");
        joined.write({2, 0}, {1});
        tests.expect(!joined.hasDeadPathWitness({2, 0}), "a later write invalidates the path witness");
        auto unknown = alive; unknown.markUnknown();
        tests.expect(!ArrayReferenceFlowState::join(dead, unknown).hasDeadPathWitness({2, 0}),
            "unknown paths never establish a witness");
        auto escaped = alive; escaped.escapedArrays.insert(2);
        tests.expect(!ArrayReferenceFlowState::join(dead, escaped).hasDeadPathWitness({2, 0}),
            "an escaped array cannot establish a witness");

        nativesharp::ast::AstNode construction(nativesharp::ast::AstNodeKind::ObjectCreationExpression, {});
        nativesharp::ast::AstNode condition(nativesharp::ast::AstNodeKind::IfStatement, {});
        nativesharp::ast::AstNode call(nativesharp::ast::AstNodeKind::CallExpression, {});
        auto base = alive;
        base.noteTrace(nativesharp::frontend::ArrayReferenceTraceKind::Construction, &construction);
        auto liveBranch = base;
        auto deadBranch = base;
        liveBranch.noteTrace(nativesharp::frontend::ArrayReferenceTraceKind::Branch, &condition, false);
        deadBranch.noteTrace(nativesharp::frontend::ArrayReferenceTraceKind::Branch, &condition, true);
        deadBranch.noteTrace(nativesharp::frontend::ArrayReferenceTraceKind::Call, &call);
        deadBranch.cleanup(1);
        deadBranch.noteTrace(nativesharp::frontend::ArrayReferenceTraceKind::Destruction, &call);
        auto observed = ArrayReferenceFlowState::join(deadBranch, liveBranch);
        const auto proof = observed.deadPathEvidence.find({2, 0});
        tests.expect(proof != observed.deadPathEvidence.end() && !proof->second.complete &&
                proof->second.events.size() == 4 &&
                proof->second.events[1].syntax == &condition && proof->second.events[1].branch == true,
            "opposite branch join retains observational dead-path events but never a complete proof");
        auto incompatible = liveBranch;
        incompatible.traceEvents[1].branch = true;
        tests.expect(ArrayReferenceFlowState::join(deadBranch, incompatible).deadPathEvidence.empty(),
            "incompatible paths do not yield provenance");
        observed.write({2, 0}, {1});
        tests.expect(observed.deadPathEvidence.empty(), "a write discards stale path provenance");
        auto bounded = alive;
        for (std::size_t i = 0; i < 257; ++i)
            bounded.noteTrace(nativesharp::frontend::ArrayReferenceTraceKind::Call, &call);
        tests.expect(bounded.traceUnknown && bounded.traceEvents.empty() && bounded.deadPathEvidence.empty(),
            "an oversized path is discarded as unknown rather than growing without limit");
    }

    const auto acceptedProject = writeProject(root / "dead-controls", "DeadControls", deadFixture + R"NS(
class Case
{
    public static int Alive()
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Holder.Store(owner, refs); int value = refs[0].Read();
        Holder.Drop(move(owner)); delete[] refs; return value;
    }
    public static int ForwardAlive()
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Holder.Store(owner, refs); raii Resource next = Holder.Forward(move(owner));
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int Compare()
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Holder.Store(owner, refs); Holder.Drop(move(owner)); Holder.Clear(refs);
        bool value = refs[0] == null; delete[] refs; return 0;
    }
    public static int Unknown(bool drop)
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Holder.Store(owner, refs); Holder.MaybeDropUnproven(move(owner), drop);
        int value = refs[0].Read(); delete[] refs; return value;
    }
    public static int Unreachable()
    {
        Resource owner = new Resource(); Resource[] refs = new Resource[1];
        Holder.Store(owner, refs); Holder.Drop(move(owner)); return 0;
        int value = refs[0].Read();
    }
}
)NS", "StaticLibrary");
    ProjectCompilation acceptedCompilation;
    std::string acceptedError;
    const bool controlsAccepted = acceptedCompilation.compileProjectMethodBodies(acceptedProject, acceptedError);
    tests.expect(controlsAccepted, "public analysis accepts alive, forwarded, comparison, unknown and unreachable controls (" +
        acceptedError + diagnosticsText(acceptedCompilation) + ")");

    // Optional external fixture: the caller supplies a copy of the historical
    // project, so this test never edits the original reproduction.
    if (const auto originalPath = environmentValue("NSC_ARRAY_REPLAY_PROJECT"))
    {
        ProjectCompilation original;
        std::string originalError;
        nativesharp::compilation::ProjectLoader loader;
        nativesharp::compilation::ProjectLoadOptions loadOptions;
        auto project = loader.load(*originalPath, loadOptions, originalError);
        std::vector<nativesharp::compilation::Project> projects;
        if (project.has_value()) projects.push_back(std::move(*project));
        const bool originalPrepared = project.has_value() &&
            nativesharp::compilation::preparePrecompiledProject(
                nativesharp::compilation::ProjectGraph(*originalPath, std::move(projects)),
                loadOptions, original, originalError);
        const bool originalInterfaces = originalPrepared &&
            original.compileProjectInterfaces(*originalPath, originalError);
        const bool originalReady = originalInterfaces && original.buildGenericConstraintModel();
        tests.expect(originalReady, "original array-reference project loads its models (" +
            originalError + diagnosticsText(original) + ")");
        nativesharp::frontend::ProjectMethodBodyModel originalReplay;
        nativesharp::diagnostics::DiagnosticEngine originalDiagnostics;
        const bool originalReplayed = originalReady && original.nameLookup() != nullptr &&
            original.memberModel() != nullptr && original.namespaceFunctionModel() != nullptr &&
            original.inheritanceModel() != nullptr && original.interfaceModel() != nullptr &&
            originalReplay.build(*original.nameLookup(), *original.memberModel(),
                *original.namespaceFunctionModel(), *original.inheritanceModel(),
                *original.interfaceModel(), original.genericModel(), originalDiagnostics,
                original.genericConstraintModel(), std::nullopt, replayOptions);
        tests.expect(originalReplayed, "original project supports isolated experimental replay");
        const auto originalModeled = [&](std::string_view name)
        {
            if (original.memberModel() == nullptr || !originalReplayed)
                return false;
            const auto members = original.memberModel()->members();
            for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
                if (members[ordinal].name == name)
                    return originalReplay.arrayReferenceHasOnlyModeledPublication(ordinal);
            return false;
        };
        tests.expect(originalModeled("SetValue") && originalModeled("GetValue") &&
                originalModeled("StoreReference"),
            "original scalar receiver methods and direct store have modeled publication");
        bool originalConstructionPrivate = false;
        bool originalTypeFound = false;
        if (original.nameLookup() != nullptr && originalReplayed)
        {
            const auto types = original.nameLookup()->namedTypes();
            for (std::size_t ordinal = 0; ordinal < types.size(); ++ordinal)
                if (types[ordinal].identity.name() == "MyClass")
                {
                    originalTypeFound = true;
                    originalConstructionPrivate =
                        originalReplay.arrayReferencePrivateDefaultConstruction(ordinal);
                }
        }
        tests.expect(originalTypeFound && !originalConstructionPrivate,
            "original construction stays unknown with a field or precompiled base");
        std::optional<std::size_t> mainOrdinal;
        if (original.memberModel() != nullptr)
        {
            const auto members = original.memberModel()->members();
            for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
                if (members[ordinal].name == "Main") mainOrdinal = ordinal;
        }
        std::vector<ArrayReferenceRead> originalReads;
        std::vector<bool> originalReceiverFlags;
        for (const auto& observation : originalReplay.arrayReferenceReplayObservations())
            if (mainOrdinal.has_value() && observation.callableOrdinal == *mainOrdinal)
            {
                originalReads.push_back(observation.state);
                originalReceiverFlags.push_back(observation.instanceMethodReceiver);
            }
        tests.expect(mainOrdinal.has_value() &&
                originalReplay.arrayReferenceLinearPublicationCoverage(*mainOrdinal),
            "the original caller satisfies the experimental linear coverage gate");
        tests.expect(originalReads.size() == 2 && originalReads[0] == ArrayReferenceRead::Unknown &&
                originalReads[1] == ArrayReferenceRead::Dead,
            "original replay separates unknown lifetime before deletion from dead identity after it");
        tests.expect(originalReceiverFlags.size() == 2 && originalReceiverFlags[0] &&
                originalReceiverFlags[1],
            "both original reads are instance method receivers");
        const std::array<std::string_view, 3> originalArguments = {
            "--dump-project-method-bodies", *originalPath, "-O0"};
        std::string originalOutput;
        std::string originalDriverError;
        const auto originalResult = runDriver(originalArguments, originalOutput, originalDriverError);
        tests.expect(originalResult != nativesharp::ExitCode::Success &&
                (originalDriverError + originalOutput).find("reference to a destroyed object") !=
                    std::string::npos,
            "public method-body analysis rejects the original dead method receiver");
    }

    const std::string producerSource = R"NS(
public delegate int Reader();
namespace EscapeContracts
{
    public class RefSlotHolder { public ref int* Slot; }
    public interface OutputSink
    {
        void Write(out long value);
    }
    public class EscapeApi
    {
        public static int* Stored;
        public static int** StoredSlot;
        public static RefSlotHolder StoredHolder;
        public static int Read(int* value) { return *value; }
        public static void ReadRef(ref int* value) { }
        public static void RetainRefValue(ref int* value) { Retain(value); }
        public static void RetainRefSlot(ref int* value) { StoredSlot = &value; }
        public static void ForwardRefSlot(ref int* value) { RetainRefValue(ref value); }
        public static ref int* ReturnRefSlot(ref int* value) { return ref value; }
        public static void StoreRefField(ref int* value) { StoredHolder.Slot = ref value; }
        public static int* Identity(int* value) { return value; }
        public static int* ReturnPointerLocal(out long size) { size = 0L; int* local = Stored; return local; }
        public static void Retain(int* value) { Stored = value; }
        public static void RetainWithScalarOffset(out int index, int* value) {
            index = 1; Retain(value + index);
        }
        public static void RetainOutAddress(out int index) {
            index = 1; Retain(&index);
        }
        public static void Forward(int* value) { Retain(value); }
        public static void ExternalSpin() { while (true) { } }
        public static Reader Capture(int* value)
        {
            return () => { unsafe { return *value; } };
        }
    }
    public void RetainFunction(int* value) { EscapeApi.Stored = value; }
}
)NS";
    ProjectCompilation producer;
    std::string error;
    const std::filesystem::path producerProject = writeProject(
        root / "producer", "EscapeProducer", producerSource, "StaticLibrary");
    const bool producerCompiled = producer.compileProjectOopMir(producerProject, error);
    tests.expect(
        producerCompiled && !producer.diagnostics().hasErrors() &&
            summaryFor(producer, "Read") == std::vector<EscapeClass>{EscapeClass::NoEscape} &&
            summaryFor(producer, "Identity") == std::vector<EscapeClass>{EscapeClass::Return} &&
            summaryFor(producer, "ReturnPointerLocal") == std::vector<EscapeClass>{EscapeClass::NoEscape} &&
            summaryFor(producer, "RetainWithScalarOffset") ==
                std::vector<EscapeClass>{EscapeClass::NoEscape, EscapeClass::Global} &&
            summaryFor(producer, "RetainOutAddress") == std::vector<EscapeClass>{EscapeClass::Global} &&
            summaryFor(producer, "Retain") == std::vector<EscapeClass>{EscapeClass::Global} &&
            summaryFor(producer, "Forward") == std::vector<EscapeClass>{EscapeClass::Global} &&
            summaryFor(producer, "Capture") == std::vector<EscapeClass>{EscapeClass::Global},
        "body analysis derives noescape, return, global, transitive and closure summaries (" +
            error + diagnosticsText(producer) + ")");

    nativesharp::interop::LibraryMetadataCodec codec;
    std::string metadataError;
    const std::filesystem::path metadataPath = root / "EscapeProducer.nsmeta";
    const std::filesystem::path producerLibrary = root / "EscapeProducer.lib";
    const std::string producerProjectText = producerProject.string();
    const std::string producerLibraryText = producerLibrary.string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProjectText, "-O0", "-o", producerLibraryText};
    std::string driverOutput;
    std::string driverError;
    const bool metadataWritten = producerCompiled &&
        runDriver(producerArguments, driverOutput, driverError) == nativesharp::ExitCode::Success;
    nativesharp::interop::LibraryMetadata roundTrip;
    const bool metadataRead = metadataWritten &&
        codec.read(metadataPath, roundTrip, metadataError);
    const auto retain = std::ranges::find_if(roundTrip.members, [](const auto& member)
    {
        return member.name == "Retain";
    });
    tests.expect(
        metadataRead &&
            retain != roundTrip.members.end() &&
            retain->parameterEscapeSummary ==
                std::vector<nativesharp::interop::LibraryMetadataEscapeClass>{
                    nativesharp::interop::LibraryMetadataEscapeClass::Global},
        "NSMETA round-trips the public escape contract (" + driverError + driverOutput +
            metadataError + ")");
    const auto offset = std::ranges::find_if(roundTrip.members, [](const auto& member)
    {
        return member.name == "RetainWithScalarOffset";
    });
    const auto outAddress = std::ranges::find_if(roundTrip.members, [](const auto& member)
    {
        return member.name == "RetainOutAddress";
    });
    tests.expect(metadataRead && offset != roundTrip.members.end() && outAddress != roundTrip.members.end() &&
            offset->parameterEscapeSummary == std::vector<nativesharp::interop::LibraryMetadataEscapeClass>{
                nativesharp::interop::LibraryMetadataEscapeClass::NoEscape,
                nativesharp::interop::LibraryMetadataEscapeClass::Global} &&
            outAddress->parameterEscapeSummary == std::vector<nativesharp::interop::LibraryMetadataEscapeClass>{
                nativesharp::interop::LibraryMetadataEscapeClass::Global},
        "metadata distinguishes a scalar offset from a retained out-parameter address");
    const auto retainRefValue = std::ranges::find_if(roundTrip.members, [](const auto& member)
        { return member.name == "RetainRefValue"; });
    const auto retainRefSlot = std::ranges::find_if(roundTrip.members, [](const auto& member)
        { return member.name == "RetainRefSlot"; });
    const auto forwardRefSlot = std::ranges::find_if(roundTrip.members, [](const auto& member)
        { return member.name == "ForwardRefSlot"; });
    const auto returnRefSlot = std::ranges::find_if(roundTrip.members, [](const auto& member)
        { return member.name == "ReturnRefSlot"; });
    const auto storeRefField = std::ranges::find_if(roundTrip.members, [](const auto& member)
        { return member.name == "StoreRefField"; });
    tests.expect(metadataRead && retainRefValue != roundTrip.members.end() &&
            retainRefSlot != roundTrip.members.end() && forwardRefSlot != roundTrip.members.end() &&
            returnRefSlot != roundTrip.members.end() && storeRefField != roundTrip.members.end() &&
            retainRefValue->parameterEscapeSummary == std::vector<nativesharp::interop::LibraryMetadataEscapeClass>{
                nativesharp::interop::LibraryMetadataEscapeClass::RefSlotNoEscape} &&
            retainRefSlot->parameterEscapeSummary != std::vector<nativesharp::interop::LibraryMetadataEscapeClass>{
                nativesharp::interop::LibraryMetadataEscapeClass::RefSlotNoEscape} &&
            forwardRefSlot->parameterEscapeSummary != std::vector<nativesharp::interop::LibraryMetadataEscapeClass>{
                nativesharp::interop::LibraryMetadataEscapeClass::RefSlotNoEscape} &&
            returnRefSlot->parameterEscapeSummary != std::vector<nativesharp::interop::LibraryMetadataEscapeClass>{
                nativesharp::interop::LibraryMetadataEscapeClass::RefSlotNoEscape} &&
            storeRefField->parameterEscapeSummary != std::vector<nativesharp::interop::LibraryMetadataEscapeClass>{
                nativesharp::interop::LibraryMetadataEscapeClass::RefSlotNoEscape},
        "producer proves only the slot that is not retained (" + driverError + driverOutput + metadataError + ")");

    nativesharp::interop::LibraryMetadata changed = roundTrip;
    const auto changedRead = std::ranges::find_if(changed.members, [](const auto& member)
    {
        return member.name == "Read";
    });
    if (changedRead != changed.members.end())
        changedRead->parameterEscapeSummary = {
            nativesharp::interop::LibraryMetadataEscapeClass::Global};
    const std::filesystem::path changedPath = root / "EscapeProducer.changed.nsmeta";
    nativesharp::interop::LibraryMetadata changedRoundTrip;
    const bool changedWritten = metadataRead &&
        codec.writeModel(changed, changedPath, metadataError) &&
        codec.read(changedPath, changedRoundTrip, metadataError);
    tests.expect(changedWritten &&
            changedRoundTrip.abiFingerprint != roundTrip.abiFingerprint,
        "changing an escape contract invalidates the public metadata fingerprint (" +
            metadataError + ")");

    nativesharp::interop::LibraryMetadata missingSummaries = roundTrip;
    // A historical label is observational only; omitted summaries remain unknown.
    for (auto& member : missingSummaries.members) member.parameterEscapeSummary.clear();
    for (auto& function : missingSummaries.namespaceFunctions)
        function.parameterEscapeSummary.clear();
    const std::filesystem::path missingPath = root / "EscapeProducer.missing-summaries.nsmeta";
    nativesharp::interop::LibraryMetadata missingRoundTrip;
    const bool missingRead = codec.writeModel(missingSummaries, missingPath, metadataError) &&
        codec.read(missingPath, missingRoundTrip, metadataError);
    const auto summariesUnknown = [](const auto& callable)
    {
        return callable.parameterEscapeSummary.size() == callable.parameters.size() &&
            std::ranges::all_of(callable.parameterEscapeSummary, [](const auto value)
            {
                return value == nativesharp::interop::LibraryMetadataEscapeClass::Unknown;
            });
    };
    tests.expect(missingRead && std::ranges::all_of(missingRoundTrip.members, summariesUnknown) &&
            std::ranges::all_of(missingRoundTrip.namespaceFunctions, summariesUnknown),
        "missing escape summaries become conservative unknown contracts (" +
            metadataError + ")");
    nativesharp::interop::LibraryMetadataCodec labelCodec;
    tests.expect(missingRead && nativesharp::tests::metadataReservedBytesPreserveContent(
            labelCodec, missingSummaries, missingPath, metadataError),
        "the historical label alone changes no written byte (" + metadataError + ")");

    nativesharp::interop::LibraryMetadata corrupt = roundTrip;
    if (auto member = std::ranges::find_if(corrupt.members, [](const auto& candidate)
        { return candidate.name == "Read"; }); member != corrupt.members.end())
    {
        member->parameterEscapeSummary.push_back(
            nativesharp::interop::LibraryMetadataEscapeClass::Unknown);
    }
    std::string corruptError;
    tests.expect(!codec.writeModel(corrupt, root / "corrupt.nsmeta", corruptError) &&
            corruptError.find("escape-summary parameter count") != std::string::npos,
        "a corrupt escape catalogue fails closed (" + corruptError + ")");

    constexpr std::string_view producerReference =
        "<LibraryReference Include=\"../EscapeProducer.lib\" />";

    // An interface has no implementation to inspect when its library is built.
    // In particular, an out parameter must not silently acquire a no-escape contract.
    const std::filesystem::path unknownOutProject = writeProject(
        root / "unknown-interface-out", "EscapeUnknownInterfaceOut", R"NS(
using EscapeContracts;
public class Program {
    public static void Send(OutputSink sink) { long local = 0L; sink.Write(out local); }
    public static int Main() { return 0; }
}
)NS", "Exe", producerReference);
    const std::string unknownOutProjectText = unknownOutProject.string();
    const std::array<std::string_view, 3> unknownOutArguments = {
        "--dump-project-method-bodies", unknownOutProjectText, "-O0"};
    std::string unknownOutOutput;
    std::string unknownOutError;
    const auto unknownOutResult = runDriver(unknownOutArguments, unknownOutOutput, unknownOutError);
    tests.expect(metadataWritten && unknownOutResult != nativesharp::ExitCode::Success &&
            (unknownOutError + unknownOutOutput).find("unknown escape contract") != std::string::npos,
        "an unknown precompiled interface out contract rejects current-frame storage (" +
            unknownOutError + unknownOutOutput + ")");

    const auto genericProducer = writeProject(root / "generic-out-producer", "GenericOutProducer", R"NS(
namespace GenericOut {
    public static class Factory {
        public static T* Make<T>(out long size) { size = 0L; return null; }
        public static T* Outer<T>(out long size) { return Make<T>(out size); }
        public static void Force() { long n = 0L; ulong* p = Make<ulong>(out n); }
    }
}
)NS", "StaticLibrary");
    const std::string genericProducerText = genericProducer.string();
    const std::string genericLibraryText = (root / "GenericOutProducer.lib").string();
    const std::array<std::string_view, 5> genericProducerArgs = {
        "--emit-project-oop-lib", genericProducerText, "-O0", "-o", genericLibraryText};
    std::string genericProducerOutput, genericProducerError;
    const bool genericProduced = runDriver(genericProducerArgs, genericProducerOutput, genericProducerError) ==
        nativesharp::ExitCode::Success;
    const auto genericConsumer = writeProject(root / "generic-out-consumer", "GenericOutConsumer", R"NS(
using GenericOut;
public class Program { public static int Main() {
    long n = 0L; ulong* p = Factory.Outer<ulong>(out n); return 0;
} }
)NS", "Exe", "<LibraryReference Include=\"../GenericOutProducer.lib\" />");
    const std::string genericConsumerText = genericConsumer.string();
    const std::array<std::string_view, 3> genericConsumerArgs = {
        "--dump-project-method-bodies", genericConsumerText, "-O0"};
    std::string genericConsumerOutput, genericConsumerError;
    const auto genericResult = runDriver(genericConsumerArgs, genericConsumerOutput, genericConsumerError);
    tests.expect(genericProduced && genericResult == nativesharp::ExitCode::Success,
        "a proven precompiled generic specialization preserves its out escape summary (" +
            genericProducerError + genericProducerOutput + genericConsumerError + genericConsumerOutput + ")");

    const auto unsafeGenericProducer = writeProject(root / "unsafe-generic-out-producer", "UnsafeGenericOutProducer", R"NS(
namespace UnsafeGenericOut {
    public static class Factory {
        public static long* saved;
        public static T* Make<T>(out long size) { size = 0L; unsafe { saved = &size; } return null; }
        public static void Force(out long size) { ulong* p = Make<ulong>(out size); }
    }
}
)NS", "StaticLibrary");
    const std::string unsafeGenericProducerText = unsafeGenericProducer.string();
    const std::string unsafeGenericLibraryText = (root / "UnsafeGenericOutProducer.lib").string();
    const std::array<std::string_view, 5> unsafeGenericProducerArgs = {
        "--emit-project-oop-lib", unsafeGenericProducerText, "-O0", "-o", unsafeGenericLibraryText};
    std::string unsafeGenericProducerOutput, unsafeGenericProducerError;
    const bool unsafeGenericProduced = runDriver(unsafeGenericProducerArgs, unsafeGenericProducerOutput,
        unsafeGenericProducerError) == nativesharp::ExitCode::Success;
    const auto unsafeGenericConsumer = writeProject(root / "unsafe-generic-out-consumer", "UnsafeGenericOutConsumer", R"NS(
using UnsafeGenericOut;
public class Program { public static int Main() {
    long n = 0L; ulong* p = Factory.Make<ulong>(out n); return 0;
} }
)NS", "Exe", "<LibraryReference Include=\"../UnsafeGenericOutProducer.lib\" />");
    const std::string unsafeGenericConsumerText = unsafeGenericConsumer.string();
    const std::array<std::string_view, 3> unsafeGenericConsumerArgs = {
        "--dump-project-method-bodies", unsafeGenericConsumerText, "-O0"};
    std::string unsafeGenericConsumerOutput, unsafeGenericConsumerError;
    const auto unsafeGenericResult = runDriver(unsafeGenericConsumerArgs, unsafeGenericConsumerOutput,
        unsafeGenericConsumerError);
    tests.expect(unsafeGenericProduced && unsafeGenericResult != nativesharp::ExitCode::Success &&
            (unsafeGenericConsumerError + unsafeGenericConsumerOutput).find("published escape contract") != std::string::npos,
        "a precompiled generic specialization cannot retain an out local (" +
            unsafeGenericProducerError + unsafeGenericProducerOutput + unsafeGenericConsumerError +
            unsafeGenericConsumerOutput + ")");

    const auto hiddenGenericProducer = writeProject(root / "hidden-generic-out-producer", "HiddenGenericOutProducer", R"NS(
namespace HiddenGenericOut {
    public static class Factory {
        public static long* saved;
        public static T* Make<T>(out long size) {
            size = 0L; unsafe { nuint bits = (nuint)&size; saved = (long*)bits; } return null;
        }
        public static void Force(out long size) { ulong* p = Make<ulong>(out size); }
    }
}
)NS", "StaticLibrary");
    const std::string hiddenProducerText = hiddenGenericProducer.string();
    const std::string hiddenLibraryText = (root / "HiddenGenericOutProducer.lib").string();
    const std::array<std::string_view, 5> hiddenProducerArgs = {
        "--emit-project-oop-lib", hiddenProducerText, "-O0", "-o", hiddenLibraryText};
    std::string hiddenProducerOutput, hiddenProducerError;
    const bool hiddenProduced = runDriver(hiddenProducerArgs, hiddenProducerOutput, hiddenProducerError) ==
        nativesharp::ExitCode::Success;
    const auto hiddenConsumer = writeProject(root / "hidden-generic-out-consumer", "HiddenGenericOutConsumer", R"NS(
using HiddenGenericOut;
public class Program { public static int Main() {
    long n = 0L; ulong* p = Factory.Make<ulong>(out n); return 0;
} }
)NS", "Exe", "<LibraryReference Include=\"../HiddenGenericOutProducer.lib\" />");
    const std::string hiddenConsumerText = hiddenConsumer.string();
    const std::array<std::string_view, 3> hiddenConsumerArgs = {
        "--dump-project-method-bodies", hiddenConsumerText, "-O0"};
    std::string hiddenConsumerOutput, hiddenConsumerError;
    const auto hiddenResult = runDriver(hiddenConsumerArgs, hiddenConsumerOutput, hiddenConsumerError);
    tests.expect(hiddenProduced && hiddenResult != nativesharp::ExitCode::Success &&
            (hiddenConsumerError + hiddenConsumerOutput).find("unknown escape contract") != std::string::npos,
        "an address hidden by an integer cast cannot certify a generic out parameter (" +
            hiddenProducerError + hiddenProducerOutput + hiddenConsumerError + hiddenConsumerOutput + ")");

    const auto noescapeProducer = writeProject(root / "noescape-producer", "NoescapeProducer", R"NS(
namespace SafeOutput { public interface Sink { void Write(out long value noescape); } }
)NS", "StaticLibrary");
    const std::string noescapeProjectText = noescapeProducer.string();
    const std::string noescapeLibraryText = (root / "NoescapeProducer.lib").string();
    const std::array<std::string_view, 5> noescapeProducerArguments = {
        "--emit-project-oop-lib", noescapeProjectText, "-O0", "-o", noescapeLibraryText};
    std::string noescapeProducerOutput;
    std::string noescapeProducerError;
    const bool noescapeProduced = runDriver(noescapeProducerArguments, noescapeProducerOutput,
        noescapeProducerError) == nativesharp::ExitCode::Success;
    constexpr std::string_view noescapeReference =
        "<LibraryReference Include=\"../NoescapeProducer.lib\" />";
    const auto safeOutProject = writeProject(root / "noescape-safe", "NoescapeSafe", R"NS(
using SafeOutput;
public class Good : Sink { public void Write(out long value) { value = 42L; } }
public class Program {
    public static void Send(Sink sink) { long local = 0L; sink.Write(out local); }
    public static int Main() { return 0; }
}
)NS", "Exe", noescapeReference);
    const std::string safeOutProjectText = safeOutProject.string();
    const std::array<std::string_view, 3> safeOutArguments = {
        "--dump-project-method-bodies", safeOutProjectText, "-O0"};
    std::string safeOutOutput;
    std::string safeOutError;
    const auto safeOutResult = runDriver(safeOutArguments, safeOutOutput, safeOutError);
    tests.expect(noescapeProduced && safeOutResult == nativesharp::ExitCode::Success,
        "a proven interface out contract accepts frame storage (" + noescapeProducerError +
            noescapeProducerOutput + safeOutError + safeOutOutput + ")");

    const auto certifiedProject = writeProject(root / "noescape-certified", "NoescapeCertified", R"NS(
using SafeOutput;
public class Good : Sink { public void Write(out long value) { value = 42L; } }
)NS", "StaticLibrary", noescapeReference);
    const std::string certifiedProjectText = certifiedProject.string();
    const std::string certifiedLibraryText = (root / "NoescapeCertified.lib").string();
    const std::array<std::string_view, 5> certifiedArguments = {
        "--emit-project-oop-lib", certifiedProjectText, "-O0", "-o", certifiedLibraryText};
    std::string certifiedOutput;
    std::string certifiedError;
    const bool certifiedProduced = runDriver(certifiedArguments, certifiedOutput, certifiedError) ==
        nativesharp::ExitCode::Success;
    nativesharp::interop::LibraryMetadata certifiedMetadata;
    const bool certifiedRead = certifiedProduced && codec.read(root / "NoescapeCertified.nsmeta",
        certifiedMetadata, metadataError);
    const auto certifiedWrite = std::ranges::find_if(certifiedMetadata.members, [](const auto& member)
    {
        return member.name == "Write" &&
            (member.flags & nativesharp::interop::kMetadataMemberVerifiedNoEscapeOut) != 0U;
    });
    tests.expect(noescapeProduced && certifiedRead && certifiedWrite != certifiedMetadata.members.end(),
        "a checked implementation publishes its noescape certificate (" +
            certifiedError + certifiedOutput + metadataError + ")");
    auto inconsistentCertificate = certifiedMetadata;
    const auto inconsistentWrite = std::ranges::find_if(inconsistentCertificate.members, [](const auto& member)
    {
        return member.name == "Write" &&
            (member.flags & nativesharp::interop::kMetadataMemberVerifiedNoEscapeOut) != 0U;
    });
    if (inconsistentWrite != inconsistentCertificate.members.end())
        for (auto& summary : inconsistentWrite->parameterEscapeSummary)
            summary = nativesharp::interop::LibraryMetadataEscapeClass::Unknown;
    std::string inconsistentCertificateError;
    tests.expect(certifiedRead && inconsistentWrite != inconsistentCertificate.members.end() &&
            !codec.verifyCurrentCompiler(inconsistentCertificate, inconsistentCertificateError) &&
            inconsistentCertificateError.find("verified no-escape out contract") != std::string::npos,
        "a certificate with no noescape out summary is rejected (" + inconsistentCertificateError + ")");

    const auto certifiedConsumer = writeProject(root / "noescape-certified-consumer", "NoescapeCertifiedConsumer", R"NS(
using SafeOutput;
public class Program { public static int Main() { return 0; } }
)NS", "Exe", "<LibraryReference Include=\"../NoescapeProducer.lib\" />"
               "<LibraryReference Include=\"../NoescapeCertified.lib\" />");
    const std::string certifiedConsumerText = certifiedConsumer.string();
    const std::array<std::string_view, 3> certifiedConsumerArguments = {
        "--dump-project-method-bodies", certifiedConsumerText, "-O0"};
    std::string certifiedConsumerOutput;
    std::string certifiedConsumerError;
    const auto certifiedConsumerResult = runDriver(
        certifiedConsumerArguments, certifiedConsumerOutput, certifiedConsumerError);
    tests.expect(certifiedRead && certifiedConsumerResult == nativesharp::ExitCode::Success,
        "a precompiled checked implementation satisfies its interface (" +
            certifiedConsumerError + certifiedConsumerOutput + ")");

    const auto legacyBaseProject = writeProject(root / "noescape-legacy-base", "NoescapeLegacyBase", R"NS(
namespace LegacyOutput {
    public class Base {
        public static long* saved;
        public void Write(out long value) {
            value = 1L;
            unsafe { nuint bits = (nuint)&value; saved = (long*)bits; }
        }
    }
}
)NS", "StaticLibrary");
    const std::string legacyBaseProjectText = legacyBaseProject.string();
    const std::string legacyBaseLibraryText = (root / "NoescapeLegacyBase.lib").string();
    const std::array<std::string_view, 5> legacyBaseArguments = {
        "--emit-project-oop-lib", legacyBaseProjectText, "-O0", "-o", legacyBaseLibraryText};
    std::string legacyBaseOutput;
    std::string legacyBaseError;
    const bool legacyBaseProduced = runDriver(legacyBaseArguments, legacyBaseOutput,
        legacyBaseError) == nativesharp::ExitCode::Success;
    const auto inheritedUnsafeProject = writeProject(root / "noescape-inherited-unsafe", "NoescapeInheritedUnsafe", R"NS(
using SafeOutput;
using LegacyOutput;
public class Bad : Base, Sink { }
public class Program { public static int Main() { return 0; } }
)NS", "Exe", "<LibraryReference Include=\"../NoescapeProducer.lib\" />"
               "<LibraryReference Include=\"../NoescapeLegacyBase.lib\" />");
    const std::string inheritedUnsafeProjectText = inheritedUnsafeProject.string();
    const std::array<std::string_view, 3> inheritedUnsafeArguments = {
        "--dump-project-method-bodies", inheritedUnsafeProjectText, "-O0"};
    std::string inheritedUnsafeOutput;
    std::string inheritedUnsafeError;
    const auto inheritedUnsafeResult = runDriver(
        inheritedUnsafeArguments, inheritedUnsafeOutput, inheritedUnsafeError);
    tests.expect(noescapeProduced && legacyBaseProduced && inheritedUnsafeResult != nativesharp::ExitCode::Success &&
            (inheritedUnsafeError + inheritedUnsafeOutput).find("non-escaping contract") != std::string::npos,
        "an inherited precompiled implementation needs a verified noescape certificate (" +
            legacyBaseError + legacyBaseOutput + inheritedUnsafeError + inheritedUnsafeOutput + ")");

    const auto unsafeOutProject = writeProject(root / "noescape-unsafe", "NoescapeUnsafe", R"NS(
using SafeOutput;
public class Bad : Sink {
    public static long* saved;
    public void Write(out long value) { value = 1L; unsafe { saved = &value; } }
}
public class Program { public static int Main() { return 0; } }
)NS", "Exe", noescapeReference);
    const std::string unsafeOutProjectText = unsafeOutProject.string();
    const std::array<std::string_view, 3> unsafeOutArguments = {
        "--dump-project-method-bodies", unsafeOutProjectText, "-O0"};
    std::string unsafeOutOutput;
    std::string unsafeOutError;
    const auto unsafeOutResult = runDriver(unsafeOutArguments, unsafeOutOutput, unsafeOutError);
    tests.expect(noescapeProduced && unsafeOutResult != nativesharp::ExitCode::Success &&
            (unsafeOutError + unsafeOutOutput).find("non-escaping contract") != std::string::npos,
        "an interface implementation cannot retain a noescape out parameter (" +
            unsafeOutError + unsafeOutOutput + ")");

    const auto sourceUnsafeProject = writeProject(root / "noescape-source-unsafe", "NoescapeSourceUnsafe", R"NS(
public interface Sink { void Write(out long value noescape); }
public class Bad : Sink {
    public static long* saved;
    public void Write(out long value) { value = 1L; unsafe { saved = &value; } }
}
public class Program { public static int Main() { return 0; } }
)NS", "Exe");
    const std::string sourceUnsafeProjectText = sourceUnsafeProject.string();
    const std::array<std::string_view, 3> sourceUnsafeArguments = {
        "--dump-project-method-bodies", sourceUnsafeProjectText, "-O0"};
    std::string sourceUnsafeOutput;
    std::string sourceUnsafeError;
    const auto sourceUnsafeResult = runDriver(sourceUnsafeArguments, sourceUnsafeOutput, sourceUnsafeError);
    tests.expect(sourceUnsafeResult != nativesharp::ExitCode::Success &&
            (sourceUnsafeError + sourceUnsafeOutput).find("non-escaping contract") != std::string::npos,
        "a source interface rejects an implementation that retains its out parameter (" +
            sourceUnsafeError + sourceUnsafeOutput + ")");

    const auto forwardedUnknownProject = writeProject(root / "noescape-forward-unknown", "NoescapeForwardUnknown", R"NS(
using SafeOutput;
using EscapeContracts;
public class Bad : Sink {
    public OutputSink other;
    public void Write(out long value) { other.Write(out value); }
}
public class Program { public static int Main() { return 0; } }
)NS", "Exe", "<LibraryReference Include=\"../NoescapeProducer.lib\" />"
               "<LibraryReference Include=\"../EscapeProducer.lib\" />");
    const std::string forwardedUnknownProjectText = forwardedUnknownProject.string();
    const std::array<std::string_view, 3> forwardedUnknownArguments = {
        "--dump-project-method-bodies", forwardedUnknownProjectText, "-O0"};
    std::string forwardedUnknownOutput;
    std::string forwardedUnknownError;
    const auto forwardedUnknownResult = runDriver(
        forwardedUnknownArguments, forwardedUnknownOutput, forwardedUnknownError);
    tests.expect(noescapeProduced && metadataWritten && forwardedUnknownResult != nativesharp::ExitCode::Success &&
            (forwardedUnknownError + forwardedUnknownOutput).find("non-escaping contract") != std::string::npos,
        "a noescape implementation cannot forward to an unknown out contract (" +
            forwardedUnknownError + forwardedUnknownOutput + ")");

    const auto misplacedContractProject = writeProject(root / "noescape-misplaced", "NoescapeMisplaced", R"NS(
public class Program {
    public static void Write(out long value noescape) { value = 0L; }
    public static int Main() { return 0; }
}
)NS", "Exe");
    const std::string misplacedContractProjectText = misplacedContractProject.string();
    const std::array<std::string_view, 3> misplacedContractArguments = {
        "--dump-project-method-bodies", misplacedContractProjectText, "-O0"};
    std::string misplacedContractOutput;
    std::string misplacedContractError;
    const auto misplacedContractResult = runDriver(
        misplacedContractArguments, misplacedContractOutput, misplacedContractError);
    tests.expect(misplacedContractResult != nativesharp::ExitCode::Success &&
            (misplacedContractError + misplacedContractOutput).find("abstract interface method") != std::string::npos,
        "noescape cannot be asserted on an ordinary method (" +
            misplacedContractError + misplacedContractOutput + ")");

    const auto aliasUnsafeProject = writeProject(root / "noescape-alias-unsafe", "NoescapeAliasUnsafe", R"NS(
public interface Sink { void Write(out long value noescape); }
public class Bad : Sink {
    public static long* saved;
    public void Write(out long value) {
        value = 1L;
        unsafe { long* alias = &value; saved = alias; }
    }
}
public class Program { public static int Main() { return 0; } }
)NS", "Exe");
    const std::string aliasUnsafeProjectText = aliasUnsafeProject.string();
    const std::array<std::string_view, 3> aliasUnsafeArguments = {
        "--dump-project-method-bodies", aliasUnsafeProjectText, "-O0"};
    std::string aliasUnsafeOutput;
    std::string aliasUnsafeError;
    const auto aliasUnsafeResult = runDriver(aliasUnsafeArguments, aliasUnsafeOutput, aliasUnsafeError);
    tests.expect(aliasUnsafeResult != nativesharp::ExitCode::Success &&
            (aliasUnsafeError + aliasUnsafeOutput).find("non-escaping contract") != std::string::npos,
        "a noescape out contract also rejects a retained pointer alias (" +
            aliasUnsafeError + aliasUnsafeOutput + ")");

    const auto integerUnsafeProject = writeProject(root / "noescape-integer-unsafe", "NoescapeIntegerUnsafe", R"NS(
public interface Sink { void Write(out long value noescape); }
public class Bad : Sink {
    public static long* saved;
    public void Write(out long value) {
        value = 1L;
        unsafe { nuint bits = (nuint)&value; saved = (long*)bits; }
    }
}
public class Program { public static int Main() { return 0; } }
)NS", "Exe");
    const std::string integerUnsafeProjectText = integerUnsafeProject.string();
    const std::array<std::string_view, 3> integerUnsafeArguments = {
        "--dump-project-method-bodies", integerUnsafeProjectText, "-O0"};
    std::string integerUnsafeOutput;
    std::string integerUnsafeError;
    const auto integerUnsafeResult = runDriver(integerUnsafeArguments, integerUnsafeOutput, integerUnsafeError);
    tests.expect(integerUnsafeResult != nativesharp::ExitCode::Success &&
            (integerUnsafeError + integerUnsafeOutput).find("non-escaping contract") != std::string::npos,
        "a noescape out contract rejects raw address conversion (" +
            integerUnsafeError + integerUnsafeOutput + ")");

    const auto indirectUnsafeProject = writeProject(root / "noescape-indirect-unsafe", "NoescapeIndirectUnsafe", R"NS(
public interface Sink { void Write(out long value noescape); }
public class Bad : Sink {
    public static long* saved;
    public static void Leak(ref long value) {
        unsafe { nuint bits = (nuint)&value; saved = (long*)bits; }
    }
    public void Write(out long value) { value = 1L; Leak(ref value); }
}
public class Program { public static int Main() { return 0; } }
)NS", "Exe");
    const std::string indirectUnsafeProjectText = indirectUnsafeProject.string();
    const std::array<std::string_view, 3> indirectUnsafeArguments = {
        "--dump-project-method-bodies", indirectUnsafeProjectText, "-O0"};
    std::string indirectUnsafeOutput;
    std::string indirectUnsafeError;
    const auto indirectUnsafeResult = runDriver(indirectUnsafeArguments, indirectUnsafeOutput, indirectUnsafeError);
    tests.expect(indirectUnsafeResult != nativesharp::ExitCode::Success &&
            (indirectUnsafeError + indirectUnsafeOutput).find("non-escaping contract") != std::string::npos,
        "a noescape out contract rejects raw address conversion in a forwarded helper (" +
            indirectUnsafeError + indirectUnsafeOutput + ")");

    const auto delegateUnsafeProject = writeProject(root / "noescape-delegate-unsafe", "NoescapeDelegateUnsafe", R"NS(
public delegate void Callback(ref long value);
public interface Sink { void Write(out long value noescape); }
public class Bad : Sink {
    public Callback callback;
    public void Write(out long value) { value = 1L; callback(ref value); }
}
public class Program { public static int Main() { return 0; } }
)NS", "Exe");
    const std::string delegateUnsafeProjectText = delegateUnsafeProject.string();
    const std::array<std::string_view, 3> delegateUnsafeArguments = {
        "--dump-project-method-bodies", delegateUnsafeProjectText, "-O0"};
    std::string delegateUnsafeOutput;
    std::string delegateUnsafeError;
    const auto delegateUnsafeResult = runDriver(delegateUnsafeArguments, delegateUnsafeOutput, delegateUnsafeError);
    tests.expect(delegateUnsafeResult != nativesharp::ExitCode::Success &&
            (delegateUnsafeError + delegateUnsafeOutput).find("non-escaping contract") != std::string::npos,
        "a noescape out contract rejects unproven indirect calls (" +
            delegateUnsafeError + delegateUnsafeOutput + ")");

    const auto constructorUnsafeProject = writeProject(root / "noescape-constructor-unsafe", "NoescapeConstructorUnsafe", R"NS(
public interface Sink { void Write(out long value noescape); }
public class Holder {
    public static long* saved;
    public Holder(ref long value) { unsafe { nuint bits = (nuint)&value; saved = (long*)bits; } }
}
public class Bad : Sink {
    public void Write(out long value) { value = 1L; raii Holder holder = new Holder(ref value); }
}
public class Program { public static int Main() { return 0; } }
)NS", "Exe");
    const std::string constructorUnsafeProjectText = constructorUnsafeProject.string();
    const std::array<std::string_view, 3> constructorUnsafeArguments = {
        "--dump-project-method-bodies", constructorUnsafeProjectText, "-O0"};
    std::string constructorUnsafeOutput;
    std::string constructorUnsafeError;
    const auto constructorUnsafeResult = runDriver(
        constructorUnsafeArguments, constructorUnsafeOutput, constructorUnsafeError);
    tests.expect(constructorUnsafeResult != nativesharp::ExitCode::Success &&
            (constructorUnsafeError + constructorUnsafeOutput).find("non-escaping contract") != std::string::npos,
        "a noescape out contract rejects unproven constructor calls (" +
            constructorUnsafeError + constructorUnsafeOutput + ")");

    // A present but unknown contract is not proof that a frame address is safe.
    nativesharp::interop::LibraryMetadata unknownMetadata = roundTrip;
    if (auto member = std::ranges::find_if(unknownMetadata.members, [](const auto& candidate)
        { return candidate.name == "Retain"; }); member != unknownMetadata.members.end())
        member->parameterEscapeSummary.assign(1, nativesharp::interop::LibraryMetadataEscapeClass::Unknown);
    if (auto member = std::ranges::find_if(unknownMetadata.members, [](const auto& candidate)
        { return candidate.name == "ReadRef"; }); member != unknownMetadata.members.end())
        member->parameterEscapeSummary.assign(1, nativesharp::interop::LibraryMetadataEscapeClass::Unknown);
    if (auto function = std::ranges::find_if(unknownMetadata.namespaceFunctions, [](const auto& candidate)
        { return candidate.name == "RetainFunction"; }); function != unknownMetadata.namespaceFunctions.end())
        function->parameterEscapeSummary.assign(1, nativesharp::interop::LibraryMetadataEscapeClass::Unknown);
    const std::filesystem::path unknownPath = root / "EscapeProducer.unknown.nsmeta";
    const bool unknownWritten = codec.writeModel(unknownMetadata, unknownPath, metadataError);
    tests.expect(unknownWritten, "write explicit unknown escape contract (" + metadataError + ")");
    const std::filesystem::path unknownRefProject = writeProject(
        root / "unknown-ref-pointer", "EscapeUnknownRefPointer", R"NS(
using EscapeContracts;
public class Program { public static int Main() { int* local = null; EscapeApi.ReadRef(ref local); return 0; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownRefProjectText = unknownRefProject.string();
    const std::array<std::string_view, 3> unknownRefArguments = {
        "--dump-project-method-bodies", unknownRefProjectText, "-O0"};
    std::string unknownRefOutput;
    std::string unknownRefError;
    const auto unknownRefResult = runDriver(unknownRefArguments, unknownRefOutput, unknownRefError);
    tests.expect(unknownWritten && unknownRefResult != nativesharp::ExitCode::Success &&
            (unknownRefError + unknownRefOutput).find("unknown escape contract") != std::string::npos,
        "an unknown ref-pointer contract cannot retain a local pointer slot (" +
            unknownRefError + unknownRefOutput + ")");
    nativesharp::interop::LibraryMetadata refSlotMetadata = unknownMetadata;
    if (auto member = std::ranges::find_if(refSlotMetadata.members, [](const auto& candidate)
        { return candidate.name == "ReadRef"; }); member != refSlotMetadata.members.end())
        member->parameterEscapeSummary.assign(1, nativesharp::interop::LibraryMetadataEscapeClass::RefSlotNoEscape);
    const auto refSlotPath = root / "EscapeProducer.ref-slot.nsmeta";
    nativesharp::interop::LibraryMetadata refSlotRoundTrip;
    const bool refSlotWritten = codec.writeModel(refSlotMetadata, refSlotPath, metadataError) &&
        codec.read(refSlotPath, refSlotRoundTrip, metadataError);
    const auto refSlotMember = std::ranges::find_if(refSlotRoundTrip.members, [](const auto& candidate)
        { return candidate.name == "ReadRef"; });
    tests.expect(refSlotWritten && refSlotMember != refSlotRoundTrip.members.end() &&
            refSlotMember->parameterEscapeSummary == std::vector<nativesharp::interop::LibraryMetadataEscapeClass>{
                nativesharp::interop::LibraryMetadataEscapeClass::RefSlotNoEscape},
        "a ref-slot-only contract survives metadata serialization (" + metadataError + ")");
    nativesharp::interop::LibraryMetadata invalidRefSlotMetadata = unknownMetadata;
    if (auto member = std::ranges::find_if(invalidRefSlotMetadata.members, [](const auto& candidate)
        { return candidate.name == "Retain"; }); member != invalidRefSlotMetadata.members.end())
        member->parameterEscapeSummary.assign(1, nativesharp::interop::LibraryMetadataEscapeClass::RefSlotNoEscape);
    std::string invalidRefSlotError;
    tests.expect(!codec.verifyCurrentCompiler(invalidRefSlotMetadata, invalidRefSlotError) &&
            invalidRefSlotError.find("ref-slot non-escape contract") != std::string::npos,
        "a ref-slot contract on a value parameter is rejected (" + invalidRefSlotError + ")");
    nativesharp::interop::LibraryMetadata invalidReturnRefMetadata = unknownMetadata;
    if (auto member = std::ranges::find_if(invalidReturnRefMetadata.members, [](const auto& candidate)
        { return candidate.name == "ReturnRefSlot"; }); member != invalidReturnRefMetadata.members.end())
        member->parameterEscapeSummary.assign(1, nativesharp::interop::LibraryMetadataEscapeClass::RefSlotNoEscape);
    std::string invalidReturnRefError;
    tests.expect(!codec.verifyCurrentCompiler(invalidReturnRefMetadata, invalidReturnRefError) &&
            invalidReturnRefError.find("ref-slot non-escape contract") != std::string::npos,
        "a ref-returning method cannot claim a nonescaping slot (" + invalidReturnRefError + ")");
    const std::filesystem::path refSlotUnprovenProject = writeProject(
        root / "ref-slot-unproven", "EscapeRefSlotUnproven", R"NS(
using EscapeContracts;
public class Program { public static int Main() { int* local = null; EscapeApi.ReadRef(ref local); return 0; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.ref-slot.nsmeta\" />");
    const std::string refSlotUnprovenProjectText = refSlotUnprovenProject.string();
    const std::array<std::string_view, 3> refSlotUnprovenArguments = {
        "--dump-project-method-bodies", refSlotUnprovenProjectText, "-O0"};
    std::string refSlotUnprovenOutput;
    std::string refSlotUnprovenError;
    const auto refSlotUnprovenResult = runDriver(
        refSlotUnprovenArguments, refSlotUnprovenOutput, refSlotUnprovenError);
    tests.expect(refSlotWritten && refSlotUnprovenResult == nativesharp::ExitCode::Success,
        "a proven nonescaping slot accepts a definitely null pointer (" +
            refSlotUnprovenError + refSlotUnprovenOutput + ")");
    const auto sourceRefSlotConsumer = [&](std::string_view name, bool frame)
    {
        const std::string source = std::string("using EscapeContracts; public class Program { ") +
            "public static void Wrap(ref int* value) { EscapeApi.Retain(value); } "
            "public static int Main() { int value = 42; int* local = " +
            (frame ? "&value" : "null") + "; Wrap(ref local); return 0; } }";
        const auto project = writeProject(root / std::string(name), std::string(name), source, "Exe",
            "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.ref-slot.nsmeta\" />");
        const std::string projectText = project.string();
        const std::array<std::string_view, 3> arguments = {"--dump-project-method-bodies", projectText, "-O0"};
        std::string output;
        std::string error;
        const auto result = runDriver(arguments, output, error);
        tests.expect(refSlotWritten && (frame ? result != nativesharp::ExitCode::Success &&
                (error + output).find("escape contract") != std::string::npos :
                result == nativesharp::ExitCode::Success),
            std::string(name) + " checks the pointer value of a proven source ref slot (" + error + output + ")");
    };
    sourceRefSlotConsumer("SourceRefSlotNull", false);
    sourceRefSlotConsumer("SourceRefSlotFrame", true);
    const auto inlineRefProducer = writeProject(root / "inline-ref-slot-producer", "InlineRefSlotProducer", R"NS(
public class InlineRefApi {
    public static int* Stored;
    public static inline void Retain(ref int* value) { Stored = value; }
}
)NS", "StaticLibrary");
    const std::string inlineRefProducerText = inlineRefProducer.string();
    const std::string inlineRefLibraryText = (root / "InlineRefSlotProducer.lib").string();
    const std::array<std::string_view, 5> inlineRefProducerArguments = {
        "--emit-project-oop-lib", inlineRefProducerText, "-O0", "-o", inlineRefLibraryText};
    std::string inlineRefProducerOutput;
    std::string inlineRefProducerError;
    const bool inlineRefWritten = runDriver(inlineRefProducerArguments, inlineRefProducerOutput,
        inlineRefProducerError) == nativesharp::ExitCode::Success;
    const auto inlineRefSlotConsumer = [&](std::string_view name, bool frame)
    {
        const std::string source = std::string("public class Program { public static int Main() { ") +
            "int value = 42; int* local = " + (frame ? "&value" : "null") +
            "; InlineRefApi.Retain(ref local); return 0; } }";
        const auto project = writeProject(root / std::string(name), std::string(name), source, "Exe",
            "<LibraryReference Include=\"../InlineRefSlotProducer.lib\" />");
        const std::string projectText = project.string();
        const std::array<std::string_view, 3> arguments = {"--dump-project-method-bodies", projectText, "-O0"};
        std::string output;
        std::string error;
        const auto result = runDriver(arguments, output, error);
        tests.expect(inlineRefWritten && (frame ? result != nativesharp::ExitCode::Success &&
                (error + output).find("escape contract") != std::string::npos :
                result == nativesharp::ExitCode::Success),
            std::string(name) + " checks a transported inline ref slot (" + inlineRefProducerError + error + output + ")");
    };
    inlineRefSlotConsumer("InlineRefSlotNull", false);
    inlineRefSlotConsumer("InlineRefSlotFrame", true);
    const std::filesystem::path refSlotRepeatedProject = writeProject(
        root / "ref-slot-repeated", "EscapeRefSlotRepeated", R"NS(
using EscapeContracts;
public class Program { public static int Main() { int* local = null;
    EscapeApi.ReadRef(ref local); EscapeApi.ReadRef(ref local); return 0; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.ref-slot.nsmeta\" />");
    const std::string refSlotRepeatedProjectText = refSlotRepeatedProject.string();
    const std::array<std::string_view, 3> refSlotRepeatedArguments = {
        "--dump-project-method-bodies", refSlotRepeatedProjectText, "-O0"};
    std::string refSlotRepeatedOutput;
    std::string refSlotRepeatedError;
    const auto refSlotRepeatedResult = runDriver(refSlotRepeatedArguments, refSlotRepeatedOutput, refSlotRepeatedError);
    tests.expect(refSlotWritten && refSlotRepeatedResult != nativesharp::ExitCode::Success &&
            (refSlotRepeatedError + refSlotRepeatedOutput).find("unknown escape contract") != std::string::npos,
        "a repeated call remains conservative without a proven pointer value (" +
            refSlotRepeatedError + refSlotRepeatedOutput + ")");
    const std::filesystem::path refSlotUnknownOutputProject = writeProject(
        root / "ref-slot-unknown-output", "EscapeRefSlotUnknownOutput", R"NS(
using EscapeContracts;
public class Program {
    public static int Run(OutputSink sink) { long value = 0L; int* local = null; sink.Write(out value);
        EscapeApi.ReadRef(ref local); EscapeApi.ReadRef(ref local); return 0; }
    public static int Main() { return 0; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.ref-slot.nsmeta\" />");
    const std::string refSlotUnknownOutputProjectText = refSlotUnknownOutputProject.string();
    const std::array<std::string_view, 3> refSlotUnknownOutputArguments = {
        "--dump-project-method-bodies", refSlotUnknownOutputProjectText, "-O0"};
    std::string refSlotUnknownOutputOutput;
    std::string refSlotUnknownOutputError;
    const auto refSlotUnknownOutputResult = runDriver(
        refSlotUnknownOutputArguments, refSlotUnknownOutputOutput, refSlotUnknownOutputError);
    tests.expect(refSlotWritten && refSlotUnknownOutputResult != nativesharp::ExitCode::Success &&
            (refSlotUnknownOutputError + refSlotUnknownOutputOutput).find("unknown escape contract") != std::string::npos,
        "an unproven out contract cannot establish that the frame is unexposed (" +
            refSlotUnknownOutputError + refSlotUnknownOutputOutput + ")");
    const std::filesystem::path refSlotFrameProject = writeProject(
        root / "ref-slot-frame", "EscapeRefSlotFrame", R"NS(
using EscapeContracts;
public class Program { public static int Main() { int value = 42; int* local = &value; EscapeApi.ReadRef(ref local); return 0; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.ref-slot.nsmeta\" />");
    const std::string refSlotFrameProjectText = refSlotFrameProject.string();
    const std::array<std::string_view, 3> refSlotFrameArguments = {
        "--dump-project-method-bodies", refSlotFrameProjectText, "-O0"};
    std::string refSlotFrameOutput;
    std::string refSlotFrameError;
    const auto refSlotFrameResult = runDriver(refSlotFrameArguments, refSlotFrameOutput, refSlotFrameError);
    tests.expect(refSlotWritten && refSlotFrameResult != nativesharp::ExitCode::Success &&
            (refSlotFrameError + refSlotFrameOutput).find("unknown escape contract") != std::string::npos,
        "a nonescaping slot does not prove its frame pointer safe (" + refSlotFrameError + refSlotFrameOutput + ")");
    const std::filesystem::path refSlotAssignmentProject = writeProject(
        root / "ref-slot-assigned-frame", "EscapeRefSlotAssignedFrame", R"NS(
using EscapeContracts;
public class Program { public static int Main() { int value = 42; int* local = null; local = &value;
    EscapeApi.ReadRef(ref local); return 0; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.ref-slot.nsmeta\" />");
    const std::string refSlotAssignmentProjectText = refSlotAssignmentProject.string();
    const std::array<std::string_view, 3> refSlotAssignmentArguments = {
        "--dump-project-method-bodies", refSlotAssignmentProjectText, "-O0"};
    std::string refSlotAssignmentOutput;
    std::string refSlotAssignmentError;
    const auto refSlotAssignmentResult = runDriver(
        refSlotAssignmentArguments, refSlotAssignmentOutput, refSlotAssignmentError);
    tests.expect(refSlotWritten && refSlotAssignmentResult != nativesharp::ExitCode::Success &&
            (refSlotAssignmentError + refSlotAssignmentOutput).find("unknown escape contract") != std::string::npos,
        "a prior assignment prevents treating the slot as null (" +
            refSlotAssignmentError + refSlotAssignmentOutput + ")");
    const std::filesystem::path refSlotObjectSourceProject = writeProject(
        root / "ref-slot-object-source", "EscapeRefSlotObjectSource", R"NS(
using EscapeContracts;
public class Source { public int* Value; }
public class Program {
    public static void Seed(out int* target, Source source) { target = source.Value; }
    public static int Main() { int value = 42; raii Source source = new Source(); source.Value = &value;
        int* local = null; Seed(out local, source); EscapeApi.RetainRefValue(ref local); return 0; }
}
)NS", "Exe", producerReference);
    const std::string refSlotObjectSourceProjectText = refSlotObjectSourceProject.string();
    const std::array<std::string_view, 3> refSlotObjectSourceArguments = {
        "--dump-project-method-bodies", refSlotObjectSourceProjectText, "-O0"};
    std::string refSlotObjectSourceOutput;
    std::string refSlotObjectSourceError;
    const auto refSlotObjectSourceResult = runDriver(
        refSlotObjectSourceArguments, refSlotObjectSourceOutput, refSlotObjectSourceError);
    tests.expect(metadataWritten && refSlotObjectSourceResult != nativesharp::ExitCode::Success &&
            (refSlotObjectSourceError + refSlotObjectSourceOutput).find("unknown escape contract") != std::string::npos,
        "an output obtained through an object does not prove a pointer safe (" +
            refSlotObjectSourceError + refSlotObjectSourceOutput + ")");
    const std::filesystem::path objectPointerEscapeProject = writeProject(
        root / "object-pointer-escape", "EscapeObjectPointer", R"NS(
using EscapeContracts;
public class Source { public int* Value; }
public class Program {
    public static void Seed(out int* target, Source source) { target = source.Value; }
    public static int Main() { int value = 42; raii Source source = new Source(); source.Value = &value;
        int* local = null; Seed(out local, source); EscapeApi.Retain(local); return 0; }
}
)NS", "Exe", producerReference);
    const std::string objectPointerEscapeProjectText = objectPointerEscapeProject.string();
    const std::array<std::string_view, 3> objectPointerEscapeArguments = {
        "--dump-project-method-bodies", objectPointerEscapeProjectText, "-O0"};
    std::string objectPointerEscapeOutput;
    std::string objectPointerEscapeError;
    const auto objectPointerEscapeResult = runDriver(
        objectPointerEscapeArguments, objectPointerEscapeOutput, objectPointerEscapeError);
    tests.expect(metadataWritten && objectPointerEscapeResult != nativesharp::ExitCode::Success &&
            (objectPointerEscapeError + objectPointerEscapeOutput).find("escape contract") != std::string::npos,
        "a frame pointer remains tracked through an object and an out argument (" +
            objectPointerEscapeError + objectPointerEscapeOutput + ")");
    const std::filesystem::path objectPointerGlobalProject = writeProject(
        root / "object-pointer-global", "EscapeObjectPointerGlobal", R"NS(
using EscapeContracts;
public class Source { public int* Value; }
public class Program {
    public static int value;
    public static void Seed(out int* target, Source source) { target = source.Value; }
    public static int Main() { raii Source source = new Source(); source.Value = &value;
        int* local = null; Seed(out local, source); EscapeApi.Retain(local); return 0; }
}
)NS", "Exe", producerReference);
    const std::string objectPointerGlobalProjectText = objectPointerGlobalProject.string();
    const std::array<std::string_view, 3> objectPointerGlobalArguments = {
        "--dump-project-method-bodies", objectPointerGlobalProjectText, "-O0"};
    std::string objectPointerGlobalOutput;
    std::string objectPointerGlobalError;
    const auto objectPointerGlobalResult = runDriver(
        objectPointerGlobalArguments, objectPointerGlobalOutput, objectPointerGlobalError);
    tests.expect(metadataWritten && objectPointerGlobalResult == nativesharp::ExitCode::Success,
        "a global pointer remains valid through an object and an out argument (" +
            objectPointerGlobalError + objectPointerGlobalOutput + ")");
    const std::filesystem::path objectPointerCrossFrameProject = writeProject(
        root / "object-pointer-cross-frame", "EscapeObjectPointerCrossFrame", R"NS(
using EscapeContracts;
public class Source { public int* Value; }
public class Program {
    public static void Seed(out int* target, Source source) { target = source.Value; }
    public static void Run(Source source) { int* local = null; Seed(out local, source);
        EscapeApi.RetainRefValue(ref local); EscapeApi.RetainRefValue(ref local); }
    public static int Main() { int value = 42; raii Source source = new Source();
        source.Value = &value; Run(source); return 0; }
}
)NS", "Exe", producerReference);
    const std::string objectPointerCrossFrameProjectText = objectPointerCrossFrameProject.string();
    const std::array<std::string_view, 3> objectPointerCrossFrameArguments = {
        "--dump-project-method-bodies", objectPointerCrossFrameProjectText, "-O0"};
    std::string objectPointerCrossFrameOutput;
    std::string objectPointerCrossFrameError;
    const auto objectPointerCrossFrameResult = runDriver(
        objectPointerCrossFrameArguments, objectPointerCrossFrameOutput, objectPointerCrossFrameError);
    tests.expect(metadataWritten && objectPointerCrossFrameResult != nativesharp::ExitCode::Success &&
            (objectPointerCrossFrameError + objectPointerCrossFrameOutput).find("unknown escape contract") != std::string::npos,
        "a caller-owned frame pointer inside an object is not assumed external (" +
            objectPointerCrossFrameError + objectPointerCrossFrameOutput + ")");
    const auto crossFrameValue = [&](std::string_view name, std::string_view valueDeclaration,
                                     std::string_view mainPrefix, std::string_view valueAddress, bool expectSuccess)
    {
        const std::string source = "using EscapeContracts; public class Source { public int* Value; } "
            "public class Program { " + std::string(valueDeclaration) +
            " public static void Seed(out int* target, Source source) { target = source.Value; } "
            "public static void Run(Source source) { int* local = null; Seed(out local, source); EscapeApi.Retain(local); } "
            "public static int Main() { " + std::string(mainPrefix) +
            " raii Source source = new Source(); source.Value = " +
            std::string(valueAddress) + "; Run(source); return 0; } }";
        const auto project = writeProject(root / std::string(name), std::string(name), source, "Exe", producerReference);
        const std::string projectText = project.string();
        const std::array<std::string_view, 3> arguments = {"--dump-project-method-bodies", projectText, "-O0"};
        std::string output;
        std::string error;
        const auto result = runDriver(arguments, output, error);
        tests.expect(metadataWritten && (expectSuccess ? result == nativesharp::ExitCode::Success :
                result != nativesharp::ExitCode::Success && (error + output).find("escape contract") != std::string::npos),
            std::string(name) + " tracks the pointer inside the object (" + error + output + ")");
    };
    crossFrameValue("ObjectPointerCallerFrame", "", "int value = 42;", "&value", false);
    crossFrameValue("ObjectPointerGlobalFrame", "public static int value;", "", "&value", true);
    const auto crossFrameStore = [&](std::string_view name, std::string_view declaration,
                                     std::string_view mainPrefix, bool expectSuccess)
    {
        const std::string source = "public class Source { public int* Value; } public class Program { " +
            std::string(declaration) + " public static int* Saved; "
            "public static void Seed(out int* target, Source source) { target = source.Value; } "
            "public static void Run(Source source) { int* local = null; Seed(out local, source); Saved = local; } "
            "public static int Main() { " + std::string(mainPrefix) +
            " raii Source source = new Source(); source.Value = &value; Run(source); return 0; } }";
        const auto project = writeProject(root / std::string(name), std::string(name), source, "Exe");
        const std::string projectText = project.string();
        const std::array<std::string_view, 3> arguments = {"--dump-project-method-bodies", projectText, "-O0"};
        std::string output;
        std::string error;
        const auto result = runDriver(arguments, output, error);
        tests.expect(expectSuccess ? result == nativesharp::ExitCode::Success :
                result != nativesharp::ExitCode::Success && (error + output).find("escape contract") != std::string::npos,
            std::string(name) + " distinguishes the stored pointer's origin (" + error + output + ")");
    };
    crossFrameStore("ObjectPointerStaticFrame", "", "int value = 42;", false);
    crossFrameStore("ObjectPointerStaticGlobal", "public static int value;", "", true);
    const std::filesystem::path objectIdentityEscapeProject = writeProject(
        root / "object-identity-escape", "ObjectIdentityEscape", R"NS(
public class Source { public int* Value; }
public class Program {
    public static int value;
    public static int* SavedPointer;
    public static Source SavedObject;
    public static void Seed(out int* target, Source source) { target = source.Value; }
    public static void Run(Source source) { int* local = null; Seed(out local, source);
        SavedPointer = local; SavedObject = source; }
    public static int Main() { raii Source source = new Source(); source.Value = &value; Run(source); return 0; }
}
)NS", "Exe");
    const std::string objectIdentityEscapeProjectText = objectIdentityEscapeProject.string();
    const std::array<std::string_view, 3> objectIdentityEscapeArguments = {
        "--dump-project-method-bodies", objectIdentityEscapeProjectText, "-O0"};
    std::string objectIdentityEscapeOutput;
    std::string objectIdentityEscapeError;
    const auto objectIdentityEscapeResult = runDriver(
        objectIdentityEscapeArguments, objectIdentityEscapeOutput, objectIdentityEscapeError);
    tests.expect(objectIdentityEscapeResult != nativesharp::ExitCode::Success &&
            (objectIdentityEscapeError + objectIdentityEscapeOutput).find("escape contract") != std::string::npos,
        "an object cannot escape merely because its pointer field is global (" +
            objectIdentityEscapeError + objectIdentityEscapeOutput + ")");
    const std::filesystem::path objectPointerReadProject = writeProject(
        root / "object-pointer-read", "ObjectPointerRead", R"NS(
using EscapeContracts;
public class Source { public int* Value; }
public class Program {
    public static void Seed(out int* target, Source source) { target = source.Value; }
    public static int Read(Source source) { int* local = null; Seed(out local, source); return EscapeApi.Read(local); }
    public static int Main() { int value = 42; raii Source source = new Source();
        source.Value = &value; return Read(source); }
}
)NS", "Exe", producerReference);
    const std::string objectPointerReadProjectText = objectPointerReadProject.string();
    const std::array<std::string_view, 3> objectPointerReadArguments = {
        "--dump-project-method-bodies", objectPointerReadProjectText, "-O0"};
    std::string objectPointerReadOutput;
    std::string objectPointerReadError;
    const auto objectPointerReadResult = runDriver(
        objectPointerReadArguments, objectPointerReadOutput, objectPointerReadError);
    tests.expect(metadataWritten && objectPointerReadResult == nativesharp::ExitCode::Success,
        "reading a frame pointer carried by an object remains allowed (" +
            objectPointerReadError + objectPointerReadOutput + ")");
    const std::filesystem::path objectPointerWrapperProject = writeProject(
        root / "object-pointer-wrapper", "EscapeObjectPointerWrapper", R"NS(
using EscapeContracts;
public class Source { public int* Value; }
public class Program {
    public static void Seed(out int* target, Source source) { target = source.Value; }
    public static void Run(Source source) { int* local = null; Seed(out local, source); EscapeApi.Retain(local); }
    public static void Forward(Source source) { Run(source); }
    public static int Main() { int value = 42; raii Source source = new Source();
        source.Value = &value; Forward(source); return 0; }
}
)NS", "Exe", producerReference);
    const std::string objectPointerWrapperProjectText = objectPointerWrapperProject.string();
    const std::array<std::string_view, 3> objectPointerWrapperArguments = {
        "--dump-project-method-bodies", objectPointerWrapperProjectText, "-O0"};
    std::string objectPointerWrapperOutput;
    std::string objectPointerWrapperError;
    const auto objectPointerWrapperResult = runDriver(
        objectPointerWrapperArguments, objectPointerWrapperOutput, objectPointerWrapperError);
    tests.expect(metadataWritten && objectPointerWrapperResult != nativesharp::ExitCode::Success &&
            (objectPointerWrapperError + objectPointerWrapperOutput).find("escape contract") != std::string::npos,
        "an object-contained pointer retains its origin through a wrapper (" +
            objectPointerWrapperError + objectPointerWrapperOutput + ")");
    const auto objectBinaryProducer = writeProject(root / "object-pointer-binary-producer", "ObjectPointerBinaryProducer", R"NS(
public class Source { public int* Value; }
public class Api {
    public static int* Saved;
    public static void Seed(out int* target, Source source) { target = source.Value; }
    public static void Run(Source source) { int* local = null; Seed(out local, source); Saved = local; }
}
)NS", "StaticLibrary");
    const std::string objectBinaryProducerText = objectBinaryProducer.string();
    const std::string objectBinaryLibraryText = (root / "ObjectPointerBinaryProducer.lib").string();
    const std::array<std::string_view, 5> objectBinaryProducerArguments = {
        "--emit-project-oop-lib", objectBinaryProducerText, "-O0", "-o", objectBinaryLibraryText};
    std::string objectBinaryProducerOutput;
    std::string objectBinaryProducerError;
    const bool objectBinaryWritten = runDriver(objectBinaryProducerArguments, objectBinaryProducerOutput,
        objectBinaryProducerError) == nativesharp::ExitCode::Success;
    nativesharp::interop::LibraryMetadata objectPointerMetadata;
    std::string objectPointerMetadataError;
    const bool objectPointerMetadataRead = codec.read(root / "ObjectPointerBinaryProducer.nsmeta",
        objectPointerMetadata, objectPointerMetadataError);
    const auto objectPointerSeed = std::ranges::find_if(objectPointerMetadata.members, [](const auto& member)
        { return member.name == "Seed"; });
    tests.expect(objectBinaryWritten && objectPointerMetadataRead && objectPointerSeed != objectPointerMetadata.members.end() &&
            objectPointerSeed->pointerOutputObjectOrigins == std::vector<std::vector<std::uint32_t>>{{1U}, {}},
        "the observed object source of an out pointer survives the library boundary (" +
            objectBinaryProducerError + objectPointerMetadataError + ")");
    auto invalidPointerMetadata = objectPointerMetadata;
    const auto invalidPointerSeed = std::ranges::find_if(invalidPointerMetadata.members, [](const auto& member)
        { return member.name == "Seed"; });
    if (invalidPointerSeed != invalidPointerMetadata.members.end())
        invalidPointerSeed->pointerOutputObjectOrigins = {{0U}, {}};
    std::string invalidPointerMetadataError;
    tests.expect(objectPointerMetadataRead && !codec.verifyCurrentCompiler(invalidPointerMetadata, invalidPointerMetadataError) &&
            invalidPointerMetadataError.find("pointer-output origin source") != std::string::npos,
        "invalid pointer-output object origins are rejected (" + invalidPointerMetadataError + ")");
    const auto objectPointerBridge = writeProject(root / "object-pointer-binary-bridge", "ObjectPointerBinaryBridge", R"NS(
public class Bridge { public static void Forward(out int* output, Source source) { Api.Seed(out output, source); } }
)NS", "StaticLibrary", "<LibraryReference Include=\"../ObjectPointerBinaryProducer.lib\" />");
    const std::string objectPointerBridgeText = objectPointerBridge.string();
    const std::string objectPointerBridgeLibrary = (root / "ObjectPointerBinaryBridge.lib").string();
    const std::array<std::string_view, 5> objectPointerBridgeArguments = {
        "--emit-project-oop-lib", objectPointerBridgeText, "-O0", "-o", objectPointerBridgeLibrary};
    std::string objectPointerBridgeOutput;
    std::string objectPointerBridgeError;
    const bool objectPointerBridgeWritten = runDriver(objectPointerBridgeArguments, objectPointerBridgeOutput,
        objectPointerBridgeError) == nativesharp::ExitCode::Success;
    nativesharp::interop::LibraryMetadata objectPointerBridgeMetadata;
    std::string objectPointerBridgeMetadataError;
    const bool objectPointerBridgeRead = codec.read(root / "ObjectPointerBinaryBridge.nsmeta",
        objectPointerBridgeMetadata, objectPointerBridgeMetadataError);
    const auto objectPointerForward = std::ranges::find_if(objectPointerBridgeMetadata.members, [](const auto& member)
        { return member.name == "Forward"; });
    tests.expect(objectPointerBridgeWritten && objectPointerBridgeRead &&
            objectPointerForward != objectPointerBridgeMetadata.members.end() &&
            objectPointerForward->pointerOutputObjectOrigins == std::vector<std::vector<std::uint32_t>>{{1U}, {}},
        "an object source survives an out-forwarding library (" + objectPointerBridgeError +
            objectPointerBridgeMetadataError + objectPointerBridgeOutput + ")");
    const auto allocatorOriginProject = writeProject(root / "allocator-origin-producer", "AllocatorOriginProducer", R"NS(
public interface Allocator { int* Allocate(); }
public class Api { public static void Allocate(Allocator allocator, out int* output) {
    int* local = allocator.Allocate(); output = local;
} }
)NS", "StaticLibrary");
    const std::string allocatorOriginProjectText = allocatorOriginProject.string();
    const std::string allocatorOriginLibrary = (root / "AllocatorOriginProducer.lib").string();
    const std::array<std::string_view, 5> allocatorOriginArguments = {
        "--emit-project-oop-lib", allocatorOriginProjectText, "-O0", "-o", allocatorOriginLibrary};
    std::string allocatorOriginOutput;
    std::string allocatorOriginError;
    const bool allocatorOriginWritten = runDriver(allocatorOriginArguments, allocatorOriginOutput,
        allocatorOriginError) == nativesharp::ExitCode::Success;
    nativesharp::interop::LibraryMetadata allocatorOriginMetadata;
    std::string allocatorOriginMetadataError;
    const bool allocatorOriginRead = codec.read(root / "AllocatorOriginProducer.nsmeta",
        allocatorOriginMetadata, allocatorOriginMetadataError);
    const auto allocatorOriginMember = std::ranges::find_if(allocatorOriginMetadata.members, [](const auto& member)
        { return member.declaringTypeKey.ends_with("::Api") && member.name == "Allocate"; });
    tests.expect(allocatorOriginWritten && allocatorOriginRead &&
            allocatorOriginMember != allocatorOriginMetadata.members.end() &&
            allocatorOriginMember->pointerOutputObjectOrigins == std::vector<std::vector<std::uint32_t>>{{}, {0U}},
        "an allocator receiver is retained as a possible out-pointer origin (" + allocatorOriginError +
            allocatorOriginMetadataError + allocatorOriginOutput + ")");
    const auto pairViewProducer = writeProject(root / "borrowed-out-producer", "BorrowedOutProducer", R"NS(
public borrowed struct Window {
    public int* data;
    public Window(int* value) { data = value; }
    public int Read() { return data[0]; }
}
public class Source {
    private owned int[] values;
    public Source() { values = new int[1]; }
    public Window View() { return new Window(&values[0]); }
}
public class PairViews {
    public static void Get(borrowed Source first, borrowed Source second, out Window left, out Window right) {
        left = first.View(); right = second.View();
    }
}
)NS", "StaticLibrary");
    const std::string pairViewProducerText = pairViewProducer.string();
    const std::string pairViewLibrary = (root / "BorrowedOutProducer.lib").string();
    const std::array<std::string_view, 5> pairViewProducerArgs = {
        "--emit-project-oop-lib", pairViewProducerText, "-O0", "-o", pairViewLibrary};
    std::string pairViewProducerOut;
    std::string pairViewProducerError;
    const bool pairViewWritten = runDriver(pairViewProducerArgs, pairViewProducerOut, pairViewProducerError) ==
        nativesharp::ExitCode::Success;
    const auto pairViewConsumer = [&](std::string_view name, std::string_view source, bool shouldCompile)
    {
        const auto project = writeProject(root / std::string(name), name, source, "Exe",
            "<LibraryReference Include=\"../BorrowedOutProducer.lib\" />");
        const std::string projectText = project.string();
        const std::array<std::string_view, 3> arguments = {"--dump-project-method-bodies", projectText, "-O0"};
        std::string output;
        std::string error;
        const auto code = runDriver(arguments, output, error);
        tests.expect(pairViewWritten && (shouldCompile ? code == nativesharp::ExitCode::Success :
                code != nativesharp::ExitCode::Success &&
                (error + output).find("view output cannot outlive its possible owner") != std::string::npos),
            std::string(name) + " keeps output views tied to binary borrowed inputs (" +
                pairViewProducerError + error + output + ")");
    };
    pairViewConsumer("BorrowedOutBinaryUnsafe", R"NS(
class Program { public static int Main() { Window outside = default(Window);
    { raii Source first = new Source(); raii Source second = new Source(); Window other = default(Window);
      PairViews.Get(first, second, out outside, out other); } return outside.Read(); } }
)NS", false);
    pairViewConsumer("BorrowedOutBinarySafe", R"NS(
class Program { public static int Main() { raii Source first = new Source(); raii Source second = new Source();
    Window left = default(Window); Window right = default(Window);
    PairViews.Get(first, second, out left, out right); return left.Read() + right.Read(); } }
)NS", true);

    const auto allocatorIdentityProject = writeProject(root / "allocator-identity-producer", "AllocatorIdentityProducer", R"NS(
public interface Allocator { int* Allocate(); void Free(int* block); }
public class SafeAllocator : Allocator {
    public static int Value; public int* Allocate() { return &Value; } public void Free(int* block) { }
}
public class RetainingAllocator : Allocator {
    public static RetainingAllocator Held; public static int Value;
    public int* Allocate() { Held = this; return &Value; } public void Free(int* block) { }
}
public class EscapingConstructorAllocator : Allocator {
    public static EscapingConstructorAllocator Held; public static int Value;
    public EscapingConstructorAllocator() { Held = this; }
    public int* Allocate() { return &Value; } public void Free(int* block) { }
}
public class WrappedAllocator : Allocator {
    private Allocator inner; private long count; public static int Value;
    public WrappedAllocator(Allocator source, long initial) { inner = source; count = initial; }
    public int* Allocate() { return &Value; } public void Free(int* block) { }
}
public class DestructorAllocator : Allocator {
    public static int Value; public DestructorAllocator() { }
    ~DestructorAllocator() { Value = 1; }
    public int* Allocate() { return &Value; } public void Free(int* block) { }
}
public class CallbackConstructorAllocator : Allocator {
    public static CallbackConstructorAllocator Held; public static int Value;
    private static void Publish(CallbackConstructorAllocator value) { Held = value; }
    public CallbackConstructorAllocator() { Publish(this); }
    public int* Allocate() { return &Value; } public void Free(int* block) { }
}
)NS", "StaticLibrary");
    const std::string allocatorIdentityText = allocatorIdentityProject.string();
    const std::string allocatorIdentityLibrary = (root / "AllocatorIdentityProducer.lib").string();
    const std::array<std::string_view, 5> allocatorIdentityArguments = {
        "--emit-project-oop-lib", allocatorIdentityText, "-O0", "-o", allocatorIdentityLibrary};
    std::string allocatorIdentityOutput;
    std::string allocatorIdentityError;
    const bool allocatorIdentityWritten = runDriver(allocatorIdentityArguments, allocatorIdentityOutput,
        allocatorIdentityError) == nativesharp::ExitCode::Success;
    nativesharp::interop::LibraryMetadata allocatorIdentityMetadata;
    std::string allocatorIdentityMetadataError;
    const bool allocatorIdentityRead = codec.read(root / "AllocatorIdentityProducer.nsmeta",
        allocatorIdentityMetadata, allocatorIdentityMetadataError);
    const auto safeAllocate = std::ranges::find_if(allocatorIdentityMetadata.members, [](const auto& member)
        { return member.declaringTypeKey.ends_with("::SafeAllocator") && member.name == "Allocate"; });
    const auto retainingAllocate = std::ranges::find_if(allocatorIdentityMetadata.members, [](const auto& member)
        { return member.declaringTypeKey.ends_with("::RetainingAllocator") && member.name == "Allocate"; });
    const bool safeLifecycle = std::ranges::find(allocatorIdentityMetadata.nonEscapingLifecycleTypeKeys,
        "type:AllocatorIdentityProducer::SafeAllocator") != allocatorIdentityMetadata.nonEscapingLifecycleTypeKeys.end();
    const bool wrappedIdentity = std::ranges::find(allocatorIdentityMetadata.identitySafeLifecycleTypeKeys,
        "type:AllocatorIdentityProducer::WrappedAllocator") != allocatorIdentityMetadata.identitySafeLifecycleTypeKeys.end();
    const bool wrappedStrong = std::ranges::find(allocatorIdentityMetadata.nonEscapingLifecycleTypeKeys,
        "type:AllocatorIdentityProducer::WrappedAllocator") != allocatorIdentityMetadata.nonEscapingLifecycleTypeKeys.end();
    const bool escapingIdentity = std::ranges::find(allocatorIdentityMetadata.identitySafeLifecycleTypeKeys,
        "type:AllocatorIdentityProducer::EscapingConstructorAllocator") != allocatorIdentityMetadata.identitySafeLifecycleTypeKeys.end();
    const bool destructorIdentity = std::ranges::find(allocatorIdentityMetadata.identitySafeLifecycleTypeKeys,
        "type:AllocatorIdentityProducer::DestructorAllocator") != allocatorIdentityMetadata.identitySafeLifecycleTypeKeys.end();
    const bool callbackIdentity = std::ranges::find(allocatorIdentityMetadata.identitySafeLifecycleTypeKeys,
        "type:AllocatorIdentityProducer::CallbackConstructorAllocator") != allocatorIdentityMetadata.identitySafeLifecycleTypeKeys.end();
    tests.expect(allocatorIdentityWritten && allocatorIdentityRead && safeLifecycle &&
            wrappedIdentity && !wrappedStrong && !escapingIdentity && !destructorIdentity && !callbackIdentity &&
            safeAllocate != allocatorIdentityMetadata.members.end() &&
            retainingAllocate != allocatorIdentityMetadata.members.end() &&
            (safeAllocate->flags & nativesharp::interop::kMetadataMemberReceiverNoReferenceFieldEffects) != 0U &&
            (retainingAllocate->flags & nativesharp::interop::kMetadataMemberReceiverNoReferenceFieldEffects) == 0U,
        "producer distinguishes the nonescaping allocator receiver and lifecycle (" +
            allocatorIdentityError + allocatorIdentityMetadataError + ")");
    auto invalidIdentityMetadata = allocatorIdentityMetadata;
    invalidIdentityMetadata.identitySafeLifecycleTypeKeys.push_back("type:Missing::Allocator");
    std::string invalidIdentityError;
    tests.expect(allocatorIdentityRead && !codec.verifyCurrentCompiler(invalidIdentityMetadata, invalidIdentityError) &&
            invalidIdentityError.find("receiver-identity lifecycle") != std::string::npos,
        "metadata rejects an identity contract for a missing type (" + invalidIdentityError + ")");
    auto duplicateIdentityMetadata = allocatorIdentityMetadata;
    duplicateIdentityMetadata.identitySafeLifecycleTypeKeys.push_back("type:AllocatorIdentityProducer::WrappedAllocator");
    std::string duplicateIdentityError;
    tests.expect(allocatorIdentityRead && wrappedIdentity &&
            !codec.verifyCurrentCompiler(duplicateIdentityMetadata, duplicateIdentityError) &&
            duplicateIdentityError.find("receiver-identity lifecycle") != std::string::npos,
        "metadata rejects duplicate identity contracts (" + duplicateIdentityError + ")");
    const auto allocatorIdentityConsumer = [&](std::string_view name, std::string_view type, bool expectSuccess)
    {
        const std::string source = std::string("public class Program { public static void Use(Allocator allocator) { ") +
            "int* block = allocator.Allocate(); allocator.Free(block); } public static int Main() { raii " +
            std::string(type) + " value = new " + std::string(type) + "(); Use(value); return 0; } }";
        const auto project = writeProject(root / std::string(name), std::string(name), source, "Exe",
            "<LibraryReference Include=\"../AllocatorIdentityProducer.lib\" />");
        const std::string projectText = project.string();
        const std::array<std::string_view, 3> arguments = {"--dump-project-method-bodies", projectText, "-O0"};
        std::string output;
        std::string error;
        const auto result = runDriver(arguments, output, error);
        tests.expect(allocatorIdentityWritten && (expectSuccess ? result == nativesharp::ExitCode::Success :
                result != nativesharp::ExitCode::Success && (error + output).find("escape contract") != std::string::npos),
            std::string(name) + " distinguishes an allocator receiver that escapes (" +
                allocatorIdentityError + error + output + ")");
    };
    allocatorIdentityConsumer("SafeBinaryAllocator", "SafeAllocator", true);
    allocatorIdentityConsumer("RetainingBinaryAllocator", "RetainingAllocator", false);
    allocatorIdentityConsumer("EscapingConstructorBinaryAllocator", "EscapingConstructorAllocator", false);
    const auto wrappedAllocatorProject = writeProject(root / "wrapped-binary-allocator", "WrappedBinaryAllocator", R"NS(
public class Program { public static void Use(Allocator allocator) {
    int* block = allocator.Allocate(); allocator.Free(block);
} public static int Main() { raii SafeAllocator inner = new SafeAllocator();
    raii WrappedAllocator outer = new WrappedAllocator(inner, 1L); Use(outer); Use(outer); return 0; } }
)NS", "Exe", "<LibraryReference Include=\"../AllocatorIdentityProducer.lib\" />");
    const std::string wrappedAllocatorText = wrappedAllocatorProject.string();
    const std::array<std::string_view, 3> wrappedAllocatorArguments = {
        "--dump-project-method-bodies", wrappedAllocatorText, "-O0"};
    std::string wrappedAllocatorOutput;
    std::string wrappedAllocatorError;
    const auto wrappedAllocatorResult = runDriver(wrappedAllocatorArguments, wrappedAllocatorOutput, wrappedAllocatorError);
    tests.expect(allocatorIdentityWritten && wrappedAllocatorResult == nativesharp::ExitCode::Success,
        "an identity-safe constructor can wrap an allocator without claiming shared-payload safety (" +
            wrappedAllocatorError + wrappedAllocatorOutput + ")");
    const auto repeatAllocatorProject = writeProject(root / "repeat-binary-allocator", "RepeatBinaryAllocator", R"NS(
public class Program { public static void Use(Allocator allocator) {
    int* block = allocator.Allocate(); allocator.Free(block);
} public static int Main() { raii SafeAllocator value = new SafeAllocator();
    Use(value); Use(value); value.Free(null); return 0; } }
)NS", "Exe", "<LibraryReference Include=\"../AllocatorIdentityProducer.lib\" />");
    const std::string repeatAllocatorText = repeatAllocatorProject.string();
    const std::array<std::string_view, 3> repeatAllocatorArguments = {
        "--dump-project-method-bodies", repeatAllocatorText, "-O0"};
    std::string repeatAllocatorOutput;
    std::string repeatAllocatorError;
    const auto repeatAllocatorResult = runDriver(repeatAllocatorArguments, repeatAllocatorOutput, repeatAllocatorError);
    tests.expect(allocatorIdentityWritten && repeatAllocatorResult == nativesharp::ExitCode::Success,
        "repeated certified calls keep an exact binary allocator valid (" + repeatAllocatorError + repeatAllocatorOutput + ")");
    const auto aliasAllocatorProject = writeProject(root / "alias-binary-allocator", "AliasBinaryAllocator", R"NS(
public class Program { public static void Use(Allocator allocator) {
    int* block = allocator.Allocate(); allocator.Free(block);
} public static int Main() { raii SafeAllocator value = new SafeAllocator();
    SafeAllocator alias = value; Use(value); return 0; } }
)NS", "Exe", "<LibraryReference Include=\"../AllocatorIdentityProducer.lib\" />");
    const std::string aliasAllocatorText = aliasAllocatorProject.string();
    const std::array<std::string_view, 3> aliasAllocatorArguments = {
        "--dump-project-method-bodies", aliasAllocatorText, "-O0"};
    std::string aliasAllocatorOutput;
    std::string aliasAllocatorError;
    const auto aliasAllocatorResult = runDriver(aliasAllocatorArguments, aliasAllocatorOutput, aliasAllocatorError);
    tests.expect(allocatorIdentityWritten && aliasAllocatorResult != nativesharp::ExitCode::Success &&
            (aliasAllocatorError + aliasAllocatorOutput).find("escape contract") != std::string::npos,
        "an unproven allocator alias does not receive the exact receiver certificate (" +
            aliasAllocatorError + aliasAllocatorOutput + ")");
    const auto objectBinaryConsumer = [&](std::string_view name, std::string_view declaration,
                                          std::string_view mainPrefix, bool expectSuccess)
    {
        const std::string source = "public class Program { " + std::string(declaration) +
            " public static int Main() { " + std::string(mainPrefix) +
            " raii Source source = new Source(); source.Value = &value; Api.Run(source); return 0; } }";
        const auto project = writeProject(root / std::string(name), std::string(name), source, "Exe",
            "<LibraryReference Include=\"../ObjectPointerBinaryProducer.lib\" />");
        const std::string projectText = project.string();
        const std::array<std::string_view, 3> arguments = {"--dump-project-method-bodies", projectText, "-O0"};
        std::string output;
        std::string error;
        const auto result = runDriver(arguments, output, error);
        tests.expect(objectBinaryWritten && (expectSuccess ? result == nativesharp::ExitCode::Success :
            result != nativesharp::ExitCode::Success && (error + output).find("escape contract") != std::string::npos),
            std::string(name) + " preserves pointer origin across a library (" +
                objectBinaryProducerError + error + output + ")");
    };
    objectBinaryConsumer("ObjectPointerBinaryLocal", "", "int value = 42;", false);
    objectBinaryConsumer("ObjectPointerBinaryGlobal", "public static int value;", "", true);
    const auto conditionalPointerProject = writeProject(root / "conditional-pointer-binary", "ConditionalPointerBinary", R"NS(
public class Program { public static int value; public static int Main() {
    raii Source source = new Source(); if (value == 0) { source.Value = &value; } Api.Run(source); return 0;
} }
)NS", "Exe", "<LibraryReference Include=\"../ObjectPointerBinaryProducer.lib\" />");
    const std::string conditionalPointerText = conditionalPointerProject.string();
    const std::array<std::string_view, 3> conditionalPointerArguments = {
        "--dump-project-method-bodies", conditionalPointerText, "-O0"};
    std::string conditionalPointerOutput;
    std::string conditionalPointerError;
    const auto conditionalPointerResult = runDriver(conditionalPointerArguments, conditionalPointerOutput,
        conditionalPointerError);
    tests.expect(objectBinaryWritten && conditionalPointerResult != nativesharp::ExitCode::Success &&
            (conditionalPointerError + conditionalPointerOutput).find("unknown to the binary escape contract") != std::string::npos,
        "a conditional field assignment does not prove the object pointer (" + conditionalPointerError +
            conditionalPointerOutput + ")");
    const auto escapingPointerConstructor = writeProject(root / "pointer-escaping-constructor-producer",
        "PointerEscapingConstructorProducer", R"NS(
public class Source { public int* Value; public static Source Held; public Source() { Held = this; } }
public class ExtraFieldSource { public int* Value; private int* Hidden; }
public class InitializedSource { public int* Value = null; }
public class Api { public static int* Saved;
    public static void Seed(out int* target, Source source) { target = source.Value; }
    public static void Run(Source source) { int* local = null; Seed(out local, source); Saved = local; }
}
)NS", "StaticLibrary");
    const std::string escapingPointerConstructorText = escapingPointerConstructor.string();
    const std::string escapingPointerConstructorLibrary = (root / "PointerEscapingConstructorProducer.lib").string();
    const std::array<std::string_view, 5> escapingPointerConstructorArguments = {
        "--emit-project-oop-lib", escapingPointerConstructorText, "-O0", "-o", escapingPointerConstructorLibrary};
    std::string escapingPointerConstructorOutput;
    std::string escapingPointerConstructorError;
    const bool escapingPointerConstructorWritten = runDriver(escapingPointerConstructorArguments,
        escapingPointerConstructorOutput, escapingPointerConstructorError) == nativesharp::ExitCode::Success;
    nativesharp::interop::LibraryMetadata escapingPointerConstructorMetadata;
    std::string escapingPointerMetadataError;
    const bool escapingPointerMetadataRead = codec.read(root / "PointerEscapingConstructorProducer.nsmeta",
        escapingPointerConstructorMetadata, escapingPointerMetadataError);
    tests.expect(escapingPointerConstructorWritten && escapingPointerMetadataRead &&
            std::ranges::find(escapingPointerConstructorMetadata.nonEscapingLifecycleTypeKeys,
                "type:PointerEscapingConstructorProducer::Source") ==
                escapingPointerConstructorMetadata.nonEscapingLifecycleTypeKeys.end() &&
            std::ranges::find(escapingPointerConstructorMetadata.nonEscapingLifecycleTypeKeys,
                "type:PointerEscapingConstructorProducer::ExtraFieldSource") ==
                escapingPointerConstructorMetadata.nonEscapingLifecycleTypeKeys.end() &&
            std::ranges::find(escapingPointerConstructorMetadata.nonEscapingLifecycleTypeKeys,
                "type:PointerEscapingConstructorProducer::InitializedSource") ==
                escapingPointerConstructorMetadata.nonEscapingLifecycleTypeKeys.end(),
        "escaping constructors, hidden fields and field initializers do not obtain the pointer-root certificate (" +
            escapingPointerConstructorError + escapingPointerMetadataError + ")");
    tests.expect(escapingPointerConstructorWritten && escapingPointerMetadataRead &&
            std::ranges::find(escapingPointerConstructorMetadata.identitySafeLifecycleTypeKeys,
                "type:PointerEscapingConstructorProducer::Source") ==
                escapingPointerConstructorMetadata.identitySafeLifecycleTypeKeys.end() &&
            std::ranges::find(escapingPointerConstructorMetadata.identitySafeLifecycleTypeKeys,
                "type:PointerEscapingConstructorProducer::InitializedSource") ==
                escapingPointerConstructorMetadata.identitySafeLifecycleTypeKeys.end(),
        "escaping constructors and field initializers do not obtain an identity-only certificate (" +
            escapingPointerConstructorError + escapingPointerMetadataError + ")");
    const auto escapingPointerConsumer = writeProject(root / "pointer-escaping-constructor-consumer",
        "PointerEscapingConstructorConsumer", R"NS(
public class Program { public static int value; public static int Main() {
    raii Source source = new Source(); source.Value = &value; Api.Run(source); return 0;
} }
)NS", "Exe", "<LibraryReference Include=\"../PointerEscapingConstructorProducer.lib\" />");
    const std::string escapingPointerConsumerText = escapingPointerConsumer.string();
    const std::array<std::string_view, 3> escapingPointerConsumerArguments = {
        "--dump-project-method-bodies", escapingPointerConsumerText, "-O0"};
    std::string escapingPointerConsumerOutput;
    std::string escapingPointerConsumerError;
    const auto escapingPointerConsumerResult = runDriver(escapingPointerConsumerArguments,
        escapingPointerConsumerOutput, escapingPointerConsumerError);
    tests.expect(escapingPointerConstructorWritten && escapingPointerConsumerResult != nativesharp::ExitCode::Success &&
            (escapingPointerConsumerError + escapingPointerConsumerOutput).find("escape contract") != std::string::npos,
        "a global field does not excuse an escaping constructor (" + escapingPointerConsumerError +
            escapingPointerConsumerOutput + ")");
    const auto objectBinaryOutConsumer = [&](std::string_view name, bool global)
    {
        const std::string source = std::string("public class Program { public static int* Saved; ") +
            (global ? "public static int value; " : "") + "public static int Main() { " +
            (global ? "" : "int value = 42; ") +
            "raii Source source = new Source(); source.Value = &value; int* local = null; "
            "Api.Seed(out local, source); Saved = local; return 0; } }";
        const auto project = writeProject(root / std::string(name), std::string(name), source, "Exe",
            "<LibraryReference Include=\"../ObjectPointerBinaryProducer.lib\" />");
        const std::string projectText = project.string();
        const std::array<std::string_view, 3> arguments = {"--dump-project-method-bodies", projectText, "-O0"};
        std::string output;
        std::string error;
        const auto result = runDriver(arguments, output, error);
        tests.expect(objectBinaryWritten && (global ? result == nativesharp::ExitCode::Success :
            result != nativesharp::ExitCode::Success &&
                (error + output).find("pointer to current-frame storage cannot escape") != std::string::npos),
            std::string(name) + " preserves the origin of a pointer returned through out (" + error + output + ")");
    };
    objectBinaryOutConsumer("ObjectPointerBinaryOutLocal", false);
    objectBinaryOutConsumer("ObjectPointerBinaryOutGlobal", true);
    const auto receiverPointerConsumer = [&](std::string_view name, bool global, bool laterAssignment)
    {
        const std::string source = std::string("public class Source { public int* Value; ") +
            "public int* Read() { return Value; } } public class Program { public static int* Saved; " +
            (global ? "public static int value; " : "") + "public static int Main() { " +
            (global ? "" : "int value = 42; ") +
            "raii Source source = new Source(); source.Value = &value; " +
            (laterAssignment ? "int* local = null; local = source.Read(); " : "int* local = source.Read(); ") +
            "Saved = local; return 0; } }";
        const auto project = writeProject(root / std::string(name), std::string(name), source, "Exe");
        const std::string projectText = project.string();
        const std::array<std::string_view, 3> arguments = {"--dump-project-method-bodies", projectText, "-O0"};
        std::string output;
        std::string error;
        const auto result = runDriver(arguments, output, error);
        tests.expect(global ? result == nativesharp::ExitCode::Success :
            result != nativesharp::ExitCode::Success &&
                (error + output).find("pointer to current-frame storage cannot escape") != std::string::npos,
            std::string(name) + " preserves the origin returned from a receiver (" + error + output + ")");
    };
    receiverPointerConsumer("ReceiverPointerLocalInitializer", false, false);
    receiverPointerConsumer("ReceiverPointerGlobalInitializer", true, false);
    receiverPointerConsumer("ReceiverPointerLocalAssignment", false, true);
    receiverPointerConsumer("ReceiverPointerGlobalAssignment", true, true);
    const auto constructorPointerConsumer = [&](std::string_view name, bool global)
    {
        const std::string source = std::string("public class Source { public int* Value; ") +
            "public Source(int* value) { Value = value; } public int* Read() { return Value; } } "
            "public class Program { public static int* Saved; " + (global ? "public static int value; " : "") +
            "public static int Main() { " + (global ? "" : "int value = 42; ") +
            "raii Source source = new Source(&value); int* local = source.Read(); Saved = local; return 0; } }";
        const auto project = writeProject(root / std::string(name), std::string(name), source, "Exe");
        const std::string projectText = project.string();
        const std::array<std::string_view, 3> arguments = {"--dump-project-method-bodies", projectText, "-O0"};
        std::string output;
        std::string error;
        const auto result = runDriver(arguments, output, error);
        tests.expect(global ? result == nativesharp::ExitCode::Success :
            result != nativesharp::ExitCode::Success &&
                (error + output).find("pointer to current-frame storage cannot escape") != std::string::npos,
            std::string(name) + " preserves the pointer origin through a constructor (" + error + output + ")");
    };
    constructorPointerConsumer("ConstructorPointerLocal", false);
    constructorPointerConsumer("ConstructorPointerGlobal", true);
    const auto nestedConstructorConsumer = [&](std::string_view name, bool global)
    {
        const std::string source = std::string("public class Source { public int* Value; ") +
            "public Source(int* value) { Value = value; } public int* Read() { return Value; } } "
            "public class Wrapper { private Source inner; public Wrapper(Source value) { inner = value; } "
            "public int* Read() { return inner.Read(); } } public class Program { public static int* Saved; " +
            (global ? "public static int value; " : "") + "public static int Main() { " +
            (global ? "" : "int value = 42; ") +
            "raii Source source = new Source(&value); raii Wrapper wrapper = new Wrapper(source); "
            "int* local = wrapper.Read(); Saved = local; return 0; } }";
        const auto project = writeProject(root / std::string(name), std::string(name), source, "Exe");
        const std::string projectText = project.string();
        const std::array<std::string_view, 3> arguments = {"--dump-project-method-bodies", projectText, "-O0"};
        std::string output;
        std::string error;
        const auto result = runDriver(arguments, output, error);
        tests.expect(global ? result == nativesharp::ExitCode::Success :
            result != nativesharp::ExitCode::Success &&
                (error + output).find("pointer to current-frame storage cannot escape") != std::string::npos,
            std::string(name) + " preserves the origin through a constructor's object input (" + error + output + ")");
    };
    nestedConstructorConsumer("NestedConstructorLocal", false);
    nestedConstructorConsumer("NestedConstructorGlobal", true);
    const auto binaryConstructorProject = writeProject(root / "binary-constructor-producer", "BinaryConstructorProducer", R"NS(
public class Source { public int* Value; public Source(int* value) { Value = value; }
    public int* Read() { return Value; } }
)NS", "StaticLibrary");
    const std::string binaryConstructorText = binaryConstructorProject.string();
    const std::string binaryConstructorLibrary = (root / "BinaryConstructorProducer.lib").string();
    const std::array<std::string_view, 5> binaryConstructorArguments = {
        "--emit-project-oop-lib", binaryConstructorText, "-O0", "-o", binaryConstructorLibrary};
    std::string binaryConstructorOutput;
    std::string binaryConstructorError;
    const bool binaryConstructorWritten = runDriver(binaryConstructorArguments, binaryConstructorOutput,
        binaryConstructorError) == nativesharp::ExitCode::Success;
    const auto binaryConstructorConsumer = [&](std::string_view name, bool global)
    {
        const std::string source = std::string("public class Program { public static int* Saved; ") +
            (global ? "public static int value; " : "") + "public static int Main() { " +
            (global ? "" : "int value = 42; ") +
            "raii Source source = new Source(&value); int* local = source.Read(); Saved = local; return 0; } }";
        const auto project = writeProject(root / std::string(name), std::string(name), source, "Exe",
            "<LibraryReference Include=\"../BinaryConstructorProducer.lib\" />");
        const std::string projectText = project.string();
        const std::array<std::string_view, 3> arguments = {"--dump-project-method-bodies", projectText, "-O0"};
        std::string output;
        std::string error;
        const auto result = runDriver(arguments, output, error);
        tests.expect(binaryConstructorWritten && (global ? result == nativesharp::ExitCode::Success :
                result != nativesharp::ExitCode::Success &&
                    (error + output).find("current-frame storage") != std::string::npos),
            std::string(name) + " preserves a constructor pointer across a library (" +
                binaryConstructorError + error + output + ")");
    };
    binaryConstructorConsumer("BinaryConstructorLocal", false);
    binaryConstructorConsumer("BinaryConstructorGlobal", true);
    const auto receiverPointerWrapperProject = writeProject(root / "receiver-pointer-wrapper",
        "ReceiverPointerWrapper", R"NS(
public class Source { public int* Value; public int* Read() { return Value; } }
public class Program {
    public static int* Saved;
    public static void Save(Source source) { int* local = source.Read(); Saved = local; }
    public static int Main() { int value = 42; raii Source source = new Source();
        source.Value = &value; Save(source); return 0; }
}
)NS", "Exe");
    const std::string receiverPointerWrapperText = receiverPointerWrapperProject.string();
    const std::array<std::string_view, 3> receiverPointerWrapperArguments = {
        "--dump-project-method-bodies", receiverPointerWrapperText, "-O0"};
    std::string receiverPointerWrapperOutput;
    std::string receiverPointerWrapperError;
    const auto receiverPointerWrapperResult = runDriver(receiverPointerWrapperArguments,
        receiverPointerWrapperOutput, receiverPointerWrapperError);
    tests.expect(receiverPointerWrapperResult != nativesharp::ExitCode::Success &&
            (receiverPointerWrapperError + receiverPointerWrapperOutput).find("escape contract") != std::string::npos,
        "a pointer returned from a receiver keeps its origin through a wrapper (" +
            receiverPointerWrapperError + receiverPointerWrapperOutput + ")");
    const auto receiverOutWrapperProject = writeProject(root / "receiver-out-wrapper", "ReceiverOutWrapper", R"NS(
public class Source { public int* Value; public int* Read() { return Value; } }
public class Program {
    public static int* Saved;
    public static void ReadOut(out int* output, Source source) { int* local = source.Read(); output = local; }
    public static void Save(Source source) { int* local = null; ReadOut(out local, source); Saved = local; }
    public static int Main() { int value = 42; raii Source source = new Source();
        source.Value = &value; Save(source); return 0; }
}
)NS", "Exe");
    const std::string receiverOutWrapperText = receiverOutWrapperProject.string();
    const std::array<std::string_view, 3> receiverOutWrapperArguments = {
        "--dump-project-method-bodies", receiverOutWrapperText, "-O0"};
    std::string receiverOutWrapperOutput;
    std::string receiverOutWrapperError;
    const auto receiverOutWrapperResult = runDriver(receiverOutWrapperArguments,
        receiverOutWrapperOutput, receiverOutWrapperError);
    tests.expect(receiverOutWrapperResult != nativesharp::ExitCode::Success &&
            (receiverOutWrapperError + receiverOutWrapperOutput).find("escape contract") != std::string::npos,
        "a receiver pointer keeps its origin through an out parameter and wrapper (" +
            receiverOutWrapperError + receiverOutWrapperOutput + ")");
    const auto receiverOutGlobalProject = writeProject(root / "receiver-out-global", "ReceiverOutGlobal", R"NS(
public class Source { public int* Value; public int* Read() { return Value; } }
public class Program {
    public static int value; public static int* Saved;
    public static void ReadOut(out int* output, Source source) { int* local = source.Read(); output = local; }
    public static void Save(Source source) { int* local = null; ReadOut(out local, source); Saved = local; }
    public static int Main() { raii Source source = new Source(); source.Value = &value; Save(source); return 0; }
}
)NS", "Exe");
    const std::string receiverOutGlobalText = receiverOutGlobalProject.string();
    const std::array<std::string_view, 3> receiverOutGlobalArguments = {
        "--dump-project-method-bodies", receiverOutGlobalText, "-O0"};
    std::string receiverOutGlobalOutput;
    std::string receiverOutGlobalError;
    const auto receiverOutGlobalResult = runDriver(receiverOutGlobalArguments,
        receiverOutGlobalOutput, receiverOutGlobalError);
    tests.expect(receiverOutGlobalResult == nativesharp::ExitCode::Success,
        "a global pointer through a direct receiver reader and out wrapper is accepted (" +
            receiverOutGlobalError + receiverOutGlobalOutput + ")");
    const auto receiverRetainedProject = writeProject(root / "receiver-retained", "ReceiverRetained", R"NS(
public class Source { public int* Value; public int* Read() { Program.Held = this; return Value; } }
public class Program {
    public static Source Held; public static int value; public static int* Saved;
    public static void Save(Source source) { int* local = source.Read(); Saved = local; }
    public static int Main() { raii Source source = new Source(); source.Value = &value; Save(source); return 0; }
}
)NS", "Exe");
    const std::string receiverRetainedText = receiverRetainedProject.string();
    const std::array<std::string_view, 3> receiverRetainedArguments = {
        "--dump-project-method-bodies", receiverRetainedText, "-O0"};
    std::string receiverRetainedOutput;
    std::string receiverRetainedError;
    const auto receiverRetainedResult = runDriver(receiverRetainedArguments,
        receiverRetainedOutput, receiverRetainedError);
    tests.expect(receiverRetainedResult != nativesharp::ExitCode::Success &&
            (receiverRetainedError + receiverRetainedOutput).find("escape contract") != std::string::npos,
        "a receiver reader that retains the object is not certified (" +
            receiverRetainedError + receiverRetainedOutput + ")");
    const auto objectBinaryOutWrapperProject = writeProject(root / "object-pointer-binary-out-wrapper",
        "ObjectPointerBinaryOutWrapper", R"NS(
public class Program {
    public static int* Saved;
    public static void Save(Source source) { int* local = null; Api.Seed(out local, source); Saved = local; }
    public static int Main() { int value = 42; raii Source source = new Source();
        source.Value = &value; Save(source); return 0; }
}
)NS", "Exe", "<LibraryReference Include=\"../ObjectPointerBinaryProducer.lib\" />");
    const std::string objectBinaryOutWrapperText = objectBinaryOutWrapperProject.string();
    const std::array<std::string_view, 3> objectBinaryOutWrapperArguments = {
        "--dump-project-method-bodies", objectBinaryOutWrapperText, "-O0"};
    std::string objectBinaryOutWrapperOutput;
    std::string objectBinaryOutWrapperError;
    const auto objectBinaryOutWrapperResult = runDriver(objectBinaryOutWrapperArguments,
        objectBinaryOutWrapperOutput, objectBinaryOutWrapperError);
    tests.expect(objectBinaryWritten && objectBinaryOutWrapperResult != nativesharp::ExitCode::Success &&
            (objectBinaryOutWrapperError + objectBinaryOutWrapperOutput).find("escape contract") != std::string::npos,
        "an object pointer returned by a binary method retains its origin through a wrapper (" +
            objectBinaryOutWrapperError + objectBinaryOutWrapperOutput + ")");
    const std::filesystem::path refSlotLoopProject = writeProject(
        root / "ref-slot-loop", "EscapeRefSlotLoop", R"NS(
using EscapeContracts;
public class Program { public static int Main() { int value = 42; int* local = null; int i = 0;
    while (i < 2) { if (i == 1) { local = &value; } EscapeApi.ReadRef(ref local); i = i + 1; } return 0; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.ref-slot.nsmeta\" />");
    const std::string refSlotLoopProjectText = refSlotLoopProject.string();
    const std::array<std::string_view, 3> refSlotLoopArguments = {
        "--dump-project-method-bodies", refSlotLoopProjectText, "-O0"};
    std::string refSlotLoopOutput;
    std::string refSlotLoopError;
    const auto refSlotLoopResult = runDriver(refSlotLoopArguments, refSlotLoopOutput, refSlotLoopError);
    tests.expect(refSlotWritten && refSlotLoopResult != nativesharp::ExitCode::Success &&
            (refSlotLoopError + refSlotLoopOutput).find("unknown escape contract") != std::string::npos,
        "a repeated call cannot retain a pointer assigned from the frame (" + refSlotLoopError + refSlotLoopOutput + ")");
    const std::filesystem::path refSlotUnknownReturnProject = writeProject(
        root / "ref-slot-unknown-return", "EscapeRefSlotUnknownReturn", R"NS(
using EscapeContracts;
public class Program { public static int Main() { int* local = null; local = EscapeApi.Identity(local);
    EscapeApi.ReadRef(ref local); return 0; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.ref-slot.nsmeta\" />");
    const std::string refSlotUnknownReturnProjectText = refSlotUnknownReturnProject.string();
    const std::array<std::string_view, 3> refSlotUnknownReturnArguments = {
        "--dump-project-method-bodies", refSlotUnknownReturnProjectText, "-O0"};
    std::string refSlotUnknownReturnOutput;
    std::string refSlotUnknownReturnError;
    const auto refSlotUnknownReturnResult = runDriver(
        refSlotUnknownReturnArguments, refSlotUnknownReturnOutput, refSlotUnknownReturnError);
    tests.expect(refSlotWritten && refSlotUnknownReturnResult != nativesharp::ExitCode::Success &&
            (refSlotUnknownReturnError + refSlotUnknownReturnOutput).find("unknown escape contract") != std::string::npos,
        "an unproven pointer-returning call cannot establish the value's origin (" +
            refSlotUnknownReturnError + refSlotUnknownReturnOutput + ")");
    const std::filesystem::path unknownRefGlobalProject = writeProject(
        root / "unknown-ref-global", "EscapeUnknownRefGlobal", R"NS(
using EscapeContracts;
public class Program { public static int* global; public static int Main() { EscapeApi.ReadRef(ref global); return 0; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownRefGlobalProjectText = unknownRefGlobalProject.string();
    const std::array<std::string_view, 3> unknownRefGlobalArguments = {
        "--dump-project-method-bodies", unknownRefGlobalProjectText, "-O0"};
    std::string unknownRefGlobalOutput;
    std::string unknownRefGlobalError;
    const auto unknownRefGlobalResult = runDriver(
        unknownRefGlobalArguments, unknownRefGlobalOutput, unknownRefGlobalError);
    tests.expect(unknownWritten && unknownRefGlobalResult == nativesharp::ExitCode::Success,
        "an unknown ref-pointer contract accepts a global pointer slot (" +
            unknownRefGlobalError + unknownRefGlobalOutput + ")");
    const std::filesystem::path unknownRefStackProject = writeProject(
        root / "unknown-ref-stack-value", "EscapeUnknownRefStackValue", R"NS(
using EscapeContracts;
public class Program {
    public static int Main() { int value = 42; int* local = &value; EscapeApi.ReadRef(ref local); return value; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownRefStackProjectText = unknownRefStackProject.string();
    const std::array<std::string_view, 3> unknownRefStackArguments = {
        "--dump-project-method-bodies", unknownRefStackProjectText, "-O0"};
    std::string unknownRefStackOutput;
    std::string unknownRefStackError;
    const auto unknownRefStackResult = runDriver(
        unknownRefStackArguments, unknownRefStackOutput, unknownRefStackError);
    tests.expect(unknownWritten && unknownRefStackResult != nativesharp::ExitCode::Success &&
            (unknownRefStackError + unknownRefStackOutput).find("unknown escape contract") != std::string::npos,
        "an unknown ref-pointer contract rejects a slot containing a frame address (" +
            unknownRefStackError + unknownRefStackOutput + ")");
    const std::filesystem::path refForwardProject = writeProject(
        root / "ref-pointer-forward", "EscapeRefPointerForward", R"NS(
using EscapeContracts;
public class Program {
    public static void Set(ref int* target, int* value) { target = value; }
    public static int Main() { int value = 42; int* local = null; Set(ref local, &value); EscapeApi.Retain(local); return 0; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string refForwardProjectText = refForwardProject.string();
    const std::array<std::string_view, 3> refForwardArguments = {
        "--dump-project-method-bodies", refForwardProjectText, "-O0"};
    std::string refForwardOutput;
    std::string refForwardError;
    const auto refForwardResult = runDriver(refForwardArguments, refForwardOutput, refForwardError);
    tests.expect(unknownWritten && refForwardResult != nativesharp::ExitCode::Success &&
            (refForwardError + refForwardOutput).find("escape contract") != std::string::npos,
        "a frame pointer forwarded through a ref slot keeps its origin (" +
            refForwardError + refForwardOutput + ")");
    const std::filesystem::path refForwardChainProject = writeProject(
        root / "ref-pointer-forward-chain", "EscapeRefPointerForwardChain", R"NS(
using EscapeContracts;
public class Program {
    public static void Set(ref int* target, int* value) { target = value; }
    public static int Main() { int value = 42; int* first = null; int* second = null;
        Set(ref first, &value); Set(ref second, first); EscapeApi.Retain(second); return 0; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string refForwardChainProjectText = refForwardChainProject.string();
    const std::array<std::string_view, 3> refForwardChainArguments = {
        "--dump-project-method-bodies", refForwardChainProjectText, "-O0"};
    std::string refForwardChainOutput;
    std::string refForwardChainError;
    const auto refForwardChainResult = runDriver(refForwardChainArguments, refForwardChainOutput, refForwardChainError);
    tests.expect(unknownWritten && refForwardChainResult != nativesharp::ExitCode::Success &&
            (refForwardChainError + refForwardChainOutput).find("escape contract") != std::string::npos,
        "a frame pointer keeps its origin through two ref slots (" + refForwardChainError + refForwardChainOutput + ")");
    const std::filesystem::path refForwardGlobalProject = writeProject(
        root / "ref-pointer-forward-global", "EscapeRefPointerForwardGlobal", R"NS(
using EscapeContracts;
public class Program {
    public static int value;
    public static void Set(ref int* target, int* source) { target = source; }
    public static int Main() { int* local = null; Set(ref local, &value); EscapeApi.Retain(local); return 0; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string refForwardGlobalProjectText = refForwardGlobalProject.string();
    const std::array<std::string_view, 3> refForwardGlobalArguments = {
        "--dump-project-method-bodies", refForwardGlobalProjectText, "-O0"};
    std::string refForwardGlobalOutput;
    std::string refForwardGlobalError;
    const auto refForwardGlobalResult = runDriver(refForwardGlobalArguments, refForwardGlobalOutput, refForwardGlobalError);
    tests.expect(unknownWritten && refForwardGlobalResult == nativesharp::ExitCode::Success,
        "a global address forwarded through a ref slot remains accepted (" +
            refForwardGlobalError + refForwardGlobalOutput + ")");
    const std::filesystem::path refForwardRefProject = writeProject(
        root / "ref-pointer-forward-ref", "EscapeRefPointerForwardRef", R"NS(
using EscapeContracts;
public class Program {
    public static void Set(ref int* target, ref int* source) { target = source; }
    public static int Main() { int value = 42; int* first = &value; int* second = null;
        Set(ref second, ref first); EscapeApi.Retain(second); return 0; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string refForwardRefProjectText = refForwardRefProject.string();
    const std::array<std::string_view, 3> refForwardRefArguments = {
        "--dump-project-method-bodies", refForwardRefProjectText, "-O0"};
    std::string refForwardRefOutput;
    std::string refForwardRefError;
    const auto refForwardRefResult = runDriver(refForwardRefArguments, refForwardRefOutput, refForwardRefError);
    tests.expect(unknownWritten && refForwardRefResult != nativesharp::ExitCode::Success &&
            (refForwardRefError + refForwardRefOutput).find("escape contract") != std::string::npos,
        "a frame pointer copied between ref arguments keeps its origin (" +
            refForwardRefError + refForwardRefOutput + ")");
    const std::filesystem::path outForwardProject = writeProject(
        root / "out-pointer-forward", "EscapeOutPointerForward", R"NS(
using EscapeContracts;
public class Program {
    public static void Set(out int* target, int* source) { target = source; }
    public static int Main() { int value = 42; int* local = null;
        Set(out local, &value); EscapeApi.Retain(local); return 0; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string outForwardProjectText = outForwardProject.string();
    const std::array<std::string_view, 3> outForwardArguments = {
        "--dump-project-method-bodies", outForwardProjectText, "-O0"};
    std::string outForwardOutput;
    std::string outForwardError;
    const auto outForwardResult = runDriver(outForwardArguments, outForwardOutput, outForwardError);
    tests.expect(unknownWritten && outForwardResult != nativesharp::ExitCode::Success &&
            (outForwardError + outForwardOutput).find("escape contract") != std::string::npos,
        "a frame pointer copied through an out argument keeps its origin (" +
            outForwardError + outForwardOutput + ")");
    const std::filesystem::path unknownProject = writeProject(
        root / "unknown", "EscapeUnknown", R"NS(
using EscapeContracts;
public class Program { public static int Main() { int local = 42; EscapeApi.Retain(&local); return local; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownProjectText = unknownProject.string();
    const std::array<std::string_view, 3> unknownArguments = {
        "--dump-project-method-bodies", unknownProjectText, "-O0"};
    std::string unknownOutput;
    std::string unknownError;
    const nativesharp::ExitCode unknownResult = runDriver(unknownArguments, unknownOutput, unknownError);
    tests.expect(unknownWritten && unknownResult != nativesharp::ExitCode::Success &&
            (unknownError + unknownOutput).find("escape contract") != std::string::npos,
        "an unknown native escape contract rejects current-frame storage (" + unknownError + unknownOutput + ")");
    const std::filesystem::path unknownAliasProject = writeProject(
        root / "unknown-alias", "EscapeUnknownAlias", R"NS(
using EscapeContracts;
public class Program { public static int Main() { int local = 42; int* alias = &local; EscapeApi.Retain(alias); return local; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownAliasProjectText = unknownAliasProject.string();
    const std::array<std::string_view, 3> unknownAliasArguments = {
        "--dump-project-method-bodies", unknownAliasProjectText, "-O0"};
    std::string unknownAliasOutput;
    std::string unknownAliasError;
    const nativesharp::ExitCode unknownAliasResult = runDriver(unknownAliasArguments, unknownAliasOutput, unknownAliasError);
    tests.expect(unknownWritten && unknownAliasResult != nativesharp::ExitCode::Success &&
            (unknownAliasError + unknownAliasOutput).find("escape contract") != std::string::npos,
        "an unknown native escape contract rejects an alias to frame storage (" +
            unknownAliasError + unknownAliasOutput + ")");
    const std::filesystem::path unknownFunctionProject = writeProject(
        root / "unknown-function", "EscapeUnknownFunction", R"NS(
using EscapeContracts;
public class Program { public static int Main() { int local = 42; RetainFunction(&local); return local; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownFunctionProjectText = unknownFunctionProject.string();
    const std::array<std::string_view, 3> unknownFunctionArguments = {
        "--dump-project-method-bodies", unknownFunctionProjectText, "-O0"};
    std::string unknownFunctionOutput;
    std::string unknownFunctionError;
    const nativesharp::ExitCode unknownFunctionResult =
        runDriver(unknownFunctionArguments, unknownFunctionOutput, unknownFunctionError);
    tests.expect(unknownWritten && unknownFunctionResult != nativesharp::ExitCode::Success &&
            (unknownFunctionError + unknownFunctionOutput).find("escape contract") != std::string::npos,
        "an unknown native function escape contract rejects current-frame storage (" +
            unknownFunctionError + unknownFunctionOutput + ")");
    const std::filesystem::path unknownForwardProject = writeProject(
        root / "unknown-forward", "EscapeUnknownForward", R"NS(
using EscapeContracts;
public class Program {
    static void Forward(int* value) { EscapeApi.Retain(value); }
    public static int Main() { int local = 42; Forward(&local); return local; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::filesystem::path forwardLibraryProject = writeProject(
        root / "forward-library", "ForwardLibrary", R"NS(
using EscapeContracts;
public class Forwarder { public static void Forward(int* value) { EscapeApi.Retain(value); } }
)NS", "StaticLibrary", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string forwardLibraryProjectText = forwardLibraryProject.string();
    const std::string forwardLibraryPath = (root / "ForwardLibrary.lib").string();
    const std::array<std::string_view, 5> forwardLibraryArguments = {
        "--emit-project-oop-lib", forwardLibraryProjectText, "-O0", "-o", forwardLibraryPath};
    std::string forwardLibraryOutput;
    std::string forwardLibraryError;
    nativesharp::interop::LibraryMetadata forwardLibraryMetadata;
    const bool forwardLibraryRead = runDriver(forwardLibraryArguments, forwardLibraryOutput, forwardLibraryError) ==
        nativesharp::ExitCode::Success && codec.read(root / "ForwardLibrary.nsmeta", forwardLibraryMetadata, metadataError);
    const auto forwardMember = std::ranges::find_if(forwardLibraryMetadata.members, [](const auto& candidate)
        { return candidate.name == "Forward"; });
    tests.expect(forwardLibraryRead && forwardMember != forwardLibraryMetadata.members.end() &&
            forwardMember->parameterEscapeSummary == std::vector<nativesharp::interop::LibraryMetadataEscapeClass>{
                nativesharp::interop::LibraryMetadataEscapeClass::Unknown},
        "forwarding preserves the unknown escape summary (" + forwardLibraryError + forwardLibraryOutput + metadataError + ")");
    const std::string unknownForwardProjectText = unknownForwardProject.string();
    const std::array<std::string_view, 3> unknownForwardArguments = {
        "--dump-project-method-bodies", unknownForwardProjectText, "-O0"};
    std::string unknownForwardOutput;
    std::string unknownForwardError;
    const nativesharp::ExitCode unknownForwardResult =
        runDriver(unknownForwardArguments, unknownForwardOutput, unknownForwardError);
    tests.expect(unknownWritten && unknownForwardResult != nativesharp::ExitCode::Success &&
            (unknownForwardError + unknownForwardOutput).find("escape contract") != std::string::npos,
        "a forwarded unknown native contract rejects current-frame storage (" +
            unknownForwardError + unknownForwardOutput + ")");
    const std::filesystem::path unknownForwardAliasProject = writeProject(
        root / "unknown-forward-alias", "EscapeUnknownForwardAlias", R"NS(
using EscapeContracts;
public class Program {
    static void Forward(int* value) { int* alias = value; EscapeApi.Retain(alias); }
    public static int Main() { int local = 42; Forward(&local); return local; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownForwardAliasProjectText = unknownForwardAliasProject.string();
    const std::array<std::string_view, 3> unknownForwardAliasArguments = {
        "--dump-project-method-bodies", unknownForwardAliasProjectText, "-O0"};
    std::string unknownForwardAliasOutput;
    std::string unknownForwardAliasError;
    const nativesharp::ExitCode unknownForwardAliasResult =
        runDriver(unknownForwardAliasArguments, unknownForwardAliasOutput, unknownForwardAliasError);
    tests.expect(unknownWritten && unknownForwardAliasResult != nativesharp::ExitCode::Success &&
            (unknownForwardAliasError + unknownForwardAliasOutput).find("escape contract") != std::string::npos,
        "a local pointer alias preserves a forwarded unknown native contract (" +
            unknownForwardAliasError + unknownForwardAliasOutput + ")");
    const std::filesystem::path unknownForwardMultipleProject = writeProject(
        root / "unknown-forward-multiple", "EscapeUnknownForwardMultiple", R"NS(
using EscapeContracts;
public class Program { public static int Data;
    static void Forward(int* first, int* second) { int* alias = first; alias = second; EscapeApi.Retain(alias); }
    public static int Main() { int local = 42; Forward(&Data, &local); return local; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownForwardMultipleProjectText = unknownForwardMultipleProject.string();
    const std::array<std::string_view, 3> unknownForwardMultipleArguments = {
        "--dump-project-method-bodies", unknownForwardMultipleProjectText, "-O0"};
    std::string unknownForwardMultipleOutput;
    std::string unknownForwardMultipleError;
    const nativesharp::ExitCode unknownForwardMultipleResult =
        runDriver(unknownForwardMultipleArguments, unknownForwardMultipleOutput, unknownForwardMultipleError);
    tests.expect(unknownWritten && unknownForwardMultipleResult != nativesharp::ExitCode::Success &&
            (unknownForwardMultipleError + unknownForwardMultipleOutput).find("escape contract") != std::string::npos,
        "a reassigned pointer alias preserves every possible parameter origin (" +
            unknownForwardMultipleError + unknownForwardMultipleOutput + ")");
    const std::filesystem::path unknownForwardChainProject = writeProject(
        root / "unknown-forward-chain", "EscapeUnknownForwardChain", R"NS(
using EscapeContracts;
public class Program { public static int Data;
    static void Forward(int* first, int* second) { int* one = first; one = second; int* two = one; EscapeApi.Retain(two); }
    public static int Main() { int local = 42; Forward(&Data, &local); return local; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownForwardChainProjectText = unknownForwardChainProject.string();
    const std::array<std::string_view, 3> unknownForwardChainArguments = {
        "--dump-project-method-bodies", unknownForwardChainProjectText, "-O0"};
    std::string unknownForwardChainOutput;
    std::string unknownForwardChainError;
    const nativesharp::ExitCode unknownForwardChainResult =
        runDriver(unknownForwardChainArguments, unknownForwardChainOutput, unknownForwardChainError);
    tests.expect(unknownWritten && unknownForwardChainResult != nativesharp::ExitCode::Success &&
            (unknownForwardChainError + unknownForwardChainOutput).find("escape contract") != std::string::npos,
        "pointer alias chains conservatively preserve parameter sources (" +
            unknownForwardChainError + unknownForwardChainOutput + ")");
    const std::filesystem::path unknownForwardBranchProject = writeProject(
        root / "unknown-forward-branch", "EscapeUnknownForwardBranch", R"NS(
using EscapeContracts;
public class Program { public static int Data;
    static void Forward(int* first, int* second) { int* alias = first; if (true) { alias = second; } EscapeApi.Retain(alias); }
    public static int Main() { int local = 42; Forward(&Data, &local); return local; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownForwardBranchProjectText = unknownForwardBranchProject.string();
    const std::array<std::string_view, 3> unknownForwardBranchArguments = {
        "--dump-project-method-bodies", unknownForwardBranchProjectText, "-O0"};
    std::string unknownForwardBranchOutput;
    std::string unknownForwardBranchError;
    const nativesharp::ExitCode unknownForwardBranchResult =
        runDriver(unknownForwardBranchArguments, unknownForwardBranchOutput, unknownForwardBranchError);
    tests.expect(unknownWritten && unknownForwardBranchResult != nativesharp::ExitCode::Success &&
            (unknownForwardBranchError + unknownForwardBranchOutput).find("escape contract") != std::string::npos,
        "pointer aliases preserve branch parameter sources (" +
            unknownForwardBranchError + unknownForwardBranchOutput + ")");
    const std::filesystem::path unknownForwardArithmeticProject = writeProject(
        root / "unknown-forward-arithmetic", "EscapeUnknownForwardArithmetic", R"NS(
using EscapeContracts;
public class Program { public static int Data;
    static void Forward(int* first, int* second) { int* alias = first; alias = second + 0; EscapeApi.Retain(alias); }
    public static int Main() { int local = 42; Forward(&Data, &local); return local; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownForwardArithmeticProjectText = unknownForwardArithmeticProject.string();
    const std::array<std::string_view, 3> unknownForwardArithmeticArguments = {
        "--dump-project-method-bodies", unknownForwardArithmeticProjectText, "-O0"};
    std::string unknownForwardArithmeticOutput;
    std::string unknownForwardArithmeticError;
    const nativesharp::ExitCode unknownForwardArithmeticResult =
        runDriver(unknownForwardArithmeticArguments, unknownForwardArithmeticOutput, unknownForwardArithmeticError);
    tests.expect(unknownWritten && unknownForwardArithmeticResult != nativesharp::ExitCode::Success &&
            (unknownForwardArithmeticError + unknownForwardArithmeticOutput).find("escape contract") != std::string::npos,
        "pointer aliases preserve arithmetic parameter sources (" +
            unknownForwardArithmeticError + unknownForwardArithmeticOutput + ")");
    const std::filesystem::path unknownForwardConditionalProject = writeProject(
        root / "unknown-forward-conditional", "EscapeUnknownForwardConditional", R"NS(
using EscapeContracts;
public class Program { public static int Data;
    static void Forward(int* first, int* second) { int* alias = false ? first : second; EscapeApi.Retain(alias); }
    public static int Main() { int local = 42; Forward(&Data, &local); return local; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownForwardConditionalProjectText = unknownForwardConditionalProject.string();
    const std::array<std::string_view, 3> unknownForwardConditionalArguments = {
        "--dump-project-method-bodies", unknownForwardConditionalProjectText, "-O0"};
    std::string unknownForwardConditionalOutput;
    std::string unknownForwardConditionalError;
    const nativesharp::ExitCode unknownForwardConditionalResult =
        runDriver(unknownForwardConditionalArguments, unknownForwardConditionalOutput, unknownForwardConditionalError);
    tests.expect(unknownWritten && unknownForwardConditionalResult != nativesharp::ExitCode::Success &&
            (unknownForwardConditionalError + unknownForwardConditionalOutput).find("escape contract") != std::string::npos,
        "pointer aliases preserve conditional parameter sources (" +
            unknownForwardConditionalError + unknownForwardConditionalOutput + ")");
    const std::filesystem::path unknownDirectConditionalProject = writeProject(
        root / "unknown-direct-conditional", "EscapeUnknownDirectConditional", R"NS(
using EscapeContracts;
public class Program { public static int Data;
    static void Forward(int* first, int* second) { EscapeApi.Retain(false ? first : second); }
    public static int Main() { int local = 42; Forward(&Data, &local); return local; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownDirectConditionalProjectText = unknownDirectConditionalProject.string();
    const std::array<std::string_view, 3> unknownDirectConditionalArguments = {
        "--dump-project-method-bodies", unknownDirectConditionalProjectText, "-O0"};
    std::string unknownDirectConditionalOutput;
    std::string unknownDirectConditionalError;
    const nativesharp::ExitCode unknownDirectConditionalResult =
        runDriver(unknownDirectConditionalArguments, unknownDirectConditionalOutput, unknownDirectConditionalError);
    tests.expect(unknownWritten && unknownDirectConditionalResult != nativesharp::ExitCode::Success &&
            (unknownDirectConditionalError + unknownDirectConditionalOutput).find("escape contract") != std::string::npos,
        "a direct conditional pointer preserves both parameter origins (" +
            unknownDirectConditionalError + unknownDirectConditionalOutput + ")");
    const std::filesystem::path directConditionalControlsProject = writeProject(
        root / "direct-conditional-controls", "EscapeDirectConditionalControls", R"NS(
using EscapeContracts;
public class Program { public static int Data;
    static void Forward(int* ignored) { EscapeApi.Retain(false ? &Data : &Data); }
    public static int Main() { int local = 42; Forward(&local); return local; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string directConditionalControlsText = directConditionalControlsProject.string();
    const std::array<std::string_view, 3> directConditionalControlsArguments = {
        "--dump-project-method-bodies", directConditionalControlsText, "-O0"};
    std::string directConditionalControlsOutput;
    std::string directConditionalControlsError;
    tests.expect(unknownWritten && runDriver(directConditionalControlsArguments, directConditionalControlsOutput,
            directConditionalControlsError) == nativesharp::ExitCode::Success,
        "a conditional of global pointers does not retain an unrelated parameter (" +
            directConditionalControlsError + directConditionalControlsOutput + ")");
    const std::filesystem::path directConditionalLocalProject = writeProject(
        root / "direct-conditional-local", "EscapeDirectConditionalLocal", R"NS(
using EscapeContracts;
public class Program { public static int Data;
    public static int Main() { int local = 42; EscapeApi.Retain(false ? &Data : &local); return local; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string directConditionalLocalText = directConditionalLocalProject.string();
    const std::array<std::string_view, 3> directConditionalLocalArguments = {
        "--dump-project-method-bodies", directConditionalLocalText, "-O0"};
    std::string directConditionalLocalOutput;
    std::string directConditionalLocalError;
    const nativesharp::ExitCode directConditionalLocalResult =
        runDriver(directConditionalLocalArguments, directConditionalLocalOutput, directConditionalLocalError);
    tests.expect(unknownWritten && directConditionalLocalResult != nativesharp::ExitCode::Success &&
            (directConditionalLocalError + directConditionalLocalOutput).find("escape contract") != std::string::npos,
        "a direct conditional cannot retain local storage (" + directConditionalLocalError + directConditionalLocalOutput + ")");
    const std::filesystem::path arithmeticConditionalProject = writeProject(
        root / "arithmetic-conditional-local", "EscapeArithmeticConditionalLocal", R"NS(
using EscapeContracts;
public class Program { public static int Data;
    public static int Main() { int local = 42; EscapeApi.Retain((false ? &Data : &local) + 0); return local; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string arithmeticConditionalText = arithmeticConditionalProject.string();
    const std::array<std::string_view, 3> arithmeticConditionalArguments = {
        "--dump-project-method-bodies", arithmeticConditionalText, "-O0"};
    std::string arithmeticConditionalOutput;
    std::string arithmeticConditionalError;
    const nativesharp::ExitCode arithmeticConditionalResult =
        runDriver(arithmeticConditionalArguments, arithmeticConditionalOutput, arithmeticConditionalError);
    tests.expect(unknownWritten && arithmeticConditionalResult != nativesharp::ExitCode::Success &&
            (arithmeticConditionalError + arithmeticConditionalOutput).find("escape contract") != std::string::npos,
        "pointer arithmetic preserves conditional local storage (" + arithmeticConditionalError + arithmeticConditionalOutput + ")");
    const std::filesystem::path reversedArithmeticProject = writeProject(
        root / "reversed-arithmetic-conditional", "EscapeReversedArithmeticConditional", R"NS(
using EscapeContracts;
public class Program { public static int Data;
    public static int Main() { int local = 42; EscapeApi.Retain(0 + (false ? &Data : &local)); return local; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string reversedArithmeticText = reversedArithmeticProject.string();
    const std::array<std::string_view, 3> reversedArithmeticArguments = {
        "--dump-project-method-bodies", reversedArithmeticText, "-O0"};
    std::string reversedArithmeticOutput;
    std::string reversedArithmeticError;
    const nativesharp::ExitCode reversedArithmeticResult =
        runDriver(reversedArithmeticArguments, reversedArithmeticOutput, reversedArithmeticError);
    tests.expect(unknownWritten && reversedArithmeticResult != nativesharp::ExitCode::Success &&
            (reversedArithmeticError + reversedArithmeticOutput).find("escape contract") != std::string::npos,
        "integer plus pointer preserves conditional local storage (" + reversedArithmeticError + reversedArithmeticOutput + ")");
    const std::filesystem::path arithmeticGlobalProject = writeProject(
        root / "arithmetic-conditional-global", "EscapeArithmeticConditionalGlobal", R"NS(
using EscapeContracts;
public class Program { public static int Data;
    public static int Main() { int offset = 0; EscapeApi.Retain((false ? &Data : &Data) + offset); EscapeApi.Retain(offset + (false ? &Data : &Data)); EscapeApi.Retain((false ? &Data : &Data) - offset); return 42; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string arithmeticGlobalText = arithmeticGlobalProject.string();
    const std::array<std::string_view, 3> arithmeticGlobalArguments = {
        "--dump-project-method-bodies", arithmeticGlobalText, "-O0"};
    std::string arithmeticGlobalOutput;
    std::string arithmeticGlobalError;
    tests.expect(unknownWritten && runDriver(arithmeticGlobalArguments, arithmeticGlobalOutput, arithmeticGlobalError) ==
            nativesharp::ExitCode::Success,
        "pointer arithmetic permits a conditional of globals (" + arithmeticGlobalError + arithmeticGlobalOutput + ")");
    const std::filesystem::path unknownForwardIgnoredProject = writeProject(
        root / "unknown-forward-ignored", "EscapeUnknownForwardIgnored", R"NS(
using EscapeContracts;
public class Program { public static int Data;
    static void Forward(int* ignored) { int* alias = &Data; EscapeApi.Retain(alias); }
    public static int Main() { int local = 42; Forward(&local); return local; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownForwardIgnoredProjectText = unknownForwardIgnoredProject.string();
    const std::array<std::string_view, 3> unknownForwardIgnoredArguments = {
        "--dump-project-method-bodies", unknownForwardIgnoredProjectText, "-O0"};
    std::string unknownForwardIgnoredOutput;
    std::string unknownForwardIgnoredError;
    tests.expect(unknownWritten && runDriver(unknownForwardIgnoredArguments, unknownForwardIgnoredOutput,
            unknownForwardIgnoredError) == nativesharp::ExitCode::Success,
        "an ignored pointer parameter does not taint a global alias (" +
            unknownForwardIgnoredError + unknownForwardIgnoredOutput + ")");
    const std::filesystem::path unknownForwardAliasGlobalProject = writeProject(
        root / "unknown-forward-alias-global", "EscapeUnknownForwardAliasGlobal", R"NS(
using EscapeContracts;
public class Program { public static int Data;
    static void Forward(int* value) { int* alias = value; EscapeApi.Retain(alias); }
    public static int Main() { Forward(&Data); return 42; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownForwardAliasGlobalProjectText = unknownForwardAliasGlobalProject.string();
    const std::array<std::string_view, 3> unknownForwardAliasGlobalArguments = {
        "--dump-project-method-bodies", unknownForwardAliasGlobalProjectText, "-O0"};
    std::string unknownForwardAliasGlobalOutput;
    std::string unknownForwardAliasGlobalError;
    tests.expect(unknownWritten && runDriver(unknownForwardAliasGlobalArguments, unknownForwardAliasGlobalOutput,
            unknownForwardAliasGlobalError) == nativesharp::ExitCode::Success,
        "a forwarded pointer alias permits global storage (" +
            unknownForwardAliasGlobalError + unknownForwardAliasGlobalOutput + ")");
    const std::filesystem::path safeForwardAliasProject = writeProject(
        root / "safe-forward-alias", "EscapeSafeForwardAlias", R"NS(
using EscapeContracts;
public class Program {
    static int Forward(int* value) { int* alias = value; return EscapeApi.Read(alias); }
    public static int Main() { int local = 42; return Forward(&local); }
}
)NS", "Exe", producerReference);
    const std::string safeForwardAliasProjectText = safeForwardAliasProject.string();
    const std::array<std::string_view, 3> safeForwardAliasArguments = {
        "--dump-project-method-bodies", safeForwardAliasProjectText, "-O0"};
    std::string safeForwardAliasOutput;
    std::string safeForwardAliasError;
    tests.expect(runDriver(safeForwardAliasArguments, safeForwardAliasOutput, safeForwardAliasError) ==
            nativesharp::ExitCode::Success,
        "a forwarded pointer alias preserves a known nonescaping contract (" +
            safeForwardAliasError + safeForwardAliasOutput + ")");
    const std::filesystem::path unknownForwardFunctionProject = writeProject(
        root / "unknown-forward-function", "EscapeUnknownForwardFunction", R"NS(
using EscapeContracts;
public class Program {
    static void First(int* value) { RetainFunction(value); }
    static void Second(int* value) { First(value); }
    public static int Main() { int local = 42; Second(&local); return local; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownForwardFunctionProjectText = unknownForwardFunctionProject.string();
    const std::array<std::string_view, 3> unknownForwardFunctionArguments = {
        "--dump-project-method-bodies", unknownForwardFunctionProjectText, "-O0"};
    std::string unknownForwardFunctionOutput;
    std::string unknownForwardFunctionError;
    const nativesharp::ExitCode unknownForwardFunctionResult =
        runDriver(unknownForwardFunctionArguments, unknownForwardFunctionOutput, unknownForwardFunctionError);
    tests.expect(unknownWritten && unknownForwardFunctionResult != nativesharp::ExitCode::Success &&
            (unknownForwardFunctionError + unknownForwardFunctionOutput).find("escape contract") != std::string::npos,
        "two wrappers preserve an unknown native function contract (" +
            unknownForwardFunctionError + unknownForwardFunctionOutput + ")");
    const std::filesystem::path unknownForwardGlobalProject = writeProject(
        root / "unknown-forward-global", "EscapeUnknownForwardGlobal", R"NS(
using EscapeContracts;
public class Program {
    public static int Data;
    static void Forward(int* value) { EscapeApi.Retain(value); }
    public static int Main() { Forward(&Data); return 42; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownForwardGlobalProjectText = unknownForwardGlobalProject.string();
    const std::array<std::string_view, 3> unknownForwardGlobalArguments = {
        "--dump-project-method-bodies", unknownForwardGlobalProjectText, "-O0"};
    std::string unknownForwardGlobalOutput;
    std::string unknownForwardGlobalError;
    tests.expect(unknownWritten && runDriver(unknownForwardGlobalArguments, unknownForwardGlobalOutput,
            unknownForwardGlobalError) == nativesharp::ExitCode::Success,
        "a forwarded unknown native contract permits global storage (" +
            unknownForwardGlobalError + unknownForwardGlobalOutput + ")");
    const std::filesystem::path unknownGlobalProject = writeProject(
        root / "unknown-global", "EscapeUnknownGlobal", R"NS(
using EscapeContracts;
public class Program { public static int Data; public static int Main() { EscapeApi.Retain(&Data); return 42; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownGlobalProjectText = unknownGlobalProject.string();
    const std::array<std::string_view, 3> unknownGlobalArguments = {
        "--dump-project-method-bodies", unknownGlobalProjectText, "-O0"};
    std::string unknownGlobalOutput;
    std::string unknownGlobalError;
    tests.expect(unknownWritten && runDriver(unknownGlobalArguments, unknownGlobalOutput, unknownGlobalError) ==
            nativesharp::ExitCode::Success,
        "an unknown native escape contract permits global storage (" + unknownGlobalError + unknownGlobalOutput + ")");
    const std::filesystem::path unknownFunctionGlobalProject = writeProject(
        root / "unknown-function-global", "EscapeUnknownFunctionGlobal", R"NS(
using EscapeContracts;
public class Program { public static int Data; public static int Main() { RetainFunction(&Data); return 42; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownFunctionGlobalProjectText = unknownFunctionGlobalProject.string();
    const std::array<std::string_view, 3> unknownFunctionGlobalArguments = {
        "--dump-project-method-bodies", unknownFunctionGlobalProjectText, "-O0"};
    std::string unknownFunctionGlobalOutput;
    std::string unknownFunctionGlobalError;
    tests.expect(unknownWritten && runDriver(unknownFunctionGlobalArguments, unknownFunctionGlobalOutput,
            unknownFunctionGlobalError) == nativesharp::ExitCode::Success,
        "an unknown native function escape contract permits global storage (" +
            unknownFunctionGlobalError + unknownFunctionGlobalOutput + ")");
    const auto externalProject = writeProject(root / "late-external", "LateExternal", R"NS(
using EscapeContracts;
class Item { public int Read() { return 1; } }
class Flow {
    public static void Store(Item value, Item[] refs) { refs[0] = value; }
    public static void Dispose(owned Item value) { delete value; }
    public static int Run(bool flag) {
        Item owner = new Item(); Item[] refs = new Item[1]; Store(owner, refs);
        EscapeApi.ExternalSpin(); if (flag) { Dispose(move(owner)); }
        int value = refs[0].Read(); delete[] refs; return value;
    }
}
)NS", "Exe", producerReference);
    ProjectCompilation externalCompilation;
    std::string externalError;
    nativesharp::compilation::ProjectLoader externalLoader;
    nativesharp::compilation::ProjectLoadOptions externalOptions;
    auto externalLoaded = externalLoader.load(externalProject, externalOptions, externalError);
    std::vector<nativesharp::compilation::Project> externalProjects;
    if (externalLoaded) externalProjects.push_back(std::move(*externalLoaded));
    const bool externalPrepared = metadataWritten && externalLoaded.has_value() &&
        nativesharp::compilation::preparePrecompiledProject(
            nativesharp::compilation::ProjectGraph(externalProject, std::move(externalProjects)),
            externalOptions, externalCompilation, externalError);
    const bool externalBuilt = externalPrepared &&
        externalCompilation.compileProjectConstructions(externalProject, externalError);
    // EN: Contract replaced on 2026-11-13 (docs/3.0/main-fixes/04): the precompiled `ExternalSpin` may
    //     return, and then `Dispose` runs only when `flag` is true; the read is unproven and rejected.
    // ES: Contrato reemplazado el 2026-11-13 (docs/3.0/main-fixes/04): `ExternalSpin` precompilado puede
    //     retornar, y entonces `Dispose` corre solo si `flag` es verdadero; la lectura no esta demostrada y
    //     se rechaza.
    tests.expect(externalPrepared && !externalBuilt && diagnosticsText(externalCompilation).find(
            "cannot prove that the referenced object is still alive") != std::string::npos,
        "late external fixture loads actual producer metadata (" + externalError +
        diagnosticsText(externalCompilation) + ")");
    if (externalPrepared && externalCompilation.methodBodyModel() && externalCompilation.memberModel())
    {
        const auto* model = externalCompilation.methodBodyModel();
        const auto members = externalCompilation.memberModel()->members();
        bool precompiledBinding = false, provedExternal = false;
        for (const auto& call : model->calls())
            if (call.selectedMemberOrdinal && *call.selectedMemberOrdinal < members.size() &&
                members[*call.selectedMemberOrdinal].name == "ExternalSpin" &&
                members[*call.selectedMemberOrdinal].isPrecompiled) precompiledBinding = true;
        for (const auto& observation : model->arrayReferenceReplayObservations())
            if (observation.callableOrdinal < members.size() && members[observation.callableOrdinal].name == "Run" &&
                nativesharp::frontend::evaluateArrayReferenceLatePath(externalCompilation, observation) ==
                    nativesharp::frontend::ArrayReferenceLatePathResult::ProvenDeadPath) provedExternal = true;
        tests.expect(precompiledBinding, "late external call resolves to a precompiled member");
        tests.expect(!provedExternal, "a call into a precompiled body cannot prove the dead path");
    }

    const std::filesystem::path safeProject = writeProject(
        root / "safe", "EscapeSafe", R"NS(
using EscapeContracts;
public class Program
{
    public static int Main()
    {
        int local = 42;
        return EscapeApi.Read(&local);
    }
}
)NS", "Exe", producerReference);
    const std::string safeProjectText = safeProject.string();
    const std::array<std::string_view, 3> safeArguments = {
        "--dump-project-method-bodies", safeProjectText, "-O0"};
    std::string safeOutput;
    std::string safeError;
    tests.expect(runDriver(safeArguments, safeOutput, safeError) ==
            nativesharp::ExitCode::Success,
        "a proven noescape callee accepts current-frame storage (" + safeError +
            safeOutput + ")");

    const std::filesystem::path unsafeProject = writeProject(
        root / "unsafe", "EscapeUnsafe", R"NS(
using EscapeContracts;
public class Program
{
    public static int Main()
    {
        int local = 42;
        EscapeApi.Retain(&local);
        return local;
    }
}
)NS", "Exe", producerReference);
    const std::string unsafeProjectText = unsafeProject.string();
    const std::array<std::string_view, 3> unsafeArguments = {
        "--dump-project-method-bodies", unsafeProjectText, "-O0"};
    std::string unsafeOutput;
    std::string unsafeError;
    const nativesharp::ExitCode unsafeResult =
        runDriver(unsafeArguments, unsafeOutput, unsafeError);
    const std::string unsafeDiagnostics = unsafeError + unsafeOutput;
    tests.expect(unsafeResult != nativesharp::ExitCode::Success &&
            unsafeDiagnostics.find("retained by the callee's published escape contract") !=
                std::string::npos,
        "a proven global escape rejects current-frame storage with a causal diagnostic (" +
            unsafeDiagnostics + ")");

    const std::filesystem::path directGlobalProject = writeProject(
        root / "direct-global", "EscapeDirectGlobal", R"NS(
public class Program
{
    private static int* Stored;
    private static void Retain(int* value) { Stored = value; }
    public static int Main() { int local = 42; Retain(&local); return local; }
}
)NS");
    const std::string directGlobalProjectText = directGlobalProject.string();
    const std::array<std::string_view, 3> directGlobalArguments = {
        "--dump-project-method-bodies", directGlobalProjectText, "-O0"};
    std::string directGlobalOutput;
    std::string directGlobalError;
    const nativesharp::ExitCode directGlobalResult =
        runDriver(directGlobalArguments, directGlobalOutput, directGlobalError);
    const std::string directGlobalDiagnostics = directGlobalError + directGlobalOutput;
    tests.expect(directGlobalResult != nativesharp::ExitCode::Success &&
            directGlobalDiagnostics.find("retained by the callee's published escape contract") !=
                std::string::npos,
        "a direct global escape rejects current-frame storage without an edition gate (" +
            directGlobalDiagnostics + ")");

    // ES: Un local puntero se valida por a dónde apunta, no por su propio slot: una dirección del
    // marco que llega por un alias se rechaza, y un puntero a almacenamiento global copiado en
    // locales se acepta (antes se rechazaba: KrLinearAllocator de nsc-debugger no compilaba).
    // EN: A pointer local is validated by where it points, not by its own slot: a frame address that
    // arrives through an alias is rejected, and a pointer to global storage copied into locals is
    // accepted (it used to be rejected: nsc-debugger's KrLinearAllocator did not compile).
    const std::filesystem::path aliasFrameProject = writeProject(
        root / "pointer-alias-frame", "EscapeNegative", R"NS(
namespace EscapeNegative
{
    int* Kept = null;

    void Retain(int* pointer)
    {
        Kept = pointer;
    }

    int Main()
    {
        int local = 5;
        int* alias = null;
        alias = &local;
        Retain(alias);
        return 0;
    }
}
)NS");
    const std::string aliasFrameProjectText = aliasFrameProject.string();
    const std::array<std::string_view, 3> aliasFrameArguments = {
        "--dump-project-method-bodies", aliasFrameProjectText, "-O0"};
    std::string aliasFrameOutput;
    std::string aliasFrameError;
    const nativesharp::ExitCode aliasFrameResult =
        runDriver(aliasFrameArguments, aliasFrameOutput, aliasFrameError);
    const std::string aliasFrameDiagnostics = aliasFrameError + aliasFrameOutput;
    tests.expect(aliasFrameResult != nativesharp::ExitCode::Success &&
            aliasFrameDiagnostics.find("retained by the callee's published escape contract") !=
                std::string::npos,
        "a frame address passed through a pointer alias is rejected (" +
            aliasFrameDiagnostics + ")");

    const std::filesystem::path aliasGlobalProject = writeProject(
        root / "pointer-alias-global", "EscapePositive", R"NS(
namespace EscapePositive
{
    byte[16] Storage;
    byte* Kept = null;

    void Retain(byte* pointer)
    {
        Kept = pointer;
    }

    byte* Take()
    {
        return &Storage[0];
    }

    int Main()
    {
        byte* block = Take();
        byte* copy = block;
        Retain(copy);
        Retain(&Storage[1]);
        return Kept == &Storage[1] ? 0 : 1;
    }
}
)NS");
    const std::string aliasGlobalProjectText = aliasGlobalProject.string();
    const std::array<std::string_view, 3> aliasGlobalArguments = {
        "--dump-project-method-bodies", aliasGlobalProjectText, "-O0"};
    std::string aliasGlobalOutput;
    std::string aliasGlobalError;
    tests.expect(runDriver(aliasGlobalArguments, aliasGlobalOutput, aliasGlobalError) ==
            nativesharp::ExitCode::Success,
        "a pointer local that holds a global address may reach a retained parameter (" +
            aliasGlobalError + aliasGlobalOutput + ")");

    // EN: Contract replaced by the user's decision of 2026-11-13 (docs/3.0/main-fixes/04). These controls
    //     were accepted because no precise proof of a dead path existed; their sources are unchanged. The
    //     use is now rejected as unproven because some path may destroy the object (a branch, a loop break,
    //     a virtual or cyclic drop, a caller-chosen argument), because the element index is not proven
    //     (reassigned, by reference, mutated or constrained only by a condition), or because the store
    //     traps out of bounds before the use, which the rule does not model.
    // ES: Contrato reemplazado por la decision del usuario del 2026-11-13 (docs/3.0/main-fixes/04). Estos
    //     controles se aceptaban porque no habia prueba precisa de un camino muerto; sus fuentes no
    //     cambian. El uso ahora se rechaza como no demostrado porque algun camino puede destruir el objeto
    //     (una rama, un break de ciclo, un drop virtual o ciclico, un argumento elegido por el llamador),
    //     porque el indice del elemento no esta demostrado (reasignado, por referencia, mutado o acotado
    //     solo por una condicion), o porque el guardado sale de limites antes del uso, lo que la regla no
    //     modela.
    const std::set<std::string_view> conservativelyRejected = {
        "integration_override", "integration_read_index_reassigned", "integration_read_index_ref",
        "integration_parameter_read_changed", "integration_parameter_read_constrained",
        "integration_restricted_parameter", "integration_mutated_parameter", "integration_reference_parameter",
        "integration_empty_parameter_array", "integration_mutable_index", "integration_aliased_index",
        "integration_literal_out_of_bounds", "integration_while_conditional_break",
        "integration_while_parameter_false", "integration_while_parameter_no_call",
        "integration_while_parameter_caller_spin", "integration_while_parameter_mutated",
        "integration_while_parameter_caller_static_init", "integration_while_local_changed",
        "integration_loop_alias_conditional_break", "integration_loop_alias_out_of_bounds",
        "integration_loop_conditional_break", "conditional_delete_nonreturning_branch",
        "linear_forwarded_delete_cycle"};
    const auto conservativeRejection = [&](std::string_view name, bool built, const ProjectCompilation& compilation,
                                           const std::string& error)
    {
        tests.expect(!built && diagnosticsText(compilation).find(
                "cannot prove that the referenced object is still alive") != std::string::npos,
            "conservative rejection for " + std::string(name) + " (" + error + diagnosticsText(compilation) + ")");
    };
    // Each
    // control is a separate project, so one rejected path cannot mask another.
    const auto integrationCase = [&](std::string_view name, std::string_view source, bool reject, bool potential = false)
    {
        const auto path = writeProject(root / std::string(name), name, source);
        ProjectCompilation bodies;
        std::string bodiesError;
        const bool bodiesBuilt = bodies.compileProjectMethodBodies(path, bodiesError);
        tests.expect(bodiesBuilt && !bodies.diagnostics().hasErrors(),
            "method-bodies remains partial for " + std::string(name) + " (" + bodiesError + diagnosticsText(bodies) + ")");
        ProjectCompilation constructions;
        std::string constructionError;
        const bool constructionBuilt = constructions.compileProjectConstructions(path, constructionError);
        if (conservativelyRejected.contains(name))
        {
            conservativeRejection(name, constructionBuilt, constructions, constructionError);
            return;
        }
        const bool lateDiagnostic = diagnosticsText(constructions).find(potential
            ? "reference to a potentially destroyed object cannot be used as an instance method receiver"
            : "reference to a destroyed object cannot be used as an instance method receiver") != std::string::npos;
        tests.expect(reject ? (!constructionBuilt && lateDiagnostic) : (constructionBuilt && !lateDiagnostic),
            "construction phase result for " + std::string(name) + " (" + constructionError + diagnosticsText(constructions) + ")");
        if (reject && potential && constructions.methodBodyModel() != nullptr)
        {
            bool separated = false;
            for (const auto& observation : constructions.methodBodyModel()->arrayReferenceReplayObservations())
                if (observation.potentialIndex && observation.state == nativesharp::frontend::ArrayReferenceRead::Unknown &&
                    (observation.potentialCandidateState == nativesharp::frontend::ArrayReferenceRead::Dead ||
                     (observation.potentialCandidateState == nativesharp::frontend::ArrayReferenceRead::MaybeDead &&
                      observation.deadPathWitness)) &&
                    nativesharp::frontend::evaluateArrayReferenceLatePath(constructions, observation) ==
                        nativesharp::frontend::ArrayReferenceLatePathResult::ProvenDeadPath) separated = true;
            tests.expect(separated, "potential parameter keeps public state unknown for " + std::string(name));
        }
    };
    integrationCase("integration_branch", R"NS(
class Resource { public Resource(int n) {} public int Read() { return 1; } }
class Holder { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } }
class Case { public static int Run(bool flag) { Resource owner = new Resource(40); Resource[] refs = new Resource[1]; Holder.Store(owner, refs); if (flag) { Holder.Drop(move(owner)); } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    integrationCase("integration_spin", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static void Spin() { while (true) {} } public static int Run(bool flag) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Spin(); if (flag) { Drop(move(owner)); } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_ctor", R"NS(
class Resource { public Resource() { while (true) {} } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool flag) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); if (flag) { Drop(move(owner)); } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_dtor", R"NS(
class Resource { ~Resource() { while (true) {} } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool flag) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); if (flag) { Drop(move(owner)); } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_dtor_effect", R"NS(
class Resource { public int Value; public static void Spin() { while (true) {} } ~Resource() { Value = 0; Spin(); } public int Read() { return Value; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool flag) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); if (flag) { Drop(move(owner)); } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_override", R"NS(
class Resource { public int Read() { return 1; } } class Base { public virtual void Drop(owned Resource x) { delete x; } } class Derived : Base { public override void Drop(owned Resource x) { delete x; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static int Run(bool flag) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Derived d = new Derived(); Store(owner, refs); if (flag) { d.Drop(move(owner)); } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_alias", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool flag) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Resource[] alias = refs; Store(owner, refs); if (flag) { Drop(move(owner)); } int value = alias[0].Read(); delete[] refs; return value; } }
)NS", true);
    integrationCase("integration_branch_alias_chain", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool flag) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Resource[] a = refs; Resource[] b = a; Store(owner, b); if (flag) { Drop(move(owner)); } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    integrationCase("integration_branch_alias_overwrite", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool flag) { Resource owner = new Resource(); Resource survivor = new Resource(); Resource[] refs = new Resource[1]; Resource[] alias = refs; Store(owner, alias); if (flag) { Drop(move(owner)); } Store(survivor, refs); int value = alias[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_branch_alias_dtor_spin", R"NS(
class Resource { ~Resource() { while (true) {} } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool flag) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Resource[] alias = refs; Store(owner, alias); if (flag) { Drop(move(owner)); } int value = alias[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_branch_alias_rebind", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool flag) { Resource owner = new Resource(); Resource survivor = new Resource(); Resource[] refs = new Resource[1]; Resource[] other = new Resource[1]; Resource[] alias = refs; Store(owner, refs); Store(survivor, other); alias = other; if (flag) { Drop(move(owner)); } int value = alias[0].Read(); delete[] refs; delete[] other; return value; } }
)NS", false);
    integrationCase("integration_literal_index", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs, int i) { refs[i] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[2]; Store(owner, refs, 0); Drop(move(owner)); int result = refs[0].Read(); delete[] refs; return result; } }
)NS", true);
    integrationCase("integration_stable_read_index", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int index = 0; int result = refs[index].Read(); delete[] refs; return result; } }
)NS", true);
    integrationCase("integration_stable_read_index_branch", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool flag) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); if (flag) { Drop(move(owner)); } int index = 0; int result = refs[index].Read(); delete[] refs; return result; } }
)NS", true);
    integrationCase("integration_read_index_reassigned", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void StoreLive(Resource value, Resource[] refs) { refs[1] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource live = new Resource(); Resource[] refs = new Resource[2]; Store(owner, refs); StoreLive(live, refs); Drop(move(owner)); int index = 0; index = 1; int result = refs[index].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_read_index_ref", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void StoreLive(Resource value, Resource[] refs) { refs[1] = value; } public static void Drop(owned Resource value) { delete value; } public static void Set(ref int value) { value = 1; } public static int Run() { Resource owner = new Resource(); Resource live = new Resource(); Resource[] refs = new Resource[2]; Store(owner, refs); StoreLive(live, refs); Drop(move(owner)); int index = 0; Set(ref index); int result = refs[index].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_read_index_live", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); int index = 0; int result = refs[index].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_read_index_other_slot", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void StoreDead(Resource value, Resource[] refs) { refs[0] = value; } public static void StoreLive(Resource value, Resource[] refs) { refs[1] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource live = new Resource(); Resource[] refs = new Resource[2]; StoreDead(owner, refs); StoreLive(live, refs); Drop(move(owner)); int index = 1; int result = refs[index].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_read_index_dtor_spin", R"NS(
class Resource { ~Resource() { while (true) {} } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int index = 0; int result = refs[index].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_read_index_ctor_spin", R"NS(
class Resource { public Resource() { while (true) {} } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int index = 0; int result = refs[index].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_unknown_index", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs, int i) { refs[i] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(int i) { Resource owner = new Resource(); Resource[] refs = new Resource[2]; Store(owner, refs, i); Drop(move(owner)); int result = refs[0].Read(); delete[] refs; return result; } }
)NS", true, true);
    integrationCase("integration_parameter_read_index", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(int i) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int result = refs[i].Read(); delete[] refs; return result; } }
)NS", true, true);
    integrationCase("integration_parameter_read_other_index", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[1] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(int i) { Resource owner = new Resource(); Resource[] refs = new Resource[2]; Store(owner, refs); Drop(move(owner)); int result = refs[i].Read(); delete[] refs; return result; } }
)NS", true, true);
    integrationCase("integration_parameter_read_branch", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(int i, bool flag) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); if (flag) { Drop(move(owner)); } int result = refs[i].Read(); delete[] refs; return result; } }
)NS", true, true);
    integrationCase("integration_parameter_read_live", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static int Run(int i) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); int result = refs[i].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_parameter_read_changed", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void StoreLive(Resource value, Resource[] refs) { refs[1] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(int i) { Resource owner = new Resource(); Resource live = new Resource(); Resource[] refs = new Resource[2]; Store(owner, refs); StoreLive(live, refs); Drop(move(owner)); i = 1; int result = refs[i].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_parameter_read_constrained", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void StoreLive(Resource value, Resource[] refs) { refs[1] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(int i) { Resource owner = new Resource(); Resource live = new Resource(); Resource[] refs = new Resource[2]; Store(owner, refs); StoreLive(live, refs); Drop(move(owner)); if (i == 0) { delete[] refs; return 0; } int result = refs[i].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_parameter_read_dtor_spin", R"NS(
class Resource { ~Resource() { while (true) {} } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(int i) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int result = refs[i].Read(); delete[] refs; return result; } }
)NS", false);
    // Contract is intraprocedural: no claim is made that Entry invokes Run.
    integrationCase("integration_private_parameter", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs, int i) { refs[i] = value; } public static void Drop(owned Resource value) { delete value; } private static int Run(int i) { Resource owner = new Resource(); Resource[] refs = new Resource[2]; Store(owner, refs, i); Drop(move(owner)); int result = refs[0].Read(); delete[] refs; return result; } public static int Entry() { return 0; } }
)NS", true, true);
    integrationCase("integration_restricted_parameter", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs, int i) { refs[i] = value; } public static void Zero(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(int i) { Resource owner = new Resource(); Resource live = new Resource(); Resource[] refs = new Resource[2]; Zero(live, refs); if (i == 1) { Store(owner, refs, i); } Drop(move(owner)); int result = refs[0].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_mutated_parameter", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs, int i) { refs[i] = value; } public static void Zero(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(int i) { Resource owner = new Resource(); Resource live = new Resource(); Resource[] refs = new Resource[2]; Zero(live, refs); i = 1; Store(owner, refs, i); Drop(move(owner)); int result = refs[0].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_reference_parameter", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs, int i) { refs[i] = value; } public static void Zero(Resource value, Resource[] refs) { refs[0] = value; } public static void Set(ref int i) { i = 1; } public static void Drop(owned Resource value) { delete value; } public static int Run(int i) { Resource owner = new Resource(); Resource live = new Resource(); Resource[] refs = new Resource[2]; Zero(live, refs); Set(ref i); Store(owner, refs, i); Drop(move(owner)); int result = refs[0].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_empty_parameter_array", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs, int i) { refs[i] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(int i) { Resource owner = new Resource(); Resource[] refs = new Resource[0]; Store(owner, refs, i); Drop(move(owner)); int result = refs[0].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_local_index", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs, int i) { refs[i] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[2]; int i = 0; Store(owner, refs, i); Drop(move(owner)); int result = refs[0].Read(); delete[] refs; return result; } }
)NS", true);
    integrationCase("integration_mutable_index", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs, int i) { refs[i] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[2]; int i = 0; i = 1; Store(owner, refs, i); Drop(move(owner)); int result = refs[0].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_aliased_index", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs, int i) { refs[i] = value; } public static void Drop(owned Resource value) { delete value; } public static void Change(ref int i) { i = 1; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[2]; int i = 0; Change(ref i); Store(owner, refs, i); Drop(move(owner)); int result = refs[0].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_local_index_overwritten", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs, int i) { refs[i] = value; } public static void StoreZero(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource survivor = new Resource(); Resource[] refs = new Resource[2]; int i = 0; Store(owner, refs, i); Drop(move(owner)); StoreZero(survivor, refs); int result = refs[0].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_literal_out_of_bounds", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs, int i) { refs[i] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[0]; Store(owner, refs, 0); Drop(move(owner)); int result = refs[0].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_literal_overwritten", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs, int i) { refs[i] = value; } public static void StoreZero(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource survivor = new Resource(); Resource[] refs = new Resource[2]; Store(owner, refs, 0); Drop(move(owner)); StoreZero(survivor, refs); int result = refs[0].Read(); delete[] refs; return result; } }
)NS", false);
    integrationCase("integration_loop_one", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); for (int i = 0; i < 1; i++) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    integrationCase("integration_while_true_break", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); while (true) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    integrationCase("integration_while_false_break", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); while (false) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_while_prefix_spin", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Spin() { while (true) {} } public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Spin(); Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); while (true) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_while_overwrite", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource survivor = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); while (true) { Drop(move(owner)); break; } Store(survivor, refs); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_while_conditional_break", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool stop) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); while (true) { if (stop) { break; } Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_while_destructor_spin", R"NS(
class Resource { ~Resource() { while (true) {} } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); while (true) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_while_parameter_true", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool repeat) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); while (repeat) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } public static int Entry() { return Run(true); } }
)NS", true);
    integrationCase("integration_while_parameter_false", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool repeat) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); while (repeat) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } public static int Entry() { return Run(false); } }
)NS", false);
    integrationCase("integration_while_parameter_no_call", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool repeat) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); while (repeat) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_while_parameter_caller_spin", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Spin() { while (true) {} } public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool repeat) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); while (repeat) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } public static int Entry() { Spin(); return Run(true); } }
)NS", false);
    integrationCase("integration_while_parameter_mutated", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool repeat) { repeat = false; Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); while (repeat) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } public static int Entry() { return Run(true); } }
)NS", false);
    integrationCase("integration_while_parameter_caller_static_init", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool repeat) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); while (repeat) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } }
class Blocked { private static int state = Spin(); private static int Spin() { while (true) {} return 0; } public static int Entry() { return Case.Run(true); } }
)NS", false);
    const auto linearReturnControl = [&](std::string_view name, std::string_view source, bool reject)
    {
        const auto path = writeProject(root / std::string(name), name, source);
        ProjectCompilation bodies;
        std::string bodiesError;
        const bool bodiesBuilt = bodies.compileProjectMethodBodies(path, bodiesError);
        const bool bodiesDiagnostic = diagnosticsText(bodies).find(
            "reference to a destroyed object cannot be used as an instance method receiver") != std::string::npos;
        tests.expect(reject ? (!bodiesBuilt && bodiesDiagnostic) : (bodiesBuilt && !bodiesDiagnostic),
            "linear method-body return control for " + std::string(name) + " (" + bodiesError + diagnosticsText(bodies) + ")");
        ProjectCompilation constructions;
        std::string constructionError;
        const bool built = constructions.compileProjectConstructions(path, constructionError);
        const bool diagnostic = diagnosticsText(constructions).find(
            "reference to a destroyed object cannot be used as an instance method receiver") != std::string::npos;
        if (conservativelyRejected.contains(name)) conservativeRejection(name, built, constructions, constructionError);
        else tests.expect(reject ? (!built && diagnostic) : (built && !diagnostic),
            "linear construction return control for " + std::string(name) + " (" + constructionError + diagnosticsText(constructions) + ")");
    };
    const auto fieldReadControl = [&](std::string_view name, std::string_view source, bool reject, bool write = false)
    {
        const auto path = writeProject(root / std::string(name), name, source);
        ProjectCompilation compilation;
        std::string error;
        const bool built = compilation.compileProjectConstructions(path, error);
        const bool diagnostic = diagnosticsText(compilation).find(write
            ? "reference to a destroyed object cannot be used to write an instance field"
            : "reference to a destroyed object cannot be used to read an instance field") != std::string::npos;
        tests.expect(reject ? (!built && diagnostic) : (built && !diagnostic),
            "array field lifetime for " + std::string(name) + " (" + error + diagnosticsText(compilation) + ")");
    };
    fieldReadControl("field_destroyed", R"NS(
class Resource { public int Value; }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Value; delete[] refs; return value; } }
)NS", true);
    fieldReadControl("field_live", R"NS(
class Resource { public int Value; }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); int value = refs[0].Value; delete[] refs; return value; } }
)NS", false);
    fieldReadControl("field_nonreturning", R"NS(
class Resource { public int Value; }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static void Spin() { while (true) {} } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); Spin(); int value = refs[0].Value; delete[] refs; return value; } }
)NS", false);
    fieldReadControl("field_nonreturning_constructor", R"NS(
class Resource { public int Value; public Resource() { while (true) {} } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Value; delete[] refs; return value; } }
)NS", false);
    fieldReadControl("field_nonreturning_destructor", R"NS(
class Resource { public int Value; ~Resource() { while (true) {} } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Value; delete[] refs; return value; } }
)NS", false);
    fieldReadControl("field_write_destroyed", R"NS(
class Resource { public int Value; }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); refs[0].Value = 4; delete[] refs; return 0; } }
)NS", true, true);
    fieldReadControl("field_write_live", R"NS(
class Resource { public int Value; }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); refs[0].Value = 4; delete[] refs; return 0; } }
)NS", false, true);
    fieldReadControl("field_write_nonreturning", R"NS(
class Resource { public int Value; }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static void Spin() { while (true) {} } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); Spin(); refs[0].Value = 4; delete[] refs; return 0; } }
)NS", false, true);
    fieldReadControl("field_write_nonreturning_value", R"NS(
class Resource { public int Value; }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Spin() { while (true) {} return 4; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); refs[0].Value = Spin(); delete[] refs; return 0; } }
)NS", false, true);
    fieldReadControl("field_write_nonreturning_destructor", R"NS(
class Resource { public int Value; ~Resource() { while (true) {} } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); refs[0].Value = 4; delete[] refs; return 0; } }
)NS", false, true);
    linearReturnControl("array_alias_destroyed", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Resource[] alias = refs; Drop(move(owner)); int value = alias[0].Read(); delete[] refs; return value; } }
)NS", true);
    linearReturnControl("array_alias_chain_destroyed", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Resource[] a = refs; Resource[] b = a; Store(owner, b); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    linearReturnControl("array_alias_overwrite_live", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource survivor = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Resource[] alias = refs; Drop(move(owner)); Store(survivor, alias); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    linearReturnControl("array_alias_overwrite_null", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Resource[] alias = refs; Drop(move(owner)); alias[0] = null; bool empty = refs[0] == null; delete[] refs; return 0; } }
)NS", false);
    linearReturnControl("array_alias_rebound", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource survivor = new Resource(); Resource[] refs = new Resource[1]; Resource[] other = new Resource[1]; Store(owner, refs); Store(survivor, other); Resource[] alias = refs; alias = other; Drop(move(owner)); int value = alias[0].Read(); delete[] refs; delete[] other; return value; } }
)NS", false);
    linearReturnControl("conditional_delete_cleanup", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value, bool eager) { if (eager) { delete value; } } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner), false); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    linearReturnControl("conditional_delete_explicit", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value, bool eager) { if (eager) { delete value; } } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner), true); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    linearReturnControl("conditional_delete_return", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value, bool early) { if (early) { return; } else { delete value; } } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner), true); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    linearReturnControl("conditional_delete_nested", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value, bool first, bool second) { if (first) { if (second) { delete value; } } else { delete value; } } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner), true, false); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    linearReturnControl("conditional_delete_owned_second", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(bool eager, owned Resource value) { if (eager) { delete value; } } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(false, move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    linearReturnControl("conditional_delete_nonreturning_branch", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value, bool eager) { if (eager) { delete value; } else { while (true) {} } } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner), false); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    linearReturnControl("conditional_delete_nonreturning_destructor", R"NS(
class Resource { ~Resource() { while (true) {} } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value, bool eager) { if (eager) { delete value; } } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner), false); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    linearReturnControl("linear_forwarded_delete", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Delete(owned Resource value) { delete value; } public static void Mid(owned Resource value) { Delete(move(value)); } public static void Drop(owned Resource value) { Mid(move(value)); } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    linearReturnControl("linear_forwarded_delete_qualified", R"NS(
class Resource { public int Read() { return 1; } }
class Destroyer { public static void Delete(owned Resource value) { delete value; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { Destroyer.Delete(move(value)); } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    linearReturnControl("linear_forwarded_delete_nonreturning_destructor", R"NS(
class Resource { ~Resource() { while (true) {} } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Delete(owned Resource value) { delete value; } public static void Drop(owned Resource value) { Delete(move(value)); } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    linearReturnControl("linear_forwarded_delete_nonreturning_derived_destructor", R"NS(
class Resource { public int Read() { return 1; } }
class Derived : Resource { ~Derived() { while (true) {} } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Delete(owned Resource value) { delete value; } public static void Drop(owned Resource value) { Delete(move(value)); } public static int Run() { Resource owner = new Derived(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    linearReturnControl("linear_forwarded_delete_returning_derived_destructor", R"NS(
class Resource { public int Read() { return 1; } }
class Derived : Resource { ~Derived() { } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Delete(owned Resource value) { delete value; } public static void Drop(owned Resource value) { Delete(move(value)); } public static int Run() { Resource owner = new Derived(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    linearReturnControl("linear_forwarded_delete_nonreturning_static_init", R"NS(
class Resource { public int Read() { return 1; } }
class Delayed { static int state = Spin(); static int Spin() { while (true) {} return 0; } public static void Delete(owned Resource value) { delete value; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { Delayed.Delete(move(value)); } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    linearReturnControl("linear_forwarded_delete_nonreturning_object_static_init", R"NS(
class Resource { static int state = Spin(); static int Spin() { while (true) {} return 0; } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Delete(owned Resource value) { delete value; } public static void Drop(owned Resource value) { Delete(move(value)); } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    linearReturnControl("linear_forwarded_delete_nonreturning_caller_static_init", R"NS(
class Resource { public int Read() { return 1; } }
class Case { static int state = Spin(); static int Spin() { while (true) {} return 0; } public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Delete(owned Resource value) { delete value; } public static void Drop(owned Resource value) { Delete(move(value)); } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    linearReturnControl("linear_forwarded_delete_cycle", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void A(owned Resource value) { B(move(value)); } public static void B(owned Resource value) { A(move(value)); } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); A(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    linearReturnControl("linear_nonreturning_call", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static void Spin() { while (true) {} } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Spin(); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    linearReturnControl("linear_nonreturning_after_delete", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static void Spin() { while (true) {} } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); Spin(); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    linearReturnControl("linear_returning_call", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static void Spin() { while (true) { break; } } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Spin(); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    linearReturnControl("linear_nonreturning_destructor", R"NS(
class Resource { ~Resource() { while (true) {} } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    linearReturnControl("linear_returning_destructor", R"NS(
class Resource { ~Resource() { while (true) { break; } } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    linearReturnControl("linear_nonreturning_constructor", R"NS(
class Resource { public Resource() { while (true) {} } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    linearReturnControl("linear_returning_constructor", R"NS(
class Resource { public Resource() { while (true) { break; } } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    linearReturnControl("linear_nonreturning_constructor_overload", R"NS(
class Resource { public Resource() { while (true) {} } public Resource(int x) {} public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    linearReturnControl("linear_returning_constructor_overload", R"NS(
class Resource { public Resource() {} public Resource(int x) { while (true) {} } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    linearReturnControl("linear_drop_after_literal_local", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { int marker = 1; delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    linearReturnControl("linear_drop_before_literal_local", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; bool marker = true; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    linearReturnControl("linear_drop_local_nonreturning_destructor", R"NS(
class Resource { ~Resource() { while (true) {} } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { int marker = 1; delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    linearReturnControl("linear_drop_nonreturning_local_initializer", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static int Spin() { while (true) {} return 1; } public static void Drop(owned Resource value) { int marker = Spin(); delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    linearReturnControl("linear_drop_nonreturning_after_delete", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Spin() { while (true) {} } public static void Drop(owned Resource value) { delete value; Spin(); } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_while_local_true", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); bool flag = true; while (flag) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    integrationCase("integration_while_local_false", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); bool flag = false; while (flag) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_while_local_changed", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); bool flag = true; flag = false; while (flag) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_loop_alias_dead", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Resource[] alias = refs; Store(owner, alias); for (int i = 0; i < 1; i++) { Drop(move(owner)); break; } int value = alias[0].Read(); delete[] refs; return value; } }
)NS", true);
    integrationCase("integration_loop_alias_chain_dead", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Resource[] a = refs; Resource[] b = a; Store(owner, b); for (int i = 0; i < 1; i++) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    integrationCase("integration_while_alias_dead", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Resource[] alias = refs; Store(owner, refs); while (true) { Drop(move(owner)); break; } int value = alias[0].Read(); delete[] refs; return value; } }
)NS", true);
    integrationCase("integration_loop_alias_zero", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Resource[] alias = refs; Store(owner, alias); for (int i = 0; i < 0; i++) { Drop(move(owner)); break; } int value = alias[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_loop_alias_overwrite", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource survivor = new Resource(); Resource[] refs = new Resource[1]; Resource[] alias = refs; Store(owner, alias); for (int i = 0; i < 1; i++) { Drop(move(owner)); break; } Store(survivor, refs); int value = alias[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_loop_alias_conditional_break", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool stop) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Resource[] alias = refs; Store(owner, alias); for (int i = 0; i < 1; i++) { if (stop) { break; } Drop(move(owner)); break; } int value = alias[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_loop_alias_out_of_bounds", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[0]; Resource[] alias = refs; Store(owner, alias); for (int i = 0; i < 1; i++) { Drop(move(owner)); break; } int value = alias[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_while_alias_destructor_spin", R"NS(
class Resource { ~Resource() { while (true) {} } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Resource[] alias = refs; Store(owner, alias); while (true) { Drop(move(owner)); break; } int value = alias[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_loop_alias_rebound", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource survivor = new Resource(); Resource[] refs = new Resource[1]; Resource[] other = new Resource[1]; Resource[] alias = refs; Store(owner, refs); Store(survivor, other); alias = other; for (int i = 0; i < 1; i++) { Drop(move(owner)); break; } int value = alias[0].Read(); delete[] refs; delete[] other; return value; } }
)NS", false);
    integrationCase("integration_loop_zero", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); for (int i = 0; i < 0; i++) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_loop_prefix_spin", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Spin() { while (true) {} } public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Spin(); Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); for (int i = 0; i < 1; i++) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_loop_conditional_break", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run(bool stop) { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); for (int i = 0; i < 1; i++) { if (stop) { break; } Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_loop_overwrite", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource survivor = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); for (int i = 0; i < 1; i++) { Drop(move(owner)); break; } Store(survivor, refs); int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_loop_alias", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); for (int i = 0; i < 1; i++) { Drop(move(owner)); break; } Resource[] alias = refs; int value = refs[0].Read(); delete[] refs; return value; } }
)NS", true);
    integrationCase("integration_loop_alias_after_chain", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); for (int i = 0; i < 1; i++) { Drop(move(owner)); break; } Resource[] a = refs; Resource[] b = a; int value = b[0].Read(); delete[] refs; return value; } }
)NS", true);
    integrationCase("integration_loop_alias_after_live", R"NS(
class Resource { public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource survivor = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); for (int i = 0; i < 1; i++) { Drop(move(owner)); break; } Store(survivor, refs); Resource[] alias = refs; int value = alias[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_loop_constructor_spin", R"NS(
class Resource { public Resource() { while (true) {} } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); for (int i = 0; i < 1; i++) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    integrationCase("integration_loop_destructor_spin", R"NS(
class Resource { ~Resource() { while (true) {} } public int Read() { return 1; } }
class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } public static void Drop(owned Resource value) { delete value; } public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); for (int i = 0; i < 1; i++) { Drop(move(owner)); break; } int value = refs[0].Read(); delete[] refs; return value; } }
)NS", false);
    // EN: A definitely destroyed element must not be copied into a local or another assignment
    //     target, passed as an argument or returned. Comparisons, copies made while the owner lives
    //     and copies after a live overwrite remain valid.
    // ES: Un elemento definitivamente destruido no puede copiarse a un local u otro destino de
    //     asignacion, pasarse como argumento ni retornarse. Las comparaciones, las copias hechas con
    //     el dueno vivo y las copias tras sobrescribir con un objeto vivo siguen siendo validas.
    const auto valueCopyControl = [&](std::string_view name, std::string_view body, bool reject)
    {
        const std::string source = std::string(
            "class Resource { public int Read() { return 1; } } "
            "class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } "
            "public static void Drop(owned Resource value) { delete value; } "
            "public static int Use(Resource value) { return value.Read(); } ") + std::string(body) + " }";
        const auto path = writeProject(root / std::string(name), name, source);
        ProjectCompilation compilation;
        std::string error;
        const bool built = compilation.compileProjectConstructions(path, error);
        const bool diagnostic = diagnosticsText(compilation).find(
            "reference to a destroyed object cannot be copied, passed or returned") != std::string::npos;
        tests.expect(reject ? (!built && diagnostic) : (built && !diagnostic),
            "destroyed array value copy for " + std::string(name) + " (" + error + diagnosticsText(compilation) + ")");
    };
    valueCopyControl("copy_local_destroyed", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); Resource copy = refs[0]; "
        "int value = copy.Read(); delete[] refs; return value; }", true);
    valueCopyControl("copy_argument_destroyed", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int value = Use(refs[0]); "
        "delete[] refs; return value; }", true);
    valueCopyControl("copy_assignment_destroyed", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); Resource copy = null; "
        "copy = refs[0]; delete[] refs; return 0; }", true);
    valueCopyControl("copy_return_destroyed", "public static Resource Pick(Resource[] refs) { return refs[0]; } "
        "public static Resource Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; "
        "Store(owner, refs); Drop(move(owner)); Resource[] alias = refs; return alias[0]; }", true);
    valueCopyControl("copy_local_live", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Store(owner, refs); Resource copy = refs[0]; int value = copy.Read(); "
        "int other = Use(refs[0]); Drop(move(owner)); delete[] refs; return value + other; }", false);
    valueCopyControl("copy_comparison_destroyed", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); bool present = refs[0] != null; "
        "delete[] refs; return present ? 1 : 0; }", false);
    valueCopyControl("copy_after_live_overwrite", "public static int Run() { Resource owner = new Resource(); "
        "Resource survivor = new Resource(); Resource[] refs = new Resource[1]; Store(owner, refs); "
        "Drop(move(owner)); Store(survivor, refs); Resource copy = refs[0]; int value = Use(copy); delete[] refs; "
        "return value; }", false);
    // EN: A `raii` local destroyed by the end of a nested block leaves a dead element when the
    //     array outlives the block. Reads inside the block, slots holding another owner and a
    //     destructor without normal exit stay accepted.
    // ES: Un local `raii` destruido al final de un bloque anidado deja un elemento muerto cuando el
    //     arreglo sobrevive al bloque. Las lecturas dentro del bloque, las ranuras con otro dueno y un
    //     destructor sin salida normal siguen aceptados.
    const auto scopeExitControl = [&](std::string_view name, std::string_view resource,
                                      std::string_view body, std::string_view expected)
    {
        const std::string source = std::string(resource) +
            " class Case { public static void Store(Resource value, Resource[] refs) { refs[0] = value; } " +
            std::string(body) + " }";
        const auto path = writeProject(root / std::string(name), name, source);
        ProjectCompilation compilation;
        std::string error;
        const bool built = compilation.compileProjectConstructions(path, error);
        const std::string text = diagnosticsText(compilation);
        const bool diagnostic = text.find("reference to a destroyed object") != std::string::npos;
        const bool expectedFound = !expected.empty() && text.find(std::string(expected)) != std::string::npos;
        tests.expect(expected.empty() ? (built && !diagnostic) : (!built && expectedFound),
            "array reference after scope cleanup for " + std::string(name) + " (" + error + text + ")");
    };
    const std::string_view plainResource = "class Resource { public int Read() { return 1; } }";
    scopeExitControl("scope_exit_receiver", plainResource,
        "public static int Run() { Resource[] refs = new Resource[1]; { Resource inner = new Resource(); "
        "Store(inner, refs); } int value = refs[0].Read(); delete[] refs; "
        "return value; }", "reference to a destroyed object cannot be used as an instance method receiver");
    scopeExitControl("scope_exit_copy", plainResource,
        "public static int Run() { Resource[] refs = new Resource[1]; { Resource inner = new Resource(); "
        "Store(inner, refs); } Resource copy = refs[0]; delete[] refs; return 0; }",
        "reference to a destroyed object cannot be copied, passed or returned");
    scopeExitControl("scope_exit_read_inside", plainResource,
        "public static int Run() { Resource[] refs = new Resource[1]; int value = 0; "
        "{ Resource inner = new Resource(); Store(inner, refs); value = refs[0].Read(); } delete[] refs; "
        "return value; }", "");
    scopeExitControl("scope_exit_other_owner", plainResource,
        "public static int Run() { Resource outer = new Resource(); Resource[] refs = new Resource[1]; "
        "{ Resource temporary = new Resource(); Store(outer, refs); } "
        "int value = refs[0].Read(); delete[] refs; return value; }", "");
    scopeExitControl("scope_exit_destructor_spin", "class Resource { ~Resource() { while (true) {} } "
        "public int Read() { return 1; } }", "public static int Run() { Resource[] refs = new Resource[1]; "
        "{ Resource inner = new Resource(); Store(inner, refs); } int value = refs[0].Read(); delete[] refs; "
        "return value; }", "");
    // EN: Conservative lifetime rule (docs/3.0/main-fixes/04). A use of an element whose possible
    //     object was destroyed on some path, or whose owner was moved without a proven outcome, is
    //     rejected; a move to a live local owner, a stored but unused reference, an unrelated live
    //     element, a comparison, a read before the event and an unreachable read stay accepted.
    // ES: Regla conservadora de vida (docs/3.0/main-fixes/04). Se rechaza el uso de un elemento cuyo
    //     objeto posible se destruyo en algun camino, o cuyo dueno se movio sin resultado demostrado;
    //     se aceptan mover a un dueno local vivo, guardar sin usar, un elemento vivo ajeno, una
    //     comparacion, una lectura antes del evento y una lectura inalcanzable.
    const auto unprovenControl = [&](std::string_view name, std::string_view body, std::string_view expected)
    {
        const std::string source = std::string(
            "class Resource { public int Read() { return 1; } } "
            "class Dropper { public virtual void Drop(owned Resource value) { delete value; } } "
            "class Case { public static int count; public static void Note() { count = count + 1; } "
            "public static void Store(Resource value, Resource[] refs) { refs[0] = value; } "
            "public static void Drop(owned Resource value) { delete value; } "
            "public static void NoteAndDrop(owned Resource value) { Note(); delete value; } "
            "public static void Forward(Resource value, Resource[] refs) { Store(value, refs); } "
            "public static void Spin() { while (true) {} } "
            "public static int ReadFirst(Resource[] refs) { return refs[0].Read(); } "
            "public static bool Flag(int n) { return n > 0; } public static int Zero(int n) { return n * 0; } ") +
            std::string(body) + " }";
        const auto path = writeProject(root / std::string(name), name, source);
        ProjectCompilation compilation;
        std::string error;
        const bool built = compilation.compileProjectConstructions(path, error);
        const std::string text = diagnosticsText(compilation);
        const bool matched = expected.empty()
            ? (built && text.find("destroyed object") == std::string::npos &&
               text.find("cannot prove that the referenced object") == std::string::npos)
            : (!built && text.find(expected) != std::string::npos);
        tests.expect(matched, "conservative array lifetime for " + std::string(name) + " (" + error + text + ")");
    };
    const std::string_view unproven = "cannot prove that the referenced object is still alive";
    unprovenControl("unproven_virtual_drop", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Store(owner, refs); Dropper dropper = new Dropper(); "
        "dropper.Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; }", unproven);
    unprovenControl("unproven_preceded_drop", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Store(owner, refs); NoteAndDrop(move(owner)); "
        "int value = refs[0].Read(); delete[] refs; return value; }", unproven);
    unprovenControl("unproven_two_level_store", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Forward(owner, refs); Drop(move(owner)); "
        "int value = refs[0].Read(); delete[] refs; return value; }", unproven);
    unprovenControl("unproven_runtime_branch", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Store(owner, refs); if (Flag(1)) { Drop(move(owner)); } "
        "int value = refs[0].Read(); delete[] refs; return value; }", unproven);
    unprovenControl("unproven_computed_index", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Store(owner, refs); Drop(move(owner)); int i = Zero(5); "
        "Resource copy = refs[i]; delete[] refs; return 0; }", unproven);
    unprovenControl("unproven_runtime_branch_copy", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Store(owner, refs); if (Flag(1)) { Drop(move(owner)); } "
        "Resource copy = refs[0]; delete[] refs; return 0; }", unproven);
    unprovenControl("proven_move_to_live_local", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Store(owner, refs); owned Resource keeper = move(owner); "
        "int value = refs[0].Read(); delete[] refs; return value; }", "");
    unprovenControl("stored_never_used", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Store(owner, refs); Dropper dropper = new Dropper(); "
        "dropper.Drop(move(owner)); delete[] refs; return 0; }", "");
    unprovenControl("unrelated_live_element", "public static int Run() { Resource owner = new Resource(); "
        "Resource other = new Resource(); Resource[] refs = new Resource[1]; Store(other, refs); "
        "Drop(move(owner)); int value = refs[0].Read(); delete[] refs; return value; }", "");
    unprovenControl("comparison_after_unknown_drop", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Store(owner, refs); Dropper dropper = new Dropper(); "
        "dropper.Drop(move(owner)); bool present = refs[0] != null; delete[] refs; return present ? 1 : 0; }", "");
    unprovenControl("read_before_lifetime_event", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Store(owner, refs); int value = ReadFirst(refs); "
        "Drop(move(owner)); delete[] refs; return value; }", "");
    // EN: Keeper is declared after Case is closed; Flow.Run uses Case.Store.
    // ES: Keeper se declara despues de cerrar Case; Flow.Run usa Case.Store.
    const std::string keeper = "public static int Unused() { return 0; } } class Keeper { owned Resource held = null; "
        "public void Adopt(owned Resource item) { held = move(item); } public void Clear() { held = null; } } "
        "class Flow { ";
    unprovenControl("adopted_by_live_owner", keeper +
        "public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; "
        "Case.Store(owner, refs); Keeper keeper = new Keeper(); keeper.Adopt(move(owner)); "
        "int value = refs[0].Read(); delete[] refs; return value; }", "");
    unprovenControl("adopted_then_receiver_call", keeper +
        "public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; "
        "Case.Store(owner, refs); Keeper keeper = new Keeper(); keeper.Adopt(move(owner)); keeper.Clear(); "
        "int value = refs[0].Read(); delete[] refs; return value; }", unproven);
    unprovenControl("adopted_owner_scope_ended", keeper +
        "public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; "
        "Case.Store(owner, refs); { Keeper keeper = new Keeper(); keeper.Adopt(move(owner)); } "
        "int value = refs[0].Read(); delete[] refs; return value; }", unproven);
    const std::string holder = "public static int Unused2() { return 0; } } "
        "class Holder { public owned Resource held = null; } "
        "class Moves { ";
    unprovenControl("moved_into_field", holder +
        "public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; "
        "Case.Store(owner, refs); Holder holder = new Holder(); holder.held = move(owner); "
        "int value = refs[0].Read(); delete[] refs; return value; }", unproven);
    unprovenControl("moved_into_live_local", holder +
        "public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; "
        "Case.Store(owner, refs); Resource next = new Resource(); next = move(owner); "
        "int value = refs[0].Read(); delete[] refs; return value; }", "");
    unprovenControl("moved_local_scope_ended", holder +
        "public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; "
        "Case.Store(owner, refs); { raii Resource inner = move(owner); } "
        "int value = refs[0].Read(); delete[] refs; return value; }", "reference to a destroyed object");
    // EN: A class owner local cannot be deleted explicitly, so delete is never a life event here.
    // ES: Un local dueno de clase no puede borrarse explicitamente, asi delete nunca es un evento aqui.
    unprovenControl("delete_of_owned_local_rejected", holder +
        "public static int Run() { owned Resource owner = new Resource(); Resource[] refs = new Resource[1]; "
        "Case.Store(owner, refs); delete owner; int value = refs[0].Read(); delete[] refs; return value; }",
        "explicit delete of a RAII local is forbidden");
    unprovenControl("unreachable_after_spin", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Store(owner, refs); if (Flag(1)) { Drop(move(owner)); Spin(); } "
        "int value = refs[0].Read(); delete[] refs; return value; }", "");
    // EN: Loop pass (M2-05): an event of one iteration reaches uses at the start of the next.
    // ES: Pasada de ciclos (M2-05): un evento de una iteracion alcanza los usos al inicio de la siguiente.
    unprovenControl("loop_previous_iteration_read", "public static int Run() { Resource[] refs = new Resource[1]; "
        "int total = 0; for (int i = 0; i < 3; i = i + 1) { if (i > 0) { total = total + refs[0].Read(); } "
        "Resource local = new Resource(); Store(local, refs); } delete[] refs; return total; }", unproven);
    unprovenControl("loop_previous_iteration_while", "public static int Run() { Resource[] refs = new Resource[1]; "
        "int total = 0; int i = 0; while (i < 3) { if (i > 0) { Resource copy = refs[0]; } "
        "Resource local = new Resource(); Store(local, refs); i = i + 1; } delete[] refs; return total; }",
        unproven);
    unprovenControl("loop_outer_owner_dropped", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Store(owner, refs); int total = 0; "
        "for (int i = 0; i < 3; i = i + 1) { total = total + refs[0].Read(); "
        "if (i == 1) { Drop(move(owner)); Resource next = new Resource(); owner = move(next); } } "
        "delete[] refs; return total; }", unproven);
    unprovenControl("loop_same_iteration", "public static int Run() { Resource[] refs = new Resource[1]; "
        "int total = 0; for (int i = 0; i < 3; i = i + 1) { Resource local = new Resource(); "
        "Store(local, refs); total = total + refs[0].Read(); } delete[] refs; return total; }", "");
    unprovenControl("loop_drop_then_break", "public static int Run() { Resource owner = new Resource(); "
        "Resource[] refs = new Resource[1]; Store(owner, refs); int total = 0; "
        "for (int i = 0; i < 3; i = i + 1) { total = total + refs[0].Read(); "
        "if (i == 1) { Drop(move(owner)); break; } } delete[] refs; return total; }", "");
    unprovenControl("loop_unrelated_array", "public static int Run() { Resource keep = new Resource(); "
        "Resource[] refs = new Resource[1]; Resource[] other = new Resource[1]; Store(keep, refs); "
        "int total = 0; for (int i = 0; i < 3; i = i + 1) { total = total + refs[0].Read(); "
        "Resource local = new Resource(); Store(local, other); } delete[] refs; delete[] other; "
        "return total; }", "");
    // EN: Field routes (M2-06, docs/3.0/main-fixes/07): references stored into a field array or a plain
    //     reference field of a local receiver and used later through another method of that receiver.
    // ES: Rutas de campo (M2-06, docs/3.0/main-fixes/07): referencias guardadas en un arreglo en campo o un
    //     campo de referencia simple de un receptor local y usadas despues por otro metodo de ese receptor.
    const std::string registry = "public static int Unused3() { return 0; } } "
        "class Registry { Resource[] items = new Resource[2]; Resource last = null; "
        "public void Add(Resource item) { items[0] = item; } public void Remember(Resource item) { last = item; } "
        "public int ReadFirst() { return items[0].Read(); } public int ReadLast() { return last.Read(); } "
        "public int ReadOther() { return items[1].Read(); } public bool HasFirst() { return items[0] != null; } "
        "public int ReadTwice() { return ReadFirst() + 1; } public virtual int Peek() { return 0; } } "
        "class Fields { ";
    const std::string destroyed = "reference to a destroyed object cannot be used";
    unprovenControl("field_array_destroyed", registry +
        "public static int Run() { Resource owner = new Resource(); Registry registry = new Registry(); "
        "registry.Add(owner); Case.Drop(move(owner)); return registry.ReadFirst(); }", destroyed);
    unprovenControl("field_plain_destroyed", registry +
        "public static int Run() { Resource owner = new Resource(); Registry registry = new Registry(); "
        "registry.Remember(owner); Case.Drop(move(owner)); return registry.ReadLast(); }", destroyed);
    unprovenControl("field_scope_exit", registry +
        "public static int Run() { Registry registry = new Registry(); "
        "{ Resource inner = new Resource(); registry.Add(inner); } return registry.ReadFirst(); }", destroyed);
    unprovenControl("field_through_this_call", registry +
        "public static int Run() { Resource owner = new Resource(); Registry registry = new Registry(); "
        "registry.Add(owner); Case.Drop(move(owner)); return registry.ReadTwice(); }", destroyed);
    unprovenControl("field_unproven_virtual_drop", registry +
        "public static int Run() { Resource owner = new Resource(); Registry registry = new Registry(); "
        "registry.Add(owner); Dropper dropper = new Dropper(); dropper.Drop(move(owner)); "
        "return registry.ReadFirst(); }", unproven);
    unprovenControl("field_virtual_receiver_call", registry +
        "public static int Run() { Resource owner = new Resource(); Registry registry = new Registry(); "
        "registry.Add(owner); Case.Drop(move(owner)); return registry.Peek(); }", unproven);
    unprovenControl("field_live_owner", registry +
        "public static int Run() { Resource owner = new Resource(); Registry registry = new Registry(); "
        "registry.Add(owner); int value = registry.ReadFirst(); Case.Drop(move(owner)); return value; }", "");
    unprovenControl("field_comparison_only", registry +
        "public static int Run() { Resource owner = new Resource(); Registry registry = new Registry(); "
        "registry.Add(owner); Case.Drop(move(owner)); return registry.HasFirst() ? 1 : 0; }", "");
    unprovenControl("field_other_slot", registry +
        "public static int Run() { Resource owner = new Resource(); Registry registry = new Registry(); "
        "registry.Add(owner); Case.Drop(move(owner)); return registry.ReadOther(); }", "");
    unprovenControl("field_never_used", registry +
        "public static int Run() { Resource owner = new Resource(); Registry registry = new Registry(); "
        "registry.Add(owner); Case.Drop(move(owner)); return 0; }", "");
    // EN: Routes around the receiver: a borrowed alias, a move of the receiver, a direct field access,
    //     the receiver passed to a method, and a loop.
    // ES: Rutas alrededor del receptor: un alias prestado, un move del receptor, un acceso directo al
    //     campo, el receptor pasado a un metodo y un ciclo.
    const std::string routes = "public static int Unused4() { return 0; } } "
        "class Box { public Resource[] items = new Resource[1]; public void Put(Resource item) { items[0] = item; } "
        "public int Get() { return items[0].Read(); } } "
        "class Filler { public static void Fill(Box box, Resource item) { box.Put(item); } } "
        "class Routes { ";
    unprovenControl("field_moved_receiver", routes +
        "public static int Run() { Resource owner = new Resource(); Box box = new Box(); box.Put(owner); "
        "raii Box next = move(box); Case.Drop(move(owner)); return next.Get(); }", destroyed);
    unprovenControl("field_direct_access", routes +
        "public static int Run() { Resource owner = new Resource(); Box box = new Box(); box.Put(owner); "
        "Case.Drop(move(owner)); return box.items[0].Read(); }", unproven);
    unprovenControl("field_receiver_passed", routes +
        "public static int Run() { Resource owner = new Resource(); Box box = new Box(); "
        "Filler.Fill(box, owner); Case.Drop(move(owner)); return box.Get(); }", unproven);
    unprovenControl("field_loop_previous_iteration", routes +
        "public static int Run() { Box box = new Box(); int total = 0; for (int i = 0; i < 3; i = i + 1) { "
        "if (i > 0) { total = total + box.Get(); } Resource item = new Resource(); box.Put(item); } "
        "return total; }", unproven);
    unprovenControl("field_loop_same_iteration", routes +
        "public static int Run() { Box box = new Box(); int total = 0; for (int i = 0; i < 3; i = i + 1) { "
        "Resource item = new Resource(); box.Put(item); total = total + box.Get(); } return total; }", "");
    // EN: Route audit (M2-07, docs/3.0/main-fixes/08): every route that can carry a reference to a tracked
    //     object out of its owner's sight is followed or made conservative.
    // ES: Auditoria de rutas (M2-07, docs/3.0/main-fixes/08): toda ruta que puede llevar una referencia a un
    //     objeto rastreado fuera de la vista de su dueno se sigue o se vuelve conservadora.
    const std::string audit = "public static int Unused5() { return 0; } } "
        "class Global { public static Resource held = null; } "
        "class Keep { public static void Static(Resource x) { Global.held = x; } "
        "public static void Into(Resource x, Resource[] refs) { refs[0] = x; } "
        "public static void Slot(ref Resource slot, Resource x) { slot = x; } } "
        "struct Pair { public Resource item; } "
        "class Fill { public static void Pair(ref Pair pair, Resource x) { pair.item = x; } } "
        "class Inner { public Resource item = null; } "
        "class Deep { public owned Inner inner = new Inner(); public void Put(Resource x) { inner.item = x; } "
        "public int Get() { return inner.item.Read(); } } "
        "class Outer { public owned Deep deep = new Deep(); } "
        "class Bag<T> { T[] items = new T[2]; public void Add(T x) { items[0] = x; } "
        "public T First() { return items[0]; } } "
        "class Audit { ";
    // EN: A callee copies an element of a tracked array into a field (routes R03b/R15, doc 15).
    // ES: Un callee copia un elemento de un arreglo rastreado a un campo (rutas R03b/R15, doc 15).
    const std::string copier = "public static int Unused6() { return 0; } } "
        "class Copier { Resource item = null; public void Take(Resource[] values) { item = values[0]; } "
        "public int Get() { return item.Read(); } } class Copies { ";
    unprovenControl("route_array_element_copied_to_field", copier +
        "public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; "
        "Case.Store(owner, refs); Copier copier = new Copier(); copier.Take(refs); Case.Drop(move(owner)); "
        "int value = copier.Get(); delete[] refs; return value; }", unproven);
    unprovenControl("route_array_element_copied_live", copier +
        "public static int Run() { Resource owner = new Resource(); Resource[] refs = new Resource[1]; "
        "Case.Store(owner, refs); Copier copier = new Copier(); copier.Take(refs); int value = copier.Get(); "
        "Case.Drop(move(owner)); delete[] refs; return value; }", "");
    unprovenControl("route_static_field", audit +
        "public static int Run() { Resource owner = new Resource(); Keep.Static(owner); Case.Drop(move(owner)); "
        "return Global.held.Read(); }", unproven);
    unprovenControl("route_static_field_live", audit +
        "public static int Run() { Resource owner = new Resource(); Keep.Static(owner); "
        "int value = Global.held.Read(); Case.Drop(move(owner)); return value; }", "");
    unprovenControl("route_parameter_array", audit +
        "public static int Use(Resource[] refs) { Resource owner = new Resource(); Keep.Into(owner, refs); "
        "Case.Drop(move(owner)); return refs[0].Read(); } public static int Run() { return 0; }", unproven);
    unprovenControl("route_parameter_array_unrelated", audit +
        "public static int Use(Resource[] refs) { Resource owner = new Resource(); int value = refs[0].Read(); "
        "Case.Drop(move(owner)); return value; } public static int Run() { return 0; }", "");
    unprovenControl("route_ref_element", audit +
        "public static int Run() { Resource[] refs = new Resource[1]; Resource owner = new Resource(); "
        "Keep.Slot(ref refs[0], owner); Case.Drop(move(owner)); int value = refs[0].Read(); delete[] refs; "
        "return value; }", unproven);
    unprovenControl("route_struct_local", audit +
        "public static int Run() { Pair pair; Resource owner = new Resource(); Fill.Pair(ref pair, owner); "
        "Case.Drop(move(owner)); return pair.item.Read(); }", unproven);
    unprovenControl("route_nested_receiver", audit +
        "public static int Run() { Outer outer = new Outer(); Resource owner = new Resource(); "
        "outer.deep.Put(owner); Case.Drop(move(owner)); return outer.deep.Get(); }", unproven);
    unprovenControl("route_deep_callee", audit +
        "public static int Run() { Deep deep = new Deep(); Resource owner = new Resource(); deep.Put(owner); "
        "Case.Drop(move(owner)); return deep.Get(); }", unproven);
    unprovenControl("route_this_receiver", audit +
        "public static int Run() { return 0; } } class Self { Resource item = null; "
        "public void Put(Resource x) { item = x; } public int Get() { return item.Read(); } "
        "public int Run() { Resource owner = new Resource(); Put(owner); Case.Drop(move(owner)); return Get(); }",
        destroyed);
    unprovenControl("route_generic_container", audit +
        "public static int Run() { Bag<Resource> bag = new Bag<Resource>(); Resource owner = new Resource(); "
        "bag.Add(owner); Case.Drop(move(owner)); return bag.First().Read(); }", destroyed);
    // EN: Async and iterator routes (M2 audit, doc 16): events across `await`/`yield return` are ordinary
    //     flow; a coroutine callee may store after the call returns (deferred), so its stores stay unproven.
    // ES: Rutas async e iteradores (auditoria M2, doc 16): los eventos a traves de `await`/`yield return`
    //     son flujo ordinario; un llamado corrutina puede guardar despues de volver la llamada (diferido),
    //     asi que sus guardados quedan sin demostrar.
    const std::string coroutine = "public static async int Tick() { yield; return 0; } "
        "public static async int StoreAsync(Resource x, Resource[] refs) { refs[0] = x; return 0; } "
        "public static async int FillLater(Resource x, Resource[] refs) { yield; refs[0] = x; return 0; } "
        "public static Iterator<int, int> FillSteps(Resource x, Resource[] refs) "
        "{ yield return 1; refs[0] = x; yield return 2; } ";
    unprovenControl("async_dead_after_await", coroutine +
        "public static async int Run() { Resource[] refs = new Resource[1]; Resource owner = new Resource(); "
        "Store(owner, refs); int t = await Tick(); Drop(move(owner)); int value = refs[0].Read(); "
        "delete[] refs; return value + t; }", destroyed);
    unprovenControl("async_live_across_await", coroutine +
        "public static async int Run() { Resource[] refs = new Resource[1]; Resource owner = new Resource(); "
        "Store(owner, refs); int t = await Tick(); int value = refs[0].Read(); Drop(move(owner)); "
        "delete[] refs; return value + t; }", "");
    unprovenControl("async_callee_store", coroutine +
        "public static async int Run() { Resource[] refs = new Resource[1]; Resource owner = new Resource(); "
        "int t = await StoreAsync(owner, refs); Drop(move(owner)); int value = refs[0].Read(); "
        "delete[] refs; return value + t; }", unproven);
    unprovenControl("async_deferred_store", coroutine +
        "public static async int Run() { Resource[] refs = new Resource[1]; Resource live = new Resource(); "
        "Resource owner = new Resource(); Future<int> pending = FillLater(owner, refs); Store(live, refs); "
        "Drop(move(owner)); int t = await pending; int value = refs[0].Read(); delete[] refs; return value + t; }",
        unproven);
    unprovenControl("iterator_dead_across_yield", coroutine +
        "public static Iterator<int, int> Run() { Resource[] refs = new Resource[1]; "
        "Resource owner = new Resource(); Store(owner, refs); yield return 1; Drop(move(owner)); "
        "int value = refs[0].Read(); delete[] refs; yield return value; }", destroyed);
    unprovenControl("iterator_live_across_yield", coroutine +
        "public static Iterator<int, int> Run() { Resource[] refs = new Resource[1]; "
        "Resource owner = new Resource(); Store(owner, refs); yield return 1; int value = refs[0].Read(); "
        "Drop(move(owner)); delete[] refs; yield return value; }", "");
    unprovenControl("iterator_deferred_store", coroutine +
        "public static int Run() { Resource[] refs = new Resource[1]; Resource live = new Resource(); "
        "Resource owner = new Resource(); Iterator<int, int> it = FillSteps(owner, refs); Store(live, refs); "
        "Drop(move(owner)); int s = 0; while (it.MoveNext()) { s += it.Current; } int value = refs[0].Read(); "
        "delete[] refs; return value + s; }", unproven);
    unprovenControl("iterator_store_live", coroutine +
        "public static int Run() { Resource[] refs = new Resource[1]; Resource owner = new Resource(); "
        "Iterator<int, int> it = FillSteps(owner, refs); int s = 0; while (it.MoveNext()) { s += it.Current; } "
        "int value = refs[0].Read(); Drop(move(owner)); delete[] refs; return value + s; }", "");
    return tests.resultKeeping(root);
}
#include <span>
#include <sstream>
