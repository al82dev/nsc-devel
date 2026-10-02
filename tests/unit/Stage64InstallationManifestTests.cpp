#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Driver/InstallationManifest.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
bool writeFile(const std::filesystem::path &path, std::string_view contents) {
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

std::string replaceOnce(std::string text, std::string_view from,
                        std::string_view to) {
  const std::size_t position = text.find(from);
  if (position != std::string::npos) {
    text.replace(position, from.size(), to);
  }
  return text;
}

std::string expectedListText() {
  return "NSFILES 1\n"
         "bin/nsc.exe\n"
         "lib/Core/A.ns\n"
         "sdk/win64/nativesharp-runtime.nsimports\n"
         "share/nativesharp/spec/spec.md\n"
         "share/nativesharp/toolchain/NativeSharpInstallation.files\n"
         "share/nativesharp/toolchain/NativeSharpToolchain.contract\n";
}

bool populatePackage(const std::filesystem::path &prefix) {
  return writeFile(prefix / "bin/nsc.exe", "compiler") &&
         writeFile(prefix / "lib/Core/A.ns", "alpha") &&
         writeFile(prefix / "sdk/win64/nativesharp-runtime.nsimports",
                   "KERNEL32.dll:ExitProcess\n") &&
         writeFile(prefix / "share/nativesharp/spec/spec.md",
                   "specification") &&
         writeFile(
             prefix /
                 "share/nativesharp/toolchain/NativeSharpInstallation.files",
             expectedListText()) &&
         writeFile(
             prefix /
                 "share/nativesharp/toolchain/NativeSharpToolchain.contract",
             "contract");
}
} // namespace

