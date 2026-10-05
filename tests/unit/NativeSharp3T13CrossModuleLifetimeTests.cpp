#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Language/CrossModuleLifetime.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

bool compileAndRun(const std::filesystem::path& root, std::string& diagnostic)
{
    const auto project = root / "Lifetime.nsproj";
    writeFile(project,
        "<Project><PropertyGroup><Name>Lifetime</Name><Language>3</Language>"
        "<OutputType>Exe</OutputType></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
namespace Lifetime
{
    public class Payload
    {
        private owned int[] values;
        public Payload() { values = new int[1]; values[0] = 42; }
        ~Payload() { delete[] values; }
        public int Value() { return values[0]; }
    }
    public class Program
    {
        public static int Main()
        {
            raii Payload payload = new Payload();
            return payload.Value();
        }
    }
}
)NS");
    const auto executable = root / "Lifetime.exe";
    const std::string p = project.string();
    const std::string e = executable.string();
    const std::array<std::string_view, 5> args = {"--emit-project-oop-exe", p, "-O0", "-o", e};
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const auto result = driver.run(args, out, err);
    diagnostic = err.str() + out.str();
    if (result != nativesharp::ExitCode::Success)
    {
        diagnostic += " driver exit code " + std::to_string(static_cast<int>(result));
        return false;
    }
    const int observed = std::system(("\"" + e + "\"").c_str());
    if (observed == 42) return true;
    diagnostic += " process exit code " + std::to_string(observed);
    return false;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    _putenv_s("NSC_LINKER", "A:\\nslink-devel\\build-debug\\bin\\nslink.exe");
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) / "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const CrossModuleLifetimeContract contract = makeCrossModuleLifetimeContract(
        "Producer", "Producer::Payload", "producer.heap.v1",
        ModuleUnloadPolicy::RejectWhileObjectsLive);
    std::string error;
    tests.expect(validateCrossModuleLifetimeContract(contract, error) &&
            contract.createThunk.find("_v1_") != std::string::npos &&
            contract.destroyThunk != contract.deallocateThunk,
        "versioned create/destroy/deallocate thunks are complete and distinct");

    ModuleLifetimeRegistry registry;
    tests.expect(registry.registerObject(contract, error) && registry.liveObjects("Producer") == 1,
        "producer contract registers a live cross-module object");
    tests.expect(!registry.requestUnload("Producer", error) &&
            error.find("still require producer destroy/deallocate thunks") != std::string::npos,
        "module unload is rejected while producer thunks remain live");
    tests.expect(registry.unregisterObject(contract, error) &&
            registry.requestUnload("Producer", error),
        "producer destruction/deallocation releases the module lifetime barrier");
    tests.expect(!registry.registerObject(contract, error) &&
            error.find("is unloading") != std::string::npos,
        "an unloading module cannot create a new exported object");

    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "ns3-t13-f08-directed";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::string diagnostic;
    const bool ran = compileAndRun(root, diagnostic);
    tests.expect(ran, "destruction path executes at O0 through NSLink (" + diagnostic + ")");
    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
