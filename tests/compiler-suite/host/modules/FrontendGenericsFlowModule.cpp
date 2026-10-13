#include "FrontendContractSupport.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/OwnershipFlowSnapshot.h"
#include "nativesharp/Frontend/ProjectCallableAnalysisStatistics.h"
#include "nativesharp/Frontend/ProjectCallableBindingIndex.h"
#include "nativesharp/Frontend/ProjectCompilationDumper.h"
#include "nativesharp/Frontend/ProjectGenericModelDumper.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

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

std::string nominalGenericInferenceSource()
{
    return R"ns(
namespace Suite
{
    public class Algorithms
    {
        public static T Identity<T>(T value) { return value; }
    }
    public class Program
    {
        public static int Run()
        {
            int explicitValue = Algorithms.Identity<int>(1);
            int inferredValue = Algorithms.Identity(2);
            return explicitValue + inferredValue;
        }
    }
}
)ns";
}

std::string genericInferenceMatrixSource()
{
    return R"ns(
namespace Suite
{
    public delegate int Mapper<T>(T value);
    public struct Box<T> { public T Value; }
    public class Algorithms
    {
        public static T Identity<T>(T value) { return value; }
        public static T Make<T>() { return default(T); }
        public static T First<T>(T[] values) { return values[0]; }
        public static T Unbox<T>(Box<T> value) { return value.Value; }
        public static int Apply<T>(Mapper<T> mapper) { return mapper(default(T)); }
        public static int PackCount<pack Ts>() { return packcount(Ts); }
    }
    public class Program
    {
        public static int Run()
        {
            int inferred = Algorithms.Identity(1);
            long explicitValue = Algorithms.Identity<long>(2);
            int expected = Algorithms.Make();
            int[] values = new int[1];
            values[0] = 3;
            int arrayElement = Algorithms.First(values);
            Box<int> box;
            box.Value = 4;
            int nested = Algorithms.Unbox(box);
            int lambda = Algorithms.Apply((int value) => value);
            int emptyPack = Algorithms.PackCount<>();
            int typePack = Algorithms.PackCount<int, long>();
            delete[] values;
            return inferred + (int)explicitValue + expected + arrayElement + nested +
                lambda + emptyPack + typePack;
        }
    }
}
)ns";
}

std::string genericInferenceBoundarySource()
{
    return R"ns(
namespace Suite
{
    public struct Fixed<T, const int N>
    {
        public int Count() { return N; }
    }
    public class Algorithms
    {
        public static int PackCount<pack Ts>() { return packcount(Ts); }
    }
    public class Program
    {
        public Fixed<int, 0> Minimum;
        public Fixed<int, 1> Adjacent;
        public static int Run()
        {
            int emptyPack = Algorithms.PackCount<>();
            int oneType = Algorithms.PackCount<int>();
            int twoTypes = Algorithms.PackCount<int, long>();
            return emptyPack + oneType + twoTypes;
        }
    }
}
)ns";
}

std::string genericInferencePackBoundarySource(std::size_t argumentCount)
{
    std::string source =
        "class Program\n"
        "{\n"
        "    private static int PackCount<pack Ts>() { return packcount(Ts); }\n"
        "    public static int Main() { return PackCount<";
    for (std::size_t index = 0; index < argumentCount; ++index)
    {
        if (index != 0) source += ", ";
        source += "int";
    }
    source += ">(); }\n}\n";
    return source;
}

