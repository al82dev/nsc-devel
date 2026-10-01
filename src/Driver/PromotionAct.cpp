#include "nativesharp/Driver/PromotionAct.h"

#include <fstream>
#include <iterator>
#include <system_error>

namespace nativesharp::driver
{
namespace
{
constexpr const char* kAct =
    "NSPROMOTION 1\n"
    "release=3.0.0\n"
    "channel=development\n"
    "predecessor=2.0.0\n"
    "candidate=3.0.0-rc.1\n"
    "compiler_identity=3.0.0+build.<UTC>.<id>\n"
    "language_identity=3.0.0\n"
    "compiler_stage=70\n"
    "breaking_change_ledger=NSBREAKING 1\n"
    "migration_guide=NSMIGRATION-GUIDE 1\n"
    "compatibility_matrix=NSCOMPAT-MATRIX 1\n"
    "final_baseline_contract=NSFINALBASELINE 1\n"
    "decision=pending-final-baseline\n";
}

std::string currentPromotionAct()
{
    return kAct;
}

bool writePromotionAct(const std::filesystem::path& destination, std::string& error)
{
    error.clear();
    if (destination.empty())
    {
        error = "promotion act destination is empty";
        return false;
    }
    std::error_code filesystemError;
    if (!destination.parent_path().empty())
    {
        std::filesystem::create_directories(destination.parent_path(), filesystemError);
    }
    if (filesystemError)
    {
        error = "cannot create promotion act directory: " + filesystemError.message();
        return false;
    }
    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    const std::string act = currentPromotionAct();
    if (!stream || !(stream.write(act.data(), static_cast<std::streamsize>(act.size()))))
    {
        error = "cannot write promotion act '" + destination.string() + "'";
        return false;
    }
    return true;
}

bool verifyPromotionAct(const std::filesystem::path& source, std::string& error)
{
    error.clear();
    std::ifstream stream(source, std::ios::binary);
    if (!stream)
    {
        error = "cannot open promotion act '" + source.string() + "'";
        return false;
    }
    const std::string actual((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad())
    {
        error = "cannot read promotion act '" + source.string() + "'";
        return false;
    }
    if (actual != currentPromotionAct())
    {
        error = "promotion act does not match the pending NativeSharp 3.0 decision";
        return false;
    }
    return true;
}
} // namespace nativesharp::driver
