#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::frontend::ProjectCompilation;
using nativesharp::frontend::ProjectOopMirDispatchTable;
using nativesharp::frontend::ProjectOopMirModel;
using nativesharp::frontend::ProjectOopMirNode;
using nativesharp::frontend::ProjectOopMirNodeKind;
using nativesharp::llvm_backend::CoffObjectVerifier;
using nativesharp::llvm_backend::LlvmCodeGenFileType;
using nativesharp::llvm_backend::LlvmModule;
using nativesharp::llvm_backend::LlvmTargetMachine;
using nativesharp::llvm_backend::ProjectOopLlvmLowerer;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(const std::filesystem::path& directory, std::string_view name, std::string_view source)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
}

void collectNodes(
    const ProjectOopMirNode* node,
    ProjectOopMirNodeKind kind,
    std::vector<const ProjectOopMirNode*>& output)
{
    if (node == nullptr) return;
    if (node->kind == kind) output.push_back(node);
    for (const auto& child : node->children) collectNodes(child.get(), kind, output);
}

std::uint64_t stableTypeId(const ProjectOopMirModel& mir, std::size_t typeOrdinal)
{
    for (const ProjectOopMirDispatchTable& table : mir.dispatchTables())
    {
        if (table.typeOrdinal == typeOrdinal) return table.stableTypeId;
    }
    return 0;
}

std::string llvmI64Literal(std::uint64_t value)
{
    constexpr std::uint64_t signedMax = 0x7fffffffffffffffULL;
    if (value <= signedMax) return std::to_string(value);
    const std::uint64_t magnitude = (~value) + 1ULL;
    return "-" + std::to_string(magnitude);
}

bool hasDiagnosticText(const ProjectCompilation& compilation, std::string_view text)
{
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (diagnostic.message().find(text) != std::string::npos) return true;
    }
    return false;
}

std::vector<std::string> catchPadLines(const std::string& text)
{
    std::vector<std::string> lines;
    std::size_t offset = 0;
    while (offset < text.size())
    {
        const std::size_t end = text.find('\n', offset);
        const std::size_t length = end == std::string::npos ? text.size() - offset : end - offset;
        std::string line = text.substr(offset, length);
        if (line.find("catchpad within") != std::string::npos) lines.push_back(std::move(line));
        if (end == std::string::npos) break;
        offset = end + 1;
    }
    return lines;
}