int main() {
  using namespace nativesharp;
  using nativesharp::tests::TestContext;

  TestContext tests;
  const std::filesystem::path evidence =
      tests::evidenceRoot("nativesharp_stage64_installation_manifest");
  std::error_code filesystemError;
  std::filesystem::remove_all(evidence, filesystemError);
  filesystemError.clear();
  std::filesystem::create_directories(evidence, filesystemError);
  tests.expect(!filesystemError,
               "isolated Stage 64 evidence directory is available");

  const std::vector<std::uint8_t> empty;
  const std::vector<std::uint8_t> abc = {'a', 'b', 'c'};
  tests.expect(
      basic::sha256Hex(basic::sha256(empty)) ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
      "shared SHA-256 authority matches the empty-string standard vector");
  tests.expect(
      basic::sha256Hex(basic::sha256(abc)) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
      "shared SHA-256 authority matches the abc standard vector");

  const std::filesystem::path prefix = evidence / "package";
  const std::filesystem::path fileList =
      prefix / "share/nativesharp/toolchain/NativeSharpInstallation.files";
  const std::filesystem::path manifest =
      prefix / "share/nativesharp/toolchain/NativeSharpInstallation.manifest";
  tests.expect(populatePackage(prefix),
               "test constructs the expected managed payload");

  basic::Sha256Digest fileDigest{};
  std::uintmax_t fileSize = 0;
  std::string error;
  tests.expect(
      basic::sha256File(prefix / "lib/Core/A.ns", fileDigest, fileSize, error),
      "shared SHA-256 utility reads an installed file");
  tests.expect(fileSize == 5U && basic::sha256Hex(fileDigest).size() == 64U &&
                   error.empty(),
               "file hashing returns byte size, lowercase digest and no error");

  tests.expect(
      driver::writeInstallationManifest(prefix, fileList, manifest, error),
      "writer inventories every path selected by the expected-file list");
  tests.expect(error.empty(),
               "successful manifest generation leaves no error text");
  const std::string canonicalManifest = readFile(manifest);
  tests.expect(canonicalManifest.starts_with("NSINSTALL 1\n"),
               "manifest starts with a versioned canonical header");
  tests.expect(canonicalManifest.ends_with('\n'),
               "manifest has one canonical final newline");
  tests.expect(canonicalManifest.find("|5|lib/Core/A.ns\n") !=
                   std::string::npos,
               "manifest records decimal size and normalized relative path");
  tests.expect(canonicalManifest.find(basic::sha256Hex(fileDigest) +
                                      "|5|lib/Core/A.ns\n") !=
                   std::string::npos,
               "manifest records the exact lowercase SHA-256 of payload bytes");
  tests.expect(canonicalManifest.find("NativeSharpInstallation.manifest") ==
                   std::string::npos,
               "manifest excludes itself and avoids a circular digest");
  tests.expect(
      canonicalManifest.find("NativeSharpInstallation.files") !=
          std::string::npos,
      "manifest authenticates the expected-file list that selected its scope");
  tests.expect(driver::verifyInstallationManifest(prefix, manifest, error),
               "freshly generated complete package verifies");
  tests.expect(error.empty(),
               "successful manifest verification leaves no error text");
  tests.expect(writeFile(prefix / "bin/nslink.exe", "separate-tool"),
               "coexisting tool fixture is written beside nsc");
  tests.expect(driver::verifyInstallationManifest(prefix, manifest, error),
               "shared bin directory accepts tools owned by a separate "
               "NativeSharp package");
  std::filesystem::remove(prefix / "bin/nslink.exe", filesystemError);
  tests.expect(writeFile(prefix / "lib/NativeAsmCore.lib", "separate-library"),
               "coexisting library fixture is written beside Core");
  tests.expect(
      driver::verifyInstallationManifest(prefix, manifest, error),
      "shared lib directory accepts archives owned by a separate package");

  const std::filesystem::path secondManifest = evidence / "second.manifest";
  tests.expect(driver::writeInstallationManifest(prefix, fileList,
                                                 secondManifest, error),
               "same package can be inventoried at a second destination");
  tests.expect(
      readFile(secondManifest) == canonicalManifest,
      "identical package bytes produce an identical deterministic manifest");

  tests.expect(writeFile(prefix / "lib/Core/A.ns", "ALPHA"),
               "same-size tamper fixture is written");
  tests.expect(
      !driver::verifyInstallationManifest(prefix, manifest, error) &&
          error.find("SHA-256 mismatch for 'lib/Core/A.ns'") !=
              std::string::npos,
      "same-size content tampering is rejected by digest and names the file");
  tests.expect(writeFile(prefix / "lib/Core/A.ns", "alpha!"),
               "size tamper fixture is written");
  tests.expect(!driver::verifyInstallationManifest(prefix, manifest, error) &&
                   error.find("size mismatch for 'lib/Core/A.ns'") !=
                       std::string::npos,
               "size tampering is rejected before hash comparison");
  tests.expect(writeFile(prefix / "lib/Core/A.ns", "alpha"),
               "payload fixture is restored");

  std::filesystem::remove(prefix / "share/nativesharp/spec/spec.md",
                          filesystemError);
  tests.expect(
      !driver::verifyInstallationManifest(prefix, manifest, error) &&
          error.find("entry is missing 'share/nativesharp/spec/spec.md'") !=
              std::string::npos,
      "missing inventoried file is rejected with its normalized path");
  tests.expect(
      writeFile(prefix / "share/nativesharp/spec/spec.md", "specification"),
      "missing payload fixture is restored");
  tests.expect(writeFile(prefix / "lib/Core/stale.ns", "stale"),
               "unexpected stale-file fixture is written");
  tests.expect(
      !driver::verifyInstallationManifest(prefix, manifest, error) &&
          error.find("unexpected installed file 'lib/Core/stale.ns'") !=
              std::string::npos,
      "residue inside an nsc-owned installation tree is rejected");
  std::filesystem::remove(prefix / "lib/Core/stale.ns", filesystemError);
  tests.expect(driver::verifyInstallationManifest(prefix, manifest, error),
               "restored package returns to a valid state");
  tests.expect(writeFile(prefix / "sdk/win64/stale.nsimports", "stale"),
               "unexpected SDK residue fixture is written");
  tests.expect(
      !driver::verifyInstallationManifest(prefix, manifest, error) &&
          error.find("unexpected installed file 'sdk/win64/stale.nsimports'") !=
              std::string::npos,
      "residue inside the managed Win64 SDK tree is rejected");
  std::filesystem::remove(prefix / "sdk/win64/stale.nsimports",
                          filesystemError);
  tests.expect(driver::verifyInstallationManifest(prefix, manifest, error),
               "SDK cleanup restores the package to a valid state");
  std::filesystem::remove(prefix / "lib/NativeAsmCore.lib", filesystemError);

  const std::filesystem::path malformed = evidence / "malformed.manifest";
  tests.expect(
      writeFile(malformed,
                replaceOnce(canonicalManifest, "NSINSTALL 1", "NSINSTALL 9")) &&
          !driver::verifyInstallationManifest(prefix, malformed, error) &&
          error.find("unsupported installation manifest header") !=
              std::string::npos,
      "future manifest format is rejected by header");
  tests.expect(
      writeFile(malformed, replaceOnce(canonicalManifest, "|5|lib/Core/A.ns",
                                       "|05|lib/Core/A.ns")) &&
          !driver::verifyInstallationManifest(prefix, malformed, error) &&
          error.find("invalid size") != std::string::npos,
      "non-canonical decimal sizes are rejected");
  tests.expect(
      writeFile(malformed, replaceOnce(canonicalManifest, "lib/Core/A.ns",
                                       "lib/../Core/A.ns")) &&
          !driver::verifyInstallationManifest(prefix, malformed, error) &&
          error.find("non-canonical installation manifest path") !=
              std::string::npos,
      "path traversal is rejected before touching the prefix");
  const std::size_t firstDigest = canonicalManifest.find('\n') + 1U;
  tests.expect(
      writeFile(malformed, canonicalManifest.substr(0, firstDigest) +
                               std::string(64U, 'A') +
                               canonicalManifest.substr(firstDigest + 64U)) &&
          !driver::verifyInstallationManifest(prefix, malformed, error) &&
          error.find("invalid SHA-256") != std::string::npos,
      "uppercase digest is rejected from the canonical format");

  const std::size_t firstEnd =
      canonicalManifest.find('\n', canonicalManifest.find('\n') + 1U);
  const std::size_t secondEnd = canonicalManifest.find('\n', firstEnd + 1U);
  const std::string firstEntry =
      canonicalManifest.substr(canonicalManifest.find('\n') + 1U,
                               firstEnd - canonicalManifest.find('\n'));
  const std::string secondEntry =
      canonicalManifest.substr(firstEnd + 1U, secondEnd - firstEnd);
  const std::string reordered =
      canonicalManifest.substr(0, canonicalManifest.find('\n') + 1U) +
      secondEntry + firstEntry + canonicalManifest.substr(secondEnd + 1U);
  tests.expect(
      writeFile(malformed, reordered) &&
          !driver::verifyInstallationManifest(prefix, malformed, error) &&
          error.find("out of canonical order") != std::string::npos,
      "otherwise valid entries cannot be reordered");

  const std::filesystem::path badList = evidence / "bad.files";
  tests.expect(writeFile(badList, "NSFILES 9\nbin/nsc.exe\n") &&
                   !driver::writeInstallationManifest(prefix, badList,
                                                      secondManifest, error) &&
                   error.find("unsupported installation file list header") !=
                       std::string::npos,
               "future expected-list format is rejected");
  tests.expect(writeFile(badList, "NSFILES 1\nlib/../bin/nsc.exe\n") &&
                   !driver::writeInstallationManifest(prefix, badList,
                                                      secondManifest, error) &&
                   error.find("non-canonical installation file list path") !=
                       std::string::npos,
               "expected list rejects traversal paths");
  tests.expect(writeFile(badList, "NSFILES 1\nlib/Core/A.ns\nbin/nsc.exe\n") &&
                   !driver::writeInstallationManifest(prefix, badList,
                                                      secondManifest, error) &&
                   error.find("out of canonical order") != std::string::npos,
               "expected list requires deterministic ordering");
  tests.expect(writeFile(badList, "NSFILES 1\nbin/nsc.exe\nbin/nsc.exe\n") &&
                   !driver::writeInstallationManifest(prefix, badList,
                                                      secondManifest, error) &&
                   error.find("duplicate installation file list path") !=
                       std::string::npos,
               "expected list rejects duplicate paths");
  tests.expect(
      writeFile(
          badList,
          "NSFILES 1\n"
          "bin/nsc.exe\n"
          "lib/Core/A.ns\n"
          "share/nativesharp/spec/spec.md\n"
          "share/nativesharp/toolchain/NativeSharpInstallation.files\n"
          "share/nativesharp/toolchain/NativeSharpInstallation.manifest\n"
          "share/nativesharp/toolchain/NativeSharpToolchain.contract\n") &&
          !driver::writeInstallationManifest(prefix, badList, manifest,
                                             error) &&
          error.find("cannot inventory itself") != std::string::npos,
      "canonical expected list cannot introduce the circular manifest digest");

  const std::filesystem::path preservedManifest =
      evidence / "preserved.manifest";
  tests.expect(writeFile(preservedManifest, "known-good"),
               "previous-manifest preservation fixture is written");
  tests.expect(writeFile(prefix / "lib/Core/A.ns", "alpha") &&
                   writeFile(badList, "NSFILES 1\n"
                                      "bin/nsc.exe\n"
                                      "lib/Core/A.ns\n"
                                      "lib/Core/Missing.ns\n") &&
                   !driver::writeInstallationManifest(
                       prefix, badList, preservedManifest, error) &&
                   readFile(preservedManifest) == "known-good",
               "input failure preserves the previous complete manifest instead "
               "of truncating it");

  driver::Driver compiler;
  const std::string prefixText = prefix.string();
  const std::string fileListText = fileList.string();
  const std::string manifestText = manifest.string();
  const std::vector<std::string_view> writeArguments = {
      "--write-installation-manifest", prefixText, fileListText, manifestText};
  std::ostringstream output;
  std::ostringstream driverError;
  tests.expect(compiler.run(writeArguments, output, driverError) ==
                   ExitCode::Success,
               "public writer command produces the installation inventory");
  tests.expect(output.str().find("installation manifest written:") !=
                       std::string::npos &&
                   driverError.str().empty(),
               "public writer reports its destination without stderr noise");

  const std::vector<std::string_view> verifyArguments = {
      "--verify-installation-manifest", prefixText, manifestText};
  output.str({});
  output.clear();
  tests.expect(compiler.run(verifyArguments, output, driverError) ==
                   ExitCode::Success,
               "public verifier command accepts the current package bytes");
  tests.expect(output.str().find("installation manifest verified:") !=
                   std::string::npos,
               "public verifier acknowledges the manifest path");
  tests.expect(writeFile(prefix / "lib/Core/A.ns", "ALPHA"),
               "driver tamper fixture is written");
  output.str({});
  output.clear();
  driverError.str({});
  driverError.clear();
  tests.expect(
      compiler.run(verifyArguments, output, driverError) ==
          ExitCode::CompilationError,
      "public verifier classifies integrity failure as controlled input error");
  tests.expect(driverError.str().find(
                   "nsc: error: installation manifest SHA-256 mismatch") !=
                   std::string::npos,
               "public verifier preserves the precise integrity diagnostic");

  const std::filesystem::path sourceRoot = NSC_SOURCE_DIR;
  const std::string cmake = readFile(sourceRoot / "CMakeLists.txt");
  const std::string installScript =
      readFile(sourceRoot / "cmake/VerifyInstalledNativeSharp.cmake");
  tests.expect(
      cmake.find("NSC_INSTALLATION_EXPECTED_PATHS") != std::string::npos &&
          cmake.find("NativeSharpInstallation.files") != std::string::npos &&
          cmake.find("list(SORT NSC_INSTALLATION_EXPECTED_PATHS)") !=
              std::string::npos,
      "CMake derives and sorts the expected payload before installation");
  tests.expect(installScript.find("--write-installation-manifest") !=
                       std::string::npos &&
                   installScript.find("--verify-installation-manifest") !=
                       std::string::npos &&
                   installScript.find("FATAL_ERROR") != std::string::npos,
               "installed compiler generates and verifies inventory as fatal "
               "install gates");

  const std::string normative =
      readFile(sourceRoot / "spec/NativeSharp_Compiler_Contract_1_0.md");
  const std::string conformance =
      readFile(sourceRoot / "spec/NativeSharp_Compiler_1_0_conformance.map");
  tests.expect(normative.find("## NSC-PKG-004 —") != std::string::npos &&
                   normative.find("## NSC-PKG-005 —") != std::string::npos,
               "normative compiler contract defines inventory production and "
               "verification");
  tests.expect(
      conformance.find("NSC-PKG-004|implemented|") != std::string::npos &&
          conformance.find("NSC-PKG-005|implemented|") != std::string::npos,
      "inventory clauses map to implementation and executable evidence");

  return tests.resultKeeping(evidence);
}