std::string genericInferenceScaleSource()
{
    constexpr std::size_t fanout = 32;
    constexpr std::size_t depth = 16;
    std::string source =
        "namespace Suite\n"
        "{\n"
        "    public struct Box<T> { public T Value; }\n";
    for (std::size_t index = 0; index < fanout; ++index)
    {
        source += "    public struct Item" + std::to_string(index) +
            " { public int Value; }\n";
    }
    for (std::size_t index = 0; index < depth; ++index)
    {
        source += "    public struct Layer" + std::to_string(index) + "<T> { public ";
        if (index + 1 == depth)
            source += "T Value; }\n";
        else
            source += "Layer" + std::to_string(index + 1) + "<T> Next; }\n";
    }
    source += "    public class Program\n    {\n";
    for (std::size_t index = 0; index < fanout; ++index)
    {
        source += "        public Box<Item" + std::to_string(index) + "> Field" +
            std::to_string(index) + ";\n";
    }
    source +=
        "        public Layer0<int> Deep;\n"
        "        public static int Run() { return 0; }\n"
        "    }\n"
        "}\n";
    return source;
}

const GenericSpecializationRecord* specializationNamed(
    const ProjectGenericModel& model,
    std::string_view name)
{
    for (const GenericSpecializationRecord& record : model.specializations())
    {
        if (record.specializationKey.find(name) != std::string::npos) return &record;
    }
    return nullptr;
}

std::size_t specializationCountNamed(
    const ProjectGenericModel& model,
    std::string_view name)
{
    std::size_t count = 0;
    for (const GenericSpecializationRecord& record : model.specializations())
    {
        if (record.specializationKey.find(name) != std::string::npos) ++count;
    }
    return count;
}

bool hasSpecializationArguments(
    const ProjectGenericModel& model,
    std::string_view name,
    std::initializer_list<std::string_view> arguments)
{
    for (const GenericSpecializationRecord& record : model.specializations())
    {
        if (record.specializationKey.find(name) == std::string::npos ||
            record.typeArguments.size() != arguments.size())
            continue;
        bool equal = true;
        std::size_t index = 0;
        for (std::string_view argument : arguments)
        {
            if (record.typeArguments[index++] != argument)
            {
                equal = false;
                break;
            }
        }
        if (equal) return true;
    }
    return false;
}

bool hasConstSpecializationArguments(
    const ProjectGenericModel& model,
    std::string_view name,
    std::string_view typeArgument,
    std::string_view constValue)
{
    const std::string suffix = "=" + std::string(constValue);
    for (const GenericSpecializationRecord& record : model.specializations())
    {
        if (record.specializationKey.find(name) != std::string::npos &&
            record.typeArguments.size() == 2 &&
            record.typeArguments[0] == typeArgument &&
            record.typeArguments[1].starts_with("const:") &&
            record.typeArguments[1].ends_with(suffix))
            return true;
    }
    return false;
}

struct GenericObservation final
{
    bool completed = false;
    std::string dump;
    std::vector<std::string> signatures;
};

GenericObservation observeGenericProject(
    TestContext& context,
    std::string_view label,
    std::string source)
{
    TemporaryProject fixture(label, std::move(source));
    ProjectCompilation compilation;
    std::string error;
    GenericObservation observation;
    observation.completed = compileTo(
        compilation, ProjectStage::MethodBodies, fixture.projectFile(), error);
    const std::string evidence = projectFailureEvidence(compilation, error);
    context.expect(observation.completed,
        "determinism fixture must reach method bodies; " + evidence);
    context.expect(error.empty(),
        "determinism fixture must not retain a load error; " + evidence);
    if (!observation.completed || compilation.genericModel() == nullptr) return observation;

    observation.dump = ProjectGenericModelDumper::dump(*compilation.genericModel());
    for (const GenericSpecializationRecord& record : compilation.genericModel()->specializations())
    {
        std::string signature = record.specializationKey;
        for (const std::string& argument : record.typeArguments)
            signature += "|" + argument;
        observation.signatures.push_back(std::move(signature));
    }
    std::sort(observation.signatures.begin(), observation.signatures.end());
    return observation;
}

