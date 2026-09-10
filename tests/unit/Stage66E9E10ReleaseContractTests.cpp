#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Driver/CompatibilityPolicy.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Driver/InstallationManifest.h"
#include "nativesharp/Driver/PackageLifecycle.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <process.h>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
bool writeFile(const std::filesystem::path &path, std::string_view contents)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
    {
        return false;
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return static_cast<bool>(stream);
}

std::string readFile(const std::filesystem::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

bool createManifest(const std::filesystem::path &root,
    std::string_view compilerBytes,
    std::string_view commonBytes,
    std::string_view changingBytes,
    bool oldFile,
    bool newFile,
    std::filesystem::path &manifest,
    std::string &error)
{
    std::string list = "NSFILES 1\n"
                       "bin/nsc.exe\n"
                       "lib/Core/Changing.ns\n"
                       "lib/Core/Common.ns\n";
    if (newFile)
    {
        list += "lib/Core/New.ns\n";
    }
    if (oldFile)
    {
        list += "lib/Core/Old.ns\n";
    }
    list += "share/nativesharp/toolchain/NativeSharpInstallation.files\n";

    const std::filesystem::path listPath = root / "share/nativesharp/toolchain/NativeSharpInstallation.files";
    manifest = root / "share/nativesharp/toolchain/NativeSharpInstallation.manifest";
    if (!writeFile(root / "bin/nsc.exe", compilerBytes) || !writeFile(root / "lib/Core/Common.ns", commonBytes) ||
        !writeFile(root / "lib/Core/Changing.ns", changingBytes) ||
        (oldFile && !writeFile(root / "lib/Core/Old.ns", "retired")) ||
        (newFile && !writeFile(root / "lib/Core/New.ns", "added")) || !writeFile(listPath, list))
    {
        error = "test could not populate package";
        return false;
    }
    return nativesharp::driver::writeInstallationManifest(root, listPath, manifest, error);
}

std::vector<std::string_view> views(const std::vector<std::string> &values)
{
    std::vector<std::string_view> result;
    result.reserve(values.size());
    for (const std::string &value : values)
    {
        result.push_back(value);
    }
    return result;
}
} // namespace

