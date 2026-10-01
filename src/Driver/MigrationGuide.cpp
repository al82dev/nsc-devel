#include "nativesharp/Driver/MigrationGuide.h"

#include <fstream>
#include <iterator>
#include <system_error>

namespace nativesharp::driver
{
namespace
{
constexpr const char* kGuide =
    "NSMIGRATION-GUIDE 1\n"
    "source_release=2.0.0\n"
    "target_release=3.0.0\n"
    "breaking_change_ledger=NSBREAKING 1\n"
    "step_count=4\n"
    "step.001=verify-the-2.0-installation-and-record-project-products\n"
    "step.002=select-Language-3-explicitly-only-for-opted-in-projects\n"
    "step.003=rebuild-all-sources-and-precompiled-libraries-with-3.0\n"
    "step.004=verify-staging-and-run-the-N-minus-1-and-N-matrix\n"
    "compat.source=review-required-only-for-ledger-entries\n"
    "compat.nsmeta-1.21=accepted\n"
    "compat.native-abi-1.0=accepted\n"
    "compat.runtime-abi-1.0=accepted\n"
    "compat.default-language-edition=2\n"
    "diagnostic.precompiled=unsupported-producer-or-runtime-abi\n"
    "migration.precompiled=rebuild-with-NativeSharp-3.0\n"
    "verify.command=nsc--verify-installation-then-final-baseline\n";
}

std::string currentMigrationGuide()
{
    return kGuide;
}

bool writeMigrationGuide(const std::filesystem::path& destination, std::string& error)
{
    error.clear();
    if (destination.empty())
    {
        error = "migration guide destination is empty";
        return false;
    }
    std::error_code filesystemError;
    if (!destination.parent_path().empty())
    {
        std::filesystem::create_directories(destination.parent_path(), filesystemError);
    }
    if (filesystemError)
    {
        error = "cannot create migration guide directory: " + filesystemError.message();
        return false;
    }
    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    const std::string guide = currentMigrationGuide();
    if (!stream || !(stream.write(guide.data(), static_cast<std::streamsize>(guide.size()))))
    {
        error = "cannot write migration guide '" + destination.string() + "'";
        return false;
    }
    return true;
}

bool verifyMigrationGuide(const std::filesystem::path& source, std::string& error)
{
    error.clear();
    std::ifstream stream(source, std::ios::binary);
    if (!stream)
    {
        error = "cannot open migration guide '" + source.string() + "'";
        return false;
    }
    const std::string actual((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad())
    {
        error = "cannot read migration guide '" + source.string() + "'";
        return false;
    }
    if (actual != currentMigrationGuide())
    {
        error = "migration guide does not match the NativeSharp 2.0 to 3.0 procedure";
        return false;
    }
    return true;
}
} // namespace nativesharp::driver
