#include "nativesharp/Driver/MigrationGuide.h"

#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>

namespace nativesharp::driver
{
namespace
{
constexpr const char* kGuide =
    "NSMIGRATION-GUIDE 1\n"
    "source_release=none\n"
    "target_release=1.0.0\n"
    "breaking_change_ledger=NSBREAKING 1\n"
    "step_count=4\n"
    "step.001=verify-the-installation-and-record-project-products\n"
    "step.002=build-all-understood-source-with-the-cumulative-language\n"
    "step.003=rebuild-all-sources-and-precompiled-libraries\n"
    "step.004=verify-staging-and-the-applicable-capability-matrix\n"
    "compat.source=review-required-only-for-ledger-entries\n"
    "compat.nsmeta=content-compatible\n"
    "compat.native-abi-1.0=accepted\n"
    "compat.runtime-abi-1.0=accepted\n"
    "compat.source-language=cumulative\n"
    "diagnostic.precompiled=incompatible-runtime-or-native-abi\n"
    "migration.precompiled=rebuild-with-NativeSharp\n"
    "verify.command=nsc--verify-installation-then-final-baseline\n";
std::string withoutInformationalLines(std::string_view text)
{
    std::string result;
    while (!text.empty())
    {
        const auto end = text.find('\n');
        const auto line = text.substr(0, end);
        const bool metadataLabel = line.starts_with("compat.nsmeta=") ||
            line.starts_with("compat.nsmeta.") || line.starts_with("compat.nsmeta-") ||
            line.starts_with("compat.nsmeta_");
        if (!metadataLabel && !line.starts_with("compat.source-language=") &&
            !line.starts_with("source_language=") && !line.starts_with("language_"))
        {
            result.append(line);
            if (end != std::string_view::npos) result.push_back('\n');
        }
        if (end == std::string_view::npos) break;
        text.remove_prefix(end + 1);
    }
    return result;
}
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
    if (withoutInformationalLines(actual) != withoutInformationalLines(currentMigrationGuide()))
    {
        error = "migration guide does not match the NativeSharp procedure";
        return false;
    }
    return true;
}
} // namespace nativesharp::driver
