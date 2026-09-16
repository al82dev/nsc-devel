#include "nativesharp/Driver/FinalBaselineContract.h"

#include <fstream>
#include <iterator>
#include <system_error>

namespace nativesharp::driver
{
namespace
{
constexpr const char* kContract =
    "NSFINALBASELINE 1\n"
    "release=2.0.0\n"
    "build=clean-release\n"
    "install=verified-C:/native-sharp\n"
    "linker.001=lld-link\n"
    "linker.002=NSLink\n"
    "optimization.001=O0\n"
    "optimization.002=O2\n"
    "ctest=all-no-skips\n"
    "fixtures=all\n"
    "bug_triage=all\n"
    "gate0=landed\n"
    "source_digest=S0-equals-S1\n"
    "compiler_hash=build-equals-install-and-stable\n"
    "nslink_hash=stable\n"
    "interactive_debug=disabled\n"
    "message_boxes=forbidden\n"
    "failure_count=0\n"
    "promotion_requires=passed-report\n";
}

std::string currentFinalBaselineContract()
{
    return kContract;
}

bool writeFinalBaselineContract(const std::filesystem::path& destination, std::string& error)
{
    error.clear();
    if (destination.empty())
    {
        error = "final baseline contract destination is empty";
        return false;
    }
    std::error_code filesystemError;
    if (!destination.parent_path().empty())
    {
        std::filesystem::create_directories(destination.parent_path(), filesystemError);
    }
    if (filesystemError)
    {
        error = "cannot create final baseline contract directory: " + filesystemError.message();
        return false;
    }
    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    const std::string contract = currentFinalBaselineContract();
    if (!stream || !(stream.write(contract.data(), static_cast<std::streamsize>(contract.size()))))
    {
        error = "cannot write final baseline contract '" + destination.string() + "'";
        return false;
    }
    return true;
}

bool verifyFinalBaselineContract(const std::filesystem::path& source, std::string& error)
{
    error.clear();
    std::ifstream stream(source, std::ios::binary);
    if (!stream)
    {
        error = "cannot open final baseline contract '" + source.string() + "'";
        return false;
    }
    const std::string actual((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad())
    {
        error = "cannot read final baseline contract '" + source.string() + "'";
        return false;
    }
    if (actual != currentFinalBaselineContract())
    {
        error = "final baseline contract does not match the NativeSharp 2.0 release gate";
        return false;
    }
    return true;
}
} // namespace nativesharp::driver
