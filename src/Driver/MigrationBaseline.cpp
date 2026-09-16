#include "nativesharp/Driver/MigrationBaseline.h"

#include "nativesharp/Basic/Version.h"
#include "nativesharp/Driver/BreakingChangeLedger.h"
#include "nativesharp/Driver/CompatibilityPolicy.h"
#include "nativesharp/Driver/ToolchainContract.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Runtime/RuntimeAbiContract.h"

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

std::vector<Field> fields()
{
    using namespace interop;
    return {
        {"source_compiler", std::string(kCompilerReleaseVersion)},
        {"source_language", std::string(kLanguageVersion)},
        {"target_major", "2"},
        {"status", "design-baseline"},
        {"nsmeta", std::to_string(kMetadataFormatMajor) + "." + std::to_string(kMetadataFormatMinor)},
        {"native_abi", std::to_string(kNativeAbiMajor) + "." + std::to_string(kNativeAbiMinor)},
        {"runtime_abi", std::to_string(kRuntimeAbiMajor) + "." + std::to_string(kRuntimeAbiMinor)},
        {"runtime_symbol_set", std::to_string(runtime::kRuntimeSymbolSetVersion)},
        {"mir_schema", std::to_string(frontend::kProjectOopMirSchemaMajor) + "." +
                           std::to_string(frontend::kProjectOopMirSchemaMinor)},
        {"mangling_abi", std::to_string(kManglingAbiVersion)},
        {"object_abi", std::to_string(kObjectAbiVersion)},
        {"delegate_abi", std::to_string(kDelegateAbiVersion)},
        {"exception_abi", std::to_string(kExceptionAbiVersion)},
        {"toolchain_contract", std::to_string(kToolchainContractFormatVersion)},
        {"compatibility_policy", std::to_string(kCompatibilityPolicyFormatVersion)},
        {"breaking_change_ledger", std::to_string(kBreakingChangeLedgerFormatVersion)},
        {"migration_notes", "required"},
        {"n_minus_one_matrix", "required"},
    };
}

void removeCarriageReturn(std::string& line)
{
    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }
}
} // namespace

std::string currentMigrationBaseline()
{
    std::string result = "NSMIGRATION " + std::to_string(kMigrationBaselineFormatVersion) + "\n";
    for (const Field& field : fields())
    {
        result += field.first + '=' + field.second + '\n';
    }
    return result;
}

bool writeMigrationBaseline(const std::filesystem::path& destination, std::string& error)
{
    error.clear();
    if (destination.empty())
    {
        error = "migration baseline destination is empty";
        return false;
    }
    const std::filesystem::path parent = destination.parent_path();
    if (!parent.empty())
    {
        std::error_code filesystemError;
        std::filesystem::create_directories(parent, filesystemError);
        if (filesystemError)
        {
            error = "cannot create migration baseline directory '" + parent.string() + "': " +
                    filesystemError.message();
            return false;
        }
    }
    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    const std::string text = currentMigrationBaseline();
    if (!stream || !(stream.write(text.data(), static_cast<std::streamsize>(text.size()))))
    {
        error = "cannot write migration baseline '" + destination.string() + "'";
        return false;
    }
    return true;
}

bool verifyMigrationBaseline(const std::filesystem::path& source, std::string& error)
{
    error.clear();
    std::ifstream stream(source, std::ios::binary);
    if (!stream)
    {
        error = "cannot open migration baseline '" + source.string() + "'";
        return false;
    }
    std::string line;
    if (!std::getline(stream, line))
    {
        error = "migration baseline is empty";
        return false;
    }
    removeCarriageReturn(line);
    const std::string header = "NSMIGRATION " + std::to_string(kMigrationBaselineFormatVersion);
    if (line != header)
    {
        error = "unsupported migration baseline header '" + line + "'; expected '" + header + "'";
        return false;
    }
    const std::vector<Field> expected = fields();
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        if (!std::getline(stream, line))
        {
            error = "migration baseline is missing field '" + expected[index].first + "'";
            return false;
        }
        removeCarriageReturn(line);
        const std::string expectedLine = expected[index].first + '=' + expected[index].second;
        if (line != expectedLine)
        {
            error = "incompatible migration baseline field on line " + std::to_string(index + 2U) +
                    ": found '" + line + "', expected '" + expectedLine + "'";
            return false;
        }
    }
    if (std::getline(stream, line))
    {
        removeCarriageReturn(line);
        error = "unexpected migration baseline field '" + line + "'";
        return false;
    }
    return !stream.bad();
}
} // namespace nativesharp::driver