std::string deterministicGenericInferenceSource(bool permuted)
{
    const std::string calls = permuted ?
        "            long second = Algorithms.Identity<long>(2);\n"
        "            int first = Algorithms.Identity(1);\n" :
        "            int first = Algorithms.Identity(1);\n"
        "            long second = Algorithms.Identity<long>(2);\n";
    return
        "namespace Suite\n"
        "{\n"
        "    public class Algorithms\n"
        "    {\n"
        "        public static T Identity<T>(T value) { return value; }\n"
        "    }\n"
        "    public class Program\n"
        "    {\n"
        "        public static int Run()\n"
        "        {\n" + calls +
        "            return first + (int)second;\n"
        "        }\n"
        "    }\n"
        "}\n";
}

void canonicalClosedTypeIdentityOracle(TestContext& context)
{
    TemporaryProject fixture(
        "front-21-6-closed-type-identity",
        R"ns(
namespace NativeSharp.Core
{
    public borrowed struct Span<T> { public T Value; }
    public borrowed struct ReadOnlySpan<T> { public T Value; }
}
namespace Suite
{
    public class Program
    {
        public static int First(int[] values)
        {
            match(values)
            {
                [.. var rest] => { },
                _ => { }
            }
            return 1;
        }
        public static int Repeated(int[] values)
        {
            match(values)
            {
                [.. var rest] => { },
                _ => { }
            }
            return 2;
        }
    }
}
)ns");
    ProjectCompilation compilation;
    std::string error;
    const bool completed = compileTo(
        compilation, ProjectStage::MethodBodies, fixture.projectFile(), error);
    context.expect(completed,
        "closed-type identity fixture must reach method-body analysis; " +
            projectFailureEvidence(compilation, error));
    const ProjectGenericModel* model = compilation.genericModel();
    if (!completed || model == nullptr) return;

    const std::string first = model->closedTypeKeyForTemplate(
        "Span", "type:FirstModule::Shared.Item");
    const std::string second = model->closedTypeKeyForTemplate(
        "Span", "type:SecondModule::Shared.Item");
    const std::string third = model->closedTypeKeyForTemplate(
        "Span", "type:FirstModule::Other.Item");
    const std::string repeated = model->closedTypeKeyForTemplate(
        "Span", "type:FirstModule::Shared.Item");
    context.expect(!first.empty() && !second.empty() && !third.empty(),
        "closed Span identities must be available for canonical named arguments");
    context.expect(first != second && first != third && second != third,
        "homonymous argument types from different modules or namespaces must not collide in "
        "Span identity");
    context.expect(first == repeated,
        "the same canonical argument identity must produce a stable closed Span key");

    const auto templateKey = [&](std::string_view name) -> std::string
    {
        const auto found = std::ranges::find_if(model->templates(), [&](const auto& record)
        {
            return record.name == name;
        });
        return found == model->templates().end() ? std::string{} : found->canonicalKey;
    };
    const std::string spanTemplate = templateKey("Span");
    const std::string readOnlySpanTemplate = templateKey("ReadOnlySpan");
    const auto materializationCount = [&](const std::string& key)
    {
        return std::ranges::count_if(model->specializations(), [&](const auto& record)
        {
            return record.templateKey == key && record.typeArguments.size() == 1U &&
                record.typeArguments.front() == "int";
        });
    };
    context.expect(!spanTemplate.empty() && !readOnlySpanTemplate.empty(),
        "body-demand fixture must discover both canonical view templates");
    context.expect(materializationCount(spanTemplate) == 1,
        "repeated array rest captures must materialize Span<int> exactly once before Sema");
    context.expect(materializationCount(readOnlySpanTemplate) == 1,
        "array rest capture must pre-materialize ReadOnlySpan<int> for readonly receivers");
    const ProjectMethodBodyModel* methodModel = compilation.methodBodyModel();
    context.expect(methodModel != nullptr,
        "body-demand fixture must publish method-body bindings");
    if (methodModel != nullptr)
    {
        const auto arrayViews = std::ranges::count_if(
            methodModel->coreBindings(), [](const MethodCoreBinding& binding)
            {
                return binding.kind == MethodCoreOperationKind::SpanFromArray ||
                    binding.kind == MethodCoreOperationKind::ReadOnlySpanFromArray;
            });
        context.expect(arrayViews == 2,
            "both body requests must consume a pre-materialized Span<int>/ReadOnlySpan<int> binding");
    }
}

