#include "FrontendContractSupport.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/OwnershipFlowSnapshot.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisStatistics.h"
#include "nativesharp/Frontend/ProjectCallableBindingIndex.h"
#include "nativesharp/Frontend/ProjectCompilationDumper.h"
#include "nativesharp/Frontend/ProjectGenericModelDumper.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <array>
#include <sstream>
#include <string>

namespace nsc::suite
{
namespace
{
using namespace frontend_contract_support;
using namespace nativesharp::frontend;

std::string genericSource()
{
    return R"ns(
namespace Suite
{
    public interface IValue { int Read(); }
    public class Item : IValue { public int Read() { return 7; } }
    public struct Box<T> { public T Value; public T Get() { return Value; } }
    public class Algorithms
    {
        public static T Identity<T>(T value) { return value; }
        public static int Run()
        {
            Box<int> box;
            box.Value = 5;
            return Identity<int>(box.Get());
        }
    }
}
)ns";
}

void genericDiscoveryOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 20, variant, ProjectStage::Generics, genericSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            test.expect(compilation.genericModel() != nullptr,
                "generic discovery must publish ProjectGenericModel");
            const std::string dump = ProjectGenericModelDumper::dump(*compilation.genericModel());
            test.expect(dump.find("Box") != std::string::npos,
                "generic discovery must preserve the template identity");
        });
}

void genericInferenceOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 21, variant, ProjectStage::MethodBodies, genericSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            test.expect(compilation.genericModel() != nullptr && compilation.methodBodyModel() != nullptr,
                "generic inference must retain specialization and analyzed callable models");
            const std::string dump = ProjectGenericModelDumper::dump(*compilation.genericModel());
            test.expect(dump.find("builtin:int") != std::string::npos,
                "explicit and inferred integer specializations must retain their substituted type");
        });
}

void genericConstraintOracle(TestContext& context, std::size_t variant)
{
    const std::string source = R"ns(
namespace Suite
{
    public interface IValue { int Read(); }
    public class Item : IValue { public int Read() { return 1; } }
    public class Box<T> where T : class, IValue
    {
        public int Read(T value) { return value.Read(); }
    }
}
)ns";
    expectProjectStage(context, 22, variant, ProjectStage::GenericConstraints, source,
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            const ProjectGenericConstraintModel* model = compilation.genericConstraintModel();
            test.expect(model != nullptr && model->isBuilt(),
                "constraint stage must publish one built normalized model");
            if (model != nullptr)
            {
                test.expect(model->violations().empty(),
                    "valid class and interface constraints must retain no violation");
            }
        });
}

void genericDumpOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 23, variant, ProjectStage::Generics, genericSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            const std::string first = ProjectGenericModelDumper::dump(*compilation.genericModel());
            const std::string second = ProjectGenericModelDumper::dump(*compilation.genericModel());
            test.expect(first == second, "generic dump must be byte deterministic");
            test.expect(first.find("Identity") != std::string::npos,
                "generic dump must retain function-template identity");
        });
}

void conversionOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> sources = {
        "builtin:byte", "builtin:short", "builtin:int", "builtin:uint",
        "builtin:float", "builtin:null", "builtin:int*", "tuple:builtin:int,builtin:long",
    };
    constexpr std::array<std::string_view, 8> targets = {
        "builtin:int", "builtin:int", "builtin:long", "builtin:ulong",
        "builtin:double", "class:Suite.Item", "builtin:void*", "tuple:builtin:long,builtin:long",
    };
    const ProjectImplicitConversion conversion =
        ProjectTypeConversion::classifyImplicit(sources[variant], targets[variant], std::nullopt, variant == 5);
    context.expect(conversion.exists(), "the selected canonical implicit conversion class must exist");
    context.expect(conversion.sourceTypeKey == sources[variant] && conversion.targetTypeKey == targets[variant],
        "conversion result must preserve exact source and target keys");
    const ProjectImplicitConversion rejected =
        ProjectTypeConversion::classifyImplicit("builtin:string", "builtin:int");
    context.expect(!rejected.exists(), "an incompatible scalar shape must not invent a conversion");
}

void integerEvaluatorOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<std::string_view, 8> keys = {
        "builtin:sbyte", "builtin:byte", "builtin:short", "builtin:ushort",
        "builtin:int", "builtin:uint", "builtin:long", "builtin:ulong",
    };
    constexpr std::array<unsigned, 8> widths = {8, 8, 16, 16, 32, 32, 64, 64};
    context.expect(ProjectIntegerEvaluator::integerWidth(keys[variant]) == widths[variant],
        "integer evaluator must publish the exact language width");
    context.expect(ProjectIntegerEvaluator::isIntegerType(keys[variant]),
        "integer evaluator must recognize every canonical integer key");
    const std::uint64_t normalized = ProjectIntegerEvaluator::normalize(~std::uint64_t{0}, keys[variant]);
    if (!ProjectIntegerEvaluator::isSignedInteger(keys[variant]))
    {
        const std::uint64_t mask = widths[variant] == 64 ? ~std::uint64_t{0} :
            ((std::uint64_t{1} << widths[variant]) - 1);
        context.expect(normalized == mask, "unsigned normalization must truncate to the exact width");
    }
    context.expect(ProjectIntegerEvaluator::integerWidth("builtin:string") == 0,
        "unsupported type must not acquire an integer width");
}

