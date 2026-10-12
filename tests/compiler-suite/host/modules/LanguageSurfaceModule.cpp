#include "FrontendContractSupport.h"
#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Language/ErrorSemantics.h"
#include "nativesharp/Language/Language3Surface.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;
using namespace frontend_contract_support;

struct ExpectedSurfaceEntry final
{
    Language3Feature feature;
    std::string_view name;
    Language3Disposition disposition;
    std::string_view rationale;
};

constexpr std::array<ExpectedSurfaceEntry, 16> kExpectedSurface = {{
    {Language3Feature::RecordStructs, "record-structs", Language3Disposition::Implemented,
        "productive parser, layout, metadata and code-generation vertical"},
    {Language3Feature::PrimaryConstructors, "primary-constructors", Language3Disposition::Implemented,
        "lowered to ordinary fields and constructors before name binding"},
    {Language3Feature::RawStringLiterals, "raw-string-literals", Language3Disposition::Implemented,
        "delimiter-aware UTF-8 literal with exact multiline contents"},
    {Language3Feature::AsyncAwait, "async-await", Language3Disposition::PlannedFor304,
        "requires productive state-machine lowering, cleanup, pinning, cancellation and debugger metadata"},
    {Language3Feature::AsyncStreams, "async-streams", Language3Disposition::PlannedFor304,
        "requires an async iterator ABI with backpressure, cancellation and deterministic cleanup"},
    {Language3Feature::YieldGenerators, "yield-generators", Language3Disposition::PlannedFor304,
        "requires productive generator frames, suspension ownership and deterministic cleanup"},
    {Language3Feature::DefaultInterfaceImplementations, "default-interface-implementations",
        Language3Disposition::PlannedFor304,
        "requires versioned dispatch, coherence and metadata rules for interface bodies"},
    {Language3Feature::ListPatterns, "list-patterns", Language3Disposition::PlannedFor304,
        "requires exhaustiveness and usefulness analysis integrated with borrow and move semantics"},
    {Language3Feature::StaticConstructors, "static-constructors", Language3Disposition::PlannedFor304,
        "requires thread-safe once initialization, reentrancy and failure semantics"},
    {Language3Feature::Decimal, "decimal", Language3Disposition::PlannedFor304,
        "requires a frozen value ABI plus arithmetic, overflow, conversion and formatting contracts"},
    {Language3Feature::ExtensionMethods, "extension-methods", Language3Disposition::Implemented,
        "static namespace extensions with deterministic import lookup and no receiver-type mutation"},
    {Language3Feature::TraitOperators, "trait-operators", Language3Disposition::Implemented,
        "stable lang-item witnesses with builtin precedence and intrinsic short-circuit control flow"},
    {Language3Feature::MoveOnlyDefaults, "move-only-defaults", Language3Disposition::Implemented,
        "value types require explicit Copy and named consuming reads require move(value)"},
    {Language3Feature::LongDouble, "long-double", Language3Disposition::RejectedFor3,
        "Windows x64 MSVC ABI aliases long double to double; use double in NativeSharp 3.0"},
    {Language3Feature::ConditionDeclarations, "condition-declarations", Language3Disposition::PlannedFor304,
        "requires scope, definite-assignment and cleanup rules across true and false edges"},
    {Language3Feature::StaticLocals, "static-locals", Language3Disposition::PlannedFor304,
        "requires thread-safe once initialization, reentrancy, destruction and module-lifetime rules"},
}};