// Proves that generic discovery completed without synthesizing templates or
// specializations for an empty translation unit.
void emptyGenericOracle(TestContext& context, const ProjectCompilation& compilation)
{
    const ProjectGenericModel* model = compilation.genericModel();
    context.expect(model != nullptr && model->isBuilt(),
        "empty source must still publish a built generic model");
    if (model != nullptr)
    {
        context.expect(model->templates().empty(),
            "empty source must not publish generic templates");
        context.expect(model->specializations().empty(),
            "empty source must not publish generic specializations");
    }
}

// Extends the generic absence contract through method-body analysis, proving
// that the downstream stage did not analyze a nonexistent callable.
void emptyGenericInferenceOracle(
    TestContext& context,
    const ProjectCompilation& compilation)
{
    emptyGenericOracle(context, compilation);

    const ProjectMethodBodyModel* methodModel = compilation.methodBodyModel();
    context.expect(methodModel != nullptr && methodModel->isBuilt(),
        "empty source must still publish a built method-body model");
    if (methodModel != nullptr)
    {
        context.expect(
            methodModel->callableAnalysisStatistics().callablesAnalyzed == 0,
            "empty source must not analyze generic callables");
    }
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
        },
        emptyGenericOracle);
}

void genericInferenceOracle(TestContext& context, std::size_t variant)
{
    if (variant == 6)
    {
        const std::string source = deterministicGenericInferenceSource(false);
        const GenericObservation first = observeGenericProject(
            context, "front-21-6-first", source);
        const GenericObservation repeated = observeGenericProject(
            context, "front-21-6-repeated", source);
        const GenericObservation permuted = observeGenericProject(
            context, "front-21-6-permuted", deterministicGenericInferenceSource(true));
        if (first.completed && repeated.completed && permuted.completed)
        {
            context.expect(first.dump == repeated.dump,
                "the same generic input must produce a byte-identical model dump");
            context.expect(first.signatures == permuted.signatures,
                "permuting independent generic calls must preserve normalized specialization identities");
        }
        canonicalClosedTypeIdentityOracle(context);
        return;
    }

    std::string source = nominalGenericInferenceSource();
    if (variant == 1)
        source = genericInferenceMatrixSource();
    else if (variant == 3)
        source = genericInferenceBoundarySource();
    else if (variant == 7)
        source = genericInferenceScaleSource();

    expectProjectStage(context, 21, variant, ProjectStage::MethodBodies, std::move(source),
        [variant](TestContext& test, const ProjectCompilation& compilation)
        {
            const ProjectGenericModel* model = compilation.genericModel();
            const ProjectMethodBodyModel* methodModel = compilation.methodBodyModel();
            test.expect(model != nullptr && methodModel != nullptr,
                "generic inference must retain specialization and analyzed callable models");
            if (model == nullptr || methodModel == nullptr) return;

            if (variant == 0)
            {
                const GenericSpecializationRecord* identity =
                    specializationNamed(*model, "Identity``1<int>");
                test.expect(identity != nullptr,
                    "explicit and inferred calls must materialize Identity<int>");
                if (identity != nullptr)
                {
                    test.expect(identity->typeArguments.size() == 1 &&
                            identity->typeArguments[0] == "int",
                        "specialization arguments must retain the source identity 'int'");
                    test.expect(identity->specializedSyntax != nullptr &&
                            !identity->specializedName.empty(),
                        "the nominal specialization must retain its cloned syntax and stable name");
                }
                test.expect(specializationCountNamed(*model, "Identity``1<int>") == 1,
                    "explicit and inferred calls must reuse one canonical Identity<int> specialization");
                test.expect(model->instantiationReport().cacheHitCount >= 1,
                    "reusing Identity<int> must be observable as a specialization-cache hit");
            }
            else if (variant == 1)
            {
                constexpr std::array<std::string_view, 6> required = {
                    "Identity``1<int>", "Identity``1<long>", "Make``1<int>",
                    "First``1<int>", "Unbox``1<int>", "Apply``1<int>",
                };
                for (std::string_view identity : required)
                {
                    test.expect(specializationNamed(*model, identity) != nullptr,
                        "valid inference matrix must materialize " + std::string(identity));
                }
                test.expect(hasSpecializationArguments(*model, "PackCount", {}),
                    "valid inference matrix must materialize the empty type pack");
                test.expect(hasSpecializationArguments(*model, "PackCount", {"int", "long"}),
                    "valid inference matrix must materialize the multi-element type pack");
                test.expect(specializationCountNamed(*model, "Identity``1<int>") == 1,
                    "explicitly equivalent normalized arguments must retain one identity");
            }
            else if (variant == 3)
            {
                test.expect(hasConstSpecializationArguments(*model, "Fixed", "int", "0"),
                    "boundary matrix must materialize the minimum const argument");
                test.expect(hasConstSpecializationArguments(*model, "Fixed", "int", "1"),
                    "boundary matrix must materialize the adjacent const argument");
                test.expect(hasSpecializationArguments(*model, "PackCount", {}),
                    "boundary matrix must materialize the minimum empty pack");
                test.expect(hasSpecializationArguments(*model, "PackCount", {"int"}),
                    "boundary matrix must materialize the first adjacent pack");
                test.expect(hasSpecializationArguments(*model, "PackCount", {"int", "long"}),
                    "boundary matrix must materialize the next adjacent pack");
                const GenericInstantiationReport report = model->instantiationReport();
                test.expect(report.maximumDepth == ProjectGenericModel::DefaultMaximumInstantiationDepth &&
                        report.maximumCount == ProjectGenericModel::DefaultMaximumInstantiationCount,
                    "boundary matrix must publish the exact configured instantiation limits");
            }
            else if (variant == 7)
            {
                const GenericInstantiationReport report = model->instantiationReport();
                test.expect(model->specializations().size() >= 48 && report.materializedCount >= 48,
                    "scale fixture must materialize its 32-way fanout and 16-level dependency chain");
                test.expect(report.maximumObservedDepth >= 16,
                    "scale fixture must exercise at least sixteen nested generic instantiations");
                test.expect(report.maximumObservedDepth <= report.maximumDepth &&
                        report.materializedCount <= report.maximumCount,
                    "scale fixture must remain inside the published resource limits");
                const ProjectCallableAnalysisStatistics& statistics =
                    methodModel->callableAnalysisStatistics();
                test.expect(statistics.requestedWorkers == 4,
                    "scale variant must retain the four-worker compilation request");
            }
        },
        emptyGenericInferenceOracle);

    if (variant == 3)
    {
        TemporaryProject maximumFixture(
            "front-21-3-maximum",
            genericInferencePackBoundarySource(
                ProjectGenericModel::DefaultMaximumPackExpansionCount));
        ProjectCompilation maximum;
        std::string maximumError;
        const bool maximumCompleted = compileTo(
            maximum, ProjectStage::MethodBodies, maximumFixture.projectFile(), maximumError);
        context.expect(maximumCompleted,
            "a pack at the public expansion limit must complete; " +
                projectFailureEvidence(maximum, maximumError));
        if (maximumCompleted && maximum.genericModel() != nullptr)
        {
            bool retainedMaximum = false;
            for (const GenericSpecializationRecord& record :
                 maximum.genericModel()->specializations())
            {
                if (record.specializationKey.find("PackCount") != std::string::npos &&
                    record.typeArguments.size() ==
                        ProjectGenericModel::DefaultMaximumPackExpansionCount)
                {
                    retainedMaximum = true;
                    break;
                }
            }
            context.expect(retainedMaximum,
                "a pack at the public expansion limit must retain every argument");
        }

        TemporaryProject fixture(
            "front-21-3-first-outside",
            genericInferencePackBoundarySource(
                ProjectGenericModel::DefaultMaximumPackExpansionCount + 1));
        ProjectCompilation rejected;
        std::string error;
        const bool completed = compileTo(
            rejected, ProjectStage::MethodBodies, fixture.projectFile(), error);
        context.expect(!completed,
            "the first pack beyond the public expansion limit must be rejected");
        context.expect(!error.empty() || rejected.diagnostics().errorCount() != 0,
            "the first pack beyond the public expansion limit must retain causal diagnostics");
    }
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
        },
        emptyGenericOracle);
}

