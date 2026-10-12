#include "FrontendContractSupport.h"
#include "ExternalProgramTestSupport.h"
#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
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
        std::string_view source) const
    {
        const std::filesystem::path sourceFile = directory_ / (std::string(name) + ".ns");
        const std::filesystem::path projectFile = directory_ / (std::string(name) + ".nsproj");
        write(sourceFile, source);
        write(projectFile,
            "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name><OutputType>" + std::string(outputType) +
            "</OutputType><Language>3</Language></PropertyGroup><ItemGroup><Compile Include=\"" +
            sourceFile.filename().string() + "\"/></ItemGroup></Project>");
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
}
}
