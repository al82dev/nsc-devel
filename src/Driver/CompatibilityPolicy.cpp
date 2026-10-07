#include "nativesharp/Driver/CompatibilityPolicy.h"

#include "nativesharp/Basic/Version.h"
#include "nativesharp/Driver/BreakingChangeLedger.h"
#include "nativesharp/Driver/FinalBaselineContract.h"
#include "nativesharp/Driver/InstallationManifest.h"
#include "nativesharp/Driver/MigrationBaseline.h"
#include "nativesharp/Driver/MigrationGuide.h"
#include "nativesharp/Driver/PackageLifecycle.h"
#include "nativesharp/Driver/ReleaseCandidateFreeze.h"
#include "nativesharp/Driver/PromotionAct.h"
#include "nativesharp/Driver/ToolchainContract.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include <array>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::driver
{
namespace
{
using Field = std::pair<std::string, std::string>;
using PolicyFields = std::unordered_map<std::string, std::string>;

std::vector<Field> fields()
{
    using namespace interop;
    return {
        {"compiler_release", std::string(kCompilerReleaseVersion)},
        {"language_release", std::string(kLanguageVersion)},
        {"compiler_compatibility", "capability-based;version-label-informational"},
        {"language_compatibility", "capability-based;version-label-informational"},
        {"deprecation", "diagnostic-before-removal;one-minor-minimum"},
        {"nsmeta_read", "content-compatible;version-label-informational"},
        {"nsmeta_write", std::to_string(interop::kMetadataFormatMajor) + "." + std::to_string(interop::kMetadataFormatMinor)},
        {"native_abi", std::to_string(kNativeAbiMajor) + "." + std::to_string(kNativeAbiMinor)},
        {"runtime_abi", std::to_string(kRuntimeAbiMajor) + "." + std::to_string(kRuntimeAbiMinor)},
        {"toolchain_contract", std::to_string(kToolchainContractFormatVersion)},
        {"installation_files", std::to_string(kInstallationFileListFormatVersion)},
        {"installation_manifest", std::to_string(kInstallationManifestFormatVersion)},
        {"reproducibility_report", std::to_string(kReproducibilityReportFormatVersion)},
        {"update_plan", std::to_string(kInstallationUpdatePlanFormatVersion)},
        {"migration_baseline", std::to_string(kMigrationBaselineFormatVersion)},
        {"breaking_change_ledger", std::to_string(kBreakingChangeLedgerFormatVersion)},
        {"migration_guide", std::to_string(kMigrationGuideFormatVersion)},
        {"release_candidate_freeze", std::to_string(kReleaseCandidateFreezeFormatVersion)},
        {"final_baseline_contract", std::to_string(kFinalBaselineContractFormatVersion)},
        {"promotion_act", std::to_string(kPromotionActFormatVersion)},
    };
}

void removeCarriageReturn(std::string &line)
{
    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }
}

bool readCompatibilityPolicy(
    const std::filesystem::path& source,
    PolicyFields& parsedFields,
    std::string& error)
{
    error.clear();
    parsedFields.clear();
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
    if (!line.starts_with("NSCOMPAT ") || line.size() == std::string_view("NSCOMPAT ").size())
    {
        error = "malformed compatibility policy header '" + line + "'";
        return false;
    }
    while (std::getline(stream, line))
    {
        removeCarriageReturn(line);
        if (line.empty()) continue;
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos || separator == 0U)
        {
            error = "malformed compatibility policy field '" + line + "'";
            return false;
        }
        const std::string key = line.substr(0, separator);
        if (!parsedFields.emplace(key, line.substr(separator + 1U)).second)
        {
            error = "duplicate compatibility policy field '" + key + "'";
            return false;
        }
    }
    return !stream.bad();
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
    PolicyFields parsedFields;
    return readCompatibilityPolicy(source, parsedFields, error);
}

