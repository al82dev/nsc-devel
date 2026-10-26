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
    {Language3Feature::Decimal, "decimal", Language3Disposition::Implemented,
        "frozen 128-bit value ABI (coefficient 96 + scale + sign) with arithmetic, overflow, conversion, formatting and FFI contracts"},
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
        "decimal=implemented:frozen 128-bit value ABI (coefficient 96 + scale + sign) with arithmetic, overflow, "
        "conversion, formatting and FFI contracts\n"
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
        context.expect(planned == 5 && deferred == 0 && rejected == 1,
            "TL-LANG-160 the catalog must expose five nonproductive 3.0.4 plans and one rejected feature");
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
            written.formatMajor == 1 &&
            written.formatMinor == nativesharp::interop::kMetadataFormatMinor &&
            written.languageFeatureFlags == nativesharp::interop::kMetadataLanguageFeatureMask,
        "TL-LANG-301 the current NSMETA format must publish all three closed T01 capabilities: " + error);

    nativesharp::interop::LibraryMetadata decoded;
    error.clear();
    const bool read = codec.read(metadataFile, decoded, error);
    context.expect(read && error.empty() &&
            decoded.languageFeatureFlags == nativesharp::interop::kMetadataLanguageFeatureMask,
        "TL-LANG-301 the T01 capability catalogue must survive NSMETA round-trip: " + error);

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
        raii TraitItem traitItem = new TraitItem();
        borrowed dyn ITraitDefaults traitView = traitItem;
        raii InterfaceItem interfaceItem = new InterfaceItem();
        borrowed IInterfaceDefaults interfaceView = interfaceItem;
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
        raii ExplicitItem explicitItem = new ExplicitItem();
        borrowed dyn IExplicit explicitView = explicitItem;
        raii NominalItem nominalItem = new NominalItem();
        borrowed dyn INominal nominalView = nominalItem;
        raii DefaultItem defaultItem = new DefaultItem();
        borrowed ICombined defaultView = defaultItem;
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
        raii Item item = new Item();
        borrowed IResolved value = item;
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
        raii Local local = new Local();
        borrowed dyn IScore score = local;
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
impl IScore for Foreign { int Score() { return 42; } }
class Program { public static int Main() { return 0; } }
)ns", libraryReference);
            const external_program_support::Result orphan = runT01Compiler(
                {L"--dump-project-interfaces", orphanProject.wstring()}, products.directory());
            context.expect(orphan.started && orphan.exitCode != 0 &&
                    orphan.output.find("orphan impl: the module owns neither") !=
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
        raii Number number = new Number();
        borrowed dyn IValue<int> erased = number;
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
        raii Local local = new Local();
        borrowed dyn IDefaults value = local;
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
        raii Local local = new Local();
        borrowed dyn IDefaults dynamicValue = local;
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
        }});
}

void registerListPatternStackEscapeCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-365", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "list", "rest", "slice", "borrow", "stackalloc", "escape"},
        [](TestContext& context) {
            TemporaryT01Products products;
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
                "TL-LANG-365 a rest view rooted in stack storage must not escape through return: " +
                    escapeDiagnostics);
        }});
}

constexpr std::string_view kT03ListPatternCoreItems =
    "<Compile Include=\"Object.ns\"/><Compile Include=\"Number.ns\"/>"
    "<Compile Include=\"NumericLimits.ns\"/><Compile Include=\"NumericParsing.ns\"/>"
    "<Compile Include=\"String.ns\"/><Compile Include=\"Span.ns\"/>"
    "<Compile Include=\"StringView.ns\"/>";

bool stageT03ListPatternCore(TestContext& context, TemporaryT01Products& products,
    std::string_view caseId)
{
    const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
    const std::filesystem::path coreDir = repository / "lib/NativeSharp.Core";
    for (const char* file : {"Object.ns", "Number.ns", "NumericLimits.ns",
             "NumericParsing.ns", "String.ns", "Span.ns", "StringView.ns"})
    {
        std::error_code copyError;
        std::filesystem::copy_file(coreDir / file, products.directory() / file,
            std::filesystem::copy_options::overwrite_existing, copyError);
        context.expect(!copyError,
            std::string(caseId) + " must stage NativeSharp.Core/" + file + ": " +
                copyError.message());
        if (copyError) return false;
    }
    return true;
}

void verifyT03ListPatternSubstrate(TestContext& context, std::string_view caseId,
    std::string_view projectName, std::string_view source, int expectedExit)
{
    TemporaryT01Products products;
    if (!stageT03ListPatternCore(context, products, caseId)) return;
    const std::filesystem::path project = products.writeProject(
        projectName, "Exe", source, kT03ListPatternCoreItems);
    const std::filesystem::path executable =
        products.directory() / (std::string(projectName) + ".exe");
    const external_program_support::Result emitted = emitT01Product(
        L"--emit-project-oop-exe", project, executable, products.directory());
    context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
        std::string(caseId) + " substrate fixture must compile: " + emitted.error + emitted.output);
    if (!emitted.started || emitted.exitCode != 0) return;
    const auto result = external_program_support::run(
        executable, {}, {}, products.directory());
    context.expect(result.started && result.error.empty() &&
            result.exitCode == static_cast<decltype(result.exitCode)>(expectedExit),
        std::string(caseId) + " rest view must preserve the selected substrate: " +
            result.error + result.output + " exit=" + std::to_string(result.exitCode));
}

void registerListPatternStringCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-310", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "list", "rest", "string", "borrow", "view"},
        [](TestContext& context) {
            verifyT03ListPatternSubstrate(context, "TL-LANG-310", "T03ListString", R"ns(
class Program
{
    public static int Main()
    {
        string text = "hello";
        int stringRest = match(text) { [.. var all] => all.Length(), _ => 0 };

        return stringRest;
    }
}
)ns", 5);
        }});
}

void registerListPatternArrayCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-357", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "list", "rest", "array", "borrow", "view"},
        [](TestContext& context) {
            verifyT03ListPatternSubstrate(context, "TL-LANG-357", "T03ListArray", R"ns(
class Program
{
    public static int Main()
    {
        int[] values = new int[4];
        values[0] = 10; values[1] = 20; values[2] = 30; values[3] = 40;
        return match(values)
        {
            [10, .. var middle, 40] => middle.Length() + middle[0] + middle[1],
            _ => 0
        };
    }
}
)ns", 52);
        }});
}

void registerListPatternInlineArrayCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-358", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "list", "rest", "inline-array", "borrow", "view"},
        [](TestContext& context) {
            verifyT03ListPatternSubstrate(context, "TL-LANG-358", "T03ListInlineArray", R"ns(
class Program
{
    private static int Read(in int[4] values)
    {
        return match(values)
        {
            [10, .. var middle, 40] => middle.Length() + middle[0] + middle[1],
            _ => 0
        };
    }

    public static int Main()
    {
        int[4] values;
        values[0] = 10; values[1] = 20; values[2] = 30; values[3] = 40;
        int mutableResult = match(values)
        {
            [10, .. var middle, 40] => middle.Length() + middle[0] + middle[1],
            _ => 0
        };
        int emptyAtEnd = match(values)
        {
            [10, 20, 30, 40, .. var empty] => empty.Length(),
            _ => -1
        };
        return mutableResult + Read(in values) + emptyAtEnd;
    }
}
)ns", 104);
        }});
}

void registerListPatternSpanCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-359", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "list", "rest", "span", "borrow", "view"},
        [](TestContext& context) {
            verifyT03ListPatternSubstrate(context, "TL-LANG-359", "T03ListSpan", R"ns(
using NativeSharp.Core;
class Program
{
    public static int Main()
    {
        int[] storage = new int[4];
        storage[0] = 10; storage[1] = 20; storage[2] = 30; storage[3] = 40;
        Span<int> values = Spans.Of<int>(storage);
        return match(values)
        {
            [10, .. var middle, 40] => middle.Length() + middle[0] + middle[1],
            _ => 0
        };
    }
}
)ns", 52);
        }});
}

void registerListPatternReadOnlySpanCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-360", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "list", "rest", "readonly-span", "borrow", "view"},
        [](TestContext& context) {
            verifyT03ListPatternSubstrate(context, "TL-LANG-360", "T03ListReadOnlySpan", R"ns(
using NativeSharp.Core;
class Program
{
    public static int Main()
    {
        int[] storage = new int[4];
        storage[0] = 10; storage[1] = 20; storage[2] = 30; storage[3] = 40;
        ReadOnlySpan<int> values = Spans.ReadOnlyOf<int>(storage);
        return match(values)
        {
            [10, .. var middle, 40] => middle.Length() + middle[0] + middle[1],
            _ => 0
        };
    }
}
)ns", 52);
        }});
}

void registerListPatternStringViewCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-361", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "list", "rest", "string-view", "borrow", "view"},
        [](TestContext& context) {
            verifyT03ListPatternSubstrate(context, "TL-LANG-361", "T03ListStringView", R"ns(
using NativeSharp.Core;
class Program
{
    public static int Main()
    {
        StringView values = StringView.Of("hello");
        return match(values) { [.. var all] => all.Length(), _ => 0 };
    }
}
)ns", 5);
        }});
}

void registerListPatternProtocolCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-362", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "list", "rest", "protocol", "borrow", "view"},
        [](TestContext& context) {
            verifyT03ListPatternSubstrate(context, "TL-LANG-362", "T03ListProtocol", R"ns(
public copy borrowed struct PatternCollection
{
    private int* data;
    public PatternCollection(int* source, int length) { data = source; Count = length; }
    public int Count;
    public readonly int Length() { return Count; }
    public readonly int this[int index] { get { return data[index]; } }
    public readonly PatternCollection Slice(int start, int length)
    {
        return new PatternCollection(data + start, length);
    }
}
class Program
{
    public static int Main()
    {
        int* storage = stackalloc int[4];
        storage[0] = 10; storage[1] = 20; storage[2] = 30; storage[3] = 40;
        PatternCollection values = new PatternCollection(storage, 4);
        return match(values)
        {
            [10, .. var middle, 40] => middle.Length() + middle[0] + middle[1],
            _ => 0
        };
    }
}
)ns", 52);
        }});
}

void registerListPatternUtf8StringCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-363", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "list", "rest", "string", "utf8", "byte-semantics"},
        [](TestContext& context) {
            verifyT03ListPatternSubstrate(context, "TL-LANG-363", "T03ListUtf8String", R"ns(
class Program
{
    public static int Main()
    {
        string text = "Méx";
        return match(text)
        {
            ['M', .. var middle, 'x'] =>
                middle.Length() == 2 &&
                (int)middle[0] == 195 &&
                (int)middle[1] == 169 ? 42 : 0,
            _ => 0
        };
    }
}
)ns", 42);
        }});
}

void registerListPatternAliasingCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-364", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "list", "rest", "span", "alias", "readonly"},
        [](TestContext& context) {
            verifyT03ListPatternSubstrate(context, "TL-LANG-364", "T03ListRestAliasing", R"ns(
class Program
{
    public static int Main()
    {
        int[] array = new int[4];
        array[0] = 10; array[1] = 20; array[2] = 30; array[3] = 40;
        int arrayWrite = match(array)
        {
            [10, .. var middle, 40] => middle[0] = 71,
            _ => 0
        };
        if (arrayWrite != 71 || array[1] != 71) { return 1; }

        int[4] fixedValues;
        fixedValues[0] = 10; fixedValues[1] = 20;
        fixedValues[2] = 30; fixedValues[3] = 40;
        int fixedWrite = match(fixedValues)
        {
            [10, .. var middle, 40] => middle[1] = 72,
            _ => 0
        };
        if (fixedWrite != 72 || fixedValues[2] != 72) { return 2; }
        return 42;
    }
}
)ns", 42);

            TemporaryT01Products readonlyProducts;
            if (!stageT03ListPatternCore(context, readonlyProducts, "TL-LANG-364")) return;
            const std::filesystem::path readonlyProject = readonlyProducts.writeProject(
                "T03ListRestReadonly", "Exe", R"ns(
class Program
{
    private static int Invalid(in int[4] values)
    {
        return match(values)
        {
            [10, .. var middle, 40] => middle[0] = 99,
            _ => 0
        };
    }
    public static int Main()
    {
        int[4] values;
        return Invalid(in values);
    }
}
)ns", kT03ListPatternCoreItems);
            const external_program_support::Result rejected = emitT01Product(
                L"--emit-project-oop-exe", readonlyProject,
                readonlyProducts.directory() / "T03ListRestReadonly.exe",
                readonlyProducts.directory());
            const std::string diagnostics = rejected.error + rejected.output;
            context.expect(rejected.started && rejected.exitCode != 0 &&
                    diagnostics.find("property/indexer has no accessible setter") !=
                        std::string::npos,
                "TL-LANG-364 readonly fixed-array rest must reject mutation with the exact "
                "receiver diagnostic: " + diagnostics);
        }});
}

void registerListPatternNoHeapCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-366", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "list", "rest", "noheap", "no-copy", "mir", "borrow"},
        [](TestContext& context) {
            TemporaryT01Products products;
            if (!stageT03ListPatternCore(context, products, "TL-LANG-366")) return;
            const std::filesystem::path project = products.writeProject(
                "T03ListNoHeap", "Exe", R"ns(
class Program
{
    public static int Read(int[] values)
    {
        return match(values)
        {
            [10, .. var middle, 40] => middle.Length() + middle[0] + middle[1],
            _ => 0
        };
    }
}
)ns", kT03ListPatternCoreItems);
            nativesharp::frontend::ProjectCompilation compilation;
            std::string error;
            const bool compiled = compileTo(
                compilation, ProjectStage::Mir, project, error);
            context.expect(compiled && error.empty(),
                "TL-LANG-366 list rest must compile through MIR: " +
                    projectFailureEvidence(compilation, error));
            if (!compiled || compilation.oopMirModel() == nullptr) return;

            std::size_t allocationNodes = 0;
            std::size_t restBorrows = 0;
            bool inspectedRead = false;
            const auto countKind = [&](const nativesharp::frontend::ProjectOopMirNode* body,
                                       nativesharp::frontend::ProjectOopMirNodeKind kind)
            {
                std::vector<const nativesharp::frontend::ProjectOopMirNode*> found;
                collectMirKind(body, kind, found);
                return found.size();
            };
            const auto* memberModel = compilation.memberModel();
            for (const auto& function : compilation.oopMirModel()->functions())
            {
                if (memberModel == nullptr || function.memberOrdinal >= memberModel->members().size() ||
                    memberModel->members()[function.memberOrdinal].name != "Read")
                {
                    continue;
                }
                inspectedRead = true;
                const auto* body = function.body.get();
                allocationNodes += countKind(body, nativesharp::frontend::ProjectOopMirNodeKind::ArrayAllocate);
                allocationNodes += countKind(body, nativesharp::frontend::ProjectOopMirNodeKind::StackAllocation);
                allocationNodes += countKind(body, nativesharp::frontend::ProjectOopMirNodeKind::RawAllocation);
                allocationNodes += countKind(body, nativesharp::frontend::ProjectOopMirNodeKind::RangeArrayAllocate);
                allocationNodes += countKind(body, nativesharp::frontend::ProjectOopMirNodeKind::AllocateAndConstruct);
                restBorrows += countKind(body, nativesharp::frontend::ProjectOopMirNodeKind::SpanFromArray);
                restBorrows += countKind(body, nativesharp::frontend::ProjectOopMirNodeKind::ReadOnlySpanFromArray);
            }
            context.expect(inspectedRead,
                "TL-LANG-366 the Read function must be present in MIR");
            context.expect(allocationNodes == 0,
                "TL-LANG-366 the list rest path must not allocate: " +
                    std::to_string(allocationNodes) + " allocation node(s) found");
            context.expect(restBorrows >= 1,
                "TL-LANG-366 the list rest capture must be a borrow (SpanFromArray), not a copy");
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
        int notAnd = match(p) { { X: !3 & >= 10 } => 8, _ => 0 };
        int notOr = match(p) { { X: !5 | >= 5 } => 16, _ => 0 };
        int grouped = match(p) { { X: !(5 & >= 10) } => 32, _ => 0 };
        int combined = match(p) { { X: !3 & >= 5 | 5 } => 64, _ => 0 };
        int guarded = match(p) { { X: !5 | >= 5 } when relational == 1 => 128, _ => 0 };
        return relational + logical + notPat + notAnd + notOr + grouped + combined + guarded;
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
            context.expect(result.started && result.error.empty() && result.exitCode == 247,
                "TL-LANG-311 pattern precedence must be ! before & before | while grouping and "
                "guards remain explicit: " + result.error + result.output +
                    " exit=" + std::to_string(result.exitCode));
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

            const std::filesystem::path exhaustiveProject = products.writeProject(
                "T03ListOrExactRest", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        int[] values = new int[2];
        return match(values)
        {
            [] | [_, ..] => 7
        };
    }
}
)ns");
            const std::filesystem::path exhaustiveExecutable =
                products.directory() / "T03ListOrExactRest.exe";
            const external_program_support::Result exhaustive = emitT01Product(
                L"--emit-project-oop-exe", exhaustiveProject, exhaustiveExecutable,
                products.directory());
            context.expect(exhaustive.started && exhaustive.exitCode == 0 &&
                    exhaustive.error.empty(),
                "TL-LANG-312 exact OR rest list domains must be exhaustive: " +
                    exhaustive.error + exhaustive.output);
            if (exhaustive.started && exhaustive.exitCode == 0)
            {
                const auto result = external_program_support::run(
                    exhaustiveExecutable, {}, {}, products.directory());
                context.expect(result.started && result.error.empty() && result.exitCode == 7,
                    "TL-LANG-312 exact OR rest list domains must select the rest alternative: " +
                        result.error + result.output);
            }

            const auto expectRejected = [&](std::string_view projectName,
                                            std::string_view source,
                                            std::string_view diagnostic) {
                const std::filesystem::path rejectedProject = products.writeProject(
                    projectName, "Exe", source);
                const external_program_support::Result rejected = emitT01Product(
                    L"--emit-project-oop-exe", rejectedProject,
                    products.directory() / (std::string(projectName) + ".exe"),
                    products.directory());
                const std::string rejectedDiagnostics = rejected.error + rejected.output;
                context.expect(rejected.started && rejected.exitCode != 0 &&
                        rejectedDiagnostics.find(diagnostic) != std::string::npos,
                    "TL-LANG-312 expected diagnostic '" + std::string(diagnostic) + "': " +
                        rejectedDiagnostics);
            };

            expectRejected("T03ListOrRedundancy", R"ns(
class Program
{
    public static int Main()
    {
        int[] values = new int[2];
        return match(values)
        {
            [_] | [_, _] => 1,
            [_] => 2,
            [..] => 3
        };
    }
}
)ns", "match arm #2 is unreachable for the list-length domain");

            expectRejected("T03OpenDomain", R"ns(
class Program
{
    public static int Main()
    {
        int value = 1;
        return match(value) { 1 => 1 };
    }
}
)ns", "match over an open or unresolved domain requires a final wildcard or binding arm");
        }});
}

void registerPatternDiagnosticsCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-315", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "diagnostic", "redundancy", "unreachable", "move"},
        [](TestContext& context) {
            TemporaryT01Products products;
            if (!stageT03ListPatternCore(context, products, "TL-LANG-315")) return;

            const std::filesystem::path redundantProject = products.writeProject(
                "T03RedundantListArm", "Exe", R"ns(
class Program
{
    public static int Read(int[] a)
    {
        return match(a)
        {
            [.. var r] => r.Length(),
            [var x] => x
        };
    }
}
)ns", kT03ListPatternCoreItems);
            const external_program_support::Result redundant = emitT01Product(
                L"--emit-project-oop-exe", redundantProject,
                products.directory() / "T03RedundantListArm.exe", products.directory());
            context.expect(redundant.started && redundant.exitCode != 0 &&
                    (redundant.error + redundant.output).find("unreachable") != std::string::npos,
                "TL-LANG-315 a redundant list arm must be unreachable: " +
                    redundant.error + redundant.output);
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
        int mutVal = match(p) { { X: ref mut rw } => rw = rw + 1, _ => 0 };
        return copyVal + moveVal + refVal + mutVal + p.X;
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
            context.expect(result.started && result.error.empty() && result.exitCode == 27,
                "TL-LANG-313 pattern binding modes must preserve values and ref mut writes: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));

            const std::filesystem::path readonlyProject = products.writeProject(
                "T03ReadonlyPatternBinding", "Exe", R"ns(
public copy borrowed struct Point
{
    public int X;
    public Point(int x) { X = x; }
}

class Program
{
    public static int Main()
    {
        Point p = new Point(5);
        return match(p) { { X: ref rx } => rx = 9, _ => 0 };
    }
}
)ns");
            const external_program_support::Result readonlyEmitted = emitT01Product(
                L"--emit-project-oop-exe", readonlyProject,
                products.directory() / "T03ReadonlyPatternBinding.exe", products.directory());
            const std::string readonlyDiagnostics = readonlyEmitted.error + readonlyEmitted.output;
            context.expect(readonlyEmitted.started && readonlyEmitted.exitCode != 0 &&
                    readonlyDiagnostics.find("left side of assignment is not assignable") !=
                        std::string::npos,
                "TL-LANG-313 ref binding must reject writes while ref mut remains writable: " +
                    readonlyDiagnostics);
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
    public delegate int Map(int x);

    public static int Main()
    {
        var f = static (int x) => x + 1;
        int staticBias = 1;
        Map ordinary = [copy staticBias] (int x) => x + staticBias;
        return f(20) + ordinary(20);
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
                "TL-LANG-318 structured static metadata must not reject an ordinary capture "
                "whose name contains 'static': " +
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

void registerMethodGroupRankingCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-367", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "method-group", "ranking", "fn-pointer", "fn-reference", "overload"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T04MethodGroupRanking", "Exe", R"ns(
class Program
{
    public static int Pick(int x) { return x; }
    public static long Pick(long x) { return x + 100; }

    public static int Main()
    {
        fn*(int) -> int f = Pick;
        int a = f(21);
        int b = f(40);
        fn&(int) -> int r = Pick;
        int c = r(0);
        return a + b + c;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T04MethodGroupRanking.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-367 fn*/fn& method-group ranking must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 61,
                "TL-LANG-367 fn* target must rank the int overload (21 + 40 + 0 = 61): " +
                    result.error + result.output);

            const std::filesystem::path captureProject = products.writeProject(
                "T04FnPointerCapture", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        int y = 5;
        fn*(int) -> int f = (x) => x + y;
        return f(1);
    }
}
)ns");
            const external_program_support::Result captureRejected = emitT01Product(
                L"--emit-project-oop-exe", captureProject,
                products.directory() / "T04FnPointerCapture.exe", products.directory());
            context.expect(captureRejected.started && captureRejected.exitCode != 0 &&
                    (captureRejected.error + captureRejected.output).find("capture") !=
                        std::string::npos,
                "TL-LANG-367 a capturing lambda must not convert to fn* (no environment): " +
                    captureRejected.error + captureRejected.output);

            const std::filesystem::path ambiguousProject = products.writeProject(
                "T04MethodGroupAmbiguous", "Exe", R"ns(
class Program
{
    public static int Pick(int x) { return x; }
    public static long Pick(long x) { return x + 100; }
    public static int Main()
    {
        var f = Pick;
        return f(1);
    }
}
)ns");
            const external_program_support::Result ambiguousRejected = emitT01Product(
                L"--emit-project-oop-exe", ambiguousProject,
                products.directory() / "T04MethodGroupAmbiguous.exe", products.directory());
            context.expect(ambiguousRejected.started && ambiguousRejected.exitCode != 0 &&
                    (ambiguousRejected.error + ambiguousRejected.output).find("multiple overloads") !=
                        std::string::npos,
                "TL-LANG-367 a multi-overload method group must report ambiguity under var: " +
                    ambiguousRejected.error + ambiguousRejected.output);
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

void registerClosureDropOracleCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-368", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "closure", "drop", "owned", "capture", "destructor"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T04ClosureDrop", "Exe", R"ns(
delegate int Read();
class Box
{
    public static int Drops;
    public int Value;
    public Box(int value) { Value = value; }
    ~Box() { Drops = Drops + 1; }
}
class Program
{
    public static int Main()
    {
        int observed = 0;
        {
            raii Box box = new Box(42);
            raii Read read = [move box] () => box.Value;
            observed = read();
        }
        return Box.Drops * 100 + observed;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T04ClosureDrop.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-368 a move-captured closure must compile and drop its owner: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 142,
                "TL-LANG-368 the owned capture must be dropped exactly once (Drops=1, value=42): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));

            const std::filesystem::path escapeProject = products.writeProject(
                "T04ClosureEscape", "Exe", R"ns(
delegate int Read();
class Program
{
    public static Read Make()
    {
        int captured = 5;
        return (Read)(() => captured);
    }
    public static int Main()
    {
        Read read = Make();
        return read();
    }
}
)ns");
            const external_program_support::Result escapeRejected = emitT01Product(
                L"--emit-project-oop-exe", escapeProject,
                products.directory() / "T04ClosureEscape.exe", products.directory());
            context.expect(escapeRejected.started && escapeRejected.exitCode != 0 &&
                    (escapeRejected.error + escapeRejected.output).find("captured") !=
                        std::string::npos,
                "TL-LANG-368 a closure escaping a captured local must be rejected: " +
                    escapeRejected.error + escapeRejected.output);
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

void registerGlobalNamespaceExtensionCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-388", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "extension", "method", "global-namespace"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T08ExtensionGlobal", "Exe", R"ns(
extension int
{
    public int Double()
    {
        return this * 2;
    }
}

class Program
{
    public static int Main()
    {
        return 21.Double();
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T08ExtensionGlobal.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-388 a global-namespace extension must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-388 the global-namespace extension must dispatch (exit 42): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerStaticExtensionMemberCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-389", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "extension", "static", "type-qualified"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T08ExtensionStatic", "Exe", R"ns(
struct Box
{
}

extension Box
{
    public static int Twice()
    {
        return 7;
    }
}

class Program
{
    public static int Main()
    {
        return Box.Twice();
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T08ExtensionStatic.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-389 a static extension member via T.Miembro() must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-389 the static extension must dispatch through the type (exit 7): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerIteratorErrorSurfaceCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-390", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "generator", "iterator", "error-surface"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T06IteratorErrorSurface", "Exe", R"ns(
class Program
{
    public static Iterator<int, int> Generate()
    {
        yield return 1;
        yield return 2;
    }

    public static int Main()
    {
        Iterator<int, int> it = Generate();
        int sum = 0;
        while (it.MoveNext()) { sum += it.Current; }
        return sum;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T06IteratorErrorSurface.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-390 Iterator<T,E> must resolve and yield: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 3,
                "TL-LANG-390 Iterator<T,E> must yield 1 then 2 (sum=3): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerForeachTeardownCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-391", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "generator", "foreach", "teardown", "drop"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T06ForeachTeardown", "Exe", R"ns(
struct Resource
{
    public static int Dropped;
    ~Resource() { Resource.Dropped = 1; }
}

class Program
{
    public static Iterator<int> Generate()
    {
        Resource r = new Resource();
        yield return 1;
    }

    public static int Main()
    {
        foreach (int x in Generate())
        {
            break;
        }
        return Resource.Dropped;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T06ForeachTeardown.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-391 the abandoned foreach must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 1,
                "TL-LANG-391 abandoning the foreach must drop the generator frame (exit 1): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerRefPropertyCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-392", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "property", "ref", "getter"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T08RefProperty", "Exe", R"ns(
struct Box
{
    public int Value;
    public ref int ValueRef
    {
        get { return ref this.Value; }
    }
}

class Program
{
    public static int Main()
    {
        Box b = new Box();
        b.Value = 42;
        int x = b.ValueRef;
        return x;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T08RefProperty.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-392 a ref-returning property getter must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-392 the ref getter must read through the reference (exit 42): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerRefPropertyWriteCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-393", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "property", "ref", "getter", "assignable"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T08RefPropertyWrite", "Exe", R"ns(
struct Box
{
    public int Value;
    public ref int ValueRef
    {
        get { return ref this.Value; }
    }
}

class Program
{
    public static int Main()
    {
        Box b = new Box();
        b.Value = 42;
        b.ValueRef = 7;
        return b.Value;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T08RefPropertyWrite.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-393 assigning through a ref getter must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-393 the assignment must write through the reference (exit 7): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerExtensionOperatorCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-394", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "extension", "operator", "static"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T08ExtensionOperator", "Exe", R"ns(
struct Vector
{
    public int X;
}

extension Vector
{
    static int operator+(Vector a, Vector b)
    {
        return a.X + b.X;
    }
}

class Program
{
    public static int Main()
    {
        Vector a = new Vector();
        Vector b = new Vector();
        a.X = 2;
        b.X = 3;
        int c = a + b;
        return c;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T08ExtensionOperator.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-394 the extension operator must compile and dispatch: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 5,
                "TL-LANG-394 a + b must dispatch to the extension operator (exit 5): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

namespace
{
    constexpr const char* collectionBuilderPrelude = R"ns(
struct MyList
{
    public int Count;
    public static int Log;
    public static MyList CreateBuilder()
    {
        MyList m = new MyList();
        return m;
    }
    public void Add(int x)
    {
        Count = Count + x;
        Log = Log * 10 + x;
    }
    public MyList ToCollection()
    {
        return this;
    }
}
)ns";
}

void registerCollectionBuilderCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-395", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "collection", "builder"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::string source = std::string(collectionBuilderPrelude) + R"ns(
class Program
{
    public static int Main()
    {
        MyList l = [2, 3];
        return l.Count;
    }
}
)ns";
            const std::filesystem::path project = products.writeProject(
                "T09CollectionBuilderMulti", "Exe", source);
            const std::filesystem::path executable =
                products.directory() / "T09CollectionBuilderMulti.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-395 the builder must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 5,
                "TL-LANG-395 [2, 3] must Add(2)+Add(3) (exit 5): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});

    registry.add(TestCase{"TL-LANG-396", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "collection", "builder", "zero", "one", "nesting"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::string source = std::string(collectionBuilderPrelude) + R"ns(
class Program
{
    public static int Main()
    {
        MyList z = [];
        MyList o = [7];
        MyList a = [10, 20];
        MyList b = [30, 40];
        return z.Count + o.Count + a.Count + b.Count;
    }
}
)ns";
            const std::filesystem::path project = products.writeProject(
                "T09CollectionBuilderShapes", "Exe", source);
            const std::filesystem::path executable =
                products.directory() / "T09CollectionBuilderShapes.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-396 zero/one/nesting must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 107,
                "TL-LANG-396 0 + 7 + 30 + 70 = 107 (exit 107): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});

    registry.add(TestCase{"TL-LANG-397", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "collection", "builder", "order", "side-effects"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::string source = std::string(collectionBuilderPrelude) + R"ns(
class Program
{
    public static int Main()
    {
        MyList.Log = 0;
        MyList m = [1, 2, 3];
        return MyList.Log;
    }
}
)ns";
            const std::filesystem::path project = products.writeProject(
                "T09CollectionBuilderOrder", "Exe", source);
            const std::filesystem::path executable =
                products.directory() / "T09CollectionBuilderOrder.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-397 order program must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 123,
                "TL-LANG-397 Add must run 1,2,3 in source order (Log=123): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});

    registry.add(TestCase{"TL-LANG-398", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "collection", "builder", "diagnostic"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::string source = std::string(collectionBuilderPrelude) + R"ns(
class Program
{
    public static int Main()
    {
        MyList l = [1.5];
        return l.Count;
    }
}
)ns";
            const std::filesystem::path project = products.writeProject(
                "T09CollectionBuilderInvalid", "Exe", source);
            const std::filesystem::path executable =
                products.directory() / "T09CollectionBuilderInvalid.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(
                emitted.started && emitted.exitCode != 0 &&
                    emitted.output.find("error") != std::string::npos,
                "TL-LANG-398 a non-int element must be rejected with a diagnostic: " +
                    emitted.error + emitted.output);
        }});
}

void registerIntermodularExtensionOperatorCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-399", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "extension", "operator", "nsmeta", "intermodular"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path producerProject = products.writeProject(
                "T08OpProducer", "Library", R"ns(
public struct Vector
{
    public int X;
}

extension Vector
{
    public static int operator+(Vector a, Vector b)
    {
        return a.X + b.X;
    }
}
)ns");
            const std::filesystem::path consumerProject = products.writeProject(
                "T08OpConsumer", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        Vector a = new Vector();
        Vector b = new Vector();
        a.X = 2;
        b.X = 3;
        int c = a + b;
        return c;
    }
}
)ns", "<ProjectReference Include=\"T08OpProducer.nsproj\"/>");
            const std::filesystem::path executable =
                products.directory() / "T08OpConsumer.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", consumerProject, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-399 the intermodular operator must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 5,
                "TL-LANG-399 a + b must dispatch to the producer's extension operator (exit 5): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerInlineArrayConstGenericCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-401", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "inline-array", "fixed-buffer", "const-generic"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T09InlineArrayConstGeneric", "Exe", R"ns(
struct Fixed<const int N>
{
    public int[N] data;
    public int Count() { return N; }
}

class Program
{
    public static int Main()
    {
        Fixed<3> f = default(Fixed<3>);
        f.data[0] = 2;
        f.data[1] = 3;
        f.data[2] = 4;
        Fixed<2> g = default(Fixed<2>);
        g.data[0] = 10;
        g.data[1] = 20;
        return f.data[0] + f.data[1] + f.data[2] + f.Count() + g.data[0] + g.data[1];
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T09InlineArrayConstGeneric.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-401 const-generic inline array must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-401 N must drive the layout arity and the value (2+3+4+3+10+20=42): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerNullConditionalUserIndexCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-402", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "indexer", "null-conditional", "user-indexer"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T10NullConditionalUserIndex", "Exe", R"ns(
class Box
{
    public int this[int i]
    {
        get { return i * 2; }
    }
}

class Program
{
    public static int Main()
    {
        raii Box b = new Box();
        int value = b?[3];
        Box n = null;
        int none = n?[3];
        return value + none;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T10NullConditionalUserIndex.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-402 the null-conditional user indexer must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 6,
                "TL-LANG-402 b?[3]=6 (non-null) + n?[3]=0 (null) = 6: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerOrphanCoherenceCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-403", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "trait", "impl", "orphan", "coherence"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path producerProject = products.writeProject(
                "T08OrphanProducer", "Library", R"ns(
public class External { }

public trait ExternalTrait
{
    int F();
}
)ns");
            const std::filesystem::path traitOwnedProject = products.writeProject(
                "T08OrphanTraitOwned", "Exe", R"ns(
trait Local
{
    int F();
}

impl Local for External
{
    public int F() { return 5; }
}

class Program
{
    public static int Main()
    {
        return 0;
    }
}
)ns", "<ProjectReference Include=\"T08OrphanProducer.nsproj\"/>");
            const std::filesystem::path traitOwnedExecutable =
                products.directory() / "T08OrphanTraitOwned.exe";
            const external_program_support::Result traitOwned = emitT01Product(
                L"--emit-project-oop-exe", traitOwnedProject, traitOwnedExecutable,
                products.directory());
            context.expect(traitOwned.started && traitOwned.exitCode == 0 &&
                    traitOwned.error.empty(),
                "TL-LANG-403 a trait-owned impl over an external type must compile: " +
                    traitOwned.error + traitOwned.output);

            const std::filesystem::path orphanProject = products.writeProject(
                "T08OrphanRejected", "Exe", R"ns(
impl ExternalTrait for External
{
    public int F() { return 5; }
}

class Program
{
    public static int Main()
    {
        return 0;
    }
}
)ns", "<ProjectReference Include=\"T08OrphanProducer.nsproj\"/>");
            const std::filesystem::path orphanExecutable =
                products.directory() / "T08OrphanRejected.exe";
            const external_program_support::Result orphan = emitT01Product(
                L"--emit-project-oop-exe", orphanProject, orphanExecutable,
                products.directory());
            context.expect(orphan.started && orphan.exitCode != 0 &&
                    orphan.output.find("orphan impl") != std::string::npos,
                "TL-LANG-403 an impl owning neither type nor trait must be rejected as orphan: " +
                    orphan.error + orphan.output);
        }});

    registry.add(TestCase{"TL-LANG-404", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "trait", "impl", "conformance", "intermodular", "dispatch"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path producerProject = products.writeProject(
                "T08DispatchProducer", "Library", R"ns(
public class External { }
)ns");
            const std::filesystem::path consumerProject = products.writeProject(
                "T08DispatchConsumer", "Exe", R"ns(
trait Local
{
    int F();
}

impl Local for External
{
    public int F() { return 5; }
}

class Program
{
    public static int Main()
    {
        raii External e = new External();
        return e.F();
    }
}
)ns", "<ProjectReference Include=\"T08DispatchProducer.nsproj\"/>");
            const std::filesystem::path executable =
                products.directory() / "T08DispatchConsumer.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", consumerProject, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-404 the intermodular conformance dispatch must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 5,
                "TL-LANG-404 e.F() must dispatch to the impl over the external type (exit 5): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerExtensionVisibilityCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-405", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "extension", "visibility", "accessibility", "intermodular"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path producerProject = products.writeProject(
                "T08VisibilityProducer", "Library", R"ns(
public struct Box { }
public extension Box { public int Twice() { return 1; } }
)ns");
            const std::filesystem::path consumerProject = products.writeProject(
                "T08VisibilityConsumer", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        Box b = new Box();
        return b.Twice();
    }
}
)ns", "<ProjectReference Include=\"T08VisibilityProducer.nsproj\"/>");
            const std::filesystem::path executable =
                products.directory() / "T08VisibilityConsumer.exe";
            const external_program_support::Result visible = emitT01Product(
                L"--emit-project-oop-exe", consumerProject, executable, products.directory());
            context.expect(visible.started && visible.exitCode == 0 && visible.error.empty(),
                "TL-LANG-405 a public extension must be visible to the consumer: " +
                    visible.error + visible.output);
            if (visible.started && visible.exitCode == 0)
            {
                const auto result = external_program_support::run(
                    executable, {}, {}, products.directory());
                context.expect(result.started && result.exitCode == 1,
                    "TL-LANG-405 the public extension dispatch must run (exit 1): " +
                        result.error + result.output + " exit=" + std::to_string(result.exitCode));
            }

            const std::filesystem::path hiddenProject = products.writeProject(
                "T08VisibilityHiddenProducer", "Library", R"ns(
public struct HiddenBox { }
internal extension HiddenBox { public int Twice() { return 2; } }
)ns");
            const std::filesystem::path hiddenConsumer = products.writeProject(
                "T08VisibilityHiddenConsumer", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        HiddenBox b = new HiddenBox();
        return b.Twice();
    }
}
)ns", "<ProjectReference Include=\"T08VisibilityHiddenProducer.nsproj\"/>");
            const std::filesystem::path hiddenExecutable =
                products.directory() / "T08VisibilityHiddenConsumer.exe";
            const external_program_support::Result hidden = emitT01Product(
                L"--emit-project-oop-exe", hiddenConsumer, hiddenExecutable,
                products.directory());
            context.expect(hidden.started && hidden.exitCode != 0 &&
                    hidden.output.find("no compatible member") != std::string::npos,
                "TL-LANG-405 an internal extension must not be visible to the consumer: " +
                    hidden.error + hidden.output);
        }});
}

void registerTraitWitnessCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-406", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "trait", "impl", "conformance", "witness", "nsmeta", "intermodular"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path typeProject = products.writeProject(
                "T08WitnessType", "Library", R"ns(
public class External { }
)ns");
            const std::filesystem::path implProject = products.writeProject(
                "T08WitnessImpl", "Library", R"ns(
public trait Local
{
    int F();
}

impl Local for External
{
    public int F() { return 5; }
}
)ns", "<ProjectReference Include=\"T08WitnessType.nsproj\"/>");
            const std::filesystem::path consumerProject = products.writeProject(
                "T08WitnessConsumer", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        raii External e = new External();
        return e.F();
    }
}
)ns", "<ProjectReference Include=\"T08WitnessImpl.nsproj\"/>\n"
        "<ProjectReference Include=\"T08WitnessType.nsproj\"/>");
            const std::filesystem::path executable =
                products.directory() / "T08WitnessConsumer.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", consumerProject, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-406 the NSMETA conformance witness must compile the consumer: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 5,
                "TL-LANG-406 e.F() must dispatch through the NSMETA witness (exit 5): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerConstructorThrowsCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-407", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "constructor", "throws", "error", "typed"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::filesystem::path coreDir = repository / "lib/NativeSharp.Core";
            for (const char* file : {"Object.ns", "Number.ns", "NumericLimits.ns",
                     "NumericParsing.ns", "String.ns", "Exception.ns"})
            {
                std::error_code copyError;
                std::filesystem::copy_file(coreDir / file, products.directory() / file,
                    std::filesystem::copy_options::overwrite_existing, copyError);
                context.expect(!copyError,
                    "TL-LANG-407 must stage NativeSharp.Core/" + std::string(file) + ": " +
                        copyError.message());
                if (copyError) return;
            }
            const std::filesystem::path project = products.writeProject(
                "T09F10CtorThrows", "Exe", R"ns(
using NativeSharp.Core;

class E : Exception { }

class C
{
    public C() throws E { }
}

class Program
{
    public static int Main() throws E
    {
        raii C c = new C();
        return 0;
    }
}
)ns", "<Compile Include=\"Object.ns\"/><Compile Include=\"Number.ns\"/>"
                "<Compile Include=\"NumericLimits.ns\"/><Compile Include=\"NumericParsing.ns\"/>"
                "<Compile Include=\"String.ns\"/><Compile Include=\"Exception.ns\"/>");
            const std::filesystem::path executable =
                products.directory() / "T09F10CtorThrows.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-407 a typed throws constructor must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-407 a typed throws constructor must run (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerSpanViewConversionCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-408", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "span", "view", "conversion", "mutability"},
        [](TestContext& context) {
            TemporaryT01Products products;
            if (!stageT03ListPatternCore(context, products, "TL-LANG-408")) return;
            const std::filesystem::path project = products.writeProject(
                "T11F01SpanView", "Exe", R"ns(
using NativeSharp.Core;
class Program
{
    public static int Main()
    {
        int[] storage = new int[2];
        storage[0] = 3; storage[1] = 4;
        Span<int> mutableView = Spans.Of<int>(storage);
        ReadOnlySpan<int> immutableView = mutableView;
        return immutableView[0] + immutableView[1];
    }
}
)ns", kT03ListPatternCoreItems);
            const std::filesystem::path executable =
                products.directory() / "T11F01SpanView.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-408 a Span view to ReadOnlySpan must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-408 the view must preserve the elements (3+4=7): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerArrayToSpanConversionCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-409", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "array", "span", "view", "conversion", "no-copy"},
        [](TestContext& context) {
            TemporaryT01Products products;
            if (!stageT03ListPatternCore(context, products, "TL-LANG-409")) return;
            const std::filesystem::path project = products.writeProject(
                "T11F02ArraySpan", "Exe", R"ns(
using NativeSharp.Core;
class Program
{
    public static int Main()
    {
        int[] storage = new int[2];
        storage[0] = 3; storage[1] = 4;
        Span<int> mutableView = storage;
        ReadOnlySpan<int> immutableView = storage;
        return mutableView[0] + mutableView[1] + immutableView[0] + immutableView[1];
    }
}
)ns", kT03ListPatternCoreItems);
            const std::filesystem::path executable =
                products.directory() / "T11F02ArraySpan.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-409 an array to Span/ReadOnlySpan view must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 14,
                "TL-LANG-409 the whole-array views must preserve the elements (3+4+3+4=14): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerStringToStringViewCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-410", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "string", "stringview", "view", "conversion", "no-copy"},
        [](TestContext& context) {
            TemporaryT01Products products;
            if (!stageT03ListPatternCore(context, products, "TL-LANG-410")) return;
            const std::filesystem::path project = products.writeProject(
                "T11F02StringView", "Exe", R"ns(
using NativeSharp.Core;
class Program
{
    public static int Main()
    {
        string text = "hello";
        StringView view = text;
        return view.Length();
    }
}
)ns", kT03ListPatternCoreItems);
            const std::filesystem::path executable =
                products.directory() / "T11F02StringView.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-410 a string to StringView view must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 5,
                "TL-LANG-410 the string view must preserve the length (hello=5): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerParamsReadOnlySpanCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-411", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "params", "readonly-span", "no-heap"},
        [](TestContext& context) {
            TemporaryT01Products products;
            if (!stageT03ListPatternCore(context, products, "TL-LANG-411")) return;
            const std::filesystem::path project = products.writeProject(
                "T11F04ParamsSpan", "Exe", R"ns(
using NativeSharp.Core;
class Program
{
    public static int Sum(params ReadOnlySpan<int> values)
    {
        return values[0] + values[1];
    }

    public static int Main()
    {
        return Sum(3, 4);
    }
}
)ns", kT03ListPatternCoreItems);
            const std::filesystem::path executable =
                products.directory() / "T11F04ParamsSpan.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-411 a params ReadOnlySpan<int> call must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-411 Sum(3, 4) over params ReadOnlySpan<int> must return 7: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerGenericExtensionDispatchCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-412", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "extension", "generic", "where", "dispatch"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T08F06GenericExtension", "Exe", R"ns(
trait Special { int V(); }
struct Box<T> { public T Value; }
extension Box<T> where T : Special { public int Twice() { return this.Value.V() * 2; } }
class I : Special { public int V() { return 3; } }
class Program { public static int Main() { Box<I> b = new Box<I>(); b.Value = new I(); return b.Twice(); } }
)ns");
            const std::filesystem::path executable =
                products.directory() / "T08F06GenericExtension.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-412 a generic extension dispatch must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 6,
                "TL-LANG-412 b.Twice() over Box<I> must return 6: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerRefLikeConstraintCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-414", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "generic", "where", "ref-like", "constraint"},
        [](TestContext& context) {
            TemporaryT01Products products;
            if (!stageT03ListPatternCore(context, products, "TL-LANG-414")) return;
            const std::filesystem::path project = products.writeProject(
                "T11F05RefLike", "Exe", R"ns(
using NativeSharp.Core;
struct Wrapper<T> where T : ref-like
{
    public int Read(T view) { return view[0]; }
}
class Program
{
    public static int Main()
    {
        int[] a = new int[] { 5 };
        ReadOnlySpan<int> s = a;
        Wrapper<ReadOnlySpan<int>> w = new Wrapper<ReadOnlySpan<int>>();
        return w.Read(s);
    }
}
)ns", kT03ListPatternCoreItems);
            const std::filesystem::path executable =
                products.directory() / "T11F05RefLike.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-414 a ref-like constrained generic must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 5,
                "TL-LANG-414 ReadOnlySpan<int> satisfies where T : ref-like (view[0] = 5): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerRefLikeTraitWitnessCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-415", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "trait", "impl", "witness", "ref-like", "borrowed", "static-dispatch"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T11F06RefLikeTrait", "Exe", R"ns(
trait Getter { int Get(); }
borrowed struct View { public int Value; }
impl Getter for View { public int Get() { return Value; } }
class Program
{
    public static int Main()
    {
        View v; v.Value = 5;
        return v.Get();
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T11F06RefLikeTrait.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-415 a borrowed struct trait witness must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 5,
                "TL-LANG-415 v.Get() must dispatch statically through the witness (exit 5): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerPartialMethodCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-416", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "partial", "method", "merge", "signature", "body"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T12F02PartialMethod", "Exe", R"ns(
partial class Foo
{
    public partial int M();
    public partial int M() { return 5; }
}
class Program
{
    public static int Main()
    {
        raii Foo f = new Foo();
        return f.M();
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T12F02PartialMethod.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-416 a partial method signature + defining body must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 5,
                "TL-LANG-416 f.M() must dispatch through the merged body (exit 5): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerPartialPropertyCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-417", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "partial", "property", "accessor", "merge"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T12F03PartialProperty", "Exe", R"ns(
partial class Foo
{
    public partial int P { get; }
    public partial int P { get { return 5; } }
}
class Program
{
    public static int Main()
    {
        raii Foo f = new Foo();
        return f.P;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T12F03PartialProperty.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-417 a partial property accessor signature + body must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 5,
                "TL-LANG-417 f.P must dispatch through the merged getter (exit 5): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerPartialConstructorCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-418", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "partial", "constructor", "merge", "body"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T12F04PartialConstructor", "Exe", R"ns(
partial class Foo
{
    public int X;
    public partial Foo();
    public partial Foo() { X = 5; }
}
class Program
{
    public static int Main()
    {
        raii Foo f = new Foo();
        return f.X;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T12F04PartialConstructor.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-418 a partial constructor signature + defining body must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 5,
                "TL-LANG-418 the merged constructor body must initialize X (exit 5): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerPartialEventCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-419", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "partial", "event", "add", "remove", "merge"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T12F05PartialEvent", "Exe", R"ns(
public delegate void Handler(int x);
partial class Foo
{
    public int Result;
    public partial event Handler Changed;
    public partial event Handler Changed
    {
        add { Result += 1; }
        remove { Result += 2; }
    }
}
class Program
{
    public static int Main()
    {
        raii Foo f = new Foo();
        f.Changed += (int x) => { };
        f.Changed -= (int x) => { };
        return f.Result;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T12F05PartialEvent.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-419 a partial event with add/remove must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 3,
                "TL-LANG-419 add(+1) then remove(+2) must dispatch the merged accessors (exit 3): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalRepresentationCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-420", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "builtin", "layout", "128-bit"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F01Decimal", "Exe", R"ns(
struct S { public decimal d; }
class Program
{
    public static int Main()
    {
        return (int)sizeof(S);
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T15F01Decimal.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-420 a decimal builtin field must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 16,
                "TL-LANG-420 decimal must be a 16-byte (128-bit) value (sizeof=16): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalLiteralCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-421", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "literal", "suffix", "m"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F02DecimalLiteral", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        decimal d = 1.5m;
        return 0;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T15F02DecimalLiteral.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-421 a decimal literal (1.5m) must type as decimal and compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-421 decimal literal must lower to a 128-bit constant (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalManglingCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-422", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "mangling", "overload", "abi"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F03DecimalMangling", "Exe", R"ns(
class Program
{
    public static decimal M(decimal d) { return d; }
    public static long M(long l) { return l; }
    public static int Main()
    {
        decimal d = 1.5m;
        long l = 3;
        decimal r = M(d);
        long s = M(l);
        return 0;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T15F03DecimalMangling.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-422 a decimal and a long overload must mangle distinctly: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-422 the decimal overload must dispatch (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalArithmeticCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-423", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "arithmetic", "add", "sub", "compare"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F04DecimalArithmetic", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        decimal a = 1.5m;
        decimal b = 2.5m;
        decimal c = a + b;
        if (c - a == b) return 0;
        return 1;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T15F04DecimalArithmetic.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-423 decimal add/sub must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-423 (a+b)-a == b must hold for equal-scale decimals (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalMultiplyCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-424", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "arithmetic", "mul", "commutativity"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F04DecimalMultiply", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        decimal a = 1.5m;
        decimal b = 2.5m;
        decimal c = a + b;
        decimal p = a * b;
        decimal q = b * a;
        if (c - a == b && p == q) return 0;
        return 1;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T15F04DecimalMultiply.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-424 decimal multiply must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-424 a*b == b*a must hold (scale-add multiply) (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalToIntCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-425", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "conversion", "to-int", "truncate"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F06DecimalToInt", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        decimal d = 1.5m;
        int i = (int)d;
        if (i == 1) return 0;
        return 1;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T15F06DecimalToInt.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-425 decimal->int cast must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-425 (int)1.5m must truncate to 1 (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalDivideCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-426", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "arithmetic", "div", "scale-align"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F04DecimalDivide", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        decimal a = 1.5m;
        decimal b = 2.5m;
        decimal c = a / b;
        if ((int)(c * 10m) == 6) return 0;
        return 1;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T15F04DecimalDivide.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-426 decimal divide must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-426 1.5m/2.5m must be 0.6 (scale-aligned coefficient) (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalDivideMixedScaleCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-478", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "arithmetic", "div", "mixed-scale"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F04DecimalDivideMixedScale", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        decimal a = 1.5m / 2m;      // 0.75 (scale 2)
        decimal b = 1m / 2.5m;      // 0.4  (scale 1)
        if ((int)(a * 100m) == 75 && (int)(b * 10m) == 4) return 0;
        return 1;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T15F04DecimalDivideMixedScale.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-478 decimal mixed-scale divide must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-478 1.5m/2m and 1m/2.5m must divide with mixed scales (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalConversionsCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-479", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "conversion", "cast", "overload"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F06DecimalConversions", "Exe", R"ns(
class P
{
    public static int M(long v) { return 1; }
    public static int M(decimal v) { return 2; }
}
class Program
{
    public static int Main()
    {
        decimal a = 42;
        decimal b = (decimal)(-7);
        decimal c = 255L;
        double x = (double)1.5m;
        decimal e = (decimal)1.5;
        if ((int)a != 42) return 1;
        if ((int)b != -7) return 2;
        if ((int)c != 255) return 3;
        if ((int)(e * 10m) != 15) return 4;
        if (P.M(42) != 1) return 5;   // int -> long beats int -> decimal
        return 0;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T15F06DecimalConversions.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-479 decimal conversions must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-479 decimal casts and overload ranking must hold (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalNaNRejectionCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-480", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "conversion", "nan", "infinity"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F06DecimalNaN", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        double nan = 0.0 / 0.0;
        decimal e = (decimal)nan;
        return 0;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T15F06DecimalNaN.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-480 NaN->decimal must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.exitCode == 145,
                "TL-LANG-480 NaN->decimal must trap DecimalOverflow (exit 145): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalOverflowPolicyCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-481", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "overflow", "saturating"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F05DecimalSaturating", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        decimal pos = saturating(1e15m * 1e15m);
        decimal neg = saturating((-1e15m) * 1e15m);
        if (pos <= 0m) return 1;
        if (neg >= 0m) return 2;
        return 0;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T15F05DecimalSaturating.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-481 saturating decimal must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-481 saturating decimal must clamp to Max/Min (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalCheckedTrapCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-482", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "overflow", "checked"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F05DecimalChecked", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        decimal d = checked(1e15m * 1e15m);
        return 0;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T15F05DecimalChecked.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-482 checked decimal must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.exitCode == 145,
                "TL-LANG-482 checked decimal overflow must trap (exit 145): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalUncheckedRejectCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-483", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "overflow", "unchecked", "wrapping"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F05DecimalUnchecked", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        decimal d = unchecked(1e15m * 1e15m);
        return 0;
    }
}
)ns");
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project,
                products.directory() / "T15F05DecimalUnchecked.exe", products.directory());
            context.expect(emitted.started && emitted.exitCode != 0 &&
                    (emitted.error.find("is not allowed for decimal arithmetic") != std::string::npos ||
                     emitted.output.find("is not allowed for decimal arithmetic") != std::string::npos),
                "TL-LANG-483 unchecked/wrapping decimal must be rejected: " +
                    emitted.error + emitted.output);
        }});
}

void registerScopeStackCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-484", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "scope", "stack", "drop", "escape"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T13F03ScopeStack", "Exe", R"ns(
class Enemy { public int X; public static int Drops; ~Enemy() { Drops += 1; } public Enemy(int x) { X = x; } }
class Program
{
    public static int Main()
    {
        { scope Enemy temporary = new Enemy(42); if (temporary.X != 42) return 2; }
        return Enemy.Drops == 1 ? 0 : 1;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T13F03ScopeStack.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-484 scope stack must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-484 scope object must construct and drop once (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerScopeEscapeRejectCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-485", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "scope", "stack", "escape", "return"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T13F03ScopeEscape", "Exe", R"ns(
class Enemy { public int X; public Enemy(int x) { X = x; } }
class Program
{
    public static Enemy Make() { scope Enemy temporary = new Enemy(42); return temporary; }
    public static int Main() { return 0; }
}
)ns");
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project,
                products.directory() / "T13F03ScopeEscape.exe", products.directory());
            context.expect(emitted.started && emitted.exitCode != 0 &&
                    (emitted.error.find("cannot be returned") != std::string::npos ||
                     emitted.output.find("cannot be returned") != std::string::npos),
                "TL-LANG-485 scope return must be rejected: " + emitted.error + emitted.output);
        }});
}

void registerScopeFieldStoreRejectCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-486", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "scope", "stack", "escape", "field"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T13F03ScopeFieldStore", "Exe", R"ns(
class Enemy { public int X; public Enemy(int x) { X = x; } }
class Holder { public Enemy e; }
class Program
{
    public static int Main()
    {
        raii Holder h = new Holder();
        { scope Enemy temporary = new Enemy(42); h.e = temporary; }
        return 0;
    }
}
)ns");
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project,
                products.directory() / "T13F03ScopeFieldStore.exe", products.directory());
            context.expect(emitted.started && emitted.exitCode != 0 &&
                    (emitted.error.find("cannot be stored in a field") != std::string::npos ||
                     emitted.output.find("cannot be stored in a field") != std::string::npos),
                "TL-LANG-486 scope field store must be rejected: " + emitted.error + emitted.output);
        }});
}

void registerArenaAllocatorIAllocatorCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-487", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "allocator", "IAllocator", "arena", "resize", "deallocate"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T13F04ArenaAllocator", "Exe", R"ns(
using NativeSharp.Core;
class A { public int X; public A(int x) { X = x; } }
class Program
{
    public static int Main()
    {
        raii scope ArenaAllocator arena = new ArenaAllocator();
        A a = new (arena) A(42);
        if (a.X != 42) return 1;
        byte* p = arena.Allocate(8);
        Span<byte> s = Spans.OfRaw<byte>(p, 8);
        s[0] = 1; s[1] = 2; s[7] = 8;
        byte* q = arena.Resize(p, 8, 64, 8);
        Span<byte> t = Spans.OfRaw<byte>(q, 64);
        if (t[0] != 1 || t[1] != 2 || t[7] != 8) return 2;
        arena.Deallocate(q, 64, 8);
        return arena.ActiveAllocationCount() == 1 ? 0 : 3;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T13F04ArenaAllocator.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-487 ArenaAllocator as IAllocator must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-487 ArenaAllocator Resize/Deallocate must hold (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerTrailingStorageCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-488", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "trailing", "storage", "flexible", "bounds"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T13F06TrailingStorage", "Exe", R"ns(
using NativeSharp.Core;
struct Packet
{
    public int Length;
    public int Capacity;
    #[TrailingStorage(nameof(Length), nameof(Capacity))]
    public byte[*] Data;
    public Packet(int length, int capacity) { Length = length; Capacity = capacity; }
}
class BlockAllocator : IAllocator
{
    private owned byte[] pool;
    public BlockAllocator(int size) { pool = new byte[size]; }
    public byte* Allocate(long bytes, int alignment) { return Spans.Of<byte>(pool).Data(); }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return ptr; }
    public void Deallocate(byte* ptr, long bytes, int alignment) { }
}
class Program
{
    public static int Main()
    {
        raii BlockAllocator alloc = new BlockAllocator(1024);
        Packet p = new (alloc, trailing: 8) Packet(4, 8);
        if (p.Length != 4) return 1;
        if (p.Capacity != 8) return 2;
        p.Data[0] = 8;
        if (p.Data[0] != 8) return 3;
        p.Data[3] = 7;
        if (p.Data[3] != 7) return 4;
        return 0;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T13F06TrailingStorage.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-488 trailing storage must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-488 trailing storage header + Data + extent must hold (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerTrailingStorageBoundsTrapCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-489", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "trailing", "storage", "flexible", "bounds", "trap"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T13F06TrailingBounds", "Exe", R"ns(
using NativeSharp.Core;
struct Packet
{
    public int Length;
    public int Capacity;
    #[TrailingStorage(nameof(Length), nameof(Capacity))]
    public byte[*] Data;
    public Packet(int length, int capacity) { Length = length; Capacity = capacity; }
}
class BlockAllocator : IAllocator
{
    private owned byte[] pool;
    public BlockAllocator(int size) { pool = new byte[size]; }
    public byte* Allocate(long bytes, int alignment) { return Spans.Of<byte>(pool).Data(); }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return ptr; }
    public void Deallocate(byte* ptr, long bytes, int alignment) { }
}
class Program
{
    public static int Main()
    {
        raii BlockAllocator alloc = new BlockAllocator(1024);
        Packet p = new (alloc, trailing: 8) Packet(4, 8);
        p.Data[4] = 7;
        return 0;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T13F06TrailingBounds.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-489 trailing bounds must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.exitCode == 132,
                "TL-LANG-489 Data[Length] must trap (exit 132 BoundsViolation): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerTrailingStorageLengthNotCapacityCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-490", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "trailing", "storage", "flexible", "bounds", "length"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T13F06TrailingLengthNotCapacity", "Exe", R"ns(
using NativeSharp.Core;
struct Packet
{
    public int Length;
    public int Capacity;
    #[TrailingStorage(nameof(Length), nameof(Capacity))]
    public byte[*] Data;
    public Packet(int length, int capacity) { Length = length; Capacity = capacity; }
}
class BlockAllocator : IAllocator
{
    private owned byte[] pool;
    public BlockAllocator(int size) { pool = new byte[size]; }
    public byte* Allocate(long bytes, int alignment) { return Spans.Of<byte>(pool).Data(); }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return ptr; }
    public void Deallocate(byte* ptr, long bytes, int alignment) { }
}
class Program
{
    public static int Main()
    {
        raii BlockAllocator alloc = new BlockAllocator(1024);
        Packet p = new (alloc, trailing: 8) Packet(2, 8);
        p.Data[1] = 3;
        if (p.Data[1] != 3) return 9;
        p.Data[5] = 1;
        return 0;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T13F06TrailingLengthNotCapacity.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-490 trailing length-vs-capacity must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.exitCode == 132,
                "TL-LANG-490 Data[index>=Length] must trap even when index<Capacity (exit 132): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerPlacementRollbackCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-491", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "placement", "rollback", "throws", "constructor", "drop", "subobject"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T13F05PlacementRollback", "Exe", R"ns(
using NativeSharp.Core;
struct Field { public static int Drops; ~Field() { Drops = Drops + 1; } }
struct S
{
    public owned Field a;
    public S(int fail) throws Exception { a = new Field(); if (fail > 0) throw new Exception(); }
}
class BlockAllocator : IAllocator
{
    private owned byte[] pool;
    public BlockAllocator(int size) { pool = new byte[size]; }
    public byte* Allocate(long bytes, int alignment) { return Spans.Of<byte>(pool).Data(); }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return ptr; }
    public void Deallocate(byte* ptr, long bytes, int alignment) { }
}
class Program
{
    public static int Main()
    {
        try { S s1 = new S(1); } catch { }
        raii BlockAllocator alloc = new BlockAllocator(1024);
        try { S* s2 = new (alloc) S(1); } catch { }
        byte[] buf = new byte[64];
        S* storage = (S*)Spans.Of<byte>(buf).Data();
        try { S* s3 = new (storage) S(1); } catch { }
        return Field.Drops == 3 ? 0 : 1;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T13F05PlacementRollback.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-491 placement rollback must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-491 partial-failure rollback must drop once per object/placement/raw (Drops=3): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAllocatorProvenanceDeallocateCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-492", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "allocator", "provenance", "deallocate", "header", "inheritance", "drop"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T13F07AllocatorProvenance", "Exe", R"ns(
using NativeSharp.Core;
class A { public int X; public static int Drops; ~A() { Drops = Drops + 1; } public A(int x) { X = x; } }
class B : A { public int Y; public B(int x, int y) : base(x) { Y = y; } }
class BlockAllocator : IAllocator
{
    private owned byte[] pool;
    public int Deallocs;
    public BlockAllocator(int size) { pool = new byte[size]; }
    public byte* Allocate(long bytes, int alignment) { return Spans.Of<byte>(pool).Data(); }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return ptr; }
    public void Deallocate(byte* ptr, long bytes, int alignment) { Deallocs = Deallocs + 1; }
}
class Program
{
    public static int Main()
    {
        raii BlockAllocator alloc = new BlockAllocator(4096);
        { owned B b = new (alloc) B(3, 4); }
        { owned A a = new (alloc) A(8); }
        // two objects, two destructor runs (B's base A + A), two Deallocate calls, no double-free.
        return (A.Drops == 2 && alloc.Deallocs == 2) ? 0 : 1;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T13F07AllocatorProvenance.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-492 allocator provenance must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-492 object drop must call the same allocator's Deallocate once per object: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAllocatorProvenanceConstructorFailureCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-493", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "allocator", "provenance", "constructor", "unwind", "deallocate"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T13F07ConstructorFailure", "Exe", R"ns(
using NativeSharp.Core;
class Failing { public Failing() throws Exception { throw new Exception(); } }
class BlockAllocator : IAllocator
{
    private owned byte[] pool;
    public int Deallocs;
    public BlockAllocator(int size) { pool = new byte[size]; }
    public byte* Allocate(long bytes, int alignment) { return Spans.Of<byte>(pool).Data(); }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return ptr; }
    public void Deallocate(byte* ptr, long bytes, int alignment) { Deallocs = Deallocs + 1; }
}
class Program
{
    public static int Main()
    {
        raii BlockAllocator alloc = new BlockAllocator(1024);
        try { owned Failing value = new (alloc) Failing(); } catch { }
        return alloc.Deallocs == 1 ? 0 : 1;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T13F07ConstructorFailure.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-493 allocator constructor-failure cleanup must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-493 a throwing constructor must deallocate through the same provenance: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAllocatorProvenanceCrossModuleCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-494", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "allocator", "provenance", "cross-module", "owned", "return", "drop"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path producerProject = products.writeProject(
                "T13F07Producer", "SharedLibrary", R"ns(
using NativeSharp.Core;
public class Product
{
    public int Value;
    public Product(int value) { Value = value; }
}
public class BlockAllocator : IAllocator
{
    private owned byte[] pool;
    public int Deallocs;
    public BlockAllocator(int size) { pool = new byte[size]; }
    public byte* Allocate(long bytes, int alignment) { return Spans.Of<byte>(pool).Data(); }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return ptr; }
    public void Deallocate(byte* ptr, long bytes, int alignment) { Deallocs = Deallocs + 1; }
}
public class ProductFactory
{
    public static owned Product Make(BlockAllocator alloc, int value)
    {
        owned Product product = new (alloc) Product(value);
        return move(product);
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path producerRuntime =
                products.directory() / "T13F07Producer.dll";
            const external_program_support::Result producer = emitT01Product(
                L"--emit-project-oop-dll", producerProject, producerRuntime,
                products.directory());
            context.expect(producer.started && producer.exitCode == 0 && producer.error.empty(),
                "TL-LANG-494 allocator-provenance DLL producer must compile: " +
                    producer.error + producer.output);
            if (!producer.started || producer.exitCode != 0) return;

            const std::filesystem::path consumer = products.writeProject(
                "T13F07Consumer", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        raii BlockAllocator alloc = new BlockAllocator(1024);
        {
            owned Product product = ProductFactory.Make(alloc, 42);
            if (product.Value != 42) return 2;
        }
        return alloc.Deallocs == 1 ? 0 : 1;
    }
}
)ns", "<LibraryReference Include=\"T13F07Producer.lib\" "
                "Runtime=\"T13F07Producer.dll\"/>"
                "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T13F07Consumer.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", consumer, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-494 DLL consumer of allocator provenance must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-494 an owned return must retain the loaded DLL thunk and caller allocator: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAllocationFailureNullCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-495", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "allocator", "failure", "null", "construction"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T13F08FailureNull", "Exe", R"ns(
using NativeSharp.Core;
class A { public static int Ctors; public int X; public A(int x) { X = x; Ctors = Ctors + 1; } }
class BlockAllocator : IAllocator
{
    private owned byte[] pool;
    public BlockAllocator(int size) { pool = new byte[size]; }
    public byte* Allocate(long bytes, int alignment) { return Spans.Of<byte>(pool).Data(); }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return ptr; }
    public void Deallocate(byte* ptr, long bytes, int alignment) { }
}
class NullAllocator : IAllocator
{
    public byte* Allocate(long bytes, int alignment) { return null; }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return ptr; }
    public void Deallocate(byte* ptr, long bytes, int alignment) { }
}
class Program
{
    public static int Main()
    {
        raii BlockAllocator ba = new BlockAllocator(1024);
        A ok = new (ba, failure: null) A(7);
        raii NullAllocator na = new NullAllocator();
        A fail = new (na, failure: null) A(9);
        return (ok != null && ok.X == 7 && fail == null && A.Ctors == 1) ? 0 : 1;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T13F08FailureNull.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-495 failure:null must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-495 failure:null must yield null and skip the ctor when Allocate is null: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAllocationFailureThrowsCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-496", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "allocator", "failure", "throws", "exception"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T13F08FailureThrows", "Exe", R"ns(
using NativeSharp.Core;
class A { public int X; public A(int x) { X = x; } }
class NullAllocator : IAllocator
{
    public byte* Allocate(long bytes, int alignment) { return null; }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return ptr; }
    public void Deallocate(byte* ptr, long bytes, int alignment) { }
}
class Program
{
    public static int Main()
    {
        raii NullAllocator na = new NullAllocator();
        try { A a = new (na, failure: throws) A(42); }
        catch (AllocationFailureException) { return 0; }
        return 1;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T13F08FailureThrows.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-496 failure:throws must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-496 failure:throws must throw AllocationFailureException on null: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAllocationFailurePanicCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-497", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "allocator", "failure", "panic"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T13F08FailurePanic", "Exe", R"ns(
using NativeSharp.Core;
class A { public int X; public A(int x) { X = x; } }
class NullAllocator : IAllocator
{
    public byte* Allocate(long bytes, int alignment) { return null; }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return ptr; }
    public void Deallocate(byte* ptr, long bytes, int alignment) { }
}
class Program
{
    public static int Main()
    {
        raii NullAllocator na = new NullAllocator();
        A a = new (na, failure: panic) A(42);
        return 0;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T13F08FailurePanic.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-497 failure:panic must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.exitCode == 142,
                "TL-LANG-497 failure:panic must panic (exit 142) when Allocate is null: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAllocationFailureResultCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-498", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "allocator", "failure", "result"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T13F08FailureResult", "Exe", R"ns(
using NativeSharp.Core;
class A { public int X; public A(int x) { X = x; } }
class BlockAllocator : IAllocator
{
    private owned byte[] pool;
    public BlockAllocator(int size) { pool = new byte[size]; }
    public byte* Allocate(long bytes, int alignment) { return Spans.Of<byte>(pool).Data(); }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return ptr; }
    public void Deallocate(byte* ptr, long bytes, int alignment) { }
}
class NullAllocator : IAllocator
{
    public byte* Allocate(long bytes, int alignment) { return null; }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return ptr; }
    public void Deallocate(byte* ptr, long bytes, int alignment) { }
}
class Program
{
    public static int Main()
    {
        raii BlockAllocator ba = new BlockAllocator(1024);
        Result<A, AllocationFailure> ok = new (ba, failure: result) A(7);
        raii NullAllocator na = new NullAllocator();
        Result<A, AllocationFailure> fail = new (na, failure: result) A(9);
        return (ok.IsOk() && fail.IsError()) ? 0 : 1;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T13F08FailureResult.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-498 failure:result must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-498 failure:result must wrap Ok on success and Error on null: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerCostReportCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-499", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "cost", "report", "stack", "arena", "heap"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T13F09CostReport", "Exe", R"ns(
using NativeSharp.Core;
class A { public int X; public A(int x) { X = x; } }
class BlockAllocator : IAllocator
{
    private owned byte[] pool;
    public BlockAllocator(int size) { pool = new byte[size]; }
    public byte* Allocate(long bytes, int alignment) { return Spans.Of<byte>(pool).Data(); }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return ptr; }
    public void Deallocate(byte* ptr, long bytes, int alignment) { }
}
class Program
{
    public static int Main()
    {
        { scope A s = new A(1); }
        raii A h = new A(2);
        raii BlockAllocator ba = new BlockAllocator(1024);
        A a = new (ba) A(3);
        return h.X + a.X;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const external_program_support::Result cost = runT01Compiler(
                {L"--explain-project-costs", project.wstring()}, products.directory());
            context.expect(cost.started && cost.exitCode == 0 && cost.error.empty(),
                "TL-LANG-499 cost report must run: " + cost.error + cost.output);
            if (!cost.started || cost.exitCode != 0) return;
            context.expect(
                cost.output.find("arena-sites=1") != std::string::npos &&
                cost.output.find("heap-sites=") != std::string::npos &&
                cost.output.find("stack-bytes-lower-bound=") != std::string::npos &&
                cost.output.find("cause: arena <- placement-construct") != std::string::npos,
                "TL-LANG-499 cost report must demonstrate stack/arena/heap: " + cost.output);
        }});
}

void registerDecimalRemainderCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-427", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "arithmetic", "rem", "modulo"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F04DecimalRemainder", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        decimal a = 4.5m;
        decimal b = 2.5m;
        decimal c = a % b;
        if ((int)(c * 10m) == 20) return 0;
        return 1;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T15F04DecimalRemainder.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-427 decimal remainder must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-427 4.5m%2.5m must be 2.0 (coefficient remainder) (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalConstCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-428", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "const", "consteval", "128-bit"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F07DecimalConst", "Exe", R"ns(
class Program
{
    public const decimal K = 1.5m;
    public static int Main()
    {
        decimal d = K;
        if ((int)d == 1) return 0;
        return 1;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T15F07DecimalConst.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-428 a const decimal must materialize as a 128-bit constant: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-428 const decimal K=1.5m must keep coefficient/scale ((int)K==1) (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalFfiCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-504", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "ffi", "c-header", "representation"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F03DecimalFfi", "Library", R"ns(
class Program
{
    public static export extern C decimal Add(decimal a, decimal b) { return a + b; }
}
)ns");
            const std::filesystem::path header = products.directory() / "T15F03DecimalFfi.h";
            const external_program_support::Result emitted = runT01Compiler(
                {L"--emit-project-host-header", project.wstring(), L"-o", header.wstring()},
                products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-504 a decimal C header must emit: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            std::ifstream input(header);
            const std::string text((std::istreambuf_iterator<char>(input)),
                std::istreambuf_iterator<char>());
            context.expect(text.find("NS_Decimal") != std::string::npos &&
                    text.find("NS_Decimal NS_CALL Add(NS_Decimal a, NS_Decimal b)") !=
                        std::string::npos,
                "TL-LANG-504 decimal must cross the C ABI as the declared NS_Decimal carrier");
        }});
}

void registerDecimalRoundingCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-505", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "arithmetic", "rounding", "scale", "overflow"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F04DecimalRounding", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        decimal q = 0.5m / 3m;
        return (int)(q * 100m) == 17 ? 0 : 1;
    }
}
)ns");
            const std::filesystem::path executable = products.directory() / "T15F04DecimalRounding.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-505 decimal rounding must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-505 0.5m/3m must round half-up to 0.17 (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));

            const std::filesystem::path overflowProject = products.writeProject(
                "T15F04ScaleOverflow", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        decimal e = 0.0000000000000000000000000001m * 0.1m;
        return 0;
    }
}
)ns");
            const std::filesystem::path overflowExe = products.directory() / "T15F04ScaleOverflow.exe";
            const external_program_support::Result overflowEmitted = emitT01Product(
                L"--emit-project-oop-exe", overflowProject, overflowExe, products.directory());
            context.expect(overflowEmitted.started && overflowEmitted.exitCode == 0,
                "TL-LANG-505 scale overflow must compile: " + overflowEmitted.error + overflowEmitted.output);
            if (!overflowEmitted.started || overflowEmitted.exitCode != 0) return;
            const auto overflow = external_program_support::run(
                overflowExe, {}, {}, products.directory());
            context.expect(overflow.started && overflow.exitCode == 145,
                "TL-LANG-505 scale overflow (scale 29) must trap DecimalOverflow (exit 145): " +
                    overflow.error + overflow.output + " exit=" + std::to_string(overflow.exitCode));
        }});
}

void registerProducerConsumerDecimalCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-508", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "producer", "consumer", "library-reference", "cross-module"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path producerProject = products.writeProject(
                "T16F03DecimalProducer", "Library", R"ns(
namespace Api
{
    public class Calculator
    {
        public static decimal Double(decimal value) { return value + value; }
    }
}
)ns");
            const std::filesystem::path producerLibrary =
                products.directory() / "T16F03DecimalProducer.lib";
            const external_program_support::Result producer = emitT01Product(
                L"--emit-project-oop-lib", producerProject, producerLibrary, products.directory());
            context.expect(producer.started && producer.exitCode == 0 && producer.error.empty(),
                "TL-LANG-508 the decimal producer must build: " + producer.error + producer.output);
            if (!producer.started || producer.exitCode != 0) return;

            std::error_code removalError;
            std::filesystem::remove(producerProject, removalError);
            std::filesystem::remove(products.directory() / "T16F03DecimalProducer.ns", removalError);

            constexpr std::string_view libraryReference =
                "<LibraryReference Include=\"T16F03DecimalProducer.lib\"/>";
            const std::filesystem::path consumerProject = products.writeProject(
                "T16F03DecimalConsumer", "Exe", R"ns(
using Api;
class Program
{
    public static int Main()
    {
        decimal d = Calculator.Double(1.5m);
        return (int)(d * 10m) == 30 ? 0 : 1;
    }
}
)ns", libraryReference);
            const std::filesystem::path consumerExecutable =
                products.directory() / "T16F03DecimalConsumer.exe";
            const external_program_support::Result consumer = emitT01Product(
                L"--emit-project-oop-exe", consumerProject, consumerExecutable,
                products.directory());
            context.expect(consumer.started && consumer.exitCode == 0 && consumer.error.empty(),
                "TL-LANG-508 a source-free consumer must use the imported decimal method: " +
                    consumer.error + consumer.output);
            if (!consumer.started || consumer.exitCode != 0) return;
            const auto result = external_program_support::run(
                consumerExecutable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-508 the imported decimal method must execute cross-module (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerCrossModuleBorrowCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-509", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "ownership", "borrow", "owned", "cross-module", "dll", "drop"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path producerProject = products.writeProject(
                "T16F04BorrowProducer", "SharedLibrary", R"ns(
public class Product { public int Value; public Product(int value) { Value = value; } }
public class ProductFactory
{
    public static owned Product Make(int value) { return new Product(value); }
    public static int Read(in Product product) { return product.Value; }
}
)ns");
            const std::filesystem::path producerRuntime =
                products.directory() / "T16F04BorrowProducer.dll";
            const external_program_support::Result producer = emitT01Product(
                L"--emit-project-oop-dll", producerProject, producerRuntime,
                products.directory());
            context.expect(producer.started && producer.exitCode == 0 && producer.error.empty(),
                "TL-LANG-509 the borrow DLL producer must compile: " +
                    producer.error + producer.output);
            if (!producer.started || producer.exitCode != 0) return;
            const std::filesystem::path consumer = products.writeProject(
                "T16F04BorrowConsumer", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        owned Product product = ProductFactory.Make(42);
        return ProductFactory.Read(in product) == 42 ? 0 : 1;
    }
}
)ns", "<LibraryReference Include=\"T16F04BorrowProducer.lib\" "
                "Runtime=\"T16F04BorrowProducer.dll\"/>");
            const std::filesystem::path executable =
                products.directory() / "T16F04BorrowConsumer.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", consumer, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-509 the borrow DLL consumer must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-509 an owned return must drop and an in borrow must read cross-module (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalDebugCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-510", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "debug", "codeview", "pdb"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T16F05DecimalDebug", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        decimal d = 1.5m;
        return (int)(d * 10m) == 15 ? 0 : 1;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T16F05DecimalDebug.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-510 the decimal debug build must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const std::filesystem::path pdb = products.directory() / "T16F05DecimalDebug.pdb";
            context.expect(std::filesystem::is_regular_file(pdb) &&
                    std::filesystem::file_size(pdb) > 0U,
                "TL-LANG-510 the decimal debug build must retain a non-empty PDB");
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-510 the decimal debug build must run to 0: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalTokenParityCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-511", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "token", "lexer", "parity", "selfhost"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path source = products.directory() / "T16F06TokenParity.ns";
            {
                std::ofstream output(source);
                output << "class Program { public static int Main() { decimal d = 1.5m; return 0; } }\n";
            }
            const external_program_support::Result tokens = runT01Compiler(
                {L"--dump-tokens", source.wstring()}, products.directory());
            context.expect(tokens.started &&
                    tokens.output.find("FloatingLiteral \"1.5m\"") != std::string::npos,
                "TL-LANG-511 the host lexer must keep the m suffix in the decimal lexeme (selfhost parity oracle)");
        }});
}

void registerCSharp14MatrixCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-512", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "csharp14", "extension", "fixture", "conformance", "matrix"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T17F01CSharp14Extension", "Exe", R"ns(
extension string { public int WordCount() { return 1; } }
class Program
{
    public static int Main()
    {
        string value = "hello";
        return value.WordCount() == 1 ? 0 : 1;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T17F01CSharp14Extension.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-512 the NativeSharp extension-block fixture must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-512 the C# 14 extension capability is linked to a productive NativeSharp fixture (exit 0)");
        }});
}

void registerBeefVariantCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-513", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "beef", "variant", "tag", "payload", "fixture", "conformance"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T17F02BeefVariant", "Exe", R"ns(
public variant Value { Num(int), Text(int) }
class Program
{
    public static int Main()
    {
        Value v = Value.Num(42);
        return match (v) { Num(n) => n, Text(t) => t };
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T17F02BeefVariant.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-513 the Beef variant fixture must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-513 the Beef non-allocating variant maps to a productive NativeSharp tagged variant (exit 42)");
        }});
}

void registerAsyncOwnershipErrorCancelCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-514", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "ownership", "errors", "cancellation", "finally"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T17F03AsyncCancel", "Exe", R"ns(
class Program
{
    public static int Log;
    public static AsyncIterator<int, int> Gen()
    {
        try { yield return 1; }
        finally { Log += 1; }
    }
    public static int Main()
    {
        AsyncIterator<int, int> it = Gen();
        bool h1 = await it.MoveNextAsync();
        it.Cancel();
        bool h2 = await it.MoveNextAsync();
        return !h2 && Log == 1 ? 7 : 1;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T17F03AsyncCancel.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-514 the async+ownership+error+cancellation fixture must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-514 cancellation must run the owned finally cleanup once and return false (exit 7)");
        }});
}

void registerPatternsGenericsMoveOnlySliceCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-515", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "patterns", "generics", "move-only", "borrowed", "slice"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T17F04PatternsGenerics", "Exe", R"ns(
using NativeSharp.Core;
public class Cell { public int Value; public Cell(int v) { Value = v; } }
class Program
{
    public static T First<T>(ReadOnlySpan<T> values) { return values[0]; }
    public static Cell Make(int v) { owned Cell c = new Cell(v); return move(c); }
    public static int Main()
    {
        int[] values = new int[] { 42, 1 };
        int first = First<int>(values);
        Cell moved = Make(first);
        return match (moved.Value) { 42 => 42, _ => 0 };
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T17F04PatternsGenerics.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-515 the patterns+generics+move-only+slice fixture must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-515 generics + borrowed slice + move-only + pattern must combine (exit 42)");
        }});
}

void registerExtensionsPartialComptimeCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-516", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "extension", "partial", "comptime", "metadata"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T17F05ExtensionsPartial", "Exe", R"ns(
partial class Foo
{
    public partial int M();
    public partial int M() { return #run(21); }
}
extension Foo { public int Twice() { return this.M() * 2; } }
class Program
{
    public static int Main()
    {
        raii Foo f = new Foo();
        return f.Twice();
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T17F05ExtensionsPartial.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-516 the extensions+partial+comptime fixture must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-516 extension (this.M) + partial method + comptime #run must combine (exit 42)");
        }});
}

void registerAllocatorsGeneratorsContainersNoheapCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-517", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "allocator", "generator", "container", "noheap", "stackalloc"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T17F06AllocatorsGenerators", "Exe", R"ns(
using NativeSharp.Core;
class A { public int X; public A(int x) { X = x; } }
class BlockAllocator : IAllocator
{
    private owned byte[] pool;
    public BlockAllocator(int size) { pool = new byte[size]; }
    public byte* Allocate(long bytes, int alignment) { return Spans.Of<byte>(pool).Data(); }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return ptr; }
    public void Deallocate(byte* ptr, long bytes, int alignment) { }
}
class Program
{
    public static AsyncIterator<int, int> Gen(int n)
    {
        yield return n;
    }
    public static int Main()
    {
        raii BlockAllocator alloc = new BlockAllocator(1024);
        A a = new (alloc) A(42);
        int* stack = stackalloc int[2];
        stack[0] = a.X;
        int[] container = new int[] { stack[0] };
        AsyncIterator<int, int> it = Gen(container[0]);
        bool h = await it.MoveNextAsync();
        return h ? 42 : 1;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T17F06AllocatorsGenerators.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-517 the allocators+generators+containers+noheap fixture must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-517 allocator + stackalloc + container + generator must combine (exit 42)");
        }});
}

void registerHiddenCostAuditCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-518", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "cost", "audit", "mir", "stackalloc", "hidden"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T17F07HiddenCost", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        int* stack = stackalloc int[2];
        stack[0] = 7;
        return stack[0];
    }
}
)ns");
            const external_program_support::Result cost = runT01Compiler(
                {L"--explain-project-costs", project.wstring()}, products.directory());
            context.expect(cost.started && cost.exitCode == 0 && cost.error.empty(),
                "TL-LANG-518 the MIR cost audit must run: " + cost.error + cost.output);
            if (!cost.started || cost.exitCode != 0) return;
            context.expect(
                cost.output.find("cause: stack <- stack-allocation") != std::string::npos &&
                    cost.output.find("cause: heap <- allocate-and-construct") == std::string::npos,
                "TL-LANG-518 a stack-only Main must audit its stack cost with no hidden heap construct: " +
                    cost.output);
        }});
}

void registerTypedDeriveFixtureCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-519", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "derive", "record", "equality", "hash", "print", "clone", "no-reflection"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T14F08TypedDerive", "Exe", R"ns(
public record struct Pair derive(Equality, Hash, Print, Clone)
{
    public int X;
    public int Y;
}
class Program
{
    public static int Main()
    {
        Pair left = { 20, 22 };
        Pair right = left.Clone();
        if (left != right) return 1;
        if (!left.Equals(right)) return 2;
        if (left.GetHashCode() != right.GetHashCode()) return 3;
        if (left.ToString() != "Pair") return 4;
        return 42;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T14F08TypedDerive.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-519 the typed derive fixture must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-519 derived Equality/Hash/Print/Clone members must work without reflection (exit 42)");
        }});
}

