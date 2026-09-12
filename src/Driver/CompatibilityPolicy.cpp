#include "nativesharp/Driver/CompatibilityPolicy.h"

#include "nativesharp/Basic/Version.h"
#include "nativesharp/Driver/InstallationManifest.h"
#include "nativesharp/Driver/MigrationBaseline.h"
#include "nativesharp/Driver/PackageLifecycle.h"
#include "nativesharp/Driver/ToolchainContract.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include <array>
#include <charconv>
#include <fstream>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace nativesharp::driver
{
namespace
{
using Field = std::pair<std::string, std::string>;

struct SemanticVersion final
{
    unsigned major = 0;
    unsigned minor = 0;
    unsigned patch = 0;
};

std::vector<Field> fields()
{
    using namespace interop;
    return {
        {"compiler_release", std::string(kCompilerReleaseVersion)},
        {"language_release", std::string(kLanguageVersion)},
        {"compiler_compatibility", "same-minor-before-1;same-major-after-1"},
        {"language_compatibility", "same-minor-before-1;same-major-after-1"},
        {"deprecation", "diagnostic-before-removal;one-minor-minimum"},
        {"nsmeta_read", "1.0-1." + std::to_string(kMetadataFormatMinor)},
        {"nsmeta_write", std::to_string(kMetadataFormatMajor) + "." + std::to_string(kMetadataFormatMinor)},
        {"native_abi", std::to_string(kNativeAbiMajor) + "." + std::to_string(kNativeAbiMinor)},
        {"runtime_abi", std::to_string(kRuntimeAbiMajor) + "." + std::to_string(kRuntimeAbiMinor)},
        {"toolchain_contract", std::to_string(kToolchainContractFormatVersion)},
        {"installation_files", std::to_string(kInstallationFileListFormatVersion)},
        {"installation_manifest", std::to_string(kInstallationManifestFormatVersion)},
        {"reproducibility_report", std::to_string(kReproducibilityReportFormatVersion)},
        {"update_plan", std::to_string(kInstallationUpdatePlanFormatVersion)},
        {"migration_baseline", std::to_string(kMigrationBaselineFormatVersion)},
    };
}

void removeCarriageReturn(std::string &line)
{
    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }
}

bool parseComponent(std::string_view text, unsigned &value)
{
    if (text.empty())
    {
        return false;
    }
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() && std::to_string(value) == text;
}

bool parseSemanticVersion(std::string_view text, SemanticVersion &version)
{
    const std::size_t first = text.find('.');
    const std::size_t second = first == std::string_view::npos ? first : text.find('.', first + 1U);
    return first != std::string_view::npos && second != std::string_view::npos &&
           text.find('.', second + 1U) == std::string_view::npos &&
           parseComponent(text.substr(0, first), version.major) &&
           parseComponent(text.substr(first + 1U, second - first - 1U), version.minor) &&
           parseComponent(text.substr(second + 1U), version.patch);
}
} // namespace

std::string currentCompatibilityPolicy()
{
    std::string result = "NSCOMPAT " + std::to_string(kCompatibilityPolicyFormatVersion) + "\n";
    for (const Field &field : fields())
    {
        result += field.first + '=' + field.second + '\n';
    }
    return result;
}

bool writeCompatibilityPolicy(const std::filesystem::path &destination, std::string &error)
{
    error.clear();
    if (destination.empty())
    {
        error = "compatibility policy destination is empty";
        return false;
    }
    std::error_code filesystemError;
    const std::filesystem::path parent = destination.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, filesystemError);
        if (filesystemError)
        {
            error =
                "cannot create compatibility policy directory '" + parent.string() + "': " + filesystemError.message();
            return false;
        }
    }
    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    const std::string text = currentCompatibilityPolicy();
    if (!stream || !(stream.write(text.data(), static_cast<std::streamsize>(text.size()))))
    {
        error = "cannot write compatibility policy '" + destination.string() + "'";
        return false;
    }
    return true;
}

bool verifyCompatibilityPolicy(const std::filesystem::path &source, std::string &error)
{
    error.clear();
    std::ifstream stream(source, std::ios::binary);
    if (!stream)
    {
        error = "cannot open compatibility policy '" + source.string() + "'";
        return false;
    }
    std::string line;
    if (!std::getline(stream, line))
    {
        error = "compatibility policy is empty";
        return false;
    }
    removeCarriageReturn(line);
    const std::string header = "NSCOMPAT " + std::to_string(kCompatibilityPolicyFormatVersion);
    if (line != header)
    {
        error = "unsupported compatibility policy header '" + line + "'; expected '" + header + "'";
        return false;
    }
    const std::vector<Field> expected = fields();
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        if (!std::getline(stream, line))
        {
            error = "compatibility policy is missing field '" + expected[index].first + "'";
            return false;
        }
        removeCarriageReturn(line);
        const std::string expectedLine = expected[index].first + '=' + expected[index].second;
        if (line != expectedLine)
        {
            error = "incompatible compatibility policy field on line " + std::to_string(index + 2U) + ": found '" +
                    line + "', expected '" + expectedLine + "'";
            return false;
        }
    }
    if (std::getline(stream, line))
    {
        removeCarriageReturn(line);
        error = "unexpected compatibility policy field '" + line + "'";
        return false;
    }
    return !stream.bad();
}

bool checkCompilerCompatibility(std::string_view requiredVersion, std::string &explanation, std::string &error)
{
    explanation.clear();
    error.clear();
    SemanticVersion current;
    SemanticVersion required;
    if (!parseSemanticVersion(kCompilerReleaseVersion, current))
    {
        error = "compiler contains an invalid release version";
        return false;
    }
    if (!parseSemanticVersion(requiredVersion, required))
    {
        error = "required compiler version '" + std::string(requiredVersion) + "' is not canonical major.minor.patch";
        return false;
    }

    bool compatible = false;
    if (current.major == 0U || required.major == 0U)
    {
        compatible =
            current.major == required.major && current.minor == required.minor && current.patch >= required.patch;
    }
    else
    {
        compatible =
            current.major == required.major &&
            (current.minor > required.minor || (current.minor == required.minor && current.patch >= required.patch));
    }
    explanation = "compiler " + std::string(kCompilerReleaseVersion) +
                  (compatible ? " satisfies " : " does not satisfy ") + std::string(requiredVersion);
    return compatible;
}
} // namespace nativesharp::driver
