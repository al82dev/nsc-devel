#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace nativesharp::compilation
{
enum class CompilationBundleLookupKind
{
    Disabled,
    Missing,
    Candidate
};

struct CompilationBundleLookup final
{
    CompilationBundleLookupKind kind = CompilationBundleLookupKind::Disabled;
    std::filesystem::path entryDirectory;
    std::filesystem::path manifestPath;
};

class CompilationBundleCache final
{
public:
    void setRoot(std::filesystem::path root);
    bool lookup(
        std::string_view identity,
        CompilationBundleLookup& result,
        std::string& error) const;

private:
    std::filesystem::path root_;
};
}
