#include "nativesharp/Interop/LibraryCapabilityAudit.h"

#include "../support/TestContext.h"

namespace
{
nativesharp::interop::LibraryMetadataType type(std::string name)
{
    nativesharp::interop::LibraryMetadataType result;
    result.fullyQualifiedName = "NativeSharp.Science." + name;
    result.identityKey = result.fullyQualifiedName;
    result.accessibility = 1;
    return result;
}
}

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::interop;

    LibraryMetadata model;
    model.moduleName = "ScientificSurfaceFixture";
    model.targetTriple = "x86_64-pc-windows-msvc";
    model.types = {
        type("CooMatrix"), type("CsrMatrix"), type("CscMatrix"),
        type("ComplexMath"), type("IntervalSet"), type("IntervalMap"),
        type("FourierTransform"), type("FastFourierTransform"), type("Convolution"),
        type("VectorizationBenchmark")};

    const auto surfaces = auditLibraryCapabilitySurfaces(
        std::span<const LibraryMetadata>(&model, 1));
    tests.expect(surfaces.size() == 19,
        "audit has one explicit surface for every library capability in both tranches");
    tests.expect(surfaces[10].id == "E036" && surfaces[10].missing.empty(),
        "sparse storage requires COO, CSR and CSC together");
    tests.expect(surfaces[11].id == "E037" && surfaces[11].missing.size() == 4,
        "storage names alone do not claim sparse algorithms or convergence reporting");
    tests.expect(surfaces[12].id == "E038" &&
            surfaces[12].present == std::vector<std::string>{"ComplexMath"} &&
            surfaces[12].missing == std::vector<std::string>{"Complex"},
        "complex algorithms without a public value type remain partial");
    tests.expect(surfaces[15].id == "E041" && surfaces[15].missing.empty(),
        "interval collections require both set and map surfaces");
    tests.expect(surfaces[17].id == "E043" && surfaces[17].missing.empty(),
        "Fourier surface requires transforms and convolution");
    tests.expect(surfaces[18].id == "E044" &&
            surfaces[18].present == std::vector<std::string>{"VectorizationBenchmark"} &&
            surfaces[18].missing == std::vector<std::string>{"ScalarKernelFallback"},
        "benchmark evidence never substitutes for the required scalar fallback");

    const std::string report = libraryCapabilitySurfaceReport(
        std::span<const LibraryMetadata>(&model, 1));
    tests.expect(report.find("E036 sparse-storage surface=present") != std::string::npos &&
            report.find("E038 complex-numbers surface=missing") != std::string::npos &&
            report.find("E044 measured-vectorization surface=missing") != std::string::npos &&
            report.find("semantics-certified=false") != std::string::npos,
        "scientific report is honest about partial type evidence and semantic certification");
    return tests.result();
}
