#include "nativesharp/Interop/LibraryCapabilityAudit.h"

#include <algorithm>
#include <array>

namespace nativesharp::interop
{
namespace
{
struct Requirement final
{
    std::string_view id;
    std::string_view name;
    std::span<const std::string_view> types;
};

bool matchesTypeName(std::string_view qualified, std::string_view expected)
{
    if (qualified == expected) return true;
    if (qualified.size() <= expected.size()) return false;
    const std::size_t offset = qualified.size() - expected.size();
    if (qualified.substr(offset) != expected) return false;
    return qualified[offset - 1] == '.' || qualified[offset - 1] == ':';
}

bool hasType(std::span<const LibraryMetadata> modules, std::string_view expected)
{
    for (const LibraryMetadata& module : modules)
        for (const LibraryMetadataType& type : module.types)
            if (matchesTypeName(type.fullyQualifiedName, expected)) return true;
    return false;
}

constexpr std::array<std::string_view, 6> e012 = {
    "FixedString32", "FixedString64", "FixedString128", "FixedString512",
    "FixedString4096", "NativeText"};
constexpr std::array<std::string_view, 1> e013 = {"FixedList"};
constexpr std::array<std::string_view, 1> e014 = {"StringName"};
constexpr std::array<std::string_view, 3> e015 = {"ObjectPool", "ListPool", "CollectionPool"};
constexpr std::array<std::string_view, 3> e016 = {"BitField32", "BitField64", "XxHash3"};
constexpr std::array<std::string_view, 3> e027 = {
    "FormatWriter", "IntegerFormatter", "FloatFormatter"};
constexpr std::array<std::string_view, 1> e028 = {"TextWriter"};
constexpr std::array<std::string_view, 2> e029 = {"UnicodeData", "UnicodeNormalizer"};
constexpr std::array<std::string_view, 2> e030 = {"GraphemeEnumerator", "UnicodeDisplayWidth"};
constexpr std::array<std::string_view, 2> e031 = {"RegexMatcher", "UnicodeRegexData"};
constexpr std::array<std::string_view, 3> e036 = {"CooMatrix", "CsrMatrix", "CscMatrix"};
constexpr std::array<std::string_view, 4> e037 = {
    "SparseOperations", "IterativeSolver", "Preconditioner", "ConvergenceReport"};
constexpr std::array<std::string_view, 2> e038 = {"Complex", "ComplexMath"};
constexpr std::array<std::string_view, 3> e039 = {"BigInteger", "BigFloat", "PrecisionLimits"};
constexpr std::array<std::string_view, 3> e040 = {"Dimension", "Quantity", "UnitConverter"};
constexpr std::array<std::string_view, 2> e041 = {"IntervalSet", "IntervalMap"};
constexpr std::array<std::string_view, 3> e042 = {"OdeSolver", "ControlledEuler", "RungeKutta"};
constexpr std::array<std::string_view, 3> e043 = {
    "FourierTransform", "FastFourierTransform", "Convolution"};
constexpr std::array<std::string_view, 2> e044 = {
    "VectorizationBenchmark", "ScalarKernelFallback"};

constexpr std::array<Requirement, 19> requirements = {{
    {"E012", "bounded-text", e012},
    {"E013", "bounded-containers", e013},
    {"E014", "string-interning", e014},
    {"E015", "object-pools", e015},
    {"E016", "bitfields-and-xxhash3", e016},
    {"E027", "typed-formatting", e027},
    {"E028", "text-sinks", e028},
    {"E029", "versioned-unicode", e029},
    {"E030", "graphemes-and-display-width", e030},
    {"E031", "unicode-regex", e031},
    {"E036", "sparse-storage", e036},
    {"E037", "sparse-solvers", e037},
    {"E038", "complex-numbers", e038},
    {"E039", "arbitrary-precision", e039},
    {"E040", "units-and-dimensions", e040},
    {"E041", "interval-sets-and-maps", e041},
    {"E042", "ordinary-differential-equations", e042},
    {"E043", "fourier-transforms", e043},
    {"E044", "measured-vectorization", e044},
}};
}

std::vector<LibraryCapabilitySurface> auditLibraryCapabilitySurfaces(
    std::span<const LibraryMetadata> modules)
{
    std::vector<LibraryCapabilitySurface> result;
    result.reserve(requirements.size());
    for (const Requirement& requirement : requirements)
    {
        LibraryCapabilitySurface surface;
        surface.id = requirement.id;
        surface.name = requirement.name;
        for (const std::string_view type : requirement.types)
        {
            (hasType(modules, type) ? surface.present : surface.missing).emplace_back(type);
        }
        result.push_back(std::move(surface));
    }
    return result;
}

std::string libraryCapabilitySurfaceReport(std::span<const LibraryMetadata> modules)
{
    std::string result = "NSLIBSURFACE 1.0\n";
    result += "modules=" + std::to_string(modules.size()) + "\n";
    result += "semantics-certified=false\n";
    for (const LibraryCapabilitySurface& surface : auditLibraryCapabilitySurfaces(modules))
    {
        result += surface.id + " " + surface.name + " surface=" +
            (surface.missing.empty() ? "present" : "missing") + "\n";
        for (const std::string& value : surface.present) result += "  present: " + value + "\n";
        for (const std::string& value : surface.missing) result += "  missing: " + value + "\n";
    }
    return result;
}

bool auditLibraryCapabilityDirectory(
    const std::filesystem::path& root,
    std::string& report,
    std::string& error)
{
    report.clear();
    error.clear();
    std::error_code iterationError;
    if (!std::filesystem::is_directory(root, iterationError) || iterationError)
    {
        error = "library capability root is not a readable directory: '" + root.string() + "'";
        return false;
    }
    std::vector<std::filesystem::path> paths;
    for (std::filesystem::recursive_directory_iterator iterator(root, iterationError), end;
         iterator != end && !iterationError; iterator.increment(iterationError))
    {
        if (iterator->is_regular_file() && iterator->path().extension() == ".nsmeta")
            paths.push_back(iterator->path());
    }
    if (iterationError)
    {
        error = "cannot enumerate library capability root '" + root.string() + "': " +
            iterationError.message();
        return false;
    }
    std::ranges::sort(paths, {}, [](const std::filesystem::path& path) {
        return path.generic_string();
    });
    if (paths.empty())
    {
        error = "library capability root contains no .nsmeta files: '" + root.string() + "'";
        return false;
    }
    LibraryMetadataCodec codec;
    std::vector<LibraryMetadata> modules;
    modules.reserve(paths.size());
    for (const std::filesystem::path& path : paths)
    {
        LibraryMetadata metadata;
        if (!codec.read(path, metadata, error))
        {
            error = "invalid library capability metadata '" + path.string() + "': " + error;
            return false;
        }
        modules.push_back(std::move(metadata));
    }
    report = libraryCapabilitySurfaceReport(modules);
    return true;
}
}
