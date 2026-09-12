#include "nativesharp/Driver/InstallationVerifier.h"

#include "nativesharp/Driver/CompatibilityPolicy.h"
#include "nativesharp/Driver/MigrationBaseline.h"
#include "nativesharp/Driver/ToolchainContract.h"

#include <array>
#include <string_view>
#include <system_error>

namespace nativesharp::driver {
namespace {
struct RequiredFile final {
  std::string_view relativePath;
  std::string_view component;
};

constexpr std::array<RequiredFile, 5> kRequiredLibraries = {{
    {"lib/NativeSharp.Core/NativeSharp.Core.nsproj",
     "NativeSharp.Core project manifest"},
    {"lib/NativeSharp.Core.Collections.Generic/"
     "NativeSharp.Core.Collections.Generic.nsproj",
     "NativeSharp.Core.Collections.Generic project manifest"},
    {"lib/NativeSharp.Core.Diagnostics/NativeSharp.Core.Diagnostics.nsproj",
     "NativeSharp.Core.Diagnostics project manifest"},
    {"lib/NativeSharp.Core.IO/NativeSharp.Core.IO.nsproj",
     "NativeSharp.Core.IO project manifest"},
    {"lib/NativeSharp.Core.Threading/NativeSharp.Core.Threading.nsproj",
     "NativeSharp.Core.Threading project manifest"},
}};

constexpr std::array<RequiredFile, 5> kRequiredSpecifications = {{
    {"share/nativesharp/spec/NativeSharp_Language_Specification_1_0.md",
     "language specification"},
    {"share/nativesharp/spec/NativeSharp_Grammar_1_0.ebnf", "language grammar"},
    {"share/nativesharp/spec/NativeSharp_1_0_conformance.map",
     "language conformance map"},
    {"share/nativesharp/spec/NativeSharp_Compiler_Contract_1_0.md",
     "compiler contract"},
    {"share/nativesharp/spec/NativeSharp_Compiler_1_0_conformance.map",
     "compiler conformance map"},
}};

constexpr std::array<RequiredFile, 7> kRequiredCurrentDocumentation = {{
    {"share/nativesharp/docs/README.md", "documentation version index"},
    {"share/nativesharp/docs/1.0.1/README.md", "1.0.1 documentation index"},
    {"share/nativesharp/docs/1.0.1/VERSION.md", "1.0.1 version contract"},
    {"share/nativesharp/docs/1.0.1/language/NativeSharp_Language_Specification_1_0_1.md",
     "1.0.1 language specification"},
    {"share/nativesharp/docs/1.0.1/compiler/NativeSharp_Compiler_Contract_1_0_1.md",
     "1.0.1 compiler contract"},
    {"share/nativesharp/docs/1.0.1/debug/NativeSharp_Debug_Artifact_Contract_1_0_1.md",
     "1.0.1 debug artifact contract"},
    {"share/nativesharp/docs/1.0.1/book/NativeSharp_El_Lenguaje_de_Programacion_ES.md",
     "1.0.1 programming reference"},
}};

bool requireRegularFile(const std::filesystem::path &prefix,
                        const RequiredFile &required, std::string &error) {
  const std::filesystem::path path =
      prefix / std::filesystem::path(std::string(required.relativePath));
  std::error_code filesystemError;
  const bool regular = std::filesystem::is_regular_file(path, filesystemError);
  if (regular && !filesystemError) {
    return true;
  }

  error = "installation is missing " + std::string(required.component) +
          " at '" + path.string() + "'";
  if (filesystemError) {
    error += ": " + filesystemError.message();
  }
  return false;
}
} // namespace

bool verifyInstallation(const std::filesystem::path &prefix,
                        std::string &error) {
  error.clear();
  if (prefix.empty()) {
    error = "NativeSharp installation prefix is empty";
    return false;
  }

  std::error_code filesystemError;
  if (!std::filesystem::is_directory(prefix, filesystemError) ||
      filesystemError) {
    error = "NativeSharp installation prefix is not a readable directory: '" +
            prefix.string() + "'";
    if (filesystemError) {
      error += ": " + filesystemError.message();
    }
    return false;
  }

  if (!requireRegularFile(prefix, {"bin/nsc.exe", "compiler executable"},
                          error)) {
    return false;
  }

  const std::filesystem::path contractPath =
      prefix / "share/nativesharp/toolchain/NativeSharpToolchain.contract";
  std::string contractError;
  if (!verifyToolchainContract(contractPath, contractError)) {
    error = "installed toolchain contract is invalid: " + contractError;
    return false;
  }

  const std::filesystem::path policyPath =
      prefix / "share/nativesharp/toolchain/NativeSharpCompatibility.policy";
  std::string policyError;
  if (!verifyCompatibilityPolicy(policyPath, policyError)) {
    error = "installed compatibility policy is invalid: " + policyError;
    return false;
  }

  const std::filesystem::path migrationPath =
      prefix / "share/nativesharp/toolchain/NativeSharp2Migration.baseline";
  std::string migrationError;
  if (!verifyMigrationBaseline(migrationPath, migrationError)) {
    error = "installed migration baseline is invalid: " + migrationError;
    return false;
  }

  for (const RequiredFile &required : kRequiredLibraries) {
    if (!requireRegularFile(prefix, required, error)) {
      return false;
    }
  }
  for (const RequiredFile &required : kRequiredSpecifications) {
    if (!requireRegularFile(prefix, required, error)) {
      return false;
    }
  }
  for (const RequiredFile &required : kRequiredCurrentDocumentation) {
    if (!requireRegularFile(prefix, required, error)) {
      return false;
    }
  }
  return true;
}
} // namespace nativesharp::driver