void flowRecordOracle(TestContext& context, std::size_t variant)
{
    OwnershipFlowSnapshot snapshot;
    snapshot.reachable = variant != 4;
    snapshot.localBaseOrdinal = 10;
    snapshot.localCount = 2;
    snapshot.localFlags = {1, static_cast<std::uint8_t>(variant)};
    snapshot.parameterFlags = {2};
    snapshot.partiallyMovedPlaces.insert("L10.field");
    snapshot.storageFacts.resize(3);
    snapshot.storageFacts[variant % 3].nonNull = true;
    OwnershipFlowSnapshot copy = snapshot;
    context.expect(copy.reachable == snapshot.reachable && copy.localFlags == snapshot.localFlags,
        "flow snapshot copy must preserve reachability and dense local flags exactly");
    context.expect(copy.partiallyMovedPlaces.contains("L10.field"),
        "flow snapshot must preserve canonical partially moved place keys");
    context.expect(copy.storageFacts.size() == snapshot.storageFacts.size(),
        "flow snapshot must preserve parameter-first storage fact cardinality");
}

void callableRecordOracle(TestContext& context, std::size_t variant)
{
    nativesharp::ast::AstNode syntax(nativesharp::ast::AstNodeKind::IdentifierExpression, {});
    ProjectCallableBindingIndex index;
    index.record(variant, &syntax, 10);
    index.record(variant, &syntax, 20);
    context.expect(index.size() == 1, "recording the same callable/syntax key must replace in place");
    context.expect(index.latest(&syntax, variant) == std::optional<std::size_t>{20},
        "callable binding index must return newest-binding-wins");
    index.record(variant, nullptr, 30);
    context.expect(index.size() == 1 && !index.latest(nullptr, variant).has_value(),
        "null syntax must never be materialized or queried as a binding key");
    index.clear();
    context.expect(index.size() == 0, "clear must remove every callable binding record");
}

void callableWaveOracle(TestContext& context, std::size_t variant)
{
    ProjectCallableAnalysisStatistics first;
    first.callablesQueued = variant + 1;
    first.callablesAnalyzed = variant;
    first.callablesSkipped = 1;
    first.requestedWorkers = variant == 7 ? 4 : 1;
    first.effectiveWorkers = first.requestedWorkers;
    ProjectCallableAnalysisStatistics merged = first;
    merged.resultsMerged = merged.callablesAnalyzed;
    merged.maximumWaveWidth = merged.callablesQueued;
    context.expect(merged.callablesAnalyzed + merged.callablesSkipped == merged.callablesQueued,
        "callable wave statistics must account for every queued callable");
    context.expect(merged.resultsMerged == first.callablesAnalyzed,
        "wave reduction must retain an exact merged-result count");
}

void projectCompilationOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 29, variant, ProjectStage::MethodBodies, genericSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            test.expect(!compilation.completedStages().empty(),
                "project orchestration must publish completed stages in order");
            test.expect(compilation.projectGraph() != nullptr && compilation.methodBodyModel() != nullptr,
                "project orchestration must retain its producer graph and requested terminal model");
        });
}

void compilationDumpOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 30, variant, ProjectStage::MethodBodies, genericSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            ProjectCompilationDumper dumper;
            std::ostringstream first;
            std::ostringstream second;
            dumper.dump(compilation, first);
            dumper.dump(compilation, second);
            test.expect(first.str() == second.str(), "project compilation dump must be byte deterministic");
            test.expect(!first.str().empty(), "project compilation dump must expose the available stage state");
        });
}
}

void registerFrontendGenericsFlowModule(TestRegistry& registry)
{
    addMatrix(registry, 20, "component", genericDiscoveryOracle);
    addMatrix(registry, 21, "component", genericInferenceOracle);
    addMatrix(registry, 22, "component", genericConstraintOracle);
    addMatrix(registry, 23, "unit", genericDumpOracle);
    addMatrix(registry, 24, "unit", conversionOracle);
    addMatrix(registry, 25, "unit", integerEvaluatorOracle);
    addMatrix(registry, 26, "unit", flowRecordOracle);
    addMatrix(registry, 27, "unit", callableRecordOracle);
    addMatrix(registry, 28, "component", callableWaveOracle);
    addMatrix(registry, 29, "integration", projectCompilationOracle);
    addMatrix(registry, 30, "unit", compilationDumpOracle);
}
}
