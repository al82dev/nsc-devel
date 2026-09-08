#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Driver/CompatibilityPolicy.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Driver/InstallationVerifier.h"
#include "nativesharp/Driver/ToolchainContract.h"

#include "../support/TestContext.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr std::array<std::string_view, 11> kRequiredFiles = {
    "bin/nsc.exe",
    "lib/NativeSharp.Core/NativeSharp.Core.nsproj",
    "lib/NativeSharp.Core.Collections.Generic/"
    "NativeSharp.Core.Collections.Generic.nsproj",
    "lib/NativeSharp.Core.Diagnostics/NativeSharp.Core.Diagnostics.nsproj",
    "lib/NativeSharp.Core.IO/NativeSharp.Core.IO.nsproj",
    "lib/NativeSharp.Core.Threading/NativeSharp.Core.Threading.nsproj",
    "share/nativesharp/spec/NativeSharp_Language_Specification_1_0.md",
    "share/nativesharp/spec/NativeSharp_Grammar_1_0.ebnf",
    "share/nativesharp/spec/NativeSharp_1_0_conformance.map",
    "share/nativesharp/spec/NativeSharp_Compiler_Contract_1_0.md",
    "share/nativesharp/spec/NativeSharp_Compiler_1_0_conformance.map",
};

bool writeFile(const std::filesystem::path &path,
               std::string_view contents = "fixture") {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) {
    return false;
  }
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  return static_cast<bool>(stream);
}

std::string readFile(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(stream),
                     std::istreambuf_iterator<char>());
}

bool populateInstallation(const std::filesystem::path &prefix) {
  for (const std::string_view relative : kRequiredFiles) {
    if (!writeFile(prefix / std::filesystem::path(std::string(relative)))) {
      return false;
    }
  }
  std::string error;
  return nativesharp::driver::writeToolchainContract(
             prefix /
                 "share/nativesharp/toolchain/NativeSharpToolchain.contract",
             error) &&
         nativesharp::driver::writeCompatibilityPolicy(
             prefix /
                 "share/nativesharp/toolchain/NativeSharpCompatibility.policy",
             error);
}
} // namespace

