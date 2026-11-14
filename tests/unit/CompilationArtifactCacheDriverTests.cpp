#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#if defined(_WIN32)
#include "nativesharp/Frontend/ClassReceiverContractManifest.h"
#include "nativesharp/LLVM/ProjectOopObjectGenerator.h"
#include "../../src/Driver/ProductContractLock.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <Windows.h>
#endif

#include "../support/TestContext.h"

#include <sstream>
#include <string_view>
#include <vector>

namespace
{
nativesharp::ExitCode run(
    const std::vector<std::string_view>& arguments,
    std::ostringstream& output,
    std::ostringstream& error)
{
    return nativesharp::driver::Driver{}.run(arguments, output, error);
}

#if defined(_WIN32)
bool receiverContractCacheRegression(nativesharp::tests::TestContext& tests)
{
    namespace fs = std::filesystem;
    using nativesharp::ExitCode;
    using namespace nativesharp::frontend;
    const fs::path root = fs::temp_directory_path() /
        ("nsc-receiver-cache-" + std::to_string(GetCurrentProcessId()) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    const auto write = [](const fs::path& path, std::string_view text) {
        std::ofstream file(path, std::ios::binary); file << text; return bool(file);
    };
    const auto invoke = [](const std::vector<std::string>& args, std::string& out, std::string& err) {
        std::vector<std::string_view> views; views.reserve(args.size());
        for (const auto& arg : args) views.push_back(arg);
        std::ostringstream output, error;
        const auto result = run(views, output, error);
        out = output.str(); err = error.str(); return result;
    };
    const auto check = [&](bool condition, const std::string& description) {
        tests.expect(condition, description); return condition;
    };
    const fs::path producerProject = root / "Producer.nsproj";
    const fs::path library = root / "Producer.lib";
    const fs::path metadata = root / "Producer.nsmeta";
    const fs::path sidecar = root / "Producer.lib.nsreceiver";
    const fs::path lock = root / "Producer.lib.nsreceiver.lock";
    const fs::path consumerProject = root / "Consumer.nsproj";
    const fs::path executable = root / "Consumer.exe";
    const std::string cache = "--artifact-cache=" + (root / "cache").string();
    if (!check(write(producerProject,
            "<Project><PropertyGroup><Name>ReceiverProducer</Name><OutputType>Library</OutputType>"
            "</PropertyGroup><ItemGroup><Compile Include=\"Producer.ns\" /></ItemGroup></Project>") &&
        write(root / "Producer.ns", "public class Root { }"), "receiver cache fixture source written")) return false;
    std::string out, err;
    if (!check(invoke({"--emit-project-oop-lib", producerProject.string(), "-O0", "-o", library.string(),
                      "--no-artifact-cache"}, out, err) == ExitCode::Success,
               "receiver cache fixture producer builds: " + err)) return false;
    {
        contract_lock_prototype::ProductArtifactGuard guard;
        if (!check(guard.acquire({library, metadata, sidecar}), "receiver artifacts are pinned")) return false;
        for (const auto& path : {library, metadata, sidecar}) {
            HANDLE writer = CreateFileW(path.c_str(), GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL, nullptr);
            const bool blocked = writer == INVALID_HANDLE_VALUE;
            if (!blocked) CloseHandle(writer);
            if (!check(blocked, "noncooperative write is blocked for " + path.filename().string())) return false;
            fs::path moved = path; moved += ".renamed";
            if (!check(!MoveFileExW(path.c_str(), moved.c_str(), 0),
                       "noncooperative replacement is blocked for " + path.filename().string())) return false;
        }
        fs::path movedDirectory = root; movedDirectory += ".renamed";
        if (!check(!MoveFileExW(root.c_str(), movedDirectory.c_str(), 0),
                   "parent directory rename is blocked while artifacts are pinned")) return false;
    }
    {
        HANDLE writer = CreateFileW(library.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (!check(writer != INVALID_HANDLE_VALUE, "writer can update artifacts after guard release")) return false;
        CloseHandle(writer);
        fs::path moved = library; moved += ".renamed";
        if (!check(MoveFileExW(library.c_str(), moved.c_str(), 0) &&
                   MoveFileExW(moved.c_str(), library.c_str(), 0),
                   "artifact can be renamed after guard release")) return false;
    }
    const fs::path staticProject = root / "StaticProducer.nsproj";
    const fs::path staticLibrary = root / "StaticProducer.lib";
    if (!check(write(staticProject,
            "<Project><PropertyGroup><Name>StaticProducer</Name><OutputType>Library</OutputType>"
            "</PropertyGroup><ItemGroup><Compile Include=\"StaticProducer.ns\" /></ItemGroup></Project>") &&
        write(root / "StaticProducer.ns", "public class Root { public static int Value = Init(); "
              "private static int Init() { return 7; } }"), "static initializer fixture written")) return false;
    if (!check(invoke({"--emit-project-oop-lib", staticProject.string(), "-O0", "-o", staticLibrary.string(),
                      "--no-artifact-cache"}, out, err) == ExitCode::Success,
               "static initializer producer builds: " + err)) return false;
    const auto staticContract = readReceiverContractManifest(root / "StaticProducer.lib.nsreceiver");
    if (!check(!staticContract.has_value(),
        "root with executable static initialization publishes no receiver contract")) return false;
    if (!check(write(consumerProject,
            "<Project><PropertyGroup><Name>ReceiverConsumer</Name><OutputType>Exe</OutputType></PropertyGroup>"
            "<ItemGroup><Compile Include=\"Consumer.ns\" /><LibraryReference Include=\"Producer.lib\" "
            "Metadata=\"Producer.nsmeta\" /></ItemGroup></Project>") &&
        write(root / "Consumer.ns", "class Derived : Root { public Derived() {} } "
              "class Program { public static int Main() { Derived value; return 0; } }"),
        "receiver cache fixture consumer written")) return false;
    const auto build = [&] (bool cacheEnabled) {
        return invoke({"--emit-project-oop-exe", consumerProject.string(), "-O0", "-o", executable.string(),
                       cacheEnabled ? cache : "--no-artifact-cache", "--time"}, out, err);
    };
    std::error_code ec;
    fs::remove(lock, ec);
    const ExitCode withoutLock = build(true);
    if (!check(!ec && withoutLock != ExitCode::Success &&
        err.find("local class construction or destruction is not proven safe") != std::string::npos,
        "missing reader lock makes receiver contract Unknown (exit " +
        std::to_string(static_cast<int>(withoutLock)) + ", filesystem " + ec.message() +
        ", stdout " + out + ", stderr " + err + ")")) return false;
    if (!check(write(lock, "") && build(true) == ExitCode::Success,
        "guarded receiver contract accepts the consumer: " + err)) return false;
    if (!check(build(true) == ExitCode::Success && out.find("compilation-bundle-cache: hit") != std::string::npos,
        "unchanged guarded bundle is reused: " + err)) return false;
    {
        contract_lock_prototype::ProductLock writer;
        if (!check(writer.acquire(library, false), "cooperative writer obtains the fixture lock")) return false;
        const auto originalTime = fs::last_write_time(sidecar);
        const auto originalSize = fs::file_size(sidecar);
        auto contract = readReceiverContractManifest(sidecar);
        if (!check(contract.has_value(), "guarded receiver contract decodes")) return false;
        bool changed = false;
        for (auto& record : contract->records)
            if (record.kind == ReceiverContractKind::Constructor && record.identity.ends_with("Root")) {
                record.identity.back() = 'u'; changed = true;
            }
        const auto bytes = encodeReceiverContractManifest(*contract);
        if (!check(changed && bytes.has_value(), "same-length changed contract encodes")) return false;
        { std::ofstream file(sidecar, std::ios::binary | std::ios::trunc);
          file.write(reinterpret_cast<const char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()));
          if (!check(bool(file), "changed contract written")) return false; }
        fs::last_write_time(sidecar, originalTime, ec);
        if (!check(!ec && fs::file_size(sidecar) == originalSize && fs::last_write_time(sidecar) == originalTime &&
            verifyReceiverManifestPair(readReceiverContractManifest(sidecar), library, metadata) ==
                ReceiverManifestPairStatus::MatchedBytes,
            "changed contract retains size, mtime and matching artifact hashes")) return false;
    }
    if (!check(build(true) != ExitCode::Success &&
        out.find("compilation-bundle-cache: not-found") != std::string::npos &&
        err.find("local class construction or destruction is not proven safe") != std::string::npos,
        "changed guarded contract misses the old bundle and rejects the local: " + err)) return false;
    if (!check(build(false) != ExitCode::Success &&
        err.find("local class construction or destruction is not proven safe") != std::string::npos,
        "uncached compilation agrees with changed contract: " + err)) return false;

    if (nativesharp::llvm_backend::ProjectOopObjectGenerator::thinLtoRuns(
            nativesharp::llvm_backend::LinkTimeOptimizationMode::Thin,
            nativesharp::llvm_backend::LlvmOptimizationLevel::O2)) {
        const fs::path thinLibrary = root / "ThinProducer.lib";
        const fs::path thinSidecar = root / "ThinProducer.nsthinlto";
        const fs::path thinProject = root / "ThinConsumer.nsproj";
        const fs::path thinExe = root / "ThinConsumer.exe";
        const std::string thinCache = "--artifact-cache=" + (root / "thin-cache").string();
        if (!check(invoke({"--emit-project-oop-lib", producerProject.string(), "-O2", "-flto=thin",
                          "-o", thinLibrary.string(), "--no-artifact-cache"}, out, err) == ExitCode::Success &&
                   fs::exists(thinSidecar), "ThinLTO producer emits bitcode sidecar: " + err)) return false;
        if (!check(write(thinProject,
            "<Project><PropertyGroup><Name>ThinConsumer</Name><OutputType>Exe</OutputType></PropertyGroup>"
            "<ItemGroup><Compile Include=\"Consumer.ns\" /><LibraryReference Include=\"ThinProducer.lib\" "
            "Metadata=\"ThinProducer.nsmeta\" /></ItemGroup></Project>"), "ThinLTO consumer written")) return false;
        const auto buildThin = [&] {
            return invoke({"--emit-project-oop-exe", thinProject.string(), "-O2", "-flto=thin", "-o",
                           thinExe.string(), thinCache, "--time"}, out, err);
        };
        if (!check(buildThin() == ExitCode::Success && out.find("thinlto-libraries: 1") != std::string::npos,
                   "pinned ThinLTO library is consumed: " + err)) return false;
        if (!check(buildThin() == ExitCode::Success && out.find("compilation-bundle-cache: hit") != std::string::npos,
                   "pinned ThinLTO bundle is reused: " + err)) return false;
        HANDLE writer = CreateFileW(thinSidecar.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (!check(writer != INVALID_HANDLE_VALUE, "ThinLTO writer fixture opens")) return false;
        const auto blockedResult = buildThin();
        CloseHandle(writer);
        if (!check(blockedResult != ExitCode::Success &&
                out.find("compilation-bundle-cache: not-found") != std::string::npos &&
                err.find("local class construction or destruction is not proven safe") != std::string::npos,
                "unavailable ThinLTO guard is Unknown and cannot reuse guarded bundle: " + err)) return false;
        std::error_code thinError;
        fs::remove(thinSidecar, thinError);
        if (!check(!thinError && buildThin() != ExitCode::Success &&
            err.find("local class construction or destruction is not proven safe") != std::string::npos,
            "missing ThinLTO sidecar keeps the receiver contract Unknown: " + err)) return false;
    }
    return true;
}
#endif
}

int main()
{
    using nativesharp::ExitCode;
    nativesharp::tests::TestContext tests;

    {
        std::ostringstream output;
        std::ostringstream error;
        tests.expect(run({}, output, error) == ExitCode::Success &&
                output.str().find("--artifact-cache=<absolute-directory>") != std::string::npos &&
                output.str().find("--no-artifact-cache") != std::string::npos &&
                output.str().find("obj/nativesharp/artifact-cache") != std::string::npos &&
                output.str().find("--artifact-cache-max-mb=N") != std::string::npos,
            "help documents the default root, replacement, opt-out and capacity");
    }
    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string_view> arguments = {
            "--emit-project-oop-lib", "input.nsproj", "-o", "output.lib",
            "--no-artifact-cache",
            "--artifact-cache-max-mb=128"};
        const ExitCode result = run(arguments, output, error);
        tests.expect(result == ExitCode::CommandLineError &&
                error.str().find("conflicting artifact-cache policy") != std::string::npos,
            "an explicit cache capacity cannot coexist with the opt-out (exit " +
                std::to_string(static_cast<int>(result)) + "): " + error.str());
    }
    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string_view> arguments = {
            "--emit-project-oop-exe", "input.nsproj", "-o", "output.exe",
            "--artifact-cache=relative/cache"};
        tests.expect(run(arguments, output, error) == ExitCode::CommandLineError &&
                error.str().find("requires an absolute directory") != std::string::npos,
            "relative artifact-cache directories are rejected before compilation");
    }
    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string_view> arguments = {
            "--version", "--artifact-cache=A:/cache"};
        tests.expect(run(arguments, output, error) == ExitCode::CommandLineError &&
                error.str().find("supported when emitting") != std::string::npos,
            "artifact cache cannot silently attach to a non-codegen action");
    }
    {
        std::ostringstream output;
        std::ostringstream error;
        const std::vector<std::string_view> arguments = {
            "--emit-project-oop-lib", "input.nsproj", "-o", "output.lib",
            "--artifact-cache=A:/one", "--artifact-cache=A:/two"};
        tests.expect(run(arguments, output, error) == ExitCode::CommandLineError &&
                error.str().find("conflicting artifact-cache policy") != std::string::npos,
            "conflicting artifact-cache roots are rejected deterministically");
    }
#if defined(_WIN32)
    receiverContractCacheRegression(tests);
#endif
    return tests.result();
}
