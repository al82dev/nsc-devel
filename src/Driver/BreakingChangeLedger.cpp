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
    "source_release=2.0.0\n"
    "target_release=3.0.0\n"
    "entry_count=1\n"
    "change.001.id=NS3-BREAK-001\n"
    "change.001.2x_contract=compiler:2.0.0;nsmeta:1.21;runtime-abi:1.0;runtime-symbol-set:2\n"
    "change.001.consumer=precompiled-libraries-runtime-and-tooling\n"
    "change.001.compatible_alternatives_rejected=silent-abi-upgrade;accept-N-minus-2-producers\n"
    "change.001.new_semantics=compiler:3.0.0;nsmeta:1.33;runtime-abi:1.1;runtime-symbol-set:3\n"
    "change.001.impact=source:no;binary:yes;metadata:yes;tools:yes\n"
    "change.001.diagnostics_migration=unsupported-producer-or-runtime-abi;rebuild-with-3.0\n"
    "change.001.minimum_component_version=compiler:3.0.0;runtime-abi:1.1;nsmeta:1.33\n"
    "change.001.compatibility_tests=nsc.stage48_2.library-metadata;nsc.item393.compatibility-matrix\n";
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
        error = "breaking-change ledger does not match the NativeSharp 2.0 to 3.0 inventory";
        return false;
    }
    return true;
}
} // namespace nativesharp::driver