int main() {
  using namespace nativesharp;
  using nativesharp::tests::TestContext;

  TestContext tests;
  const std::filesystem::path evidence = nativesharp::tests::evidenceRoot(
      "nativesharp_stage63_installation_verifier");
  std::error_code filesystemError;
  std::filesystem::remove_all(evidence, filesystemError);
  filesystemError.clear();
  std::filesystem::create_directories(evidence, filesystemError);
  tests.expect(!filesystemError,
               "isolated Stage 63 evidence directory is available");

  std::string error;
  tests.expect(!driver::verifyInstallation({}, error) &&
                   error.find("prefix is empty") != std::string::npos,
               "empty installation prefix is rejected precisely");

  const std::filesystem::path absent = evidence / "absent";
  tests.expect(
      !driver::verifyInstallation(absent, error) &&
          error.find("not a readable directory") != std::string::npos,
      "absent installation root is rejected before inspecting components");

  const std::filesystem::path prefix = evidence / "complete";
  tests.expect(populateInstallation(prefix),
               "test constructs the complete minimum package layout");
  tests.expect(driver::verifyInstallation(prefix, error),
               "complete current installation is accepted");
  tests.expect(error.empty(),
               "successful installation verification leaves no error text");

  const std::filesystem::path compiler = prefix / "bin/nsc.exe";
  std::filesystem::remove(compiler, filesystemError);
  tests.expect(!driver::verifyInstallation(prefix, error) &&
                   error.find("missing compiler executable") !=
                       std::string::npos,
               "missing compiler executable names its expected path");
  tests.expect(writeFile(compiler), "compiler fixture is restored");

  const std::filesystem::path contract =
      prefix / "share/nativesharp/toolchain/NativeSharpToolchain.contract";
  const std::string currentContract = driver::currentToolchainContract();
  std::string incompatibleContract = currentContract;
  const std::string stageField = "stage=" + std::string(kCompilerStage);
  const std::string incompatibleStage =
      kCompilerStage == "0" ? "stage=1" : "stage=0";
  const std::size_t stage = incompatibleContract.find(stageField);
  tests.expect(stage != std::string::npos,
               "fixture found the current stage field before corrupting it");
  if (stage != std::string::npos) {
    incompatibleContract.replace(stage, stageField.size(), incompatibleStage);
  }
  tests.expect(writeFile(contract, incompatibleContract),
               "incompatible contract fixture is written");
  tests.expect(
      !driver::verifyInstallation(prefix, error) &&
          error.find("installed toolchain contract is invalid") !=
              std::string::npos &&
          error.find("incompatible toolchain contract field 'stage'") !=
              std::string::npos,
      "installation composes the exact toolchain-contract diagnostic");
  tests.expect(writeFile(contract, currentContract),
               "current contract fixture is restored");

  const std::filesystem::path policy =
      prefix / "share/nativesharp/toolchain/NativeSharpCompatibility.policy";
  const std::string currentPolicy = driver::currentCompatibilityPolicy();
  tests.expect(writeFile(policy, "NSCOMPAT 9\n"),
               "incompatible compatibility policy fixture is written");
  tests.expect(!driver::verifyInstallation(prefix, error) &&
                   error.find("installed compatibility policy is invalid") !=
                       std::string::npos,
               "installation composes compatibility-policy diagnostics");
  tests.expect(writeFile(policy, currentPolicy),
               "current compatibility policy fixture is restored");

  const std::filesystem::path collection =
      prefix / "lib/NativeSharp.Core.Collections.Generic/"
               "NativeSharp.Core.Collections.Generic.nsproj";
  std::filesystem::remove(collection, filesystemError);
  tests.expect(!driver::verifyInstallation(prefix, error) &&
                   error.find("missing NativeSharp.Core.Collections.Generic") !=
                       std::string::npos,
               "missing distributed library names the exact Core component");
  tests.expect(writeFile(collection), "library fixture is restored");

  const std::filesystem::path compilerContract =
      prefix / "share/nativesharp/spec/NativeSharp_Compiler_Contract_1_0.md";
  std::filesystem::remove(compilerContract, filesystemError);
  tests.expect(
      !driver::verifyInstallation(prefix, error) &&
          error.find("missing compiler contract") != std::string::npos,
      "missing normative document names the exact specification component");
  tests.expect(writeFile(compilerContract),
               "specification fixture is restored");
  tests.expect(driver::verifyInstallation(prefix, error),
               "restored package returns to a valid state");

  driver::Driver compilerDriver;
  const std::string prefixText = prefix.string();
  const std::vector<std::string_view> arguments = {"--verify-installation",
                                                   prefixText};
  std::ostringstream output;
  std::ostringstream driverError;
  tests.expect(compilerDriver.run(arguments, output, driverError) ==
                   ExitCode::Success,
               "public installation command accepts the complete package");
  tests.expect(
      output.str().find("NativeSharp installation verified:") !=
              std::string::npos &&
          driverError.str().empty(),
      "public command reports its verified prefix without stderr noise");

  const std::string absentText = absent.string();
  const std::vector<std::string_view> absentArguments = {
      "--verify-installation", absentText};
  output.str({});
  output.clear();
  tests.expect(compilerDriver.run(absentArguments, output, driverError) ==
                   ExitCode::CompilationError,
               "invalid installed layout returns the controlled "
               "compilation-error class");
  tests.expect(
      driverError.str().find("nsc: error: NativeSharp installation prefix") !=
          std::string::npos,
      "driver preserves a useful installation diagnostic");

  const std::filesystem::path sourceRoot = NSC_SOURCE_DIR;
  const std::string cmake = readFile(sourceRoot / "CMakeLists.txt");
  const std::string installScript =
      readFile(sourceRoot / "cmake/VerifyInstalledNativeSharp.cmake");
  tests.expect(
      cmake.find("install(SCRIPT "
                 "\"${CMAKE_CURRENT_SOURCE_DIR}/cmake/"
                 "VerifyInstalledNativeSharp.cmake\")") != std::string::npos,
      "CMake runs installed-layout verification after copying package files");
  tests.expect(
      installScript.find("--verify-installation") != std::string::npos &&
          installScript.find("FATAL_ERROR") != std::string::npos,
      "install script invokes the installed compiler and makes failure fatal");

  const std::string normative =
      readFile(sourceRoot / "spec/NativeSharp_Compiler_Contract_1_0.md");
  const std::string conformance =
      readFile(sourceRoot / "spec/NativeSharp_Compiler_1_0_conformance.map");
  tests.expect(
      normative.find("## NSC-PKG-003 —") != std::string::npos,
      "normative compiler contract defines installed-layout verification");
  tests.expect(
      conformance.find("NSC-PKG-003|implemented|") != std::string::npos,
      "installed-layout clause is connected to implementation and evidence");

  return tests.resultKeeping(evidence);
}
