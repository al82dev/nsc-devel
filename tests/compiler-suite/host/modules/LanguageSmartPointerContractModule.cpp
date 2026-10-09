#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/SmartPointerSemantics.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

void addCase(
    TestRegistry& registry,
    std::string id,
    TestFunction function,
    std::string profile = "PR")
{
    registry.add(TestCase{std::move(id), "CL-LANG-019", "Host.Language", "component",
        std::move(profile), {"language", "smart-pointer", "contract"}, std::move(function)});
}

std::string expectedManifest()
{
    return
        "NSSMARTPTR 1.0\n"
        "target=x86_64-pc-windows-msvc\n"
        "ownership_basis=owned-T\n"
        "null_access=checked-failure\n"
        "count_overflow=reject-before-wrap\n"
        "weak_upgrade=no-resurrection\n"
        "cycle_policy=explicit-weak-no-implicit-gc\n"
        "control_block=strong+weak+allocator+destroy+deallocate+module-provenance\n"
        "module_unload=forbidden-while-control-block-live\n"
        "allocation_policy=unique-one;shared-single-allocation-factory\n"
        "family Unique nullable=yes implicit-copy=no explicit-clone=no weak=no "
            "thread=unique-transfer-if-payload-send\n"
        "family Rc nullable=yes implicit-copy=no explicit-clone=yes weak=no thread=local-only\n"
        "family WeakRc nullable=yes implicit-copy=no explicit-clone=yes weak=yes thread=local-only\n"
        "family Arc nullable=yes implicit-copy=no explicit-clone=yes weak=no "
            "thread=atomic-shared-if-payload-send-sync\n"
        "family WeakArc nullable=yes implicit-copy=no explicit-clone=yes weak=yes "
            "thread=atomic-shared-if-payload-send-sync\n"
        "api Unique=MakeUnique,Get,Borrow,Release,Reset\n"
        "api Rc=MakeRc,Get,Borrow,Clone,Downgrade,StrongCount,WeakCount\n"
        "api WeakRc=Clone,Upgrade,Expired,WeakCount\n"
        "api Arc=MakeArc,Get,Borrow,Clone,Downgrade,StrongCount,WeakCount\n"
        "api WeakArc=Clone,Upgrade,Expired,WeakCount\n";
}

class SmartPointerFixtureTree final
{
public:
    explicit SmartPointerFixtureTree(std::string_view caseId)
        : root_(std::filesystem::current_path() /
                (".nsc-suite-" + std::string(caseId) + "-" + std::to_string(GetCurrentProcessId())))
    {
        if (!std::filesystem::create_directory(root_))
            throw std::runtime_error("cannot create isolated smart pointer fixture: " + root_.string());
        tracked_.push_back(root_);
    }

    SmartPointerFixtureTree(const SmartPointerFixtureTree&) = delete;
    SmartPointerFixtureTree& operator=(const SmartPointerFixtureTree&) = delete;

    ~SmartPointerFixtureTree()
    {
        std::error_code ignored;
        for (auto path = tracked_.rbegin(); path != tracked_.rend(); ++path)
        {
            std::filesystem::remove(*path, ignored);
            ignored.clear();
        }
    }

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }
    void track(const std::filesystem::path& path) { tracked_.push_back(path); }

private:
    std::filesystem::path root_;
    std::vector<std::filesystem::path> tracked_;
};

void writeFixture(const std::filesystem::path& path, std::string_view bytes)
{
    // NOLINTNEXTLINE(bugprone-signed-bitwise) -- standard openmode flags are a bitmask enum.
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream) throw std::runtime_error("cannot write smart pointer fixture: " + path.string());
}

std::string withCrlf(std::string_view text)
{
    std::string result;
    result.reserve(text.size() * 2);
    for (const char byte : text)
    {
        if (byte == '\n') result.push_back('\r');
        result.push_back(byte);
    }
    return result;
}

void registerManifestCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-257", [](TestContext& context) {
        const std::string first = smartPointerContractManifest();
        const std::string second = smartPointerContractManifest();
        context.expect(first == expectedManifest() && second == first && first.size() == 1157,
            "TL-LANG-257 manifest bytes, order, length and repeated output must be exact");
    });
}

void registerFingerprintCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-258", [](TestContext& context) {
        const std::string first = smartPointerContractFingerprintHex();
        const std::string second = smartPointerContractFingerprintHex();
        context.expect(first == "b4cdf8ae6d2c4a10" && second == first,
            "TL-LANG-258 repeated fingerprints must equal the independently frozen FNV value");
    });
}

void registerWriteCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-259", [](TestContext& context) {
        SmartPointerFixtureTree tree("TL-LANG-259");
        std::string error = "stale";

        const auto existing = tree.root() / "existing";
        context.expect(std::filesystem::create_directory(existing),
            "TL-LANG-259 the existing-parent fixture must be created");
        tree.track(existing);
        const auto existingContract = existing / "smart-pointer.contract";
        tree.track(existingContract);
        context.expect(writeSmartPointerContract(existingContract, error) && error.empty() &&
                std::filesystem::is_regular_file(existingContract),
            "TL-LANG-259 writing below an existing parent must publish the contract");

        const auto missing = tree.root() / "missing";
        const auto nested = missing / "nested";
        const auto nestedContract = nested / "smart-pointer.contract";
        context.expect(writeSmartPointerContract(nestedContract, error) && error.empty() &&
                std::filesystem::is_regular_file(nestedContract),
            "TL-LANG-259 writing must create every absent parent");
        tree.track(missing);
        tree.track(nested);
        tree.track(nestedContract);

        const auto blocked = tree.root() / "blocked";
        context.expect(std::filesystem::create_directory(blocked),
            "TL-LANG-259 the filesystem-failure fixture must be created");
        tree.track(blocked);
        context.expect(!writeSmartPointerContract(blocked, error) &&
                error == "cannot write smart pointer contract '" + blocked.string() + "'",
            "TL-LANG-259 a directory destination must produce the exact write diagnostic");
        context.expect(!writeSmartPointerContract({}, error) &&
                error == "smart pointer contract destination is empty",
            "TL-LANG-259 an empty destination must produce the exact deterministic diagnostic");
    }, "NIGHTLY");
}

void registerVerifyCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-260", [](TestContext& context) {
        SmartPointerFixtureTree tree("TL-LANG-260");
        std::string error;

        const auto exact = tree.root() / "exact.contract";
        tree.track(exact);
        writeFixture(exact, expectedManifest());
        context.expect(verifySmartPointerContractFile(exact, error) && error.empty(),
            "TL-LANG-260 exact LF bytes must verify");

        const auto crlf = tree.root() / "crlf.contract";
        tree.track(crlf);
        writeFixture(crlf, withCrlf(expectedManifest()));
        context.expect(verifySmartPointerContractFile(crlf, error) && error.empty(),
            "TL-LANG-260 CRLF bytes must normalize to the canonical LF manifest");

        const auto altered = tree.root() / "altered.contract";
        tree.track(altered);
        writeFixture(altered, expectedManifest() + "altered=yes\n");
        context.expect(!verifySmartPointerContractFile(altered, error) &&
                error == "incompatible smart pointer contract '" + altered.string() +
                    "': expected fingerprint b4cdf8ae6d2c4a10",
            "TL-LANG-260 any remaining byte difference must produce the exact fingerprint diagnostic");
    }, "NIGHTLY");
}
}

void registerLanguageSmartPointerContractModule(TestRegistry& registry)
{
    registerManifestCase(registry);
    registerFingerprintCase(registry);
    registerWriteCase(registry);
    registerVerifyCase(registry);
}
}