class TemporaryModuleInitializationGraph final
{
public:
    explicit TemporaryModuleInitializationGraph(bool cyclic = false)
    {
        static std::atomic<std::uint64_t> sequence{0};
        directory_ = std::filesystem::current_path() /
            ("nsc-language-module-init-" +
             std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)));
        std::filesystem::create_directories(directory_);
        write("Producer.ns", "public class ProducerGate { static ProducerGate() { } }");
        write("Consumer.ns", "public class ConsumerGate { static ConsumerGate() { } }");
        write("Producer.nsproj",
            "<Project><PropertyGroup><Name>Producer</Name><OutputType>Library</OutputType>"
            "</PropertyGroup><ItemGroup><Compile Include=\"Producer.ns\"/>" +
            std::string(cyclic ? "<ProjectReference Include=\"Consumer.nsproj\"/>" : "") +
            "</ItemGroup></Project>");
        write("Consumer.nsproj",
            "<Project><PropertyGroup><Name>Consumer</Name><OutputType>Library</OutputType>"
            "</PropertyGroup><ItemGroup><Compile Include=\"Consumer.ns\"/>"
            "<ProjectReference Include=\"Producer.nsproj\"/></ItemGroup></Project>");
    }

    TemporaryModuleInitializationGraph(const TemporaryModuleInitializationGraph&) = delete;
    TemporaryModuleInitializationGraph& operator=(const TemporaryModuleInitializationGraph&) = delete;

    ~TemporaryModuleInitializationGraph()
    {
        std::error_code ignored;
        for (const std::string_view name : {
                 "Producer.ns", "Consumer.ns", "Producer.nsproj", "Consumer.nsproj"})
        {
            std::filesystem::remove(directory_ / name, ignored);
        }
        std::filesystem::remove(directory_, ignored);
    }

    std::filesystem::path rootProject() const { return directory_ / "Consumer.nsproj"; }

private:
    void write(std::string_view name, std::string_view contents)
    {
        std::ofstream output(directory_ / name, std::ios::binary | std::ios::trunc);
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    }

    std::filesystem::path directory_;
};

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-012", "Host.Language", "unit", "PR",
        {"language", "historical-surface"}, std::move(function)});
}

std::string expectedManifest()
{
    return
        "NSLANGSURFACE 3.0\n"
        "target=x86_64-pc-windows-msvc\n"
        "policy=implemented-planned-or-explicitly-deferred-no-placeholder-syntax\n"
        "record-structs=implemented:productive parser, layout, metadata and code-generation vertical\n"
        "primary-constructors=implemented:lowered to ordinary fields and constructors before name binding\n"
        "raw-string-literals=implemented:delimiter-aware UTF-8 literal with exact multiline contents\n"
        "async-await=planned-3.0.4:requires productive state-machine lowering, cleanup, pinning, cancellation "
        "and debugger metadata\n"
        "async-streams=planned-3.0.4:requires an async iterator ABI with backpressure, cancellation and "
        "deterministic cleanup\n"
        "yield-generators=planned-3.0.4:requires productive generator frames, suspension ownership and "
        "deterministic cleanup\n"
        "default-interface-implementations=planned-3.0.4:requires versioned dispatch, coherence and metadata "
        "rules for interface bodies\n"
        "list-patterns=planned-3.0.4:requires exhaustiveness and usefulness analysis integrated with borrow "
        "and move semantics\n"
        "static-constructors=planned-3.0.4:requires thread-safe once initialization, reentrancy and failure "
        "semantics\n"
        "decimal=planned-3.0.4:requires a frozen value ABI plus arithmetic, overflow, conversion and formatting "
        "contracts\n"
        "extension-methods=implemented:static namespace extensions with deterministic import lookup and no "
        "receiver-type mutation\n"
        "trait-operators=implemented:stable lang-item witnesses with builtin precedence and intrinsic "
        "short-circuit control flow\n"
        "move-only-defaults=implemented:value types require explicit Copy and named consuming reads require "
        "move(value)\n"
        "long-double=rejected-3.0:Windows x64 MSVC ABI aliases long double to double; use double in "
        "NativeSharp 3.0\n"
        "condition-declarations=planned-3.0.4:requires scope, definite-assignment and cleanup rules across true "
        "and false edges\n"
        "static-locals=planned-3.0.4:requires thread-safe once initialization, reentrancy, destruction and "
        "module-lifetime rules\n";
}

std::string expectedDiagnostic(const ExpectedSurfaceEntry& entry)
{
    std::string result(entry.name);
    if (entry.disposition == Language3Disposition::Implemented)
        result += " is implemented in NativeSharp 3.0: ";
    else if (entry.disposition == Language3Disposition::PlannedFor304)
        result += " is planned for NativeSharp 3.0.4 but is not implemented because ";
    else if (entry.disposition == Language3Disposition::DeferredTo4)
        result += " is not part of the frozen NativeSharp 3.0 surface; deferred to 4.0 because ";
    else
        result += " is not supported by NativeSharp 3.0 because ";
    result += entry.rationale;
    return result;
}

void registerSurfaceCatalogCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-155", [](TestContext& context) {
        const auto surface = language3Surface();
        context.expect(surface.size() == kExpectedSurface.size(),
            "TL-LANG-155 the frozen surface must contain exactly sixteen entries");
        if (surface.size() != kExpectedSurface.size()) return;
        for (std::size_t index = 0; index < kExpectedSurface.size(); ++index)
        {
            const auto& actual = surface[index];
            const auto& expected = kExpectedSurface[index];
            context.expect(actual.feature == expected.feature && actual.name == expected.name &&
                    actual.disposition == expected.disposition && actual.rationale == expected.rationale,
                "TL-LANG-155 feature, name, disposition, rationale, and order must be exact");
        }
    });
}

void registerSurfaceLookupCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-156", [](TestContext& context) {
        const auto surface = language3Surface();
        for (std::size_t index = 0; index < kExpectedSurface.size(); ++index)
        {
            const auto& entry = language3SurfaceEntry(kExpectedSurface[index].feature);
            context.expect(&entry == &surface[index] && entry.feature == kExpectedSurface[index].feature,
                "TL-LANG-156 every valid enum must return a reference to its corresponding table entry");
        }
    });
}

void registerSurfaceExternalEnumCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-157", [](TestContext& context) {
        const auto surface = language3Surface();
        constexpr std::array externalValues = {
            -1, 16, 17, (std::numeric_limits<int>::max)()};
        for (const int value : externalValues)
        {
            const auto& entry = language3SurfaceEntry(static_cast<Language3Feature>(value));
            context.expect(&entry == &surface.front() && entry.feature == Language3Feature::RecordStructs,
                "TL-LANG-157 every external enum value must use the characterized first-entry fallback");
        }
    });
}

void registerSurfaceManifestCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-158", [](TestContext& context) {
        const std::string expected = expectedManifest();
        const std::string actual = language3SurfaceManifest();
        context.expect(actual == expected,
            "TL-LANG-158 the complete manifest bytes, ordering, labels, and trailing newline must be exact");
        context.expect(actual.size() == expected.size() && !actual.empty() && actual.back() == '\n',
            "TL-LANG-158 manifest length and terminal newline must remain explicit");
    });
}

void registerImplementedDiagnosticCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-159", [](TestContext& context) {
        for (const ExpectedSurfaceEntry& entry : kExpectedSurface)
        {
            if (entry.disposition != Language3Disposition::Implemented) continue;
            context.expect(language3SurfaceDiagnostic(entry.feature) == expectedDiagnostic(entry),
                "TL-LANG-159 every implemented feature must return its exact historical diagnostic");
        }
    });
}

void registerUnavailableDiagnosticCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-160", [](TestContext& context) {
        std::size_t planned = 0;
        std::size_t deferred = 0;
        std::size_t rejected = 0;
        for (const ExpectedSurfaceEntry& entry : kExpectedSurface)
        {
            if (entry.disposition == Language3Disposition::Implemented) continue;
            planned += static_cast<std::size_t>(entry.disposition == Language3Disposition::PlannedFor304);
            deferred += static_cast<std::size_t>(entry.disposition == Language3Disposition::DeferredTo4);
            rejected += static_cast<std::size_t>(entry.disposition == Language3Disposition::RejectedFor3);
            context.expect(language3SurfaceDiagnostic(entry.feature) == expectedDiagnostic(entry),
                "TL-LANG-160 every planned, deferred, or rejected feature must return its exact status diagnostic");
        }
        context.expect(planned == 9 && deferred == 0 && rejected == 1,
            "TL-LANG-160 the catalog must expose nine nonproductive 3.0.4 plans and one rejected feature");
    });
}

void registerSurfaceDeterminismCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-161", [](TestContext& context) {
        const auto firstSurface = language3Surface();
        const auto secondSurface = language3Surface();
        const std::string firstManifest = language3SurfaceManifest();
        const std::string secondManifest = language3SurfaceManifest();
        context.expect(firstSurface.data() == secondSurface.data() &&
                firstSurface.size() == secondSurface.size() && firstManifest == secondManifest,
            "TL-LANG-161 repeated surface and manifest calls must be stable and deterministic");

        for (std::size_t index = 0; index < kExpectedSurface.size(); ++index)
        {
            const auto& expected = kExpectedSurface[index];
            const std::string firstDiagnostic = language3SurfaceDiagnostic(expected.feature);
            const std::string secondDiagnostic = language3SurfaceDiagnostic(expected.feature);
            context.expect(firstDiagnostic == secondDiagnostic &&
                    firstSurface[index].name == expected.name &&
                    firstSurface[index].rationale == expected.rationale,
                "TL-LANG-161 repeated diagnostics must not mutate the static catalog");
        }
        context.expect(language3SurfaceManifest() == firstManifest,
            "TL-LANG-161 diagnostics must not mutate subsequent manifest bytes");
    });
}

void registerStaticInitializationStatesCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-294", "CL-LANG-012", "Host.Language", "unit", "PR",
        {"language", "static-initialization", "state"}, [](TestContext& context) {
            constexpr std::array states = {
                StaticInitializationState::Uninitialized,
                StaticInitializationState::Initializing,
                StaticInitializationState::Initialized,
                StaticInitializationState::Failed};
            constexpr std::array<std::string_view, 4> names = {
                "uninitialized", "initializing", "initialized", "failed"};

            for (std::size_t index = 0; index < states.size(); ++index)
            {
                context.expect(static_cast<std::size_t>(states[index]) == index &&
                        staticInitializationStateName(states[index]) == names[index],
                    "TL-LANG-294 static initialization states and names must retain exact order");
            }
            context.expect(staticInitializationStateName(
                    static_cast<StaticInitializationState>(states.size())) == "unknown",
                "TL-LANG-294 an external state value must remain explicitly unknown");
        }});
}

void registerStaticConstructorDeclarationCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-295", "CL-LANG-012", "Host.Language", "component", "PR",
        {"language", "static-initialization", "constructor"}, [](TestContext& context) {
            const std::string source = R"ns(
namespace Suite
{
    public class Gate
    {
        private static int Value;
        static Gate() { Value = 7; }
        public Gate() { }
        public static int Read() { return Value; }
    }
}
)ns";
            TemporaryProject project("tl-lang-295-positive", source);
            nativesharp::frontend::ProjectCompilation compilation;
            std::string error;
            const bool compiled = compileTo(
                compilation, ProjectStage::Mir, project.projectFile(), error);
            context.expect(compiled && error.empty(),
                "TL-LANG-295 a static and an instance constructor must compile together through MIR: " +
                    projectFailureEvidence(compilation, error));

            const auto* lookup = compilation.nameLookup();
            const auto* members = compilation.memberModel();
            const auto* mir = compilation.oopMirModel();
            if (!compiled || lookup == nullptr || members == nullptr || mir == nullptr)
            {
                return;
            }

            std::optional<std::size_t> gateType;
            for (std::size_t typeOrdinal = 0; typeOrdinal < lookup->namedTypes().size(); ++typeOrdinal)
            {
                if (lookup->namedTypes()[typeOrdinal].identity.fullyQualifiedName().ends_with("Gate"))
                {
                    gateType = typeOrdinal;
                    break;
                }
            }
            context.expect(gateType.has_value(),
                "TL-LANG-295 the containing type must retain its canonical identity");
            if (!gateType.has_value())
            {
                return;
            }

            std::optional<std::size_t> staticConstructor;
            std::size_t instanceConstructors = 0;
            for (const std::size_t memberOrdinal : members->membersOfType(*gateType))
            {
                const auto& member = members->members()[memberOrdinal];
                if (member.kind != nativesharp::frontend::ClassMemberKind::Constructor)
                {
                    continue;
                }
                if (member.isStatic)
                {
                    staticConstructor = memberOrdinal;
                    context.expect(!member.hasImplicitThis && member.parameters.empty(),
                        "TL-LANG-295 a static constructor has no implicit this or source parameters");
                }
                else
                {
                    ++instanceConstructors;
                }
            }
            context.expect(staticConstructor.has_value() && instanceConstructors == 1U &&
                    members->constructorsOfType(*gateType).size() == 1U,
                "TL-LANG-295 static construction must remain outside new/base/this overload sets");

            bool staticMir = false;
            if (staticConstructor.has_value())
            {
                for (const auto& function : mir->functions())
                {
                    if (function.memberOrdinal == *staticConstructor)
                    {
                        staticMir = function.constructor && function.isStatic &&
                            function.parameterTypeKeys.empty() && function.body != nullptr;
                        break;
                    }
                }
            }
            context.expect(staticMir,
                "TL-LANG-295 the static constructor body must lower as void static MIR without this");

            const auto expectRejected = [&](std::string_view label, std::string invalidSource,
                                            ProjectStage stage, std::string_view diagnostic) {
                TemporaryProject invalidProject(label, std::move(invalidSource));
                nativesharp::frontend::ProjectCompilation invalidCompilation;
                std::string invalidError;
                const bool accepted = compileTo(
                    invalidCompilation, stage, invalidProject.projectFile(), invalidError);
                const std::string evidence = projectFailureEvidence(
                    invalidCompilation, invalidError);
                context.expect(!accepted && evidence.find(diagnostic) != std::string::npos,
                    "TL-LANG-295 invalid static constructor form must fail with its exact semantic cause: " +
                        evidence);
            };

            expectRejected(
                "tl-lang-295-accessibility",
                "class Gate { public static Gate() { } }",
                ProjectStage::Syntax,
                "static constructors cannot declare accessibility");
            expectRejected(
                "tl-lang-295-parameters",
                "class Gate { static Gate(int value) { } }",
                ProjectStage::Syntax,
                "static constructors cannot declare parameters");
            expectRejected(
                "tl-lang-295-initializer",
                "class Gate { static Gate() : this() { } }",
                ProjectStage::Syntax,
                "static constructors cannot declare base(...) or this(...) initializers");
            expectRejected(
                "tl-lang-295-duplicate",
                "class Gate { static Gate() { } static Gate() { } }",
                ProjectStage::Members,
                "duplicate constructor signature");
            expectRejected(
                "tl-lang-295-this",
                "class Gate { int value; static Gate() { this.value = 1; } }",
                ProjectStage::MethodBodies,
                "this is not available in a static member or field initializer");
        }});
}

void registerModuleInitializationPlanCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-296", "CL-LANG-012", "Host.Language", "component", "PR",
        {"language", "static-initialization", "module-order"}, [](TestContext& context) {
            TemporaryModuleInitializationGraph project;
            nativesharp::frontend::ProjectCompilation compilation;
            std::string error;
            const bool compiled = compileTo(
                compilation, ProjectStage::Members, project.rootProject(), error);
            context.expect(compiled && error.empty(),
                "TL-LANG-296 a referenced module graph must build its initialization plan: " +
                    projectFailureEvidence(compilation, error));

            const auto* lookup = compilation.nameLookup();
            const auto* members = compilation.memberModel();
            if (!compiled || lookup == nullptr || members == nullptr) return;

            const auto plan = members->moduleInitializers();
            context.expect(plan.size() == 2U && plan[0].moduleName == "Producer" &&
                    plan[1].moduleName == "Consumer",
                "TL-LANG-296 module initialization order must remain dependency-first");
            if (plan.size() != 2U) return;

            context.expect(plan[0].projectOrdinal == 0U &&
                    plan[0].dependencyProjectOrdinals.empty() &&
                    plan[1].projectOrdinal == 1U &&
                    plan[1].dependencyProjectOrdinals == std::vector<std::size_t>{0U},
                "TL-LANG-296 direct module dependencies must be explicit and stable");
            context.expect(plan[0].staticConstructorMemberOrdinals.size() == 1U &&
                    plan[1].staticConstructorMemberOrdinals.size() == 1U,
                "TL-LANG-296 each module must own exactly its declared static constructors");

            for (const auto& module : plan)
            {
                for (const std::size_t memberOrdinal : module.staticConstructorMemberOrdinals)
                {
                    const auto& member = members->members()[memberOrdinal];
                    const auto& type = lookup->namedTypes()[member.declaringTypeOrdinal];
                    context.expect(member.kind == nativesharp::frontend::ClassMemberKind::Constructor &&
                            member.isStatic && type.projectOrdinal == module.projectOrdinal,
                        "TL-LANG-296 imports must schedule, not execute or re-own, dependency initializers");
                }
            }
        }});
}

void registerStaticInitializationOnceCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-297", "CL-LANG-012", "Host.Language", "component", "PR",
        {"language", "static-initialization", "once", "reentrancy"}, [](TestContext& context) {
            const std::string source = R"ns(
class Gate
{
    private static int Value = 1;
    static Gate() { Value = Value + 1; }
    public Gate() { }
    public static int Read() { return Value; }
}
)ns";
            TemporaryProject project("tl-lang-297-once", source);
            nativesharp::frontend::ProjectCompilation compilation;
            std::string error;
            const bool compiled = compileTo(
                compilation, ProjectStage::Mir, project.projectFile(), error);
            context.expect(compiled && error.empty(),
                "TL-LANG-297 static initialization must compile through MIR: " +
                    projectFailureEvidence(compilation, error));
            if (!compiled || compilation.nameLookup() == nullptr ||
                compilation.oopMirModel() == nullptr)
            {
                return;
            }
            const auto& functions = compilation.oopMirModel()->functions();
            const auto body = std::find_if(functions.begin(), functions.end(),
                [](const nativesharp::frontend::ProjectOopMirFunction& function) {
                    return function.constructor && function.isStatic;
                });
            context.expect(body != functions.end() &&
                    body->mangledName.starts_with("__nsc_cctor_body_"),
                "TL-LANG-297 MIR must keep the explicit static-constructor body distinct from its once guard");

            TemporaryModuleInitializationGraph cycle(true);
            nativesharp::frontend::ProjectCompilation cyclicCompilation;
            std::string cycleError;
            const bool acceptedCycle = compileTo(
                cyclicCompilation, ProjectStage::Members, cycle.rootProject(), cycleError);
            context.expect(!acceptedCycle && cycleError.find("project reference cycle:") != std::string::npos,
                "TL-LANG-297 module dependency cycles must fail with their explicit graph cycle");
        }});
}

void registerStaticInitializationFailureCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-298", "CL-LANG-012", "Host.Language", "unit", "PR",
        {"language", "static-initialization", "failure", "panic"}, [](TestContext& context) {
            context.expect(staticInitializationStateIsTerminal(
                    StaticInitializationState::Initialized) &&
                    staticInitializationStateIsTerminal(StaticInitializationState::Failed) &&
                    !staticInitializationStateIsTerminal(
                        StaticInitializationState::Uninitialized) &&
                    !staticInitializationStateIsTerminal(
                        StaticInitializationState::Initializing),
                "TL-LANG-298 only initialized and failed are terminal initialization states");
            context.expect(staticInitializationStateMayRetry(
                    StaticInitializationState::Uninitialized) &&
                    !staticInitializationStateMayRetry(
                        StaticInitializationState::Initializing) &&
                    !staticInitializationStateMayRetry(
                        StaticInitializationState::Initialized) &&
                    !staticInitializationStateMayRetry(StaticInitializationState::Failed),
                "TL-LANG-298 a failed initialization is terminal and never retries");

            const PanicContract abort = panicContract(PanicProfile::Abort);
            const PanicContract nativeUnwind = panicContract(PanicProfile::NativeUnwind);
            context.expect(!abort.runsCleanup && nativeUnwind.runsCleanup &&
                    !abort.mayCrossFfi && !nativeUnwind.mayCrossFfi,
                "TL-LANG-298 initialization failure retains the configured process panic profile");
        }});
}
}

void registerLanguageSurfaceModule(TestRegistry& registry)
{
    registerSurfaceCatalogCase(registry);
    registerSurfaceLookupCase(registry);
    registerSurfaceExternalEnumCase(registry);
    registerSurfaceManifestCase(registry);
    registerImplementedDiagnosticCase(registry);
    registerUnavailableDiagnosticCase(registry);
    registerSurfaceDeterminismCase(registry);
    registerStaticInitializationStatesCase(registry);
    registerStaticConstructorDeclarationCase(registry);
    registerModuleInitializationPlanCase(registry);
    registerStaticInitializationOnceCase(registry);
    registerStaticInitializationFailureCase(registry);
}
}
