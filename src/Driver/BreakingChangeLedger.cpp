#include "nativesharp/Driver/BreakingChangeLedger.h"

#include <fstream>
#include <iterator>
#include <system_error>

namespace nativesharp::driver
{
namespace
{
constexpr const char* kLedger =
    "NSBREAKING 1\n"
    "source_release=1.0.1\n"
    "target_release=2.0.0\n"
    "entry_count=1\n"
    "change.001.id=NS2-BREAK-001\n"
    "change.001.1x_contract=static-library-products-contained-win32-runtime\n"
    "change.001.consumer=native-c-and-cpp-static-library-hosts\n"
    "change.001.compatible_alternatives_rejected=duplicate-runtime-per-library;implicit-linker-injection\n"
    "change.001.new_semantics=link-NativeSharp.Runtime.Win32.lib-explicitly\n"
    "change.001.impact=source:no;binary:yes;metadata:no;tools:yes\n"
    "change.001.diagnostics_migration=unresolved-runtime-symbol;add-NativeSharp.Runtime.Win32.lib\n"
    "change.001.minimum_component_version=compiler:2.0.0;runtime-abi:1.0;nslink:2.0.0\n"
    "change.001.compatibility_tests=nsc.item401.independent-windows-runtime;nsc.item402.breaking-change-ledger\n";
}

std::string currentBreakingChangeLedger()
{
    return kLedger;
}

bool writeBreakingChangeLedger(const std::filesystem::path& destination, std::string& error)
{
    error.clear();
    if (destination.empty())
    {
        error = "breaking-change ledger destination is empty";
        return false;
    }
    std::error_code filesystemError;
    if (!destination.parent_path().empty())
    {
        std::filesystem::create_directories(destination.parent_path(), filesystemError);
    }
    if (filesystemError)
    {
        error = "cannot create breaking-change ledger directory: " + filesystemError.message();
        return false;
    }
    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    const std::string ledger = currentBreakingChangeLedger();
    if (!stream || !(stream.write(ledger.data(), static_cast<std::streamsize>(ledger.size()))))
    {
        error = "cannot write breaking-change ledger '" + destination.string() + "'";
        return false;
    }
    return true;
}

bool verifyBreakingChangeLedger(const std::filesystem::path& source, std::string& error)
{
    error.clear();
    std::ifstream stream(source, std::ios::binary);
    if (!stream)
    {
        error = "cannot open breaking-change ledger '" + source.string() + "'";
        return false;
    }
    const std::string actual((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad())
    {
        error = "cannot read breaking-change ledger '" + source.string() + "'";
        return false;
    }
    if (actual != currentBreakingChangeLedger())
    {
        error = "breaking-change ledger does not match the certified 1.0.1 to 2.0 inventory";
        return false;
    }
    return true;
}
} // namespace nativesharp::driver