int main()
{
    using namespace nativesharp;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const std::filesystem::path evidence =
        tests::evidenceRoot("nativesharp_stage66_e9_e10_release_contract") / std::to_string(_getpid());
    std::error_code filesystemError;
    std::filesystem::remove_all(evidence, filesystemError);
    filesystemError.clear();
    std::filesystem::create_directories(evidence, filesystemError);
    tests.expect(!filesystemError, "isolated E9/E10 evidence directory is available");

    std::string error;
    std::filesystem::path leftManifest;
    std::filesystem::path rightManifest;
    const std::filesystem::path left = evidence / "build-a";
    const std::filesystem::path right = evidence / "build-b";
    tests.expect(createManifest(left, "compiler build A", "stable", "same", true, false, leftManifest, error),
        "first independent installation manifest is created");
    tests.expect(createManifest(right, "compiler build B", "stable", "same", true, false, rightManifest, error),
        "second independent installation manifest is created");

    driver::ReproducibilityComparison comparison;
    tests.expect(driver::compareInstallationManifests(leftManifest, rightManifest, comparison, error),
        "two canonical installation manifests can be compared");
    tests.expect(comparison.reproducible, "build-id-only binary variation is explicitly reproducible");
    tests.expect(comparison.volatileEntries == 1, "only nsc.exe is classified as volatile in the fixture");
    tests.expect(comparison.deterministicEntries == 4, "every remaining package entry is compared byte-for-byte");
    tests.expect(comparison.differences == 0, "equivalent independent builds have no deterministic difference");
    tests.expect(comparison.report.starts_with("NSREPRO 1\n") &&
                     comparison.report.find("result=reproducible\n") != std::string::npos &&
                     comparison.report.find("volatile=build-identity|bin/nsc.exe\n") != std::string::npos,
        "reproducibility report is canonical and explains volatility");

    tests.expect(createManifest(right, "compiler build C", "changed", "same", true, false, rightManifest, error),
        "content-divergence fixture is regenerated");
    tests.expect(driver::compareInstallationManifests(leftManifest, rightManifest, comparison, error),
        "divergent manifests remain structurally comparable");
    tests.expect(!comparison.reproducible && comparison.differences == 1 &&
                     comparison.report.find("difference=content|lib/Core/Common.ns") != std::string::npos,
        "one unexplained content change fails with its exact path");

    std::filesystem::path nextManifest;
    const std::filesystem::path next = evidence / "next";
    tests.expect(createManifest(next, "compiler next", "stable", "replacement", false, true, nextManifest, error),
        "next-version manifest is created independently");
    const std::filesystem::path updatePlan = evidence / "update.plan";
    tests.expect(driver::writeInstallationUpdatePlan(leftManifest, nextManifest, updatePlan, error),
        "authenticated update plan is generated");
    const std::string updateText = readFile(updatePlan);
    tests.expect(updateText.starts_with("NSUPDATE 1\n") &&
                     updateText.find("keep|lib/Core/Common.ns\n") != std::string::npos &&
                     updateText.find("replace|lib/Core/Changing.ns\n") != std::string::npos &&
                     updateText.find("add|lib/Core/New.ns\n") != std::string::npos &&
                     updateText.find("remove|lib/Core/Old.ns\n") != std::string::npos,
        "plan distinguishes keep, replace, add and remove actions");
    tests.expect(writeFile(left / "lib/NativeAsmCore.lib", "neighbor"),
        "neighbor package fixture is present in the shared lib directory");
    tests.expect(!driver::applyInstallationRemovalPlan(left, leftManifest, {}, updatePlan, error) &&
                     error.find("requires its bound next installation manifest") != std::string::npos,
        "update application refuses to trust an omitted next manifest");
    tests.expect(std::filesystem::exists(left / "lib/Core/Old.ns"), "missing next-manifest proof performs no removal");
    tests.expect(driver::applyInstallationRemovalPlan(left, leftManifest, nextManifest, updatePlan, error),
        "update removal phase applies against its bound old manifest");
    tests.expect(!std::filesystem::exists(left / "lib/Core/Old.ns"), "retired package-owned file is removed");
    tests.expect(
        std::filesystem::exists(left / "lib/Core/Common.ns") && std::filesystem::exists(left / "lib/Core/Changing.ns"),
        "keep and replace candidates are not deleted by removal phase");
    tests.expect(
        std::filesystem::exists(left / "lib/NativeAsmCore.lib"), "neighbor archive survives package update cleanup");

    const std::filesystem::path removalRoot = evidence / "retirement";
    std::filesystem::path removalManifest;
    tests.expect(createManifest(removalRoot, "compiler", "stable", "same", true, false, removalManifest, error),
        "retirement fixture has a current manifest");
    tests.expect(writeFile(removalRoot / "bin/nslink.exe", "neighbor"), "neighbor executable fixture is present");
    const std::filesystem::path removalPlan = evidence / "retirement.plan";
    tests.expect(driver::writeInstallationRemovalPlan(removalManifest, removalPlan, error),
        "full retirement plan is generated from current ownership");
    tests.expect(readFile(removalPlan).find("next_manifest_sha256=none\n") != std::string::npos &&
                     readFile(removalPlan).find("remove|bin/nsc.exe\n") != std::string::npos,
        "retirement plan is explicit and complete");
    tests.expect(driver::applyInstallationRemovalPlan(removalRoot, removalManifest, {}, removalPlan, error),
        "retirement plan removes all manifest-owned payload");
    tests.expect(!std::filesystem::exists(removalRoot / "bin/nsc.exe") &&
                     !std::filesystem::exists(removalRoot / "lib/Core/Common.ns"),
        "owned retirement targets disappear");
    tests.expect(
        !std::filesystem::exists(removalManifest), "full retirement also removes its non-recursive manifest authority");
    tests.expect(std::filesystem::exists(removalRoot / "bin/nslink.exe"),
        "neighbor executable survives full package retirement");
    tests.expect(std::filesystem::exists(removalRoot / "bin") && std::filesystem::exists(removalRoot / "lib") &&
                     !std::filesystem::exists(removalRoot / "share/nativesharp"),
        "retirement prunes empty package directories but preserves shared roots");

    const std::filesystem::path guardedRoot = evidence / "guarded";
    std::filesystem::path guardedManifest;
    tests.expect(createManifest(guardedRoot, "compiler", "stable", "same", true, false, guardedManifest, error),
        "manifest-binding fixture is created");
    const std::filesystem::path guardedPlan = evidence / "guarded.plan";
    tests.expect(
        driver::writeInstallationRemovalPlan(guardedManifest, guardedPlan, error), "bound removal plan is created");
    std::string changedManifest = readFile(guardedManifest);
    const std::size_t firstDigest = changedManifest.find('\n') + 1U;
    if (firstDigest < changedManifest.size())
    {
        changedManifest[firstDigest] = changedManifest[firstDigest] == '0' ? '1' : '0';
    }
    tests.expect(writeFile(guardedManifest, changedManifest), "current manifest identity is changed after planning");
    tests.expect(!driver::applyInstallationRemovalPlan(guardedRoot, guardedManifest, {}, guardedPlan, error) &&
                     error.find("was created for manifest") != std::string::npos,
        "stale plan cannot act on a different manifest");
    tests.expect(std::filesystem::exists(guardedRoot / "lib/Core/Old.ns"), "binding failure performs no removal");

    const std::filesystem::path alteredPlanRoot = evidence / "altered-plan";
    std::filesystem::path alteredCurrentManifest;
    tests.expect(
        createManifest(alteredPlanRoot, "compiler", "stable", "same", true, false, alteredCurrentManifest, error),
        "altered-plan fixture has a current manifest");
    const std::filesystem::path alteredNextRoot = evidence / "altered-plan-next";
    std::filesystem::path alteredNextManifest;
    tests.expect(
        createManifest(alteredNextRoot, "compiler next", "stable", "same", false, true, alteredNextManifest, error),
        "altered-plan fixture has a next manifest");
    const std::filesystem::path alteredPlan = evidence / "altered.plan";
    tests.expect(driver::writeInstallationUpdatePlan(alteredCurrentManifest, alteredNextManifest, alteredPlan, error),
        "canonical update plan exists before tampering");
    std::string alteredPlanText = readFile(alteredPlan);
    const std::string keepAction = "keep|lib/Core/Common.ns";
    const std::size_t keepPosition = alteredPlanText.find(keepAction);
    if (keepPosition != std::string::npos)
    {
        alteredPlanText.replace(keepPosition, keepAction.size(), "remove|lib/Core/Common.ns");
    }
    tests.expect(writeFile(alteredPlan, alteredPlanText), "one valid-looking action is altered after planning");
    tests.expect(!driver::applyInstallationRemovalPlan(
                     alteredPlanRoot, alteredCurrentManifest, alteredNextManifest, alteredPlan, error) &&
                     error.find("does not match its bound manifests") != std::string::npos,
        "manifest pair detects a changed plan action");
    tests.expect(std::filesystem::exists(alteredPlanRoot / "lib/Core/Common.ns") &&
                     std::filesystem::exists(alteredPlanRoot / "lib/Core/Old.ns"),
        "altered action performs no partial removal");

    const std::filesystem::path modifiedRoot = evidence / "modified-owned-file";
    std::filesystem::path modifiedManifest;
    tests.expect(createManifest(modifiedRoot, "compiler", "stable", "same", true, false, modifiedManifest, error),
        "modified-file fixture has a current manifest");
    const std::filesystem::path modifiedPlan = evidence / "modified.plan";
    tests.expect(driver::writeInstallationRemovalPlan(modifiedManifest, modifiedPlan, error),
        "modified-file fixture has a bound retirement plan");
    tests.expect(writeFile(modifiedRoot / "lib/Core/Old.ns", "user change"),
        "package-owned file is changed after the manifest was recorded");
    tests.expect(!driver::applyInstallationRemovalPlan(modifiedRoot, modifiedManifest, {}, modifiedPlan, error) &&
                     error.find("refuses modified package-owned file") != std::string::npos,
        "retirement refuses to erase modified package-owned content");
    tests.expect(std::filesystem::exists(modifiedRoot / "bin/nsc.exe") &&
                     std::filesystem::exists(modifiedRoot / "lib/Core/Old.ns"),
        "full preflight prevents partial deletion before a late failure");

    const std::string policy = driver::currentCompatibilityPolicy();
    tests.expect(
        policy.starts_with("NSCOMPAT 1\n") && policy.ends_with('\n'), "compatibility policy has canonical framing");
    tests.expect(policy.find("compiler_release=0.9.0\n") != std::string::npos &&
                     policy.find("language_release=0.1.0\n") != std::string::npos,
        "policy separates compiler and language releases");
    tests.expect(policy.find("nsmeta_read=1.0-1.10\n") != std::string::npos &&
                     policy.find("nsmeta_write=1.10\n") != std::string::npos,
        "policy publishes metadata read and write ranges independently");
    tests.expect(
        policy.find("native_abi=1.0\n") != std::string::npos && policy.find("runtime_abi=1.0\n") != std::string::npos,
        "binary compatibility versions are explicit");
    tests.expect(policy.find("reproducibility_report=1\n") != std::string::npos &&
                     policy.find("update_plan=1\n") != std::string::npos,
        "E9 lifecycle formats are frozen by E10 policy");

    const std::filesystem::path policyPath = evidence / "NativeSharpCompatibility.policy";
    tests.expect(driver::writeCompatibilityPolicy(policyPath, error), "compatibility policy writer succeeds");
    tests.expect(readFile(policyPath) == policy, "writer emits exactly the canonical policy bytes");
    tests.expect(driver::verifyCompatibilityPolicy(policyPath, error), "current compatibility policy verifies");
    tests.expect(writeFile(policyPath, "NSCOMPAT 9\n") && !driver::verifyCompatibilityPolicy(policyPath, error) &&
                     error.find("unsupported compatibility policy header") != std::string::npos,
        "future policy format is rejected precisely");
    tests.expect(writeFile(policyPath, policy + "unknown=value\n") &&
                     !driver::verifyCompatibilityPolicy(policyPath, error) &&
                     error.find("unexpected compatibility policy field") != std::string::npos,
        "policy rejects unknown trailing fields");

    std::string explanation;
    tests.expect(driver::checkCompilerCompatibility("0.9.0", explanation, error) &&
                     explanation.find("satisfies") != std::string::npos,
        "current compiler release satisfies its exact requirement");
    tests.expect(!driver::checkCompilerCompatibility("0.9.1", explanation, error) && error.empty() &&
                     explanation.find("does not satisfy") != std::string::npos,
        "newer patch requirement is incompatible");
    tests.expect(!driver::checkCompilerCompatibility("0.8.9", explanation, error) && error.empty(),
        "pre-1.0 minor lines are not silently compatible");
    tests.expect(!driver::checkCompilerCompatibility("0.9", explanation, error) &&
                     error.find("canonical major.minor.patch") != std::string::npos,
        "non-canonical version requirement is rejected");

    driver::Driver compiler;
    std::ostringstream output;
    std::ostringstream driverError;
    const std::vector<std::string_view> printPolicy = {"--print-compatibility-policy"};
    tests.expect(compiler.run(printPolicy, output, driverError) == ExitCode::Success && output.str() == policy &&
                     driverError.str().empty(),
        "public print command exposes canonical policy only");

    output.str({});
    output.clear();
    driverError.str({});
    driverError.clear();
    const std::vector<std::string_view> compatibleRequirement = {"--check-compiler-compatibility", "0.9.0"};
    tests.expect(compiler.run(compatibleRequirement, output, driverError) == ExitCode::Success &&
                     output.str().find("satisfies 0.9.0") != std::string::npos && driverError.str().empty(),
        "public compatibility command returns an executable positive decision");

    tests.expect(
        driver::writeCompatibilityPolicy(policyPath, error), "canonical policy is restored for public verification");
    output.str({});
    output.clear();
    const std::string policyText = policyPath.string();
    std::vector<std::string> verifyPolicyStorage = {"--verify-compatibility-policy", policyText};
    tests.expect(compiler.run(views(verifyPolicyStorage), output, driverError) == ExitCode::Success &&
                     output.str().find("compatibility policy verified") != std::string::npos,
        "public verifier accepts the installed policy format");

    const std::filesystem::path driverRoot = evidence / "driver-lifecycle";
    std::filesystem::path driverCurrentManifest;
    const std::filesystem::path driverNextRoot = evidence / "driver-lifecycle-next";
    std::filesystem::path driverNextManifest;
    tests.expect(
        createManifest(driverRoot, "compiler", "stable", "old", true, false, driverCurrentManifest, error) &&
            createManifest(driverNextRoot, "compiler next", "stable", "new", false, true, driverNextManifest, error),
        "public lifecycle commands receive two complete package fixtures");
    const std::filesystem::path driverPlan = evidence / "driver-lifecycle.plan";
    output.str({});
    output.clear();
    const std::vector<std::string> writePlanStorage = {"--write-installation-update-plan",
        driverCurrentManifest.string(),
        driverNextManifest.string(),
        driverPlan.string()};
    tests.expect(compiler.run(views(writePlanStorage), output, driverError) == ExitCode::Success &&
                     readFile(driverPlan).starts_with("NSUPDATE 1\n"),
        "public update command writes the manifest-bound plan");
    output.str({});
    output.clear();
    const std::vector<std::string> applyPlanStorage = {"--apply-installation-removal-plan",
        driverRoot.string(),
        driverCurrentManifest.string(),
        driverNextManifest.string(),
        driverPlan.string()};
    tests.expect(compiler.run(views(applyPlanStorage), output, driverError) == ExitCode::Success &&
                     !std::filesystem::exists(driverRoot / "lib/Core/Old.ns"),
        "public apply command retires only the path absent from its bound next manifest");

    output.str({});
    output.clear();
    driverError.str({});
    driverError.clear();
    const std::string leftText = leftManifest.string();
    const std::string rightText = rightManifest.string();
    std::vector<std::string> compareStorage = {"--compare-installation-manifests", leftText, rightText};
    const auto compareArguments = views(compareStorage);
    tests.expect(compiler.run(compareArguments, output, driverError) == ExitCode::CompilationError,
        "public comparison command fails on deterministic divergence");
    tests.expect(output.str().find("difference=content|lib/Core/Common.ns") != std::string::npos &&
                     driverError.str().find("installation builds differ") != std::string::npos,
        "public comparison preserves machine report and human error");

    const std::filesystem::path sourceRoot = NSC_SOURCE_DIR;
    const std::string cmake = readFile(sourceRoot / "CMakeLists.txt");
    const std::string installScript = readFile(sourceRoot / "cmake/VerifyInstalledNativeSharp.cmake");
    const std::string reproducibilityScript = readFile(sourceRoot / "cmake/VerifyReproducibleNativeSharpPackage.cmake");
    const std::string contract = readFile(sourceRoot / "spec/NativeSharp_Compiler_Contract_1_0.md");
    const std::string conformance = readFile(sourceRoot / "spec/NativeSharp_Compiler_1_0_conformance.map");
    tests.expect(cmake.find("NativeSharpCompatibility.policy") != std::string::npos &&
                     cmake.find("NSC_COMPATIBILITY_POLICY") != std::string::npos,
        "compatibility policy is generated and installed with package");
    tests.expect(installScript.find("--write-installation-update-plan") != std::string::npos &&
                     installScript.find("--apply-installation-removal-plan") != std::string::npos &&
                     installScript.find("_nsc_previous_manifest") != std::string::npos,
        "normal install upgrades prune only ownership proven by prior manifest");
    const std::size_t writeManifestPosition = installScript.find("--write-installation-manifest");
    const std::size_t writePlanPosition = installScript.find("--write-installation-update-plan");
    const std::size_t applyPlanPosition = installScript.find("--apply-installation-removal-plan");
    const std::size_t verifyLayoutPosition = installScript.find("--verify-installation \"");
    tests.expect(writeManifestPosition < writePlanPosition && writePlanPosition < applyPlanPosition &&
                     applyPlanPosition < verifyLayoutPosition,
        "upgrade retires prior owned files before validating the final layout");
    tests.expect(reproducibilityScript.find("NSC_BUILD_A") != std::string::npos &&
                     reproducibilityScript.find("NSC_BUILD_B") != std::string::npos &&
                     reproducibilityScript.find("NSC_PREFIX_A") != std::string::npos &&
                     reproducibilityScript.find("NSC_PREFIX_B") != std::string::npos &&
                     reproducibilityScript.find("NSC_BUILD_ID_OVERRIDE") != std::string::npos &&
                     reproducibilityScript.find("strict_compiler_binary=identical") != std::string::npos &&
                     reproducibilityScript.find("file(SHA256") != std::string::npos &&
                     reproducibilityScript.find("--compare-installation-manifests") != std::string::npos,
        "public gate compares two independent packages and exact compiler bytes");
    tests.expect(contract.find("## NSC-PKG-006 —") != std::string::npos &&
                     contract.find("## NSC-PKG-007 —") != std::string::npos &&
                     contract.find("## NSC-COMPAT-001 —") != std::string::npos,
        "normative contract defines E9 and E10 obligations");
    tests.expect(conformance.find("NSC-PKG-006|implemented|") != std::string::npos &&
                     conformance.find("NSC-PKG-007|implemented|") != std::string::npos &&
                     conformance.find("NSC-COMPAT-001|implemented|") != std::string::npos,
        "new normative clauses map to code and executable evidence");
    tests.expect(
        std::stoul(std::string(kCompilerStage)) >= 66,
        "the current compiler retains the E9/E10 Stage 66 release contract");

    return tests.resultKeeping(evidence);
}
