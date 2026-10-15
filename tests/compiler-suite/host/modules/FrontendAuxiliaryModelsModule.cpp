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

// Proves that construction analysis completed without source construction or
// constructor-initializer records.
void emptyConstructionOracle(
    TestContext& context,
    const ProjectCompilation& compilation)
{
    const ProjectConstructionModel* model = compilation.constructionModel();
    context.expect(model != nullptr && model->isBuilt(),
        "empty source must still publish a built construction model");
    if (model != nullptr)
    {
        context.expect(model->constructions().empty(),
            "empty source must not publish construction records");
        context.expect(model->constructorInitializerBindings().empty(),
            "empty source must not publish constructor initializers");
    }
}

// Proves that physical-layout analysis completed without class or struct
// declarations. ProjectObjectLayoutModel is the actual public model type.
void emptyLayoutOracle(
    TestContext& context,
    const ProjectCompilation& compilation)
{
    const ProjectObjectLayoutModel* model = compilation.objectLayoutModel();
    context.expect(model != nullptr && model->isBuilt(),
        "empty source must still publish a built object-layout model");
    if (model != nullptr)
    {
        context.expect(model->classLayouts().empty(),
            "empty source must not publish class layouts");
        context.expect(model->structLayouts().empty(),
            "empty source must not publish struct layouts");
    }
}

// Proves that compile-time metadata discovery completed without source types or
// attributes and without changing its compile-time-only contract.
void emptyMetaprogrammingOracle(
    TestContext& context,
    const ProjectCompilation& compilation)
{
    const ProjectMetaprogrammingModel* model =
        compilation.metaprogrammingModel();
    context.expect(model != nullptr && model->isBuilt(),
        "empty source must still publish a built metaprogramming model");
    if (model != nullptr)
    {
        context.expect(!model->emitsRuntimeMetadata(),
            "empty metaprogramming model must remain compile-time-only");
        context.expect(model->types().empty(),
            "empty source must not publish metaprogramming types");
        context.expect(model->attributes().empty(),
            "empty source must not publish metaprogramming attributes");
    }
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
        },
        emptyConstructionOracle);
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
        },
        emptyConstructionOracle);
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
        },
        emptyLayoutOracle);
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
    const std::string source = "#macro add(x, y) { $x + $y } int value = #expand add(2, 3);";
    const MacroExpansionResult first = engine.expand(source);
    const MacroExpansionResult second = engine.expand(source);
    if (variant == 4)
    {
        context.expect(!first.success && !first.error.empty(),
            "zero expansion limit must reject an invocation explicitly");
        return;
    }
    if (!first.success || first.expansions.size() != 1)
    {
        context.expect(false, "one macro invocation must publish one expansion record");
        return;
    }
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
        },
        emptyMetaprogrammingOracle);
}

void opaqueResultOracle(TestContext& context, std::size_t variant)
{
    const std::string source = R"ns(namespace Suite {
        public trait IValue { int Read(); }
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

void typedDescriptorsOracle(TestContext& context, std::size_t variant)
{
    const std::string source = R"ns(
namespace Suite
{
    public class Item
    {
        public readonly int flag;
        public const int K = 7;
        public volatile int counter;
        public owned byte[] buffer;
        public Item() { flag = 1; buffer = new byte[4]; }
    }
}
)ns";
    expectProjectStage(context, 64, variant, ProjectStage::ObjectLayouts, source,
        [](TestContext& test, const ProjectCompilation& compilation)
        {
            const ProjectMetaprogrammingModel* model = compilation.metaprogrammingModel();
            test.expect(model != nullptr && model->isBuilt(),
                "typed introspection must publish a built catalog");
            if (model == nullptr) return;
            bool sawFlag = false, sawK = false, sawCounter = false, sawBuffer = false;
            for (const StaticTypeInfo& type : model->types())
            {
                for (const StaticMemberInfo& member : type.members)
                {
                    if (member.name == "flag") sawFlag = member.isReadonly;
                    else if (member.name == "K") sawK = member.isConst;
                    else if (member.name == "counter") sawCounter = member.isVolatile;
                    else if (member.name == "buffer")
                        sawBuffer = member.declaredOwnership == nativesharp::sema::OwnershipKind::Owned;
                }
            }
            test.expect(sawFlag && sawK && sawCounter && sawBuffer,
                "typed introspection must capture readonly/const/volatile modifiers and owned storage");
        },
        emptyMetaprogrammingOracle);
}

void macroContextOracle(TestContext& context, std::size_t variant)
{
    const auto typed = MacroExpansionEngine().expand(
        "#macro make() expression { 40 + 2 } int v = #expand make();");
    if (variant == 4)
    {
        const auto bad = MacroExpansionEngine().expand(
            "#macro bad() nonsense { 1 } void F() { #expand bad(); }");
        context.expect(!bad.success && bad.error.find(
                "macro context must be statement, expression, member, or type") != std::string::npos,
            "an unknown macro context is rejected before the AST is published");
        return;
    }
    context.expect(typed.success && typed.expansions.size() == 1 &&
            typed.expansions[0].context == MacroExpansionContext::Expression &&
            typed.expansions[0].macroName == "make",
        "a macro can declare an expression context and the record carries it");
    const auto memberContext = MacroExpansionEngine().expand(
        "#macro field() member { public int X; } class C { #expand field(); }");
    context.expect(memberContext.success && memberContext.expansions.size() == 1 &&
            memberContext.expansions[0].context == MacroExpansionContext::Member,
        "a member-context macro records the member kind");
    const auto statement = MacroExpansionEngine().expand(
        "#macro log() statement { int t = 1; } void F() { #expand log(); }");
    context.expect(statement.success && statement.expansions.size() == 1 &&
            statement.expansions[0].context == MacroExpansionContext::Statement,
        "an omitted context defaults to statement");
}

void macroSourceMappingOracle(TestContext& context, std::size_t variant)
{
    const auto mapped = MacroExpansionEngine().expand(
        "#macro inc(v) expression { $v + 1 } int a = #expand inc(1);");
    if (variant == 4)
    {
        const auto nested = MacroExpansionEngine().expand(
            "#macro outer() statement { #expand inner() } #macro inner() statement { int x = 1; } void F() { #expand outer() }");
        context.expect(nested.success && nested.expansions.size() == 2,
            "a nested expansion records the full chain");
        return;
    }
    context.expect(mapped.success && mapped.expansions.size() == 1 &&
            mapped.expansions[0].definitionOffset == 0 &&
            mapped.expansions[0].definitionLength != 0 &&
            mapped.expansions[0].invocationOffset > mapped.expansions[0].definitionOffset,
        "an expansion records both its invocation and its template source mapping");
    const std::string backtrace = macroExpansionBacktrace(mapped.expansions);
    context.expect(backtrace.find("#expand inc") != std::string::npos &&
            backtrace.find("invocation offset") != std::string::npos &&
            backtrace.find("template offset") != std::string::npos,
        "the expansion backtrace navigates to both invocation and template without fictitious spans");
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
    addMatrix(registry, 64, "component", typedDescriptorsOracle);
    addMatrix(registry, 65, "unit", macroContextOracle);
    addMatrix(registry, 66, "unit", macroSourceMappingOracle);
}
}
