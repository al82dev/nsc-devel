#include "nativesharp/Driver/ToolchainContract.h"

#include "nativesharp/Basic/Version.h"
#include "nativesharp/Driver/CompatibilityPolicy.h"
#include "nativesharp/Driver/InstallationManifest.h"
#include "nativesharp/Driver/PackageLifecycle.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/LLVM/LlvmVersion.h"
#include "nativesharp/Target/TargetInfo.h"

#include <array>
#include <fstream>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::driver {
namespace {
using Field = std::pair<std::string, std::string>;

std::vector<Field> expectedFields() {
  using namespace interop;

  const target::TargetInfo target = target::TargetInfo::WindowsX64();
  return {
      {"compiler_release", std::string(kCompilerReleaseVersion)},
      {"language", std::string(kLanguageVersion)},
      {"stage", std::string(kCompilerStage)},
      {"target", std::string(target.llvmTriple())},
      {"llvm", std::to_string(llvm_backend::kSupportedLlvmMajor) + ".x"},
      {"nsmeta", std::to_string(kMetadataFormatMajor) + "." +
                     std::to_string(kMetadataFormatMinor)},
      {"native_abi",
       std::to_string(kNativeAbiMajor) + "." + std::to_string(kNativeAbiMinor)},
      {"runtime_abi", std::to_string(kRuntimeAbiMajor) + "." +
                          std::to_string(kRuntimeAbiMinor)},
      {"mangling_abi", std::to_string(kManglingAbiVersion)},
      {"object_abi", std::to_string(kObjectAbiVersion)},
      {"delegate_abi", std::to_string(kDelegateAbiVersion)},
      {"exception_abi", std::to_string(kExceptionAbiVersion)},
      {"managed_delegate_cell_size", std::to_string(kManagedDelegateCellSize)},
      {"supported_linkers", "lld-link,NSLink"},
      {"certified_optimizations", "O0,O2"},
      {"artifact_kinds", "exe,static-lib,dll"},
      {"installation_files",
       std::to_string(kInstallationFileListFormatVersion)},
      {"installation_manifest",
       std::to_string(kInstallationManifestFormatVersion)},
      {"reproducibility_report",
       std::to_string(kReproducibilityReportFormatVersion)},
      {"update_plan", std::to_string(kInstallationUpdatePlanFormatVersion)},
      {"compatibility_policy",
       std::to_string(kCompatibilityPolicyFormatVersion)},
  };
}

void removeCarriageReturn(std::string &line) {
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
}
} // namespace

std::string currentToolchainContract() {
  std::string contract =
      "NSTOOLCHAIN " + std::to_string(kToolchainContractFormatVersion) + "\n";
  for (const Field &field : expectedFields()) {
    contract += field.first;
    contract += '=';
    contract += field.second;
    contract += '\n';
  }
  return contract;
}

bool writeToolchainContract(const std::filesystem::path &destination,
                            std::string &error) {
  error.clear();
  if (destination.empty()) {
    error = "toolchain contract destination is empty";
    return false;
  }

  const std::filesystem::path parent = destination.parent_path();
  if (!parent.empty()) {
    std::error_code filesystemError;
    std::filesystem::create_directories(parent, filesystemError);
    if (filesystemError) {
      error = "cannot create toolchain contract directory '" + parent.string() +
              "': " + filesystemError.message();
      return false;
    }
  }

  std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
  if (!stream) {
    error = "cannot open toolchain contract destination '" +
            destination.string() + "'";
    return false;
  }

  const std::string contract = currentToolchainContract();
  stream.write(contract.data(), static_cast<std::streamsize>(contract.size()));
  if (!stream) {
    error = "cannot write toolchain contract destination '" +
            destination.string() + "'";
    return false;
  }
  return true;
}

bool verifyToolchainContract(const std::filesystem::path &source,
                             std::string &error) {
  error.clear();
  std::ifstream stream(source, std::ios::binary);
  if (!stream) {
    error = "cannot open toolchain contract '" + source.string() + "'";
    return false;
  }

  std::string line;
  if (!std::getline(stream, line)) {
    error = "toolchain contract is empty";
    return false;
  }
  removeCarriageReturn(line);
  const std::string expectedHeader =
      "NSTOOLCHAIN " + std::to_string(kToolchainContractFormatVersion);
  if (line != expectedHeader) {
    error = "unsupported toolchain contract header '" + line + "'; expected '" +
            expectedHeader + "'";
    return false;
  }

  const std::vector<Field> expected = expectedFields();
  std::unordered_map<std::string, std::string> expectedByName;
  for (const Field &field : expected) {
    expectedByName.emplace(field.first, field.second);
  }

  std::vector<Field> parsed;
  std::unordered_set<std::string> seen;
  std::size_t lineNumber = 1;
  while (std::getline(stream, line)) {
    ++lineNumber;
    removeCarriageReturn(line);
    const std::size_t separator = line.find('=');
    if (line.empty() || separator == std::string::npos || separator == 0 ||
        separator + 1 >= line.size()) {
      error = "malformed toolchain contract line " +
              std::to_string(lineNumber) + ": '" + line + "'";
      return false;
    }

    std::string name = line.substr(0, separator);
    std::string value = line.substr(separator + 1);
    const auto expectedField = expectedByName.find(name);
    if (expectedField == expectedByName.end()) {
      error = "unknown toolchain contract field '" + name + "' on line " +
              std::to_string(lineNumber);
      return false;
    }
    if (!seen.insert(name).second) {
      error = "duplicate toolchain contract field '" + name + "' on line " +
              std::to_string(lineNumber);
      return false;
    }
    if (value != expectedField->second) {
      error = "incompatible toolchain contract field '" + name + "': found '" +
              value + "', expected '" + expectedField->second + "'";
      return false;
    }
    parsed.emplace_back(std::move(name), std::move(value));
  }
  if (stream.bad()) {
    error = "cannot read toolchain contract '" + source.string() + "'";
    return false;
  }

  for (const Field &field : expected) {
    if (!seen.contains(field.first)) {
      error = "missing toolchain contract field '" + field.first + "'";
      return false;
    }
  }
  for (std::size_t index = 0; index < expected.size(); ++index) {
    if (parsed[index].first != expected[index].first) {
      error = "toolchain contract field '" + parsed[index].first +
              "' is out of canonical order; expected '" +
              expected[index].first + "'";
      return false;
    }
  }
  return true;
}
} // namespace nativesharp::driver
