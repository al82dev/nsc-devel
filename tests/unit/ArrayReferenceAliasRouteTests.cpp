// Routes of the array lifetime rule (docs 04 and 08) that carry a reference without naming its owner:
// a borrowed alias stored into an element, an element copied into another element, and a local copied
// from an element. Before doc 20 the replay forgot the stored value and accepted a use after the owner's
// destruction (probes R02d, R02e, R09c and R26 in correcciones-revision\rutas-corregidas).
//
// Each case expects the destroyed-object error (E1), the unproven error (E2) or acceptance. Every route
// has a valid control that reads before the destruction, and the analysis must not reject it.
//
// Rutas de la regla de vida en arreglos que llevan una referencia sin nombrar a su dueno: un alias
// prestado guardado en un elemento, un elemento copiado a otro y un local copiado de un elemento.

#include "nativesharp/Frontend/ProjectCompilation.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::frontend::ProjectCompilation;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
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

constexpr std::string_view kDestroyed = "reference to a destroyed object cannot be used";
constexpr std::string_view kUnproven = "cannot prove that the referenced object is still alive";

constexpr std::string_view kPrelude =
    "class R { public int Read() { return 1; } } "
    "class D { public static void Drop(owned R v) { delete v; } } "
    "class H { public static void Store(R x, R[] refs) { refs[0] = x; } "
    "public static R First(R[] refs) { return refs[0]; } } ";

// `expected` empty: accepted with no lifetime error. Otherwise: rejected with that diagnostic.
// `alsoAccept` allows the other lifetime error for routes where either one is correct.
void expectRoute(
    TestContext& tests,
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view program,
    std::string_view expected,
    std::string_view alsoAccept = {})
{
    const std::filesystem::path directory = root / std::string(name);
    const std::filesystem::path project = directory / "P.nsproj";
    writeFile(project,
        "<Project><PropertyGroup><Name>P</Name><OutputType>Exe</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"Main.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", std::string(kPrelude) + std::string(program));
    ProjectCompilation compilation;
    std::string error;
    const bool built = compilation.compileProjectConstructions(project, error);
    const std::string text = diagnosticsText(compilation);
    bool matched = false;
    if (expected.empty())
        matched = built && text.find(kDestroyed) == std::string::npos && text.find(kUnproven) == std::string::npos;
    else
        matched = !built && (text.find(expected) != std::string::npos ||
                             (!alsoAccept.empty() && text.find(alsoAccept) != std::string::npos));
    tests.expect(matched, std::string(name) + ": " + (expected.empty() ? "accepted" : std::string(expected)) +
        " (" + error + text + ")");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_array_reference_alias_routes";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    // A borrowed alias stored directly into a tracked local array, then the owner is destroyed.
    expectRoute(tests, root, "AliasIntoLocalArray",
        "class Program { public static int Main() { R[] refs = new R[1]; R o = new R(); "
        "{ borrowed R b = o; refs[0] = b; } D.Drop(move(o)); int v = refs[0].Read(); delete[] refs; return v; } }",
        kDestroyed);
    expectRoute(tests, root, "AliasIntoLocalArrayLive",
        "class Program { public static int Main() { R[] refs = new R[1]; R o = new R(); "
        "{ borrowed R b = o; refs[0] = b; } int v = refs[0].Read(); D.Drop(move(o)); delete[] refs; return v; } }",
        {});
    // The same into a parameter array (read through the unknown receiver).
    expectRoute(tests, root, "AliasIntoParameterArray",
        "class Program { static int Run(R[] refs) { R o = new R(); { borrowed R b = o; refs[0] = b; } "
        "D.Drop(move(o)); return refs[0].Read(); } "
        "public static int Main() { R[] a = new R[1]; int v = Run(a); delete[] a; return v; } }",
        kUnproven);
    expectRoute(tests, root, "AliasIntoParameterArrayLive",
        "class Program { static int Run(R[] refs) { R o = new R(); { borrowed R b = o; refs[0] = b; } "
        "int v = refs[0].Read(); D.Drop(move(o)); return v; } "
        "public static int Main() { R[] a = new R[1]; int v = Run(a); delete[] a; return v; } }",
        {});
    // The same into a nested array element.
    expectRoute(tests, root, "AliasIntoNestedArray",
        "class Program { public static int Main() { R[][] grid = new R[1][]; grid[0] = new R[1]; R o = new R(); "
        "{ borrowed R b = o; grid[0][0] = b; } D.Drop(move(o)); return grid[0][0].Read(); } }",
        kUnproven);
    expectRoute(tests, root, "AliasIntoNestedArrayLive",
        "class Program { public static int Main() { R[][] grid = new R[1][]; grid[0] = new R[1]; R o = new R(); "
        "{ borrowed R b = o; grid[0][0] = b; } int v = grid[0][0].Read(); D.Drop(move(o)); return v; } }",
        {});
    // An alias of an alias.
    expectRoute(tests, root, "AliasOfAliasIntoLocalArray",
        "class Program { public static int Main() { R[] refs = new R[1]; R o = new R(); "
        "{ borrowed R b = o; borrowed R c = b; refs[0] = c; } D.Drop(move(o)); int v = refs[0].Read(); "
        "delete[] refs; return v; } }",
        kDestroyed, kUnproven);
    // An element copied from one tracked array into another.
    expectRoute(tests, root, "ElementCopy",
        "class Program { public static int Main() { R[] refs = new R[1]; R[] other = new R[1]; R o = new R(); "
        "H.Store(o, refs); other[0] = refs[0]; D.Drop(move(o)); int v = other[0].Read(); "
        "delete[] refs; delete[] other; return v; } }",
        kUnproven, kDestroyed);
    expectRoute(tests, root, "ElementCopyLive",
        "class Program { public static int Main() { R[] refs = new R[1]; R[] other = new R[1]; R o = new R(); "
        "H.Store(o, refs); other[0] = refs[0]; int v = other[0].Read(); D.Drop(move(o)); "
        "delete[] refs; delete[] other; return v; } }",
        {});
    // A call result (not modelled) stored into an element: it may name any tracked object.
    expectRoute(tests, root, "CallResultIntoElement",
        "class Program { public static int Main() { R[] refs = new R[1]; R[] other = new R[1]; R o = new R(); "
        "H.Store(o, refs); other[0] = H.First(refs); D.Drop(move(o)); int v = other[0].Read(); "
        "delete[] refs; delete[] other; return v; } }",
        kUnproven);
    expectRoute(tests, root, "CallResultIntoElementLive",
        "class Program { public static int Main() { R[] refs = new R[1]; R[] other = new R[1]; R o = new R(); "
        "H.Store(o, refs); other[0] = H.First(refs); int v = other[0].Read(); D.Drop(move(o)); "
        "delete[] refs; delete[] other; return v; } }",
        {});
    // Storing without using is not an error: the alias is stored, the owner destroyed, the element never read.
    expectRoute(tests, root, "AliasStoredNeverUsed",
        "class Program { public static int Main() { R[] refs = new R[1]; R o = new R(); "
        "{ borrowed R b = o; refs[0] = b; } D.Drop(move(o)); delete[] refs; return 0; } }",
        {});

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
