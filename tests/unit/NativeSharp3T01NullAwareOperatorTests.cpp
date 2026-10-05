#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <span>
#include <string>
#include <string_view>

namespace
{
using nativesharp::driver::Driver;
using nativesharp::ExitCode;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

void writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
}

ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}

bool rejected(
    const std::filesystem::path& directory,
    std::string_view source,
    std::string& diagnostic)
{
    writeProject(directory, "Rejected", source);
    const std::string project = (directory / "Rejected.nsproj").string();
    const std::array<std::string_view, 3> arguments = {
        "--check-project-oop-hir", project, "-O0"};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    return result != ExitCode::Success;
}

std::size_t occurrences(std::string_view text, std::string_view needle)
{
    std::size_t count = 0;
    for (std::size_t at = 0; (at = text.find(needle, at)) != std::string_view::npos;
         at += needle.size())
    {
        ++count;
    }
    return count;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t01_null_aware_operators";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const std::filesystem::path positive = root / "positive";
    writeProject(positive, "NullAware", R"NS(
namespace NativeSharp.Core
{
    public struct Optional<T>
    {
        private T value;
        private bool present;
        public Optional(T item) { value = item; present = true; }
        public bool HasValue() { return present; }
        public T UncheckedValue() { return value; }
    }
}

namespace NullAware
{
    using NativeSharp.Core;

    public interface ILink
    {
        ILink Step(int marker);
    }

    public class Link : ILink
    {
        public static int calls = 0;
        public ILink Step(int marker)
        {
            calls = calls + marker;
            return this;
        }
    }

    public class Node
    {
        private Node next;
        public Node Link;
        public static int getterCalls = 0;
        public static int methodCalls = 0;

        public Node(Node value) { next = value; Link = value; }
        public Node Next { get { getterCalls = getterCalls + 1; return next; } }
        public virtual Node Advance(int marker)
        {
            methodCalls = methodCalls + marker;
            return next;
        }
    }

    public class Program
    {
        private static Node probeValue = null;
        private static int receiverCalls = 0;
        private static int argumentTrace = 0;
        private static int fallbackCalls = 0;
        private static int fallbackValue = 42;

        private static Node Probe()
        {
            receiverCalls = receiverCalls + 1;
            return probeValue;
        }

        private static int Mark(int value)
        {
            argumentTrace = argumentTrace * 10 + value;
            return value;
        }

        private static int* FallbackPointer()
        {
            fallbackCalls = fallbackCalls + 1;
            unsafe("the static fallback outlives the returned pointer")
            {
                return &fallbackValue;
            }
        }

        private static int FallbackValue()
        {
            fallbackCalls = fallbackCalls + 1;
            return 42;
        }

        public static int Main()
        {
            Node tail = new Node(null);
            Node head = new Node(tail);

            probeValue = head;
            Node propertyValue = Probe()?.Next;
            if (propertyValue != tail || receiverCalls != 1 || Node.getterCalls != 1) { return 1; }

            probeValue = null;
            propertyValue = Probe()?.Next;
            if (propertyValue != null || receiverCalls != 2 || Node.getterCalls != 1) { return 2; }

            probeValue = head;
            Node methodValue = Probe()?.Advance(Mark(2));
            if (methodValue != tail || receiverCalls != 3 || argumentTrace != 2 || Node.methodCalls != 2)
                { return 3; }

            probeValue = null;
            methodValue = Probe()?.Advance(Mark(9));
            if (methodValue != null || receiverCalls != 4 || argumentTrace != 2 || Node.methodCalls != 2)
                { return 4; }

            probeValue = head;
            Node chained = Probe()?.Advance(Mark(3))?.Next;
            if (chained != null || receiverCalls != 5 || argumentTrace != 23 ||
                Node.methodCalls != 5 || Node.getterCalls != 2) { return 5; }

            Node fieldValue = Probe()?.Link;
            if (fieldValue != tail || receiverCalls != 6) { return 10; }

            Link linkObject = new Link();
            ILink link = linkObject;
            ILink interfaceValue = link?.Step(Mark(4));
            if (interfaceValue == null || argumentTrace != 234 || Link.calls != 4) { return 11; }
            link = null;
            interfaceValue = link?.Step(Mark(9));
            if (interfaceValue != null || argumentTrace != 234 || Link.calls != 4) { return 12; }
            destroy linkObject;

            unsafe("the local pointer remains live throughout the coalescing checks")
            {
                int presentValue = 7;
                int* present = &presentValue;
                int* missing = null;
                if (*(present ?? FallbackPointer()) != 7 || fallbackCalls != 0) { return 6; }
                missing ??= FallbackPointer();
                if (*missing != 42 || fallbackCalls != 1) { return 7; }
            }

            Optional<int> some = new Optional<int>(42);
            Optional<int> none = new Optional<int>();
            if ((some ?? FallbackValue()) != 42 || fallbackCalls != 1) { return 8; }
            if ((none ?? FallbackValue()) != 42 || fallbackCalls != 2) { return 9; }

            destroy head;
            destroy tail;
            return 42;
        }
    }
}
)NS");

    const std::string positiveProject = (positive / "NullAware.nsproj").string();
    const std::string executable = (positive / "NullAware.exe").string();
    const std::array<std::string_view, 5> emitArguments = {
        "--emit-project-oop-exe", positiveProject, "-O0", "-o", executable};
    std::string output;
    std::string error;
    const ExitCode emitted = runDriver(emitArguments, output, error);
    tests.expect(
        emitted == ExitCode::Success,
        "O0 compiles pointer/reference/Optional null-aware operators with NSLink (" + error + output + ")");
    if (emitted == ExitCode::Success)
    {
        tests.expect(
            std::system(("\"" + executable + "\"").c_str()) == 42,
            "coalescing, conditional assignment, property/method access, and chains preserve laziness");
    }

    const std::array<std::string_view, 3> hirArguments = {
        "--dump-project-oop-hir", positiveProject, "-O0"};
    output.clear();
    error.clear();
    const bool hirDumped = runDriver(hirArguments, output, error) == ExitCode::Success;
    tests.expect(
        hirDumped && occurrences(output, "null-conditional=true") >= 2U &&
            output.find("null-conditional-field-value") != std::string::npos &&
            output.find("optional-coalescing") != std::string::npos &&
            output.find("null-coalescing-assignment") != std::string::npos,
        "HIR publishes conditional calls, fields, Optional coalescing, and conditional stores (" +
            error + " null-calls=" +
            std::to_string(occurrences(output, "null-conditional=true")) +
            " field=" + std::to_string(output.find("null-conditional-field-value") != std::string::npos) +
            " optional=" + std::to_string(output.find("optional-coalescing") != std::string::npos) +
            " store=" + std::to_string(output.find("null-coalescing-assignment") != std::string::npos) + ")");

    const std::array<std::string_view, 3> llvmArguments = {
        "--dump-project-oop-llvm", positiveProject, "-O0"};
    output.clear();
    error.clear();
    const bool llvmDumped = runDriver(llvmArguments, output, error) == ExitCode::Success;
    tests.expect(
        llvmDumped && output.find("nullcall.hasreceiver") != std::string::npos &&
            output.find("nullcall.invoke") != std::string::npos &&
            output.find("optional.hasvalue") != std::string::npos &&
            output.find("coalesce.store.fallback") != std::string::npos &&
            output.find("box") == std::string::npos,
        "LLVM O0 uses explicit lazy branches and introduces no boxing path (" + error + ")");

    std::string diagnostic;
    const bool valueMethodRejected = rejected(root / "value-method", R"NS(
class Node { public int Value() { return 42; } }
class Program
{
    public static int Main()
    {
        Node node = null;
        int value = node?.Value();
        return value;
    }
}
)NS", diagnostic);
    tests.expect(
        valueMethodRejected && diagnostic.find("Optional<T>") != std::string::npos,
        "?. rejects value-returning calls until an Optional<T> result carrier is explicit (" +
            diagnostic + ")");

    diagnostic.clear();
    const bool lookalikeRejected = rejected(root / "lookalike", R"NS(
struct Maybe<T>
{
    private T stored;
    public bool HasValue() { return true; }
    public T UncheckedValue() { return stored; }
}
class Program
{
    public static int Main()
    {
        Maybe<int> value = new Maybe<int>();
        return value ?? 42;
    }
}
)NS", diagnostic);
    tests.expect(
        lookalikeRejected && diagnostic.find("supported Optional<T> carrier") != std::string::npos,
        "lookalike methods cannot overload ?? or acquire the sealed Core Optional protocol (" +
            diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