void registerAsyncOwnedFinallyConditionalYieldCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-520", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "owned", "finally", "conditional-yield", "cleanup", "regression"},
        [](TestContext& context) {
            TemporaryT01Products products;

            constexpr std::string_view logAndResource = R"ns(
public class Log { public static int FinallyCount; public static int DropCount; }
public class Resource { ~Resource() { Log.DropCount += 1; } }
)ns";
            constexpr std::string_view generator = R"ns(
    public static AsyncIterator<int, int> Gen(int n)
    {
        owned Resource res = new Resource();
        try { yield return n; if (n < 0) yield return -1; }
        finally { Log.FinallyCount += 1; }
    }
)ns";

            const auto runScenario = [&](std::string_view name, std::string_view body,
                                        std::uint32_t expectedExit, std::string_view label)
            {
                const std::string source = std::string(logAndResource) +
                    "class Program\n{\n" + std::string(generator) + std::string(body) + "\n}\n";
                const std::filesystem::path project = products.writeProject(name, "Exe", source);
                const std::filesystem::path executable =
                    products.directory() / (std::string(name) + ".exe");
                const external_program_support::Result emitted = emitT01Product(
                    L"--emit-project-oop-exe", project, executable, products.directory());
                context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                    std::string(label) + " must compile: " + emitted.error + emitted.output);
                if (!emitted.started || emitted.exitCode != 0) return;
                const auto result = external_program_support::run(
                    executable, {}, {}, products.directory());
                context.expect(
                    result.started && result.error.empty() && result.exitCode == expectedExit,
                    std::string(label) + " must exit " + std::to_string(expectedExit) + ": " +
                        result.error + result.output + " exit=" + std::to_string(result.exitCode));
            };

            // D: natural termination, condition false and true -- values, finally once, drop once.
            runScenario("T17F09NaturalFalse", R"ns(
    public static int Main()
    {
        AsyncIterator<int, int> it = Gen(5);
        bool h1 = await it.MoveNextAsync();
        bool h2 = await it.MoveNextAsync();
        return (h1 && !h2 && Log.FinallyCount == 1 && Log.DropCount == 1) ? 0 : 1;
    }
)ns", 0, "TL-LANG-520 natural termination (condition false)");
            runScenario("T17F09NaturalTrue", R"ns(
    public static int Main()
    {
        AsyncIterator<int, int> it = Gen(-5);
        bool h1 = await it.MoveNextAsync();
        int item1 = it.Current;
        bool h2 = await it.MoveNextAsync();
        int item2 = it.Current;
        bool h3 = await it.MoveNextAsync();
        return (h1 && item1 == -5 && h2 && item2 == -1 && !h3 &&
                Log.FinallyCount == 1 && Log.DropCount == 1) ? 0 : 1;
    }
)ns", 0, "TL-LANG-520 natural termination (condition true, values correct)");

            // E: cancellation after the first suspension.
            runScenario("T17F09CancelFirst", R"ns(
    public static int Main()
    {
        AsyncIterator<int, int> it = Gen(-5);
        bool h1 = await it.MoveNextAsync();
        it.Cancel();
        bool h2 = await it.MoveNextAsync();
        return (!h2 && Log.FinallyCount == 1 && Log.DropCount == 1) ? 0 : 1;
    }
)ns", 0, "TL-LANG-520 cancellation after first suspension");
            runScenario("T17F09CancelSecond", R"ns(
    public static int Main()
    {
        AsyncIterator<int, int> it = Gen(-5);
        bool h1 = await it.MoveNextAsync();
        bool h2 = await it.MoveNextAsync();
        it.Cancel();
        bool h3 = await it.MoveNextAsync();
        return (h1 && h2 && !h3 && Log.FinallyCount == 1 && Log.DropCount == 1) ? 0 : 1;
    }
)ns", 0, "TL-LANG-520 cancellation after second suspension");

            // G: a suspension alone must not advance the cleanup counters yet.
            runScenario("T17F09SuspendNoCleanup", R"ns(
    public static int Main()
    {
        AsyncIterator<int, int> it = Gen(5);
        bool h1 = await it.MoveNextAsync();
        int finallyAfterYield = Log.FinallyCount;
        int dropAfterYield = Log.DropCount;
        return (h1 && finallyAfterYield == 0 && dropAfterYield == 0) ? 0 : 1;
    }
)ns", 0, "TL-LANG-520 a suspension must not run cleanup yet");

            // F: exception inside the try after a resume -- finally once, drop once.
            {
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
                const std::filesystem::path exceptionProject = products.writeProject(
                    "T17F09Exception", "Exe", R"ns(
using NativeSharp.Core;
public class Log { public static int FinallyCount; public static int DropCount; }
public class Resource { ~Resource() { Log.DropCount += 1; } }
class Program
{
    public static AsyncIterator<int, Exception> Gen(int n) throws Exception
    {
        owned Resource res = new Resource();
        try { yield return n; if (n < 0) throw new Exception(); }
        finally { Log.FinallyCount += 1; }
    }
    public static int Main() throws Exception
    {
        AsyncIterator<int, Exception> it = Gen(-5);
        bool h1 = await it.MoveNextAsync();
        // Freeze 3.0 C-17: the error reaches the await of MoveNextAsync; before, it was lost.
        bool caught = false;
        try { bool h2 = await it.MoveNextAsync(); } catch (Exception e) { caught = true; }
        return (h1 && caught && Log.FinallyCount == 1 && Log.DropCount == 1) ? 0 : 1;
    }
}
)ns", "<Compile Include=\"Object.ns\"/><Compile Include=\"Number.ns\"/>"
                    "<Compile Include=\"NumericLimits.ns\"/><Compile Include=\"NumericParsing.ns\"/>"
                    "<Compile Include=\"String.ns\"/><Compile Include=\"Exception.ns\"/>");
                const std::filesystem::path exceptionExecutable =
                    products.directory() / "T17F09Exception.exe";
                const external_program_support::Result exceptionEmitted = emitT01Product(
                    L"--emit-project-oop-exe", exceptionProject, exceptionExecutable,
                    products.directory());
                context.expect(exceptionEmitted.started && exceptionEmitted.exitCode == 0 &&
                        exceptionEmitted.error.empty(),
                    "TL-LANG-520 exception after resume must compile: " +
                        exceptionEmitted.error + exceptionEmitted.output);
                if (exceptionEmitted.started && exceptionEmitted.exitCode == 0)
                {
                    const auto result = external_program_support::run(
                        exceptionExecutable, {}, {}, products.directory());
                    context.expect(result.started && result.error.empty() && result.exitCode == 0,
                        "TL-LANG-520 exception after resume must run finally+drop exactly once (exit 0)");
                }
            }
        }});
}

void registerDeriveCollisionCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-521", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "derive", "collision", "metaprogramming", "record"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path collisionProject = products.writeProject(
                "T12F06DeriveCollision", "Exe", R"ns(
public record struct Pair derive(Clone)
{
    public int X;
    public Pair Clone() { return this; }
}
class Program
{
    public static int Main() { return 0; }
}
)ns");
            const external_program_support::Result collision = runT01Compiler(
                {L"--emit-project-oop-exe", collisionProject.wstring(), L"-o",
                 (products.directory() / "T12F06DeriveCollision.exe").wstring()},
                products.directory());
            context.expect(collision.started && collision.exitCode != 0 &&
                    (collision.error + collision.output).find("collides with an existing member") !=
                        std::string::npos,
                "TL-LANG-521 a derive colliding with a hand-written member must be rejected: " +
                    collision.error + collision.output);

            const std::filesystem::path okProject = products.writeProject(
                "T12F06DeriveOk", "Exe", R"ns(
public record struct Pair derive(Clone)
{
    public int X;
    public int Y;
}
class Program
{
    public static int Main()
    {
        Pair p = { 1, 2 };
        Pair q = p.Clone();
        return q.X == 1 && q.Y == 2 ? 0 : 1;
    }
}
)ns");
            const std::filesystem::path okExecutable =
                products.directory() / "T12F06DeriveOk.exe";
            const external_program_support::Result ok = emitT01Product(
                L"--emit-project-oop-exe", okProject, okExecutable, products.directory());
            context.expect(ok.started && ok.exitCode == 0 && ok.error.empty(),
                "TL-LANG-521 a non-colliding derive must compile: " + ok.error + ok.output);
            if (!ok.started || ok.exitCode != 0) return;
            const auto result = external_program_support::run(
                okExecutable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-521 the derived Clone must run (exit 0)");
        }});
}

void registerDecimalOverflowCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-429", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "overflow", "checked", "trap"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F05DecimalOverflow", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        decimal a = 1e15m;
        decimal b = a * a;
        return 0;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T15F05DecimalOverflow.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-429 decimal overflow must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 145,
                "TL-LANG-429 1e15m*1e15m must trap DecimalOverflow (exit 145): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalMixedScaleCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-430", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "arithmetic", "add", "mixed-scale", "alignment"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F04DecimalMixedScale", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        decimal a = 1.5m;
        decimal b = 2m;
        decimal c = a + b;
        if ((int)(c * 10m) == 35) return 0;
        return 1;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T15F04DecimalMixedScale.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-430 mixed-scale decimal add must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-430 1.5m+2m must align to 3.5 (max-scale alignment) (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerDecimalMixedScaleRemCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-431", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "decimal", "arithmetic", "rem", "mixed-scale", "alignment"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T15F04DecimalMixedScaleRem", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        decimal a = 4.5m;
        decimal b = 2m;
        decimal c = a % b;
        if ((int)(c * 10m) == 5) return 0;
        return 1;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T15F04DecimalMixedScaleRem.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-431 mixed-scale decimal rem must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-431 4.5m%2m must align to 0.5 (max-scale alignment) (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAllocatorProtocolCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-432", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "allocator", "trait", "IAllocator", "protocol", "pointer", "pairing"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T13F01AllocatorProtocol", "Exe", R"ns(
using NativeSharp.Core;

class BlockAllocator : IAllocator
{
    private owned byte[] pool;
    private owned bool[] used;
    private int blockSize;
    private int blockCount;

    public BlockAllocator(int size, int count)
    {
        blockSize = (size + 7) / 8 * 8;
        blockCount = count;
        pool = new byte[blockSize * blockCount];
        used = new bool[blockCount];
    }

    public byte* Allocate(long bytes, int alignment)
    {
        if (bytes <= 0 || bytes > blockSize) return null;
        for (int i = 0; i < blockCount; i++)
        {
            if (!used[i]) { used[i] = true; byte* b = Spans.Of<byte>(pool).Data(); return b + i * blockSize; }
        }
        return null;
    }

    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment)
    {
        if (ptr == null || newBytes <= 0) return null;
        if (newBytes <= blockSize) return ptr;
        return null;
    }

    public void Deallocate(byte* ptr, long bytes, int alignment)
    {
        if (ptr == null) return;
        byte* base0 = Spans.Of<byte>(pool).Data();
        for (int i = 0; i < blockCount; i++)
        {
            if (ptr == base0 + i * blockSize) { used[i] = false; return; }
        }
    }
}

class Program
{
    public static int Main()
    {
        raii BlockAllocator a = new BlockAllocator(16, 8);
        if (a.Allocate(0, 8) != null) return 1;
        if (a.Allocate(20, 8) != null) return 2;
        byte* p = a.Allocate(8, 8);
        if (p == null) return 3;
        p[0] = 42;
        p[1] = 7;
        byte* q = a.Resize(p, 8, 12, 8);
        if (q == null) return 4;
        if (q[0] != 42 || q[1] != 7) return 5;
        if (a.Resize(p, 8, 20, 8) != null) return 6;
        a.Deallocate(q, 12, 8);
        byte* r = a.Allocate(8, 8);
        if (r == null) return 7;
        if (r != q) return 8;
        return 0;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T13F01AllocatorProtocol.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-432 IAllocator protocol must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-432 IAllocator allocate/resize/deallocate must pair and reuse (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
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

void registerLambdaAttributeCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-327", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "lambda", "attribute"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T04LambdaAttribute", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        var f = [Attr] (int x) => x * 2;
        return f(21);
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T04LambdaAttribute.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-327 an attributed lambda must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-327 an attributed lambda must dispatch: " +
                    result.error + result.output);
        }});
}

void registerAwaitDiagnosticCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-369", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "await", "diagnostic", "future-surface"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T05AwaitDiagnostic", "Exe", R"ns(
class Program
{
    public static int Other()
    {
        return 1;
    }

    public static async int Read()
    {
        return await Other();
    }
}
)ns");
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project,
                products.directory() / "T05AwaitDiagnostic.exe", products.directory());
            context.expect(emitted.started && emitted.exitCode != 0 &&
                    (emitted.error + emitted.output).find(
                        "await operand must be a Future value") != std::string::npos,
                "TL-LANG-369 await over a non-Future operand must reject: " +
                    emitted.error + emitted.output);
        }});
}

void registerAsyncAwaitRunCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-370", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "await", "constructor", "poll", "run"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T05AsyncAwaitRun", "Exe", R"ns(
class Program
{
    public static async int Inner()
    {
        return 5;
    }

    public static async int Read()
    {
        int value = await Inner();
        return value + 1;
    }

    public static int Main()
    {
        int result = await Read();
        return result;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T05AsyncAwaitRun.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-370 async await must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 6,
                "TL-LANG-370 async await must run through constructor+poll: " +
                    result.error + result.output);
        }});
}

void registerAsyncYieldRunCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-371", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "yield", "suspend", "resume", "run"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T05AsyncYieldRun", "Exe", R"ns(
class Program
{
    public static async int PendingOnce()
    {
        yield;
        return 42;
    }

    public static async int Read()
    {
        int value = await PendingOnce();
        return value + 1;
    }

    public static async int Main()
    {
        int result = await Read();
        return result;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T05AsyncYieldRun.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-371 async yield suspend must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 43,
                "TL-LANG-371 async yield must suspend and resume (pending->ready): " +
                    result.error + result.output);
        }});
}

void registerAsyncDropCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-372", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "drop", "destructor", "cleanup"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T05AsyncDrop", "Exe", R"ns(
class Box
{
    public static int Drops;
    public Box() { }
    ~Box() { Drops = Drops + 1; }
}
class Program
{
    public static async int Read()
    {
        raii Box box = new Box();
        return 42;
    }

    public static int Main()
    {
        int result = await Read();
        return Box.Drops * 100 + result;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T05AsyncDrop.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-372 async drop must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 142,
                "TL-LANG-372 the owned local must drop exactly once on completion (Drops=1, result=42): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncDropAfterSuspensionCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-435", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "drop", "destructor", "cleanup", "suspension", "liveness"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T05AsyncDropAfterSuspension", "Exe", R"ns(
class Box
{
    public static int Drops;
    public Box() { }
    ~Box() { Drops = Drops + 1; }
}
class Program
{
    public static async int Read()
    {
        raii Box box = new Box();
        yield;
        return 42;
    }

    public static async int Main()
    {
        int result = await Read();
        return Box.Drops * 100 + result;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T05AsyncDropAfterSuspension.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-435 async drop-after-suspension must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 142,
                "TL-LANG-435 the owned local must survive the suspension and drop exactly once (Drops=1, result=42): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncLivenessCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-373", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "yield", "liveness", "frame", "local"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T05AsyncLiveness", "Exe", R"ns(
class Program
{
    public static async int Read()
    {
        int value = 10;
        yield;
        return value;
    }

    public static async int Main()
    {
        int result = await Read();
        return result;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T05AsyncLiveness.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-373 async liveness must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 10,
                "TL-LANG-373 a local live across a suspension must survive (value=10): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncPreciseLivenessCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-433", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "yield", "liveness", "frame", "def-use", "local"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T05AsyncPreciseLiveness", "Exe", R"ns(
class Program
{
    public static async int Read()
    {
        int live = 10;
        int dead = 5;
        yield;
        return live;
    }

    public static async int Main()
    {
        return await Read();
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T05AsyncPreciseLiveness.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-433 precise liveness must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 10,
                "TL-LANG-433 a live-across-suspension local must survive while a dead local is omitted (exit 10): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncBorrowRejectCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-374", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "borrow", "suspend", "survive", "frame"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T05AsyncBorrowSurvive", "Exe", R"ns(
class Box { public int Value; public Box() { Value = 10; } }
class Program
{
    public static async int Read()
    {
        raii Box box = new Box();
        borrowed Box view = box;
        yield;
        return view.Value;
    }

    public static async int Main()
    {
        return await Read();
    }
}
)ns");
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project,
                products.directory() / "T05AsyncBorrowSurvive.exe", products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-374 a borrowed local across a suspension must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                products.directory() / "T05AsyncBorrowSurvive.exe", {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 10,
                "TL-LANG-374 a borrowed local across a suspension must survive (view.Value=10): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncBorrowParameterRejectCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-434", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "borrow", "suspend", "parameter", "reject"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T05AsyncBorrowParameterReject", "Exe", R"ns(
class Box { public int Value; public Box() { Value = 10; } }
class Program
{
    public static async int Read(Box box)
    {
        borrowed Box view = box;
        yield;
        return view.Value;
    }

    public static int Main()
    {
        return 42;
    }
}
)ns");
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project,
                products.directory() / "T05AsyncBorrowParameterReject.exe", products.directory());
            context.expect(emitted.started && emitted.exitCode != 0 &&
                    (emitted.error + emitted.output).find(
                        "borrowed local source parameter") != std::string::npos,
                "TL-LANG-434 a borrowed view of a parameter across a suspension must be rejected: " +
                    emitted.error + emitted.output);
        }});
}

void registerAsyncThrowsCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-375", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "throws", "future", "error-type"},
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
                "T05AsyncThrows", "Exe", R"ns(
using NativeSharp.Core;
class Program
{
    public static async int Read() throws Exception
    {
        return 42;
    }

    public static int Main() throws Exception
    {
        int result = await Read();
        return result;
    }
}
)ns", "<Compile Include=\"Object.ns\"/><Compile Include=\"Number.ns\"/>"
                "<Compile Include=\"NumericLimits.ns\"/><Compile Include=\"NumericParsing.ns\"/>"
                "<Compile Include=\"String.ns\"/><Compile Include=\"Exception.ns\"/>");
            const std::filesystem::path executable =
                products.directory() / "T05AsyncThrows.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-375 a throwing async function must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-375 a throwing async future must carry its error type and run: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

// EN: Freeze 3.0 DEC-15: C-17 makes a synchronous `await` outside `try` propagate the future's error
//     as an exception. The case proves that propagation with a stable result: Forward awaits outside
//     any try, Main catches and returns 9. Exit 9 is reachable only through the catch, so neither the
//     value after the await (142) nor a default (100) can pass. An error escaping Main ends the process
//     with the native code 0xE04E5301, and Windows Error Reporting delayed that path intermittently at
//     -O2, so the case does not depend on it.
// ES: Congelamiento 3.0 DEC-15: C-17 hace que un `await` sincrono fuera de `try` propague el error del
//     future como excepcion. El caso prueba esa propagacion con un resultado estable: Forward espera
//     fuera de todo try, Main atrapa y devuelve 9. El 9 solo se alcanza por el catch, asi que ni el
//     valor despues del await (142) ni un valor por defecto (100) pueden pasar. Un error que sale de
//     Main termina el proceso con el codigo nativo 0xE04E5301 y el reporte de errores de Windows demoro
//     esa ruta de forma intermitente en -O2, por eso el caso no depende de ella.
void registerAsyncThrowErrorCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-376", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "throws", "throw", "error", "future"},
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
                "T05AsyncThrowError", "Exe", R"ns(
using NativeSharp.Core;
class Program
{
    public static async int Read() throws Exception
    {
        throw new Exception();
        return 42;
    }

    public static int Forward() throws Exception
    {
        int result = await Read();
        return result + 100;
    }

    public static int Main()
    {
        try
        {
            return Forward();
        }
        catch (Exception e)
        {
            return 9;
        }
    }
}
)ns", "<Compile Include=\"Object.ns\"/><Compile Include=\"Number.ns\"/>"
                "<Compile Include=\"NumericLimits.ns\"/><Compile Include=\"NumericParsing.ns\"/>"
                "<Compile Include=\"String.ns\"/><Compile Include=\"Exception.ns\"/>");
            const std::filesystem::path executable =
                products.directory() / "T05AsyncThrowError.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-376 a throwing async that throws must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 9,
                "TL-LANG-376 a throw in the async body must propagate through a synchronous await outside "
                "try to the caller's catch (C-17): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncRethrowCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-413", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "throws", "throw", "error", "future", "rethrow"},
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
                "T05AsyncRethrow", "Exe", R"ns(
using NativeSharp.Core;
class Program
{
    public static async int Read() throws Exception
    {
        throw new Exception();
        return 42;
    }

    public static int Main()
    {
        try { return await Read(); }
        catch (Exception e) { return 7; }
    }
}
)ns", "<Compile Include=\"Object.ns\"/><Compile Include=\"Number.ns\"/>"
                "<Compile Include=\"NumericLimits.ns\"/><Compile Include=\"NumericParsing.ns\"/>"
                "<Compile Include=\"String.ns\"/><Compile Include=\"Exception.ns\"/>");
            const std::filesystem::path executable =
                products.directory() / "T05AsyncRethrow.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-413 a blocking await inside try must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-413 the future error must rethrow into the catch: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncNestedErrorPropagationCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-436", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "throws", "throw", "error", "future", "ReadyError", "propagate"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T05AsyncNestedError", "Exe", R"ns(
using NativeSharp.Core;
class Program
{
    public static async int Child() throws Exception
    {
        throw new Exception();
        return 42;
    }

    public static async int Parent() throws Exception
    {
        return await Child();
    }

    public static int Main()
    {
        try { return await Parent(); }
        catch (Exception e) { return 7; }
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T05AsyncNestedError.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-436 nested async error propagation must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-436 a child future ReadyError must propagate to the parent and rethrow (exit 7): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncFrameStorageHeapCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-437", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "frame", "storage", "FrameStorage", "heap", "explicit"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T05AsyncFrameStorageHeap", "Exe", R"ns(
class Program
{
    [FrameStorage(Heap)]
    public static async int Inner()
    {
        int x = 20;
        yield;
        return x + 1;
    }

    public static async int Outer()
    {
        return await Inner();
    }

    public static async int Main()
    {
        int a = await Inner();
        int b = await Outer();
        return a + b;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T05AsyncFrameStorageHeap.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-437 [FrameStorage(Heap)] async must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-437 [FrameStorage(Heap)] async must run with an explicit heap frame (blocking + nested await, exit 42): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncIntermodularFutureCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-438", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "future", "pollFn", "intermodular", "abi"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path producerProject = products.writeProject(
                "T05FutureProducer", "Library", R"ns(
public class Async
{
    public static async int Make()
    {
        int x = 5;
        yield;
        return x;
    }
}
)ns");
            const std::filesystem::path consumerProject = products.writeProject(
                "T05FutureConsumer", "Exe", R"ns(
class Program
{
    public static async int Main()
    {
        return await Async.Make();
    }
}
)ns", "<ProjectReference Include=\"T05FutureProducer.nsproj\"/>");
            const std::filesystem::path executable =
                products.directory() / "T05FutureConsumer.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", consumerProject, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-438 the intermodular future must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 5,
                "TL-LANG-438 the opaque future must be consumed across modules via its pollFn (exit 5): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncAwaitVariableCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-377", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "await", "variable", "future", "poll-fn"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T05AsyncAwaitVariable", "Exe", R"ns(
class Program
{
    public static async int Read() { return 5; }

    public static async int Combine()
    {
        var future = Read();
        int value = await future;
        return value + 1;
    }

    public static int Main()
    {
        return await Combine();
    }
}
)ns", "");
            const std::filesystem::path executable =
                products.directory() / "T05AsyncAwaitVariable.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-377 an await over a future variable must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 6,
                "TL-LANG-377 await over a future variable must resolve the poll from pollFn: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncFutureTypeCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-378", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "future", "type", "opaque", "nameable"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T05AsyncFutureType", "Exe", R"ns(
class Program
{
    public static async int Read() { return 5; }

    public static int Main()
    {
        Future<int> future = Read();
        return await future;
    }
}
)ns", "");
            const std::filesystem::path executable =
                products.directory() / "T05AsyncFutureType.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-378 Future<T> must name the opaque future surface: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 5,
                "TL-LANG-378 Future<T> must map to future:<T> and be awaitable: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncMethodMarkerCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-328", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "method", "marker"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T05AsyncMethodMarker", "Exe", R"ns(
class Program
{
    public static async void Pump()
    {
    }

    public static int Main()
    {
        return 42;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T05AsyncMethodMarker.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-328 an async class method must compile without NS3049: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-328 an async method marker must not alter execution: " +
                    result.error + result.output);
        }});
}

void registerExtensionPropertyCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-324", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "extension", "property", "getter"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T08ExtensionProperty", "Exe", R"ns(
namespace Suite
{
    extension int
    {
        public int Doubled
        {
            get { return this * 2; }
        }
    }
}

using Suite;

class Program
{
    public static int Main()
    {
        return 21.Doubled;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T08ExtensionProperty.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-324 an extension property must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-324 an extension property getter must dispatch: " +
                    result.error + result.output);
        }});
}

void registerPositionalPatternCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-322", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "pattern", "positional", "deconstruct"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T03Positional", "Exe", R"ns(
class Program
{
    public struct Point
    {
        public int X;
        public int Y;
        public Point(int x, int y)
        {
            X = x;
            Y = y;
        }
        public (int, int) Deconstruct()
        {
            return (X, Y);
        }
    }

    private static int Score(Point p)
    {
        return match(p)
        {
            Point(3, y) => 3 + y,
            _ => 0
        };
    }

    public static int Main()
    {
        return Score(new Point(3, 4)) + Score(new Point(2, 4));
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T03Positional.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-322 a positional pattern must deconstruct by position: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-322 positional deconstruction must bind elements: " +
                    result.error + result.output);

            const std::filesystem::path unaryProject = products.writeProject(
                "T03UnaryPositional", "Exe", R"ns(
class Program
{
    public struct Scalar
    {
        public int Value;
        public Scalar(int value) { Value = value; }
        public (int,) Deconstruct() { return (Value,); }
    }

    public static int Main()
    {
        Scalar value = new Scalar(42);
        return match(value) { Scalar(item) => item, _ => 0 };
    }
}
)ns");
            const std::filesystem::path unaryExecutable =
                products.directory() / "T03UnaryPositional.exe";
            const external_program_support::Result unaryEmitted = emitT01Product(
                L"--emit-project-oop-exe", unaryProject, unaryExecutable, products.directory());
            context.expect(unaryEmitted.started && unaryEmitted.exitCode == 0 &&
                    unaryEmitted.error.empty(),
                "TL-LANG-322 a unary positional pattern must compile: " +
                    unaryEmitted.error + unaryEmitted.output);
            if (unaryEmitted.started && unaryEmitted.exitCode == 0)
            {
                const auto unaryResult = external_program_support::run(
                    unaryExecutable, {}, {}, products.directory());
                context.expect(unaryResult.started && unaryResult.error.empty() &&
                        unaryResult.exitCode == 42,
                    "TL-LANG-322 a unary positional pattern must bind its element: " +
                        unaryResult.error + unaryResult.output);
            }

            const auto expectRejected = [&](std::string_view projectName,
                                            std::string_view source,
                                            std::string_view diagnostic) {
                const std::filesystem::path rejectedProject = products.writeProject(
                    projectName, "Exe", source);
                const external_program_support::Result rejected = runT01Compiler(
                    {L"--check-project-method-bodies", rejectedProject.wstring()},
                    products.directory());
                context.expect(rejected.started && rejected.exitCode != 0 &&
                        rejected.output.find(diagnostic) != std::string::npos,
                    "TL-LANG-322 invalid positional pattern must fail with '" +
                        std::string(diagnostic) + "': " + rejected.error + rejected.output);
            };

            expectRejected("T03WrongPositionalType", R"ns(
struct Point { public (int, int) Deconstruct() { return (1, 2); } }
struct Other { public (int, int) Deconstruct() { return (1, 2); } }
class Program
{
    public static int Main()
    {
        Point value;
        return match(value) { Other(x, y) => x + y, _ => 0 };
    }
}
)ns", "does not match governing value type");

            expectRejected("T03MissingDeconstruct", R"ns(
struct Plain { public int Value; }
class Program
{
    public static int Main()
    {
        Plain value;
        return match(value) { Plain(x) => x, _ => 0 };
    }
}
)ns", "requires one non-static, non-virtual, parameterless Deconstruct() method");

            expectRejected("T03WrongPositionalArity", R"ns(
struct Point { public (int, int) Deconstruct() { return (1, 2); } }
class Program
{
    public static int Main()
    {
        Point value;
        return match(value) { Point(x) => x, _ => 0 };
    }
}
)ns", "has 1 binding(s), but the tuple has 2");
        }});
}

void registerYieldDiagnosticCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-329", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "diagnostic"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T06YieldDiagnostic", "Exe", R"ns(
class Program
{
    public static Iterator<int> Generate()
    {
        yield return 42;
    }

    public static int Main() { return 0; }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T06YieldDiagnostic.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-329 a generator must now compile (MoveNext emitted): " +
                    emitted.error + emitted.output);
        }});
}

void registerYieldContextCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-379", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "generator", "context", "diagnostic"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T06YieldContext", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        yield return 1;
        return 0;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T06YieldContext.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode != 0 &&
                    (emitted.error + emitted.output).find("yield requires a generator") !=
                        std::string::npos,
                "TL-LANG-379 yield outside a generator must report the context diagnostic: " +
                    emitted.error + emitted.output);
        }});
}

void registerGeneratorRulesCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-380", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "generator", "item", "return", "diagnostic"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T06GeneratorRules", "Exe", R"ns(
class Program
{
    public static Iterator<int> WithReturn()
    {
        yield return 1;
        return 2;
    }

    public static Iterator<int> BadItem()
    {
        yield return "not-an-int";
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T06GeneratorRules.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            const std::string diagnostic = emitted.error + emitted.output;
            context.expect(emitted.started && emitted.exitCode != 0 &&
                    diagnostic.find("a generator cannot return a value") != std::string::npos &&
                    diagnostic.find("generator yields") != std::string::npos &&
                    diagnostic.find("item type is") != std::string::npos,
                "TL-LANG-380 a generator must reject return v and unify yields to the item type: " +
                    diagnostic);
        }});
}

void registerGeneratorConsumerCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-381", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "generator", "move-next", "current", "consumer"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T06GeneratorConsumer", "Exe", R"ns(
class Program
{
    public static Iterator<int> Generate()
    {
        yield return 1;
        yield return 2;
    }

