#include "CompilationTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace compilation_support;

std::optional<SdkManifest> loadSdk(
    TemporaryTree& tree,
    std::string_view document,
    std::string& error,
    std::string_view name = "Windows.nssdk")
{
    return SdkManifestLoader{}.load(tree.write(std::string(name), document), error);
}

void sdkMappingContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("sdk_mapping");
    std::string mapping = variant == 1 ? "<Header Include=\"windows.h\" Namespace=\"Windows\"/>" :
        variant == 2 ? "<Type Native=\"DWORD\" NativeSharp=\"uint\"/>"
            "<Field Type=\"POINT\" Native=\"x\" NativeSharp=\"X\"/>"
            "<Define Name=\"MAX_PATH\" Value=\"260\"/>" :
        variant == 3 ? "<Type Native=\"DWORD\"/>" :
        variant == 4 ? "<Function Native=\"CreateFileW\"/>" :
        variant == 5 ? "<Type Native=\"DWORD\" NativeSharp=\"uint\"/>"
            "<Type Native=\"DWORD\" NativeSharp=\"int\"/>" :
        variant == 6 ? "<Header Include=\"a.h\" Namespace=\"A\"/>"
            "<Header Include=\"b.h\" Namespace=\"B\"/>" :
        variant == 7 ? "<Define Name=\"A\" Value=\"1\"/><Define Name=\"B\" Value=\"2\"/>" :
            "<Type Native=\"T0\" NativeSharp=\"N0\"/>";
    const std::string document = sdkDocument("<Mappings>" + mapping + "</Mappings>");
    std::string error;
    const auto manifest = loadSdk(tree, document, error);
    const bool expected = variant == 1 || variant == 2 || variant == 6 || variant == 7 || variant == 8;
    bool exact = manifest.has_value() == expected;
    if (manifest.has_value() && variant == 2)
    {
        exact = manifest->types.size() == 1U && manifest->fields.size() == 1U &&
            manifest->defines.size() == 1U;
    }
    context.expect(exact, "CF-COMPILATION-043 SDK mappings enforce closed attributes, unique category keys, and order");
}

void sdkVersionContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("sdk_version");
    const std::string minimum = variant == 3 ? "" : variant == 4 ? "10.x" :
        variant == 5 ? "1.2.3.4.5.6.7.8.9" : variant == 6 ? "10.0.0" : "10.0";
    const std::string maximum = variant == 4 ? "11.x" : variant == 6 ? "10.0.0.0" : "11.0";
    const std::string document = sdkDocument(
        "<Targets><Target Architecture=\"x64\" MinVersion=\"" + minimum +
        "\" MaxVersion=\"" + maximum + "\"/></Targets>");
    std::string error;
    const auto manifest = loadSdk(tree, document, error);
    const bool expected = variant != 3 && variant != 4 && variant != 5;
    bool exact = manifest.has_value() == expected;
    if (manifest.has_value() && variant == 6)
    {
        SdkTargetSelector selector;
        exact = selector.select(*manifest, "x64", "10.0", error) != nullptr &&
            selector.select(*manifest, "x64", "10.0.0.0", error) != nullptr;
    }
    context.expect(exact, "CF-COMPILATION-044 numeric SDK versions parse at eight parts and compare after zero normalization");
}

void sdkTargetRangeContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("sdk_ranges");
    std::string targets = variant == 1 ?
        "<Target Architecture=\"x64\" MinVersion=\"10.0\" MaxVersion=\"11.0\"/>" :
        "<Target Architecture=\"arm64\" MinVersion=\"10.0\" MaxVersion=\"11.0\"/>"
        "<Target Architecture=\"x64\" MinVersion=\"10.0\" MaxVersion=\"10.4\"/>"
        "<Target Architecture=\"x64\" MinVersion=\"10.5\" MaxVersion=\"11.0\"/>";
    if (variant == 3)
    {
        targets = "<Target Architecture=\"x64\" MinVersion=\"11\" MaxVersion=\"10\"/>";
    }
    if (variant == 4 || variant == 5)
    {
        targets = "<Target Architecture=\"x64\" MinVersion=\"10\" MaxVersion=\"10.5\"/>"
            "<Target Architecture=\"x64\" MinVersion=\"10.5\" MaxVersion=\"11\"/>";
    }
    std::string error;
    const auto manifest = loadSdk(tree, sdkDocument("<Targets>" + targets + "</Targets>"), error);
    const bool expected = variant != 3 && variant != 4 && variant != 5;
    bool exact = manifest.has_value() == expected;
    if (manifest.has_value() && variant == 2)
    {
        exact = manifest->targets.size() == 3U && manifest->targets.front().architecture == "arm64";
    }
    context.expect(exact, "CF-COMPILATION-045 target ranges require min <= max, no overlap, and canonical order");
}

void sdkLibraryContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("sdk_libraries");
    std::string libraries = variant == 1 ? "<Library Name=\"user32\" Path=\"lib/user32.lib\"/>" :
        "<Library Name=\"zlib\" Path=\"lib/z.lib\"/>"
        "<Library Name=\"advapi32\" Path=\"lib/a.lib\"/>";
    if (variant == 3) libraries = "<Library Name=\"\" Path=\"lib/a.lib\"/>";
    if (variant == 4) libraries = "<Library Name=\"bad-name\" Path=\"../bad.lib\"/>";
    if (variant == 5) libraries = "<Library Name=\"a\" Path=\"a.lib\"/>"
        "<Library Name=\"a\" Path=\"b.lib\"/>";
    if (variant == 6) libraries = "<Library Name=\"a\" Path=\"absolute.dll\"/>";
    std::string error;
    const auto manifest = loadSdk(
        tree, sdkDocument("<Libraries>" + libraries + "</Libraries>"), error);
    const bool expected = variant == 1 || variant == 2 || variant == 7 || variant == 8;
    bool exact = manifest.has_value() == expected;
    if (manifest.has_value() && variant == 2)
    {
        exact = manifest->libraries.size() == 2U && manifest->libraries.front().name == "advapi32";
    }
    context.expect(exact, "CF-COMPILATION-046 SDK libraries require identifier, confined relative .lib path, uniqueness, and order");
}

void sdkManifestContract(TestContext& context, std::size_t variant)
{
    TemporaryTree tree("sdk_manifest");
    const std::string source = variant == 1 ? sdkDocument() :
        variant == 2 ? sdkDocument("<Mappings/><Targets/><Libraries/>") :
        variant == 3 ? "<NativeSharpSdk FormatVersion=\"1\"/>" :
        variant == 4 ? "<Other Name=\"Windows\" FormatVersion=\"1\"/>" :
        variant == 5 ? "<NativeSharpSdk Name=\"Windows\" FormatVersion=\"2\"/>" :
        variant == 6 ? sdkDocument("<Mappings/><Mappings/>") :
        variant == 7 ? "<?xml version=\"1.0\"?>" + sdkDocument() :
            sdkDocument("<Mappings><Define Name=\"A\" Value=\"1\"/></Mappings>");
    std::string error;
    const auto manifest = loadSdk(tree, source, error);
    const bool expected = variant == 1 || variant == 2 || variant == 7 || variant == 8;
    context.expect(manifest.has_value() == expected &&
            (manifest.has_value() ? manifest->formatVersion == 1U : !error.empty()),
        "CF-COMPILATION-047 SDK file enforces root, format version, and unique sections");
}

void sdkTargetSelectionContract(TestContext& context, std::size_t variant)
{
    SdkManifest manifest;
    manifest.name = "Windows";
    manifest.targets = {
        {"arm64", "10.0", "11.0"},
        {"x64", "10.0", "10.4"},
        {"x64", "10.5", "11.0"}};
    const std::string architecture = variant == 3 ? "x86" : "x64";
    const std::string version = variant == 4 ? "12.0" : variant == 5 ? "bad" :
        variant == 6 ? "10.5.0" : "10.2";
    std::string error;
    const SdkTargetVariant* selected = SdkTargetSelector{}.select(manifest, architecture, version, error);
    const bool expected = variant != 3 && variant != 4 && variant != 5;
    bool exact = (selected != nullptr) == expected;
    if (selected != nullptr && variant == 6)
    {
        exact = selected->minimumVersion == "10.5";
    }
    context.expect(exact && (selected != nullptr || contains(error, "NSC-SDK-003")),
        "CF-COMPILATION-048 selector returns first compatible range or exact stable diagnostic");
}
}

void registerCompilationSdk337To384Module(TestRegistry& registry)
{
    addCompilationContract(registry, 43U, "unit", sdkMappingContract);
    addCompilationContract(registry, 44U, "unit", sdkVersionContract);
    addCompilationContract(registry, 45U, "unit", sdkTargetRangeContract);
    addCompilationContract(registry, 46U, "unit", sdkLibraryContract);
    addCompilationContract(registry, 47U, "integration", sdkManifestContract);
    addCompilationContract(registry, 48U, "unit", sdkTargetSelectionContract);
}
}
