#include "FrontendContractSupport.h"

#include "nativesharp/Frontend/ComptimeEngine.h"
#include "nativesharp/Frontend/InliningPolicy.h"
#include "nativesharp/Frontend/MacroExpansionEngine.h"
#include "nativesharp/Frontend/MethodEscapeOrigin.h"
#include "nativesharp/Frontend/ProjectCompilationDumper.h"
#include "nativesharp/Frontend/ProjectConstructionModelDumper.h"
#include "nativesharp/Frontend/ProjectDestructionModelDumper.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModelDumper.h"

#include <array>
#include <sstream>
#include <string>

namespace nsc::suite
{
namespace
{
using namespace frontend_contract_support;
using namespace nativesharp::frontend;

std::string objectSource()
{
    return R"ns(
namespace Suite
{
    public struct Point { public int x; public int y; }
    public class Base { public int value; public Base(int source) { value = source; } }
    public class Item : Base
    {
        public int extra;
        public Item(int source, int second) : base(source) { extra = second; }
    }
    public int Run() { Item value = new Item(2, 3); return value.value + value.extra; }
}
)ns";
}

void escapeOriginOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<MethodEscapeOriginKind, 5> kinds = {
        MethodEscapeOriginKind::ExternalStorage,
        MethodEscapeOriginKind::CurrentReceiver,
        MethodEscapeOriginKind::CurrentFrameLocal,
        MethodEscapeOriginKind::CurrentFrameParameter,
        MethodEscapeOriginKind::VariableLengthArray,
    };
    MethodEscapeOrigin origin;
    origin.kind = kinds[variant % kinds.size()];
    origin.ordinal = variant;
    origin.maximumValidScopeOrdinal = variant + 1;
    origin.lifetimeName = variant == 0 ? "static" : "$p" + std::to_string(variant);
    const MethodEscapeOrigin copied = origin;
    context.expect(copied.kind == origin.kind && copied.ordinal == origin.ordinal,
        "escape origin must preserve kind and storage ordinal exactly");
    context.expect(copied.maximumValidScopeOrdinal == origin.maximumValidScopeOrdinal &&
            copied.lifetimeName == origin.lifetimeName,
        "escape origin must preserve lifetime boundary and canonical name");
}

void constructionOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 47, variant, ProjectStage::Constructions, objectSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            const ProjectConstructionModel* model = compilation.constructionModel();
            test.expect(model != nullptr && model->isBuilt(),
                "construction stage must publish one built resolution model");
            if (model != nullptr)
            {
                test.expect(!model->constructions().empty(),
                    "explicit object creation must retain a construction binding");
            }
        });
}

void constructionDumpOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 48, variant, ProjectStage::Constructions, objectSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            ProjectConstructionModelDumper dumper;
            std::ostringstream first;
            std::ostringstream second;
            dumper.dump(*compilation.constructionModel(), *compilation.memberModel(),
                *compilation.nameLookup(), first);
            dumper.dump(*compilation.constructionModel(), *compilation.memberModel(),
                *compilation.nameLookup(), second);
            test.expect(first.str() == second.str(), "construction dump must be byte deterministic");
            test.expect(first.str().find("Item") != std::string::npos,
                "construction dump must retain selected target identity");
        });
}

void destructionOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 49, variant, ProjectStage::Destruction, objectSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            const ProjectDestructionModel* model = compilation.destructionModel();
            test.expect(model != nullptr && model->isBuilt(),
                "destruction stage must publish one built plan model");
            std::ostringstream first;
            std::ostringstream second;
            ProjectDestructionModelDumper::dump(
                *model, *compilation.nameLookup(), *compilation.memberModel(), first);
            ProjectDestructionModelDumper::dump(
                *model, *compilation.nameLookup(), *compilation.memberModel(), second);
            test.expect(first.str() == second.str(), "destruction plan dump must be deterministic");
        });
}

void layoutOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 50, variant, ProjectStage::ObjectLayouts, objectSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            const ProjectObjectLayoutModel* model = compilation.objectLayoutModel();
            test.expect(model != nullptr && model->isBuilt(),
                "layout stage must publish one built object-layout model");
            test.expect(!model->classLayouts().empty() && !model->structLayouts().empty(),
                "class and struct fixtures must publish both physical layout families");
            const auto integer = model->valueLayoutForTypeKey(*compilation.nameLookup(), "builtin:int");
            test.expect(integer.has_value() && integer->first == 4 && integer->second == 4,
                "Windows x64 int layout must remain exactly size 4 alignment 4");
        });
}

