#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModelDumper.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::MethodCleanupContinuationKind;
using nativesharp::frontend::MethodCleanupRegionKind;
using nativesharp::frontend::MethodScopeKind;
using nativesharp::frontend::MethodStructuredExitKind;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(const std::filesystem::path& directory, std::string_view name, std::string_view source)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
}

bool hasDiagnosticText(const ProjectCompilation& compilation, std::string_view text)
{
    return std::ranges::any_of(
        compilation.diagnostics().diagnostics(),
        [text](const auto& diagnostic) { return diagnostic.message().find(text) != std::string::npos; });
}

constexpr std::string_view kDisposableCore = R"NS(
namespace NativeSharp.Core
{
    public interface IDisposable
    {
        void Dispose();
    }
}
using NativeSharp.Core;
)NS";

void testPositiveUsingModel(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "Positive";
    writeProject(directory, "Stage45UsingPositive", std::string(kDisposableCore) + R"NS(
public class Resource : IDisposable
{
    public void Dispose() { }
}

public class DerivedResource : Resource { }

class UsingCases
{
    public static void Normal(Resource input)
    {
        using (Resource resource = input)
        {
            Resource copy = resource;
        }
    }

    public static void Return(Resource input)
    {
        using (Resource resource = input)
        {
            return;
        }
    }

    public static void Nested(Resource first, Resource second)
    {
        using (Resource outer = first)
        {
            using (Resource inner = second)
            {
                return;
            }
        }
    }

    public static void InterfaceResource(IDisposable input)
    {
        using (IDisposable resource = input)
        {
            IDisposable copy = resource;
        }
    }

    public static void InheritedImplementation(DerivedResource input)
    {
        using (DerivedResource resource = input)
        {
            DerivedResource copy = resource;
        }
    }

    public static void UsingThenFinally(Resource input)
    {
        try
        {
            using (Resource resource = input)
            {
                return;
            }
        }
        finally
        {
            int cleanup = 1;
        }
    }

    public static void BreakAcross(Resource input, bool run)
    {
        while (run)
        {
            using (Resource resource = input)
            {
                break;
            }
        }
    }

    public static void ContinueAcross(Resource input, bool run)
    {
        while (run)
        {
            using (Resource resource = input)
            {
                continue;
            }
        }
    }

    public static void BreakInsideUsingLoop(Resource input, bool run)
    {
        using (Resource resource = input)
        {
            while (run)
            {
                break;
            }
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / "Stage45UsingPositive.nsproj", error);
    tests.expect(ok && error.empty(), "Stage 45.5 valid using/IDisposable sample binds without diagnostics");
    tests.expect(compilation.methodBodyModel() != nullptr,
        "Stage 45.5 exposes using bindings through the method-body model");
    if (!ok || compilation.methodBodyModel() == nullptr || compilation.memberModel() == nullptr ||
        compilation.nameLookup() == nullptr)
    {
        return;
    }

    const auto& model = *compilation.methodBodyModel();
    const auto usingBindings = model.usingBindings();
    tests.expect(usingBindings.size() == 10,
        "ten using declarations produce ten deterministic resource bindings");
    tests.expect(std::ranges::count_if(model.scopes(), [](const auto& scope) {
        return scope.kind == MethodScopeKind::Using;
    }) == 10, "each using declaration owns a lexical resource scope");
    tests.expect(std::ranges::count_if(model.cleanupRegions(), [](const auto& region) {
        return region.kind == MethodCleanupRegionKind::UsingDispose;
    }) == 10, "each using declaration creates one using-dispose cleanup region");
    tests.expect(std::ranges::count_if(model.cleanupRegions(), [](const auto& region) {
        return region.kind == MethodCleanupRegionKind::Finally;
    }) == 1, "the mixed using/finally sample retains its independent finally cleanup region");
    tests.expect(std::ranges::all_of(usingBindings, [](const auto& binding) {
        return binding.disposeContractMemberOrdinal != static_cast<std::size_t>(-1);
    }), "every using binding freezes the IDisposable.Dispose contract member at compile time");
    tests.expect(std::ranges::count_if(usingBindings, [](const auto& binding) {
        return binding.interfaceDispatch && !binding.disposeImplementationMemberOrdinal.has_value();
    }) == 1, "an IDisposable-typed resource records interface-slot dispatch rather than inventing a concrete target");
    tests.expect(std::ranges::count_if(usingBindings, [](const auto& binding) {
        return !binding.interfaceDispatch && binding.disposeImplementationMemberOrdinal.has_value();
    }) == 9, "class-typed resources resolve their concrete Dispose implementation member");
    tests.expect(std::ranges::any_of(usingBindings, [](const auto& binding) {
        return binding.resourceTypeKey.find("DerivedResource") != std::string::npos &&
            binding.disposeImplementationMemberOrdinal.has_value();
    }), "inherited IDisposable implementation is resolved for a derived resource type");
    tests.expect(std::ranges::all_of(model.cleanupRegions(), [](const auto& region) {
        return region.kind != MethodCleanupRegionKind::UsingDispose ||
            (region.resourceLocalOrdinal.has_value() && region.disposeContractMemberOrdinal.has_value());
    }), "using cleanup regions carry the exact resource local and Dispose contract metadata");
    tests.expect(std::ranges::any_of(model.nameBindings(), [](const auto& binding) {
        return binding.name == "resource" && binding.localOrdinal.has_value();
    }), "using resource names bind as ordinary locals inside their using body");

    const auto nestedReturn = std::ranges::find_if(model.structuredExits(), [&](const auto& exit) {
        if (exit.kind != MethodStructuredExitKind::Return || exit.cleanupRegionOrdinals.size() != 2)
        {
            return false;
        }
        const auto& first = model.cleanupRegions()[exit.cleanupRegionOrdinals[0]];
        const auto& second = model.cleanupRegions()[exit.cleanupRegionOrdinals[1]];
        return first.kind == MethodCleanupRegionKind::UsingDispose &&
            second.kind == MethodCleanupRegionKind::UsingDispose;
    });
    tests.expect(nestedReturn != model.structuredExits().end(),
        "nested using return unwinds resources inner-to-outer");

    const auto usingFinallyReturn = std::ranges::find_if(model.structuredExits(), [&](const auto& exit) {
        if (exit.kind != MethodStructuredExitKind::Return || exit.cleanupRegionOrdinals.size() != 2)
        {
            return false;
        }
        const auto& first = model.cleanupRegions()[exit.cleanupRegionOrdinals[0]];
        const auto& second = model.cleanupRegions()[exit.cleanupRegionOrdinals[1]];
        return first.kind == MethodCleanupRegionKind::UsingDispose &&
            second.kind == MethodCleanupRegionKind::Finally;
    });
    tests.expect(usingFinallyReturn != model.structuredExits().end(),
        "return from using inside try/finally disposes the resource before the enclosing finally");
    tests.expect(std::ranges::count_if(model.structuredExits(), [&](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Break && exit.cleanupRegionOrdinals.size() == 1 &&
            model.cleanupRegions()[exit.cleanupRegionOrdinals.front()].kind == MethodCleanupRegionKind::UsingDispose;
    }) == 1, "break crossing a using body records Dispose before the loop target");
    tests.expect(std::ranges::count_if(model.structuredExits(), [&](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Continue && exit.cleanupRegionOrdinals.size() == 1 &&
            model.cleanupRegions()[exit.cleanupRegionOrdinals.front()].kind == MethodCleanupRegionKind::UsingDispose;
    }) == 1, "continue crossing a using body records Dispose before the loop target");
    tests.expect(std::ranges::count_if(model.structuredExits(), [](const auto& exit) {
        return exit.kind == MethodStructuredExitKind::Break && exit.cleanupRegionOrdinals.empty();
    }) >= 1, "break targeting a loop wholly inside using does not dispose the outer resource early");
    tests.expect(std::ranges::count_if(model.cleanupContinuations(), [](const auto& continuation) {
        return continuation.kind == MethodCleanupContinuationKind::Normal;
    }) == 11, "all ten using cleanups plus the finally have explicit normal continuations");

    std::ostringstream dump;
    nativesharp::frontend::ProjectMethodBodyModelDumper{}.dump(
        model, *compilation.memberModel(), *compilation.nameLookup(), dump);
    const std::string text = dump.str();
    tests.expect(text.find("using-bindings=10") != std::string::npos,
        "method-body dumper freezes the using binding count");
    tests.expect(text.find("kind=using-dispose") != std::string::npos,
        "method-body dumper exposes using-dispose cleanup regions");
    tests.expect(text.find("dispose-contract=") != std::string::npos &&
            text.find("dispose-implementation=") != std::string::npos,
        "method-body dumper exposes compile-time Dispose resolution evidence");
    tests.expect(text.find("dispatch=interface") != std::string::npos,
        "method-body dumper distinguishes interface-slot Dispose dispatch");
}

void expectReject(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string source,
    std::string_view expected)
{
    const auto directory = root / std::string(name);
    writeProject(directory, name, source);
    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectMethodBodies(
        directory / (std::string(name) + ".nsproj"), error);
    tests.expect(!ok, std::string(name) + " is rejected by Stage 45.5 using sema");
    tests.expect(hasDiagnosticText(compilation, expected),
        std::string(name) + " reports the deterministic using/Dispose contract");
}

void testNegativeUsingModel(TestContext& tests, const std::filesystem::path& root)
{
    expectReject(tests, root, "MissingIDisposable", R"NS(
class Resource { public void Dispose() { } }
class Holder { public static void F(Resource input) { using (Resource resource = input) { } } }
)NS", "NativeSharp.Core.IDisposable must be declared as an interface");

    expectReject(tests, root, "BadIDisposableContract", R"NS(
namespace NativeSharp.Core { public interface IDisposable { int Dispose(); } }
using NativeSharp.Core;
class Resource : IDisposable { public int Dispose() { return 0; } }
class Holder { public static void F(Resource input) { using (Resource resource = input) { } } }
)NS", "must declare exactly one instance 'void Dispose()' contract");

    expectReject(tests, root, "StructuralDisposeOnly", std::string(kDisposableCore) + R"NS(
class Resource { public void Dispose() { } }
class Holder { public static void F(Resource input) { using (Resource resource = input) { } } }
)NS", "must implement NativeSharp.Core.IDisposable");

    expectReject(tests, root, "WrongImplementation", std::string(kDisposableCore) + R"NS(
class Resource : IDisposable { public int Dispose() { return 0; } }
class Holder { public static void F(Resource input) { using (Resource resource = input) { } } }
)NS", "does not implement interface method 'Dispose()'");

    expectReject(tests, root, "BuiltinResource", std::string(kDisposableCore) + R"NS(
class Holder { public static void F(int value) { using (int resource = value) { } } }
)NS", "using resource type 'builtin:int' must implement NativeSharp.Core.IDisposable");

    expectReject(tests, root, "InitializerMismatch", std::string(kDisposableCore) + R"NS(
class Resource : IDisposable { public void Dispose() { } }
class Other { }
class Holder { public static void F(Other value) { using (Resource resource = value) { } } }
)NS", "cannot initialize using resource 'resource'");

    expectReject(tests, root, "SelfReferenceInitializer", std::string(kDisposableCore) + R"NS(
class Resource : IDisposable { public void Dispose() { } }
class Holder { public static void F() { using (Resource resource = resource) { } } }
)NS", "unknown identifier 'resource'");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage45_5_using_disposable";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testPositiveUsingModel(tests, root);
    testNegativeUsingModel(tests, root);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
