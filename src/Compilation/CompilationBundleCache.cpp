#include "nativesharp/Compilation/CompilationBundleCache.h"

#include <system_error>

namespace nativesharp::compilation
{
void CompilationBundleCache::setRoot(std::filesystem::path root)
{
    root_ = std::move(root);
}

bool CompilationBundleCache::lookup(
    std::string_view identity,
    CompilationBundleLookup& result,
    std::string& error) const
{
    result = {};
    if (root_.empty())
    {
        return true;
    }
    if (identity.size() < 2)
    {
        error = "compilation bundle identity is incomplete";
        return false;
    }
    result.entryDirectory = root_ /
        std::string(identity.substr(0, 2)) / std::string(identity);
    result.manifestPath = result.entryDirectory / "bundle.manifest";
    std::error_code fileError;
    const bool present = std::filesystem::is_regular_file(
        result.manifestPath, fileError);
    if (fileError)
    {
        error = "cannot inspect compilation bundle manifest '" +
            result.manifestPath.generic_string() + "': " + fileError.message();
        return false;
    }
    result.kind = present
        ? CompilationBundleLookupKind::Candidate
        : CompilationBundleLookupKind::Missing;
    return true;
}
}
