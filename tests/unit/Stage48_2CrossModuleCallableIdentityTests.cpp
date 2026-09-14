#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::interop::LibraryMetadata;
using nativesharp::interop::LibraryMetadataCodec;
using nativesharp::interop::LibraryMetadataMember;
using nativesharp::tests::TestContext;

std::string utf8Path(const std::filesystem::path& path)
{
    const std::u8string value = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

ExitCode emitLibrary(
    const std::filesystem::path& project,
    const std::filesystem::path& destination,
    std::string& error)
{
    Driver driver;
    std::ostringstream output;
    std::ostringstream diagnostics;
    const std::string projectText = utf8Path(project);
    const std::string destinationText = utf8Path(destination);
    const std::array<std::string_view, 5> arguments = {
        "-O0", "--emit-project-oop-lib", projectText, "-o", destinationText};
    const ExitCode result = driver.run(arguments, output, diagnostics);
    error = diagnostics.str();
    return result;
}

void writeProject(
    const std::filesystem::path& directory,
    std::string_view module,
    std::string_view source,
    std::string_view references = {})
{
    writeFile(directory / (std::string(module) + ".ns"), source);
    writeFile(
        directory / (std::string(module) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(module) +
            "</Name><OutputType>StaticLibrary</OutputType></PropertyGroup><ItemGroup>"
            "<Compile Include=\"**/*.ns\" />" + std::string(references) +
            "</ItemGroup></Project>");
}

bool buildModule(
    TestContext& tests,
    const std::filesystem::path& directory,
    std::string_view module)
{
    std::string error;
    const ExitCode result = emitLibrary(
        directory / (std::string(module) + ".nsproj"),
        directory / (std::string(module) + ".lib"),
        error);
    tests.expect(result == ExitCode::Success,
        "O0 module " + std::string(module) + " emits library and NSMETA (" + error + ")");
    return result == ExitCode::Success;
}

std::string reference(
    const std::filesystem::path& directory,
    std::string_view module)
{
    return "<LibraryReference Include=\"" +
        (directory / (std::string(module) + ".lib")).generic_string() +
        "\" Metadata=\"" +
        (directory / (std::string(module) + ".nsmeta")).generic_string() +
        "\" />";
}

void testCallableWithProviderOwnedGenericOfForeignType(
    TestContext& tests,
    const std::filesystem::path& root)
{
    const auto store = root / "Store";
    const auto ids = root / "Ids";
    const auto storage = root / "Storage";
    const auto core = root / "Core";
    const auto ordering = root / "Ordering";

    writeProject(store, "SpanStore", R"NS(
namespace Native.Store
{
    public struct Span<T>
    {
        public T* data;
        public int count;
    }
}

    public struct ReadOnlySpan<T>
    {
        public T* data;
        public int count;
    }
)NS");
    if (!buildModule(tests, store, "SpanStore")) return;

    writeProject(ids, "GraphIds", R"NS(
namespace Native.Graph
{
    public struct NodeId
    {
        public int value;
    }
}
)NS");
    if (!buildModule(tests, ids, "GraphIds")) return;

    writeProject(storage, "GraphStorage", R"NS(
using Native.Graph;

namespace Native.Graph.Storage
{
    public class AdjacencyGraph
    {
        public NodeId root;
    }
}
)NS", reference(ids, "GraphIds"));
    if (!buildModule(tests, storage, "GraphStorage")) return;

    writeProject(core, "GraphCore", R"NS(
using Native.Store;
using Native.Graph.Storage;

namespace Native.Graph
{
    public delegate void NodeVisitor(NodeId node);

    public class Bfs
    {
        public static void Run(
            AdjacencyGraph graph,
            ReadOnlySpan<NodeId> roots,
            Span<int> queue,
            Span<int> distances,
            Span<NodeId> parents,
            NodeVisitor visitor)
        {
        }
    }
}
)NS", reference(store, "SpanStore") +
        reference(ids, "GraphIds") +
        reference(storage, "GraphStorage"));
    if (!buildModule(tests, core, "GraphCore")) return;

    LibraryMetadata metadata;
    LibraryMetadataCodec codec;
    std::string error;
    const bool read = codec.read(core / "GraphCore.nsmeta", metadata, error);
    tests.expect(read, "GraphCore NSMETA remains readable (" + error + ")");
    if (!read) return;
    const auto run = std::ranges::find_if(
        metadata.members,
        [](const LibraryMetadataMember& member) { return member.name == "Run"; });
    tests.expect(run != metadata.members.end() && !run->mangledName.empty(),
        "producer publishes Bfs.Run with a physical identity");
    if (run == metadata.members.end()) return;

    writeProject(ordering, "GraphOrdering", R"NS(
namespace Native.Graph.Ordering
{
    public class OrderingMarker
    {
        public static int Value() { return 42; }
    }
}
)NS", reference(store, "SpanStore") +
        reference(ids, "GraphIds") +
        reference(storage, "GraphStorage") +
        reference(core, "GraphCore"));

    error.clear();
    const ExitCode consumerResult = emitLibrary(
        ordering / "GraphOrdering.nsproj",
        ordering / "GraphOrdering.lib",
        error);
    tests.expect(
        consumerResult == ExitCode::Success,
        "consumer binds Bfs.Run across ReadOnlySpan<NodeId>, Span<NodeId>, repeated Span<int>, and callback (" +
            error + "; published=" + run->signatureKey + ")");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        "nativesharp_stage48_2_cross_module_callable_identity";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testCallableWithProviderOwnedGenericOfForeignType(tests, root);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