    public static int Main()
    {
        Iterator<int> it = Generate();
        int sum = 0;
        while (it.MoveNext()) { sum += it.Current; }
        return sum;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T06GeneratorConsumer.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-381 a generator iterator must be consumable via MoveNext/Current: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 3,
                "TL-LANG-381 MoveNext/Current must yield 1,2 then terminate (sum=3): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerGeneratorLivenessCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-382", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "generator", "liveness", "local", "frame"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T06GeneratorLiveness", "Exe", R"ns(
class Program
{
    public static Iterator<int> Generate()
    {
        int x = 10;
        yield return x;
        yield return x + 1;
    }

    public static int Main()
    {
        Iterator<int> it = Generate();
        int sum = 0;
        while (it.MoveNext()) { sum += it.Current; }
        return sum;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T06GeneratorLiveness.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-382 a live local must persist across a yield: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 21,
                "TL-LANG-382 the local x must survive the yield (10 + 11 = 21): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerGeneratorParameterCaptureCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-439", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "generator", "liveness", "parameter", "frame", "capture"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T06GeneratorParameterCapture", "Exe", R"ns(
class Program
{
    public static Iterator<int> Generate(int x)
    {
        int dead = 100;
        yield return x;
        yield return x + 1;
    }

    public static int Main()
    {
        Iterator<int> it = Generate(10);
        int sum = 0;
        while (it.MoveNext()) { sum += it.Current; }
        return sum;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T06GeneratorParameterCapture.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-439 a generator parameter must persist across a yield: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 21,
                "TL-LANG-439 the parameter x must survive the yield (10 + 11 = 21): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerGeneratorRefItemCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-440", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "generator", "ref", "foreach", "item", "reference"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T06GeneratorRefItem", "Exe", R"ns(
class Program
{
    public static Iterator<ref int> Gen()
    {
        int x = 10;
        yield return x;
    }

    public static int Main()
    {
        foreach (ref int v in Gen()) { return v; }
        return 0;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T06GeneratorRefItem.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-440 an Iterator<ref T> item must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 10,
                "TL-LANG-440 the ref item must reference the frame local (exit 10): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerGeneratorRefMutItemCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-441", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "generator", "ref mut", "foreach", "item", "reference"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T06GeneratorRefMutItem", "Exe", R"ns(
class Program
{
    public static Iterator<ref mut int> Gen()
    {
        int x = 10;
        yield return x;
    }

    public static int Main()
    {
        foreach (ref mut int v in Gen()) { v = 42; return v; }
        return 0;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T06GeneratorRefMutItem.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-441 an Iterator<ref mut T> item must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-441 the ref mut item must write through to the frame local (exit 42): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerGeneratorPinnedFrameCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-442", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "generator", "ref", "pinning", "frame", "stability"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T06GeneratorPinnedFrame", "Exe", R"ns(
class Program
{
    public static Iterator<ref int> Gen()
    {
        int x = 10;
        yield return x;
        yield return x;
    }

    public static int Main()
    {
        int sum = 0;
        foreach (ref int v in Gen()) { sum += v; }
        return sum;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T06GeneratorPinnedFrame.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-442 a self-referential pinned frame must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 20,
                "TL-LANG-442 the pinned frame must stay stable across yields (10 + 10 = 20): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerGeneratorTypedErrorCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-443", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "generator", "throws", "error", "Iterator<T,E>", "typed"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T06GeneratorTypedError", "Exe", R"ns(
using NativeSharp.Core;
class Program
{
    public static Iterator<int, Exception> Fail() throws Exception
    {
        throw new Exception();
        yield return 42;
    }

    public static Iterator<int, Exception> Ok() throws Exception
    {
        yield return 42;
    }

    public static int Main() throws Exception
    {
        int r = 0;
        try { foreach (int x in Fail()) { r += x; } }
        catch (Exception e) { r += 7; }
        foreach (int x in Ok()) { r += x; }
        return r;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T06GeneratorTypedError.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-443 a typed generator error must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 49,
                "TL-LANG-443 the typed error must rethrow into the catch (7 + 42 = 49): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerGeneratorIntermodularCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-444", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "generator", "iterator", "moveNextFn", "intermodular", "abi"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path producerProject = products.writeProject(
                "T06IteratorProducer", "Library", R"ns(
public class Async
{
    public static Iterator<int> Gen()
    {
        yield return 1;
        yield return 2;
    }
}
)ns");
            const std::filesystem::path consumerProject = products.writeProject(
                "T06IteratorConsumer", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        Iterator<int> it = Async.Gen();
        int sum = 0;
        while (it.MoveNext()) { sum += it.Current; }
        return sum;
    }
}
)ns", "<ProjectReference Include=\"T06IteratorProducer.nsproj\"/>");
            const std::filesystem::path executable =
                products.directory() / "T06IteratorConsumer.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", consumerProject, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-444 the intermodular iterator must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 3,
                "TL-LANG-444 the opaque iterator must be consumed across modules via its moveNextFn (1 + 2 = 3): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncGeneratorRefItemCase(TestRegistry& registry)
{
    const auto addRefCase = [&](std::string id, std::string projectName, std::string source,
                                int expectedExit, std::string expectText)
    {
        registry.add(TestCase{id, "CL-LANG-020", "Host.Language", "integration", "PR",
            {"language", "yield", "async-generator", "ref", "item", "MoveNextAsync", "Current"},
            [source = std::move(source), expectedExit, expectText = std::move(expectText),
             projectName = std::move(projectName), id = std::move(id)](TestContext& context) {
                TemporaryT01Products products;
                const std::filesystem::path project =
                    products.writeProject(projectName, "Exe", source);
                const std::filesystem::path executable =
                    products.directory() / (projectName + ".exe");
                const external_program_support::Result emitted = emitT01Product(
                    L"--emit-project-oop-exe", project, executable, products.directory());
                context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                    id + " must compile: " + emitted.error + emitted.output);
                if (!emitted.started || emitted.exitCode != 0) return;
                const auto result = external_program_support::run(
                    executable, {}, {}, products.directory());
                context.expect(result.started && result.error.empty() &&
                    result.exitCode == static_cast<unsigned long>(expectedExit),
                    id + " " + expectText + ": " + result.error + result.output +
                        " exit=" + std::to_string(result.exitCode));
            }});
    };

    addRefCase("TL-LANG-445", "T07AsyncGenRefRead", R"ns(
class Program
{
    public static AsyncIterator<ref int, int> Gen()
    {
        int x = 10;
        yield return x;
    }
    public static int Main()
    {
        AsyncIterator<ref int, int> it = Gen();
        bool has = await it.MoveNextAsync();
        ref int v = ref it.Current;
        return v;
    }
}
)ns", 10, "the ref item must read through to the persistent frame slot (exit 10)");

    addRefCase("TL-LANG-446", "T07AsyncGenRefMutWrite", R"ns(
class Program
{
    public static AsyncIterator<ref mut int, int> Gen()
    {
        int x = 10;
        yield return x;
    }
    public static int Main()
    {
        AsyncIterator<ref mut int, int> it = Gen();
        bool has = await it.MoveNextAsync();
        ref int v = ref it.Current;
        v = 42;
        return v;
    }
}
)ns", 42, "the ref mut item must write through to the frame slot (exit 42)");

    addRefCase("TL-LANG-447", "T07AsyncGenRefStableFrame", R"ns(
class Program
{
    public static AsyncIterator<ref int, int> Gen()
    {
        int x = 100;
        yield return x;
        int y = 20;
        yield return y;
    }
    public static int Main()
    {
        AsyncIterator<ref int, int> it = Gen();
        int first = 0;
        bool h1 = await it.MoveNextAsync();
        { ref int a = ref it.Current; first = a; }
        bool h2 = await it.MoveNextAsync();
        { ref int b = ref it.Current; return first + b; }
    }
}
)ns", 120, "two ref items must share the same stable frame (100 + 20 = 120)");

    addRefCase("TL-LANG-448", "T07AsyncGenRefAwaitBody", R"ns(
class Program
{
    public static async int Fetch() { return 42; }
    public static AsyncIterator<ref int, int> Gen()
    {
        int x = await Fetch();
        yield return x;
    }
    public static int Main()
    {
        AsyncIterator<ref int, int> it = Gen();
        bool has = await it.MoveNextAsync();
        ref int v = ref it.Current;
        return v;
    }
}
)ns", 42, "the single await+yield machine must keep working with a ref item (exit 42)");
}

void registerAsyncGeneratorRefBorrowRejectCase(TestRegistry& registry)
{
    const auto addRejectCase = [&](std::string id, std::string projectName, std::string source,
                                   std::string diagnostic)
    {
        registry.add(TestCase{id, "CL-LANG-020", "Host.Language", "integration", "PR",
            {"language", "yield", "async-generator", "ref", "borrow", "reject"},
            [source = std::move(source), diagnostic = std::move(diagnostic),
             projectName = std::move(projectName), id = std::move(id)](TestContext& context) {
                TemporaryT01Products products;
                const std::filesystem::path project =
                    products.writeProject(projectName, "Exe", source);
                const std::filesystem::path executable =
                    products.directory() / (projectName + ".exe");
                const external_program_support::Result rejected = emitT01Product(
                    L"--emit-project-oop-exe", project, executable, products.directory());
                context.expect(rejected.started && rejected.exitCode != 0 &&
                        (rejected.error + rejected.output).find(diagnostic) != std::string::npos,
                    id + " must reject '" + diagnostic + "': " + rejected.error + rejected.output);
            }});
    };

    addRejectCase("TL-LANG-449", "T07AsyncGenRefAdvanceBorrow", R"ns(
class Program
{
    public static AsyncIterator<ref int, int> Gen() { int x = 10; yield return x; }
    public static int Main()
    {
        AsyncIterator<ref int, int> it = Gen();
        bool h1 = await it.MoveNextAsync();
        ref int v = ref it.Current;
        bool h2 = await it.MoveNextAsync();
        return v;
    }
}
)ns", "still live");

    addRejectCase("TL-LANG-450", "T07AsyncGenRefDropBorrow", R"ns(
class Program
{
    public static AsyncIterator<ref int, int> Gen() { int x = 10; yield return x; }
    public static int Main()
    {
        AsyncIterator<ref int, int> it = Gen();
        bool h1 = await it.MoveNextAsync();
        ref int v = ref it.Current;
        it.Drop();
        return v;
    }
}
)ns", "still live");

    addRejectCase("TL-LANG-451", "T07AsyncGenRefMutSecondBorrow", R"ns(
class Program
{
    public static AsyncIterator<ref mut int, int> Gen() { int x = 10; yield return x; }
    public static int Main()
    {
        AsyncIterator<ref mut int, int> it = Gen();
        bool h1 = await it.MoveNextAsync();
        ref int a = ref it.Current;
        ref int b = ref it.Current;
        return a;
    }
}
)ns", "mutable borrow");
}

void registerAsyncGeneratorMoveOnlyItemCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-452", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "async-generator", "move-only", "move", "item", "Current"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T07AsyncGenMoveOnly", "Exe", R"ns(
struct Box { public owned int* p; }
class Program
{
    public static AsyncIterator<Box, int> Gen() { Box b = new Box(); yield return move(b); }
    public static int Main()
    {
        AsyncIterator<Box, int> it = Gen();
        bool h = await it.MoveNextAsync();
        Box g = move(it.Current);
        it.Drop();
        return 7;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T07AsyncGenMoveOnly.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-452 the move-only item must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-452 the move-only item must transfer once and not double-free (exit 7): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});

    registry.add(TestCase{"TL-LANG-453", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "async-generator", "move-only", "move", "item", "reject"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T07AsyncGenMoveOnlyTwice", "Exe", R"ns(
struct Box { public owned int* p; }
class Program
{
    public static AsyncIterator<Box, int> Gen() { Box b = new Box(); yield return move(b); }
    public static int Main()
    {
        AsyncIterator<Box, int> it = Gen();
        bool h = await it.MoveNextAsync();
        Box g = move(it.Current);
        Box g2 = move(it.Current);
        return 7;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T07AsyncGenMoveOnlyTwice.exe";
            const external_program_support::Result rejected = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(rejected.started && rejected.exitCode != 0 &&
                    (rejected.error + rejected.output).find("twice") != std::string::npos,
                "TL-LANG-453 a second move(it.Current) must be rejected: " +
                    rejected.error + rejected.output);
        }});

    registry.add(TestCase{"TL-LANG-454", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "async-generator", "drop", "frame", "idempotent"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T07AsyncGenDropIdempotent", "Exe", R"ns(
class Program
{
    public static AsyncIterator<int, int> Gen() { yield return 1; }
    public static int Main()
    {
        AsyncIterator<int, int> it = Gen();
        bool h = await it.MoveNextAsync();
        it.Drop();
        it.Drop();
        return 7;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T07AsyncGenDropIdempotent.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-454 the async iterator double Drop must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-454 the second Drop must not re-free the frame (exit 7): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncGeneratorCancellationCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-455", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "async-generator", "cancel", "MoveNextAsync", "terminal"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T07AsyncGenCancel", "Exe", R"ns(
class Program
{
    public static AsyncIterator<int, int> Gen() { yield return 1; yield return 2; }
    public static int Main()
    {
        AsyncIterator<int, int> it = Gen();
        bool h1 = await it.MoveNextAsync();
        it.Cancel();
        bool h2 = await it.MoveNextAsync();
        return h2 ? 1 : 7;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T07AsyncGenCancel.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-455 the cancelled async generator must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-455 it.Cancel() must make the next demand return false (exit 7): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});

    registry.add(TestCase{"TL-LANG-456", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "async-generator", "cancel", "finally", "cleanup"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T07AsyncGenCancelFinally", "Exe", R"ns(
class Program
{
    public static int Log;
    public static AsyncIterator<int, int> Gen()
    {
        try { yield return 1; }
        finally { Log += 1; }
    }
    public static int Main()
    {
        AsyncIterator<int, int> it = Gen();
        bool h1 = await it.MoveNextAsync();
        it.Cancel();
        bool h2 = await it.MoveNextAsync();
        return Log;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T07AsyncGenCancelFinally.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-456 the cancelled finally must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 1,
                "TL-LANG-456 the finally must run once on cancellation (Log=1): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerTaskScopeCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-457", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "task-scope", "spawn", "join", "executor", "trait"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T07TaskScopeJoin", "Exe", R"ns(
using NativeSharp.Core;
struct InlineExecutor : IExecutor
{
    public int spawned;
    public int joined;
    public InlineExecutor() { spawned = 0; joined = 0; }
    public int Spawn(Future<void> task) { spawned = spawned + 1; return spawned - 1; }
    public int PollJoin(int handle) { joined = joined + 1; return 0; }
    public void Cancel(int handle) { }
}
class Program
{
    public static async void Child() { }
    public static int Main()
    {
        TaskScope<InlineExecutor> scope = TaskScope<InlineExecutor>.Create(new InlineExecutor());
        scope.Spawn(Child());
        scope.Spawn(Child());
        scope.Join();
        return scope.ChildCount();
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T07TaskScopeJoin.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-457 the task scope must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 2,
                "TL-LANG-457 Spawn must register two children and Join must run (count=2): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});

    registry.add(TestCase{"TL-LANG-458", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "task-scope", "spawn", "join", "child-failure", "executor"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T07TaskScopeFailure", "Exe", R"ns(
using NativeSharp.Core;
struct FailingExecutor : IExecutor
{
    public int spawned;
    public FailingExecutor() { spawned = 0; }
    public int Spawn(Future<void> task) { spawned = spawned + 1; return spawned - 1; }
    public int PollJoin(int handle) { return 1; }
    public void Cancel(int handle) { }
}
class Program
{
    public static async void Child() { }
    public static int Main()
    {
        TaskScope<FailingExecutor> scope = TaskScope<FailingExecutor>.Create(new FailingExecutor());
        scope.Spawn(Child());
        scope.Join();
        return scope.Failed() ? 7 : 1;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T07TaskScopeFailure.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-458 the failing child scope must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-458 a failing child must mark the scope failed (exit 7): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});

    registry.add(TestCase{"TL-LANG-459", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async", "ref", "capture", "future", "yield", "child"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T07AsyncRefCapture", "Exe", R"ns(
class Program
{
    public static async void Child(ref int value)
    {
        yield;
        value = value + 1;
    }
    public static async int Main()
    {
        int value = 41;
        Future<void> f = Child(ref value);
        await f;
        return value;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T07AsyncRefCapture.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-459 the ref-capturing async child must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-459 Child(ref value) must write through to the source (41->42): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});

    registry.add(TestCase{"TL-LANG-462", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "task-scope", "destructor", "cleanup", "reverse-order"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T07TaskScopeCleanup", "Exe", R"ns(
using NativeSharp.Core;
struct TrackingExecutor : IExecutor
{
    public int Spawn(Future<void> task) { Program.SpawnCount = Program.SpawnCount + 1; return Program.SpawnCount - 1; }
    public int PollJoin(int handle) { Program.JoinLog[Program.JoinCount] = handle; Program.JoinCount = Program.JoinCount + 1; return 0; }
    public void Cancel(int handle) { Program.CancelLog[Program.CancelCount] = handle; Program.CancelCount = Program.CancelCount + 1; }
}
class Program
{
    public static int SpawnCount;
    public static int[] JoinLog = new int[16];
    public static int JoinCount;
    public static int[] CancelLog = new int[16];
    public static int CancelCount;
    public static async void Child() { }
    public static int Main()
    {
        {
            TaskScope<TrackingExecutor> scope = TaskScope<TrackingExecutor>.Create(new TrackingExecutor());
            scope.Spawn(Child());
            scope.Spawn(Child());
            scope.Spawn(Child());
        }
        bool ok = CancelCount == 3 && JoinCount == 3 &&
            CancelLog[0] == 2 && CancelLog[1] == 1 && CancelLog[2] == 0 &&
            JoinLog[0] == 2 && JoinLog[1] == 1 && JoinLog[2] == 0;
        return ok ? 7 : 1;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T07TaskScopeCleanup.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-462 the task scope cleanup must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-462 the destructor must Cancel+Join children in reverse spawn order once (exit 7): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});

    registry.add(TestCase{"TL-LANG-463", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "task-scope", "join", "idempotent"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T07TaskScopeIdempotent", "Exe", R"ns(
using NativeSharp.Core;
struct CountingExecutor : IExecutor
{
    public int Spawn(Future<void> task) { return 0; }
    public int PollJoin(int handle) { Program.Joins = Program.Joins + 1; return 0; }
    public void Cancel(int handle) { Program.Cancels = Program.Cancels + 1; }
}
class Program
{
    public static int Joins;
    public static int Cancels;
    public static async void Child() { }
    public static int Main()
    {
        TaskScope<CountingExecutor> scope = TaskScope<CountingExecutor>.Create(new CountingExecutor());
        scope.Spawn(Child());
        scope.Join();
        scope.Join();
        return (Joins == 1 && Cancels == 0) ? 7 : 1;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T07TaskScopeIdempotent.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-463 the idempotent Join must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-463 a second Join must not re-join (one PollJoin, zero Cancel): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});

    registry.add(TestCase{"TL-LANG-460", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "task-scope", "borrow", "lifetime", "reject"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T07TaskScopeBorrowEscape", "Exe", R"ns(
using NativeSharp.Core;
struct InlineExecutor : IExecutor
{
    public int Spawn(Future<void> task) { return 0; }
    public int PollJoin(int handle) { return 0; }
    public void Cancel(int handle) { }
}
class Program
{
    public static async void Child(ref int value) { yield; value = value + 1; }
    public static int Main()
    {
        TaskScope<InlineExecutor> scope = TaskScope<InlineExecutor>.Create(new InlineExecutor());
        {
            int value = 41;
            scope.Spawn(Child(ref value));
        }
        scope.Join();
        return 0;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T07TaskScopeBorrowEscape.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode != 0 &&
                    emitted.output.find("scoped child borrow outlives its source") != std::string::npos,
                "TL-LANG-460 a child borrow that outlives its source must be rejected: " +
                    emitted.error + emitted.output);
        }});

    registry.add(TestCase{"TL-LANG-461", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "task-scope", "borrow", "join", "discharge"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T07TaskScopeBorrowJoin", "Exe", R"ns(
using NativeSharp.Core;
struct InlineExecutor : IExecutor
{
    public int Spawn(Future<void> task) { return 0; }
    public int PollJoin(int handle) { return 0; }
    public void Cancel(int handle) { }
}
class Program
{
    public static async void Child(ref int value) { yield; value = value + 1; }
    public static int Main()
    {
        int value = 41;
        TaskScope<InlineExecutor> scope = TaskScope<InlineExecutor>.Create(new InlineExecutor());
        scope.Spawn(Child(ref value));
        scope.Join();
        value = value + 1;
        return value;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T07TaskScopeBorrowJoin.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-461 Join must discharge the borrow so the source can die: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-461 after Join the child write + the source increment reach 42: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerSendSyncCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-464", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "send", "sync", "trait", "structural", "constraint"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T07SendSyncPositive", "Exe", R"ns(
using NativeSharp.Core;
struct Safe { public int x; public int y; }
struct Holder<T> where T : Send
{
    public T value;
}
class Program
{
    public static int Main()
    {
        Holder<Safe> h;
        h.value = new Safe();
        return 7;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T07SendSyncPositive.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-464 a struct of safe fields must satisfy where T : Send structurally: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-464 the structural Send program must run (exit 7): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});

    registry.add(TestCase{"TL-LANG-465", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "send", "raw-pointer", "reject", "constraint"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T07SendSyncRawNegative", "Exe", R"ns(
using NativeSharp.Core;
struct Raw { public byte* p; }
struct Holder<T> where T : Send
{
    public T value;
}
class Program
{
    public static int Main()
    {
        Holder<Raw> h;
        return 0;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T07SendSyncRawNegative.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode != 0 &&
                    emitted.output.find("does not implement it") != std::string::npos,
                "TL-LANG-465 a raw-pointer struct must not derive Send structurally: " +
                    emitted.error + emitted.output);
        }});

    registry.add(TestCase{"TL-LANG-466", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "thread-executor", "local", "send", "compile"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T07ThreadLocalExecutor", "Exe", R"ns(
using NativeSharp.Core;
struct Raw { public byte* p; }
struct LocalExec : IExecutor
{
    public int Spawn(Future<void> task) { return 0; }
    public int PollJoin(int handle) { return 0; }
    public void Cancel(int handle) { }
}
class Program
{
    public static async void Child(ref Raw value) { yield; }
    public static int Main()
    {
        Raw value = default(Raw);
        TaskScope<LocalExec> scope = TaskScope<LocalExec>.Create(new LocalExec());
        scope.Spawn(Child(ref value));
        scope.Join();
        return 7;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T07ThreadLocalExecutor.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-466 a non-Send Future on a local IExecutor must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-466 the local-executor program must run (exit 7): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});

    registry.add(TestCase{"TL-LANG-467", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "thread-executor", "send", "reject"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T07ThreadSendNegative", "Exe", R"ns(
using NativeSharp.Core;
struct Raw { public byte* p; }
struct ThreadExec : IThreadExecutor
{
    public int Spawn(Future<void> task) { return 0; }
    public int PollJoin(int handle) { return 0; }
    public void Cancel(int handle) { }
}
class Program
{
    public static async void Child(ref Raw value) { yield; }
    public static int Main()
    {
        Raw value = default(Raw);
        TaskScope<ThreadExec> scope = TaskScope<ThreadExec>.Create(new ThreadExec());
        scope.Spawn(Child(ref value));
        scope.Join();
        return 0;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T07ThreadSendNegative.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode != 0 &&
                    emitted.output.find("future submitted to a thread executor is not Send") !=
                        std::string::npos,
                "TL-LANG-467 a non-Send Future on an IThreadExecutor must be rejected: " +
                    emitted.error + emitted.output);
        }});

    registry.add(TestCase{"TL-LANG-468", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "thread-executor", "send", "compile", "run"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T07ThreadSendPositive", "Exe", R"ns(
using NativeSharp.Core;
struct ThreadExec : IThreadExecutor
{
    public int Spawn(Future<void> task) { return 0; }
    public int PollJoin(int handle) { return 0; }
    public void Cancel(int handle) { }
}
class Program
{
    public static async void Child(ref int value) { yield; value = value + 1; }
    public static int Main()
    {
        int value = 41;
        TaskScope<ThreadExec> scope = TaskScope<ThreadExec>.Create(new ThreadExec());
        scope.Spawn(Child(ref value));
        scope.Join();
        value = value + 1;
        return value;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T07ThreadSendPositive.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-468 a Send Future on an IThreadExecutor must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-468 the thread-executor child write + source increment reach 42: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});

    registry.add(TestCase{"TL-LANG-469", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "thread-executor", "sync", "shared", "reject"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T07ThreadSyncNegative", "Exe", R"ns(
using NativeSharp.Core;
struct SendOnly { public ref mut int cell; }
struct ThreadExec : IThreadExecutor
{
    public int Spawn(Future<void> task) { return 0; }
    public int PollJoin(int handle) { return 0; }
    public void Cancel(int handle) { }
}
class Program
{
    public static async void Child(ref SendOnly value) { yield; }
    public static int Main()
    {
        SendOnly value = default(SendOnly);
        TaskScope<ThreadExec> scope = TaskScope<ThreadExec>.Create(new ThreadExec());
        scope.Spawn(Child(ref value));
        scope.Join();
        return 0;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T07ThreadSyncNegative.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode != 0 &&
                    emitted.output.find("requires its referred type") != std::string::npos,
                "TL-LANG-469 a shared capture of a non-Sync type must be rejected: " +
                    emitted.error + emitted.output);
        }});
}

void registerAsyncIteratorMetadataCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-470", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async-iterator", "nsmeta", "library-reference", "source-free", "intermodular"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path producerProject = products.writeProject(
                "T07AsyncIterMetadata", "Library", R"ns(
public class Lib
{
    public static AsyncIterator<int, int> Gen()
    {
        yield return 1;
        yield return 2;
        yield return 3;
    }
}
)ns");
            const std::filesystem::path producerLibrary =
                products.directory() / "T07AsyncIterMetadata.lib";
            const external_program_support::Result producer = emitT01Product(
                L"--emit-project-oop-lib", producerProject, producerLibrary,
                products.directory());
            context.expect(producer.started && producer.exitCode == 0 && producer.error.empty(),
                "TL-LANG-470 the async-iterator producer must build: " +
                    producer.error + producer.output);
            if (!producer.started || producer.exitCode != 0) return;

            std::error_code removalError;
            std::filesystem::remove(producerProject, removalError);
            std::filesystem::remove(
                products.directory() / "T07AsyncIterMetadata.ns", removalError);

            constexpr std::string_view libraryReference =
                "<LibraryReference Include=\"T07AsyncIterMetadata.lib\"/>";
            const std::filesystem::path consumerProject = products.writeProject(
                "T07AsyncIterMetadataConsumer", "Exe", R"ns(
class Program
{
    public static async int Main()
    {
        AsyncIterator<int, int> it = Lib.Gen();
        bool has = await it.MoveNextAsync();
        int sum = 0;
        while (has) { sum = sum + it.Current; has = await it.MoveNextAsync(); }
        return sum;
    }
}
)ns", libraryReference);
            const std::filesystem::path consumerExecutable =
                products.directory() / "T07AsyncIterMetadataConsumer.exe";
            const external_program_support::Result consumer = emitT01Product(
                L"--emit-project-oop-exe", consumerProject, consumerExecutable,
                products.directory());
            context.expect(consumer.started && consumer.exitCode == 0 && consumer.error.empty(),
                "TL-LANG-470 the source-free async-iterator consumer must compile: " +
                    consumer.error + consumer.output);
            if (!consumer.started || consumer.exitCode != 0) return;
            const auto result = external_program_support::run(
                consumerExecutable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 6,
                "TL-LANG-470 the imported async-iterator must iterate to sum 6: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncIteratorPdbCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-471", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "async-iterator", "pdb", "suspension", "async-frame", "debug"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T07AsyncIterPdb", "Exe", R"ns(
class Program
{
    public static AsyncIterator<int, int> Gen()
    {
        yield return 1;
        yield return 2;
        yield return 3;
    }
    public static int Main()
    {
        AsyncIterator<int, int> it = Gen();
        bool has = await it.MoveNextAsync();
        int sum = 0;
        while (has) { sum = sum + it.Current; has = await it.MoveNextAsync(); }
        return sum;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T07AsyncIterPdb.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-471 the async-iterator debug build must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const std::filesystem::path pdb = products.directory() / "T07AsyncIterPdb.pdb";
            context.expect(std::filesystem::is_regular_file(pdb) &&
                    std::filesystem::file_size(pdb) > 0U,
                "TL-LANG-471 the async-iterator must retain a non-empty PDB");
            std::ifstream input(pdb, std::ios::binary);
            const std::string content((std::istreambuf_iterator<char>(input)),
                                      std::istreambuf_iterator<char>());
            context.expect(content.find("Gen$moveNext$poll") != std::string::npos &&
                    content.find("Gen$moveNext") != std::string::npos &&
                    content.find("Gen$drop") != std::string::npos,
                "TL-LANG-471 the PDB must carry the async suspension frames "
                "($moveNext$poll/$moveNext/$drop)");
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 6,
                "TL-LANG-471 the debug async-iterator must run to 6: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerPartialFailureCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-472", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "partial-failure", "drop", "throws", "constructor", "subobject"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T09PartialFailure", "Exe", R"ns(
using NativeSharp.Core;
struct Field
{
    public static int Drops;
    ~Field() { Drops = Drops + 1; }
}
struct S
{
    public owned Field a;
    public owned Field b;
    public S() throws Exception
    {
        a = new Field();
        b = new Field();
        throw new Exception();
    }
}
class Program
{
    public static int Main()
    {
        try { S s = new S(); } catch { }
        return Field.Drops == 2 ? 7 : 1;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T09PartialFailure.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-472 the partial-failure program must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-472 already-initialized subobjects must drop on a failing constructor (Drops=2): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerUtf8LiteralCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-473", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "utf8", "literal", "readonly-span", "allocation-free"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T10Utf8Literal", "Exe", R"ns(
using NativeSharp.Core;
class Program
{
    public static int Main()
    {
        ReadOnlySpan<byte> ascii = u8"hello";
        ReadOnlySpan<byte> accented = u8"\u00E9";
        return ascii.Length() * 10 + accented.Length();
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T10Utf8Literal.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-473 the u8 literal must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 52,
                "TL-LANG-473 u8 literals must carry UTF-8 bytes (5 + 2) as ReadOnlySpan<byte>: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerInterpolationHandlerCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-474", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "interpolation", "handler", "builder", "allocation-free"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T10InterpolationHandler", "Exe", R"ns(
using NativeSharp.Core;
struct Builder
{
    public int Sum;
    public void AppendLiteral(string s) { }
    public void AppendFormatted(int value) { Sum = Sum + value; }
    public int Finish() { return Sum; }
}
class Program
{
    public static int Main()
    {
        Builder b = $"1 {2} 3";
        return b.Finish();
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T10InterpolationHandler.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-474 the interpolation handler must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 2,
                "TL-LANG-474 the interpolation handler must call AppendLiteral/AppendFormatted/Finish: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerRefFieldEscapeCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-475", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "ref-field", "escape", "borrow", "lifetime"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path okProject = products.writeProject(
                "T11RefFieldOk", "Exe", R"ns(
using NativeSharp.Core;
struct RefHolder
{
    public ref int Field;
}
class Program
{
    public static int Main()
    {
        int x = 5;
        RefHolder h = new RefHolder();
        h.Field = ref x;
        return h.Field;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path okExecutable =
                products.directory() / "T11RefFieldOk.exe";
            const external_program_support::Result ok = emitT01Product(
                L"--emit-project-oop-exe", okProject, okExecutable, products.directory());
            context.expect(ok.started && ok.exitCode == 0 && ok.error.empty(),
                "TL-LANG-475 ref field store/read must compile: " + ok.error + ok.output);
            if (!ok.started || ok.exitCode != 0) return;
            const auto okRun = external_program_support::run(
                okExecutable, {}, {}, products.directory());
            context.expect(okRun.started && okRun.error.empty() && okRun.exitCode == 5,
                "TL-LANG-475 h.Field = ref x; h.Field must read 5: " +
                    okRun.error + okRun.output + " exit=" + std::to_string(okRun.exitCode));

            const std::filesystem::path escapeProject = products.writeProject(
                "T11RefFieldEscape", "Exe", R"ns(
using NativeSharp.Core;
struct RefHolder
{
    public ref int Field;
    public static RefHolder Make()
    {
        int x = 5;
        RefHolder h = new RefHolder();
        h.Field = ref x;
        return h;
    }
}
class Program
{
    public static int Main()
    {
        RefHolder h = RefHolder.Make();
        return h.Field;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path escapeExecutable =
                products.directory() / "T11RefFieldEscape.exe";
            const external_program_support::Result escaped = emitT01Product(
                L"--emit-project-oop-exe", escapeProject, escapeExecutable, products.directory());
            context.expect(
                escaped.started && escaped.exitCode != 0 &&
                    escaped.output.find("cannot return a value whose ref field") != std::string::npos,
                "TL-LANG-475 returning a struct whose ref field aliases a local must be rejected: " +
                    escaped.error + escaped.output);
        }});
}

void registerGenericAttributeCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-476", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "attribute", "generic", "metadata"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T11GenericAttribute", "Exe", R"ns(
using NativeSharp.Core;
#[attribute Tag<T> targets(type)]
#[Tag<int>]
class Program
{
    public static int Main() { return 7; }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T11GenericAttribute.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-476 a generic attribute must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-476 a generic attribute must match its Tag<T> definition: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAllocatorSelectionCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-477", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "allocator", "IAllocator", "new", "placement", "construction"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path repository(NSC_SUITE_REPOSITORY_ROOT);
            const std::string coreProject =
                (repository / "lib/NativeSharp.Core/NativeSharp.Core.nsproj").generic_string();
            const std::filesystem::path project = products.writeProject(
                "T13F02AllocatorSelection", "Exe", R"ns(
using NativeSharp.Core;
class A { public int X; public A(int x) { X = x; } }
class BlockAllocator : IAllocator
{
    private owned byte[] pool;
    public BlockAllocator(int size) { pool = new byte[size]; }
    public byte* Allocate(long bytes, int alignment) { return Spans.Of<byte>(pool).Data(); }
    public byte* Resize(byte* ptr, long oldBytes, long newBytes, int alignment) { return ptr; }
    public void Deallocate(byte* ptr, long bytes, int alignment) { }
}
class Program
{
    public static int Main()
    {
        raii BlockAllocator alloc = new BlockAllocator(1024);
        A a = new (alloc) A(42);
        return a.X;
    }
}
)ns", "<ProjectReference Include=\"" + coreProject + "\"/>");
            const std::filesystem::path executable =
                products.directory() / "T13F02AllocatorSelection.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-477 allocator selection must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-477 new (alloc) A(42) must construct through the allocator: " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerGeneratorEarlyStopCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-383", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "generator", "yield-break", "early-stop"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T06GeneratorEarlyStop", "Exe", R"ns(
class Program
{
    public static Iterator<int> Generate()
    {
        yield return 1;
        yield break;
        yield return 2;
    }

    public static int Main()
    {
        Iterator<int> it = Generate();
        int sum = 0;
        while (it.MoveNext()) { sum += it.Current; }
        return sum;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T06GeneratorEarlyStop.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-383 yield break must terminate the generator: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 1,
                "TL-LANG-383 yield break must stop before the unreachable yield (sum=1): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerGeneratorDropCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-384", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "generator", "drop", "cleanup"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T06GeneratorDrop", "Exe", R"ns(
class Program
{
    public static Iterator<int> Generate()
    {
        int x = 10;
        yield return x;
    }

    public static int Main()
    {
        Iterator<int> it = Generate();
        it.MoveNext();
        it.Drop();
        return 0;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T06GeneratorDrop.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-384 it.Drop() must resolve and call the drop function: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 0,
                "TL-LANG-384 the explicit iterator Drop must run (exit 0): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncGeneratorSyncCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-385", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "async-generator", "async-stream", "MoveNext"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T07AsyncGeneratorSync", "Exe", R"ns(
class Program
{
    public static AsyncIterator<int, int> Generate()
    {
        yield return 1;
        yield return 2;
    }

    public static int Main()
    {
        AsyncIterator<int, int> it = Generate();
        int sum = 0;
        bool has = await it.MoveNextAsync();
        while (has) { sum += it.Current; has = await it.MoveNextAsync(); }
        return sum;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T07AsyncGeneratorSync.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-385 async generator sync MoveNext must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 3,
                "TL-LANG-385 async generator must yield 1 then 2 (sum=3): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncGeneratorDemandCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-386", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "async-generator", "MoveNextAsync", "await", "demand"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T07AsyncGeneratorDemand", "Exe", R"ns(
class Program
{
    public static AsyncIterator<int, int> Generate()
    {
        yield return 1;
        yield return 2;
    }

    public static int Main()
    {
        AsyncIterator<int, int> it = Generate();
        int sum = 0;
        bool has = await it.MoveNextAsync();
        while (has) { sum += it.Current; has = await it.MoveNextAsync(); }
        return sum;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T07AsyncGeneratorDemand.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-386 await MoveNextAsync must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 3,
                "TL-LANG-386 the demand future must drive the generator (sum=3): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncGeneratorAwaitBodyCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-387", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "async-generator", "await", "single-machine"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T07AsyncGeneratorAwaitBody", "Exe", R"ns(
class Program
{
    public static async int Fetch()
    {
        return 42;
    }

    public static AsyncIterator<int, int> Generate()
    {
        int v = await Fetch();
        yield return v;
    }

    public static int Main()
    {
        AsyncIterator<int, int> it = Generate();
        int sum = 0;
        bool has = await it.MoveNextAsync();
        while (has) { sum += it.Current; has = await it.MoveNextAsync(); }
        return sum;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T07AsyncGeneratorAwaitBody.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-387 await inside the generator body must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-387 the single await+yield machine must yield 42 (sum=42): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerAsyncGeneratorPostTerminalCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-400", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "yield", "async-generator", "MoveNextAsync", "terminal", "backpressure"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T07AsyncGeneratorPostTerminal", "Exe", R"ns(
class Program
{
    public static AsyncIterator<int, int> Generate()
    {
        yield return 1;
    }

    public static int Main()
    {
        AsyncIterator<int, int> it = Generate();
        bool has = await it.MoveNextAsync();
        bool terminal = await it.MoveNextAsync();
        bool postTerminal = await it.MoveNextAsync();
        return (has ? 1 : 0) + (terminal ? 2 : 0) + (postTerminal ? 4 : 0);
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T07AsyncGeneratorPostTerminal.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-400 post-terminal demand must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 1,
                "TL-LANG-400 terminal is sticky (has=1, terminal=0, postTerminal=0, exit 1): " +
                    result.error + result.output + " exit=" + std::to_string(result.exitCode));
        }});
}

void registerExtensionSetterMethodCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-330", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "extension", "property", "setter"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T08ExtensionSetter", "Exe", R"ns(
namespace Suite
{
    extension int
    {
        public int Doubled
        {
            get { return this * 2; }
            set { int observed = value; }
        }
    }
}

using Suite;

class Program
{
    public static int Main()
    {
        21.set_Doubled(42);
        21.Doubled = 42;
        return 21.Doubled;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T08ExtensionSetter.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-330 an extension property setter method must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-330 the setter method + getter must dispatch: " +
                    result.error + result.output);
        }});
}

void registerNameOfCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-331", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "nameof", "compile-time", "string"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T10NameOf", "Exe", R"ns(
class Box<T>
{
}

class Program
{
    private static int Counter;

    private static int SideEffect()
    {
        Counter = Counter + 1;
        return Counter;
    }

    public static int Main()
    {
        string s = nameof(Program);
        string m = nameof(Program.Main);
        string g = nameof(Box<int>);
        string e = nameof(SideEffect());
        if (Counter != 0 || e.Length != 10)
        {
            return 1;
        }
        return s.Length + m.Length + g.Length;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T10NameOf.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-331 nameof must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 14,
                "TL-LANG-331 nameof must preserve spelling without evaluating SideEffect(): " +
                    result.error + result.output);
        }});
}

void registerRawInterpolatedCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-332", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "raw-interpolated", "string"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T10RawInterpolated", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        string s = $"""answer""";
        return s.Length;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T10RawInterpolated.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-332 a raw interpolated string must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 6,
                "TL-LANG-332 $\"\"\"answer\"\"\" must be \"answer\" (length 6): " +
                    result.error + result.output);
        }});
}

void registerNoHeapEscapeCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-333", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "noheap", "escape", "string"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T10NoHeapEscape", "Exe", R"ns(
class Program
{
    public static noheap int Main()
    {
        return "A\u0041\eB".Length;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T10NoHeapEscape.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-333 escapes in a noheap string literal must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 4,
                "TL-LANG-333 \"A\\u0041\\eB\" must decode to 4 bytes: " +
                    result.error + result.output);
        }});
}

void registerInitAccessorCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-334", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "init", "accessor", "property"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T09InitAccessor", "Exe", R"ns(
class Program
{
    public int Value { get; init; }

    public Program()
    {
        Value = 42;
    }

    public static int Main()
    {
        raii Program p = new Program();
        return p.Value;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T09InitAccessor.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-334 an init accessor must parse and compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-334 the init accessor must dispatch as a setter: " +
                    result.error + result.output);

            const std::filesystem::path negativeProject = products.writeProject(
                "T09InitNegative", "Exe", R"ns(
class Program
{
    public int Value { get; init; }

    public static int Main()
    {
        raii Program p = new Program();
        p.Value = 42;
        return p.Value;
    }
}
)ns");
            const external_program_support::Result rejected = emitT01Product(
                L"--emit-project-oop-exe", negativeProject,
                products.directory() / "T09InitNegative.exe", products.directory());
            context.expect(rejected.started && rejected.exitCode != 0 &&
                    (rejected.error + rejected.output).find(
                        "init accessor can only be assigned during construction") !=
                        std::string::npos,
                "TL-LANG-334 init assignment outside a constructor must be rejected: " +
                    rejected.error + rejected.output);
        }});
}

void verifyRequiredMemberFlowCases(
    TestContext& context,
    TemporaryT01Products& products)
{
    const auto compile = [&](std::string_view name, std::string_view source)
    {
        const std::filesystem::path project = products.writeProject(
            std::string(name), "Exe", std::string(source));
        return emitT01Product(
            L"--emit-project-oop-exe", project,
            products.directory() / (std::string(name) + ".exe"), products.directory());
    };
    const auto expectRequiredFailure = [&](std::string_view name, std::string_view source)
    {
        const external_program_support::Result result = compile(name, source);
        context.expect(result.started && result.exitCode != 0 &&
                (result.error + result.output).find(
                    "required field 'Value' is never assigned") != std::string::npos,
            "TL-LANG-335 " + std::string(name) +
                " must reject a normal exit missing Value: " + result.error + result.output);
    };

    expectRequiredFailure("T09RequiredOneBranch", R"ns(
class Program
{
    public required int Value;
    public Program(bool assign) { if (assign) { Value = 42; } }
    public static int Main() { return 0; }
}
)ns");

    const external_program_support::Result bothBranches = compile(
        "T09RequiredBothBranches", R"ns(
class Program
{
    public required int Value;
    public Program(bool first)
    {
        if (first) { Value = 42; } else { Value = 7; }
    }
    public static int Main() { raii Program p = new Program(true); return p.Value; }
}
)ns");
    context.expect(
        bothBranches.started && bothBranches.exitCode == 0 && bothBranches.error.empty(),
        "TL-LANG-335 both constructor branches must assign Value: " +
            bothBranches.error + bothBranches.output);

    const external_program_support::Result terminatingBranch = compile(
        "T09RequiredTerminatingBranch", R"ns(
namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;
namespace RequiredFlow
{
    public class ConstructionFailure : Exception { }
    public class Program
    {
        public required int Value;
        public Program(bool fail) throws RequiredFlow.ConstructionFailure
        {
            if (fail) { throw new ConstructionFailure(); }
            Value = 42;
        }
        public static int Main() throws RequiredFlow.ConstructionFailure { raii Program p = new Program(false); return p.Value; }
    }
}
)ns");
    context.expect(
        terminatingBranch.started && terminatingBranch.exitCode == 0 &&
            terminatingBranch.error.empty(),
        "TL-LANG-335 a throwing branch must not contaminate the normal exit: " +
            terminatingBranch.error + terminatingBranch.output);

    expectRequiredFailure("T09RequiredEarlyReturn", R"ns(
class Program
{
    public required int Value;
    public Program(bool early)
    {
        if (early) { return; }
        Value = 42;
    }
    public static int Main() { return 0; }
}
)ns");
}

void registerRequiredMemberCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-335", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "required", "member", "field"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T09RequiredMember", "Exe", R"ns(
class Program
{
    public required int Value;

    public Program()
    {
        Value = 42;
    }

    public static int Main()
    {
        raii Program p = new Program();
        return p.Value;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T09RequiredMember.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-335 a required field must parse and compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-335 the required field must behave like an ordinary field: " +
                    result.error + result.output);

            const std::filesystem::path negativeProject = products.writeProject(
                "T09RequiredNegative", "Exe", R"ns(
class Program
{
    public required int Value;

    public Program()
    {
    }

    public static int Main()
    {
        return 0;
    }
}
)ns");
            const external_program_support::Result rejected = emitT01Product(
                L"--emit-project-oop-exe", negativeProject,
                products.directory() / "T09RequiredNegative.exe", products.directory());
            context.expect(rejected.started && rejected.exitCode != 0 &&
                    (rejected.error + rejected.output).find(
                        "required field 'Value' is never assigned") != std::string::npos,
                "TL-LANG-335 an unassigned required field must be rejected: " +
                    rejected.error + rejected.output);
            verifyRequiredMemberFlowCases(context, products);
        }});
}

void registerConstInterpolationCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-336", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "interpolation", "const", "string"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T10ConstInterpolation", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        string s = $"answer={42}";
        return s.Length;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T10ConstInterpolation.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-336 a consteval interpolation must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 9,
                "TL-LANG-336 $\"answer={42}\" must be \"answer=42\" (length 9): " +
                    result.error + result.output);
        }});
}

void registerPrimaryConstructorCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-337", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "primary", "constructor", "struct"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T09PrimaryConstructor", "Exe", R"ns(
public struct Point(public int x, public int y)
{
}

class Program
{
    public static int Main()
    {
        Point p = new Point(3, 4);
        return p.x + p.y;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T09PrimaryConstructor.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-337 a struct primary constructor must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-337 promoted fields must dispatch: 3 + 4 = 7: " +
                    result.error + result.output);
        }});
}

void registerEventCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-338", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "event", "delegate", "handler"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T09Event", "Exe", R"ns(
public delegate void Handler(int x);

class Program
{
    public static int Result = 0;
    public event Handler Changed;

    public void Fire()
    {
        Changed(42);
    }

    public static void Handle(int x)
    {
        Result = x;
    }

    public static int Main()
    {
        raii Program p = new Program();
        p.Changed += Handle;
        p.Fire();
        return Result;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T09Event.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-338 an event with add/remove and invocation must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-338 the event handler must dispatch through add/invoke: " +
                    result.error + result.output);
        }});
}

// EN: Freeze 3.0 DEC-02: a `new` stored in a local without `owned` or `raii` is a compile error
//     (LostOwnership, C-01). The case keeps testing target-typed construction, so the local takes the
//     object through an explicit `raii` owner and the type still comes only from the target.
// ES: Congelamiento 3.0 DEC-02: un `new` guardado en un local sin `owned` ni `raii` es error de
//     compilacion (LostOwnership, C-01). El caso sigue probando la construccion target-typed, asi que
//     el local recibe el objeto con un dueno `raii` explicito y el tipo sigue saliendo solo del destino.
void registerTargetTypedNewCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-339", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "target-typed", "new", "construction"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T09TargetTypedNew", "Exe", R"ns(
class Point
{
    public int X;
    public int Y;

    public Point(int x, int y)
    {
        X = x;
        Y = y;
    }
}

class Program
{
    public static int Main()
    {
        raii Point p = new(3, 4);
        return p.X + p.Y;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T09TargetTypedNew.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-339 target-typed new() must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-339 new() must infer Point from the target: 3 + 4 = 7: " +
                    result.error + result.output);
        }});
}

void registerNullConditionalWriteCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-340", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "null-conditional", "assignment", "write"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T10NullConditionalWrite", "Exe", R"ns(
class Box
{
    public int Value;
}

class Program
{
    private static int ReceiverEvaluations;
    private static int RhsEvaluations;

    private static Box Receiver(Box value)
    {
        ReceiverEvaluations += 1;
        return value;
    }

    private static int NextValue(int value)
    {
        RhsEvaluations += 1;
        return value;
    }

    public static int Main()
    {
        Box b = null;
        int skipped = (Receiver(b)?.Value = NextValue(1));
        raii Box c = new Box();
        int stored = (Receiver(c)?.Value = NextValue(42));
        if (skipped != 0) return 100;
        if (stored != 42 || c.Value != 42) return 101;
        if (ReceiverEvaluations != 2 || RhsEvaluations != 1) return 102;
        return c.Value;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T10NullConditionalWrite.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-340 null-conditional write must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-340 c?.Value = 42 must store (and b?.Value skip on null): " +
                    result.error + result.output);
        }});
}

void registerCustomEventCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-341", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "event", "custom", "accessor"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T09CustomEvent", "Exe", R"ns(
public delegate void Handler(int x);

class Program
{
    public static int Result = 0;
    public event Handler Changed
    {
        add { Result += 1; }
        remove { Result += 2; }
    }

    public static void Handle(int x)
    {
    }

    public static int Main()
    {
        raii Program p = new Program();
        p.Changed += Handle;
        p.Changed -= Handle;
        return Result;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T09CustomEvent.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-341 a custom event with add/remove must parse and compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 3,
                "TL-LANG-341 add (+=1) then remove (+=2) must invoke the custom accessors: " +
                    result.error + result.output);
        }});
}

void registerNullConditionalCompoundCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-342", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "null-conditional", "compound", "assignment"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T10NullCondCompound", "Exe", R"ns(
class Box { public int Value; }
class Program
{
    private static int ReceiverEvaluations;
    private static int RhsEvaluations;

    private static Box Receiver(Box value)
    {
        ReceiverEvaluations += 1;
        return value;
    }

    private static int Addend(int value)
    {
        RhsEvaluations += 1;
        return value;
    }

    public static int Main()
    {
        Box b = null;
        int skipped = (Receiver(b)?.Value += Addend(1));
        raii Box c = new Box();
        c.Value = 10;
        int stored = (Receiver(c)?.Value += Addend(5));
        if (skipped != 0) return 100;
        if (stored != 15 || c.Value != 15) return 101;
        if (ReceiverEvaluations != 2 || RhsEvaluations != 1) return 102;
        return c.Value;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T10NullCondCompound.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-342 obj?.Field += x must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 15,
                "TL-LANG-342 c?.Value += 5 must store (and b?.Value skip on null): " +
                    result.error + result.output);
        }});
}

void registerNullConditionalIndexDiagnosticCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-343", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "null-conditional", "index", "diagnostic"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T10NullCondIndex", "Exe", R"ns(
class Box
{
    public int Value;
}
class Program
{
    public static int Main()
    {
        raii Box b = new Box();
        return b?[0];
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T10NullCondIndex.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(
                (emitted.error + emitted.output).find("null-conditional index access") !=
                    std::string::npos,
                "TL-LANG-343 obj?[i] must report a clear diagnostic: " +
                    emitted.error + emitted.output);
        }});
}

void registerClassPrimaryConstructorCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-344", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "class", "primary", "constructor"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T09ClassPrimaryCtor", "Exe", R"ns(
class Point(int x, int y)
{
    public int Sum()
    {
        return x + y;
    }
}

class Program
{
    public static int Main()
    {
        raii Point p = new Point(3, 4);
        return p.Sum();
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T09ClassPrimaryCtor.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-344 a class primary constructor must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 7,
                "TL-LANG-344 Point(3, 4).Sum() must be 7: " + result.error + result.output);
        }});
}

void registerNullConditionalIndexReadCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-345", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "null-conditional", "index", "read"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T10NullCondIndexRead", "Exe", R"ns(
class Program
{
    private static int ReceiverEvaluations;
    private static int IndexEvaluations;
    private static int RhsEvaluations;

    private static string[] Receiver(string[] value)
    {
        ReceiverEvaluations += 1;
        return value;
    }

    private static int NextIndex()
    {
        IndexEvaluations += 1;
        return 1;
    }

    private static string NextValue(string value)
    {
        RhsEvaluations += 1;
        return value;
    }

    public static int Main()
    {
        string[] arr = null;
        string skippedStore = (Receiver(arr)?[NextIndex()] = NextValue("x"));
        string skippedLoad = Receiver(arr)?[NextIndex()];
        string[] arr2 = new string[2];
        string stored = (Receiver(arr2)?[NextIndex()] = NextValue("hello"));
        string y = Receiver(arr2)?[NextIndex()];
        if (skippedStore != null || skippedLoad != null) return 100;
        if (stored.Length != 5 || y.Length != 5) return 101;
        if (ReceiverEvaluations != 4 || IndexEvaluations != 2 || RhsEvaluations != 1) return 102;
        return y.Length;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T10NullCondIndexRead.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-345 arr?[i] must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 5,
                "TL-LANG-345 arr2?[1] must load (arr?[0] skip on null): " +
                    result.error + result.output);
        }});
}

void registerNullConditionalStringConcatCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-346", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "null-conditional", "compound", "string", "assignment"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T10NullCondStringConcat", "Exe", R"ns(
class Box { public owned string Name; }
class Program
{
    private static int ReceiverEvaluations;
    private static int RhsEvaluations;

    private static Box NullReceiver(Box value)
    {
        ReceiverEvaluations += 1;
        return value;
    }

    private static borrowed Box Receiver(borrowed Box value)
    {
        ReceiverEvaluations += 1;
        return value;
    }

    private static string Suffix(string value)
    {
        RhsEvaluations += 1;
        return value;
    }

    public static int Main()
    {
        Box b = null;
        string skipped = (NullReceiver(b)?.Name += Suffix("x"));
        raii Box c = new Box();
        string first = (c?.Name += Suffix("hello"));
        string stored = (Receiver(c)?.Name += Suffix(" world"));
        if (skipped != null) return 100;
        if (first.Length != 5 || stored.Length != 11 || c.Name.Length != 11) return 101;
        if (ReceiverEvaluations != 2 || RhsEvaluations != 2) return 102;
        return c.Name.Length;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T10NullCondStringConcat.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-346 obj?.Name += \"x\" must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 11,
                "TL-LANG-346 owned c.Name += \" world\" must return/store length 11 and skip the null RHS: " +
                    result.error + result.output);
        }});
}

void registerCollectionExpressionCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-347", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "collection", "expression", "array"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T09CollectionExpression", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        int[] a = [1, 2, 3];
        var b = [4, 5];
        var d = [1, 2.0];
        return a[0] + a[1] + a[2] + b[0] + b[1] + (int)(d[0] + d[1]);
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T09CollectionExpression.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-347 int[] a = [1,2,3] must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 18,
                "TL-LANG-347 sum with heterogeneous [1,2.0] (common type) must be 18: " +
                    result.error + result.output);
        }});
}

void registerEmptyCollectionExpressionDiagnosticCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-348", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "collection", "expression", "empty", "diagnostic"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T09EmptyCollection", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        var a = [];
        return 0;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T09EmptyCollection.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(
                (emitted.error + emitted.output).find("requires a native array target type") !=
                    std::string::npos,
                "TL-LANG-348 an empty collection expression without a target must diagnose: " +
                    emitted.error + emitted.output);
        }});
}

void registerCollectionSpreadCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-349", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "collection", "expression", "spread", "assignment"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T09CollectionSpread", "Exe", R"ns(
class Program
{
    private static int Steps;

    private static int Mark(int digit, int value)
    {
        Steps = Steps * 10 + digit;
        return value;
    }

    private static int[] MarkArray(int digit, int first, int second)
    {
        Steps = Steps * 10 + digit;
        return [first, second];
    }

    public static int Main()
    {
        int[] b = [Mark(1, 9), ..MarkArray(2, 1, 2), Mark(3, 8),
                   ..MarkArray(4, 3, 4), Mark(5, 7)];
        if (Steps != 12345 || b.Length != 7) { return 100; }
        return b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6];
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T09CollectionSpread.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-349 ordered literal/spread segments must compile: " +
                    emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 34,
                "TL-LANG-349 segments must evaluate once in order and preserve all values: " +
                    result.error + result.output);

            const std::filesystem::path negativeProject = products.writeProject(
                "T09CollectionSpreadTypeMismatch", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        int[] integers = [1, 2];
        long[] longs = [3, 4];
        int[] invalid = [..integers, 5, ..longs];
        return invalid.Length;
    }
}
)ns");
            const external_program_support::Result rejected = emitT01Product(
                L"--emit-project-oop-exe", negativeProject,
                products.directory() / "T09CollectionSpreadTypeMismatch.exe",
                products.directory());
            context.expect(rejected.started && rejected.exitCode != 0 &&
                    (rejected.error + rejected.output).find(
                        "spread element type 'builtin:long' must exactly match target element "
                        "type 'builtin:int'") != std::string::npos,
                "TL-LANG-349 incompatible spread element types must be rejected: " +
                    rejected.error + rejected.output);
        }});
}

void registerNameOfGenericMethodCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-350", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "nameof", "generic", "method"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T10NameOfGenericMethod", "Exe", R"ns(
class Program
{
    public static void Method<T>()
    {
    }

    public static int Main()
    {
        string n = nameof(Method<int>);
        return n.Length;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T10NameOfGenericMethod.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-350 nameof(Method<int>) must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 6,
                "TL-LANG-350 nameof(Method<int>) must be \"Method\" (length 6): " +
                    result.error + result.output);

            const std::filesystem::path invalidProject = products.writeProject(
                "T10NameOfGenericMethodArity", "Exe", R"ns(
class Program
{
    public static void Method<T>() { }
    public static int Main()
    {
        string n = nameof(Method<int, int>);
        return n.Length;
    }
}
)ns");
            const std::filesystem::path invalidExecutable =
                products.directory() / "T10NameOfGenericMethodArity.exe";
            const external_program_support::Result invalid = emitT01Product(
                L"--emit-project-oop-exe", invalidProject, invalidExecutable,
                products.directory());
            const std::string invalidEvidence = invalid.error + invalid.output;
            context.expect(invalid.started && invalid.exitCode != 0 &&
                    invalidEvidence.find("accepts 2 generic argument(s)") != std::string::npos,
                "TL-LANG-350 nameof must reject an invalid generic method arity: " +
                    invalidEvidence);
        }});
}

void registerStaticLocalFunctionCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-351", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "local", "function", "static"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T04StaticLocalFunction", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        static int Twice(int n)
        {
            return n * 2;
        }
        int staticNamed(int n)
        {
            return n + 1;
        }
        return staticNamed(Twice(20)) + 1;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T04StaticLocalFunction.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-351 a static local function must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-351 static must be structural and an ordinary local function name may "
                "contain 'static': " + result.error + result.output);
        }});
}

void registerGenericLocalFunctionCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-352", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "local", "function", "generic"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T04GenericLocalFunction", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        static T Identity<T>(T x)
        {
            return x;
        }
        return Identity<int>(42);
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T04GenericLocalFunction.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-352 a generic local function must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 42,
                "TL-LANG-352 Identity<int>(42) must be 42: " + result.error + result.output);
        }});
}

void registerNameOfQualifiedGenericCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-353", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "nameof", "qualified", "generic", "type"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T10NameOfQualifiedGeneric", "Exe", R"ns(
namespace Foo
{
    class Bar<T>
    {
    }
}

class Program
{
    public static int Main()
    {
        string n = nameof(Foo.Bar<int>);
        return n.Length;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T10NameOfQualifiedGeneric.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-353 nameof(Foo.Bar<int>) must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 3,
                "TL-LANG-353 nameof(Foo.Bar<int>) must be \"Bar\" (length 3): " +
                    result.error + result.output);

            const std::filesystem::path invalidProject = products.writeProject(
                "T10NameOfUnknownSymbol", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        string n = nameof(MissingType);
        return n.Length;
    }
}
)ns");
            const std::filesystem::path invalidExecutable =
                products.directory() / "T10NameOfUnknownSymbol.exe";
            const external_program_support::Result invalid = emitT01Product(
                L"--emit-project-oop-exe", invalidProject, invalidExecutable,
                products.directory());
            const std::string invalidEvidence = invalid.error + invalid.output;
            context.expect(invalid.started && invalid.exitCode != 0 &&
                    invalidEvidence.find("unknown identifier 'MissingType'") != std::string::npos,
                "TL-LANG-353 nameof must reject an unresolved symbol: " + invalidEvidence);
        }});
}

void registerSingleSpreadCollectionCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-354", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "collection", "expression", "spread", "single"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T09SingleSpread", "Exe", R"ns(
class Program
{
    private static int Evaluations;

    private static int[] Source(int[] value)
    {
        Evaluations += 1;
        return value;
    }

    public static int Main()
    {
        int[] a = [1, 2, 3];
        int[] b = [..Source(a)];
        b[0] = 9;
        if (a[0] != 1) return 100;
        a[1] = 8;
        if (b[1] != 2) return 101;
        if (Evaluations != 1) return 102;
        if (a.Length != 3 || b.Length != 3) return 103;
        return b[0] + b[1] + b[2] + a[0] + a[1] + a[2];
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T09SingleSpread.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-354 [..a] single spread must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 26,
                "TL-LANG-354 single spread must evaluate once and copy independently: " +
                    result.error + result.output);
        }});
}

void registerStaticLocalCaptureRejectionCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-355", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "local", "function", "static", "capture", "diagnostic"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T04StaticLocalCapture", "Exe", R"ns(
class Program
{
    public static int Main()
    {
        int x = 42;
        static int Get()
        {
            return x;
        }
        return Get();
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T04StaticLocalCapture.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            const std::string evidence = emitted.error + emitted.output;
            context.expect(
                emitted.started && emitted.exitCode != 0 &&
                    evidence.find(
                        "a static local function cannot capture variables from the enclosing scope") !=
                        std::string::npos,
                "TL-LANG-355 must fail with the specific static-local capture diagnostic: " +
                    evidence);
        }});
}

void registerNameOfQualifiedGenericMethodCase(TestRegistry& registry)
{
    registry.add(TestCase{"TL-LANG-356", "CL-LANG-020", "Host.Language", "integration", "PR",
        {"language", "nameof", "qualified", "generic", "method"},
        [](TestContext& context) {
            TemporaryT01Products products;
            const std::filesystem::path project = products.writeProject(
                "T10NameOfQualifiedGenericMethod", "Exe", R"ns(
namespace Foo
{
    class Bar
    {
        public static void Method<T>()
        {
        }
    }
}

class Program
{
    public static int Main()
    {
        string n = nameof(Foo.Bar.Method<int>);
        return n.Length;
    }
}
)ns");
            const std::filesystem::path executable =
                products.directory() / "T10NameOfQualifiedGenericMethod.exe";
            const external_program_support::Result emitted = emitT01Product(
                L"--emit-project-oop-exe", project, executable, products.directory());
            context.expect(emitted.started && emitted.exitCode == 0 && emitted.error.empty(),
                "TL-LANG-356 nameof(Foo.Bar.Method<int>) must compile: " + emitted.error + emitted.output);
            if (!emitted.started || emitted.exitCode != 0) return;
            const auto result = external_program_support::run(
                executable, {}, {}, products.directory());
            context.expect(result.started && result.error.empty() && result.exitCode == 6,
                "TL-LANG-356 nameof(Foo.Bar.Method<int>) must be \"Method\" (length 6): " +
                    result.error + result.output);

            const std::filesystem::path invalidProject = products.writeProject(
                "T10NameOfInaccessibleMember", "Exe", R"ns(
namespace HiddenApi
{
    class Gate
    {
        private static int Secret;
    }
}

class Program
{
    public static int Main()
    {
        string n = nameof(HiddenApi.Gate.Secret);
        return n.Length;
    }
}
)ns");
            const std::filesystem::path invalidExecutable =
                products.directory() / "T10NameOfInaccessibleMember.exe";
            const external_program_support::Result invalid = emitT01Product(
                L"--emit-project-oop-exe", invalidProject, invalidExecutable,
                products.directory());
            const std::string invalidEvidence = invalid.error + invalid.output;
            context.expect(invalid.started && invalid.exitCode != 0 &&
                    invalidEvidence.find("not accessible") != std::string::npos,
                "TL-LANG-356 nameof must reject an inaccessible member: " + invalidEvidence);
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
    registerListPatternStackEscapeCase(registry);
    registerListPatternStringCase(registry);
    registerListPatternArrayCase(registry);
    registerListPatternInlineArrayCase(registry);
    registerListPatternSpanCase(registry);
    registerListPatternReadOnlySpanCase(registry);
    registerListPatternStringViewCase(registry);
    registerListPatternProtocolCase(registry);
    registerListPatternUtf8StringCase(registry);
    registerListPatternAliasingCase(registry);
    registerListPatternNoHeapCase(registry);
    registerPropertyPatternCase(registry);
    registerListExhaustivenessCase(registry);
    registerPatternBindingModeCase(registry);
    registerPatternDiagnosticsCase(registry);
    registerLambdaNaturalTypeCase(registry);
    registerLambdaParameterModifierCase(registry);
    registerLambdaDefaultParamCase(registry);
    registerStaticLambdaCase(registry);
    registerMethodGroupNaturalTypeCase(registry);
    registerMethodGroupRankingCase(registry);
    registerClosureErrorCase(registry);
    registerClosureErrorThrowCase(registry);
    registerClosureDropOracleCase(registry);
    registerExtensionMethodCase(registry);
    registerGlobalNamespaceExtensionCase(registry);
    registerStaticExtensionMemberCase(registry);
    registerIteratorErrorSurfaceCase(registry);
    registerForeachTeardownCase(registry);
    registerRefPropertyCase(registry);
    registerRefPropertyWriteCase(registry);
    registerExtensionOperatorCase(registry);
    registerCollectionBuilderCase(registry);
    registerIntermodularExtensionOperatorCase(registry);
    registerInlineArrayConstGenericCase(registry);
    registerNullConditionalUserIndexCase(registry);
    registerOrphanCoherenceCase(registry);
    registerExtensionVisibilityCase(registry);
    registerTraitWitnessCase(registry);
    registerConstructorThrowsCase(registry);
    registerSpanViewConversionCase(registry);
    registerArrayToSpanConversionCase(registry);
    registerStringToStringViewCase(registry);
    registerParamsReadOnlySpanCase(registry);
    registerGenericExtensionDispatchCase(registry);
    registerRefLikeConstraintCase(registry);
    registerRefLikeTraitWitnessCase(registry);
    registerPartialMethodCase(registry);
    registerPartialPropertyCase(registry);
    registerPartialConstructorCase(registry);
    registerPartialEventCase(registry);
    registerDecimalRepresentationCase(registry);
    registerDecimalLiteralCase(registry);
    registerDecimalManglingCase(registry);
    registerDecimalArithmeticCase(registry);
    registerDecimalMultiplyCase(registry);
    registerDecimalToIntCase(registry);
    registerDecimalDivideCase(registry);
    registerDecimalDivideMixedScaleCase(registry);
    registerDecimalConversionsCase(registry);
    registerDecimalNaNRejectionCase(registry);
    registerDecimalOverflowPolicyCase(registry);
    registerDecimalCheckedTrapCase(registry);
    registerDecimalUncheckedRejectCase(registry);
    registerScopeStackCase(registry);
    registerScopeEscapeRejectCase(registry);
    registerScopeFieldStoreRejectCase(registry);
    registerArenaAllocatorIAllocatorCase(registry);
    registerTrailingStorageCase(registry);
    registerTrailingStorageBoundsTrapCase(registry);
    registerTrailingStorageLengthNotCapacityCase(registry);
    registerPlacementRollbackCase(registry);
    registerAllocatorProvenanceDeallocateCase(registry);
    registerAllocatorProvenanceConstructorFailureCase(registry);
    registerAllocatorProvenanceCrossModuleCase(registry);
    registerAllocationFailureNullCase(registry);
    registerAllocationFailureThrowsCase(registry);
    registerAllocationFailurePanicCase(registry);
    registerAllocationFailureResultCase(registry);
    registerCostReportCase(registry);
    registerDecimalRemainderCase(registry);
    registerDecimalConstCase(registry);
    registerDecimalFfiCase(registry);
    registerDecimalRoundingCase(registry);
    registerProducerConsumerDecimalCase(registry);
    registerCrossModuleBorrowCase(registry);
    registerDecimalDebugCase(registry);
    registerDecimalTokenParityCase(registry);
    registerCSharp14MatrixCase(registry);
    registerBeefVariantCase(registry);
    registerAsyncOwnershipErrorCancelCase(registry);
    registerAsyncOwnedFinallyConditionalYieldCase(registry);
    registerPatternsGenericsMoveOnlySliceCase(registry);
    registerExtensionsPartialComptimeCase(registry);
    registerAllocatorsGeneratorsContainersNoheapCase(registry);
    registerHiddenCostAuditCase(registry);
    registerTypedDeriveFixtureCase(registry);
    registerDeriveCollisionCase(registry);
    registerDecimalOverflowCase(registry);
    registerDecimalMixedScaleCase(registry);
    registerDecimalMixedScaleRemCase(registry);
    registerAllocatorProtocolCase(registry);
    registerLocalFunctionCase(registry);
    registerLocalFunctionRecursionCase(registry);
    registerLambdaAttributeCase(registry);
    registerAsyncMethodMarkerCase(registry);
    registerAwaitDiagnosticCase(registry);
    registerAsyncAwaitRunCase(registry);
    registerAsyncYieldRunCase(registry);
    registerAsyncDropCase(registry);
    registerAsyncDropAfterSuspensionCase(registry);
    registerAsyncLivenessCase(registry);
    registerAsyncPreciseLivenessCase(registry);
    registerAsyncBorrowRejectCase(registry);
    registerAsyncBorrowParameterRejectCase(registry);
    registerAsyncThrowsCase(registry);
    registerAsyncThrowErrorCase(registry);
    registerAsyncRethrowCase(registry);
    registerAsyncNestedErrorPropagationCase(registry);
    registerAsyncFrameStorageHeapCase(registry);
    registerAsyncIntermodularFutureCase(registry);
    registerAsyncAwaitVariableCase(registry);
    registerAsyncFutureTypeCase(registry);
    registerExtensionPropertyCase(registry);
    registerPositionalPatternCase(registry);
    registerYieldDiagnosticCase(registry);
    registerYieldContextCase(registry);
    registerGeneratorRulesCase(registry);
    registerGeneratorConsumerCase(registry);
    registerGeneratorLivenessCase(registry);
    registerGeneratorParameterCaptureCase(registry);
    registerGeneratorRefItemCase(registry);
    registerGeneratorRefMutItemCase(registry);
    registerGeneratorPinnedFrameCase(registry);
    registerGeneratorTypedErrorCase(registry);
    registerGeneratorIntermodularCase(registry);
    registerAsyncGeneratorRefItemCase(registry);
    registerAsyncGeneratorRefBorrowRejectCase(registry);
    registerAsyncGeneratorMoveOnlyItemCase(registry);
    registerAsyncGeneratorCancellationCase(registry);
    registerTaskScopeCase(registry);
    registerSendSyncCase(registry);
    registerAsyncIteratorMetadataCase(registry);
    registerAsyncIteratorPdbCase(registry);
    registerPartialFailureCase(registry);
    registerUtf8LiteralCase(registry);
    registerInterpolationHandlerCase(registry);
    registerRefFieldEscapeCase(registry);
    registerGenericAttributeCase(registry);
    registerAllocatorSelectionCase(registry);
    registerGeneratorEarlyStopCase(registry);
    registerGeneratorDropCase(registry);
    registerAsyncGeneratorSyncCase(registry);
    registerAsyncGeneratorDemandCase(registry);
    registerAsyncGeneratorAwaitBodyCase(registry);
    registerAsyncGeneratorPostTerminalCase(registry);
    registerExtensionSetterMethodCase(registry);
    registerNameOfCase(registry);
    registerRawInterpolatedCase(registry);
    registerNoHeapEscapeCase(registry);
    registerInitAccessorCase(registry);
    registerRequiredMemberCase(registry);
    registerConstInterpolationCase(registry);
    registerPrimaryConstructorCase(registry);
    registerEventCase(registry);
    registerTargetTypedNewCase(registry);
    registerNullConditionalWriteCase(registry);
    registerCustomEventCase(registry);
    registerNullConditionalCompoundCase(registry);
    registerNullConditionalIndexDiagnosticCase(registry);
    registerNullConditionalIndexReadCase(registry);
    registerNullConditionalStringConcatCase(registry);
    registerClassPrimaryConstructorCase(registry);
    registerCollectionExpressionCase(registry);
    registerEmptyCollectionExpressionDiagnosticCase(registry);
    registerCollectionSpreadCase(registry);
    registerNameOfGenericMethodCase(registry);
    registerStaticLocalFunctionCase(registry);
    registerGenericLocalFunctionCase(registry);
    registerNameOfQualifiedGenericCase(registry);
    registerSingleSpreadCollectionCase(registry);
    registerStaticLocalCaptureRejectionCase(registry);
    registerNameOfQualifiedGenericMethodCase(registry);
}
}
