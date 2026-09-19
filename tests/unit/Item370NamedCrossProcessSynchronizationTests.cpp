#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"

#include "../support/TestContext.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << contents;
}

ExitCode runDriver(std::span<const std::string_view> arguments, std::string& output, std::string& error)
{
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const ExitCode result = Driver{}.run(arguments, outputStream, errorStream);
    output = outputStream.str();
    error = errorStream.str();
    return result;
}

bool buildExecutable(
    const std::filesystem::path& project,
    const std::filesystem::path& executable,
    std::string& diagnostic)
{
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, "-O2", "-o", executableText};
    std::string output;
    std::string error;
    const bool ok = runDriver(arguments, output, error) == ExitCode::Success;
    diagnostic = error + output;
    return ok;
}
}

int main()
{
    TestContext tests;
    const auto sourceRoot = std::filesystem::path(NSC_SOURCE_DIR);
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item370_named_sync";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);

    const std::string threadingProject =
        (sourceRoot / "lib/NativeSharp.Core.Threading/NativeSharp.Core.Threading.nsproj").generic_string();
    const std::array<std::string_view, 2> dumpArguments = {
        "--dump-project-oop-llvm", threadingProject};
    std::string libraryIr;
    std::string libraryError;
    const bool libraryCompiled =
        runDriver(dumpArguments, libraryIr, libraryError) == ExitCode::Success;
    tests.expect(libraryCompiled,
        "the threading library compiles with named synchronization overloads (" + libraryError + ")");
    tests.expect(
        libraryIr.find("call i64 @__nativesharp_mutex_create_named(") != std::string::npos &&
            libraryIr.find("call i64 @__nativesharp_semaphore_create_named(") != std::string::npos &&
            libraryIr.find("call i64 @__nativesharp_event_create_named(") != std::string::npos,
        "all three named constructors lower to their dedicated runtime hooks");

    nativesharp::runtime::WindowsRuntimeBuilder runtimeBuilder;
    std::string runtimeError;
    auto runtimeModule = runtimeBuilder.buildSupport(runtimeError);
    tests.expect(runtimeModule != nullptr && runtimeError.empty(),
        "the runtime module builds with named kernel-object support (" + runtimeError + ")");
    if (runtimeModule != nullptr)
    {
        std::string verifyError;
        tests.expect(runtimeModule->verify(verifyError),
            "the runtime module with named synchronization verifies (" + verifyError + ")");
        const std::string runtimeIr = runtimeModule->text();
        tests.expect(
            runtimeIr.find("define i64 @__nativesharp_mutex_create_named(") != std::string::npos &&
                runtimeIr.find("define i64 @__nativesharp_semaphore_create_named(") != std::string::npos &&
                runtimeIr.find("define i64 @__nativesharp_event_create_named(") != std::string::npos,
            "the runtime exports named mutex, semaphore, and event creation");
        tests.expect(runtimeIr.find("ansi_buffer") != std::string::npos,
            "NativeSharp object names are copied to null-terminated ANSI buffers");
    }

    const std::string uniqueName = "NativeSharp_Item370_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto waiterDirectory = root / "waiter";
    const auto signalerDirectory = root / "signaler";
    const std::string projectPrefix =
        "<Project><PropertyGroup><Name>Item370</Name></PropertyGroup><ItemGroup>"
        "<Compile Include=\"Main.ns\"/><ProjectReference Include=\"" + threadingProject +
        "\"/></ItemGroup></Project>";
    writeFile(waiterDirectory / "Waiter.nsproj", projectPrefix);
    writeFile(signalerDirectory / "Signaler.nsproj", projectPrefix);
    writeFile(waiterDirectory / "Main.ns",
        "using NativeSharp.Core.Threading; public class Program { public static int Main() { "
        "raii Event ready = new Event(false, false, \"" + uniqueName +
        "\"); return ready.Wait(15000) ? 42 : 1; } }");
    writeFile(signalerDirectory / "Main.ns",
        "using NativeSharp.Core.Threading; public class Program { public static int Main() { "
        "raii Event ready = new Event(false, false, \"" + uniqueName +
        "\"); ready.Set(); return 42; } }");

    const auto waiterExecutable = waiterDirectory / "Waiter.exe";
    const auto signalerExecutable = signalerDirectory / "Signaler.exe";
    std::string waiterDiagnostic;
    std::string signalerDiagnostic;
    const bool waiterBuilt = buildExecutable(
        waiterDirectory / "Waiter.nsproj", waiterExecutable, waiterDiagnostic);
    const bool signalerBuilt = buildExecutable(
        signalerDirectory / "Signaler.nsproj", signalerExecutable, signalerDiagnostic);
    tests.expect(waiterBuilt, "the cross-process waiter builds (" + waiterDiagnostic + ")");
    tests.expect(signalerBuilt, "the cross-process signaler builds (" + signalerDiagnostic + ")");

    if (waiterBuilt && signalerBuilt)
    {
        auto waiter = std::async(std::launch::async, [&waiterExecutable]() {
            return std::system(("\"" + waiterExecutable.string() + "\"").c_str());
        });
        bool waiterReady = false;
        const auto readinessDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < readinessDeadline)
        {
            if (HANDLE event = OpenEventA(SYNCHRONIZE, FALSE, uniqueName.c_str()))
            {
                CloseHandle(event);
                waiterReady = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        tests.expect(waiterReady,
            "the waiter creates the named event before the signaler starts");
        const int signalerExit = waiterReady
            ? std::system(("\"" + signalerExecutable.string() + "\"").c_str())
            : 1;
        const int waiterExit = waiter.get();
        tests.expect(signalerExit == 42,
            "the second process opens and signals the named event");
        tests.expect(waiterExit == 42,
            "the first process observes the signal through the shared named event");
    }

    const int result = tests.result();
    if (result == 0) std::filesystem::remove_all(root, filesystemError);
    return result;
}
