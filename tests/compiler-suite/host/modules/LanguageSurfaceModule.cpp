#include "FrontendContractSupport.h"
#include "ExternalProgramTestSupport.h"
#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Interop/LibraryApiDiff.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Language/ErrorSemantics.h"
#include "nativesharp/Language/Language3Surface.h"
#include "nativesharp/LLVM/DebugMetadataVersion.h"

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
#include <thread>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#ifndef NSC_SUITE_NSC_EXECUTABLE
#error "NSC_SUITE_NSC_EXECUTABLE is required for T01 product integration"
#endif
#ifndef NSC_SUITE_NSLINK_EXECUTABLE
#error "NSC_SUITE_NSLINK_EXECUTABLE is required for T01 product integration"
#endif
#ifndef NSC_SUITE_WINDOWS_RUNTIME_LIBRARY
#error "NSC_SUITE_WINDOWS_RUNTIME_LIBRARY is required for T01 product integration"
#endif

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
    {Language3Feature::StaticConstructors, "static-constructors", Language3Disposition::Implemented,
        "thread-safe once initialization with reentrancy and terminal failure"},
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
    {Language3Feature::ConditionDeclarations, "condition-declarations", Language3Disposition::Implemented,
        "edge-scoped if, while and switch locals with definite assignment and cleanup"},
    {Language3Feature::StaticLocals, "static-locals", Language3Disposition::Implemented,
        "module storage with thread-safe once initialization and unload destruction"},
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

class TemporaryT01Products final
{
public:
    TemporaryT01Products()
    {
        static std::atomic<std::uint64_t> sequence{0};
        directory_ = std::filesystem::current_path() /
            ("nsc-language-t01-products-" +
             std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)));
        std::filesystem::create_directories(directory_);
    }

    TemporaryT01Products(const TemporaryT01Products&) = delete;
    TemporaryT01Products& operator=(const TemporaryT01Products&) = delete;

    ~TemporaryT01Products()
    {
        std::error_code ignored;
        std::filesystem::remove_all(directory_, ignored);
    }

    std::filesystem::path writeProject(
        std::string_view name,
        std::string_view outputType,
        std::string_view source,
        std::string_view additionalItems = {}) const
    {
        const std::filesystem::path sourceFile = directory_ / (std::string(name) + ".ns");
        const std::filesystem::path projectFile = directory_ / (std::string(name) + ".nsproj");
        write(sourceFile, source);
        write(projectFile,
            "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name><OutputType>" + std::string(outputType) +
            "</OutputType><Language>3</Language></PropertyGroup><ItemGroup><Compile Include=\"" +
            sourceFile.filename().string() + "\"/>" + std::string(additionalItems) +
            "</ItemGroup></Project>");
        return projectFile;
    }

    const std::filesystem::path& directory() const noexcept
    {
        return directory_;
    }

private:
    static void write(const std::filesystem::path& path, std::string_view contents)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    }

    std::filesystem::path directory_;
};

external_program_support::Result runT01Compiler(
    std::vector<std::wstring> arguments,
    const std::filesystem::path& directory)
{
    const std::vector<std::pair<std::wstring, std::wstring>> environment = {
        {L"NSC_LINKER", std::filesystem::path(NSC_SUITE_NSLINK_EXECUTABLE).wstring()},
        {L"NSC_WINDOWS_RUNTIME_LIBRARY",
            std::filesystem::path(NSC_SUITE_WINDOWS_RUNTIME_LIBRARY).wstring()}};
    return external_program_support::run(
        std::filesystem::path(NSC_SUITE_NSC_EXECUTABLE), arguments, {}, directory, environment);
}

external_program_support::Result emitT01Product(
    std::wstring_view action,
    const std::filesystem::path& project,
    const std::filesystem::path& product,
    const std::filesystem::path& directory)
{
    return runT01Compiler(
        {L"-O0", L"-g", std::wstring(action), project.wstring(), L"-o", product.wstring()},
        directory);
}

constexpr std::string_view kT01DllSource = R"ns(
public class Gate
{
    private static int Value = Initialize();

    static Gate()
    {
        Value = Value + Reenter();
    }

    private static int Initialize() { return 40; }
    private static int Reenter() { return 2; }

    public static export extern C int Read() { return Value; }

    public static export extern C int Next()
    {
        static int count = 40;
        count = count + 1;
        return count;
    }

    public static export extern C int Select(int choice)
    {
        if (bool ready = choice > 0)
        {
            return ready ? 7 : 0;
        }
        switch (int selected = choice)
        {
            case 0: return selected;
            default: return -1;
        }
    }
}
)ns";

constexpr std::string_view kT01ExeSource = R"ns(
public class ProgramGate
{
    private static int Value = 40;
    static ProgramGate() { Value = Value + Reenter(); }
    private static int Reenter() { return 2; }
    public static int Read()
    {
        static int cached = Value;
        if (bool ready = cached == 42) { return ready ? cached : 1; }
        return 2;
    }

    public static int Main() { return Read(); }
}
)ns";

void collectMirKind(
    const nativesharp::frontend::ProjectOopMirNode* node,
    nativesharp::frontend::ProjectOopMirNodeKind kind,
    std::vector<const nativesharp::frontend::ProjectOopMirNode*>& result)
{
    if (node == nullptr) return;
    if (node->kind == kind) result.push_back(node);
    for (const auto& child : node->children) collectMirKind(child.get(), kind, result);
}

bool mirBlockContainsOrdered(
    const nativesharp::frontend::ProjectOopMirNode* node,
    std::string_view localName,
    nativesharp::frontend::ProjectOopMirNodeKind followingKind)
{
    if (node == nullptr)
    {
        return false;
    }
    if (node->kind == nativesharp::frontend::ProjectOopMirNodeKind::Block)
    {
        bool foundLocal = false;
        for (const auto& child : node->children)
        {
            if (child->kind == nativesharp::frontend::ProjectOopMirNodeKind::LocalDeclaration &&
                child->stringValue == localName)
            {
                foundLocal = true;
            }
            else if (foundLocal && child->kind == followingKind)
            {
                return true;
            }
        }
    }
    for (const auto& child : node->children)
    {
        if (mirBlockContainsOrdered(child.get(), localName, followingKind))
        {
            return true;
        }
    }
    return false;
}

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
        "static-constructors=implemented:thread-safe once initialization with reentrancy and terminal failure\n"
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
        "condition-declarations=implemented:edge-scoped if, while and switch locals with definite assignment "
        "and cleanup\n"
        "static-locals=implemented:module storage with thread-safe once initialization and unload destruction\n";
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
        context.expect(planned == 6 && deferred == 0 && rejected == 1,
            "TL-LANG-160 the catalog must expose six nonproductive 3.0.4 plans and one rejected feature");
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

void registerStaticLocalLifetimeCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-299", "CL-LANG-012", "Host.Language", "component", "PR",
        {"language", "static-initialization", "static-local", "module-lifetime"},
        [](TestContext& context) {
            const std::string source = R"ns(
class Gate
{
    public static int Next()
    {
        static int counter = 1;
        static raii string lifetime = null;
        counter = counter + 1;
        return counter;
    }
}
)ns";
            TemporaryProject project("tl-lang-299-static-local", source);
            nativesharp::frontend::ProjectCompilation compilation;
            std::string error;
            const bool compiled = compileTo(
                compilation, ProjectStage::Mir, project.projectFile(), error);
            context.expect(compiled && error.empty(),
                "TL-LANG-299 static locals must compile through MIR: " +
                    projectFailureEvidence(compilation, error));
            if (!compiled || compilation.oopMirModel() == nullptr) return;

            std::vector<const nativesharp::frontend::ProjectOopMirNode*> declarations;
            for (const auto& function : compilation.oopMirModel()->functions())
                collectMirKind(function.body.get(),
                    nativesharp::frontend::ProjectOopMirNodeKind::LocalDeclaration,
                    declarations);
            const auto staticCount = std::ranges::count_if(
                declarations, [](const auto* declaration) { return declaration->staticLocal; });
            const auto staticOwner = std::ranges::find_if(
                declarations, [](const auto* declaration) {
                    return declaration->staticLocal && declaration->automaticDestruction;
                });
            context.expect(staticCount == 2U && staticOwner != declarations.end() &&
                    !(*staticOwner)->cleanupRegionOrdinal.has_value(),
                "TL-LANG-299 MIR must retain two module-static declarations and keep owner "
                "destruction out of lexical cleanup regions");

            const auto expectRejected = [&](std::string_view label, std::string invalidSource,
                                            std::string_view diagnostic) {
                TemporaryProject invalidProject(label, std::move(invalidSource));
                nativesharp::frontend::ProjectCompilation invalidCompilation;
                std::string invalidError;
                const bool accepted = compileTo(
                    invalidCompilation, ProjectStage::MethodBodies,
                    invalidProject.projectFile(), invalidError);
                const std::string evidence = projectFailureEvidence(
                    invalidCompilation, invalidError);
                context.expect(!accepted && evidence.find(diagnostic) != std::string::npos,
                    "TL-LANG-299 invalid static-local storage must fail causally: " + evidence);
            };
            expectRejected(
                "tl-lang-299-reference",
                "class Gate { static int Value; static void Run() { static ref int alias = ref Value; } }",
                "static local cannot be a reference local");
            expectRejected(
                "tl-lang-299-vla",
                "class Gate { static void Run(int count) { static int[count] values; } }",
                "static local cannot be a variable-length array");
        }});
}

void registerConditionDeclarationCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-300", "CL-LANG-012", "Host.Language", "component", "PR",
        {"language", "condition-declaration", "scope", "cleanup"},
        [](TestContext& context) {
            const std::string source = R"ns(
class Gate
{
    public static int Run(bool first, int choice)
    {
        int result = 0;
        if (bool ready = first) { result = 1; } else { result = 2; }
        while (bool again = result < 4)
        {
            result = result + 1;
            if (result == 3) { continue; }
            if (result > 3) { break; }
        }
        switch (int selected = choice)
        {
            case 0: result = result + selected; break;
            default: result = result + 1; break;
        }
        return result;
    }
}
)ns";
            TemporaryProject project("tl-lang-300-condition-declaration", source);
            nativesharp::frontend::ProjectCompilation compilation;
            std::string error;
            const bool compiled = compileTo(
                compilation, ProjectStage::Mir, project.projectFile(), error);
            context.expect(compiled && error.empty(),
                "TL-LANG-300 condition declarations must compile through MIR: " +
                    projectFailureEvidence(compilation, error));
            if (!compiled || compilation.oopMirModel() == nullptr) return;

            bool ifScope = false;
            bool whileScope = false;
            bool switchScope = false;
            bool whilePerIteration = false;
            for (const auto& function : compilation.oopMirModel()->functions())
            {
                ifScope = ifScope || mirBlockContainsOrdered(
                    function.body.get(), "ready",
                    nativesharp::frontend::ProjectOopMirNodeKind::If);
                switchScope = switchScope || mirBlockContainsOrdered(
                    function.body.get(), "selected",
                    nativesharp::frontend::ProjectOopMirNodeKind::Switch);
                std::vector<const nativesharp::frontend::ProjectOopMirNode*> loops;
                collectMirKind(function.body.get(),
                    nativesharp::frontend::ProjectOopMirNodeKind::While, loops);
                for (const auto* loop : loops)
                {
                    if (loop->children.size() < 2U ||
                        loop->children[0]->kind !=
                            nativesharp::frontend::ProjectOopMirNodeKind::BooleanConstant ||
                        !loop->children[0]->booleanValue)
                    {
                        continue;
                    }
                    const auto* iteration = loop->children[1].get();
                    const bool hasPerIterationLocal = mirBlockContainsOrdered(
                        iteration, "again", nativesharp::frontend::ProjectOopMirNodeKind::If);
                    whileScope = whileScope || hasPerIterationLocal;
                    whilePerIteration = whilePerIteration || hasPerIterationLocal;
                }
            }
            context.expect(ifScope && whileScope && switchScope && whilePerIteration,
                "TL-LANG-300 MIR must place condition locals before their if/synthetic-stop/switch "
                "inside the exact lexical lifetime block");

            const auto expectRejected = [&](std::string_view label, std::string invalidSource,
                                            std::string_view diagnostic) {
                TemporaryProject invalidProject(label, std::move(invalidSource));
                nativesharp::frontend::ProjectCompilation invalidCompilation;
                std::string invalidError;
                const bool accepted = compileTo(
                    invalidCompilation, ProjectStage::MethodBodies,
                    invalidProject.projectFile(), invalidError);
                const std::string evidence = projectFailureEvidence(
                    invalidCompilation, invalidError);
                context.expect(!accepted && evidence.find(diagnostic) != std::string::npos,
                    "TL-LANG-300 invalid condition declaration must fail causally: " + evidence);
            };
            expectRejected(
                "tl-lang-300-non-bool",
                "class Gate { static void Run() { if (int value = 1) { } } }",
                "if condition must have type bool");
            expectRejected(
                "tl-lang-300-multiple",
                "class Gate { static void Run() { if (bool first = true, second = false) { } } }",
                "condition declaration must declare exactly one local");
            expectRejected(
                "tl-lang-300-scope",
                "class Gate { static void Run() { switch (int value = 1) { default: break; } value = 2; } }",
                "unknown identifier 'value'");
        }});
}