bool verifyInstalledCompatibilityCapabilities(
    const std::filesystem::path &source,
    std::string &error)
{
    PolicyFields parsedFields;
    if (!readCompatibilityPolicy(source, parsedFields, error)) return false;
    for (const Field& required : fields())
    {
        const auto found = parsedFields.find(required.first);
        if (found == parsedFields.end())
        {
            error = "compatibility policy is missing required capability '" + required.first + "'";
            return false;
        }
        if (found->second != required.second)
        {
            error = "incompatible compatibility policy capability '" + required.first +
                "': found '" + found->second + "', expected '" + required.second + "'";
            return false;
        }
    }
    return true;
}

bool checkCompilerCompatibility(std::string_view requiredVersion, std::string &explanation, std::string &error)
{
    explanation.clear();
    error.clear();
    explanation = "compiler " + std::string(kCompilerReleaseVersion) + " satisfies " +
        std::string(requiredVersion) +
        "; version labels are informational and concrete capabilities are validated when used";
    return true;
}

std::string currentCompatibilityMatrix()
{
    std::string matrix = "NSCOMPAT-MATRIX " +
        std::to_string(kCompatibilityMatrixFormatVersion) + "\n";
    matrix += "certification=pending-final-baseline\n";
    matrix += "compiler.N=" + std::string(kCompilerReleaseVersion) + "\n";
    matrix += "compiler.N-1=" + std::string(kPreviousCompilerReleaseVersion) + "\n";
    matrix += "nsmeta.N=" + std::to_string(interop::kMetadataFormatMajor) + "." +
        std::to_string(interop::kMetadataFormatMinor) + "\n";
    matrix += "nsmeta.N-1=1.21\n";
    matrix += "native-abi.N=" + std::to_string(interop::kNativeAbiMajor) + "." +
        std::to_string(interop::kNativeAbiMinor) + "\n";
    matrix += "native-abi.N-1=1.0\n";
    matrix += "runtime-abi.N=" + std::to_string(interop::kRuntimeAbiMajor) + "." +
        std::to_string(interop::kRuntimeAbiMinor) + "\n";
    matrix += "runtime-abi.N-1=1.0\n";
    for (const std::string_view surface : {
             std::string_view("compiler"), std::string_view("nsmeta"),
             std::string_view("native-abi"), std::string_view("runtime-abi")})
    {
        matrix += "row=" + std::string(surface) + "|consumer:N|producer:N-1|accept\n";
        matrix += "row=" + std::string(surface) + "|consumer:N|producer:N|accept\n";
        matrix += "row=" + std::string(surface) + "|consumer:N-1|producer:N|reject\n";
    }
    return matrix;
}

bool writeCompatibilityMatrix(const std::filesystem::path& destination, std::string& error)
{
    error.clear();
    if (destination.empty())
    {
        error = "compatibility matrix destination is empty";
        return false;
    }
    std::error_code filesystemError;
    if (!destination.parent_path().empty())
        std::filesystem::create_directories(destination.parent_path(), filesystemError);
    if (filesystemError)
    {
        error = "cannot create compatibility matrix directory: " + filesystemError.message();
        return false;
    }
    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    const std::string matrix = currentCompatibilityMatrix();
    if (!stream || !(stream.write(matrix.data(), static_cast<std::streamsize>(matrix.size()))))
    {
        error = "cannot write compatibility matrix '" + destination.string() + "'";
        return false;
    }
    return true;
}

bool verifyCompatibilityMatrix(const std::filesystem::path& source, std::string& error)
{
    error.clear();
    std::ifstream stream(source, std::ios::binary);
    if (!stream)
    {
        error = "cannot open compatibility matrix '" + source.string() + "'";
        return false;
    }
    const std::string actual((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad())
    {
        error = "cannot read compatibility matrix '" + source.string() + "'";
        return false;
    }
    const std::string expected = currentCompatibilityMatrix();
    if (actual != expected)
    {
        error = "compatibility matrix does not match the certified N-1/N policy";
        return false;
    }
    return true;
}
} // namespace nativesharp::driver