void tupleConversionScaleOracle(TestContext& context)
{
    constexpr std::size_t fanout = 64;
    constexpr std::size_t depth = 16;

    std::vector<std::string> sourceElements;
    std::vector<std::string> targetElements;
    sourceElements.reserve(fanout);
    targetElements.reserve(fanout);
    for (std::size_t index = 0; index < fanout; ++index)
    {
        sourceElements.push_back(index % 2 == 0 ? "builtin:int" : "builtin:short");
        targetElements.push_back("builtin:long");
    }

    const std::string wideSource = ProjectTypeConversion::tupleTypeKey(sourceElements);
    const std::string wideTarget = ProjectTypeConversion::tupleTypeKey(targetElements);
    const std::optional<std::vector<std::string>> decoded =
        ProjectTypeConversion::tupleElementTypeKeys(wideSource);
    context.expect(decoded.has_value() && *decoded == sourceElements,
        "wide tuple conversion source must retain all canonical element identities");

    const ProjectImplicitConversion wide =
        ProjectTypeConversion::classifyImplicit(wideSource, wideTarget);
    context.expect(wide.exists() && wide.kind == ProjectConversionKind::Tuple && wide.rank == 4,
        "64-way tuple conversion must retain its structural kind and maximum element rank");
    context.expect(wide.sourceTypeKey == wideSource && wide.targetTypeKey == wideTarget,
        "wide tuple conversion must retain its exact canonical source and target payload");

    std::string nestedSource = "builtin:int";
    std::string nestedTarget = "builtin:long";
    for (std::size_t level = 0; level < depth; ++level)
    {
        const std::array<std::string, 2> source = {nestedSource, "builtin:int"};
        const std::array<std::string, 2> target = {nestedTarget, "builtin:long"};
        nestedSource = ProjectTypeConversion::tupleTypeKey(source);
        nestedTarget = ProjectTypeConversion::tupleTypeKey(target);
    }
    const ProjectImplicitConversion nested =
        ProjectTypeConversion::classifyImplicit(nestedSource, nestedTarget);
    context.expect(nested.exists() && nested.kind == ProjectConversionKind::Tuple &&
            nested.rank == static_cast<int>(depth + 3),
        "sixteen nested tuple levels must terminate with the accumulated structural rank");
    context.expect(nested.sourceTypeKey == nestedSource &&
            nested.targetTypeKey == nestedTarget,
        "nested tuple conversion must retain its exact canonical source and target payload");

    targetElements.pop_back();
    const std::string wrongArityTarget =
        ProjectTypeConversion::tupleTypeKey(targetElements);
    const ProjectImplicitConversion rejected =
        ProjectTypeConversion::classifyImplicit(wideSource, wrongArityTarget);
    context.expect(!rejected.exists(),
        "tuple conversion must reject the first structurally incompatible arity");
}

void conversionOracle(TestContext& context, std::size_t variant)
{
    if (variant == 7)
    {
        tupleConversionScaleOracle(context);
        return;
    }

    constexpr std::array<std::string_view, 7> sources = {
        "builtin:byte", "builtin:short", "builtin:int", "builtin:uint",
        "builtin:float", "builtin:null", "builtin:int*",
    };
    constexpr std::array<std::string_view, 7> targets = {
        "builtin:int", "builtin:int", "builtin:long", "builtin:ulong",
        "builtin:double", "class:Suite.Item", "builtin:void*",
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