void verifyT01MetadataAndDebug(
    TestContext& context,
    const std::filesystem::path& project,
    const std::filesystem::path& directory)
{
    nativesharp::frontend::ProjectCompilation compilation;
    std::string error;
    const bool compiled = compilation.compileProjectOopMir(project, error);
    context.expect(compiled && error.empty(),
        "TL-LANG-301 T01 metadata/debug fixture must compile through MIR: " +
            projectFailureEvidence(compilation, error));
    if (!compiled || compilation.nameLookup() == nullptr ||
        compilation.memberModel() == nullptr || compilation.oopMirModel() == nullptr)
    {
        return;
    }

    nativesharp::interop::LibraryMetadataCodec codec;
    nativesharp::interop::LibraryMetadata written;
    const std::filesystem::path metadataFile = directory / "T01.nsmeta";
    error.clear();
    const bool emitted = codec.write(compilation, metadataFile, written, error);
    context.expect(emitted && error.empty() &&
            written.formatMajor == 1 && written.formatMinor == 39 &&
            written.languageFeatureFlags == nativesharp::interop::kMetadataLanguageFeatureMask,
        "TL-LANG-301 NSMETA 1.39 must publish all three closed T01 capabilities: " + error);

    nativesharp::interop::LibraryMetadata decoded;
    error.clear();
    const bool read = codec.read(metadataFile, decoded, error);
    context.expect(read && error.empty() &&
            decoded.languageFeatureFlags == nativesharp::interop::kMetadataLanguageFeatureMask,
        "TL-LANG-301 the T01 capability catalogue must survive NSMETA round-trip: " + error);

    nativesharp::interop::LibraryMetadata legacy = written;
    legacy.formatMinor = 38;
    const std::filesystem::path legacyFile = directory / "T01-legacy.nsmeta";
    error.clear();
    const bool legacyWritten = codec.writeModel(legacy, legacyFile, error);
    nativesharp::interop::LibraryMetadata legacyDecoded;
    const bool legacyRead = legacyWritten && codec.read(legacyFile, legacyDecoded, error);
    context.expect(legacyRead && error.empty() && legacyDecoded.languageFeatureFlags == 0,
        "TL-LANG-301 NSMETA 1.38 must remain readable without inventing 1.39 capabilities: " +
            error);

    const external_program_support::Result debugIr = runT01Compiler(
        {L"-O0", L"--dump-project-oop-llvm", project.wstring()}, directory);
    context.expect(debugIr.started && debugIr.exitCode == 0 && debugIr.error.empty(),
        "TL-LANG-301 T01 fixture must lower with full debug information: " +
            debugIr.error + debugIr.output);
    if (!debugIr.started || debugIr.exitCode != 0)
    {
        return;
    }
    context.expect(debugIr.output.find("__nsc_cctor_body_") != std::string::npos,
        "TL-LANG-301 productive LLVM must retain the static-constructor identity");
}

void verifyT01Executable(
    TestContext& context,
    const TemporaryT01Products& products)
{
    const std::filesystem::path project = products.writeProject(
        "T01Executable", "Exe", kT01ExeSource);
    const std::filesystem::path executable = products.directory() / "T01Executable.exe";
    const external_program_support::Result emitted = emitT01Product(
        L"--emit-project-oop-exe", project, executable, products.directory());
    context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
        "TL-LANG-301 T01 EXE must link through the productive compiler: " +
            emitted.error + emitted.output);
    if (!emitted.started || emitted.exitCode != 0)
    {
        return;
    }
    std::filesystem::path programDatabase = executable;
    programDatabase.replace_extension(".pdb");
    context.expect(std::filesystem::exists(programDatabase) &&
            std::filesystem::file_size(programDatabase) != 0 &&
            nativesharp::llvm_backend::kDebugMetadataMajor == 1 &&
            nativesharp::llvm_backend::kDebugMetadataMinor == 1,
        "TL-LANG-301 T01 EXE must emit a non-empty NSDEBUG 1.1 CodeView/PDB artifact");
    const auto result = external_program_support::run(
        executable, {}, {}, products.directory());
    context.expect(result.started && result.error.empty() && result.exitCode == 42,
        "TL-LANG-301 T01 EXE must execute cctor recursion, static local and condition declaration "
        "with exit 42: " + result.error + result.output);
}

void verifyT01DynamicLibrary(
    TestContext& context,
    const TemporaryT01Products& products,
    const std::filesystem::path& project)
{
    const std::filesystem::path library = products.directory() / "T01Dynamic.dll";
    const external_program_support::Result emitted = emitT01Product(
        L"--emit-project-oop-dll", project, library, products.directory());
    context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
        "TL-LANG-301 T01 DLL must link through the productive compiler: " +
            emitted.error + emitted.output);
    if (!emitted.started || emitted.exitCode != 0)
    {
        return;
    }
    std::filesystem::path programDatabase = library;
    programDatabase.replace_extension(".pdb");
    context.expect(std::filesystem::exists(programDatabase) &&
            std::filesystem::file_size(programDatabase) != 0 &&
            nativesharp::llvm_backend::kDebugMetadataMajor == 1 &&
            nativesharp::llvm_backend::kDebugMetadataMinor == 1,
        "TL-LANG-301 T01 DLL must emit a non-empty NSDEBUG 1.1 CodeView/PDB artifact");

    HMODULE module = LoadLibraryW(library.c_str());
    using NullaryInt = int(__cdecl*)();
    using UnaryInt = int(__cdecl*)(int);
    NullaryInt read = module != nullptr
        ? reinterpret_cast<NullaryInt>(GetProcAddress(module, "Read"))
        : nullptr;
    NullaryInt next = module != nullptr
        ? reinterpret_cast<NullaryInt>(GetProcAddress(module, "Next"))
        : nullptr;
    UnaryInt select = module != nullptr
        ? reinterpret_cast<UnaryInt>(GetProcAddress(module, "Select"))
        : nullptr;
    context.expect(module != nullptr && read != nullptr && next != nullptr && select != nullptr,
        "TL-LANG-301 T01 DLL must expose its three exact native probes");
    if (read == nullptr || next == nullptr || select == nullptr)
    {
        if (module != nullptr)
        {
            FreeLibrary(module);
        }
        return;
    }

    std::atomic<bool> start{false};
    std::array<int, 16> observed{};
    std::vector<std::thread> workers;
    workers.reserve(observed.size());
    for (std::size_t index = 0; index < observed.size(); ++index)
    {
        workers.emplace_back([&, index] {
            while (!start.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
            observed[index] = read();
        });
    }
    start.store(true, std::memory_order_release);
    for (std::thread& worker : workers)
    {
        worker.join();
    }
    context.expect(std::ranges::all_of(observed, [](int value) { return value == 42; }),
        "TL-LANG-301 concurrent DLL first touch must initialize once and preserve recursive reentry");
    context.expect(next() == 41 && next() == 42,
        "TL-LANG-301 DLL static local must persist across calls");
    context.expect(select(1) == 7 && select(0) == 0,
        "TL-LANG-301 DLL condition declarations must execute with scoped values");
    context.expect(FreeLibrary(module) != FALSE,
        "TL-LANG-301 T01 DLL must unload through the module-lifetime boundary");
}

void registerT01IntegrationCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-301", "CL-LANG-012", "Host.Language", "integration", "PR",
        {"language", "static-initialization", "metadata", "debug", "exe", "dll"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path libraryProject = products.writeProject(
                "T01Dynamic", "SharedLibrary", kT01DllSource);
            verifyT01MetadataAndDebug(context, libraryProject, products.directory());
            verifyT01Executable(context, products);
            verifyT01DynamicLibrary(context, products, libraryProject);
        }});
}

void registerDefaultInterfaceBodyCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-302", "CL-LANG-012", "Host.Language", "integration", "PR",
        {"language", "trait", "interface", "default-body", "property", "indexer"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T02DefaultBodies", "Exe", R"ns(
trait ITraitDefaults
{
    int Method() { return 5; }
    int Value { get { return 7; } }
    int this[int index] { get { return index + 8; } }
}
class TraitItem : ITraitDefaults { }

interface IInterfaceDefaults
{
    int Method() { return 5; }
    int Value { get { return 7; } }
    int this[int index] { get { return index + 8; } }
}
class InterfaceItem : IInterfaceDefaults { }

class Program
{
    public static int Main()
    {
        TraitItem traitItem = new TraitItem();
        dyn ITraitDefaults traitView = traitItem;
        InterfaceItem interfaceItem = new InterfaceItem();
        IInterfaceDefaults interfaceView = interfaceItem;
        return traitView.Method() + traitView.Value + traitView[1]
            + interfaceView.Method() + interfaceView.Value + interfaceView[1];
    }
}
)ns");
            const std::filesystem::path executable = products.directory() / "T02DefaultBodies.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-302 method, property and indexer defaults must lower for traits and "
                "interfaces: " + emitted.error + emitted.output);
            if (emitted.started && emitted.exitCode == 0)
            {
                const auto result = external_program_support::run(
                    executable, {}, {}, products.directory());
                context.expect(result.started && result.error.empty() && result.exitCode == 42,
                    "TL-LANG-302 trait and interface defaults must dispatch with identical "
                    "observable results: " + result.error + result.output);
            }

            TemporaryProject invalid(
                "tl-lang-302-abstract-body",
                "interface IBad { abstract int Read() { return 1; } }");
            nativesharp::frontend::ProjectCompilation invalidCompilation;
            std::string invalidError;
            const bool invalidAccepted = compileTo(
                invalidCompilation, ProjectStage::Syntax, invalid.projectFile(), invalidError);
            const std::string evidence = projectFailureEvidence(invalidCompilation, invalidError);
            context.expect(!invalidAccepted &&
                    evidence.find("abstract methods declare no body") != std::string::npos,
                "TL-LANG-302 an explicitly abstract interface member must still reject a body: " +
                    evidence);
        }});
}

void registerDefaultImplementationPrecedenceCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-303", "CL-LANG-012", "Host.Language", "integration", "PR",
        {"language", "trait", "interface", "default-body", "precedence"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T02DefaultPrecedence", "Exe", R"ns(
trait IExplicit { int Value() { return 1; } }
class ExplicitItem { }
impl IExplicit for ExplicitItem { int Value() { return 42; } }

trait INominal { int Value() { return 1; } }
class NominalItem : INominal { public int Value() { return 42; } }

interface IBase { int Value() { return 1; } }
interface IDerived : IBase { int Value() { return 42; } }
interface ICombined : IBase, IDerived { }
class DefaultItem : ICombined { }

class Program
{
    public static int Main()
    {
        ExplicitItem explicitItem = new ExplicitItem();
        dyn IExplicit explicitView = explicitItem;
        NominalItem nominalItem = new NominalItem();
        dyn INominal nominalView = nominalItem;
        DefaultItem defaultItem = new DefaultItem();
        ICombined defaultView = defaultItem;
        if (explicitView.Value() != 42) return 11;
        if (nominalView.Value() != 42) return 12;
        if (defaultView.Value() != 42) return 13;
        return 42;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T02DefaultPrecedence.exe";
            const external_program_support::Result interfacePlan = runT01Compiler(
                {L"--dump-project-interfaces", project.wstring()}, products.directory());
            context.expect(interfacePlan.started && interfacePlan.exitCode == 0 &&
                    interfacePlan.error.empty(),
                "TL-LANG-303 precedence fixture must expose its interface-table plan: " +
                    interfacePlan.error + interfacePlan.output);
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-303 explicit, nominal and most-specific default candidates must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;

            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-303 precedence must be explicit implementation, nominal member, then "
                "most-specific default (observed exit " + std::to_string(result.exitCode) + "): " +
                    result.error + result.output + interfacePlan.output);
        }});
}

void registerDefaultDiamondAndCoherenceCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-304", "CL-LANG-012", "Host.Language", "integration", "PR",
        {"language", "trait", "interface", "default-body", "diamond", "coherence"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path conflictProject = products.writeProject(
                "T02DiamondConflict", "Exe", R"ns(
interface ILeft { int Value() { return 1; } }
interface IRight { int Value() { return 2; } }
interface IConflict : ILeft, IRight { }
class Broken : IConflict { }
class Program { public static int Main() { return 0; } }
)ns");
            const external_program_support::Result conflict = runT01Compiler(
                {L"--dump-project-interfaces", conflictProject.wstring()}, products.directory());
            context.expect(conflict.started && conflict.exitCode != 0 &&
                    conflict.output.find("competing default implementations") != std::string::npos,
                "TL-LANG-304 an unresolved ordinary-interface diamond must fail causally: " +
                    conflict.error + conflict.output);

            const std::filesystem::path resolvedProject = products.writeProject(
                "T02DiamondResolved", "Exe", R"ns(
interface ILeft { int Value() { return 1; } }
interface IRight { int Value() { return 2; } }
interface IResolved : ILeft, IRight { int Value() { return 42; } }
class Item : IResolved { }
class Program
{
    public static int Main()
    {
        Item item = new Item();
        IResolved value = item;
        return value.Value();
    }
}
)ns");
            const std::filesystem::path resolvedExecutable =
                products.directory() / "T02DiamondResolved.exe";
            const external_program_support::Result resolved = emitT01Product(
                L"--emit-project-oop-exe", resolvedProject, resolvedExecutable,
                products.directory());
            context.expect(resolved.started && resolved.exitCode == 0 && resolved.error.empty(),
                "TL-LANG-304 a derived redeclaration must resolve the default diamond: " +
                    resolved.error + resolved.output);
            if (resolved.started && resolved.exitCode == 0)
            {
                const auto result = external_program_support::run(
                    resolvedExecutable, {}, {}, products.directory());
                context.expect(result.started && result.error.empty() && result.exitCode == 42,
                    "TL-LANG-304 the resolving override must dispatch to 42: " +
                        result.error + result.output);
            }

            const std::filesystem::path duplicateProject = products.writeProject(
                "T02DuplicateImpl", "Exe", R"ns(
trait IScore { int Score(); }
class Number : IScore { public int Score() { return 42; } }
impl IScore for Number { }
class Program { public static int Main() { return 0; } }
)ns");
            const external_program_support::Result duplicate = runT01Compiler(
                {L"--dump-project-interfaces", duplicateProject.wstring()}, products.directory());
            context.expect(duplicate.started && duplicate.exitCode != 0 &&
                    duplicate.output.find("duplicate trait impl") != std::string::npos,
                "TL-LANG-304 base-list plus impl must remain a coherence violation: " +
                    duplicate.error + duplicate.output);

            const std::filesystem::path producerProject = products.writeProject(
                "T02CoherenceApi", "Library", R"ns(
namespace CoherenceApi
{
    public trait IScore { int Score(); }
    public class Foreign { }
}
)ns");
            const std::filesystem::path producerLibrary =
                products.directory() / "T02CoherenceApi.lib";
            const external_program_support::Result producer = emitT01Product(
                L"--emit-project-oop-lib", producerProject, producerLibrary,
                products.directory());
            context.expect(producer.started && producer.exitCode == 0 && producer.error.empty(),
                "TL-LANG-304 the imported-trait producer must build: " +
                    producer.error + producer.output);
            if (!producer.started || producer.exitCode != 0) return;

            constexpr std::string_view libraryReference =
                "<LibraryReference Include=\"T02CoherenceApi.lib\"/>";
            const std::filesystem::path localProject = products.writeProject(
                "T02LocalImpl", "Exe", R"ns(
using CoherenceApi;
class Local { }
impl IScore for Local { int Score() { return 42; } }
class Program
{
    public static int Main()
    {
        Local local = new Local();
        dyn IScore score = local;
        return score.Score();
    }
}
)ns", libraryReference);
            const std::filesystem::path localExecutable =
                products.directory() / "T02LocalImpl.exe";
            const external_program_support::Result local = emitT01Product(
                L"--emit-project-oop-exe", localProject, localExecutable, products.directory());
            context.expect(local.started && local.exitCode == 0 && local.error.empty(),
                "TL-LANG-304 an imported trait may be implemented for a local class: " +
                    local.error + local.output);
            if (local.started && local.exitCode == 0)
            {
                const auto result = external_program_support::run(
                    localExecutable, {}, {}, products.directory());
                context.expect(result.started && result.error.empty() && result.exitCode == 42,
                    "TL-LANG-304 the coherent local impl must execute with exit 42: " +
                        result.error + result.output);
            }

            const std::filesystem::path orphanProject = products.writeProject(
                "T02OrphanImpl", "Exe", R"ns(
using CoherenceApi;
trait ILocal { int Score(); }
impl ILocal for Foreign { int Score() { return 42; } }
class Program { public static int Main() { return 0; } }
)ns", libraryReference);
            const external_program_support::Result orphan = runT01Compiler(
                {L"--dump-project-interfaces", orphanProject.wstring()}, products.directory());
            context.expect(orphan.started && orphan.exitCode != 0 &&
                    orphan.output.find("sealed trait coherence rejects this impl") !=
                        std::string::npos,
                "TL-LANG-304 a consumer must not implement a trait for a foreign class: " +
                    orphan.error + orphan.output);
        }});
}

void registerGenericDefaultIntegrationCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-305", "CL-LANG-012", "Host.Language", "integration", "PR",
        {"language", "trait", "default-body", "generic", "associated-item", "some", "dyn"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T02GenericDefaults", "Exe", R"ns(
trait IValue<TValue>
{
    type Item = TValue;
    const int Expected = 42;
    int Score() { return 42; }
}
class Number { }
impl IValue<int> for Number { }
class Factory
{
    public static some IValue<int> Make() { return new Number(); }
}
class Program
{
    private static T.Item Echo<T>(T.Item value) where T : IValue<int> { return value; }

    public static int Main()
    {
        Number number = new Number();
        dyn IValue<int> erased = number;
        var opaque = Factory.Make();
        if (Echo<Number>(42) != 42) return 11;
        if (erased.Score() != 42) return 12;
        return opaque.Score();
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T02GenericDefaults.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-305 generic defaults, associated items, constraints, some and dyn "
                "must compile together: " + emitted.error + emitted.output);
            if (emitted.started && emitted.exitCode == 0)
            {
                const auto result = external_program_support::run(
                    executable, {}, {}, products.directory());
                context.expect(result.started && result.error.empty() && result.exitCode == 42,
                    "TL-LANG-305 static projection, dyn and some must observe the same default: " +
                        result.error + result.output);
            }

            const std::filesystem::path genericMethod = products.writeProject(
                "T02GenericMethodDyn", "Exe", R"ns(
trait IMap { int Map<T>(T value) { return 42; } }
class Program
{
    public static int Main()
    {
        dyn IMap value = null;
        return value == null ? 0 : 1;
    }
}
)ns");
            const external_program_support::Result genericRejected = runT01Compiler(
                {L"--check-project-interfaces", genericMethod.wstring()}, products.directory());
            context.expect(genericRejected.started && genericRejected.exitCode != 0 &&
                    genericRejected.output.find("not object-safe") != std::string::npos,
                "TL-LANG-305 dyn must reject a generic default method: " +
                    genericRejected.error + genericRejected.output);

            const std::filesystem::path unboundAssociated = products.writeProject(
                "T02UnboundAssociatedDyn", "Exe", R"ns(
trait IItems { type Item; int Count() { return 42; } }
class Program
{
    public static int Main()
    {
        dyn IItems value = null;
        return value == null ? 0 : 1;
    }
}
)ns");
            const external_program_support::Result associatedRejected = runT01Compiler(
                {L"--check-project-interfaces", unboundAssociated.wstring()},
                products.directory());
            context.expect(associatedRejected.started && associatedRejected.exitCode != 0 &&
                    associatedRejected.output.find("fully defaulted associated items") !=
                        std::string::npos,
                "TL-LANG-305 dyn must reject an unbound associated identity: " +
                    associatedRejected.error + associatedRejected.output);
        }});
}

void registerMetadataDefaultWitnessCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-306", "CL-LANG-012", "Host.Language", "integration", "PR",
        {"language", "trait", "default-body", "nsmeta", "library-reference", "witness"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path producerProject = products.writeProject(
                "T02MetadataDefaults", "Library", R"ns(
namespace DefaultsApi
{
    public trait IDefaults
    {
        int Method() { return 13; }
        int Value { get { return 14; } }
        int this[int index] { get { return index + 14; } }
    }
}
)ns");
            const std::filesystem::path producerLibrary =
                products.directory() / "T02MetadataDefaults.lib";
            const external_program_support::Result producer = emitT01Product(
                L"--emit-project-oop-lib", producerProject, producerLibrary,
                products.directory());
            context.expect(producer.started && producer.exitCode == 0 && producer.error.empty(),
                "TL-LANG-306 the default-body producer must build: " +
                    producer.error + producer.output);
            if (!producer.started || producer.exitCode != 0) return;

            nativesharp::interop::LibraryMetadata metadata;
            std::string metadataError;
            const bool metadataRead = nativesharp::interop::LibraryMetadataCodec{}.read(
                products.directory() / "T02MetadataDefaults.nsmeta", metadata, metadataError);
            const std::string traitKey = "type:T02MetadataDefaults::DefaultsApi.IDefaults";
            const std::array<std::string_view, 3> witnessNames = {
                "Method", "get_Value", "get_Item"};
            const bool physicalWitnesses = std::ranges::all_of(
                witnessNames, [&](std::string_view name) {
                    return std::ranges::any_of(metadata.members, [&](const auto& member) {
                        return member.declaringTypeKey == traitKey && member.name == name &&
                            (member.flags & (1U << 5U)) == 0U && !member.mangledName.empty();
                    });
                });
            context.expect(metadataRead &&
                    metadata.formatMinor == nativesharp::interop::kMetadataFormatMinor &&
                    physicalWitnesses,
                "TL-LANG-306 current NSMETA must version all three non-abstract physical "
                "witnesses: " + metadataError);

            std::error_code removalError;
            std::filesystem::remove(producerProject, removalError);
            context.expect(!removalError,
                "TL-LANG-306 the producer project must be removable before consumption: " +
                    removalError.message());
            removalError.clear();
            std::filesystem::remove(
                products.directory() / "T02MetadataDefaults.ns", removalError);
            context.expect(!removalError,
                "TL-LANG-306 the producer source must be removable before consumption: " +
                    removalError.message());

            constexpr std::string_view libraryReference =
                "<LibraryReference Include=\"T02MetadataDefaults.lib\"/>";
            const std::filesystem::path consumerProject = products.writeProject(
                "T02MetadataConsumer", "Exe", R"ns(
using DefaultsApi;
class Local { }
impl IDefaults for Local { }
class Program
{
    public static int Main()
    {
        Local local = new Local();
        dyn IDefaults value = local;
        return value.Method() + value.Value + value[1];
    }
}
)ns", libraryReference);
            const std::filesystem::path consumerExecutable =
                products.directory() / "T02MetadataConsumer.exe";
            const external_program_support::Result consumer = emitT01Product(
                L"--emit-project-oop-exe", consumerProject, consumerExecutable,
                products.directory());
            context.expect(consumer.started && consumer.exitCode == 0 && consumer.error.empty(),
                "TL-LANG-306 a source-free consumer must reconstruct every default witness: " +
                    consumer.error + consumer.output);
            if (!consumer.started || consumer.exitCode != 0) return;

            const auto result = external_program_support::run(
                consumerExecutable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-306 imported method/property/indexer defaults must execute through "
                "producer-owned witnesses: " + result.error + result.output);
        }});
}

void registerDefaultDispatchAbiCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-307", "CL-LANG-012", "Host.Language", "integration", "PR",
        {"language", "trait", "default-body", "dll", "abi", "mir", "llvm", "debug"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path producerProject = products.writeProject(
                "T02DefaultAbi", "SharedLibrary", R"ns(
namespace DefaultAbi
{
    public trait IDefaults
    {
        int First() { return 20; }
        int Second() { return 1; }
    }
}
)ns");
            const std::filesystem::path producerRuntime =
                products.directory() / "T02DefaultAbi.dll";
            const external_program_support::Result producer = emitT01Product(
                L"--emit-project-oop-dll", producerProject, producerRuntime,
                products.directory());
            context.expect(producer.started && producer.exitCode == 0 && producer.error.empty(),
                "TL-LANG-307 the debug DLL producer must build: " +
                    producer.error + producer.output);
            if (!producer.started || producer.exitCode != 0) return;

            const std::filesystem::path metadataPath =
                products.directory() / "T02DefaultAbi.nsmeta";
            nativesharp::interop::LibraryMetadata metadata;
            std::string metadataError;
            const bool metadataRead = nativesharp::interop::LibraryMetadataCodec{}.read(
                metadataPath, metadata, metadataError);
            const auto trait = std::ranges::find_if(metadata.types, [](const auto& type) {
                return type.fullyQualifiedName == "DefaultAbi.IDefaults";
            });
            std::vector<std::string> witnesses;
            for (const auto& member : metadata.members)
            {
                if (member.declaringTypeKey == "type:T02DefaultAbi::DefaultAbi.IDefaults" &&
                    (member.name == "First" || member.name == "Second") &&
                    !member.mangledName.empty())
                    witnesses.push_back(member.mangledName);
            }
            context.expect(metadataRead && trait != metadata.types.end() &&
                    trait->traitMethodSignatures.size() == 2U && witnesses.size() == 2U,
                "TL-LANG-307 NSMETA must publish two ordered default slots and callees: " +
                    metadataError);

            bool reorderedBreaksContract = false;
            if (metadataRead && trait != metadata.types.end() &&
                trait->traitMethodSignatures.size() == 2U)
            {
                nativesharp::interop::LibraryMetadata reordered = metadata;
                const std::size_t traitIndex = static_cast<std::size_t>(
                    std::distance(metadata.types.begin(), trait));
                std::ranges::reverse(reordered.types[traitIndex].traitMethodSignatures);
                const auto diff = nativesharp::interop::LibraryApiDiffer::compare(
                    metadata, reordered);
                reorderedBreaksContract = !diff.sourceCompatible || !diff.abiCompatible;
            }
            context.expect(reorderedBreaksContract,
                "TL-LANG-307 reordering default dispatch slots must be an observable evolution "
                "break");

            constexpr std::string_view libraryReference =
                "<LibraryReference Include=\"T02DefaultAbi.lib\" "
                "Runtime=\"T02DefaultAbi.dll\"/>";
            const std::filesystem::path consumerProject = products.writeProject(
                "T02DefaultAbiConsumer", "Exe", R"ns(
using DefaultAbi;
class Local { }
impl IDefaults for Local { }
class Program
{
    private static int StaticRead<T>(T value) where T : IDefaults
    {
        return value.First() + value.Second();
    }

    public static int Main()
    {
        Local local = new Local();
        dyn IDefaults dynamicValue = local;
        return StaticRead<Local>(local) + dynamicValue.First() + dynamicValue.Second();
    }
}
)ns", libraryReference);

            const external_program_support::Result mir = runT01Compiler(
                {L"--dump-project-oop-mir", consumerProject.wstring(), L"-O0"},
                products.directory());
            bool mirCarriesWitnesses = mir.started && mir.exitCode == 0 &&
                mir.output.find("interface-binding") != std::string::npos;
            for (const std::string& witness : witnesses)
                mirCarriesWitnesses = mirCarriesWitnesses &&
                    mir.output.find(witness) != std::string::npos;
            context.expect(mirCarriesWitnesses,
                "TL-LANG-307 MIR must preserve static callees and the dynamic interface table: " +
                    mir.error + mir.output);

            const external_program_support::Result llvm = runT01Compiler(
                {L"--dump-project-oop-llvm", consumerProject.wstring(), L"-O0"},
                products.directory());
            bool llvmCarriesWitnesses = llvm.started && llvm.exitCode == 0;
            for (const std::string& witness : witnesses)
                llvmCarriesWitnesses = llvmCarriesWitnesses &&
                    llvm.output.find(witness) != std::string::npos;
            context.expect(llvmCarriesWitnesses,
                "TL-LANG-307 LLVM must retain imported default callees and debug locations: " +
                    llvm.error + llvm.output);

            const std::filesystem::path consumerExecutable =
                products.directory() / "T02DefaultAbiConsumer.exe";
            const external_program_support::Result consumer = emitT01Product(
                L"--emit-project-oop-exe", consumerProject, consumerExecutable,
                products.directory());
            context.expect(consumer.started && consumer.exitCode == 0 && consumer.error.empty(),
                "TL-LANG-307 the separated static/dynamic consumer must build: " +
                    consumer.error + consumer.output);
            if (!consumer.started || consumer.exitCode != 0) return;
            context.expect(std::filesystem::is_regular_file(
                    products.directory() / "T02DefaultAbi.pdb") &&
                    std::filesystem::file_size(
                        products.directory() / "T02DefaultAbi.pdb") > 0U,
                "TL-LANG-307 the producer default witnesses must retain a non-empty PDB");

            const auto result = external_program_support::run(
                consumerExecutable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-307 static and dynamic default dispatch must produce the same result: " +
                    result.error + result.output);
        }});
}

void registerListPatternShapeCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-308", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "list", "exact", "prefix", "suffix", "discard"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T03ListShapes", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        int[] values = new int[4];
        values[0] = 10;
        values[1] = 20;
        values[2] = 30;
        values[3] = 40;
        int exact = match(values) { [10, 20, _, 40] => 20, _ => 0 };
        int prefix = match(values) { [10, 20, ..] => 10, _ => 0 };
        int suffix = match(values) { [.., 30, 40] => 12, _ => 0 };
        delete[] values;
        return exact + prefix + suffix;
    }
}

)ns");
            const std::filesystem::path executable =
                products.directory() / "T03ListShapes.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-308 exact, prefix, suffix and discard list patterns must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-308 list-shape lowering must preserve ordered element tests: " +
                    result.error + result.output);
        }});
}

void registerListPatternRestCaptureCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-309", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "list", "rest", "slice", "borrow"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T03ListRestCapture", "Exe", R"ns(
public copy borrowed struct PatternSlice
{
    private int* data;
    public int Length;

    public PatternSlice(int* source, int length)
    {
        data = source;
        Length = length;
    }

    public readonly int this[int index]
    {
        get { return data[index]; }
    }

    public readonly PatternSlice Slice(int start, int length)
    {
        return new PatternSlice(data + start, length);
    }
}

class Program
{
    public static int Main()
    {
        int* storage = stackalloc int[4];
        storage[0] = 10;
        storage[1] = 20;
        storage[2] = 30;
        storage[3] = 40;
        PatternSlice values = new PatternSlice(storage, 4);
        return match(values)
        {
            [10, .. var middle, 40] => middle.Length + middle[0] + middle[1],
            _ => 0
        };
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T03ListRestCapture.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-309 rest capture must compile as a borrowed Slice view: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 52,
                "TL-LANG-309 rest capture must preserve origin-backed elements without copying: " +
                    result.error + result.output);

            const std::filesystem::path escapeProject = products.writeProject(
                "T03ListRestEscape", "Exe", R"ns(
public copy borrowed struct EscapeSlice
{
    private int* data;
    public int Length;
    public EscapeSlice(int* source, int length) { data = source; Length = length; }
    public readonly int this[int index] { get { return data[index]; } }
    public readonly EscapeSlice Slice(int start, int length)
    {
        return new EscapeSlice(data + start, length);
    }
}

class Program
{
    public static EscapeSlice Escape()
    {
        int* storage = stackalloc int[4];
        EscapeSlice values = new EscapeSlice(storage, 4);
        return match(values) { [.. var middle] => middle, _ => values };
    }
    public static int Main() { return Escape().Length; }
}
)ns");
            const external_program_support::Result escaped = emitT01Product(
                L"--emit-project-oop-exe", escapeProject,
                products.directory() / "T03ListRestEscape.exe", products.directory());
            const std::string escapeDiagnostics = escaped.error + escaped.output;
            context.expect(escaped.started && escaped.exitCode != 0 &&
                    escapeDiagnostics.find("borrows from local 'storage' and cannot be returned") !=
                        std::string::npos,
                "TL-LANG-309 a rest view rooted in local storage must not escape: " +
                    escapeDiagnostics);
        }});
}

void registerListPatternSubstratesCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-310", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "list", "rest", "array", "string", "borrow", "view"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::filesystem::path coreDir = repository / "lib/NativeSharp.Core";
            std::error_code copyError;
            for (const char* file : {"Object.ns", "Number.ns", "NumericLimits.ns",
                     "NumericParsing.ns", "String.ns", "Span.ns", "StringView.ns"})
            {
                copyError.clear();
                std::filesystem::copy_file(coreDir / file, products.directory() / file,
                    std::filesystem::copy_options::overwrite_existing, copyError);
            }
            const std::filesystem::path project = products.writeProject(
                "T03ListSubstrates", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        string text = "hello";
        int stringRest = match(text) { [.. var all] => all.Length(), _ => 0 };

        return stringRest;
    }
}
)ns", "<Compile Include=\"Object.ns\"/><Compile Include=\"Number.ns\"/>"
                "<Compile Include=\"NumericLimits.ns\"/><Compile Include=\"NumericParsing.ns\"/>"
                "<Compile Include=\"String.ns\"/><Compile Include=\"Span.ns\"/>"
                "<Compile Include=\"StringView.ns\"/>");
            const std::filesystem::path executable =
                products.directory() / "T03ListSubstrates.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-310 array and string rest capture must compile as Span/StringView: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 5,
                "TL-LANG-310 rest views must preserve origin elements without copying: " +
                    result.error + result.output);
        }});
}

void registerPropertyPatternCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-311", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "property", "relational", "logical", "not"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T03PropertyPatterns", "Exe", R"ns(
public copy borrowed struct Point
{
    public int X;
    public int Y;
    public Point(int x, int y) { X = x; Y = y; }
}

class Program
{
    public static int Main()
    {
        Point p = new Point(5, 10);
        int relational = match(p) { { X: >= 5 } => 1, _ => 0 };
        int logical = match(p) { { X: >= 5 & <= 10 } => 2, _ => 0 };
        int notPat = match(p) { { X: !3 } => 4, _ => 0 };
        return relational + logical + notPat;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T03PropertyPatterns.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-311 relational and logical property patterns must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-311 property sub-patterns must evaluate correctly: " +
                    result.error + result.output);
        }});
}

void registerListExhaustivenessCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-312", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "list", "exhaustiveness", "length"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T03ListExhaustiveness", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        int[] values = new int[2];
        values[0] = 1;
        values[1] = 2;
        return match(values)
        {
            [var x] => x,
            [var x, var y] => x + y
        };
    }
}
)ns");
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project,
                products.directory() / "T03ListExhaustiveness.exe", products.directory());
            const std::string diagnostics = emitted.error + emitted.output;
            context.expect(emitted.started && emitted.exitCode != 0 &&
                    diagnostics.find("missing list length") != std::string::npos,
                "TL-LANG-312 a list match without rest/wildcard must be non-exhaustive: " +
                    diagnostics);
        }});
}

void registerPatternBindingModeCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-313", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "binding", "copy", "ref", "move"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T03PatternBindingModes", "Exe", R"ns(
public copy borrowed struct Point
{
    public int X;
    public int Y;
    public Point(int x, int y) { X = x; Y = y; }
}

class Program
{
    public static int Main()
    {
        Point p = new Point(5, 10);
        int copyVal = match(p) { { X: copy cx } => cx, _ => 0 };
        int moveVal = match(p) { { X: move mx } => mx, _ => 0 };
        int refVal = match(p) { { X: ref rx } => rx, _ => 0 };
        return copyVal + moveVal + refVal;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T03PatternBindingModes.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-313 copy/move/ref pattern bindings must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 15,
                "TL-LANG-313 pattern binding modes must preserve values: " +
                    result.error + result.output);
        }});
}

void registerLambdaNaturalTypeCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-314", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "lambda", "callable", "inference", "natural"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T04LambdaNaturalType", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        var f = (int x) => x + 1;
        return f(41);
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T04LambdaNaturalType.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-314 a lambda must infer its natural callable type under var: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-314 a naturally-typed lambda must be invocable: " +
                    result.error + result.output);
        }});
}

void registerLambdaParameterModifierCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-316", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "lambda", "callable", "parameter", "ref", "inference"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T04LambdaParams", "Exe", R"ns(
class Program
{
    public delegate void Bump(ref int value);

    public static int Main()
    {
        int x = 10;
        Bump bump = (ref value) => { value = value + 1; };
        bump(ref x);
        return x;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T04LambdaParams.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-316 a lambda with a ref parameter must infer its type from the target: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 11,
                "TL-LANG-316 a ref lambda must write through the reference: " +
                    result.error + result.output);
        }});
}

}

void registerLambdaDefaultParamCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-317", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "lambda", "callable", "parameter", "default", "optional"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T04LambdaDefaults", "Exe", R"ns(
class Program
{
    public delegate int Add(int value = 5);

    public static int Main()
    {
        Add add = (int x = 5) => x + 1;
        return add();
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T04LambdaDefaults.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-317 a lambda with a default parameter must compile against the target: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 6,
                "TL-LANG-317 the default parameter must apply when the argument is omitted: " +
                    result.error + result.output);
        }});
}

void registerStaticLambdaCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-318", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "lambda", "static", "capture", "prohibition"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T04StaticLambda", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        var f = static (int x) => x + 1;
        return f(41);
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T04StaticLambda.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-318 a captureless static lambda must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-318 a static lambda must remain invocable: " +
                    result.error + result.output);

            const std::filesystem::path badProject = products.writeProject(
                "T04StaticLambdaCapture", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        int captured = 5;
        var f = static (int x) => x + captured;
        return f(1);
    }
}
)ns");
            const external_program_support::Result badEmitted = emitT01Product(
                L"--emit-project-oop-exe", badProject,
                products.directory() / "T04StaticLambdaCapture.exe", products.directory());
            context.expect(badEmitted.started && badEmitted.exitCode != 0,
                "TL-LANG-318 a static lambda must reject capture of the enclosing scope: " +
                    badEmitted.output);
        }});
}

void registerMethodGroupNaturalTypeCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-319", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "method-group", "callable", "inference", "natural"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T04MethodGroup", "Exe", R"ns(
class Program
{
    public static int Double(int x)
    {
        return x * 2;
    }

    public static int Main()
    {
        var f = Double;
        return f(21);
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T04MethodGroup.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-319 a unique method group must infer its natural callable type: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-319 a naturally-typed method group must be invocable: " +
                    result.error + result.output);
        }});
}

void registerClosureErrorCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-320", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "closure", "error", "layout", "drop"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T04ClosureError", "Exe", R"ns(
class Program
{
    public delegate int Map(int x);

    public static int Main()
    {
        int captured = 5;
        Map m = (int x) => x + captured;
        return m(37);
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T04ClosureError.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-320 a capturing closure must compile with layout+drop: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-320 a capturing closure must execute: " +
                    result.error + result.output);
        }});
}

void registerClosureErrorThrowCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-321", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "closure", "error", "throws"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::filesystem::path coreDir = repository / "lib/NativeSharp.Core";
            std::error_code copyError;
            for (const char* file : {"Object.ns", "Number.ns", "NumericLimits.ns",
                     "NumericParsing.ns", "String.ns", "Exception.ns"})
            {
                copyError.clear();
                std::filesystem::copy_file(coreDir / file, products.directory() / file,
                    std::filesystem::copy_options::overwrite_existing, copyError);
            }
            const std::filesystem::path project = products.writeProject(
                "T04ClosureThrows", "Exe", R"ns(
using NativeSharp.Core;

class Program
{
    public delegate int Map(int x) throws Exception;

    public static int Main()
    {
        int captured = 5;
        Map m = (int x) => x + captured;
        try
        {
            return m(37);
        }
        catch (Exception)
        {
            return 0;
        }
    }
}
)ns", "<Compile Include=\"Object.ns\"/><Compile Include=\"Number.ns\"/>"
                "<Compile Include=\"NumericLimits.ns\"/><Compile Include=\"NumericParsing.ns\"/>"
                "<Compile Include=\"String.ns\"/><Compile Include=\"Exception.ns\"/>");
            const std::filesystem::path executable =
                products.directory() / "T04ClosureThrows.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-321 a closure over a throws-delegate must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-321 a closure over a throws-delegate must execute: " +
                    result.error + result.output);
        }});
}

void registerExtensionMethodCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-323", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "extension", "method", "receiver"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T08Extension", "Exe", R"ns(
namespace Suite
{
    extension int
    {
        public int Double()
        {
            return this * 2;
        }
    }
}

using Suite;

class Program
{
    public static int Main()
    {
        return 21.Double();
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T08Extension.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-323 an extension method must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-323 an extension method must dispatch on the receiver: " +
                    result.error + result.output);
        }});
}

void registerLocalFunctionCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-325", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "local-function"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T04LocalFunction", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        int Double(int n)
        {
            return n * 2;
        }
        return Double(21);
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T04LocalFunction.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-325 a local function must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-325 a local function must be invocable: " +
                    result.error + result.output);
        }});
}

void registerLocalFunctionRecursionCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-326", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "local-function", "recursion"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T04LocalFunctionRecursion", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        int Fact(int n)
        {
            return n <= 1 ? 1 : n * Fact(n - 1);
        }
        return Fact(5);
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T04LocalFunctionRecursion.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-326 a recursive local function must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 120,
                "TL-LANG-326 a recursive local function must self-invoke: " +
                    result.error + result.output);
        }});
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
    registerStaticLocalLifetimeCase(registry);
    registerConditionDeclarationCase(registry);
    registerT01IntegrationCase(registry);
    registerDefaultInterfaceBodyCase(registry);
    registerDefaultImplementationPrecedenceCase(registry);
    registerDefaultDiamondAndCoherenceCase(registry);
    registerGenericDefaultIntegrationCase(registry);
    registerMetadataDefaultWitnessCase(registry);
    registerDefaultDispatchAbiCase(registry);
    registerListPatternShapeCase(registry);
    registerListPatternRestCaptureCase(registry);
    registerListPatternSubstratesCase(registry);
    registerPropertyPatternCase(registry);
    registerListExhaustivenessCase(registry);
    registerPatternBindingModeCase(registry);
    registerLambdaNaturalTypeCase(registry);
    registerLambdaParameterModifierCase(registry);
    registerLambdaDefaultParamCase(registry);
    registerStaticLambdaCase(registry);
    registerMethodGroupNaturalTypeCase(registry);
    registerClosureErrorCase(registry);
    registerClosureErrorThrowCase(registry);
    registerExtensionMethodCase(registry);
    registerLocalFunctionCase(registry);
    registerLocalFunctionRecursionCase(registry);
}
}
