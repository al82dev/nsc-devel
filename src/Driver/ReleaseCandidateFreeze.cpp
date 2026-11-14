#include "nativesharp/Driver/ReleaseCandidateFreeze.h"

#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>

namespace nativesharp::driver
{
namespace
{
constexpr const char* kFreeze =
    "NSRCFREEZE 1\n"
    "candidate=3.0.0-rc.1\n"
    "promotion_target=3.0.0\n"
    "source_baseline=2.0.0\n"
    "breaking_change_ledger=NSBREAKING 1\n"
    "migration_guide=NSMIGRATION-GUIDE 1\n"
    "compatibility_matrix=NSCOMPAT-MATRIX 1\n"
    "nsmeta=content-compatible\n"
    "native_abi=1.0\n"
    "runtime_abi=1.2\n"
    "runtime_symbol_set=4\n"
    "mir_schema=1.21\n"
    "change_policy=no-contract-or-product-surface-changes\n"
    "allowed_changes=release-blocker-fixes-with-new-rc-and-recertification\n"
    "evidence_policy=clean-build-install-focused-checks-and-final-baseline\n"
    "promotion_policy=explicit-only-after-final-N-minus-1-and-N-certification\n";
std::string withoutMetadataLine(std::string_view text)
{
    std::string result;
    while (!text.empty())
    {
        const auto end = text.find('\n');
        const auto line = text.substr(0, end);
        if (!line.starts_with("nsmeta="))
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

std::string currentReleaseCandidateFreeze()
{
    return kFreeze;
}

bool writeReleaseCandidateFreeze(const std::filesystem::path& destination, std::string& error)
{
    error.clear();
    if (destination.empty())
    {
        error = "release-candidate freeze destination is empty";
        return false;
    }
    std::error_code filesystemError;
    if (!destination.parent_path().empty())
    {
        std::filesystem::create_directories(destination.parent_path(), filesystemError);
    }
    if (filesystemError)
    {
        error = "cannot create release-candidate freeze directory: " + filesystemError.message();
        return false;
    }
    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    const std::string freeze = currentReleaseCandidateFreeze();
    if (!stream || !(stream.write(freeze.data(), static_cast<std::streamsize>(freeze.size()))))
    {
        error = "cannot write release-candidate freeze '" + destination.string() + "'";
        return false;
    }
    return true;
}

bool verifyReleaseCandidateFreeze(const std::filesystem::path& source, std::string& error)
{
    error.clear();
    std::ifstream stream(source, std::ios::binary);
    if (!stream)
    {
        error = "cannot open release-candidate freeze '" + source.string() + "'";
        return false;
    }
    const std::string actual((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad())
    {
        error = "cannot read release-candidate freeze '" + source.string() + "'";
        return false;
    }
    if (withoutMetadataLine(actual) != withoutMetadataLine(currentReleaseCandidateFreeze()))
    {
        error = "release-candidate freeze does not match NativeSharp 3.0 RC.1";
        return false;
    }
    return true;
}
} // namespace nativesharp::driver
