#include "nativesharp/Language/SmartPointerSemantics.h"

#include "../support/TestContext.h"

#include <filesystem>

int main(int argc, char** argv)
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;

    const SmartPointerFamilyContract* unique = smartPointerFamilyContract(SmartPointerFamily::Unique);
    const SmartPointerFamilyContract* weakArc = smartPointerFamilyContract(SmartPointerFamily::WeakArc);
    const SmartPointerFamilyContract* rc = smartPointerFamilyContract(SmartPointerFamily::Rc);
    const SmartPointerFamilyContract* arc = smartPointerFamilyContract(SmartPointerFamily::Arc);
    tests.expect(kSmartPointerFamilies.size() == 5 && unique != nullptr && weakArc != nullptr &&
            unique->name == "Unique" && weakArc->weak,
        "E046 freezes the five distinct smart pointer families");
    tests.expect(rc != nullptr && arc != nullptr && !rc->implicitCopy &&
            rc->explicitClone && arc->explicitClone,
        "shared ownership is cloned explicitly and never copied invisibly");

    std::string error;
    tests.expect(validateSmartPointerOperation(
            {SmartPointerFamily::Unique, SmartPointerOperation::Move}, error) &&
            !validateSmartPointerOperation(
                {SmartPointerFamily::Unique, SmartPointerOperation::ImplicitCopy}, error),
        "Unique is nullable move-only ownership rather than implicit sharing");
    tests.expect(validateSmartPointerOperation(
            {SmartPointerFamily::Rc, SmartPointerOperation::Downgrade, 1, 0}, error) &&
            validateSmartPointerOperation(
                {SmartPointerFamily::WeakRc, SmartPointerOperation::Upgrade, 1, 1}, error),
        "Rc downgrade and live WeakRc upgrade preserve explicit local counts");
    tests.expect(!validateSmartPointerOperation(
            {SmartPointerFamily::WeakArc, SmartPointerOperation::Upgrade, 0, 1}, error) &&
            error.find("cannot resurrect") != std::string::npos,
        "weak upgrade cannot resurrect a payload after the last strong owner");
    tests.expect(!validateSmartPointerOperation(
            {SmartPointerFamily::Arc, SmartPointerOperation::Clone, UINT64_MAX, 1}, error) &&
            error.find("overflow") != std::string::npos,
        "reference counts reject overflow before wraparound");
    tests.expect(!validateSmartPointerOperation(
            {SmartPointerFamily::Rc, SmartPointerOperation::CrossThread, 1, 1, true, true}, error),
        "Rc and WeakRc remain non-atomic and local-only");
    tests.expect(validateSmartPointerOperation(
            {SmartPointerFamily::Arc, SmartPointerOperation::CrossThread, 1, 1, true, true}, error) &&
            !validateSmartPointerOperation(
                {SmartPointerFamily::Arc, SmartPointerOperation::CrossThread, 1, 1, false, true}, error),
        "Arc crosses threads only for a Send/Sync payload");
    tests.expect(!validateSmartPointerOperation(
            {SmartPointerFamily::WeakArc, SmartPointerOperation::CrossThread, 1, 1, true, false}, error) &&
            error.find("producer module") != std::string::npos,
        "cross-module ownership keeps producer destroy/deallocate thunks alive");

    const std::string manifest = smartPointerContractManifest();
    tests.expect(manifest.find("cycle_policy=explicit-weak-no-implicit-gc") != std::string::npos &&
            manifest.find("control_block=strong+weak+allocator+destroy+deallocate+module-provenance") !=
                std::string::npos,
        "manifest freezes cycle, allocator, destructor, deallocator, and module provenance policy");
    tests.expect(smartPointerContractFingerprintHex().size() == 16,
        "smart pointer architecture has a stable policy fingerprint");

    const std::filesystem::path executable = argc > 0 ? std::filesystem::path(argv[0]) : std::filesystem::path{};
    const std::filesystem::path contract = executable.parent_path() / "e046-smart-pointer.contract";
    tests.expect(writeSmartPointerContract(contract, error) &&
            verifySmartPointerContractFile(contract, error),
        "versioned smart pointer contract round-trips through the compiler-owned format");
    std::error_code cleanupError;
    std::filesystem::remove(contract, cleanupError);
    return tests.result();
}