void comptimeOracle(TestContext& context, std::size_t variant)
{
    auto cache = std::make_shared<ComptimeCache>();
    ComptimeLimits limits;
    limits.maximumSteps = variant == 4 ? 1 : 1000;
    ComptimeEngine engine(limits, cache, {});
    const std::string source = variant == 4 ? "int x = #run(1 + 2 + 3 + 4);" :
        "int x = #run((4 + 3) * 2);";
    const ComptimeResult first = engine.execute(source);
    const ComptimeResult second = engine.execute(source);
    if (variant == 4)
    {
        context.expect(!first.success && !first.error.empty(),
            "comptime step limit must fail explicitly without a partial result");
        return;
    }
    context.expect(first.success && second.success, "valid comptime expression must evaluate successfully");
    context.expect(first.source == second.source && first.reproducibilityDigest == second.reproducibilityDigest,
        "comptime replacement and digest must be deterministic");
    context.expect(second.cacheHits >= first.cacheHits,
        "repeated comptime execution must preserve observable cache accounting");
}

void macroOracle(TestContext& context, std::size_t variant)
{
    const std::size_t limit = variant == 4 ? 0 : 64;
    MacroExpansionEngine engine(limit);
    const std::string source = "macro add($x, $y) { $x + $y } int value = add!(2, 3);";
    const MacroExpansionResult first = engine.expand(source);
    const MacroExpansionResult second = engine.expand(source);
    if (variant == 4)
    {
        context.expect(!first.success && !first.error.empty(),
            "zero expansion limit must reject an invocation explicitly");
        return;
    }
    context.expect(first.success && first.expansions.size() == 1,
        "one macro invocation must publish one expansion record");
    context.expect(first.source == second.source && first.expansions.front().macroName == "add",
        "macro expansion text and source identity must be deterministic");
}

void metaprogrammingOracle(TestContext& context, std::size_t variant)
{
    expectProjectStage(context, 53, variant, ProjectStage::ObjectLayouts, objectSource(),
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            const ProjectMetaprogrammingModel* model = compilation.metaprogrammingModel();
            test.expect(model != nullptr && model->isBuilt(),
                "layout completion must publish the compile-time metaprogramming catalog");
            test.expect(model != nullptr && !model->emitsRuntimeMetadata(),
                "metaprogramming catalog must remain compile-time-only");
            test.expect(model != nullptr && !model->types().empty(),
                "metaprogramming catalog must retain declared static types");
        });
}

void opaqueResultOracle(TestContext& context, std::size_t variant)
{
    const std::string source = R"ns(namespace Suite {
        public interface IValue { int Read(); }
        public struct Value : IValue { public int Read() { return 7; } }
        public some IValue Make() { Value value; return value; }
    })ns";
    expectProjectStage(context, 54, variant, ProjectStage::Members, source,
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            test.expect(compilation.genericModel() != nullptr && compilation.memberModel() != nullptr,
                "opaque result rewriting must feed the ordinary generic and member models");
            test.expect(compilation.diagnostics().errorCount() == 0,
                "one concrete contributor satisfying the trait must remain valid");
        });
}

void inlinePolicyOracle(TestContext& context, std::size_t variant)
{
    constexpr std::array<InliningPolicy, 4> policies = {
        InliningPolicy::Automatic,
        InliningPolicy::Preferred,
        InliningPolicy::Required,
        InliningPolicy::Forbidden,
    };
    const InliningPolicy policy = policies[variant % policies.size()];
    context.expect(isValidInliningPolicy(policy) && inliningPolicyName(policy) != "invalid",
        "every canonical inline policy must have a stable valid name");
    context.expect(permitsInlining(policy) == (policy != InliningPolicy::Forbidden),
        "inline permission must reject only the forbidden policy");
    context.expect(requiresInlining(policy) == (policy == InliningPolicy::Required),
        "required inline policy must be the only hard-inline contract");
    context.expect(requiresOutOfLineCall(policy) == (policy == InliningPolicy::Forbidden),
        "forbidden inline policy must require an out-of-line call");
}
}

void registerFrontendAuxiliaryModelsModule(TestRegistry& registry)
{
    addMatrix(registry, 46, "unit", escapeOriginOracle);
    addMatrix(registry, 47, "component", constructionOracle);
    addMatrix(registry, 48, "unit", constructionDumpOracle);
    addMatrix(registry, 49, "component", destructionOracle);
    addMatrix(registry, 50, "component", layoutOracle);
    addMatrix(registry, 51, "component", comptimeOracle);
    addMatrix(registry, 52, "component", macroOracle);
    addMatrix(registry, 53, "component", metaprogrammingOracle);
    addMatrix(registry, 54, "component", opaqueResultOracle);
    addMatrix(registry, 55, "unit", inlinePolicyOracle);
}
}