std::uint16_t readU16(const std::array<unsigned char, 20>& bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

std::vector<std::string> coffSectionNames(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    std::array<unsigned char, 20> header{};
    stream.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    if (!stream) return {};
    const std::uint16_t sectionCount = readU16(header, 2);
    const std::uint16_t optionalHeaderSize = readU16(header, 16);
    stream.seekg(static_cast<std::streamoff>(20U + optionalHeaderSize), std::ios::beg);
    std::vector<std::string> names;
    for (std::uint16_t index = 0; index < sectionCount; ++index)
    {
        std::array<char, 40> section{};
        stream.read(section.data(), static_cast<std::streamsize>(section.size()));
        if (!stream) return {};
        std::size_t length = 0;
        while (length < 8 && section[length] != '\0') ++length;
        names.emplace_back(section.data(), length);
    }
    return names;
}

bool containsName(const std::vector<std::string>& names, std::string_view expected)
{
    for (const std::string& name : names)
    {
        if (name == expected) return true;
    }
    return false;
}

std::string readBinary(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

void testTypedCatchRttiLowering(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "TypedHierarchy";
    writeProject(directory, "TypedHierarchy", R"NS(
namespace NativeSharp.Core
{
    public class Exception { }
}
using NativeSharp.Core;

class BaseProblem : Exception { }
class DerivedProblem : BaseProblem { }
class OtherProblem : Exception { }

class Runner
{
    public static int UseDerived(DerivedProblem value) { return 11; }
    public static int UseBase(BaseProblem value) { return 22; }

    public static int Run(Exception problem)
    {
        try
        {
            throw new Exception();
        }
        catch (DerivedProblem derived)
        {
            return UseDerived(derived);
        }
        catch (BaseProblem baseProblem)
        {
            return UseBase(baseProblem);
        }
        catch
        {
            return 33;
        }
        finally
        {
            int finalized = 44;
        }
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok = compilation.compileProjectOopMir(directory / "TypedHierarchy.nsproj", error);
    tests.expect(ok, "Stage 45.11 typed hierarchy sample reaches OOP MIR");
    tests.expect(error.empty(), "Stage 45.11 typed hierarchy MIR construction has no diagnostic");
    tests.expect(compilation.oopMirModel() != nullptr && compilation.nameLookup() != nullptr,
        "Stage 45.11 retains MIR and name lookup for typed EH lowering");
    if (!ok || compilation.oopMirModel() == nullptr || compilation.nameLookup() == nullptr) return;

    const ProjectOopMirModel& mir = *compilation.oopMirModel();
    std::vector<const ProjectOopMirNode*> catches;
    for (const auto& function : mir.functions()) collectNodes(function.body.get(), ProjectOopMirNodeKind::CatchRegion, catches);
    tests.expect(catches.size() == 3, "MIR preserves the three source-order catch handlers");
    if (catches.size() != 3) return;
    tests.expect(!catches[0]->catchAll && catches[0]->exceptionTypeOrdinal.has_value(),
        "first MIR handler is the DerivedProblem typed catch");
    tests.expect(!catches[1]->catchAll && catches[1]->exceptionTypeOrdinal.has_value(),
        "second MIR handler is the BaseProblem typed catch");
    tests.expect(catches[2]->catchAll && !catches[2]->exceptionTypeOrdinal.has_value(),
        "third MIR handler remains the terminal catch-all");
    tests.expect(catches[0]->localOrdinal.has_value() && catches[1]->localOrdinal.has_value(),
        "typed catch variables retain dedicated MIR local ordinals");
    if (!catches[0]->exceptionTypeOrdinal.has_value() || !catches[1]->exceptionTypeOrdinal.has_value()) return;

    const std::uint64_t derivedId = stableTypeId(mir, *catches[0]->exceptionTypeOrdinal);
    const std::uint64_t baseId = stableTypeId(mir, *catches[1]->exceptionTypeOrdinal);
    tests.expect(derivedId != 0 && baseId != 0 && derivedId != baseId,
        "typed catches resolve distinct stable RTTI ids from existing class descriptors");

    ProjectOopLlvmLowerer lowerer;
    std::unique_ptr<LlvmModule> module = lowerer.lower(*compilation.nameLookup(), mir);
    tests.expect(module != nullptr, "Stage 45.11 lowers typed/multi-catch MIR to LLVM Win64 EH");
    tests.expect(lowerer.error().empty(), "Stage 45.11 LLVM typed catch lowering has no internal diagnostic");
    if (module == nullptr) return;

    std::string verificationError;
    tests.expect(module->verify(verificationError), "Stage 45.11 typed catch LLVM passes LLVMVerifyModule");
    tests.expect(verificationError.empty(), "verified Stage 45.11 module has no LLVM diagnostic");

    const std::string text = module->text();
    const std::string derivedFilter = "__nativesharp_eh_filter_type_" + std::to_string(derivedId);
    const std::string baseFilter = "__nativesharp_eh_filter_type_" + std::to_string(baseId);
    tests.expect(text.find("define internal i32 @" + derivedFilter) != std::string::npos,
        "LLVM defines a module-local constant SEH filter function for DerivedProblem");
    tests.expect(text.find("define internal i32 @" + baseFilter) != std::string::npos,
        "LLVM defines a module-local constant SEH filter function for BaseProblem");
    std::size_t filterDefinitionCount = 0;
    std::size_t filterOffset = 0;
    while ((filterOffset = text.find(
                "define internal i32 @__nativesharp_eh_filter_type_", filterOffset)) != std::string::npos)
    {
        ++filterDefinitionCount;
        ++filterOffset;
    }
    tests.expect(filterDefinitionCount == 2,
        "LLVM emits typed SEH filter wrappers only for exception types actually used by catch clauses");
    tests.expect(text.find("call i32 @__nativesharp_eh_filter") != std::string::npos,
        "typed filters first reject non-NativeSharp SEH records through the domain filter");
    tests.expect(text.find("call i1 @__nativesharp_rtti_is_class") != std::string::npos,
        "typed filters reuse the Stage 39 class-ancestry RTTI helper");
    tests.expect(text.find("i64 " + llvmI64Literal(derivedId)) != std::string::npos &&
            text.find("i64 " + llvmI64Literal(baseId)) != std::string::npos,
        "typed filters embed the resolved stable type ids rather than names/reflection");

    // UPDATED (multi-catch fix, see ProjectOopLlvmLowerer.cpp's catchswitch comment and
    // tests/unit/Stage45_10EhRaiiReuseAfterCatchTests.cpp scenario 11): this used to assert one
    // catchpad PER catch clause, and that assertion was pinning a shape that could not work.
    // `__C_specific_handler` honours exactly ONE handler per catchswitch -- LLVM's SEH state
    // numbering emits a scope-table entry for the first handler only -- so the second and third
    // catchpads this test was requiring were dead code, and a program whose matching clause was not
    // the first let the exception escape to the OS. Confirmed by real execution, not by reading IR.
    //
    // The shape is now one catchpad using the generic domain filter, which catchrets to ordinary
    // parent code that re-tests the caught object against each clause's RTTI type in SOURCE ORDER.
    // So what this test must pin is no longer "how many pads" but "clause priority is preserved and
    // decided by real type tests".
    const std::vector<std::string> padLines = catchPadLines(text);
    tests.expect(padLines.size() == 1, "LLVM catchswitch owns exactly one catchpad handler");
    if (padLines.size() == 1)
    {
        tests.expect(padLines[0].find("@__nativesharp_eh_filter") != std::string::npos &&
                padLines[0].find("_type_") == std::string::npos,
            "the single catchpad screens on the NativeSharp domain filter, not on one clause's type");
    }
    const std::size_t derivedTest = text.find("i64 " + llvmI64Literal(derivedId));
    const std::size_t baseTest = text.find("i64 " + llvmI64Literal(baseId));
    tests.expect(derivedTest != std::string::npos && baseTest != std::string::npos,
        "clause dispatch tests both DerivedProblem and BaseProblem by stable type id");
    tests.expect(text.find("eh.catch.select") != std::string::npos,
        "clause dispatch happens in ordinary parent code reached by catchret, not inside a funclet");
    tests.expect(text.find("eh.catch.unmatched") != std::string::npos,
        "clause dispatch keeps an explicit no-clause-matched path for enclosing cleanups + rethrow");
    tests.expect(text.find("store ptr %eh.caught.exception") != std::string::npos,
        "entered typed catch stores the current exception object into its catch-local storage");
    tests.expect(text.find("cleanuppad within") != std::string::npos &&
            text.find("cleanupret from") != std::string::npos,
        "typed catch dispatch composes with the Stage 45.10 finally cleanup path");
    tests.expect(text.find("catchret from") != std::string::npos,
        "typed handlers still exit Windows funclets through catchret");

    std::string targetError;
    std::unique_ptr<LlvmTargetMachine> target = LlvmTargetMachine::createWindowsX64(targetError);
    tests.expect(target != nullptr && targetError.empty(), "Stage 45.11 obtains a Windows x64 target machine");
    if (target == nullptr) return;
    const std::filesystem::path objectPath = root / "stage45_11_typed_catch.obj";
    std::string emissionError;
    tests.expect(target->emit(*module, objectPath, LlvmCodeGenFileType::Object, emissionError),
        "Stage 45.11 emits a real AMD64 COFF object for typed EH");
    tests.expect(emissionError.empty(), "Stage 45.11 typed EH COFF emission has no diagnostic");
    CoffObjectVerifier verifier;
    tests.expect(verifier.verify(objectPath), "COFF verifier accepts the Stage 45.11 typed EH object");
    const std::vector<std::string> sections = coffSectionNames(objectPath);
    tests.expect(containsName(sections, ".pdata") && containsName(sections, ".xdata"),
        "typed EH COFF retains Win64 .pdata/.xdata metadata");
    const std::string objectBytes = readBinary(objectPath);
    tests.expect(objectBytes.find("__C_specific_handler") != std::string::npos,
        "typed EH COFF retains the Windows language-specific handler dependency");
    tests.expect(objectBytes.find("__nativesharp_rtti_is_class") != std::string::npos,
        "typed EH COFF retains the native RTTI ancestry helper");
}

void testTypedOnlyCatchPropagatesOnMismatch(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "TypedOnly";
    writeProject(directory, "TypedOnly", R"NS(
namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;
class BaseProblem : Exception { }
class OtherProblem : Exception { }
class Runner
{
    public static void Run(Exception problem)
    {
        try { throw new Exception(); }
        catch (BaseProblem caught) { int handled = 1; }
    }
}
)NS");
    ProjectCompilation compilation;
    std::string error;
    tests.expect(compilation.compileProjectOopMir(directory / "TypedOnly.nsproj", error),
        "typed-only catch sample remains semantically valid");
    if (compilation.oopMirModel() == nullptr || compilation.nameLookup() == nullptr) return;
    ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        "typed-only catch lowers without inventing a catch-all");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "typed-only catch LLVM verifies");
    const std::string text = module->text();
    tests.expect(text.find("catchswitch within none") != std::string::npos &&
            text.find("unwind to caller") != std::string::npos,
        "a typed-only catch continues unwinding to the caller when its RTTI filter rejects the exception");
}

void testNestedFinallyKeepsUnmatchedCleanupTraversalStable(
    TestContext& tests,
    const std::filesystem::path& root)
{
    const auto directory = root / "NestedFinallyUnmatched";
    writeProject(directory, "NestedFinallyUnmatched", R"NS(
namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;
class RaisedProblem : Exception { }
class OtherProblem : Exception { }
class Runner
{
    public static int Run()
    {
        try
        {
            try
            {
                raii RaisedProblem problem = new RaisedProblem();
                throw move(problem);
            }
            catch (OtherProblem other)
            {
                return 1;
            }
        }
        finally
        {
            try { int first = 2; }
            finally { int second = 3; }
        }
        return 0;
    }
}
)NS");

    ProjectCompilation compilation;
    std::string error;
    const bool ok =
        compilation.compileProjectOopMir(directory / "NestedFinallyUnmatched.nsproj", error);
    tests.expect(ok, "nested finally/unmatched typed catch sample reaches OOP MIR");
    if (!ok || compilation.oopMirModel() == nullptr || compilation.nameLookup() == nullptr) return;
    ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        "nested finally lowering does not invalidate the unmatched cleanup traversal");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "nested finally/unmatched typed catch LLVM verifies");
}

void testSemanticOrderingGuardsRemain(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "BadOrdering";
    writeProject(directory, "BadOrdering", R"NS(
namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;
class BaseProblem : Exception { }
class DerivedProblem : BaseProblem { }
class Runner
{
    public static void Run(Exception problem)
    {
        try { throw new Exception(); }
        catch (BaseProblem first) { }
        catch (DerivedProblem unreachable) { }
    }
}
)NS");
    ProjectCompilation compilation;
    std::string error;
    tests.expect(!compilation.compileProjectOopMir(directory / "BadOrdering.nsproj", error),
        "semantic analysis still rejects base-before-derived catch ordering before LLVM");
    tests.expect(hasDiagnosticText(
            compilation,
            "unreachable because an earlier catch handles the same or a base exception type"),
        "invalid catch hierarchy reports the precise Stage 45 semantic ordering diagnostic");
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage45_11_typed_catch_rtti";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    testTypedCatchRttiLowering(tests, root);
    testTypedOnlyCatchPropagatesOnMismatch(tests, root);
    testNestedFinallyKeepsUnmatchedCleanupTraversalStable(tests, root);
    testSemanticOrderingGuardsRemain(tests, root);

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
