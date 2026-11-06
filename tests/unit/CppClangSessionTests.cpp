#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "nativesharp/Frontend/CppDiagnosticAdapter.h"
#include "nativesharp/Frontend/CppAliasRecordType.h"
#include "nativesharp/Frontend/CppAliasArrayType.h"
#include "nativesharp/Frontend/CppAliasEnumType.h"
#include "nativesharp/Frontend/CppAliasFunctionType.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticRenderer.h"
#include "nativesharp/Source/SourceManager.h"
#include <sstream>
#include "../../src/Interop/Cpp/ClangSessionSupport.h"
#include "../../src/LLVM/Lowering/PackedArrayAlignment.h"
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/FrontendActions.h>
#include <atomic>
#include <array>
#include <thread>
#include <llvm/Support/MemoryBuffer.h>
#include "nativesharp/Basic/ArtifactTransaction.h"
#include "nativesharp/Parser/ScopedKnownTypeNames.h"
#include "nativesharp/Parser/LexicalNamespaceScopes.h"
#include "nativesharp/Frontend/QualifiedNamespaceTarget.h"
#include "nativesharp/Frontend/LexicalNamespaceEnvironment.h"
#include "nativesharp/Frontend/CppScopedTypeNames.h"
#include "nativesharp/Frontend/CppParserProjectInventory.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/Source/SourceFile.h"
#include "nativesharp/Interop/CppProjectSessions.h"
#include "nativesharp/Interop/CppBinaryDependencies.h"
#include "nativesharp/Interop/LibraryMetadataCodec.h"
#include "nativesharp/Interop/LibraryMetadataMerger.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Interop/CppScalarTypeMapping.h"
#include "nativesharp/Interop/CppSemanticFunction.h"
#include "nativesharp/Interop/CppSemanticRecord.h"
#include "../../src/Interop/Internal/CppCRecordMetadataCodec.h"
#include "../../src/Interop/Internal/LibraryMetadataCodecSupport.h"
#include "nativesharp/Interop/CppSemanticInventory.h"
#include "nativesharp/Interop/CppSemanticFunctionGroups.h"
#include "nativesharp/Interop/CppNamespaceInventory.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectOopHirNode.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Frontend/ProjectOopMirNode.h"
#include "nativesharp/Frontend/ObjectConstructionBinding.h"
#include "nativesharp/Frontend/ClassDestructionPlan.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/CppLifetimeObjectEmitter.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/LLVM/ProjectOopObjectGenerator.h"
#include "nativesharp/LLVM/ProjectOopObjectBundle.h"
#include "nativesharp/Linker/WindowsExecutableBuilder.h"
#include "nativesharp/Linker/WindowsLibraryBuilder.h"
#include "nativesharp/Compilation/ProjectLoader.h"
#include "nativesharp/Compilation/ProjectGraphLoader.h"
#include "nativesharp/Compilation/PrecompiledProjectPreparation.h"
#include "nativesharp/Compilation/PrecompiledProductSidecarCodec.h"
#include "nativesharp/Compilation/CompilationInputManifest.h"
#include "../../src/Driver/Internal/DriverExecutionScope.h"
#include "../../src/Driver/Internal/DriverInternal.h"
#include "../../src/Compilation/PrecompiledSurfaceReachability.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <variant>

using namespace nativesharp::interop;

namespace
{
class RejectSourceBegin final : public clang::SyntaxOnlyAction
{
public:
    unsigned beginCalls = 0;
    unsigned endCalls = 0;

protected:
    bool BeginSourceFileAction(clang::CompilerInstance&) override
    {
        ++beginCalls;
        return false;
    }
    void EndSourceFileAction() override { ++endCalls; }
};

class RejectUnexpectedOpen final : public CppInteropSessionFactory
{
public:
    CppSessionOpenResult open(CppSessionOptions, CppCancellationToken) override
    {
        ++calls;
        return CppSessionOpenFailure{"test", "unexpected C/C++ session", {}};
    }
    unsigned calls = 0;
};

class FailWithDiagnostic final : public CppInteropSessionFactory
{
public:
    CppSessionOpenResult open(CppSessionOptions, CppCancellationToken) override
    {
        CppSessionDiagnostic diagnostic;
        diagnostic.message = "header failure";
        diagnostic.location = CppSourcePoint{"A:/headers/broken.hpp", 4, 9};
        CppSessionDiagnostic note;
        note.message = "related declaration";
        diagnostic.notes.push_back(std::move(note));
        return CppSessionOpenFailure{"parse", "failed to parse header", {std::move(diagnostic)}};
    }
};

class FailThirdOpen final : public CppInteropSessionFactory
{
public:
    explicit FailThirdOpen(CppInteropSessionFactory& delegate) : delegate_(delegate) {}
    CppSessionOpenResult open(CppSessionOptions options, CppCancellationToken cancellation) override
    {
        if (++calls_ == 3) return CppSessionOpenFailure{"test", "controlled failure", {}};
        return delegate_.open(std::move(options), cancellation);
    }
private:
    CppInteropSessionFactory& delegate_;
    unsigned calls_ = 0;
};

class NoImplicitSession final : public CppInteropSession
{
public:
    explicit NoImplicitSession(std::unique_ptr<CppInteropSession> inner) : inner_(std::move(inner)) {}
    CppSessionState state() const noexcept override { return inner_->state(); }
    CppSessionOptions optionsSnapshot() const override { return inner_->optionsSnapshot(); }
    std::vector<CppSessionDiagnostic> diagnosticsSnapshot() const override { return inner_->diagnosticsSnapshot(); }
    std::optional<std::size_t> topLevelDeclarationCount() const override { return inner_->topLevelDeclarationCount(); }
    std::optional<std::vector<CppFunctionDeclaration>> freeFunctionsSnapshot() const override { return inner_->freeFunctionsSnapshot(); }
    std::optional<std::vector<CppRecordDeclaration>> recordsSnapshot() const override { return inner_->recordsSnapshot(); }
    std::optional<std::vector<CppTypeAliasDeclaration>> typeAliasesSnapshot() const override { return inner_->typeAliasesSnapshot(); }
    std::optional<std::vector<CppEnumDeclaration>> enumsSnapshot() const override { return inner_->enumsSnapshot(); }
    std::optional<std::vector<CppGlobalDeclaration>> globalsSnapshot() const override { return inner_->globalsSnapshot(); }
    std::optional<std::vector<CppNamespaceDeclaration>> namespacesSnapshot() const override { return inner_->namespacesSnapshot(); }
    std::optional<std::vector<std::filesystem::path>> sourceFilesSnapshot() const override { return inner_->sourceFilesSnapshot(); }
    std::optional<std::vector<CppSourceFileContent>> sourceFileContentsSnapshot() const override { return inner_->sourceFileContentsSnapshot(); }
    CppSessionCloseResult close() override { return inner_->close(); }
private:
    std::unique_ptr<CppInteropSession> inner_;
};

class AlteredEnumeratorUsrSession final : public CppInteropSession
{
public:
    AlteredEnumeratorUsrSession(std::unique_ptr<CppInteropSession> inner, std::optional<std::string> usr,
                                bool alterValueType = false, bool alterTarget = false)
        : inner_(std::move(inner)), usr_(std::move(usr)), alterValueType_(alterValueType), alterTarget_(alterTarget) {}
    CppSessionState state() const noexcept override { return inner_->state(); }
    CppSessionOptions optionsSnapshot() const override
    {
        auto result = inner_->optionsSnapshot();
        if (alterTarget_) result.targetTriple = "different-target";
        return result;
    }
    std::vector<CppSessionDiagnostic> diagnosticsSnapshot() const override { return inner_->diagnosticsSnapshot(); }
    std::optional<std::size_t> topLevelDeclarationCount() const override { return inner_->topLevelDeclarationCount(); }
    std::optional<std::vector<CppFunctionDeclaration>> freeFunctionsSnapshot() const override { return inner_->freeFunctionsSnapshot(); }
    std::optional<std::vector<CppRecordDeclaration>> recordsSnapshot() const override { return inner_->recordsSnapshot(); }
    std::optional<std::vector<CppGlobalDeclaration>> globalsSnapshot() const override { return inner_->globalsSnapshot(); }
    std::optional<std::vector<CppNamespaceDeclaration>> namespacesSnapshot() const override { return inner_->namespacesSnapshot(); }
    std::optional<std::vector<std::filesystem::path>> sourceFilesSnapshot() const override { return inner_->sourceFilesSnapshot(); }
    std::optional<std::vector<CppSourceFileContent>> sourceFileContentsSnapshot() const override { return inner_->sourceFileContentsSnapshot(); }
    std::optional<std::vector<CppEnumDeclaration>> enumsSnapshot() const override
    {
        auto enums = inner_->enumsSnapshot();
        if (enums)
            for (auto& item : *enums)
                for (auto& value : item.enumerators)
                    if (value.name == "Choice")
                    {
                        value.clangUsr = usr_;
                        if (alterValueType_) value.valueType.canonicalSpelling = "different-enumerator-type";
                    }
        return enums;
    }
    CppSessionCloseResult close() override { return inner_->close(); }
private:
    std::unique_ptr<CppInteropSession> inner_;
    std::optional<std::string> usr_;
    bool alterValueType_;
    bool alterTarget_;
};

class MissingFieldUsrSession final : public CppInteropSession
{
public:
    explicit MissingFieldUsrSession(std::unique_ptr<CppInteropSession> inner) : inner_(std::move(inner)) {}
    CppSessionState state() const noexcept override { return inner_->state(); }
    CppSessionOptions optionsSnapshot() const override { return inner_->optionsSnapshot(); }
    std::vector<CppSessionDiagnostic> diagnosticsSnapshot() const override { return inner_->diagnosticsSnapshot(); }
    std::optional<bool> microsoftVolatileSemantics() const override { return inner_->microsoftVolatileSemantics(); }
    std::optional<std::size_t> topLevelDeclarationCount() const override { return inner_->topLevelDeclarationCount(); }
    std::optional<std::vector<CppFunctionDeclaration>> freeFunctionsSnapshot() const override { return inner_->freeFunctionsSnapshot(); }
    std::optional<std::vector<CppRecordDeclaration>> recordsSnapshot() const override
    {
        auto records = inner_->recordsSnapshot();
        if (records)
            for (auto& record : *records)
                if (record.name == "Widget")
                    for (auto& field : record.fields)
                        if (field.name == "value") field.clangUsr.reset();
        return records;
    }
    std::optional<std::vector<CppTypeAliasDeclaration>> typeAliasesSnapshot() const override { return inner_->typeAliasesSnapshot(); }
    std::optional<std::vector<CppEnumDeclaration>> enumsSnapshot() const override { return inner_->enumsSnapshot(); }
    std::optional<std::vector<CppGlobalDeclaration>> globalsSnapshot() const override { return inner_->globalsSnapshot(); }
    std::optional<std::vector<CppNamespaceDeclaration>> namespacesSnapshot() const override { return inner_->namespacesSnapshot(); }
    std::optional<std::vector<std::filesystem::path>> sourceFilesSnapshot() const override { return inner_->sourceFilesSnapshot(); }
    std::optional<std::vector<CppSourceFileContent>> sourceFileContentsSnapshot() const override { return inner_->sourceFileContentsSnapshot(); }
    std::optional<bool> structurallyEquivalentRecord(const CppInteropSession& other, std::string_view name) const override
    { return inner_->structurallyEquivalentRecord(other, name); }
    CppSessionCloseResult close() override { return inner_->close(); }
private:
    std::unique_ptr<CppInteropSession> inner_;
};

class AlteredFieldUsrSession final : public CppInteropSession
{
public:
    explicit AlteredFieldUsrSession(std::unique_ptr<CppInteropSession> inner) : inner_(std::move(inner)) {}
    CppSessionState state() const noexcept override { return inner_->state(); }
    CppSessionOptions optionsSnapshot() const override { return inner_->optionsSnapshot(); }
    std::vector<CppSessionDiagnostic> diagnosticsSnapshot() const override { return inner_->diagnosticsSnapshot(); }
    std::optional<bool> microsoftVolatileSemantics() const override { return inner_->microsoftVolatileSemantics(); }
    std::optional<std::size_t> topLevelDeclarationCount() const override { return inner_->topLevelDeclarationCount(); }
    std::optional<std::vector<CppFunctionDeclaration>> freeFunctionsSnapshot() const override { return inner_->freeFunctionsSnapshot(); }
    std::optional<std::vector<CppRecordDeclaration>> recordsSnapshot() const override
    {
        auto records = inner_->recordsSnapshot();
        if (records)
            for (auto& record : *records)
                if (record.name == "Branch")
                    for (auto& field : record.fields)
                        if (field.name == "items" && field.type.element && field.type.element->recordIdentity)
                        {
                            auto element = std::make_shared<CppType>(*field.type.element);
                            element->recordIdentity->clangUsr = "different-record-usr";
                            field.type.element = std::move(element);
                        }
        return records;
    }
    std::optional<std::vector<CppTypeAliasDeclaration>> typeAliasesSnapshot() const override { return inner_->typeAliasesSnapshot(); }
    std::optional<std::vector<CppEnumDeclaration>> enumsSnapshot() const override { return inner_->enumsSnapshot(); }
    std::optional<std::vector<CppGlobalDeclaration>> globalsSnapshot() const override { return inner_->globalsSnapshot(); }
    std::optional<std::vector<CppNamespaceDeclaration>> namespacesSnapshot() const override { return inner_->namespacesSnapshot(); }
    std::optional<std::vector<std::filesystem::path>> sourceFilesSnapshot() const override { return inner_->sourceFilesSnapshot(); }
    std::optional<std::vector<CppSourceFileContent>> sourceFileContentsSnapshot() const override { return inner_->sourceFileContentsSnapshot(); }
    CppSessionCloseResult close() override { return inner_->close(); }
private:
    std::unique_ptr<CppInteropSession> inner_;
};

CppSessionOptions options(const char* resource, const char* working, const char* driver,
                          CppLanguageMode mode, std::string source)
{
    CppSessionOptions result;
    result.language = mode;
    result.languageStandard = mode == CppLanguageMode::C ? "c11" : "c++20";
    result.targetTriple = "x86_64-pc-windows-msvc";
    result.driverReferencePath = driver;
    result.resourceDirectory = resource;
    result.workingDirectory = working;
    result.input = CppBufferInput{mode == CppLanguageMode::C ? "probe.c" : "probe.cpp", std::move(source)};
    return result;
}

bool expect(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

class CancelDuringHeaderLookup final : public llvm::vfs::ProxyFileSystem
{
public:
    CancelDuringHeaderLookup(llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> underlying,
                             CppCancellationToken token, bool& destroyed)
        : llvm::vfs::ProxyFileSystem(std::move(underlying)), token_(std::move(token)), destroyed_(destroyed) {}
    ~CancelDuringHeaderLookup() override { destroyed_ = true; }

    llvm::ErrorOr<llvm::vfs::Status> status(const llvm::Twine& path) override
    {
        if (cancelAtHeader(path)) return std::make_error_code(std::errc::operation_canceled);
        return ProxyFileSystem::status(path);
    }

    bool exists(const llvm::Twine& path) override
    {
        return !cancelAtHeader(path) && ProxyFileSystem::exists(path);
    }

    llvm::ErrorOr<std::unique_ptr<llvm::vfs::File>> openFileForRead(const llvm::Twine& path) override
    {
        if (cancelAtHeader(path)) return std::make_error_code(std::errc::operation_canceled);
        return ProxyFileSystem::openFileForRead(path);
    }

    bool observed = false;
private:
    bool cancelAtHeader(const llvm::Twine& path)
    {
        if (path.str().find("cancel_during_parse.h") == std::string::npos) return false;
        observed = true;
        token_.cancel();
        return true;
    }
    CppCancellationToken token_;
    bool& destroyed_;
};

class CountingSessionDiagnostics final : public clang::DiagnosticConsumer
{
public:
    void HandleDiagnostic(clang::DiagnosticsEngine::Level level, const clang::Diagnostic& info) override
    {
        if (level == clang::DiagnosticsEngine::Error || level == clang::DiagnosticsEngine::Fatal)
            ++errors;
        clang::DiagnosticConsumer::HandleDiagnostic(level, info);
    }
    void BeginSourceFile(const clang::LangOptions& options, const clang::Preprocessor* preprocessor) override
    {
        ++begins;
        clang::DiagnosticConsumer::BeginSourceFile(options, preprocessor);
    }
    void EndSourceFile() override
    {
        ++ends;
        clang::DiagnosticConsumer::EndSourceFile();
    }
    unsigned begins = 0;
    unsigned ends = 0;
    unsigned errors = 0;
};

const CppFunctionDeclaration* functionByName(const std::vector<CppFunctionDeclaration>& values,
                                              const std::string& name)
{
    const auto it = std::find_if(values.begin(), values.end(), [&](const auto& value) {
        return value.qualifiedName == name;
    });
    return it == values.end() ? nullptr : &*it;
}

const CppRecordDeclaration* recordByName(const std::vector<CppRecordDeclaration>& values,
                                         const std::string& name)
{
    const auto it = std::find_if(values.begin(), values.end(), [&](const auto& value) {
        return value.qualifiedName == name;
    });
    return it == values.end() ? nullptr : &*it;
}

const CppGlobalDeclaration* globalByName(const std::vector<CppGlobalDeclaration>& values,
                                         const std::string& name)
{
    const auto it = std::find_if(values.begin(), values.end(), [&](const auto& value) {
        return value.qualifiedName == name;
    });
    return it == values.end() ? nullptr : &*it;
}

std::unique_ptr<CppInteropSession> open(CppClangSessionFactory& factory, CppSessionOptions opts)
{
    auto result = factory.open(std::move(opts), {});
    if (auto* failure = std::get_if<CppSessionOpenFailure>(&result))
    {
        std::cerr << "Open failed in " << failure->phase << ": " << failure->message << '\n';
        for (const auto& diagnostic : failure->diagnostics)
            std::cerr << diagnostic.message << '\n';
        return {};
    }
    return std::move(std::get<std::unique_ptr<CppInteropSession>>(result));
}
}

int main(int argc, char** argv)
{
    if (argc != 4 && argc != 5) return 2;
    if (!std::filesystem::is_regular_file(std::filesystem::path(argv[2]) /
                                          "cpp-fixtures" / "project" / "CppProbe.nsproj"))
    {
        std::cerr << "C/C++ session fixtures are missing beneath " << argv[2] << '\n';
        return 2;
    }
    CppClangSessionFactory factory;
    bool ok = true;
    {
        nativesharp::frontend::ProjectCompilation compilation;
        LibraryMetadata metadata;
        std::vector<std::uint8_t> output{9};
        std::string error;
        ok &= expect(compilation.collectCppMetadataCatalog(metadata, output, error) && output.empty(),
                     "absent C metadata provider returns an empty catalogue");
        const bool configuredFailure = compilation.setCppMetadataCatalogProvider(
            [](const auto&, const auto&, auto& bytes, auto& why) {
                bytes = {1, 2}; why = "fixture failure"; return false;
            }, error);
        output = {9};
        ok &= expect(configuredFailure && !compilation.collectCppMetadataCatalog(metadata, output, error) &&
                     output == std::vector<std::uint8_t>{9} && error == "fixture failure",
                     "failed C metadata provider does not publish partial output");
        const bool configuredSuccess = compilation.setCppMetadataCatalogProvider(
            [](const auto&, const auto&, auto& bytes, auto&) { bytes = {3, 4}; return true; }, error);
        ok &= expect(configuredSuccess && compilation.collectCppMetadataCatalog(metadata, output, error) &&
                     output == std::vector<std::uint8_t>({3, 4}) && error.empty(),
                     "C metadata provider publishes complete output on success");
    }
    {
        using nativesharp::frontend::ProjectOopMirNode;
        using nativesharp::frontend::ProjectOopMirNodeKind;
        auto field = std::make_unique<ProjectOopMirNode>();
        field->kind = ProjectOopMirNodeKind::FieldAddress;
        field->alignment = 2;
        field->children.push_back(std::make_unique<ProjectOopMirNode>());
        field->children.back()->kind = ProjectOopMirNodeKind::LocalValue;
        auto row = std::make_unique<ProjectOopMirNode>();
        row->kind = ProjectOopMirNodeKind::RawElementAddress;
        row->immediate = 12;
        row->children.push_back(std::move(field));
        row->children.push_back(std::make_unique<ProjectOopMirNode>());
        ProjectOopMirNode element;
        element.kind = ProjectOopMirNodeKind::RawElementAddress;
        element.immediate = 4;
        element.children.push_back(std::move(row));
        element.children.push_back(std::make_unique<ProjectOopMirNode>());
        ok &= expect(nativesharp::llvm_backend::fieldArrayElementAlignment(&element) == 2,
                     "nested array field alignment respects record alignment and strides");
        element.children.front()->children.front()->kind = ProjectOopMirNodeKind::LocalAddress;
        ok &= expect(!nativesharp::llvm_backend::fieldArrayElementAlignment(&element),
                     "raw indexing without a field root does not invent field alignment");
        auto parent = std::make_unique<ProjectOopMirNode>();
        parent->kind = ProjectOopMirNodeKind::FieldAddress;
        parent->alignment = 1;
        parent->immediate = 1;
        parent->children.push_back(std::make_unique<ProjectOopMirNode>());
        parent->children.back()->kind = ProjectOopMirNodeKind::LocalValue;
        ProjectOopMirNode nested;
        nested.kind = ProjectOopMirNodeKind::FieldLoad;
        nested.alignment = 4;
        nested.immediate = 4;
        nested.children.push_back(std::move(parent));
        ok &= expect(nativesharp::llvm_backend::fieldOrArrayAddressAlignment(&nested) == 1,
                     "nested field access respects the subaligned parent address");
        nested.children.front()->kind = ProjectOopMirNodeKind::FieldLoad;
        ok &= expect(nativesharp::llvm_backend::fieldOrArrayAddressAlignment(&nested) == 4,
                     "a loaded pointer does not inherit the alignment of its storage slot");
    }
    {
        using namespace nativesharp;
        interop::CppSessionDiagnostic fatal;
        fatal.severity = interop::CppSessionDiagnosticSeverity::Fatal;
        fatal.message = "invalid declaration";
        fatal.location = interop::CppSourcePoint{"A:/headers/test.hpp", 12, 7};
        interop::CppSessionDiagnostic note;
        note.message = "declared here";
        note.location = interop::CppSourcePoint{"A:/headers/test.hpp", 3, 2};
        fatal.notes.push_back(note);
        interop::CppSessionDiagnostic warning;
        warning.severity = interop::CppSessionDiagnosticSeverity::Warning;
        warning.message = "warning text";
        interop::CppSessionDiagnostic remark;
        remark.severity = interop::CppSessionDiagnosticSeverity::Remark;
        remark.message = "remark text";
        const std::vector<interop::CppSessionDiagnostic> input{fatal, warning, remark};
        diagnostics::DiagnosticEngine converted;
        frontend::appendCppDiagnostics(input, converted);
        const auto& values = converted.diagnostics();
        ok &= expect(values.size() == 3 && values[0].severity() == diagnostics::DiagnosticSeverity::Error &&
                     values[1].severity() == diagnostics::DiagnosticSeverity::Warning &&
                     values[2].severity() == diagnostics::DiagnosticSeverity::Note &&
                     values[0].externalLocation() && values[0].externalLocation()->line == 12 &&
                     values[0].externalLocation()->column == 7 &&
                     values[0].externalLocation()->file == std::filesystem::path("A:/headers/test.hpp") &&
                     values[0].message().find("A:/headers/test.hpp:12:7") != std::string::npos &&
                     values[0].notes().size() == 1 &&
                     values[0].notes()[0].find("A:/headers/test.hpp:3:2") != std::string::npos,
                     "Clang diagnostics retain severity, external locations and notes");
        source::SourceManager sources;
        std::ostringstream rendered;
        diagnostics::DiagnosticRenderer(sources).renderAll(converted, rendered);
        ok &= expect(rendered.str().find("<source>") == std::string::npos &&
                     rendered.str().find("declared here") != std::string::npos,
                     "unresolved C++ locations render without inventing NativeSharp locations");
        const FileId file = sources.addBuffer("A:/headers/test.hpp", "x\n");
        const SourceLocation location(file, 0);
        diagnostics::DiagnosticEngine resolved;
        frontend::appendCppDiagnostics(std::span(input).first(1), resolved,
            [location](const interop::CppSourcePoint&) -> std::optional<SourceRange> {
                return SourceRange(location, location);
            });
        ok &= expect(resolved.diagnostics().size() == 1 &&
                     resolved.diagnostics()[0].location() == location &&
                     resolved.diagnostics()[0].message().find("A:/headers/test.hpp:12:7") == std::string::npos,
                     "explicitly resolved C++ locations use the supplied NativeSharp range");
        diagnostics::DiagnosticEngine copied;
        copied.append(values[0]);
        ok &= expect(copied.diagnostics().size() == 1 && copied.diagnostics()[0].externalLocation() &&
                     copied.diagnostics()[0].externalLocation()->line == 12 &&
                     copied.diagnostics()[0].externalLocation()->column == 7,
                     "appending a C++ diagnostic preserves its structured external location");
        auto later = fatal;
        later.location->file = "A:/headers/z.hpp";
        auto earlier = fatal;
        earlier.location->file = "A:/headers/a.hpp";
        std::array<diagnostics::DiagnosticEngine, 2> shards;
        const auto resolver = [location](const interop::CppSourcePoint&) -> std::optional<SourceRange> {
            return SourceRange(location, location);
        };
        frontend::appendCppDiagnostics(std::span(&later, 1), shards[0], resolver);
        frontend::appendCppDiagnostics(std::span(&earlier, 1), shards[1], resolver);
        diagnostics::DiagnosticEngine merged;
        merged.mergeDeterministically(shards);
        ok &= expect(merged.diagnostics().size() == 2 && merged.diagnostics()[0].externalLocation() &&
                     merged.diagnostics()[0].externalLocation()->file == std::filesystem::path("A:/headers/a.hpp"),
                     "external locations break ties deterministically when merging diagnostics");
    }
    {
        CppSessionOptions config;
        config.language = CppLanguageMode::C;
        config.languageStandard = "c11";
        config.targetTriple = "x86_64-pc-windows-msvc";
        config.resourceDirectory = "A:/resource";
        config.includePaths = {{"A:/user", CppIncludeKind::User}, {"A:/system", CppIncludeKind::System}};
        config.macros = {{CppMacroAction::Define, "ONE=1"}, {CppMacroAction::Undefine, "TWO"}};
        const auto actual = nativesharp::interop::detail::buildClangInvocationArguments(config);
        const auto includePath = (config.resourceDirectory / "include").u8string();
        const std::string resourceInclude(reinterpret_cast<const char*>(includePath.data()), includePath.size());
        const std::vector<std::string> expected{
            "-target", "x86_64-pc-windows-msvc", "-x", "c", "-std=c11",
            "-resource-dir", "A:/resource", "-nostdinc", "-isystem", resourceInclude,
            "-I", "A:/user", "-isystem", "A:/system", "-DONE=1", "-UTWO"};
        ok &= expect(actual == expected, "Clang arguments preserve language, resources and option order");
        config.language = CppLanguageMode::Cpp;
        config.languageStandard = "c++20";
        const auto cppArgs = nativesharp::interop::detail::buildClangInvocationArguments(config);
        ok &= expect(cppArgs.size() == expected.size() && cppArgs[3] == "c++" && cppArgs[4] == "-std=c++20",
                     "Clang arguments select C++ and its requested standard");
    }
    {
        auto memory = llvm::makeIntrusiveRefCnt<llvm::vfs::InMemoryFileSystem>();
        memory->addFile("/probe.h", 0, llvm::MemoryBuffer::getMemBufferCopy("int probe;"));
        CppCancellationToken token;
        auto filesystem = nativesharp::interop::detail::makeCancellationFileSystem(memory, token);
        ok &= expect(static_cast<bool>(filesystem->status("/probe.h")), "file query succeeds before cancellation");
        token.cancel();
        const auto status = filesystem->status("/probe.h");
        const auto file = filesystem->openFileForRead("/probe.h");
        ok &= expect(!status && status.getError() == std::errc::operation_canceled &&
                     !file && file.getError() == std::errc::operation_canceled,
                     "file query and open observe cancellation");
    }
    {
        const auto directory = std::filesystem::absolute(
            std::filesystem::path(argv[2]) / "cpp-fixtures" / "cancel-during-parse");
        std::filesystem::create_directories(directory);
        { std::ofstream header(directory / "cancel_during_parse.h"); header << "int value;\n"; }
        auto config = options(argv[1], argv[2], argv[3], CppLanguageMode::C, "");
        config.includePaths.push_back({directory, CppIncludeKind::User});
        const auto args = nativesharp::interop::detail::buildClangInvocationArguments(config);
        auto physical = llvm::vfs::createPhysicalFileSystem();
        CppCancellationToken token;
        bool filesystemDestroyed = false;
        auto observed = llvm::makeIntrusiveRefCnt<CancelDuringHeaderLookup>(
            llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem>(physical.release()), token, filesystemDestroyed);
        std::vector<CppSessionDiagnostic> diagnostics;
        nativesharp::interop::detail::DiagnosticCollector consumer(diagnostics);
        const auto input = (directory / "main.c").generic_string();
        auto ast = nativesharp::interop::detail::runClangSessionAction(
            "#include \"cancel_during_parse.h\"\nint main(void) { return 0; }\n",
            args, input, argv[3], &consumer, observed, token);
        if (!observed->observed || !token.cancelled() || (ast && !ast->getDiagnostics().hasErrorOccurred()))
            std::cerr << "cancel analysis observed=" << observed->observed << " cancelled=" << token.cancelled()
                      << " ast=" << static_cast<bool>(ast) << " errors="
                      << (ast && ast->getDiagnostics().hasErrorOccurred()) << '\n';
        ok &= expect(observed->observed && token.cancelled() &&
                     (!ast || ast->getDiagnostics().hasErrorOccurred()),
                     "cancellation during real Clang header lookup interrupts the analysis");
        ast.reset();
        observed.reset();
        ok &= expect(filesystemDestroyed, "cancelled analysis releases the observed filesystem");
    }
    {
        auto left = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
                                         "struct Concurrent { int value; }; int probe(int);\n"));
        auto right = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
                                          "struct Concurrent { int value; }; int probe(int);\n"));
        std::atomic_bool correct{left && right};
        if (left && right)
        {
            std::vector<std::thread> workers;
            for (int worker = 0; worker < 4; ++worker)
                workers.emplace_back([&, worker] {
                    for (int iteration = 0; iteration < 50; ++iteration)
                    {
                        const auto count = left->topLevelDeclarationCount();
                        const auto functions = left->freeFunctionsSnapshot();
                        const auto equivalent = worker % 2 == 0
                            ? left->structurallyEquivalentRecord(*right, "Concurrent")
                            : right->structurallyEquivalentRecord(*left, "Concurrent");
                        if (!count || !functions || !equivalent || !*equivalent)
                            correct.store(false, std::memory_order_relaxed);
                    }
                });
            for (auto& worker : workers) worker.join();
            left->close();
            right->close();
        }
        ok &= expect(correct.load(std::memory_order_relaxed),
                     "concurrent queries and opposite-order comparisons coordinate session access");
    }
    {
        auto config = options(argv[1], argv[2], argv[3], CppLanguageMode::C, "");
        auto arguments = nativesharp::interop::detail::buildClangInvocationArguments(config);
        auto filesystem = llvm::vfs::createPhysicalFileSystem();
        CountingSessionDiagnostics diagnostics;
        auto ast = nativesharp::interop::detail::runClangSessionAction(
            "int lifecycle_probe;\n", arguments,
            (std::filesystem::path(argv[2]) / "lifecycle_probe.c").generic_string(), argv[3], &diagnostics,
            llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem>(filesystem.release()), {});
        ok &= expect(ast && ast->hasSema() && diagnostics.begins == 1 && diagnostics.ends == 1,
                     "successful Clang action pairs one source begin and end and retains Sema");
        ast.reset();
        ok &= expect(diagnostics.begins == 1 && diagnostics.ends == 1,
                     "destroying retained AST does not end the source a second time");

        auto failingFilesystem = llvm::vfs::createPhysicalFileSystem();
        CountingSessionDiagnostics rejectedDiagnostics;
        arguments[4] = "-std=not-a-real-c-standard";
        auto rejected = nativesharp::interop::detail::runClangSessionAction(
            "int rejected_probe;\n", arguments,
            (std::filesystem::path(argv[2]) / "rejected_probe.c").generic_string(), argv[3], &rejectedDiagnostics,
            llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem>(failingFilesystem.release()), {});
        (void)rejected;
        ok &= expect(rejectedDiagnostics.errors > 0 && rejectedDiagnostics.begins == rejectedDiagnostics.ends,
                     "driver errors are captured and source begin/end notifications remain balanced");
        config.languageStandard = "not-a-real-c-standard";
        const auto rejectedSession = factory.open(std::move(config), {});
        const auto* failure = std::get_if<CppSessionOpenFailure>(&rejectedSession);
        ok &= expect(failure && failure->phase == "parse" && !failure->diagnostics.empty(),
                     "opening rejects Clang driver diagnostics even if an AST is produced");

        RejectSourceBegin failingAction;
        CountingSessionDiagnostics failedBeginDiagnostics;
        const auto beginConfig = options(argv[1], argv[2], argv[3], CppLanguageMode::C, "");
        const auto validArgs = nativesharp::interop::detail::buildClangInvocationArguments(beginConfig);
        auto beginFailureFilesystem = llvm::vfs::createPhysicalFileSystem();
        auto beginFailure = nativesharp::interop::detail::runClangSessionAction(
            "int begin_failure_probe;\n", validArgs,
            (std::filesystem::path(argv[2]) / "begin_failure.c").generic_string(), argv[3],
            &failedBeginDiagnostics,
            llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem>(beginFailureFilesystem.release()), {},
            &failingAction);
        ok &= expect(!beginFailure && failingAction.beginCalls == 1 && failingAction.endCalls == 0,
                     "failed BeginSourceFile never calls EndSourceFileAction");
    }
    {
        CppSessionOptions base;
        base.language = CppLanguageMode::Cpp;
        base.languageStandard = "c++20";
        base.targetTriple = "x86_64-pc-windows-msvc";
        base.driverReferencePath = "A:/tools/clang.exe";
        base.resourceDirectory = "A:/tools/resource";
        base.workingDirectory = "A:/work";
        base.includePaths = {{"A:/include/one", CppIncludeKind::User},
                             {"A:/include/two", CppIncludeKind::System}};
        base.macros = {{CppMacroAction::Define, "A=1"}, {CppMacroAction::Undefine, "B"}};
        base.input = CppBufferInput{"A:/work/main.cpp", "int value;"};
        const auto identity = cppSessionConfigurationIdentity(base);
        auto changed = [&](auto mutate) {
            auto copy = base;
            mutate(copy);
            return cppSessionConfigurationIdentity(copy) != identity;
        };
        ok &= expect(identity.size() == 64 && identity == cppSessionConfigurationIdentity(base) &&
            changed([](auto& o) { o.language = CppLanguageMode::C; }) &&
            changed([](auto& o) { o.languageStandard = "c++23"; }) &&
            changed([](auto& o) { o.targetTriple = "other"; }) &&
            changed([](auto& o) { o.driverReferencePath = "A:/other/clang.exe"; }) &&
            changed([](auto& o) { o.resourceDirectory = "A:/other/resource"; }) &&
            changed([](auto& o) { o.workingDirectory = "A:/other/work"; }) &&
            changed([](auto& o) { o.includePaths[0].path = "A:/other/include"; }) &&
            changed([](auto& o) { o.includePaths[0].path = "A:/include/dir/../one"; }) &&
            changed([](auto& o) { o.includePaths[0].kind = CppIncludeKind::System; }) &&
            changed([](auto& o) { std::swap(o.includePaths[0], o.includePaths[1]); }) &&
            changed([](auto& o) { o.macros[0].action = CppMacroAction::Undefine; }) &&
            changed([](auto& o) { o.macros[0].text = "A=2"; }) &&
            changed([](auto& o) { std::swap(o.macros[0], o.macros[1]); }) &&
            changed([](auto& o) { std::get<CppBufferInput>(o.input).virtualPath = "A:/work/other.cpp"; }) &&
            changed([](auto& o) { std::get<CppBufferInput>(o.input).contents = "int other;"; }) &&
            changed([](auto& o) { o.input = CppFileInput{"A:/work/main.cpp"}; }),
            "session configuration identity is stable and distinguishes every option and order");
        auto file = base;
        file.input = CppFileInput{"A:/work/main.cpp"};
        auto otherFile = file;
        otherFile.input = CppFileInput{"A:/work/other.cpp"};
        ok &= expect(cppSessionConfigurationIdentity(file) != cppSessionConfigurationIdentity(otherFile),
                     "session configuration identity distinguishes file input paths");
    }
    {
        namespace fs = std::filesystem;
        const auto root = fs::absolute(fs::path(argv[2]) / "cpp-fixtures" / "no-replace-transaction");
        std::error_code ec;
        fs::create_directories(root, ec);
        const auto primary = root / "primary.txt";
        const auto protectedPath = root / "protected.dll";
        const auto stagePrimary = root / "primary.stage";
        const auto stageProtected = root / "protected.stage";
        { std::ofstream file(primary, std::ios::binary); file << "original"; }
        { std::ofstream file(stagePrimary, std::ios::binary); file << "replacement"; }
        { std::ofstream file(stageProtected, std::ios::binary); file << "new-runtime"; }
        fs::remove(protectedPath, ec);
        nativesharp::basic::ArtifactTransaction transaction;
        std::string error;
        const bool staged = !ec && transaction.addStaged(primary, stagePrimary, error) &&
            transaction.addStagedNoReplace(protectedPath, stageProtected, error);
        { std::ofstream file(protectedPath, std::ios::binary); file << "concurrent"; }
        const bool rejected = staged && !transaction.commit(error);
        const auto read = [](const fs::path& path) {
            std::ifstream file(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        };
        ok &= expect(rejected && error.find("rollback-complete") != std::string::npos &&
                         read(primary) == "original" && read(protectedPath) == "concurrent",
                     "no-replace transaction rejects late destination and rolls back earlier artifact");
        fs::remove(protectedPath, ec);
        nativesharp::basic::ArtifactTransaction freeTransaction;
        const bool installed = !ec && freeTransaction.addStagedNoReplace(protectedPath, stageProtected, error) &&
            freeTransaction.commit(error);
        ok &= expect(installed && read(protectedPath) == "new-runtime",
                     "no-replace transaction installs when destination remains absent");
    }
    {
        using namespace nativesharp::interop;
        auto model = std::make_shared<LibraryMetadata>();
        LibraryMetadataType keep;
        keep.identityKey = "Example::Keep";
        keep.fullyQualifiedName = "Keep";
        LibraryMetadataType drop;
        drop.identityKey = "Example::Drop";
        drop.fullyQualifiedName = "Drop";
        model->types = {keep, drop};
        LibraryMetadataCppBinaryDependency first;
        first.libraryFileName = "one.lib";
        LibraryMetadataCppBinaryDependency second;
        second.libraryFileName = "two.lib";
        model->cppBinaryDependencies = {first, second, first};
        model->cppCRecordCatalog = {1, 2, 3, 4};
        std::vector<std::shared_ptr<const LibraryMetadata>> modules = {model};
        nativesharp::compilation::PrecompiledSurfaceReachability reachability(modules, {false});
        reachability.reach({"Keep"});
        const auto reduced = reachability.prunedMetadata(0);
        ok &= expect(reduced != model && reduced->types.size() == 1 &&
                         reduced->types[0].identityKey == keep.identityKey &&
                         reduced->cppBinaryDependencies == model->cppBinaryDependencies &&
                         reduced->cppCRecordCatalog == model->cppCRecordCatalog,
                     "precompiled surface pruning retains C++ binary dependency order and repetitions");
        nativesharp::compilation::PrecompiledSurfaceReachability full(modules, {true});
        ok &= expect(full.prunedMetadata(0) == model &&
                         full.prunedMetadata(0)->cppBinaryDependencies == model->cppBinaryDependencies,
                     "full precompiled surface keeps the original C++ binary dependencies");
    }
    {
        namespace fs = std::filesystem;
        const fs::path root = fs::absolute(fs::path(argv[2]) / "cpp-fixtures" / "source-files");
        std::error_code ec;
        fs::create_directories(root, ec);
        const auto mainHeader = root / "root.h";
        const auto nestedHeader = root / "nested.h";
        const auto skippedHeader = root / "skipped.h";
        { std::ofstream file(mainHeader); file << "#include \"nested.h\"\n#if 0\n#include \"skipped.h\"\n#endif\n"; }
        { std::ofstream file(nestedHeader, std::ios::binary); file << "struct NestedDependency {};\n"; }
        { std::ofstream file(skippedHeader); file << "struct SkippedDependency {};\n"; }
        auto config = options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp, "");
        config.input = CppFileInput{mainHeader};
        auto session = open(factory, std::move(config));
        ok &= expect(!ec && static_cast<bool>(session), "file-dependency fixture opens");
        if (session)
        {
            auto files = session->sourceFilesSnapshot();
            auto contents = session->sourceFileContentsSnapshot();
            const auto contains = [&](const fs::path& path) {
                return files && std::find(files->begin(), files->end(), path.lexically_normal()) != files->end();
            };
            ok &= expect(files && contains(mainHeader) && contains(nestedHeader) && !contains(skippedHeader) &&
                             std::is_sorted(files->begin(), files->end()) &&
                             std::adjacent_find(files->begin(), files->end()) == files->end(),
                         "Clang source-file snapshot includes observed transitive headers only once");
            const auto expectedNested = nativesharp::basic::sha256(std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>("struct NestedDependency {};\n"), 28));
            const auto nested = contents ? std::find_if(contents->begin(), contents->end(), [&](const auto& item) {
                return item.path == nestedHeader.lexically_normal();
            }) : std::vector<CppSourceFileContent>::const_iterator{};
            nativesharp::basic::Sha256Digest rootDigest{};
            std::uintmax_t rootSize = 0;
            std::string rootError;
            const bool rootHashed = nativesharp::basic::sha256File(mainHeader, rootDigest, rootSize, rootError);
            const auto rootEntry = contents ? std::find_if(contents->begin(), contents->end(), [&](const auto& item) {
                return item.path == mainHeader.lexically_normal();
            }) : std::vector<CppSourceFileContent>::const_iterator{};
            ok &= expect(rootHashed && contents && rootEntry != contents->end() &&
                             rootEntry->size == rootSize && rootEntry->digest == rootDigest,
                         "Clang source-file content snapshot hashes the physical root header");
            ok &= expect(contents && nested != contents->end() && nested->size == 28 && nested->digest == expectedNested,
                         "Clang source-file content snapshot hashes the observed transitive header");
            session->close();
            ok &= expect(!session->sourceFilesSnapshot() && !session->sourceFileContentsSnapshot() &&
                             contains(nestedHeader) && contents && nested != contents->end() && nested->digest == expectedNested,
                         "copied source-file snapshot survives session closure");
            { std::ofstream file(nestedHeader, std::ios::binary | std::ios::trunc); file << "struct SestedDependency {};\n"; }
            auto changedConfig = options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp, "");
            changedConfig.input = CppFileInput{mainHeader};
            auto changedSession = open(factory, std::move(changedConfig));
            auto changedContents = changedSession ? changedSession->sourceFileContentsSnapshot() : std::nullopt;
            const auto changedNested = changedContents ? std::find_if(changedContents->begin(), changedContents->end(),
                [&](const auto& item) { return item.path == nestedHeader.lexically_normal(); }) :
                std::vector<CppSourceFileContent>::const_iterator{};
            const auto changedRecords = changedSession ? changedSession->recordsSnapshot() : std::nullopt;
            ok &= expect(changedContents && changedNested != changedContents->end() &&
                         changedNested->size == 28 && changedNested->digest != expectedNested &&
                         changedRecords && recordByName(*changedRecords, "SestedDependency") &&
                         !recordByName(*changedRecords, "NestedDependency"),
                         "reopening observes a same-size transitive header change and updated declarations");
        }
    }
    {
        namespace fs = std::filesystem;
        const fs::path root = fs::absolute(fs::path(argv[2]) / "cpp-fixtures" / "binary-catalog");
        std::error_code ec;
        fs::create_directories(root / "first", ec);
        if (!ec) fs::create_directories(root / "second", ec);
        const fs::path library = root / "first" / "api.lib";
        const fs::path duplicate = root / "second" / "API.lib";
        const fs::path runtime = root / "first" / "api.dll";
        { std::ofstream file(library, std::ios::binary); file << "first"; }
        { std::ofstream file(duplicate, std::ios::binary); file << "different"; }
        { std::ofstream file(runtime, std::ios::binary); file << "runtime"; }
        nativesharp::compilation::CppLibraryReference first;
        first.libraryPath = library; first.runtimePath = runtime;
        nativesharp::compilation::CppLibraryReference headerOnly;
        std::vector<nativesharp::interop::CppBinaryDependency> catalog;
        std::string catalogError;
        const std::vector<nativesharp::compilation::CppLibraryReference> references = {headerOnly, first, first};
        const bool built = !ec && nativesharp::interop::collectCppBinaryDependencies(references, catalog, catalogError);
        const std::string expected = "a7937b64b8caa58f03721bb6bacf5c78cb235febe0e70b1b84cd99541461a08e";
        ok &= expect(built && catalog.size() == 2 && catalog[0].library.fileName == "api.lib" &&
                         catalog[0].library.size == 5 && nativesharp::basic::sha256Hex(catalog[0].library.digest) == expected &&
                         catalog[0].runtime.fileName == "api.dll" && catalog[0].runtime.size == 7 &&
                         catalog[1].library.digest == catalog[0].library.digest,
                     "C++ binary catalog hashes artifacts and preserves link-order duplicates");
        std::vector<nativesharp::interop::ResolvedCppBinaryDependency> resolved;
        const bool resolvedOk = built && nativesharp::interop::resolveCppBinaryDependencies(
            catalog, root / "first", resolved, catalogError);
        ok &= expect(resolvedOk && resolved.size() == 2 && resolved[0].libraryPath == library &&
                         resolved[0].runtimePath == runtime && resolved[1].libraryPath == library,
                     "C++ binary consumer resolves verified libraries and runtimes in link order");
        { std::ofstream file(library, std::ios::binary | std::ios::trunc); file << "other"; }
        ok &= expect(!nativesharp::interop::resolveCppBinaryDependencies(catalog, root / "first", resolved, catalogError) &&
                         catalogError.find("identity mismatch") != std::string::npos && resolved.size() == 2 &&
                         resolved[0].libraryPath == library,
                     "C++ binary consumer rejects modified same-size library without changing output");
        { std::ofstream file(library, std::ios::binary | std::ios::trunc); file << "first"; }
        auto unsafeCatalog = catalog;
        if (!unsafeCatalog.empty()) unsafeCatalog[0].library.fileName = "../api.lib";
        ok &= expect(!nativesharp::interop::resolveCppBinaryDependencies(unsafeCatalog, root / "first", resolved, catalogError) &&
                         resolved.size() == 2 && resolved[0].libraryPath == library,
                     "C++ binary consumer rejects parent traversal without changing output");
        nativesharp::compilation::CppLibraryReference collision;
        collision.libraryPath = duplicate;
        const std::vector<nativesharp::compilation::CppLibraryReference> conflicting = {first, collision};
        const bool collisionRejected = !nativesharp::interop::collectCppBinaryDependencies(conflicting, catalog, catalogError);
        ok &= expect(collisionRejected && catalog.size() == 2 && nativesharp::basic::sha256Hex(catalog[0].library.digest) == expected,
                     "C++ binary catalog rejects conflicting Windows names without changing output");
        collision.libraryPath = root / "missing.lib";
        const std::vector<nativesharp::compilation::CppLibraryReference> missing = {first, collision};
        ok &= expect(!nativesharp::interop::collectCppBinaryDependencies(missing, catalog, catalogError) &&
                         catalog.size() == 2 && nativesharp::basic::sha256Hex(catalog[0].library.digest) == expected,
                     "C++ binary catalog rejects missing files without changing output");
        {
            using namespace nativesharp::compilation;
            const auto otherPath = root / "first" / "other.lib";
            { std::ofstream file(otherPath, std::ios::binary); file << "second"; }
            CppLibraryReference other;
            other.libraryPath = otherPath;
            const auto makeProject = [&](const char* name, std::vector<CppLibraryReference> refs) {
                return Project(root / name, name, ProjectOutputKind::Library, ProjectSourceSet(root),
                    {}, {}, {}, nativesharp::language::PanicProfile::Abort, {}, {}, {}, std::move(refs));
            };
            std::vector<Project> projects;
            projects.push_back(makeProject("dep.nsproj", {headerOnly, first}));
            projects.push_back(makeProject("root.nsproj", {other, first}));
            ProjectGraph graph(root / "root.nsproj", std::move(projects));
            std::vector<LibraryMetadataCppBinaryDependency> graphCatalog;
            const bool graphBuilt = collectCppGraphBinaryMetadataDependencies(graph, graphCatalog, catalogError);
            ok &= expect(graphBuilt && graphCatalog.size() == 3 && graphCatalog[0].libraryFileName == "api.lib" &&
                             graphCatalog[1].libraryFileName == "other.lib" && graphCatalog[2].libraryFileName == "api.lib",
                         "C++ binary catalog includes references from all projects in graph order");
            collision.libraryPath = duplicate;
            std::vector<Project> conflictProjects;
            conflictProjects.push_back(makeProject("dep.nsproj", {first}));
            conflictProjects.push_back(makeProject("root.nsproj", {collision}));
            ProjectGraph conflictingGraph(root / "root.nsproj", std::move(conflictProjects));
            ok &= expect(!collectCppGraphBinaryMetadataDependencies(conflictingGraph, graphCatalog, catalogError) &&
                             graphCatalog.size() == 3 && graphCatalog[1].libraryFileName == "other.lib",
                         "C++ graph catalog rejects cross-project name conflicts without changing output");
        }
        if (built && !catalog.empty())
        {
        nativesharp::interop::LibraryMetadata model;
        model.formatMinor = 41;
        model.moduleName = "BinaryCatalogTest";
        model.producerVersion = "test";
        model.targetTriple = "x86_64-pc-windows-msvc";
        std::string metadataError;
        auto libraryOnly = first;
        libraryOnly.runtimePath.clear();
        const std::vector<nativesharp::compilation::CppLibraryReference> metadataReferences = {first, libraryOnly, first};
        const bool metadataCollected = nativesharp::interop::collectCppBinaryMetadataDependencies(
            metadataReferences, model.cppBinaryDependencies, metadataError);
        nativesharp::interop::LibraryMetadataCodec codec;
        const auto metadataPath = root / "cpp-closure.nsmeta";
        nativesharp::interop::LibraryMetadata decoded;
        const bool encoded = metadataCollected && codec.writeModel(model, metadataPath, metadataError) &&
            codec.read(metadataPath, decoded, metadataError);
        ok &= expect(encoded && decoded.formatMinor == 41 && decoded.cppBinaryDependencies == model.cppBinaryDependencies,
                     "NSMETA preserves ordered C++ binary dependencies and repeated link inputs");
        std::vector<nativesharp::interop::ResolvedCppBinaryDependency> decodedPaths;
        const bool decodedResolved = encoded && nativesharp::interop::resolveCppBinaryMetadataDependencies(
            decoded.cppBinaryDependencies, root / "first", decodedPaths, metadataError);
        ok &= expect(decodedResolved && decodedPaths.size() == 3 && decodedPaths[0].libraryPath == library &&
                         decodedPaths[0].runtimePath == runtime && decodedPaths[1].runtimePath.empty() &&
                         decodedPaths[2].libraryPath == library,
                     "serialized C++ binary closure resolves in order with an optional runtime");
        { std::ofstream file(library, std::ios::binary | std::ios::trunc); file << "other"; }
        ok &= expect(!nativesharp::interop::resolveCppBinaryMetadataDependencies(
                         decoded.cppBinaryDependencies, root / "first", decodedPaths, metadataError) &&
                         metadataError.find("identity mismatch") != std::string::npos &&
                         decodedPaths.size() == 3 && decodedPaths[0].libraryPath == library,
                     "serialized C++ closure rejects modified files without changing resolved output");
        { std::ofstream file(library, std::ios::binary | std::ios::trunc); file << "first"; }
        ok &= expect(encoded && codec.verifyCurrentCompiler(decoded, metadataError),
                     "NSMETA accepts structurally valid C++ binary and runtime dependencies");
        auto oldModel = model;
        oldModel.formatMinor = 40;
        ok &= expect(!codec.writeModel(oldModel, root / "invalid-old.nsmeta", metadataError),
                     "NSMETA 1.40 refuses to silently discard C++ dependencies");
        auto cCatalogModel = model;
        cCatalogModel.cppCRecordCatalog = {1, 2, 3};
        nativesharp::interop::LibraryMetadataMerger merger;
        nativesharp::interop::LibraryMetadata mergedCatalog;
        const std::vector<nativesharp::interop::LibraryMetadata> matchingCatalogs{cCatalogModel, cCatalogModel};
        ok &= expect(merger.merge(matchingCatalogs, mergedCatalog, metadataError) &&
                     mergedCatalog.cppCRecordCatalog == cCatalogModel.cppCRecordCatalog,
                     "metadata merger preserves matching C record catalogues");
        auto disagreeingCatalog = cCatalogModel;
        disagreeingCatalog.cppCRecordCatalog = {4};
        const std::vector<nativesharp::interop::LibraryMetadata> disagreeingCatalogs{cCatalogModel, disagreeingCatalog};
        ok &= expect(!merger.merge(disagreeingCatalogs, mergedCatalog, metadataError),
                     "metadata merger rejects conflicting C record catalogues");
        ok &= expect(!codec.writeModel(cCatalogModel, root / "old-c-records.nsmeta", metadataError),
                     "NSMETA 1.41 refuses to discard a C record catalogue");
        oldModel.cppBinaryDependencies.clear();
        nativesharp::interop::LibraryMetadata oldDecoded;
        const bool oldRoundTrip = codec.writeModel(oldModel, root / "old.nsmeta", metadataError) &&
            codec.read(root / "old.nsmeta", oldDecoded, metadataError) &&
            codec.verifyCurrentCompiler(oldDecoded, metadataError);
        ok &= expect(oldRoundTrip && oldDecoded.formatMinor == 40 && oldDecoded.cppBinaryDependencies.empty(),
                     "NSMETA 1.40 without C++ closure remains readable and valid");
        auto baseline = model;
        baseline.cppBinaryDependencies.clear();
        nativesharp::interop::LibraryMetadata baselineDecoded;
        const bool baselineRoundTrip = codec.writeModel(baseline, root / "baseline.nsmeta", metadataError) &&
            codec.read(root / "baseline.nsmeta", baselineDecoded, metadataError);
        ok &= expect(encoded && baselineRoundTrip && decoded.abiFingerprint == baselineDecoded.abiFingerprint,
                     "private C++ binary closure does not change the public ABI fingerprint");
        }
    }
    {
        const std::vector<std::string> current{"A", "B"};
        std::vector<std::string> visited;
        const auto relative = nativesharp::frontend::resolveQualifiedNamespaceTarget(
            current, "C", [&](const std::string& candidate) {
                visited.push_back(candidate);
                return candidate == "A.C" || candidate == "C";
            });
        const bool relativeOrder = relative == "A.C" && visited == std::vector<std::string>({"A.B.C", "A.C"});
        visited.clear();
        const auto global = nativesharp::frontend::resolveQualifiedNamespaceTarget(
            current, "C", [&](const std::string& candidate) {
                visited.push_back(candidate);
                return candidate == "C";
            });
        const bool globalOrder = global == "C" && visited == std::vector<std::string>({"A.B.C", "A.C", "C"});
        const auto missing = nativesharp::frontend::resolveQualifiedNamespaceTarget(
            current, "Missing", [](const std::string&) { return false; });
        ok &= expect(relativeOrder && globalOrder && !missing,
                     "namespace target lookup prefers nearest visible scope, then global, and reports absence");
    }
    {
        using nativesharp::FileId;
        using nativesharp::SourceLocation;
        using nativesharp::parser::ScopedKnownTypeNames;
        const FileId file{1};
        const ScopedKnownTypeNames names({
            {file, 0, 100, {"Global"}},
            {file, 10, 30, {"Left"}},
            {file, 15, 20, {"Nested"}},
            {file, 30, 50, {"Right"}}});
        ok &= expect(names.contains(SourceLocation(file, 15), "Global") &&
                     names.contains(SourceLocation(file, 15), "Left") &&
                     names.contains(SourceLocation(file, 15), "Nested") &&
                     !names.contains(SourceLocation(file, 20), "Nested") &&
                     !names.contains(SourceLocation(file, 30), "Left") &&
                     names.contains(SourceLocation(file, 30), "Right") &&
                     !names.contains(SourceLocation(file, 100), "Global") &&
                     !names.contains(SourceLocation(FileId{2}, 15), "Left") &&
                     !names.contains(SourceLocation{}, "Global") &&
                     !names.contains(SourceLocation(file, 15), ""),
                     "scoped C++ parser names respect files, nested scopes, siblings and half-open boundaries");
        const ScopedKnownTypeNames effective({{file, 0, 100, {"Parent"}}, {file, 10, 20, {"Child"}}}, true);
        ok &= expect(effective.contains(SourceLocation(file, 5), "Parent") &&
                     !effective.contains(SourceLocation(file, 15), "Parent") &&
                     effective.contains(SourceLocation(file, 15), "Child") &&
                     effective.contains(SourceLocation(file, 20), "Parent"),
                     "effective scoped name sets use the innermost complete environment");
    }
    {
        using nativesharp::FileId;
        using nativesharp::parser::ScopedKnownTypeNames;
        const FileId file{77};
        const std::string source = "namespace Left { int F() { Item* p = null; return 0; } } "
            "namespace Right { int G() { int Item = 2; int p = 3; Item * p; return Item; } }";
        const auto right = static_cast<std::uint32_t>(source.find("namespace Right"));
        const ScopedKnownTypeNames scopes({{file, 0, right, {"Item"}}});
        nativesharp::source::SourceFile input(file, "scoped-test.ns", source);
        nativesharp::frontend::ProjectSyntaxUnit unit(0, 0, "ScopeTest", file);
        nativesharp::diagnostics::DiagnosticEngine diagnostics;
        const auto known = nativesharp::parser::buildKnownTypeNameSet();
        const bool parsed = unit.lex(input, diagnostics) && unit.parse(diagnostics, known, &scopes);
        bool pointer = false;
        bool multiply = false;
        std::vector<const nativesharp::ast::AstNode*> pending{unit.syntaxTree()};
        std::unordered_set<const nativesharp::ast::AstNode*> seen;
        while (!pending.empty())
        {
            const auto* node = pending.back(); pending.pop_back();
            if (!node || !seen.insert(node).second) continue;
            pointer |= node->kind() == nativesharp::ast::AstNodeKind::VariableDeclarationStatement &&
                node->typeSyntax() && node->typeSyntax()->baseName() == "Item";
            multiply |= node->kind() == nativesharp::ast::AstNodeKind::BinaryExpression &&
                node->tokenKind() == nativesharp::lexer::TokenKind::Star;
            pending.push_back(node->first()); pending.push_back(node->second()); pending.push_back(node->third());
            for (const auto* child : node->children()) pending.push_back(child);
        }
        ok &= expect(parsed && pointer && multiply && !diagnostics.hasErrors(),
                     "parser consumes scoped names only at the source location of each statement");
    }
    {
        const nativesharp::FileId file{78};
        const std::string source = "using Root; using Root; namespace A.B { using Alias = Shared.Token; "
            "namespace Inner { using Shared; int F() { using Fake; return 0; } } } "
            "namespace Other { int G() { return 0; } }";
        nativesharp::source::SourceFile input(file, "scope-scan.ns", source);
        nativesharp::frontend::ProjectSyntaxUnit unit(0, 0, "ScopeTest", file);
        nativesharp::diagnostics::DiagnosticEngine diagnostics;
        const bool lexed = unit.lex(input, diagnostics);
        const auto scopes = nativesharp::parser::lexicalNamespaceScopes(unit.tokens());
        ok &= expect(lexed && scopes.size() == 4 && scopes[0].directives.size() == 2 &&
                     scopes[0].directives[0].target == std::vector<std::string>{"Root"} &&
                     scopes[1].namespaceSegments == std::vector<std::string>({"A", "B"}) &&
                     scopes[1].parent == 0 && scopes[1].directives.size() == 1 &&
                     scopes[1].directives[0].alias == "Alias" &&
                     scopes[1].directives[0].target == std::vector<std::string>({"Shared", "Token"}) &&
                     scopes[2].parent == 1 && scopes[2].directives.size() == 1 &&
                     scopes[2].namespaceSegments == std::vector<std::string>({"A", "B", "Inner"}) &&
                     scopes[3].parent == 0 && scopes[2].end <= scopes[3].begin,
                     "lexical namespace scopes preserve parents, directives and skip function bodies");
        const std::vector<std::string> visibleNamespaces{"Root", "Shared", "A.B.Inner.Shared"};
        const auto environments = nativesharp::frontend::lexicalNamespaceEnvironments(scopes, visibleNamespaces);
        ok &= expect(environments && environments->size() == 4 &&
                     (*environments)[0].importedNamespaces == std::vector<std::string>{"Root"} &&
                     (*environments)[1].aliases.size() == 1 &&
                     (*environments)[2].importedNamespaces ==
                         std::vector<std::string>({"Root", "A.B.Inner.Shared"}) &&
                     (*environments)[2].aliases.size() == 1 &&
                     (*environments)[3].importedNamespaces == std::vector<std::string>{"Root"} &&
                     (*environments)[3].aliases.empty(),
                     "namespace environments use relative priority and inherit only through parents");
        const std::vector<std::string> missingNamespaces{"Root"};
        const auto missingEnvironments = nativesharp::frontend::lexicalNamespaceEnvironments(scopes, missingNamespaces);
        ok &= expect(missingEnvironments && (*missingEnvironments)[2].unresolvedNamespaces.size() == 1 &&
                     (*missingEnvironments)[3].unresolvedNamespaces.empty(),
                     "unresolved namespace directives stay visible in their own scope");
        const std::string fileSource = "namespace A.B; using Shared; int F() { return 0; }";
        nativesharp::source::SourceFile fileInput(file, "file-scope.ns", fileSource);
        nativesharp::frontend::ProjectSyntaxUnit fileUnit(0, 0, "ScopeTest", file);
        nativesharp::diagnostics::DiagnosticEngine fileDiagnostics;
        const bool fileLexed = fileUnit.lex(fileInput, fileDiagnostics);
        const auto fileScopes = nativesharp::parser::lexicalNamespaceScopes(fileUnit.tokens());
        ok &= expect(fileLexed && fileScopes.size() == 2 && fileScopes[1].parent == 0 &&
                     fileScopes[1].namespaceSegments == std::vector<std::string>({"A", "B"}) &&
                     fileScopes[1].directives.size() == 1 && fileScopes[1].end == fileScopes[0].end,
                     "file-scoped namespace retains its using until the end of the file");
    }
    {
        const nativesharp::FileId file{79};
        const std::string source = "namespace Left { using Shared; int F() { Item* p = null; return 0; } } "
            "namespace Right { int G() { Shared.Item* q = null; int Item = 2; int p = 3; "
            "Item * p; return Item; } }";
        nativesharp::source::SourceFile input(file, "cpp-scope-seeds.ns", source);
        nativesharp::frontend::ProjectSyntaxUnit unit(0, 0, "ScopeTest", file);
        nativesharp::diagnostics::DiagnosticEngine diagnostics;
        const bool lexed = unit.lex(input, diagnostics);
        const auto lexical = nativesharp::parser::lexicalNamespaceScopes(unit.tokens());
        const std::vector<std::string> namespaces{"Shared"};
        const std::vector<std::string> types{"Shared.Item"};
        const auto scoped = nativesharp::frontend::cppScopedTypeNames(lexical, namespaces, types);
        const auto leftPos = static_cast<std::uint32_t>(source.find("Item* p"));
        const auto rightPos = static_cast<std::uint32_t>(source.find("Item * p"));
        const bool visibility = scoped && scoped->contains(nativesharp::SourceLocation(file, leftPos), "Item") &&
            !scoped->contains(nativesharp::SourceLocation(file, rightPos), "Item") &&
            scoped->contains(nativesharp::SourceLocation(file, rightPos), "Shared.Item");
        const auto known = nativesharp::parser::buildKnownTypeNameSet();
        const bool parsed = lexed && scoped && unit.parse(diagnostics, known, &*scoped);
        bool pointer = false;
        bool multiply = false;
        std::vector<const nativesharp::ast::AstNode*> pending{unit.syntaxTree()};
        std::unordered_set<const nativesharp::ast::AstNode*> seen;
        while (!pending.empty())
        {
            const auto* node = pending.back(); pending.pop_back();
            if (!node || !seen.insert(node).second) continue;
            pointer |= node->kind() == nativesharp::ast::AstNodeKind::VariableDeclarationStatement &&
                node->typeSyntax() && node->typeSyntax()->baseName() == "Item";
            multiply |= node->kind() == nativesharp::ast::AstNodeKind::BinaryExpression &&
                node->tokenKind() == nativesharp::lexer::TokenKind::Star;
            pending.push_back(node->first()); pending.push_back(node->second()); pending.push_back(node->third());
            for (const auto* child : node->children()) pending.push_back(child);
        }
        ok &= expect(visibility && parsed && pointer && multiply && !diagnostics.hasErrors(),
                     "C++ scoped seeds follow namespace imports without affecting sibling expressions");
        auto unsupported = lexical;
        unsupported[0].directives.push_back({"Alias", {"Shared", "Item"}});
        const auto simpleAlias = nativesharp::frontend::cppScopedTypeNames(unsupported, namespaces, types);
        ok &= expect(simpleAlias && simpleAlias->contains(nativesharp::SourceLocation(file, leftPos), "Alias"),
                     "scoped seed preparation recognizes a simple alias to a C++ type");
        const std::string aliasSource = "using P = Shared.Item*; using Count = int; "
            "int F() { P p = null; Count n = 1; return n; }";
        nativesharp::source::SourceFile aliasInput(file, "cpp-complex-alias.ns", aliasSource);
        nativesharp::frontend::ProjectSyntaxUnit aliasUnit(0, 0, "ScopeTest", file);
        nativesharp::diagnostics::DiagnosticEngine aliasDiagnostics;
        const bool aliasLexed = aliasUnit.lex(aliasInput, aliasDiagnostics);
        const auto aliasScopes = nativesharp::parser::lexicalNamespaceScopes(aliasUnit.tokens());
        const auto aliasEnvironment = nativesharp::frontend::lexicalNamespaceEnvironments(aliasScopes, namespaces);
        const auto aliasNames = nativesharp::frontend::cppScopedTypeNames(aliasScopes, namespaces, types);
        const auto aliasKnown = nativesharp::parser::buildKnownTypeNameSet();
        const bool aliasParsed = aliasLexed && aliasNames && aliasUnit.parse(aliasDiagnostics, aliasKnown, &*aliasNames);
        ok &= expect(aliasLexed && aliasEnvironment &&
                     (*aliasEnvironment)[0].unsupportedDirectives.empty() &&
                     (*aliasEnvironment)[0].aliases.size() == 2 &&
                     (*aliasEnvironment)[0].aliases[0].directive.explicitTypeSyntax &&
                     (*aliasEnvironment)[0].aliases[1].directive.explicitTypeSyntax &&
                     aliasParsed && !aliasDiagnostics.hasErrors(),
                     "record pointer and builtin aliases remain type names and parse with their full syntax");
        const std::string invalidSource = "using Bad = Shared.Item + 1; int F() { return 0; }";
        nativesharp::source::SourceFile invalidInput(file, "cpp-invalid-alias.ns", invalidSource);
        nativesharp::frontend::ProjectSyntaxUnit invalidUnit(0, 0, "ScopeTest", file);
        nativesharp::diagnostics::DiagnosticEngine invalidDiagnostics;
        const bool invalidLexed = invalidUnit.lex(invalidInput, invalidDiagnostics);
        const auto invalidScopes = nativesharp::parser::lexicalNamespaceScopes(invalidUnit.tokens());
        const auto invalidEnvironment = nativesharp::frontend::lexicalNamespaceEnvironments(invalidScopes, namespaces);
        ok &= expect(invalidLexed && invalidEnvironment &&
                     (*invalidEnvironment)[0].unsupportedDirectives.size() == 1 &&
                     !nativesharp::frontend::cppScopedTypeNames(invalidScopes, namespaces, types) &&
                     !invalidUnit.parse(invalidDiagnostics, aliasKnown) && invalidDiagnostics.hasErrors(),
                     "invalid alias remains unsupported and the ordinary parser reports its syntax error");
    }
    {
        const nativesharp::FileId file{80};
        const std::string source = "namespace Left { using N = Shared; using T = Shared.Item; using U = T; "
            "int F() { N.Item* n = null; T* t = null; U* u = null; return 0; } "
            "namespace Child { int H() { U* p = null; return 0; } } } "
            "namespace Right { int G() { int T = 1; int p = 2; T * p; return T; } }";
        nativesharp::source::SourceFile input(file, "cpp-scope-aliases.ns", source);
        nativesharp::frontend::ProjectSyntaxUnit unit(0, 0, "ScopeTest", file);
        nativesharp::diagnostics::DiagnosticEngine diagnostics;
        const bool lexed = unit.lex(input, diagnostics);
        const auto scopes = nativesharp::parser::lexicalNamespaceScopes(unit.tokens());
        const std::vector<std::string> namespaces{"Shared"};
        const std::vector<std::string> types{"Shared.Item"};
        const auto scoped = nativesharp::frontend::cppScopedTypeNames(scopes, namespaces, types);
        const auto left = static_cast<std::uint32_t>(source.find("N.Item*"));
        const auto right = static_cast<std::uint32_t>(source.find("T * p"));
        const auto child = static_cast<std::uint32_t>(source.find("U* p"));
        const bool correctNames = scoped &&
            scoped->contains(nativesharp::SourceLocation(file, left), "N.Item") &&
            scoped->contains(nativesharp::SourceLocation(file, left), "T") &&
            scoped->contains(nativesharp::SourceLocation(file, left), "U") &&
            scoped->contains(nativesharp::SourceLocation(file, child), "U") &&
            !scoped->contains(nativesharp::SourceLocation(file, right), "T");
        const auto known = nativesharp::parser::buildKnownTypeNameSet();
        const bool parsed = lexed && scoped && unit.parse(diagnostics, known, &*scoped);
        ok &= expect(correctNames && parsed && !diagnostics.hasErrors(),
                     "scoped C++ names resolve namespace and chained type aliases without leaking to siblings");
    }
    {
        using namespace nativesharp::compilation;
        const auto fixture = std::filesystem::path(argv[2]) / "cpp-fixtures";
        const auto projectPath = fixture / "project" / "CppProbe.nsproj";
        std::string error;
        ProjectLoader loader;
        auto project = loader.load(projectPath, error);
        if (!project) std::cerr << "Project load failed: " << error << '\n';
        ok &= expect(project && project->cppLibraryReferences().size() == 2,
                     "project loads C and C++ references");
        if (project && project->cppLibraryReferences().size() == 2)
        {
            const auto& cRef = project->cppLibraryReferences()[0];
            const auto& cppRef = project->cppLibraryReferences()[1];
            ok &= expect(cRef.language == CppLanguageMode::C && cRef.languageStandard == "c11" &&
                         !cRef.libraryPath.empty() && cRef.includePaths.size() == 1 &&
                         cRef.macros.size() == 3 && cRef.macros[0].action == CppMacroAction::Define &&
                         cRef.macros[1].action == CppMacroAction::Undefine &&
                         cRef.macros[2].action == CppMacroAction::Define,
                         "C library options and macro ordering survive loading");
            ok &= expect(cppRef.language == CppLanguageMode::Cpp && cppRef.libraryPath.empty(),
                         "C++ header-only references need no library");
            const CppProjectSessionEnvironment environment{
                "x86_64-pc-windows-msvc", argv[3], argv[1], argv[2], {fixture}};
            auto opened = openCppProjectSessions(project->cppLibraryReferences(), environment, factory, {});
            auto* sessions = std::get_if<CppProjectSessions>(&opened);
            ok &= expect(sessions && sessions->sessions.size() == 2,
                         "project references each open a real Clang session");
            if (sessions && sessions->sessions.size() == 2)
            {
                const auto cOptions = sessions->sessions[0]->optionsSnapshot();
                const auto cppOptions = sessions->sessions[1]->optionsSnapshot();
                const auto* file = std::get_if<CppFileInput>(&cOptions.input);
                ok &= expect(cOptions.language == CppLanguageMode::C && cOptions.macros.size() == 3 &&
                             cOptions.includePaths.size() == 2 &&
                             cOptions.includePaths[1].kind == CppIncludeKind::System &&
                             cOptions.includePaths[1].path == fixture && file && file->path == cRef.headerPath &&
                             cppOptions.language == CppLanguageMode::Cpp && cppOptions.macros.empty() &&
                             cppOptions.languageStandard == "c++20" && cppOptions.includePaths.size() == 1 &&
                             cppOptions.includePaths[0].kind == CppIncludeKind::System,
                             "project session options retain each reference configuration and order");
            }
            std::vector<CppLibraryReference> broken(project->cppLibraryReferences().begin(),
                                                     project->cppLibraryReferences().end());
            auto invalidOptions = broken;
            invalidOptions[1].languageStandard.clear();
            RejectUnexpectedOpen preflightFactory;
            const auto invalidOptionsResult = openCppProjectSessions(invalidOptions, environment, preflightFactory, {});
            const auto* invalidOptionsFailure = std::get_if<CppProjectSessionFailure>(&invalidOptionsResult);
            ok &= expect(invalidOptionsFailure && invalidOptionsFailure->referenceIndex == 1 &&
                         invalidOptionsFailure->failure.phase == "options" && preflightFactory.calls == 0,
                         "invalid later reference is rejected before opening any project session");
            for (const auto* invalidStandard : {"not-a-real-standard", "c11"})
            {
                invalidOptions[1].languageStandard = invalidStandard;
                const auto result = openCppProjectSessions(invalidOptions, environment, preflightFactory, {});
                const auto* failure = std::get_if<CppProjectSessionFailure>(&result);
                ok &= expect(failure && failure->referenceIndex == 1 && failure->failure.phase == "options" &&
                             preflightFactory.calls == 0,
                             "unknown or wrong-language standard is rejected before opening sessions");
            }
            broken[1].headerPath = fixture / "missing-session-header.h";
            auto failed = openCppProjectSessions(broken, environment, factory, {});
            const auto* failure = std::get_if<CppProjectSessionFailure>(&failed);
            ok &= expect(failure && failure->referenceIndex == 1 && failure->failure.phase == "input",
                         "failed second session returns its index without a partial session set");
            CppCancellationToken cancelledToken;
            cancelledToken.cancel();
            auto cancelledProject = openCppProjectSessions(project->cppLibraryReferences(), environment,
                                                            factory, cancelledToken);
            const auto* cancelledProjectFailure = std::get_if<CppProjectSessionFailure>(&cancelledProject);
            ok &= expect(cancelledProjectFailure && cancelledProjectFailure->referenceIndex == 0 &&
                         cancelledProjectFailure->failure.phase == "cancel",
                         "project session opening propagates cancellation");
            auto systemReference = std::vector<CppLibraryReference>{cRef};
            systemReference[0].headerPath = fixture / "system-probe.h";
            auto systemEnvironment = environment;
            systemEnvironment.systemIncludePaths = {fixture / "system"};
            auto systemOpened = openCppProjectSessions(systemReference, systemEnvironment, factory, {});
            const auto* systemSessions = std::get_if<CppProjectSessions>(&systemOpened);
            bool foundSystemFunction = false;
            if (systemSessions && systemSessions->sessions.size() == 1)
            {
                const auto functions = systemSessions->sessions[0]->freeFunctionsSnapshot();
                foundSystemFunction = functions && functionByName(*functions, "common_system_function");
            }
            ok &= expect(foundSystemFunction, "common system include paths are used to parse headers");

            auto projectOpened = openCppProjectSessions(*project, environment, factory, {});
            auto* ownedSessions = std::get_if<CppProjectSessions>(&projectOpened);
            ok &= expect(ownedSessions && ownedSessions->projectFile == project->projectFile(),
                         "project session set retains its owner identity");
            auto otherProject = loader.load(fixture / "other" / "Other.nsproj", error);
            ok &= expect(static_cast<bool>(otherProject), "second project loads");
            if (otherProject)
            {
                auto plainProject = loader.load(fixture / "plain" / "Plain.nsproj", error);
                ok &= expect(static_cast<bool>(plainProject), "project without C/C++ references loads");
                std::vector<Project> graphProjects;
                graphProjects.push_back(*otherProject);
                if (plainProject) graphProjects.push_back(*plainProject);
                graphProjects.push_back(*project);
                ProjectGraph combinedGraph(project->projectFile(), std::move(graphProjects));
                {
                    std::vector<CppProjectSessions> groups(2);
                    groups[0].projectFile = otherProject->projectFile();
                    groups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::C, "struct Opaque; int other_value(void); int pointer_value(struct Opaque* p); extern int c_global; extern volatile int* c_volatile_pointer; static int internal_global;\n")));
                    groups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::C, "int other_value(void);\n")));
                    groups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::Cpp, "namespace other {} namespace { namespace hidden {} }\n")));
                    groups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::Cpp, "namespace other { namespace deep {} }\n")));
                    groups[1].projectFile = project->projectFile();
                    groups[1].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::Cpp, "namespace feature { int value(int); double value(double); extern int shared; extern const int limit; extern volatile int io; extern thread_local int tls; static int local; }\n")));
                    groups[1].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::C, "int other_value(void);\n")));
                    std::vector<Project> originalOrder{*otherProject, *project};
                    ProjectGraph finalGraph(project->projectFile(), std::move(originalOrder));
                    const std::vector<PrecompiledProjectDescriptor> addedModules{{
                        project->projectFile(), fixture / "invented-metadata.nsmeta", "InventoryMetadata"}};
                    ok &= expect(finalGraph.addPrecompiledProjects(addedModules, error),
                                 "test inserts a precompiled module before the source projects");
                    {
                        const auto globals = collectCppSemanticGlobals(finalGraph, groups);
                        const auto* entries = std::get_if<std::vector<CppSemanticGlobalEntry>>(&globals);
                        bool cGlobal = false, cppGlobal = false, internalRejected = false;
                        bool volatileProjected = false, constProjected = false, tlsRejected = false;
                        bool volatilePointerProjected = false;
                        bool cVolatileOptionRetained = false, cppVolatileOptionRetained = false;
                        if (entries)
                            for (const auto& entry : *entries)
                            {
                                const auto* symbol = std::get_if<CppSemanticGlobal>(&entry.projection);
                                const auto* rejected = std::get_if<CppSemanticGlobalFailure>(&entry.projection);
                                if (entry.declaration.name == "c_global")
                                {
                                    cGlobal = symbol && entry.projectOrdinal == 1 && entry.referenceIndex == 0 &&
                                        symbol->externalSymbol == "c_global" && symbol->typeKey == "builtin:int" &&
                                        symbol->namespaceName.empty() && entry.targetTriple == "x86_64-pc-windows-msvc";
                                    cVolatileOptionRetained = entry.microsoftVolatileSemantics.has_value() &&
                                        entry.microsoftVolatileSemantics == groups[0].sessions[0]->microsoftVolatileSemantics();
                                }
                                if (entry.declaration.name == "c_volatile_pointer")
                                    volatilePointerProjected = symbol && symbol->pointerPointeeVolatile &&
                                        symbol->typeKey == "builtin:int*";
                                if (entry.declaration.name == "shared")
                                {
                                    cppGlobal = symbol && entry.projectOrdinal == 2 && entry.referenceIndex == 0 &&
                                        symbol->externalSymbol == "?shared@feature@@3HA" && symbol->namespaceName == "feature" &&
                                        symbol->typeKey == "builtin:int";
                                    cppVolatileOptionRetained = entry.microsoftVolatileSemantics.has_value() &&
                                        entry.microsoftVolatileSemantics == groups[1].sessions[0]->microsoftVolatileSemantics();
                                }
                                if (entry.declaration.name == "local" || entry.declaration.name == "internal_global")
                                    internalRejected |= rejected && !entry.declaration.externalLinkage;
                                if (entry.declaration.name == "io") volatileProjected = symbol && symbol->isVolatile;
                                if (entry.declaration.name == "limit") constProjected = symbol && symbol->isConst;
                                if (entry.declaration.name == "tls") tlsRejected = rejected && entry.declaration.threadLocal;
                            }
                        ok &= expect(entries && entries->size() == 8 && cGlobal && cppGlobal && internalRejected &&
                                     volatilePointerProjected &&
                                     volatileProjected && constProjected && tlsRejected,
                                     "global inventory preserves project, exact symbol, type and unsupported storage contracts");
                        ok &= expect(cVolatileOptionRetained && cppVolatileOptionRetained,
                                     "global inventory retains the effective volatile option of each originating session");
                        if (entries)
                        {
                            nativesharp::frontend::CppFunctionDemand globalDemand;
                            globalDemand.lookupName = "shared";
                            globalDemand.lookupNamespaces = {"feature"};
                            globalDemand.visibleProjects = {2};
                            auto unsupportedDemand = globalDemand;
                            unsupportedDemand.lookupName = "tls";
                            auto invisibleDemand = globalDemand;
                            invisibleDemand.visibleProjects = {1};
                            const std::vector demands{globalDemand, globalDemand, unsupportedDemand, invisibleDemand};
                            std::vector<nativesharp::frontend::NamespaceStateSymbol> provided;
                            std::string providerError;
                            const bool providedOk = provideCppSemanticGlobals(finalGraph, groups, demands, provided, providerError);
                            bool foundShared = false, foundDeferred = false;
                            for (const auto& state : provided)
                            {
                                foundShared |= state.name == "shared" && state.projectOrdinal == 2 &&
                                    state.storageName == "?shared@feature@@3HA" && state.cppImportError.empty() &&
                                    state.cppMicrosoftVolatileSemantics.has_value() &&
                                    state.cppMicrosoftVolatileSemantics == groups[1].sessions[0]->microsoftVolatileSemantics() &&
                                    state.cppStorageSizeBits == std::optional<std::uint64_t>{32} &&
                                    state.cppDeclarationAlignmentBits.has_value();
                                foundDeferred |= state.name == "tls" && state.projectOrdinal == 2 &&
                                    state.cppImportError.find("thread-local") != std::string::npos;
                            }
                            ok &= expect(providedOk && provided.size() == 2 && foundShared && foundDeferred,
                                         "global provider deduplicates demands, keeps project visibility and defers unsupported storage");
                            const auto selected = selectCppSemanticGlobal(*entries, 2, "feature::shared");
                            const auto providedShared = std::find_if(provided.begin(), provided.end(),
                                [](const auto& state) { return state.name == "shared" && state.projectOrdinal == 2; });
                            ok &= expect(selected.candidate && selected.candidate->clangUsr &&
                                         providedShared != provided.end() &&
                                         providedShared->cppClangUsr == selected.candidate->clangUsr,
                                         "global symbol retains the selected Clang USR");
                            const auto internal = selectCppSemanticGlobal(*entries, 2, "feature::local");
                            const auto unsupported = selectCppSemanticGlobal(*entries, 2, "feature::tls");
                            const auto invisible = selectCppSemanticGlobal(*entries, 1, "feature::shared");
                            ok &= expect(selected.candidate && selected.declarationIndices.size() == 1 &&
                                         internal.internalIndices.size() == 1 && !internal.candidate &&
                                         unsupported.unsupportedIndices.size() == 1 && !unsupported.candidate &&
                                         invisible.declarationIndices.empty(),
                                         "global selection retains project, internal and unsupported declarations");
                            if (selected.candidate && !selected.declarationIndices.empty())
                            {
                            auto repeated = *entries;
                            repeated.push_back((*entries)[selected.declarationIndices[0]]);
                            repeated.back().referenceIndex += 10;
                            const auto same = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(same.candidate && same.declarationIndices.size() == 2 && same.conflict.empty(),
                                         "identical external global declarations retain both origins");
                            repeated[selected.declarationIndices[0]].declaration.clangUsr = std::nullopt;
                            repeated.back().declaration.clangUsr = "known-global-usr";
                            const auto knownGlobalUsr = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            repeated[selected.declarationIndices[0]].declaration.clangUsr = "different-global-usr";
                            const auto conflictingGlobalUsr = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(knownGlobalUsr.candidate &&
                                         knownGlobalUsr.candidate->clangUsr == std::optional<std::string>{"known-global-usr"} &&
                                         !conflictingGlobalUsr.candidate && !conflictingGlobalUsr.conflict.empty(),
                                         "global selection retains known USR and rejects contradictory same-name USRs");
                            repeated[selected.declarationIndices[0]].declaration.clangUsr =
                                (*entries)[selected.declarationIndices[0]].declaration.clangUsr;
                            repeated.back().declaration.clangUsr = (*entries)[selected.declarationIndices[0]].declaration.clangUsr;
                            repeated[selected.declarationIndices[0]].microsoftVolatileSemantics = true;
                            repeated.back().microsoftVolatileSemantics = true;
                            const auto agreedVolatile = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(agreedVolatile.candidate &&
                                         agreedVolatile.candidate->microsoftVolatileSemantics == std::optional<bool>{true},
                                         "global selection retains an agreed effective volatile option");
                            repeated.back().microsoftVolatileSemantics = false;
                            const auto differingVolatile = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(differingVolatile.candidate &&
                                         !differingVolatile.candidate->microsoftVolatileSemantics.has_value(),
                                         "global selection does not choose between different volatile options");
                            repeated.back().microsoftVolatileSemantics = std::nullopt;
                            const auto unknownVolatile = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(unknownVolatile.candidate &&
                                         !unknownVolatile.candidate->microsoftVolatileSemantics.has_value(),
                                         "global selection retains unknown volatile options as unknown");
                            repeated[selected.declarationIndices[0]].declaration.declarationAlignmentBits = 128;
                            repeated.back().declaration.declarationAlignmentBits = 32;
                            const auto conservativeAlignment = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(conservativeAlignment.candidate &&
                                         conservativeAlignment.candidate->storageSizeBits == std::optional<std::uint64_t>{32} &&
                                         conservativeAlignment.candidate->declarationAlignmentBits == std::optional<std::uint64_t>{32},
                                         "global selection retains the type size and conservative alignment");
                            repeated.back().declaration.declarationAlignmentBits = std::nullopt;
                            const auto unknownAlignment = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(unknownAlignment.candidate &&
                                         !unknownAlignment.candidate->declarationAlignmentBits.has_value(),
                                         "global selection does not invent an unknown alignment");
                            repeated.back().declaration.type.canonicalSpelling = "double";
                            const auto conflict = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(!conflict.candidate && !conflict.conflict.empty() &&
                                         conflict.declarationIndices.size() == 2,
                                         "different types on repeated global declarations conflict");
                            repeated.back() = (*entries)[selected.declarationIndices[0]];
                            repeated.back().declaration.dllImport = !repeated.back().declaration.dllImport;
                            const auto dllConflict = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(!dllConflict.candidate && !dllConflict.conflict.empty(),
                                         "different DLL import attributes on repeated global declarations conflict");
                            repeated.back() = (*entries)[selected.declarationIndices[0]];
                            repeated.back().declaration.qualifiedName = "feature::alias";
                            repeated.back().declaration.type.isVolatile = true;
                            const auto alias = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(!alias.candidate && !alias.conflict.empty() && alias.aliasIndices.size() == 1,
                                         "an incompatible alias of the same data symbol conflicts");
                            repeated.back().declaration.type.isVolatile = false;
                            repeated.back().targetTriple = "other-target";
                            const auto targetConflict = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(!targetConflict.candidate && !targetConflict.conflict.empty(),
                                         "a symbol alias from a different target cannot be merged");
                            repeated.back() = (*entries)[selected.declarationIndices[0]];
                            repeated[selected.declarationIndices[0]].declaration.clangUsr = "test-global-usr";
                            repeated.back().declaration.clangUsr = "test-global-usr";
                            repeated.back().declaration.qualifiedName = "feature::other_name";
                            repeated.back().declaration.externalSymbol = "different_symbol";
                            const auto usrConflict = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(!usrConflict.candidate && !usrConflict.conflict.empty() &&
                                         usrConflict.aliasIndices.size() == 1,
                                         "the same global USR cannot hide a different external symbol under another name");
                            repeated.back().projectOrdinal = 1;
                            const auto isolatedUsr = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(isolatedUsr.candidate && isolatedUsr.conflict.empty(),
                                         "global USR conflicts do not cross project ownership");
                            repeated.back().projectOrdinal = 2;
                            repeated.back().declaration.externalSymbol = repeated[selected.declarationIndices[0]].declaration.externalSymbol;
                            repeated.back().declaration.clangUsr = "other-global-usr";
                            const auto compatibleAlias = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(compatibleAlias.candidate && compatibleAlias.conflict.empty() &&
                                         compatibleAlias.aliasIndices.size() == 1,
                                         "compatible global aliases with different USRs retain their shared symbol");
                            auto indirect = repeated.back();
                            indirect.declaration.qualifiedName = "feature::indirect";
                            indirect.declaration.externalSymbol = "indirect_symbol";
                            repeated.insert(repeated.begin(), indirect);
                            const auto transitiveConflict = selectCppSemanticGlobal(repeated, 2, "feature::shared");
                            ok &= expect(!transitiveConflict.candidate && !transitiveConflict.conflict.empty() &&
                                         transitiveConflict.aliasIndices.size() == 2,
                                         "global aliases discover USR conflicts transitively regardless of inventory order");
                            }
                        }
                    }
                    {
                        std::vector<CppProjectSessions> recordGroups(2);
                        recordGroups[0].projectFile = otherProject->projectFile();
                        recordGroups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                            CppLanguageMode::Cpp, "typedef int InventoryAlias; namespace { typedef int HiddenAlias; } enum InventoryEnum { FirstEnumValue = -2 }; struct Shared { int x; }; struct Same { int x; }; struct Equal { int x; }; struct Forward; struct OnlyForward; "
                            "Shared* use(Shared*); OnlyForward* store(OnlyForward*); int nestedPtr(Shared**); "
                            "void observe(const OnlyForward*); Shared value(Shared); "
                            "template<class T> struct Box {}; int specialized(Box<int>*);\n")));
                        recordGroups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                            CppLanguageMode::Cpp, "typedef double InventoryAlias; enum InventoryEnum { SecondEnumValue = 7 }; struct Shared { double x; }; struct Same { float x; }; struct Equal { int x; }; struct Forward { int x; };\n")));
                        recordGroups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                            CppLanguageMode::Cpp, "struct Shared { int x; }; struct Same { int x; };\n")));
                        recordGroups[1].projectFile = project->projectFile();
                        recordGroups[1].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                            CppLanguageMode::Cpp, "typedef int InventoryAlias; namespace { struct Hidden {}; } struct Item {}; struct Shared {}; Shared* own(Shared*);\n")));
                        auto aliasInventory = collectCppSemanticTypeAliases(finalGraph, recordGroups);
                        const auto* aliases = std::get_if<std::vector<CppSemanticTypeAliasEntry>>(&aliasInventory);
                        bool aliasFirst = false, aliasSecond = false, aliasOtherProject = false, anonymousAlias = false;
                        if (aliases)
                            for (const auto& entry : *aliases)
                            {
                                anonymousAlias |= entry.declaration.name == "HiddenAlias" &&
                                    entry.declaration.insideAnonymousNamespace;
                                if (entry.declaration.name != "InventoryAlias") continue;
                                aliasFirst |= entry.projectOrdinal == 1 && entry.referenceIndex == 0 &&
                                    entry.declaration.underlyingType.kind == CppTypeKind::SignedInteger;
                                aliasSecond |= entry.projectOrdinal == 1 && entry.referenceIndex == 1 &&
                                    entry.declaration.underlyingType.kind == CppTypeKind::Floating;
                                aliasOtherProject |= entry.projectOrdinal == 2 && entry.referenceIndex == 0 &&
                                    entry.declaration.underlyingType.kind == CppTypeKind::SignedInteger;
                            }
                        ok &= expect(aliasFirst && aliasSecond && aliasOtherProject && anonymousAlias,
                                     "typedef inventory retains conflicting observations and project origins");
                        auto enumInventory = collectCppSemanticEnums(finalGraph, recordGroups);
                        const auto* enums = std::get_if<std::vector<CppSemanticEnumEntry>>(&enumInventory);
                        bool enumFirst = false, enumSecond = false;
                        if (enums)
                            for (const auto& entry : *enums)
                            {
                                if (entry.declaration.name != "InventoryEnum" || entry.declaration.enumerators.size() != 1) continue;
                                enumFirst |= entry.projectOrdinal == 1 && entry.referenceIndex == 0 &&
                                    entry.declaration.enumerators[0].decimalValue == "-2";
                                enumSecond |= entry.projectOrdinal == 1 && entry.referenceIndex == 1 &&
                                    entry.declaration.enumerators[0].decimalValue == "7";
                            }
                        ok &= expect(enumFirst && enumSecond,
                                     "enum inventory retains values and distinct reference origins");
                        {
                            std::vector<CppProjectSessions> enumGroups(1);
                            enumGroups[0].projectFile = otherProject->projectFile();
                            enumGroups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                                CppLanguageMode::C, "enum ChoiceType { Choice = -3 }; enum ChoiceType echo_choice(enum ChoiceType* x); int apply_choice(enum ChoiceType (*cb)(enum ChoiceType*));\n")));
                            nativesharp::frontend::CppFunctionDemand demand;
                            demand.lookupName = "Choice";
                            demand.lookupNamespaces = {""};
                            demand.visibleProjects = {1};
                            std::vector<nativesharp::frontend::NamespaceStateSymbol> states;
                            std::string enumError;
                            const bool provided = provideCppSemanticEnumConstants(finalGraph, enumGroups,
                                std::span<const nativesharp::frontend::CppFunctionDemand>(&demand, 1), states, enumError);
                            const bool correct = provided && states.size() == 1 && states[0].isCppEnumConstant &&
                                states[0].isConst && !states[0].isCppExternal && states[0].integerValue == 0xfffffffdULL;
                            const auto checkEnumeratorUsr = [&](bool conflict) {
                                std::vector<CppProjectSessions> groups(1);
                                groups[0].projectFile = otherProject->projectFile();
                                const auto add = [&](std::optional<std::string> usr) {
                                    auto session = open(factory, options(argv[1], argv[2], argv[3],
                                        CppLanguageMode::C, "enum ChoiceType { Choice = -3 };\n"));
                                    if (!session) return false;
                                    groups[0].sessions.push_back(std::make_unique<AlteredEnumeratorUsrSession>(
                                        std::move(session), std::move(usr)));
                                    return true;
                                };
                                if (!add("test:choice") || !add(std::nullopt) ||
                                    !add(conflict ? "test:other-choice" : "test:choice")) return false;
                                std::vector<nativesharp::frontend::NamespaceStateSymbol> projected;
                                std::string error;
                                if (!provideCppSemanticEnumConstants(finalGraph, groups,
                                    std::span<const nativesharp::frontend::CppFunctionDemand>(&demand, 1),
                                    projected, error) || projected.size() != 1) return false;
                                std::vector<nativesharp::frontend::CppImportedTypeDeclaration> types;
                                if (!provideCppEnumTypes(finalGraph, groups, types, error) || types.size() != 1)
                                    return false;
                                const bool typeAgrees = conflict ? !types[0].deferredImportError.empty() :
                                                                   types[0].deferredImportError.empty();
                                return typeAgrees && (conflict ? !projected[0].cppImportError.empty() :
                                    projected[0].isCppEnumConstant && projected[0].cppImportError.empty() &&
                                    projected[0].cppClangUsr == "test:choice");
                            };
                            ok &= expect(checkEnumeratorUsr(false),
                                "matching enumerator USRs survive an unknown observation");
                            ok &= expect(checkEnumeratorUsr(true),
                                "different enumerator USRs conflict across an unknown observation");
                            {
                                std::vector<CppProjectSessions> groups(1);
                                groups[0].projectFile = otherProject->projectFile();
                                auto first = open(factory, options(argv[1], argv[2], argv[3],
                                    CppLanguageMode::C, "enum ChoiceType { Choice = -3 };\n"));
                                auto second = open(factory, options(argv[1], argv[2], argv[3],
                                    CppLanguageMode::C, "enum ChoiceType { Choice = -3 };\n"));
                                bool detectsTargetChange = false;
                                if (first && second)
                                {
                                    groups[0].sessions.push_back(std::make_unique<AlteredEnumeratorUsrSession>(
                                        std::move(first), "test:choice"));
                                    groups[0].sessions.push_back(std::make_unique<AlteredEnumeratorUsrSession>(
                                        std::move(second), "test:choice", false, true));
                                    std::vector<nativesharp::frontend::NamespaceStateSymbol> projected;
                                    std::string error;
                                    detectsTargetChange = provideCppSemanticEnumConstants(finalGraph, groups,
                                        std::span<const nativesharp::frontend::CppFunctionDemand>(&demand, 1),
                                        projected, error) && projected.size() == 1 &&
                                        !projected[0].cppImportError.empty();
                                }
                                ok &= expect(detectsTargetChange,
                                    "C enumerator observations from different targets conflict");
                            }
                            {
                                std::vector<CppProjectSessions> groups(1);
                                groups[0].projectFile = otherProject->projectFile();
                                auto original = open(factory, options(argv[1], argv[2], argv[3],
                                    CppLanguageMode::C, "enum ChoiceType { Choice = -3 };\n"));
                                auto changed = open(factory, options(argv[1], argv[2], argv[3],
                                    CppLanguageMode::C, "enum ChoiceType { Choice = -3 };\n"));
                                bool detectsTypeChange = false;
                                if (original && changed)
                                {
                                    groups[0].sessions.push_back(std::make_unique<AlteredEnumeratorUsrSession>(
                                        std::move(original), "test:choice"));
                                    groups[0].sessions.push_back(std::make_unique<AlteredEnumeratorUsrSession>(
                                        std::move(changed), "test:choice", true));
                                    std::vector<nativesharp::frontend::CppImportedTypeDeclaration> types;
                                    std::string error;
                                    detectsTypeChange = provideCppEnumTypes(finalGraph, groups, types, error) &&
                                        types.size() == 1 && !types[0].deferredImportError.empty();
                                }
                                ok &= expect(detectsTypeChange,
                                    "enum definitions with equal values and different enumerator types conflict");
                            }
                            std::vector<nativesharp::frontend::CppImportedTypeDeclaration> enumTypes;
                            const bool typeProvided = provideCppEnumTypes(finalGraph, enumGroups, enumTypes, enumError);
                            const bool correctType = typeProvided && enumTypes.size() == 1 &&
                                enumTypes[0].identity.name() == "ChoiceType" && enumTypes[0].enumBase &&
                                enumTypes[0].enumBase->token == nativesharp::lexer::TokenKind::KwInt &&
                                enumTypes[0].objectLayout && enumTypes[0].objectLayout->sizeBytes == 4;
                            ok &= expect(correctType && enumTypes[0].origin == nativesharp::frontend::CppImportedTypeOrigin::CEnum &&
                                         enumTypes[0].targetTriple == "x86_64-pc-windows-msvc",
                                         "C enum retains its origin and target");
                            const auto signatureInventory = collectCppSemanticFunctions(finalGraph, enumGroups);
                            const auto* functions = std::get_if<std::vector<CppSemanticInventoryEntry>>(&signatureInventory);
                            const auto dependencies = functions ? collectCppSignatureEnumDependencies(finalGraph, *functions, enumTypes)
                                : CppSignatureEnumResult{CppSignatureEnumFailure{"function fixture unavailable"}};
                            const auto* enumDependencies = std::get_if<std::vector<CppSignatureEnumDependency>>(&dependencies);
                            auto wrongIdentity = enumTypes;
                            if (!wrongIdentity.empty()) wrongIdentity[0].clangUsr = "different-enum-usr";
                            const auto mismatched = functions ? collectCppSignatureEnumDependencies(finalGraph, *functions, wrongIdentity)
                                : CppSignatureEnumResult{CppSignatureEnumFailure{"function fixture unavailable"}};
                            const auto* mismatchedDependencies = std::get_if<std::vector<CppSignatureEnumDependency>>(&mismatched);
                            bool rejectsMismatchedIdentity = mismatchedDependencies && !mismatchedDependencies->empty();
                            if (mismatchedDependencies)
                                for (const auto& dependency : *mismatchedDependencies)
                                    rejectsMismatchedIdentity &= dependency.status == CppSignatureEnumStatus::Unprojectable &&
                                        !dependency.candidateIndex;
                            ok &= expect(!enumTypes.empty() && enumTypes[0].clangUsr && rejectsMismatchedIdentity,
                                         "enum signature dependency rejects a different known Clang USR");
                            bool resultDependency = false, parameterDependency = false;
                            bool callbackResult = false, callbackParameter = false;
                            if (enumDependencies)
                                for (const auto& dependency : *enumDependencies)
                                {
                                    resultDependency |= !dependency.parameterIndex && dependency.indirectionDepth == 0 &&
                                        dependency.status == CppSignatureEnumStatus::Matched && dependency.candidateIndex == 0;
                                    parameterDependency |= dependency.parameterIndex == 0 && dependency.indirectionDepth == 1 &&
                                        dependency.status == CppSignatureEnumStatus::Matched && dependency.candidateIndex == 0;
                                    if (functions && dependency.functionIndex < functions->size() &&
                                        (*functions)[dependency.functionIndex].declaration.name == "apply_choice" &&
                                        dependency.parameterIndex == 0 && dependency.status == CppSignatureEnumStatus::Matched)
                                    {
                                        callbackResult |= dependency.indirectionDepth == 1;
                                        callbackParameter |= dependency.indirectionDepth == 2;
                                    }
                                }
                            nativesharp::frontend::CppFunctionDemand callDemand;
                            callDemand.lookupName = "echo_choice";
                            callDemand.lookupNamespaces = {""};
                            callDemand.visibleProjects = {1};
                            const auto callSelections = functions ? selectCppSemanticDemands(*functions,
                                std::span<const nativesharp::frontend::CppFunctionDemand>(&callDemand, 1), true)
                                : CppSemanticDemandSelection{CppSemanticDemandFailure{0, {0, 0, "fixture unavailable"}}};
                            const auto* selections = std::get_if<std::vector<CppSemanticDemandCandidates>>(&callSelections);
                            const auto selectedEnums = enumDependencies && selections
                                ? selectCppDemandedEnumCandidates(*enumDependencies, *selections, functions->size(), enumTypes.size())
                                : std::variant<std::vector<std::size_t>, CppSignatureEnumFailure>{CppSignatureEnumFailure{"fixture unavailable"}};
                            const auto* selectedEnumIndices = std::get_if<std::vector<std::size_t>>(&selectedEnums);
                            const bool demandedEnum = selectedEnumIndices && selectedEnumIndices->size() == 1 &&
                                selectedEnumIndices->front() == 0;
                            enumGroups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                                CppLanguageMode::C, "enum ChoiceType { Choice = 5 };\n")));
                            const bool conflicted = provideCppSemanticEnumConstants(finalGraph, enumGroups,
                                std::span<const nativesharp::frontend::CppFunctionDemand>(&demand, 1), states, enumError);
                            const bool conflictingEnums = conflicted && states.size() == 1 &&
                                !states[0].cppImportError.empty();
                            enumTypes.clear();
                            const bool deferredType = provideCppEnumTypes(finalGraph, enumGroups, enumTypes, enumError) &&
                                enumTypes.size() == 1 && !enumTypes[0].deferredImportError.empty();
                            enumGroups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                                CppLanguageMode::C, "extern int Choice;\n")));
                            const bool combined = provideCppSemanticValues(finalGraph, enumGroups,
                                std::span<const nativesharp::frontend::CppFunctionDemand>(&demand, 1), states, enumError);
                            ok &= expect(correct && correctType && resultDependency && parameterDependency &&
                                         callbackResult && callbackParameter && demandedEnum &&
                                         conflictingEnums && deferredType && combined && states.size() == 1 &&
                                         states[0].cppImportError.find("conflicts") != std::string::npos,
                                         "C enum provider preserves value and reports conflicting observations");
                        }
                        auto inventory = collectCppSemanticRecords(finalGraph, recordGroups);
                        const auto* entries = std::get_if<std::vector<CppSemanticRecordEntry>>(&inventory);
                        bool first = false, second = false, item = false, anonymous = false, forward = false;
                        bool recordModesPreserved = entries && !entries->empty();
                        std::optional<std::uint32_t> hashA, hashB, hashAgain;
                        if (entries)
                            for (const auto& entry : *entries)
                            {
                                const auto origin = std::find_if(recordGroups.begin(), recordGroups.end(),
                                    [&](const auto& group) { return group.projectFile == entry.projectFile; });
                                recordModesPreserved &= origin != recordGroups.end() &&
                                    entry.referenceIndex < origin->sessions.size() &&
                                    origin->sessions[entry.referenceIndex] &&
                                    entry.microsoftVolatileSemantics ==
                                        origin->sessions[entry.referenceIndex]->microsoftVolatileSemantics();
                                first |= entry.projectOrdinal == 1 && entry.referenceIndex == 0 &&
                                    entry.declaration.name == "Shared" && entry.declaration.sizeBits == 32 && entry.identity;
                                second |= entry.projectOrdinal == 1 && entry.referenceIndex == 1 &&
                                    entry.declaration.name == "Shared" && entry.declaration.sizeBits == 64 && entry.identity;
                                item |= entry.projectOrdinal == 2 && entry.declaration.name == "Item" && entry.identity;
                                anonymous |= entry.projectOrdinal == 2 && entry.declaration.name == "Hidden" && !entry.identity;
                                forward |= entry.projectOrdinal == 1 && entry.declaration.name == "Forward" &&
                                    !entry.declaration.complete && !entry.declaration.odrHash;
                                if (entry.projectOrdinal == 1 && entry.declaration.name == "Same")
                                {
                                    if (entry.referenceIndex == 0) hashA = entry.declaration.odrHash;
                                    if (entry.referenceIndex == 1) hashB = entry.declaration.odrHash;
                                    if (entry.referenceIndex == 2) hashAgain = entry.declaration.odrHash;
                                }
                            }
                        ok &= expect(first && second && item && anonymous,
                                     "record inventory retains conflicting definitions, origins and anonymous entries");
                        ok &= expect(recordModesPreserved,
                                     "record inventory preserves each originating session volatile mode");
                        ok &= expect(forward && hashA && hashB && hashAgain && *hashA != *hashB && *hashA == *hashAgain,
                                     "Clang ODR fingerprints distinguish equal-size fields and remain stable across sessions");
                        if (entries)
                        {
                            const auto grouped = groupCppSemanticRecords(*entries);
                            const auto originalRecord = std::find_if(entries->begin(), entries->end(),
                                [](const auto& entry) { return entry.identity && entry.declaration.clangUsr; });
                            bool differentRecordUsrConflicts = false;
                            if (originalRecord != entries->end())
                            {
                                auto changed = *entries;
                                changed.push_back(*originalRecord);
                                changed.back().declaration.clangUsr = "different-record-usr";
                                const auto changedGroups = groupCppSemanticRecords(changed);
                                for (const auto& group : changedGroups.groups)
                                    if (group.projectOrdinal == originalRecord->projectOrdinal &&
                                        group.identity.key() == originalRecord->identity->key())
                                        differentRecordUsrConflicts = group.status == CppRecordGroupStatus::Conflict;
                            }
                            ok &= expect(differentRecordUsrConflicts,
                                         "record grouping detects distinct known Clang USRs for one projected name");
                            bool sharedConflict = false, sameConflict = false, equalUnverified = false;
                            bool oneDefinition = false, forwardOnly = false, forwardAndDefinition = false, anonymousRetained = false;
                            for (const auto& group : grouped.groups)
                            {
                                sharedConflict |= group.identity.name() == "Shared" && group.projectOrdinal == 1 &&
                                    group.status == CppRecordGroupStatus::Conflict && group.inventoryIndices.size() == 3;
                                sameConflict |= group.identity.name() == "Same" && group.projectOrdinal == 1 &&
                                    group.status == CppRecordGroupStatus::Conflict && group.inventoryIndices.size() == 3;
                                equalUnverified |= group.identity.name() == "Equal" &&
                                    group.status == CppRecordGroupStatus::MultipleDefinitionsUnverified &&
                                    group.inventoryIndices.size() == 2;
                                oneDefinition |= group.identity.name() == "Item" && group.projectOrdinal == 2 &&
                                    group.status == CppRecordGroupStatus::OneDefinition;
                                forwardOnly |= group.identity.name() == "OnlyForward" &&
                                    group.status == CppRecordGroupStatus::ForwardOnly;
                                forwardAndDefinition |= group.identity.name() == "Forward" &&
                                    group.status == CppRecordGroupStatus::OneDefinition && group.inventoryIndices.size() == 2;
                            }
                            for (const auto index : grouped.unprojectedIndices)
                                anonymousRetained |= index < entries->size() && (*entries)[index].declaration.name == "Hidden";
                            ok &= expect(sharedConflict && sameConflict && equalUnverified && oneDefinition &&
                                         forwardOnly && forwardAndDefinition && anonymousRetained,
                                         "record groups retain origins, proven conflicts and unverified matches");
                            const auto comparison = compareCppRecordDefinitions(*entries, grouped, recordGroups);
                            const auto* compared = std::get_if<std::vector<CppRecordStructuralComparison>>(&comparison);
                            bool sameDifferent = false, sharedDifferent = false, equalEquivalent = false;
                            if (compared)
                                for (const auto& item : *compared)
                                {
                                    const auto& record = grouped.groups[item.groupIndex];
                                    sameDifferent |= record.identity.name() == "Same" && item.comparedDefinitions == 3 &&
                                        item.verdict == CppRecordStructuralVerdict::Different;
                                    sharedDifferent |= record.identity.name() == "Shared" && item.comparedDefinitions == 3 &&
                                        item.verdict == CppRecordStructuralVerdict::Different;
                                    equalEquivalent |= record.identity.name() == "Equal" && item.comparedDefinitions == 2 &&
                                        item.verdict == CppRecordStructuralVerdict::Equivalent;
                                }
                            ok &= expect(sameDifferent && sharedDifferent && equalEquivalent,
                                         "record-group comparison retains structural evidence for each owner and definition");
                            const auto refinement = refineCppRecordGroupsForPointerTransport(*entries, grouped, recordGroups);
                            const auto* refined = std::get_if<CppRecordGroups>(&refinement);
                            bool equalPointerOnly = false, conflictsPreserved = false;
                            if (refined)
                                for (const auto& record : refined->groups)
                                {
                                    equalPointerOnly |= record.identity.name() == "Equal" &&
                                        record.status == CppRecordGroupStatus::MultipleDefinitionsPointerCompatible;
                                    conflictsPreserved |= record.identity.name() == "Same" &&
                                        record.status == CppRecordGroupStatus::Conflict;
                                    if (record.identity.name() == "Equal" && record.inventoryIndices.size() == 2)
                                    {
                                        auto candidateEntries = *entries;
                                        for (const auto i : record.inventoryIndices)
                                            candidateEntries[i].declaration.location = CppSourcePoint{
                                                std::filesystem::path(argv[3]), 1, 1};
                                        const bool selected = selectCppRecordMemberDefinition(candidateEntries, record).has_value();
                                        auto& changed = candidateEntries[record.inventoryIndices.back()].declaration;
                                        if (!changed.fields.empty()) changed.fields[0].access = CppAccess::Private;
                                        const bool rejected = !selectCppRecordMemberDefinition(candidateEntries, record);
                                        ok &= expect(selected && rejected,
                                                     "repeated members require equal captured contracts at one physical source location");
                                        auto usrEntries = *entries;
                                        for (const auto i : record.inventoryIndices)
                                        {
                                            auto& declaration = usrEntries[i].declaration;
                                            declaration.location = CppSourcePoint{std::filesystem::path(argv[3]), 1, 1};
                                            if (!declaration.fields.empty()) declaration.fields[0].clangUsr = "shared-field-usr";
                                        }
                                        const bool sameUsrSelected = selectCppRecordMemberDefinition(usrEntries, record).has_value();
                                        bool conflictRejected = false, absentRetained = false;
                                        auto& usrFields = usrEntries[record.inventoryIndices.back()].declaration.fields;
                                        if (!usrFields.empty())
                                        {
                                            usrFields[0].clangUsr = "different-field-usr";
                                            conflictRejected = !selectCppRecordMemberDefinition(usrEntries, record);
                                            usrFields[0].clangUsr = std::nullopt;
                                            absentRetained = selectCppRecordMemberDefinition(usrEntries, record).has_value();
                                        }
                                        ok &= expect(sameUsrSelected && conflictRejected && absentRetained,
                                                     "repeated members reject conflicting known field USRs without inventing missing ones");
                                        auto transitiveEntries = usrEntries;
                                        auto transitiveGroup = record;
                                        auto& firstFields = transitiveEntries[transitiveGroup.inventoryIndices.front()].declaration.fields;
                                        bool transitiveRejected = false, emptyAccepted = false;
                                        if (!firstFields.empty())
                                        {
                                            firstFields[0].clangUsr.reset();
                                            transitiveEntries[transitiveGroup.inventoryIndices.back()].declaration.fields[0].clangUsr = "field-one";
                                            auto third = transitiveEntries[transitiveGroup.inventoryIndices.back()];
                                            third.declaration.fields[0].clangUsr = "field-two";
                                            transitiveGroup.inventoryIndices.push_back(transitiveEntries.size());
                                            transitiveEntries.push_back(std::move(third));
                                            transitiveRejected = !selectCppRecordMemberDefinition(transitiveEntries, transitiveGroup);
                                            transitiveEntries.back().declaration.fields[0].clangUsr = std::string{};
                                            emptyAccepted = selectCppRecordMemberDefinition(transitiveEntries, transitiveGroup).has_value();
                                        }
                                        ok &= expect(transitiveRejected && emptyAccepted,
                                                     "field USR conflicts remain visible after an initially missing observation");
                                        const auto nestedIdentityConflict = [&](bool isEnum) {
                                            auto nestedEntries = usrEntries;
                                            auto nestedGroup = record;
                                            for (const auto index : nestedGroup.inventoryIndices)
                                            {
                                                auto& type = nestedEntries[index].declaration.fields[0].type;
                                                if (isEnum)
                                                {
                                                    type.kind = CppTypeKind::Enumeration;
                                                    type.enumIdentity = CppEnumTypeIdentity{};
                                                    type.enumIdentity->name = "Nested";
                                                }
                                                else
                                                {
                                                    type.kind = CppTypeKind::Record;
                                                    type.recordIdentity = CppRecordTypeIdentity{};
                                                    type.recordIdentity->name = "Nested";
                                                }
                                            }
                                            auto& second = nestedEntries[nestedGroup.inventoryIndices.back()].declaration.fields[0].type;
                                            if (isEnum) second.enumIdentity->clangUsr = "nested-one";
                                            else second.recordIdentity->clangUsr = "nested-one";
                                            auto third = nestedEntries[nestedGroup.inventoryIndices.back()];
                                            auto& thirdType = third.declaration.fields[0].type;
                                            if (isEnum) thirdType.enumIdentity->clangUsr = "nested-two";
                                            else thirdType.recordIdentity->clangUsr = "nested-two";
                                            nestedGroup.inventoryIndices.push_back(nestedEntries.size());
                                            nestedEntries.push_back(std::move(third));
                                            return !selectCppRecordMemberDefinition(nestedEntries, nestedGroup);
                                        };
                                        ok &= expect(!usrEntries[record.inventoryIndices.front()].declaration.fields.empty() &&
                                                     nestedIdentityConflict(false) && nestedIdentityConflict(true),
                                                     "nested record and enum USR conflicts are detected across missing observations");
                                        auto signatureEntries = *entries;
                                        for (const auto i : record.inventoryIndices)
                                        {
                                            auto& declaration = signatureEntries[i].declaration;
                                            declaration.location = CppSourcePoint{std::filesystem::path(argv[3]), 1, 1};
                                            if (!declaration.fields.empty())
                                            {
                                                auto signature = std::make_shared<CppFunctionType>();
                                                signature->callingConvention = i == record.inventoryIndices.front() ? "cdecl" : "stdcall";
                                                declaration.fields[0].type.function = std::move(signature);
                                            }
                                        }
                                        ok &= expect(!selectCppRecordMemberDefinition(signatureEntries, record),
                                                     "repeated members reject differing captured function conventions");
                                    }
                                }
                            const auto refinedPrepared = refined ? prepareCppRecordObservation(*entries, *refined) :
                                CppRecordObservationResult{CppRecordObservationFailure{"refinement failed"}};
                            bool noRepeatedLayout = false, forwardIncomplete = false;
                            if (const auto* prepared = std::get_if<CppRecordObservationInputs>(&refinedPrepared))
                                for (std::size_t i = 0; i < prepared->types.size(); ++i)
                                {
                                    noRepeatedLayout |= refined->groups[prepared->groupByImportedType[i]].identity.name() == "Equal" &&
                                        !prepared->types[i].objectLayout && !prepared->types[i].isIncomplete;
                                    forwardIncomplete |= refined->groups[prepared->groupByImportedType[i]].identity.name() == "OnlyForward" &&
                                        !prepared->types[i].objectLayout && prepared->types[i].isIncomplete;
                                }
                            auto noHashEntries = *entries;
                            for (auto& entry : noHashEntries)
                                if (entry.declaration.name == "Equal" && entry.referenceIndex == 1) entry.declaration.odrHash.reset();
                            const auto noHashRefinement = refineCppRecordGroupsForPointerTransport(noHashEntries, grouped, recordGroups);
                            bool missingEvidenceRetained = false;
                            if (const auto* missing = std::get_if<CppRecordGroups>(&noHashRefinement))
                                for (const auto& record : missing->groups)
                                    missingEvidenceRetained |= record.identity.name() == "Equal" &&
                                        record.status == CppRecordGroupStatus::MultipleDefinitionsUnverified;
                            ok &= expect(equalPointerOnly && conflictsPreserved && noRepeatedLayout && forwardIncomplete && missingEvidenceRetained,
                                         "structural refinement admits only pointer transport with complete matching evidence");
                            auto invalidComparisonGroups = grouped;
                            invalidComparisonGroups.groups.push_back(grouped.groups.front());
                            ok &= expect(std::holds_alternative<CppRecordStructuralComparisonFailure>(
                                             compareCppRecordDefinitions(*entries, invalidComparisonGroups, recordGroups)),
                                         "record-group comparison rejects duplicate groups");
                            auto invalidEntries = *entries;
                            invalidEntries[grouped.groups.front().inventoryIndices.front()].referenceIndex = recordGroups[0].sessions.size() + 10;
                            ok &= expect(std::holds_alternative<CppRecordStructuralComparisonFailure>(
                                             compareCppRecordDefinitions(invalidEntries, grouped, recordGroups)),
                                         "record-group comparison rejects missing session references");
                            invalidEntries = *entries;
                            invalidEntries[grouped.groups.front().inventoryIndices.front()].targetTriple = "different-target";
                            ok &= expect(std::holds_alternative<CppRecordStructuralComparisonFailure>(
                                             compareCppRecordDefinitions(invalidEntries, grouped, recordGroups)),
                                         "record-group comparison rejects a target inconsistent with its session");
                            auto signatures = collectCppSemanticFunctions(finalGraph, recordGroups);
                            const auto* signatureEntries = std::get_if<std::vector<CppSemanticInventoryEntry>>(&signatures);
                            bool signatureUseReturn = false, signatureUseParameter = false, signatureNested = false;
                            bool signatureForward = false, signatureOtherOwner = false, signatureSpecialized = false;
                            if (signatureEntries)
                            {
                                bool projectedForward = false, projectedReadonly = false, projectedOther = false;
                                bool rejectedConflict = false, rejectedValue = false, rejectedSpecialization = false;
                                bool rejectedWrongOwner = false, rejectedDuplicate = false, rejectedUnverified = false;
                                for (const auto& entry : *signatureEntries)
                                {
                                    const auto& moduleName = finalGraph.projects()[entry.projectOrdinal].moduleName();
                                    const auto projection = projectCppSemanticFunctionWithRecords(
                                        entry.declaration, entry.projectOrdinal, moduleName, grouped);
                                    const auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projection);
                                    const bool failed = std::holds_alternative<CppSemanticFunctionFailure>(projection);
                                    if (entry.declaration.name == "store")
                                    {
                                        const auto identity = projectCppRecordTypeIdentity(*entry.declaration.returnType.element, moduleName);
                                        projectedForward = symbol && identity && symbol->declaredTypeKey == "type:" + identity->key() + "*" &&
                                            symbol->parameters.size() == 1 && symbol->parameters[0].typeKey == "type:" + identity->key() + "*";
                                        auto duplicate = grouped;
                                        for (const auto& group : grouped.groups)
                                            if (group.projectOrdinal == entry.projectOrdinal && group.identity.name() == "OnlyForward")
                                            { duplicate.groups.push_back(group); break; }
                                        rejectedDuplicate = std::holds_alternative<CppSemanticFunctionFailure>(
                                            projectCppSemanticFunctionWithRecords(entry.declaration, entry.projectOrdinal, moduleName, duplicate));
                                        auto unverified = grouped;
                                        for (auto& group : unverified.groups)
                                            if (group.projectOrdinal == entry.projectOrdinal && group.identity.name() == "OnlyForward")
                                                group.status = CppRecordGroupStatus::MultipleDefinitionsUnverified;
                                        rejectedUnverified = std::holds_alternative<CppSemanticFunctionFailure>(
                                            projectCppSemanticFunctionWithRecords(entry.declaration, entry.projectOrdinal, moduleName, unverified));
                                        auto wrongUsr = grouped;
                                        bool changedUsr = false;
                                        for (auto& group : wrongUsr.groups)
                                            if (group.projectOrdinal == entry.projectOrdinal && group.identity.name() == "OnlyForward" && group.clangUsr)
                                            { group.clangUsr = "different-record-usr"; changedUsr = true; }
                                        ok &= expect(changedUsr && std::holds_alternative<CppSemanticFunctionFailure>(
                                            projectCppSemanticFunctionWithRecords(entry.declaration, entry.projectOrdinal, moduleName, wrongUsr)),
                                            "function projection rejects a different record USR without a lookup");
                                        rejectedWrongOwner = std::holds_alternative<CppSemanticFunctionFailure>(
                                            projectCppSemanticFunctionWithRecords(entry.declaration, 2, moduleName, grouped));
                                    }
                                    if (entry.declaration.name == "observe")
                                        projectedReadonly = symbol && symbol->parameters.size() == 1 && symbol->parameters[0].pointerPointeeReadonly;
                                    if (entry.declaration.name == "own") projectedOther = symbol && symbol->projectOrdinal == 2;
                                    if (entry.declaration.name == "use") rejectedConflict = failed;
                                    if (entry.declaration.name == "value") rejectedValue = failed;
                                    if (entry.declaration.name == "specialized") rejectedSpecialization = failed;
                                }
                                ok &= expect(projectedForward && projectedReadonly && projectedOther && rejectedConflict &&
                                             rejectedValue && rejectedSpecialization && rejectedWrongOwner && rejectedDuplicate && rejectedUnverified,
                                             "isolated record pointer projection preserves identity and rejects unsafe or ambiguous records");
                                const auto dependencyResult = collectCppSignatureRecordDependencies(finalGraph, *signatureEntries, grouped);
                                auto wrongRecordUsr = grouped;
                                bool changedRecordUsr = false;
                                for (auto& group : wrongRecordUsr.groups)
                                    if (group.projectOrdinal == 1 && group.identity.name() == "OnlyForward" && group.clangUsr)
                                    { group.clangUsr = "different-record-usr"; changedRecordUsr = true; }
                                const auto wrongUsrResult = collectCppSignatureRecordDependencies(
                                    finalGraph, *signatureEntries, wrongRecordUsr);
                                bool rejectedWrongRecordUsr = false;
                                if (const auto* wrong = std::get_if<std::vector<CppSignatureRecordDependency>>(&wrongUsrResult))
                                    for (const auto& dependency : *wrong)
                                        rejectedWrongRecordUsr |= (*signatureEntries)[dependency.functionIndex].declaration.name == "store" &&
                                            dependency.status == CppSignatureRecordStatus::Unprojectable && !dependency.groupIndex;
                                ok &= expect(changedRecordUsr && rejectedWrongRecordUsr,
                                             "record signature association rejects a different known Clang USR");
                                if (const auto* dependencies = std::get_if<std::vector<CppSignatureRecordDependency>>(&dependencyResult))
                                    for (const auto& dependency : *dependencies)
                                    {
                                        const auto& function = (*signatureEntries)[dependency.functionIndex];
                                        const CppRecordGroup* group = dependency.groupIndex ? &grouped.groups[*dependency.groupIndex] : nullptr;
                                        const bool conflict = group && group->projectOrdinal == 1 &&
                                            group->identity.name() == "Shared" && group->status == CppRecordGroupStatus::Conflict;
                                        if (function.declaration.name == "use" && dependency.indirectionDepth == 1 && conflict)
                                        {
                                            signatureUseReturn |= !dependency.parameterIndex;
                                            signatureUseParameter |= dependency.parameterIndex == std::optional<std::size_t>(0);
                                        }
                                        signatureNested |= function.declaration.name == "nestedPtr" && dependency.indirectionDepth == 2 && conflict;
                                        signatureForward |= function.declaration.name == "store" && group &&
                                            group->status == CppRecordGroupStatus::ForwardOnly;
                                        signatureOtherOwner |= function.declaration.name == "own" && group &&
                                            group->projectOrdinal == 2 && group->identity.name() == "Shared";
                                        signatureSpecialized |= function.declaration.name == "specialized" &&
                                            dependency.status == CppSignatureRecordStatus::Unprojectable && !dependency.groupIndex;
                                    }
                                auto invalidSignatureGroups = grouped;
                                invalidSignatureGroups.groups.push_back(grouped.groups[0]);
                                ok &= expect(std::holds_alternative<CppSignatureRecordFailure>(
                                                 collectCppSignatureRecordDependencies(finalGraph, *signatureEntries, invalidSignatureGroups)),
                                             "signature record association rejects duplicate groups");
                                auto absentGroups = grouped;
                                std::erase_if(absentGroups.groups, [](const auto& item) {
                                    return item.projectOrdinal == 1 && item.identity.name() == "Shared";
                                });
                                const auto missingResult = collectCppSignatureRecordDependencies(finalGraph, *signatureEntries, absentGroups);
                                bool missingReported = false;
                                if (const auto* missing = std::get_if<std::vector<CppSignatureRecordDependency>>(&missingResult))
                                    for (const auto& dependency : *missing)
                                        missingReported |= (*signatureEntries)[dependency.functionIndex].declaration.name == "use" &&
                                            dependency.status == CppSignatureRecordStatus::Missing && !dependency.groupIndex;
                                auto malformedSignatures = *signatureEntries;
                                for (auto& entry : malformedSignatures)
                                    if (entry.declaration.name == "use") entry.declaration.returnType.element.reset();
                                ok &= expect(missingReported && std::holds_alternative<CppSignatureRecordFailure>(
                                                 collectCppSignatureRecordDependencies(finalGraph, malformedSignatures, grouped)),
                                             "signature association reports missing records and rejects incomplete type descriptions");
                            }
                            ok &= expect(signatureUseReturn && signatureUseParameter && signatureNested && signatureForward &&
                                         signatureOtherOwner && signatureSpecialized,
                                         "signature records retain function position, owning project, conflicts and unprojectable types");
                            const auto prepared = prepareCppRecordObservation(*entries, grouped);
                            const auto* inputs = std::get_if<CppRecordObservationInputs>(&prepared);
                            bool preparedConflict = false, preparedOther = false, preparedForwardNoLayout = false;
                            bool preparedUnverifiedNoLayout = false, originsMatch = inputs &&
                                inputs->types.size() == inputs->groupByImportedType.size();
                            if (inputs)
                                for (std::size_t i = 0; i < inputs->types.size(); ++i)
                                {
                                    const auto groupIndex = inputs->groupByImportedType[i];
                                    if (groupIndex >= grouped.groups.size()) { originsMatch = false; continue; }
                                    const auto& group = grouped.groups[groupIndex];
                                    originsMatch &= inputs->types[i].projectOrdinal == group.projectOrdinal &&
                                        inputs->types[i].identity.key() == group.identity.key() &&
                                        !inputs->types[i].sourcePath.empty();
                                    preparedConflict |= group.identity.name() == "Shared" &&
                                        group.status == CppRecordGroupStatus::Conflict && group.inventoryIndices.size() == 3 &&
                                        !inputs->types[i].objectLayout;
                                    preparedOther |= group.identity.name() == "Item" && group.projectOrdinal == 2;
                                    preparedForwardNoLayout |= group.identity.name() == "OnlyForward" && !inputs->types[i].objectLayout;
                                    preparedUnverifiedNoLayout |= group.identity.name() == "Equal" && !inputs->types[i].objectLayout;
                                }
                            ok &= expect(inputs && originsMatch && preparedConflict && preparedOther &&
                                         preparedForwardNoLayout && preparedUnverifiedNoLayout,
                                         "record observation inputs retain origins and do not invent uncertain layouts");
                            auto invalidGroups = grouped;
                            invalidGroups.groups[0].inventoryIndices.push_back(entries->size());
                            ok &= expect(std::holds_alternative<CppRecordObservationFailure>(
                                             prepareCppRecordObservation(*entries, invalidGroups)),
                                         "record observation rejects invalid inventory indices");
                            auto changedTarget = *entries;
                            for (auto& entry : changedTarget)
                                if (entry.declaration.name == "Equal" && entry.referenceIndex == 1)
                                    entry.targetTriple = "different-target";
                            const auto changedGroups = groupCppSemanticRecords(changedTarget);
                            bool targetConflict = false;
                            for (const auto& group : changedGroups.groups)
                                targetConflict |= group.identity.name() == "Equal" && group.status == CppRecordGroupStatus::Conflict;
                            ok &= expect(targetConflict, "record grouping detects target mismatches");
                        }
                        recordGroups[1].sessions[0]->close();
                        auto closed = collectCppSemanticRecords(finalGraph, recordGroups);
                        const auto* closedError = std::get_if<CppSemanticRecordFailure>(&closed);
                        ok &= expect(closedError && closedError->projectFile == project->projectFile() &&
                                     closedError->referenceIndex == 0,
                                     "record inventory reports the owner and index of a closed session");
                        if (entries)
                        {
                            const auto grouped = groupCppSemanticRecords(*entries);
                            recordGroups[0].sessions[2]->close();
                            const auto partlyClosed = compareCppRecordDefinitions(*entries, grouped, recordGroups);
                            bool differenceWins = false;
                            if (const auto* results = std::get_if<std::vector<CppRecordStructuralComparison>>(&partlyClosed))
                                for (const auto& item : *results)
                                    differenceWins |= grouped.groups[item.groupIndex].identity.name() == "Same" &&
                                        item.verdict == CppRecordStructuralVerdict::Different;
                            recordGroups[0].sessions[1]->close();
                            const auto closedAliasInventory = collectCppSemanticTypeAliases(finalGraph, recordGroups);
                            const auto closedEnumInventory = collectCppSemanticEnums(finalGraph, recordGroups);
                            const auto* closedAliasFailure = std::get_if<CppSemanticInventoryFailure>(&closedAliasInventory);
                            const auto* closedEnumFailure = std::get_if<CppSemanticInventoryFailure>(&closedEnumInventory);
                            ok &= expect(closedAliasFailure && closedAliasFailure->referenceIndex == 1 &&
                                         closedEnumFailure && closedEnumFailure->referenceIndex == 1,
                                         "alias and enum inventories report the unavailable reference");
                            const auto closedComparison = compareCppRecordDefinitions(*entries, grouped, recordGroups);
                            bool unavailable = false;
                            if (const auto* results = std::get_if<std::vector<CppRecordStructuralComparison>>(&closedComparison))
                                for (const auto& item : *results)
                                    unavailable |= grouped.groups[item.groupIndex].identity.name() == "Equal" &&
                                        item.verdict == CppRecordStructuralVerdict::Unavailable;
                            const auto closedRefinement = refineCppRecordGroupsForPointerTransport(*entries, grouped, recordGroups);
                            bool closedNotPromoted = false;
                            if (const auto* result = std::get_if<CppRecordGroups>(&closedRefinement))
                                for (const auto& record : result->groups)
                                    closedNotPromoted |= record.identity.name() == "Equal" &&
                                        record.status == CppRecordGroupStatus::MultipleDefinitionsUnverified;
                            ok &= expect(differenceWins && unavailable && closedNotPromoted,
                                         "record comparison preserves differences and reports closed-session uncertainty");
                        }
                        recordGroups[0].projectFile = fixture / "missing-owner.nsproj";
                        auto missingOwner = collectCppSemanticRecords(finalGraph, recordGroups);
                        ok &= expect(std::holds_alternative<CppSemanticRecordFailure>(missingOwner),
                                     "record inventory rejects a missing project owner");
                    }
                    auto namespaceInventory = collectCppNamespaces(finalGraph, groups);
                    const auto* namespaceEntries = std::get_if<std::vector<CppNamespaceInventoryEntry>>(
                        &namespaceInventory);
                    bool otherNamespace = false, featureNamespace = false, anonymousChild = false;
                    if (namespaceEntries)
                        for (const auto& entry : *namespaceEntries)
                        {
                            otherNamespace |= entry.declaration.qualifiedName == "other" &&
                                entry.projectOrdinal == 1 && entry.referenceIndex == 2 &&
                                entry.moduleName == otherProject->moduleName();
                            featureNamespace |= entry.declaration.qualifiedName == "feature" &&
                                entry.projectOrdinal == 2 && entry.referenceIndex == 0 &&
                                entry.moduleName == project->moduleName();
                            anonymousChild |= entry.projectOrdinal == 1 && entry.declaration.insideAnonymous;
                        }
                    ok &= expect(namespaceEntries && otherNamespace && featureNamespace && anonymousChild,
                                 "namespace inventory preserves project, reference and anonymous provenance");
                    if (namespaceEntries)
                    {
                        const auto visible = visibleCppNamespaces(*namespaceEntries);
                        const auto* names = std::get_if<std::vector<std::pair<std::size_t, std::string>>>(&visible);
                        const auto contains = [&](std::size_t owner, std::string_view name) {
                            return names && std::any_of(names->begin(), names->end(), [&](const auto& item) {
                                return item.first == owner && item.second == name;
                            });
                        };
                        ok &= expect(names && names->size() == 3 && contains(1, "other") &&
                                     contains(1, "other.deep") && contains(2, "feature"),
                                     "visible namespaces deduplicate reopened names and exclude anonymous descendants");
                    }
                    auto inventory = collectCppSemanticFunctions(finalGraph, groups);
                    const auto* entries = std::get_if<std::vector<CppSemanticInventoryEntry>>(&inventory);
                    bool otherFound = false, rejectedPointer = false, cppFound = false;
                    if (entries)
                    {
                        for (const auto& entry : *entries)
                        {
                            const auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(
                                &entry.projection);
                            if (entry.declaration.name == "other_value")
                                otherFound |= symbol && symbol->projectOrdinal == 1 &&
                                    symbol->moduleName == otherProject->moduleName() &&
                                    entry.projectFile == otherProject->projectFile() && entry.referenceIndex == 0;
                            if (entry.declaration.name == "pointer_value")
                                rejectedPointer = std::holds_alternative<CppSemanticFunctionFailure>(entry.projection);
                            if (entry.declaration.name == "value")
                                cppFound = symbol && symbol->projectOrdinal == 2 &&
                                    symbol->moduleName == project->moduleName() && symbol->namespaceName == "feature";
                        }
                    }
                    ok &= expect(entries && entries->size() == 6 && otherFound && rejectedPointer && cppFound,
                                 "inventory uses final graph owners and retains unsupported declarations");
                    if (entries)
                    {
                        auto grouped = groupCppSemanticFunctions(*entries);
                        const auto* groupedValue = std::get_if<CppSemanticFunctionGroups>(&grouped);
                        bool duplicateRetained = false, separateOwner = false;
                        if (groupedValue)
                        {
                            for (const auto& group : groupedValue->groups)
                            {
                                if (group.symbol.name == "other_value" && group.symbol.projectOrdinal == 1)
                                    duplicateRetained = group.inventoryIndices.size() == 2 &&
                                        (*entries)[group.inventoryIndices[0]].referenceIndex == 0 &&
                                        (*entries)[group.inventoryIndices[1]].referenceIndex == 1;
                                else if (group.symbol.name == "other_value" && group.symbol.projectOrdinal == 2)
                                    separateOwner = group.inventoryIndices.size() == 1;
                            }
                        }
                        ok &= expect(groupedValue && groupedValue->groups.size() == 4 &&
                                     groupedValue->unprojectedIndices.size() == 1 && duplicateRetained && separateOwner,
                                     "matching declarations group while origins and overloads remain distinct");
                        if (groupedValue && !groupedValue->groups.empty())
                        {
                            auto conflictingDll = *entries;
                            conflictingDll.push_back((*entries)[groupedValue->groups[0].inventoryIndices[0]]);
                            conflictingDll.back().declaration.dllImport = !conflictingDll.back().declaration.dllImport;
                            const auto conflict = selectCppSemanticFunctions(conflictingDll,
                                conflictingDll.back().projectOrdinal, conflictingDll.back().declaration.qualifiedName);
                            ok &= expect(std::holds_alternative<CppSemanticFunctionGroupFailure>(conflict),
                                         "function redeclarations with conflicting DLL import attributes are rejected");
                            auto conflictingUsr = *entries;
                            const auto& original = (*entries)[groupedValue->groups[0].inventoryIndices[0]];
                            std::vector<CppSemanticInventoryEntry> sameNameUsr{original, original};
                            sameNameUsr[0].declaration.clangUsr = std::nullopt;
                            sameNameUsr[1].declaration.clangUsr = "known-function-usr";
                            const auto knownFunctionUsr = groupCppSemanticFunctions(sameNameUsr);
                            const auto* knownGroups = std::get_if<CppSemanticFunctionGroups>(&knownFunctionUsr);
                            const bool knownRetained = knownGroups && knownGroups->groups.size() == 1 &&
                                knownGroups->groups[0].symbol.cppClangUsr == std::optional<std::string>{"known-function-usr"};
                            sameNameUsr[0].declaration.clangUsr = "different-function-usr";
                            const auto conflictingFunctionUsr = groupCppSemanticFunctions(sameNameUsr);
                            ok &= expect(knownRetained &&
                                         std::holds_alternative<CppSemanticFunctionGroupFailure>(conflictingFunctionUsr),
                                         "function grouping retains known USR and rejects same-name contradictions");
                            conflictingUsr.push_back(original);
                            conflictingUsr.back().declaration.qualifiedName += "_other_name";
                            conflictingUsr.back().declaration.externalSymbol = "different_external_symbol";
                            const auto byUsr = selectCppSemanticFunctions(
                                conflictingUsr, original.projectOrdinal, original.declaration.qualifiedName);
                            ok &= expect(original.declaration.clangUsr &&
                                         std::holds_alternative<CppSemanticFunctionGroupFailure>(byUsr),
                                         "same Clang USR with a different external symbol conflicts across names");
                            nativesharp::frontend::CppFunctionDemand usrDemand;
                            const auto separator = original.declaration.qualifiedName.rfind("::");
                            usrDemand.lookupName = separator == std::string::npos ? original.declaration.qualifiedName :
                                original.declaration.qualifiedName.substr(separator + 2);
                            std::string usrNamespace = separator == std::string::npos ? "" :
                                original.declaration.qualifiedName.substr(0, separator);
                            for (std::size_t p = 0; (p = usrNamespace.find("::", p)) != std::string::npos; ++p)
                                usrNamespace.replace(p, 2, ".");
                            usrDemand.lookupNamespaces = {usrNamespace};
                            usrDemand.visibleProjects = {original.projectOrdinal};
                            const std::vector usrDemands{usrDemand};
                            const auto indexedUsr = selectCppSemanticDemands(conflictingUsr, usrDemands, true);
                            const auto* usrMatches = std::get_if<std::vector<CppSemanticDemandCandidates>>(&indexedUsr);
                            ok &= expect(usrMatches && usrMatches->size() == 1 && usrMatches->front().deferredConflict,
                                         "indexed lookup retains the conflict for a different name with the same USR");
                            auto distinctAliases = *entries;
                            distinctAliases.push_back(original);
                            distinctAliases.back().declaration.qualifiedName += "_alias";
                            distinctAliases.back().declaration.clangUsr = "independent-usr";
                            ok &= expect(std::holds_alternative<CppSemanticFunctionGroups>(
                                             groupCppSemanticFunctions(distinctAliases)),
                                         "different USRs may remain compatible aliases of one external symbol");
                        }
                        const auto absent = selectCppSemanticFunctions(*entries, 1, "missing_function");
                        const auto* absentValue = std::get_if<CppSemanticFunctionGroups>(&absent);
                        ok &= expect(absentValue && absentValue->groups.empty() &&
                                     absentValue->unprojectedIndices.empty(),
                                     "lookup of an absent name returns no candidates");
                        const auto otherOwner = selectCppSemanticFunctions(*entries, 2, "other_value");
                        const auto* ownerValue = std::get_if<CppSemanticFunctionGroups>(&otherOwner);
                        ok &= expect(ownerValue && ownerValue->groups.size() == 1 &&
                                     ownerValue->groups[0].symbol.projectOrdinal == 2,
                                     "lookup does not mix owners of the same name");
                        bool everyLookupMatches = true;
                        for (const auto& entry : *entries)
                        {
                            const auto& qualified = entry.declaration.qualifiedName;
                            const auto separator = qualified.rfind("::");
                            nativesharp::frontend::CppFunctionDemand demand;
                            demand.lookupName = separator == std::string::npos ? qualified : qualified.substr(separator + 2);
                            std::string ns = separator == std::string::npos ? "" : qualified.substr(0, separator);
                            for (std::size_t p = 0; (p = ns.find("::", p)) != std::string::npos; ++p)
                                ns.replace(p, 2, ".");
                            demand.lookupNamespaces = {ns};
                            demand.visibleProjects = {entry.projectOrdinal};
                            const std::vector demands{demand};
                            const auto reference = selectCppSemanticFunctions(*entries, entry.projectOrdinal, qualified);
                            const auto indexed = selectCppSemanticDemands(*entries, demands);
                            const auto* referenceGroups = std::get_if<CppSemanticFunctionGroups>(&reference);
                            const auto* indexedGroups = std::get_if<std::vector<CppSemanticDemandCandidates>>(&indexed);
                            if (!referenceGroups || !indexedGroups || indexedGroups->size() != 1 ||
                                (*indexedGroups)[0].candidates.unprojectedIndices != referenceGroups->unprojectedIndices ||
                                (*indexedGroups)[0].candidates.groups.size() != referenceGroups->groups.size())
                            { everyLookupMatches = false; continue; }
                            for (std::size_t i = 0; i < referenceGroups->groups.size(); ++i)
                                everyLookupMatches &= (*indexedGroups)[0].candidates.groups[i].inventoryIndices ==
                                    referenceGroups->groups[i].inventoryIndices &&
                                    (*indexedGroups)[0].candidates.groups[i].symbol.mangledName ==
                                        referenceGroups->groups[i].symbol.mangledName;
                        }
                        ok &= expect(everyLookupMatches, "indexed lookup agrees for every real inventory declaration");
                    }
                    groups[1].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::Cpp, "namespace feature { struct Opaque; int value(Opaque*); } namespace top { namespace inner { int f(); } }\n")));
                    auto overloadedInventory = collectCppSemanticFunctions(finalGraph, groups);
                    const auto* overloadedEntries = std::get_if<std::vector<CppSemanticInventoryEntry>>(
                        &overloadedInventory);
                    if (overloadedEntries)
                    {
                        auto selected = selectCppSemanticFunctions(*overloadedEntries, 2, "feature::value");
                        const auto* selectedValue = std::get_if<CppSemanticFunctionGroups>(&selected);
                        ok &= expect(selectedValue && selectedValue->groups.size() == 2 &&
                                     selectedValue->unprojectedIndices.size() == 1 &&
                                     (*overloadedEntries)[selectedValue->unprojectedIndices[0]].declaration.name == "value",
                                     "lookup retains representable and unprojected overloads with original indices");
                        nativesharp::frontend::CppFunctionDemand demand;
                        demand.lookupName = "value";
                        demand.lookupNamespaces = {"feature"};
                        demand.visibleProjects = {2};
                        auto inaccessible = demand;
                        inaccessible.visibleProjects = {1};
                        auto nested = demand;
                        nested.lookupName = "f";
                        nested.lookupNamespaces = {"top.inner"};
                        const std::vector demands{demand, demand, inaccessible, nested};
                        const auto selection = selectCppSemanticDemands(*overloadedEntries, demands);
                        const auto* matches = std::get_if<std::vector<CppSemanticDemandCandidates>>(&selection);
                        bool agreesWithReference = selectedValue && matches && !matches->empty() &&
                            (*matches)[0].candidates.unprojectedIndices == selectedValue->unprojectedIndices &&
                            (*matches)[0].candidates.groups.size() == selectedValue->groups.size();
                        if (agreesWithReference)
                            for (std::size_t i = 0; i < selectedValue->groups.size(); ++i)
                                agreesWithReference &= (*matches)[0].candidates.groups[i].inventoryIndices ==
                                    selectedValue->groups[i].inventoryIndices &&
                                    (*matches)[0].candidates.groups[i].symbol.mangledName ==
                                        selectedValue->groups[i].symbol.mangledName;
                        ok &= expect(matches && matches->size() == 3 && (*matches)[0].demandIndex == 0 &&
                                     (*matches)[1].demandIndex == 1 && (*matches)[0].projectOrdinal == 2 &&
                                     (*matches)[0].namespaceName == "feature" &&
                                     (*matches)[0].candidates.groups.size() == 2 &&
                                     (*matches)[0].candidates.unprojectedIndices.size() == 1 &&
                                     (*matches)[2].demandIndex == 3 && (*matches)[2].namespaceName == "top.inner" &&
                                     (*matches)[2].candidates.groups.size() == 1 &&
                                     (*matches)[2].candidates.groups[0].symbol.name == "f",
                                     "demand selection preserves overloads, visibility and demand provenance");
                        ok &= expect(agreesWithReference, "indexed demand selection agrees with the reference lookup");
                    }
                    else ok &= expect(false, "overloaded inventory is available");
                    groups[1].sessions.pop_back();
                    groups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::C, "int pointer_value(int p);\n")));
                    auto pointerConflict = collectCppSemanticFunctions(finalGraph, groups);
                    const auto* pointerConflictEntries = std::get_if<std::vector<CppSemanticInventoryEntry>>(
                        &pointerConflict);
                    ok &= expect(pointerConflictEntries && std::holds_alternative<CppSemanticFunctionGroupFailure>(
                                     groupCppSemanticFunctions(*pointerConflictEntries)),
                                 "conflict is detected even when one C signature is unprojected");
                    if (pointerConflictEntries)
                    {
                        const auto related = selectCppSemanticFunctions(*pointerConflictEntries, 1, "pointer_value");
                        const auto* conflict = std::get_if<CppSemanticFunctionGroupFailure>(&related);
                        const auto unrelated = selectCppSemanticFunctions(*pointerConflictEntries, 2, "feature::value");
                        const auto* unrelatedValue = std::get_if<CppSemanticFunctionGroups>(&unrelated);
                        ok &= expect(conflict && conflict->firstIndex < pointerConflictEntries->size() &&
                                     conflict->conflictingIndex < pointerConflictEntries->size() &&
                                     (*pointerConflictEntries)[conflict->firstIndex].declaration.name == "pointer_value" &&
                                     unrelatedValue && unrelatedValue->groups.size() == 2,
                                     "lookup reports relevant conflict by original indices without blocking other names");
                        nativesharp::frontend::CppFunctionDemand demand;
                        demand.lookupName = "pointer_value";
                        demand.lookupNamespaces = {""};
                        demand.visibleProjects = {1};
                        const std::vector demands{demand};
                        const auto demandSelection = selectCppSemanticDemands(*pointerConflictEntries, demands);
                        const auto* demandFailure = std::get_if<CppSemanticDemandFailure>(&demandSelection);
                        ok &= expect(conflict && demandFailure && demandFailure->demandIndex == 0 &&
                                     demandFailure->conflict.firstIndex == conflict->firstIndex &&
                                     demandFailure->conflict.conflictingIndex == conflict->conflictingIndex,
                                     "demand selection reports the originating conflict without losing indices");
                    }
                    groups[0].sessions.pop_back();
                    groups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::Cpp, "namespace alpha { extern \"C\" int alias(int); }\n")));
                    groups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::Cpp, "namespace beta { extern \"C\" double alias(int); }\n")));
                    auto aliasInventory = collectCppSemanticFunctions(finalGraph, groups);
                    const auto* aliasEntries = std::get_if<std::vector<CppSemanticInventoryEntry>>(&aliasInventory);
                    if (aliasEntries)
                    {
                        const auto aliasResult = selectCppSemanticFunctions(*aliasEntries, 1, "alpha::alias");
                        const auto* aliasFailure = std::get_if<CppSemanticFunctionGroupFailure>(&aliasResult);
                        ok &= expect(aliasFailure && aliasFailure->firstIndex < aliasEntries->size() &&
                                     aliasFailure->conflictingIndex < aliasEntries->size() &&
                                     (*aliasEntries)[aliasFailure->conflictingIndex].declaration.qualifiedName ==
                                         "beta::alias",
                                     "selection checks incompatible aliases of the same C symbol");
                        nativesharp::frontend::CppFunctionDemand demand;
                        demand.lookupName = "alias";
                        demand.lookupNamespaces = {"alpha"};
                        demand.visibleProjects = {1};
                        const std::vector demands{demand};
                        const auto indexed = selectCppSemanticDemands(*aliasEntries, demands);
                        const auto* indexedFailure = std::get_if<CppSemanticDemandFailure>(&indexed);
                        ok &= expect(aliasFailure && indexedFailure &&
                                     indexedFailure->conflict.firstIndex == aliasFailure->firstIndex &&
                                     indexedFailure->conflict.conflictingIndex == aliasFailure->conflictingIndex,
                                     "indexed selection preserves conflicts between names sharing a C symbol");
                        const auto delayed = selectCppSemanticDemands(*aliasEntries, demands, true);
                        const auto* delayedMatches = std::get_if<std::vector<CppSemanticDemandCandidates>>(&delayed);
                        ok &= expect(aliasFailure && delayedMatches && delayedMatches->size() == 1 &&
                                     delayedMatches->front().deferredConflict &&
                                     delayedMatches->front().deferredConflict->firstIndex == aliasFailure->firstIndex &&
                                     delayedMatches->front().deferredConflict->conflictingIndex == aliasFailure->conflictingIndex,
                                     "deferred selection preserves conflicts between aliases of one C symbol");
                    }
                    else ok &= expect(false, "alias inventory is available");
                    groups[0].sessions.pop_back();
                    groups[0].sessions.pop_back();
                    groups[0].sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::C, "double other_value(void);\n")));
                    auto conflictInventory = collectCppSemanticFunctions(finalGraph, groups);
                    const auto* conflictEntries = std::get_if<std::vector<CppSemanticInventoryEntry>>(
                        &conflictInventory);
                    ok &= expect(conflictEntries && std::holds_alternative<CppSemanticFunctionGroupFailure>(
                                     groupCppSemanticFunctions(*conflictEntries)),
                                 "separate Clang sessions with conflicting C signatures are not grouped");
                    std::vector<Project> duplicateOwners{*otherProject, *otherProject, *project};
                    ProjectGraph ambiguousGraph(project->projectFile(), std::move(duplicateOwners));
                    ok &= expect(std::holds_alternative<CppSemanticInventoryFailure>(
                                     collectCppSemanticFunctions(ambiguousGraph, groups)),
                                 "inventory rejects ambiguous project owners");
                    ok &= expect(std::holds_alternative<CppNamespaceInventoryFailure>(
                                     collectCppNamespaces(ambiguousGraph, groups)),
                                 "namespace inventory rejects ambiguous project owners");
                    std::vector<CppProjectSessions> invalidGroup(1);
                    invalidGroup[0].projectFile = fixture / "missing-owner.nsproj";
                    ok &= expect(std::holds_alternative<CppSemanticInventoryFailure>(
                                     collectCppSemanticFunctions(finalGraph, invalidGroup)),
                                 "inventory rejects missing project owners");
                    ok &= expect(std::holds_alternative<CppNamespaceInventoryFailure>(
                                     collectCppNamespaces(finalGraph, invalidGroup)),
                                 "namespace inventory rejects missing project owners");
                    invalidGroup[0].projectFile = project->projectFile();
                    invalidGroup[0].sessions.push_back(nullptr);
                    const auto nullResult = collectCppSemanticFunctions(finalGraph, invalidGroup);
                    const auto* nullFailure = std::get_if<CppSemanticInventoryFailure>(&nullResult);
                    ok &= expect(nullFailure && nullFailure->referenceIndex == 0 &&
                                 nullFailure->message.find("null") != std::string::npos,
                                 "inventory identifies a null session");
                    const auto nullNamespaceResult = collectCppNamespaces(finalGraph, invalidGroup);
                    const auto* nullNamespaceFailure = std::get_if<CppNamespaceInventoryFailure>(&nullNamespaceResult);
                    ok &= expect(nullNamespaceFailure && nullNamespaceFailure->referenceIndex == 0,
                                 "namespace inventory identifies a null session");
                    if (groups[0].sessions[0]) groups[0].sessions[0]->close();
                    ok &= expect(std::holds_alternative<CppSemanticInventoryFailure>(
                                     collectCppSemanticFunctions(finalGraph, groups)),
                                 "inventory rejects unavailable sessions without partial results");
                    ok &= expect(std::holds_alternative<CppNamespaceInventoryFailure>(
                                     collectCppNamespaces(finalGraph, groups)),
                                 "namespace inventory rejects unavailable sessions without partial results");
                }
                {
                    std::ostringstream driverOutput, driverErrors;
                    nativesharp::driver::DriverExecutionScope scope(driverOutput, driverErrors);
                    auto* previousScope = nativesharp::driver::activeExecutionScope;
                    nativesharp::driver::activeExecutionScope = &scope;
                    RejectUnexpectedOpen missingEnvironmentFactory;
                    scope.cppSessionFactory = &missingEnvironmentFactory;
                    CompilationInputManifest ordinaryManifest(combinedGraph, {});
                    nativesharp::frontend::ProjectCompilation ordinaryCompilation;
                    error.clear();
                    const bool ordinaryPrepared = nativesharp::driver::configurePrecompiledModules(
                        ordinaryManifest, ordinaryCompilation, error, false);
                    ok &= expect(!ordinaryPrepared && missingEnvironmentFactory.calls == 0 &&
                                 error.find("--cpp-clang-root") != std::string::npos &&
                                 ordinaryCompilation.cppProjectSessions().empty(),
                                 "driver rejects C/C++ references without configuration before opening a session");
                    if (plainProject)
                    {
                        ProjectGraph plainGraph(plainProject->projectFile(), std::vector<Project>{*plainProject});
                        CompilationInputManifest plainManifest(plainGraph, {});
                        nativesharp::frontend::ProjectCompilation plainCompilation;
                        RejectUnexpectedOpen unexpectedFactory;
                        scope.cppSessionEnvironment = environment;
                        scope.cppSessionFactory = &unexpectedFactory;
                        error.clear();
                        const bool plainPrepared = nativesharp::driver::configurePrecompiledModules(
                            plainManifest, plainCompilation, error, false);
                        ok &= expect(plainPrepared && unexpectedFactory.calls == 0 &&
                                     plainCompilation.cppProjectSessions().empty(),
                                     "driver with C/C++ support and no references opens no Clang session");
                    }
                    scope.cppSessionEnvironment = environment;
                    scope.cppSessionFactory = nullptr;
                    CompilationInputManifest noFactoryManifest(combinedGraph, {});
                    nativesharp::frontend::ProjectCompilation noFactoryCompilation;
                    error.clear();
                    ok &= expect(!nativesharp::driver::configurePrecompiledModules(
                                     noFactoryManifest, noFactoryCompilation, error, false) &&
                                 error.find("requires a session factory") != std::string::npos,
                                 "driver rejects an explicit environment without a factory");
                    FailWithDiagnostic failingFactory;
                    scope.cppSessionFactory = &failingFactory;
                    CompilationInputManifest badManifest(combinedGraph, {});
                    nativesharp::frontend::ProjectCompilation badCompilation;
                    error.clear();
                    const bool badPrepared = nativesharp::driver::configurePrecompiledModules(
                        badManifest, badCompilation, error, false);
                    const auto& badDiagnostics = badCompilation.diagnostics().diagnostics();
                    ok &= expect(!badPrepared && badDiagnostics.size() == 1 &&
                                 badDiagnostics[0].message().find("broken.hpp:4:9") != std::string::npos &&
                                 badDiagnostics[0].notes().size() == 1 &&
                                 badDiagnostics[0].notes()[0] == "related declaration",
                                 "driver preserves Clang failure locations and notes");
                    scope.cppSessionFactory = &factory;
                    CompilationInputManifest driverManifest(combinedGraph, {});
                    nativesharp::frontend::ProjectCompilation driverCompilation;
                    error.clear();
                    const bool prepared = nativesharp::driver::configurePrecompiledModules(
                        driverManifest, driverCompilation, error, false);
                    if (!prepared) std::cerr << "Driver preparation failed: " << error << '\n';
                    ok &= expect(prepared && driverCompilation.cppProjectSessions().size() == 2,
                                 "driver prepares real project sessions before compilation");
                    if (prepared)
                    {
                        error.clear();
                        ok &= expect(!driverCompilation.setCppImportedFunctions({}, error) &&
                                     error.find("function provider") != std::string::npos,
                                     "driver installs the C/C++ function provider with real sessions");
                    }
                    scope.cppSessionEnvironment->resourceDirectory /= "nonexistent-resource";
                    CompilationInputManifest failedManifest(combinedGraph, {});
                    nativesharp::frontend::ProjectCompilation failedCompilation;
                    error.clear();
                    ok &= expect(!nativesharp::driver::configurePrecompiledModules(
                                     failedManifest, failedCompilation, error, false) &&
                                 error.find("C/C++ session failed") != std::string::npos &&
                                 error.find("reference 0") != std::string::npos,
                                 "driver reports the project and reference when Clang preparation fails");
                    nativesharp::driver::activeExecutionScope = previousScope;
                }
                auto graphOpened = openCppGraphSessions(combinedGraph, environment, factory, {});
                auto* graphSessions = std::get_if<CppGraphSessions>(&graphOpened);
                ok &= expect(graphSessions && graphSessions->projects.size() == 2 &&
                             graphSessions->projects[0].projectFile == otherProject->projectFile() &&
                             graphSessions->projects[1].projectFile == project->projectFile() &&
                             graphSessions->projects[0].sessions.size() == 1 &&
                             graphSessions->projects[1].sessions.size() == 2,
                             "graph session groups retain project and reference order");
                if (graphSessions)
                {
                    nativesharp::frontend::ProjectCompilation graphOwner;
                    ok &= expect(graphOwner.setCppProjectSessions(std::move(graphSessions->projects), error) &&
                                 graphOwner.cppProjectSessions().size() == 2,
                                 "graph sessions transfer directly into frontend ownership");
                }
                FailThirdOpen failingFactory(factory);
                auto graphFailed = openCppGraphSessions(combinedGraph, environment, failingFactory, {});
                const auto* graphFailure = std::get_if<CppGraphSessionFailure>(&graphFailed);
                ok &= expect(graphFailure && graphFailure->projectFile == project->projectFile() &&
                             graphFailure->projectFailure.referenceIndex == 1 &&
                             graphFailure->projectFailure.failure.phase == "test",
                             "graph failure identifies the exact project and reference without partial output");
                CppCancellationToken graphCancelled;
                graphCancelled.cancel();
                auto graphCancelledResult = openCppGraphSessions(combinedGraph, environment, factory, graphCancelled);
                const auto* cancelledGraphFailure = std::get_if<CppGraphSessionFailure>(&graphCancelledResult);
                ok &= expect(cancelledGraphFailure && cancelledGraphFailure->projectFile == otherProject->projectFile() &&
                             cancelledGraphFailure->projectFailure.failure.phase == "cancel",
                             "graph session creation propagates cancellation at the owning project");
                auto first = openCppProjectSessions(*project, environment, factory, {});
                auto second = openCppProjectSessions(*otherProject, environment, factory, {});
                auto* firstGroup = std::get_if<CppProjectSessions>(&first);
                auto* secondGroup = std::get_if<CppProjectSessions>(&second);
                ok &= expect(firstGroup && secondGroup, "sessions for separate projects open");
                if (firstGroup && secondGroup)
                {
                    std::vector<CppProjectSessions> multiple;
                    multiple.push_back(std::move(*firstGroup));
                    multiple.push_back(std::move(*secondGroup));
                    nativesharp::frontend::ProjectCompilation multiOwner;
                    ok &= expect(multiOwner.setCppProjectSessions(std::move(multiple), error) &&
                                 multiOwner.cppProjectSessions().size() == 2 &&
                                 multiOwner.cppProjectSessions()[0].projectFile !=
                                     multiOwner.cppProjectSessions()[1].projectFile &&
                                 multiOwner.cppProjectSessions()[1].sessions[0]->optionsSnapshot().macros.size() == 1,
                                 "frontend keeps sessions and options separate for two projects");
                }
            }
            if (ownedSessions)
            {
                nativesharp::frontend::ProjectCompilation owner;
                std::vector<CppProjectSessions> groups;
                groups.push_back(std::move(*ownedSessions));
                error.clear();
                ok &= expect(owner.setCppProjectSessions(std::move(groups), error) && error.empty() &&
                             owner.cppProjectSessions().size() == 1 &&
                             owner.cppProjectSessions()[0].sessions.size() == 2 &&
                             owner.cppProjectSessions()[0].sessions[0]->topLevelDeclarationCount().has_value(),
                             "frontend retains real sessions for their project");
                std::vector<CppProjectSessions> duplicateGroups(2);
                duplicateGroups[0].projectFile = project->projectFile();
                duplicateGroups[1].projectFile = project->projectFile();
                ok &= expect(!owner.setCppProjectSessions(std::move(duplicateGroups), error) &&
                             owner.cppProjectSessions().size() == 1,
                             "duplicate owners are rejected without replacing existing sessions");
                std::vector<CppProjectSessions> missingOwner(1);
                ok &= expect(!owner.setCppProjectSessions(std::move(missingOwner), error) &&
                             owner.cppProjectSessions().size() == 1,
                             "missing owner is rejected without replacing existing sessions");
                std::vector<CppProjectSessions> nullSession(1);
                nullSession[0].projectFile = project->projectFile();
                nullSession[0].sessions.push_back(nullptr);
                ok &= expect(!owner.setCppProjectSessions(std::move(nullSession), error) &&
                             owner.cppProjectSessions().size() == 1,
                             "null session is rejected without replacing existing sessions");
                owner.compileProjectSyntax(projectPath, error);
                ok &= expect(!owner.setCppProjectSessions({}, error) && !error.empty() &&
                             owner.cppProjectSessions().size() == 1,
                             "project session sets cannot be replaced after compilation begins");
            }
        }
        ProjectGraphLoader graphLoader;
        auto graph = graphLoader.load(projectPath, error);
        if (!graph) std::cerr << "Graph load failed: " << error << '\n';
        ok &= expect(graph && graph->rootProject() && graph->rootProject()->cppLibraryReferences().size() == 2,
                     "project graph retains C/C++ references");
        if (graph && graph->rootProject() && graph->rootProject()->cppLibraryReferences().size() == 2)
        {
            const auto owner = graph->rootProject()->projectFile();
            const std::vector<LibraryReference> additional{{fixture / "record-symbols.lib", {}, {}, {}}};
            ok &= expect(graph->addLibraryReferences(owner, additional, error) && graph->rootProject() &&
                         graph->rootProject()->cppLibraryReferences().size() == 2,
                         "reconstructing a graph project preserves C/C++ references");
            const auto header = graph->rootProject()->cppLibraryReferences()[0].headerPath;
            const auto library = graph->rootProject()->cppLibraryReferences()[0].libraryPath;
            CompilationInputManifest manifest(std::move(*graph), {});
            const auto inputs = manifest.inputFiles();
            ok &= expect(std::find(inputs.begin(), inputs.end(), header) != inputs.end() &&
                         std::find(inputs.begin(), inputs.end(), library) != inputs.end(),
                         "manifest includes the header and binary library");
            LibraryMetadataRepository metadata;
            error.clear();
            ok &= expect(!manifest.calculateIdentity(metadata, error) &&
                         error.find("not yet connected") != std::string::npos,
                         "unconnected references cannot reuse a compilation cache");
        }
        for (const auto& [name, expected] : std::vector<std::pair<const char*, const char*>>{
                 {"BadLanguage.nsproj", "Language must be"},
                 {"MissingHeader.nsproj", "header does not exist"},
                 {"MissingLibrary.nsproj", "library"},
                 {"RuntimeWithoutLibrary.nsproj", "Runtime requires"}})
        {
            error.clear();
            const auto invalidProject = loader.load(fixture / "invalid" / name, error);
            if (invalidProject || error.find(expected) == std::string::npos)
                std::cerr << "Unexpected result for " << name << ": " << error << '\n';
            ok &= expect(!invalidProject && error.find(expected) != std::string::npos,
                         "invalid C/C++ reference is rejected for the expected reason");
        }
        nativesharp::frontend::ProjectCompilation compilation;
        error.clear();
        ok &= expect(!compilation.compileProjectSyntax(projectPath, error) &&
                     error.find("not yet connected") != std::string::npos,
                     "project compilation rejects references until integration is complete");
        if (project && project->cppLibraryReferences().size() == 2)
        {
            std::vector<std::filesystem::path> libraries;
            std::vector<std::filesystem::path> runtimes;
            const bool collected = nativesharp::driver::appendValidatedLibraryReferences(
                compilation, libraries, runtimes, error, nullptr);
            const bool collectedAgain = nativesharp::driver::appendValidatedLibraryReferences(
                compilation, libraries, runtimes, error, nullptr);
            ok &= expect(collected && collectedAgain && libraries.size() == 1 &&
                         libraries[0] == project->cppLibraryReferences()[0].libraryPath && runtimes.empty(),
                         "link inputs include each C/C++ archive once and omit header-only references");
        }
    }
    {
        const auto projectPath = std::filesystem::path("A:/nsc-devel/nativesharp-interop/pruebas-proyecto/CppProject.nsproj");
        nativesharp::compilation::ProjectGraphLoader loader;
        std::string error;
        auto graph = loader.load(projectPath, error);
        const CppProjectSessionEnvironment environment{
            "x86_64-pc-windows-msvc", argv[3], argv[1], argv[2], {}};
        auto opened = graph ? openCppGraphSessions(*graph, environment, factory, {}) :
            CppGraphSessionsResult{CppGraphSessionFailure{}};
        auto* sessions = std::get_if<CppGraphSessions>(&opened);
        nativesharp::frontend::ProjectCompilation compilation;
        const bool configured = sessions && compilation.setCppProjectSessions(std::move(sessions->projects), error) &&
            compilation.setCppNamespaceProvider([](const auto& graph, auto sessions, auto& output, auto& why) {
                auto inventory = collectCppNamespaces(graph, sessions);
                if (!std::holds_alternative<std::vector<CppNamespaceInventoryEntry>>(inventory)) {
                    why = "namespace inventory failed"; return false;
                }
                auto names = visibleCppNamespaces(std::get<std::vector<CppNamespaceInventoryEntry>>(inventory));
                if (!std::holds_alternative<std::vector<std::pair<std::size_t, std::string>>>(names)) {
                    why = "namespace projection failed"; return false;
                }
                output = std::move(std::get<std::vector<std::pair<std::size_t, std::string>>>(names));
                return true;
            }, error) && compilation.setCppFunctionProvider(
                [](const auto& graph, auto sessions, auto, auto demands, auto& projected, auto& deferred, auto& why) {
                    return provideCppSemanticFunctions(graph, sessions, demands, projected, deferred, why);
                }, error) && compilation.setCppGlobalProvider(
                [](const auto& graph, auto sessions, auto demands, auto& states, auto& why) {
                    return provideCppSemanticGlobals(graph, sessions, demands, states, why);
                }, error);
        const bool ready = configured && compilation.compileProjectOopMir(projectPath, error);
        if (!ready) std::cerr << "project reference MIR failed: " << error << '\n';
        bool imported = false;
        if (ready && compilation.oopMirModel())
            for (const auto& function : compilation.oopMirModel()->functions())
                imported |= function.isImportedC && function.mangledName == "project_sum";
        ok &= expect(ready && imported,
                     "actual project reference with real Clang sessions reaches MIR");
        if (graph)
        {
            auto missing = openCppGraphSessions(*graph, environment, factory, {});
            auto* available = std::get_if<CppGraphSessions>(&missing);
            nativesharp::frontend::ProjectCompilation noProviders;
            const bool installed = available && noProviders.setCppProjectSessions(std::move(available->projects), error);
            ok &= expect(installed && !noProviders.compileProjectSyntax(projectPath, error) &&
                         error.find("not yet connected") != std::string::npos,
                         "project references require both semantic providers");
            CppProjectSessions wrongGroup;
            wrongGroup.projectFile = std::filesystem::absolute(projectPath);
            auto wrongOptions = options(argv[1], argv[2], argv[3], CppLanguageMode::C, "int other(void);\n");
            wrongGroup.sessions.push_back(open(factory, std::move(wrongOptions)));
            std::vector<CppProjectSessions> wrongGroups;
            wrongGroups.push_back(std::move(wrongGroup));
            nativesharp::frontend::ProjectCompilation wrongHeader;
            const bool wrongConfigured = wrongHeader.setCppProjectSessions(std::move(wrongGroups), error) &&
                wrongHeader.setCppNamespaceProvider([](const auto&, auto, auto&, auto&) { return true; }, error) &&
                wrongHeader.setCppFunctionProvider([](const auto&, auto, auto, auto, auto&, auto&, auto&) {
                    return true;
                }, error);
            ok &= expect(wrongConfigured && !wrongHeader.compileProjectSyntax(projectPath, error) &&
                         error.find("not yet connected") != std::string::npos,
                         "project reference rejects a session for a different input");
        }
    }
    auto c = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::C,
                                  "#include <stddef.h>\nint bridge_probe(size_t n, ...);\n"
                                  "int c_add(int left, int right); int c_invoke(int (*cb)(int), int value);\n"
                                  "int ptr_read(int* p); void restrict_use(int *restrict p);\n"
                                  "int ptr_const(const int* p); int ptr_volatile(volatile int* p); int** ptr_nested(int** p);\n"
                                  "volatile int* ptr_volatile_result(void); void ptr_volatile_nested(volatile int** p);\n"
                                  "void* ptr_void(void* p); int ptr_nested_const(const int** p);\n"
                                  "int c_no_prototype(); int c_void_prototype(void);\n"
                                  "int c_refined(); int c_refined(int); int c_reverse(int); int c_reverse();\n"
                                  "static inline int inline_add(int x) { return x + 1; }\n"
                                  "static inline int inline_composed(int x) { return inline_add(x) + 2; }\n"
                                  "static inline int inline_counter(void) { static int count; return ++count; }\n"
                                  "int bridge_probe(size_t n, ...);\n"
                                  "struct Pair { char a; int b; }; struct OuterPair { struct Pair nested; struct Pair array[2][3]; }; struct CDevice { volatile int counter; }; struct Incomplete;\n"
                                  "#define DECLARE_MACRO_RECORD struct MacroRecord { int x; };\nDECLARE_MACRO_RECORD\n"
                                  "typedef unsigned int Count; typedef Count* CountPtr; "
                                  "typedef struct Pair Pair; typedef struct Pair PairAlias; typedef struct Pair PairMatrix[2][3]; typedef const struct Pair* PairPtr; typedef const int* ReadOnlyPtr;\n"
                                  "typedef struct { int value; } AnonStruct; typedef union { int integer; float real; } AnonUnion;\n"
                                  "typedef int IncompleteArray[]; typedef int FixedArray[7]; typedef int Matrix[2][3];\n"
                                  "typedef int (*Callback)(const int*, ...); typedef int (*OldCallback)(); typedef int *restrict RestrictedPtr; typedef int *restrict *PtrToRestricted;\n"
                                  "enum Color { CMinus = -3, CPlus = 5 }; typedef enum Color ColorAlias; typedef enum Color ColorMatrix[2][3]; typedef enum Color (*ColorCallback)(enum Color); typedef enum Color (*NestedCallback)(enum Color (*)(enum Color)); enum Color color_echo(enum Color value); int apply_color(enum Color (*cb)(enum Color)); struct Paint { enum Color shade; const enum Color* view; enum Color colors[2][3]; }; enum { CAnonymous = 9 }; typedef enum { ModeA = 1 } Mode;\n"
                                  "extern int c_global; extern int c_global; static const int c_local = 3; "
                                  "extern volatile int c_volatile; extern const volatile float c_float;\n"
                                  "union CValue { int integer; float real; }; struct Flags { unsigned a:3; unsigned b:5; }; "
                                  "struct Later; struct Later { int value; }; struct MatrixHolder { int matrix[2][3]; }; struct Flexible { int count; int values[]; };\n"));
    std::optional<std::vector<CppFunctionDeclaration>> cFunctions;
    std::optional<std::vector<CppRecordDeclaration>> cRecords;
    std::optional<std::vector<CppGlobalDeclaration>> cGlobals;
    std::optional<std::vector<CppTypeAliasDeclaration>> cAliases;
    std::optional<std::vector<CppEnumDeclaration>> cEnums;
    ok &= expect(static_cast<bool>(c), "valid C translation unit opens");
    if (c)
    {
        const std::vector<CppCInlineIRExport> inlineExports{
            {"inline_add", "nsc_inline_add"}, {"inline_composed", "nsc_inline_composed"},
            {"inline_counter", "nsc_inline_counter"}};
        const auto inlineIr = c->emitCStaticInlineIR(inlineExports);
        ok &= expect(inlineIr && inlineIr->find("@nsc_inline_add") != std::string::npos &&
                     inlineIr->find("@nsc_inline_composed") != std::string::npos &&
                     inlineIr->find("@nsc_inline_counter") != std::string::npos &&
                     inlineIr->find("inline_counter.count") != std::string::npos &&
                     !c->emitCStaticInlineIR({{"missing", "nsc_missing"}}) &&
                     !c->emitCStaticInlineIR({{"inline_add", "same"}, {"inline_counter", "same"}}),
                     "Clang emits selected C static inline functions and internal state into one IR module");
        const auto namespaces = c->namespacesSnapshot();
        ok &= expect(namespaces && namespaces->empty(), "C translation unit has no C++ namespaces");
        ok &= expect(c->state() == CppSessionState::Ready, "C session is ready");
        cAliases = c->typeAliasesSnapshot();
        const auto aliasByName = [&](std::string_view name) -> const CppTypeAliasDeclaration* {
            if (!cAliases) return nullptr;
            for (const auto& alias : *cAliases) if (alias.name == name) return &alias;
            return nullptr;
        };
        const auto* countAlias = aliasByName("Count");
        const auto* pointerAlias = aliasByName("CountPtr");
        const auto* recordAlias = aliasByName("PairAlias");
        const auto* recordPointerAlias = aliasByName("PairPtr");
        const auto* readonlyAlias = aliasByName("ReadOnlyPtr");
        const auto* incompleteArrayAlias = aliasByName("IncompleteArray");
        const auto* fixedArrayAlias = aliasByName("FixedArray");
        const auto* matrixAlias = aliasByName("Matrix");
        const auto* colorAlias = aliasByName("ColorAlias");
        const auto* modeAlias = aliasByName("Mode");
        const auto* callbackAlias = aliasByName("Callback");
        const auto* oldCallbackAlias = aliasByName("OldCallback");
        const auto* restrictedAlias = aliasByName("RestrictedPtr");
        ok &= expect(restrictedAlias && restrictedAlias->underlyingType.kind == CppTypeKind::Pointer &&
                     restrictedAlias->underlyingType.isRestrict && restrictedAlias->underlyingType.element &&
                     !restrictedAlias->underlyingType.element->isRestrict,
                     "C typedef snapshot retains restrict at the qualified pointer level");
        ok &= expect(countAlias && countAlias->underlyingType.kind == CppTypeKind::UnsignedInteger &&
                     countAlias->underlyingType.sizeBits == 32 && countAlias->location &&
                     pointerAlias && pointerAlias->underlyingType.kind == CppTypeKind::Pointer &&
                     pointerAlias->underlyingType.element &&
                     pointerAlias->underlyingType.element->kind == CppTypeKind::UnsignedInteger &&
                     recordAlias && recordAlias->underlyingType.kind == CppTypeKind::Record &&
                     readonlyAlias && readonlyAlias->underlyingType.element &&
                     readonlyAlias->underlyingType.element->isConst,
                     "C typedef snapshot preserves scalar, pointer, record and const underlying types");
        const auto projectedRecord = recordAlias ? nativesharp::frontend::cppAliasRecordType(recordAlias->underlyingType)
                                                 : std::nullopt;
        const auto projectedRecordPointer = recordPointerAlias
            ? nativesharp::frontend::cppAliasRecordType(recordPointerAlias->underlyingType) : std::nullopt;
        CppType nestedRecordType;
        nestedRecordType.kind = CppTypeKind::Record;
        nestedRecordType.recordIdentity = CppRecordTypeIdentity{"Inner", {"N"}, {"Outer"}, false, false, std::nullopt};
        const auto nestedRecord = nativesharp::frontend::cppAliasRecordType(nestedRecordType);
        ok &= expect(projectedRecord && projectedRecord->name == "Pair" && projectedRecord->pointerDepth == 0 &&
                     projectedRecordPointer && projectedRecordPointer->name == "Pair" &&
                     projectedRecordPointer->pointerDepth == 1 && projectedRecordPointer->pointeeConst &&
                     nestedRecord && nestedRecord->name == "N.Outer.Inner" &&
                     nestedRecord->canonicalName == "N.Outer+Inner",
                     "record typedef projection preserves identity, pointer and pointee const");
        ok &= expect(incompleteArrayAlias && incompleteArrayAlias->underlyingType.kind == CppTypeKind::Array &&
                     incompleteArrayAlias->underlyingType.isIncompleteArray &&
                     incompleteArrayAlias->underlyingType.element &&
                     incompleteArrayAlias->underlyingType.element->kind == CppTypeKind::SignedInteger &&
                     !incompleteArrayAlias->underlyingType.arrayCount &&
                     !incompleteArrayAlias->underlyingType.sizeBits &&
                     fixedArrayAlias && fixedArrayAlias->underlyingType.element &&
                     !fixedArrayAlias->underlyingType.isIncompleteArray &&
                     fixedArrayAlias->underlyingType.arrayCount == 7 &&
                     fixedArrayAlias->underlyingType.sizeBits == 224,
                     "C array typedef snapshot preserves element and distinguishes incomplete bounds");
        const auto matrixType = matrixAlias ? nativesharp::frontend::cppAliasArrayType(matrixAlias->underlyingType)
                                            : std::nullopt;
        ok &= expect(matrixType && matrixType->dimensions.size() == 2 &&
                     matrixType->dimensions[0] == 2 && matrixType->dimensions[1] == 3 &&
                     matrixType->element.token == nativesharp::lexer::TokenKind::KwInt &&
                     incompleteArrayAlias && !nativesharp::frontend::cppAliasArrayType(incompleteArrayAlias->underlyingType),
                     "array typedef preserves C dimensions and rejects an unknown bound");
        const auto projectedEnum = colorAlias ? nativesharp::frontend::cppAliasEnumType(colorAlias->underlyingType)
                                              : std::nullopt;
        ok &= expect(projectedEnum && projectedEnum->name == "Color" &&
                     projectedEnum->base.token == nativesharp::lexer::TokenKind::KwInt,
                     "enum typedef preserves its identity and Clang integer representation");
        const auto* callbackType = callbackAlias && callbackAlias->underlyingType.element
            ? callbackAlias->underlyingType.element->function.get() : nullptr;
        const auto* oldCallbackType = oldCallbackAlias && oldCallbackAlias->underlyingType.element
            ? oldCallbackAlias->underlyingType.element->function.get() : nullptr;
        ok &= expect(callbackType && callbackType->hasPrototype && callbackType->variadic &&
                     callbackType->callingConvention == "cdecl" &&
                     callbackType->returnType.kind == CppTypeKind::SignedInteger &&
                     callbackType->parameterTypes.size() == 1 &&
                     callbackType->parameterTypes[0].kind == CppTypeKind::Pointer &&
                     callbackType->parameterTypes[0].element && callbackType->parameterTypes[0].element->isConst &&
                     oldCallbackType && !oldCallbackType->hasPrototype,
                     "C function pointer typedef retains signature and distinguishes old-style declarations");
        const auto projectedCallback = callbackAlias
            ? nativesharp::frontend::cppAliasFunctionType(callbackAlias->underlyingType) : std::nullopt;
        const auto projectedOldCallback = oldCallbackAlias
            ? nativesharp::frontend::cppAliasFunctionType(oldCallbackAlias->underlyingType) : std::nullopt;
        ok &= expect(projectedCallback && projectedCallback->variadic &&
                     projectedCallback->callingConvention == nativesharp::ast::FunctionCallingConventionSyntaxKind::C &&
                     projectedCallback->result.name == "int" && projectedCallback->parameters.size() == 1 &&
                     projectedCallback->parameters[0].name == "int" &&
                     projectedCallback->parameters[0].pointerDepth == 1 &&
                     projectedCallback->parameters[0].pointeeConst && !projectedOldCallback,
                     "C function pointer projection preserves signature and rejects unspecified parameters");
        cEnums = c->enumsSnapshot();
        const CppEnumDeclaration* color = nullptr;
        const CppEnumDeclaration* anonymous = nullptr;
        const CppEnumDeclaration* mode = nullptr;
        if (cEnums)
            for (const auto& enumeration : *cEnums)
            {
                if (enumeration.name == "Color") color = &enumeration;
                if (enumeration.name == "Mode") mode = &enumeration;
                if (enumeration.name.empty() && !enumeration.enumerators.empty() &&
                    enumeration.enumerators.front().name == "CAnonymous") anonymous = &enumeration;
            }
        const auto mappedMode = modeAlias ? nativesharp::frontend::cppAliasEnumType(modeAlias->underlyingType)
                                          : std::nullopt;
        ok &= expect(mode && mode->complete && mode->enumerators.size() == 1 &&
                     mode->enumerators[0].name == "ModeA" && mappedMode && mappedMode->name == "Mode",
                     "anonymous C enum typedef preserves declaration and type identity");
        ok &= expect(color && color->complete && color->underlyingType && color->location &&
                     color->sizeBits == 32 && color->alignmentBits == 32 && !color->isCxx &&
                     color->enumerators.size() == 2 && color->enumerators[0].name == "CMinus" &&
                     color->enumerators[0].decimalValue == "-3" && color->enumerators[0].location &&
                     color->enumerators[0].valueType.kind == CppTypeKind::SignedInteger &&
                     color->enumerators[0].valueType.sizeBits == 32 &&
                     color->enumerators[1].decimalValue == "5" && anonymous &&
                     anonymous->enumerators[0].decimalValue == "9",
                     "C enum snapshot preserves named and anonymous enumerators and signed values");
        if (color)
        {
            CppSemanticEnumEntry enumEntry{{}, 0, 0, "x86_64-pc-windows-msvc", *color};
            std::vector<CppSemanticEnumEntry> enumEntries{enumEntry, enumEntry};
            enumEntries[0].declaration.clangUsr.reset();
            enumEntries[0].declaration.enumerators[0].clangUsr.reset();
            enumEntries[1].declaration.clangUsr = "enum-usr";
            enumEntries[1].declaration.enumerators[0].clangUsr = "value-usr";
            const nativesharp::compilation::TypeIdentity colorIdentity("Semantic", {}, {}, "Color");
            const std::vector<std::string> enumSeeds{colorIdentity.key()};
            const auto enumCatalog = captureCppCEnumCatalog(enumEntries, 0, "Semantic", enumSeeds);
            const auto* enumSnapshots = std::get_if<std::vector<CppCEnumSnapshot>>(&enumCatalog);
            ok &= expect(enumSnapshots && enumSnapshots->size() == 1 &&
                         (*enumSnapshots)[0].declaration.clangUsr == "enum-usr" &&
                         (*enumSnapshots)[0].declaration.enumerators[0].clangUsr == "value-usr" &&
                         (*enumSnapshots)[0].declaration.enumerators[0].decimalValue == "-3",
                         "C enum catalogue reconciles identities and preserves exact signed values");
            {
                auto forward = enumEntry;
                forward.declaration.complete = false;
                forward.declaration.underlyingType.reset();
                forward.declaration.sizeBits.reset();
                forward.declaration.alignmentBits.reset();
                forward.declaration.enumerators.clear();
                forward.declaration.clangUsr = "enum-usr";
                auto definition = enumEntry;
                definition.declaration.clangUsr.reset();
                const std::vector<CppSemanticEnumEntry> withForward{forward, definition};
                const auto captured = captureCppCEnumCatalog(withForward, 0, "Semantic", enumSeeds);
                const auto* snapshots = std::get_if<std::vector<CppCEnumSnapshot>>(&captured);
                ok &= expect(snapshots && snapshots->size() == 1 &&
                             (*snapshots)[0].declaration.clangUsr == "enum-usr" &&
                             (*snapshots)[0].declaration.enumerators.size() == definition.declaration.enumerators.size(),
                             "C enum catalogue reconciles a forward declaration with its definition");
                auto conflicting = forward;
                conflicting.declaration.clangUsr = "other-enum-usr";
                const std::vector<CppSemanticEnumEntry> conflict{forward, definition, conflicting};
                ok &= expect(std::holds_alternative<CppCEnumSnapshotFailure>(
                                 captureCppCEnumCatalog(conflict, 0, "Semantic", enumSeeds)),
                             "C enum catalogue rejects conflicting forward identities");
                conflicting = forward;
                conflicting.targetTriple = "another-target";
                const std::vector<CppSemanticEnumEntry> targetConflict{forward, definition, conflicting};
                ok &= expect(std::holds_alternative<CppCEnumSnapshotFailure>(
                                 captureCppCEnumCatalog(targetConflict, 0, "Semantic", enumSeeds)),
                             "C enum catalogue rejects conflicting forward targets");
                const std::vector<CppSemanticEnumEntry> onlyForward{forward};
                ok &= expect(std::holds_alternative<CppCEnumSnapshotFailure>(
                                 captureCppCEnumCatalog(onlyForward, 0, "Semantic", enumSeeds)),
                             "C enum catalogue requires a complete definition");
            }
            if (enumSnapshots)
            {
                std::vector<std::uint8_t> bytes;
                std::vector<CppCRecordSnapshot> decodedRecords;
                std::vector<CppCEnumSnapshot> decodedEnums;
                std::string codecError;
                const bool encoded = metadata_detail::encodeCTypeCatalog({}, *enumSnapshots, bytes, codecError);
                const bool decoded = encoded && metadata_detail::decodeCTypeCatalog(bytes, decodedRecords, decodedEnums, codecError);
                ok &= expect(decoded && decodedRecords.empty() && decodedEnums.size() == 1 &&
                             decodedEnums[0].declaration.underlyingType &&
                             decodedEnums[0].declaration.enumerators[0].decimalValue == "-3" &&
                             decodedEnums[0].declaration.enumerators[0].clangUsr == "value-usr",
                             "C type codec preserves enum type, signed value and identity");
                if (!bytes.empty()) bytes.pop_back();
                ok &= expect(!metadata_detail::decodeCTypeCatalog(bytes, decodedRecords, decodedEnums, codecError) &&
                             decodedEnums.size() == 1,
                             "C type codec rejects truncated enum without changing outputs");
            }
            enumEntries[1].declaration.enumerators[0].decimalValue = "-4";
            ok &= expect(std::holds_alternative<CppCEnumSnapshotFailure>(
                             captureCppCEnumCatalog(enumEntries, 0, "Semantic", enumSeeds)),
                         "C enum catalogue rejects conflicting values");
            const auto negative = projectCppEnumConstant(enumEntry, 0);
            const auto positive = projectCppEnumConstant(enumEntry, 1);
            const auto* negativeValue = std::get_if<CppSemanticEnumConstant>(&negative);
            const auto* positiveValue = std::get_if<CppSemanticEnumConstant>(&positive);
            auto withUsr = enumEntry;
            withUsr.declaration.enumerators[0].clangUsr = "test:enumerator";
            const auto identified = projectCppEnumConstant(withUsr, 0);
            const auto* identifiedValue = std::get_if<CppSemanticEnumConstant>(&identified);
            auto withoutUsr = enumEntry;
            withoutUsr.declaration.enumerators[0].clangUsr.reset();
            const auto unidentified = projectCppEnumConstant(withoutUsr, 0);
            const auto* unidentifiedValue = std::get_if<CppSemanticEnumConstant>(&unidentified);
            ok &= expect(identifiedValue && identifiedValue->clangUsr == "test:enumerator" &&
                         unidentifiedValue && !unidentifiedValue->clangUsr,
                         "C enum projection preserves known and absent enumerator identities");
            auto overflow = enumEntry;
            overflow.declaration.enumerators[0].decimalValue = "2147483648";
            auto cppEnum = enumEntry;
            cppEnum.declaration.isCxx = true;
            ok &= expect(negativeValue && negativeValue->typeKey == "builtin:int" &&
                         negativeValue->bits == 0xfffffffdULL && positiveValue && positiveValue->bits == 5 &&
                         std::holds_alternative<CppSemanticEnumConstantFailure>(projectCppEnumConstant(overflow, 0)) &&
                         std::holds_alternative<CppSemanticEnumConstantFailure>(projectCppEnumConstant(cppEnum, 0)),
                         "C enum projection preserves exact signed bits and rejects overflow or C++ enum assumptions");
        }
        auto count = c->topLevelDeclarationCount();
        ok &= expect(count && *count > 0, "C AST is available after parsing");
        cFunctions = c->freeFunctionsSnapshot();
        const auto* probe = cFunctions ? functionByName(*cFunctions, "bridge_probe") : nullptr;
        ok &= expect(probe && probe->externalSymbol == "bridge_probe" && probe->variadic &&
                     probe->parameters.size() == 1 && !probe->parameters[0].canonicalTypeSpelling.empty() &&
                     probe->location.has_value() && !probe->canonicalFunctionType.empty() &&
                     probe->callingConvention == "cdecl" && probe->languageLinkage == CppLanguageLinkage::C,
                     "C function projection preserves symbol, signature and source");
        if (probe)
        {
            auto projected = projectCppSemanticFunction(*probe, 0, "Semantic");
            const auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projected);
            ok &= expect(symbol && symbol->isVariadic && symbol->parameters.size() == 1 &&
                         symbol->mangledName == "bridge_probe" &&
                         symbol->cppCanonicalIdentity.find("...") != std::string::npos,
                         "Clang C variadic signature projects with its external identity");
            if (symbol)
            {
                nativesharp::frontend::ProjectCompilation compilation;
                compilation.setSourceBufferProvider([](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() == "Main.ns")
                        return "int Main() { byte x = 3; float y = 1.5f; return bridge_probe(2, x, y); }\n";
                    return std::nullopt;
                });
                std::string error;
                const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                const bool ready = compilation.setCppImportedFunctions({*symbol}, error) &&
                    compilation.compileProjectOopMir(path, error);
                if (!ready)
                {
                    std::cerr << "C variadic MIR failed: " << error << '\n';
                    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
                        std::cerr << diagnostic.message() << '\n';
                }
                bool emitted = false;
                if (ready && compilation.oopMirModel() && compilation.nameLookup())
                {
                    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
                    const bool valid = module && module->verify(error);
                    const auto ir = module ? module->text() : std::string{};
                    ok &= expect(valid && ir.find("@bridge_probe(i64, ...)") != std::string::npos,
                                 "C variadic declaration reaches LLVM with its fixed argument");
                    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(error);
                    emitted = valid && target && target->emit(*module,
                        std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/variadic-call.obj",
                        nativesharp::llvm_backend::LlvmCodeGenFileType::Object, error);
                    if (!emitted) std::cerr << "C variadic object failed: " << lowerer.error() << ' ' << error << '\n';
                }
                ok &= expect(emitted, "C variadic call emits a Windows x64 object");
            }
        }
        if (cFunctions)
            ok &= expect(std::count_if(cFunctions->begin(), cFunctions->end(), [](const auto& value) {
                return value.qualifiedName == "bridge_probe";
            }) == 1, "repeated declarations share one canonical function");
        cRecords = c->recordsSnapshot();
        const auto* anonymousStructAlias = aliasByName("AnonStruct");
        const auto* anonymousUnionAlias = aliasByName("AnonUnion");
        const auto anonymousStructType = anonymousStructAlias
            ? nativesharp::frontend::cppAliasRecordType(anonymousStructAlias->underlyingType) : std::nullopt;
        const auto anonymousUnionType = anonymousUnionAlias
            ? nativesharp::frontend::cppAliasRecordType(anonymousUnionAlias->underlyingType) : std::nullopt;
        bool capturedAnonymousStruct = false, capturedAnonymousUnion = false;
        if (cRecords)
            for (const auto& record : *cRecords)
            {
                capturedAnonymousStruct |= record.name == "AnonStruct" && record.kind == CppRecordKind::Struct &&
                    record.complete && record.fields.size() == 1 && record.fields[0].name == "value";
                capturedAnonymousUnion |= record.name == "AnonUnion" && record.kind == CppRecordKind::Union &&
                    record.complete && record.fields.size() == 2;
            }
        ok &= expect(capturedAnonymousStruct && capturedAnonymousUnion && anonymousStructType && anonymousUnionType &&
                     anonymousStructType->name == "AnonStruct" && anonymousUnionType->name == "AnonUnion",
                     "C anonymous typedef records retain matching declaration and type identities");
        if (cRecords)
        {
            const auto sourcePair = std::find_if(cRecords->begin(), cRecords->end(),
                [](const auto& value) { return value.name == "Pair"; });
            const auto sourceOuter = std::find_if(cRecords->begin(), cRecords->end(),
                [](const auto& value) { return value.name == "OuterPair"; });
            const auto sourceMacro = std::find_if(cRecords->begin(), cRecords->end(),
                [](const auto& value) { return value.name == "MacroRecord"; });
            ok &= expect(sourcePair != cRecords->end() && sourceOuter != cRecords->end() &&
                         sourcePair->sourceProvenance && sourceOuter->sourceProvenance &&
                         sourcePair->sourceProvenance->sourcePath.filename() == "probe.c" &&
                         sourcePair->sourceProvenance->bufferSha256.size() == 64 &&
                         sourcePair->sourceProvenance->bufferSha256 == sourceOuter->sourceProvenance->bufferSha256 &&
                         sourcePair->sourceProvenance->byteOffset != sourceOuter->sourceProvenance->byteOffset,
                         "Clang record source provenance captures the analyzed buffer and declaration offsets");
            ok &= expect(sourceMacro != cRecords->end() && !sourceMacro->sourceProvenance,
                         "macro-generated record does not claim a direct source location");
            using nativesharp::frontend::CppImportedTypeOrigin;
            const auto checkOrigin = [&](std::string_view name, CppImportedTypeOrigin expected) {
                const auto record = std::find_if(cRecords->begin(), cRecords->end(),
                    [&](const auto& value) { return value.name == name; });
                if (record == cRecords->end()) return false;
                const auto identity = projectCppRecordIdentity(*record, "Semantic");
                if (!identity) return false;
                const std::vector<CppSemanticRecordEntry> entries{{{}, 0, 0, "x86_64-pc-windows-msvc", *record, identity}};
                const auto result = prepareCppRecordObservation(entries, groupCppSemanticRecords(entries));
                const auto* prepared = std::get_if<CppRecordObservationInputs>(&result);
                return prepared && prepared->types.size() == 1 && prepared->types[0].origin == expected &&
                    prepared->types[0].targetTriple == "x86_64-pc-windows-msvc";
            };
            ok &= expect(checkOrigin("Pair", CppImportedTypeOrigin::CStruct) &&
                         checkOrigin("AnonUnion", CppImportedTypeOrigin::CUnion),
                         "C struct and union retain their distinct origin and target");
            const auto checkSnapshot = [&](std::string_view name, CppRecordKind kind, bool complete) {
                const auto record = std::find_if(cRecords->begin(), cRecords->end(),
                    [&](const auto& value) { return value.name == name; });
                if (record == cRecords->end()) return false;
                const auto identity = projectCppRecordIdentity(*record, "Semantic");
                if (!identity) return false;
                const std::vector<CppSemanticRecordEntry> entries{{{}, 0, 0, "x86_64-pc-windows-msvc", *record, identity}};
                const auto groups = groupCppSemanticRecords(entries);
                if (groups.groups.size() != 1) return false;
                const auto result = captureCppCRecordSnapshot(entries, groups.groups.front());
                const auto* snapshot = std::get_if<CppCRecordSnapshot>(&result);
                if (!snapshot || snapshot->identityKey != identity->key() || snapshot->kind != kind ||
                    snapshot->moduleName != "Semantic" || snapshot->name != name ||
                    snapshot->valueLayoutUsable != complete ||
                    snapshot->complete != complete || snapshot->targetTriple != "x86_64-pc-windows-msvc") return false;
                return complete ? snapshot->sourceProvenance == record->sourceProvenance &&
                    snapshot->sizeBits == record->sizeBits &&
                    snapshot->alignmentBits == record->alignmentBits && snapshot->fields.size() == record->fields.size() :
                    snapshot->sourceProvenance == record->sourceProvenance &&
                    !snapshot->sizeBits && !snapshot->alignmentBits && snapshot->fields.empty();
            };
            ok &= expect(checkSnapshot("Pair", CppRecordKind::Struct, true) &&
                         checkSnapshot("AnonUnion", CppRecordKind::Union, true) &&
                         checkSnapshot("Incomplete", CppRecordKind::Struct, false),
                         "C record snapshots preserve complete struct and union or an incomplete declaration");
            if (sourcePair != cRecords->end() && sourcePair->sourceProvenance)
            {
                const auto identity = projectCppRecordIdentity(*sourcePair, "Semantic");
                if (identity)
                {
                    std::vector<CppSemanticRecordEntry> entries{
                        {{}, 0, 0, "x86_64-pc-windows-msvc", *sourcePair, identity},
                        {{}, 1, 0, "x86_64-pc-windows-msvc", *sourcePair, identity}};
                    for (auto& entry : entries)
                        entry.declaration.location = CppSourcePoint{std::filesystem::path(argv[2]), 1, 1};
                    auto groups = groupCppSemanticRecords(entries);
                    groups.groups.front().status = CppRecordGroupStatus::MultipleDefinitionsPointerCompatible;
                    auto same = captureCppCRecordSnapshot(entries, groups.groups.front());
                    const auto* sameSnapshot = std::get_if<CppCRecordSnapshot>(&same);
                    ok &= expect(sameSnapshot && sameSnapshot->sourceProvenance == sourcePair->sourceProvenance,
                                 "C snapshot retains agreed buffer provenance");
                    entries[1].declaration.sourceProvenance->bufferSha256 = std::string(64, '0');
                    auto differing = captureCppCRecordSnapshot(entries, groups.groups.front());
                    const auto* differingSnapshot = std::get_if<CppCRecordSnapshot>(&differing);
                    ok &= expect(differingSnapshot && !differingSnapshot->sourceProvenance,
                                 "C snapshot discards conflicting buffer provenance");
                }
                else ok &= expect(false, "C provenance test has a record identity");
            }
            else ok &= expect(false, "C provenance test has a direct record");
            std::vector<CppSemanticRecordEntry> catalogEntries;
            for (const std::string_view name : {"Pair", "Incomplete"})
            {
                const auto found = std::find_if(cRecords->begin(), cRecords->end(),
                    [&](const auto& value) { return value.name == name; });
                if (found != cRecords->end())
                    catalogEntries.push_back({{}, 0, 0, "x86_64-pc-windows-msvc", *found,
                                              projectCppRecordIdentity(*found, "Semantic")});
            }
            auto catalogGroups = groupCppSemanticRecords(catalogEntries).groups;
            const auto catalogResult = captureCppCRecordCatalog(catalogEntries, catalogGroups);
            const auto* catalog = std::get_if<std::vector<CppCRecordSnapshot>>(&catalogResult);
            ok &= expect(catalogEntries.size() == 2 && catalog && catalog->size() == 2 &&
                         (*catalog)[0].identityKey < (*catalog)[1].identityKey,
                         "C record catalog is ordered deterministically");
            if (catalog)
            {
                auto restoredCatalog = *catalog;
                for (auto& record : restoredCatalog)
                    if (record.complete) record.valueLayoutUsable = false;
                std::vector<CppCEnumSnapshot> enumCatalog;
                if (cEnums)
                {
                    const auto color = std::find_if(cEnums->begin(), cEnums->end(),
                        [](const auto& item) { return item.name == "Color"; });
                    if (color != cEnums->end())
                    {
                        const std::vector<CppSemanticEnumEntry> entries{{{}, 0, 0, "x86_64-pc-windows-msvc", *color}};
                        const nativesharp::compilation::TypeIdentity identity("Semantic", {}, {}, "Color");
                        const std::vector<std::string> wanted{identity.key()};
                        auto captured = captureCppCEnumCatalog(entries, 0, "Semantic", wanted);
                        if (auto* values = std::get_if<std::vector<CppCEnumSnapshot>>(&captured)) enumCatalog = std::move(*values);
                    }
                }
                std::vector<nativesharp::frontend::CppImportedTypeDeclaration> restored;
                std::string restoreError;
                const bool reconstructed = metadata_detail::reconstructCImportedTypes(restoredCatalog, enumCatalog,
                    7, "Semantic", "x86_64-pc-windows-msvc", "producer.nsmeta", restored, restoreError);
                bool pointerLayout = false, incompleteType = false, enumBase = false;
                for (const auto& type : restored)
                {
                    pointerLayout |= type.identity.name() == "Pair" && !type.objectLayout && type.memberAccessLayout.has_value();
                    incompleteType |= type.identity.name() == "Incomplete" && type.isIncomplete && !type.objectLayout;
                    enumBase |= type.identity.name() == "Color" && type.enumBase.has_value() && type.objectLayout.has_value();
                }
                ok &= expect(reconstructed && enumCatalog.size() == 1 && pointerLayout && incompleteType && enumBase,
                             "C metadata reconstructs record layout, incomplete type and enum base");
                const auto restoredCount = restored.size();
                ok &= expect(!metadata_detail::reconstructCImportedTypes(restoredCatalog, enumCatalog,
                                 7, "Semantic", "different-target", "producer.nsmeta", restored, restoreError) &&
                             restored.size() == restoredCount,
                             "C type reconstruction rejects another target without changing output");
                std::vector<std::uint8_t> encoded, reencoded;
                std::vector<CppCRecordSnapshot> decoded;
                std::string codecError;
                const bool roundTrip = metadata_detail::encodeCRecordCatalog(*catalog, encoded, codecError) &&
                    metadata_detail::decodeCRecordCatalog(encoded, decoded, codecError) &&
                    metadata_detail::encodeCRecordCatalog(decoded, reencoded, codecError);
                ok &= expect(roundTrip && encoded == reencoded && decoded.size() == catalog->size(),
                             "C record catalog codec preserves its complete encoding");
                auto provenanceCatalog = *catalog;
                for (auto& record : provenanceCatalog)
                    if (!record.complete) record.sourceProvenance.reset();
                {
                    std::vector<std::uint8_t> forwardBytes;
                    std::vector<CppCRecordSnapshot> forwardRecords;
                    std::vector<CppCEnumSnapshot> forwardEnums;
                    const auto forward = std::find_if(catalog->begin(), catalog->end(),
                        [](const auto& record) { return !record.complete && record.sourceProvenance.has_value(); });
                    const bool forwardRoundTrip = forward != catalog->end() &&
                        metadata_detail::encodeCTypeCatalogWithProvenance(*catalog, {}, forwardBytes, codecError) &&
                        !forwardBytes.empty() && forwardBytes[0] == 3 &&
                        metadata_detail::decodeVersionedCTypeCatalog(forwardBytes, forwardRecords, forwardEnums, codecError);
                    const auto recoveredForward = std::find_if(forwardRecords.begin(), forwardRecords.end(),
                        [](const auto& record) { return !record.complete && record.sourceProvenance.has_value(); });
                    ok &= expect(forwardRoundTrip && recoveredForward != forwardRecords.end() &&
                                 recoveredForward->sourceProvenance == forward->sourceProvenance,
                                 "version 3 preserves incomplete C record provenance");
                    LibraryMetadata forwardMetadata;
                    forwardMetadata.formatMinor = 44;
                    forwardMetadata.targetTriple = "x86_64-pc-windows-msvc";
                    forwardMetadata.moduleName = "Semantic";
                    forwardMetadata.producerVersion = "forward-test";
                    forwardMetadata.cppCRecordCatalog = forwardBytes;
                    ok &= expect(forwardRoundTrip && !metadata_detail::verifyBinaryClosureMetadata(forwardMetadata, codecError),
                                 "metadata 1.44 rejects incomplete C provenance");
                    forwardMetadata.formatMinor = 45;
                    ok &= expect(forwardRoundTrip && metadata_detail::verifyBinaryClosureMetadata(forwardMetadata, codecError),
                                 "metadata 1.45 accepts incomplete C provenance");
                }
                const auto provenanceRecord = std::find_if(provenanceCatalog.begin(), provenanceCatalog.end(),
                    [](const auto& item) { return item.complete; });
                if (provenanceRecord != provenanceCatalog.end())
                {
                    provenanceRecord->sourceProvenance = CppRecordDeclaration::SourceProvenance{
                        std::filesystem::path(u8"headers/ni\u00f1o.h"), std::string(64, 'a'), 27};
                    const auto recordIndex = static_cast<std::size_t>(provenanceRecord - provenanceCatalog.begin());
                    std::vector<std::uint8_t> envelope;
                    std::vector<CppCRecordSnapshot> recovered;
                    std::vector<CppCEnumSnapshot> recoveredEnums;
                    const bool withProvenance = metadata_detail::encodeCTypeCatalogWithProvenance(
                        provenanceCatalog, {}, envelope, codecError) &&
                        metadata_detail::decodeCTypeCatalogWithProvenance(envelope, recovered, recoveredEnums, codecError);
                    ok &= expect(withProvenance && recovered.size() == provenanceCatalog.size() &&
                                 recovered[recordIndex].sourceProvenance == provenanceRecord->sourceProvenance,
                                 "versioned C catalogue retains UTF-8 source provenance");
                    if (!provenanceRecord->fields.empty() && provenanceRecord->sizeBits &&
                        *provenanceRecord->sizeBits >= 64)
                    {
                        auto floatingCatalog = provenanceCatalog;
                        auto& field = floatingCatalog[recordIndex].fields.front();
                        CppType floating;
                        floating.kind = CppTypeKind::Floating;
                        floating.spelling = "long double";
                        floating.canonicalSpelling = "long double";
                        floating.unqualifiedCanonicalSpelling = "long double";
                        floating.sizeBits = 64;
                        floating.alignmentBits = 64;
                        floating.ieeeBinary64 = true;
                        field.type = floating;
                        field.offsetBits = 0;
                        std::vector<std::uint8_t> floatingBytes;
                        std::vector<CppCRecordSnapshot> floatingRecords;
                        std::vector<CppCEnumSnapshot> floatingEnums;
                        const bool floatingRoundTrip = metadata_detail::encodeCTypeCatalogWithProvenance(
                            floatingCatalog, {}, floatingBytes, codecError) &&
                            metadata_detail::decodeCTypeCatalogWithProvenance(
                                floatingBytes, floatingRecords, floatingEnums, codecError);
                        ok &= expect(floatingRoundTrip && floatingBytes[0] == 2 &&
                                     floatingRecords[recordIndex].fields.front().type.ieeeBinary64,
                                     "versioned C catalogue preserves confirmed long double format");
                        auto missingFormat = floatingCatalog[recordIndex];
                        missingFormat.fields.front().type.ieeeBinary64 = false;
                        auto confirmed = floatingCatalog[recordIndex];
                        confirmed.clangUsr = "float-record-usr";
                        missingFormat.clangUsr = confirmed.clangUsr;
                        metadata_detail::CRecordContractComparison floatComparison{};
                        ok &= expect(metadata_detail::compareCRecordContracts(confirmed, missingFormat, floatComparison, codecError) &&
                                     floatComparison == metadata_detail::CRecordContractComparison::Insufficient,
                                     "C record comparison does not infer an unknown floating format");
                        LibraryMetadata formatMetadata;
                        formatMetadata.formatMinor = 43;
                        formatMetadata.targetTriple = "x86_64-pc-windows-msvc";
                        formatMetadata.moduleName = "Semantic";
                        formatMetadata.producerVersion = "format-test";
                        formatMetadata.cppCRecordCatalog = floatingBytes;
                        ok &= expect(!metadata_detail::verifyBinaryClosureMetadata(formatMetadata, codecError),
                                     "old metadata version rejects a catalogue with floating format");
                        formatMetadata.formatMinor = 44;
                        ok &= expect(metadata_detail::verifyBinaryClosureMetadata(formatMetadata, codecError),
                                     "current metadata accepts a canonical floating format catalogue");
                        auto mixedCatalog = floatingCatalog;
                        for (std::size_t i = 0; i < mixedCatalog.size(); ++i)
                            if (!mixedCatalog[i].complete) mixedCatalog[i].sourceProvenance = (*catalog)[i].sourceProvenance;
                        auto mixedMetadata = formatMetadata;
                        mixedMetadata.formatMinor = 45;
                        const bool mixedEncoded = metadata_detail::encodeCTypeCatalogWithProvenance(
                            mixedCatalog, {}, mixedMetadata.cppCRecordCatalog, codecError);
                        ok &= expect(mixedEncoded && !mixedMetadata.cppCRecordCatalog.empty() &&
                                     mixedMetadata.cppCRecordCatalog[0] == 4 &&
                                     metadata_detail::verifyBinaryClosureMetadata(mixedMetadata, codecError) &&
                                     metadata_detail::encodeAbiProjection(mixedMetadata) ==
                                         metadata_detail::encodeAbiProjection(formatMetadata),
                                     "version 4 retains floating ABI while excluding incomplete provenance");
                        auto movedSource = floatingCatalog;
                        movedSource[recordIndex].sourceProvenance->sourcePath = "different/source.h";
                        auto movedMetadata = formatMetadata;
                        const bool movedEncoded = metadata_detail::encodeCTypeCatalogWithProvenance(
                            movedSource, {}, movedMetadata.cppCRecordCatalog, codecError);
                        ok &= expect(movedEncoded && metadata_detail::encodeAbiProjection(movedMetadata) ==
                                     metadata_detail::encodeAbiProjection(formatMetadata),
                                     "floating format ABI projection excludes source provenance");
                        auto withoutFormat = floatingCatalog;
                        withoutFormat[recordIndex].fields.front().type.ieeeBinary64 = false;
                        auto withoutFormatMetadata = formatMetadata;
                        const bool withoutEncoded = metadata_detail::encodeCTypeCatalogWithProvenance(
                            withoutFormat, {}, withoutFormatMetadata.cppCRecordCatalog, codecError);
                        ok &= expect(withoutEncoded && metadata_detail::encodeAbiProjection(withoutFormatMetadata) !=
                                     metadata_detail::encodeAbiProjection(formatMetadata),
                                     "floating format changes the ABI projection");
                        if (!floatingBytes.empty()) floatingBytes.back() = 2;
                        ok &= expect(!metadata_detail::decodeCTypeCatalogWithProvenance(
                                         floatingBytes, floatingRecords, floatingEnums, codecError),
                                     "C catalogue rejects an invalid floating format flag");
                    }
                    std::vector<std::uint8_t> oldCatalog;
                    std::vector<CppCRecordSnapshot> oldRecords;
                    const bool oldFormat = metadata_detail::encodeCRecordCatalog(provenanceCatalog, oldCatalog, codecError) &&
                        metadata_detail::decodeCRecordCatalog(oldCatalog, oldRecords, codecError);
                    ok &= expect(oldFormat && !oldRecords[recordIndex].sourceProvenance,
                                 "C record catalogue 1.42 does not silently change its format");
                    auto truncated = envelope;
                    if (!truncated.empty()) truncated.pop_back();
                    const auto recoveredSize = recovered.size();
                    ok &= expect(!metadata_detail::decodeCTypeCatalogWithProvenance(truncated, recovered, recoveredEnums, codecError) &&
                                 recovered.size() == recoveredSize,
                                 "truncated provenance catalogue preserves previous outputs");
                    provenanceRecord->sourceProvenance->bufferSha256 = "invalid";
                    const auto previousSize = envelope.size();
                    ok &= expect(!metadata_detail::encodeCTypeCatalogWithProvenance(provenanceCatalog, {}, envelope, codecError) &&
                                 envelope.size() == previousSize,
                                 "provenance catalogue rejects an invalid digest without replacing output");
                }
                else ok &= expect(false, "provenance codec has a complete C record");
                const auto comparisonRecord = std::find_if(catalog->begin(), catalog->end(),
                    [](const auto& item) { return item.complete && !item.fields.empty(); });
                if (comparisonRecord != catalog->end())
                {
                    auto first = *comparisonRecord;
                    first.clangUsr = "test-record-usr";
                    auto second = first;
                    second.moduleName = "OtherModule";
                    std::vector<nativesharp::compilation::TypeNameSegment> containers;
                    for (const auto& name : second.containingRecords) containers.push_back({name, 0});
                    second.identityKey = nativesharp::compilation::TypeIdentity(second.moduleName,
                        second.namespaceSegments, std::move(containers), second.name).key();
                    metadata_detail::CRecordContractComparison comparison{};
                    const auto compare = [&] { return metadata_detail::compareCRecordContracts(first, second, comparison, codecError); };
                    ok &= expect(compare() && comparison == metadata_detail::CRecordContractComparison::Matching,
                                 "C record contracts match across module identities without asserting nominal identity");
                    second.fields.front().name += "_different";
                    ok &= expect(compare() && comparison == metadata_detail::CRecordContractComparison::Different,
                                 "C record contracts distinguish field changes");
                    second = first; second.targetTriple = "other-target";
                    ok &= expect(compare() && comparison == metadata_detail::CRecordContractComparison::Different,
                                 "C record contracts distinguish target triples");
                    second = first; second.clangUsr.reset();
                    ok &= expect(compare() && comparison == metadata_detail::CRecordContractComparison::Insufficient,
                                 "C record comparison requires both USRs");
                    second = first; second.fields.front().offsetBits = *second.sizeBits + 1;
                    comparison = metadata_detail::CRecordContractComparison::Matching;
                    ok &= expect(!compare() && comparison == metadata_detail::CRecordContractComparison::Matching,
                                 "invalid C record comparison leaves its result unchanged");
                    const auto actualFile = std::filesystem::absolute(std::filesystem::path(__FILE__));
                    nativesharp::basic::Sha256Digest digest;
                    std::uintmax_t fileSize = 0;
                    std::string digestError;
                    const bool hashed = nativesharp::basic::sha256File(actualFile, digest, fileSize, digestError);
                    first.sourceProvenance = CppRecordDeclaration::SourceProvenance{
                        actualFile, nativesharp::basic::sha256Hex(digest), 0};
                    second = first;
                    const auto sourceEvidence = [&] {
                        return metadata_detail::compareCRecordSourceEvidence(first, second, comparison, codecError);
                    };
                    ok &= expect(hashed && fileSize && sourceEvidence() &&
                                 comparison == metadata_detail::CRecordContractComparison::Matching,
                                 "C root evidence matches an unchanged physical source and contract");
                    auto compareClosure = [&] {
                        return metadata_detail::compareCRecordClosureEvidence(std::span(&first, 1), {}, first.identityKey,
                            std::span(&second, 1), {}, second.identityKey, comparison, codecError);
                    };
                    ok &= expect(compareClosure() && comparison == metadata_detail::CRecordContractComparison::Matching,
                                 "C closure accepts a record without foreign dependencies");
                    if (first.sizeBits && *first.sizeBits >= 64)
                    {
                        CppType self;
                        self.kind = CppTypeKind::Record;
                        CppRecordTypeIdentity selfIdentity;
                        selfIdentity.name = first.name;
                        selfIdentity.namespaceSegments = first.namespaceSegments;
                        selfIdentity.containingRecords = first.containingRecords;
                        selfIdentity.clangUsr = first.clangUsr;
                        self.recordIdentity = std::move(selfIdentity);
                        CppType pointer;
                        pointer.kind = CppTypeKind::Pointer;
                        pointer.sizeBits = 64;
                        pointer.element = std::make_shared<CppType>(self);
                        first.fields.front().offsetBits = 0;
                        first.fields.front().type = pointer;
                        second = first;
                        ok &= expect(compareClosure() && comparison == metadata_detail::CRecordContractComparison::Matching,
                                     "C closure terminates on a self-referential pointer");
                        self.recordIdentity->name = "MissingDependency";
                        pointer.element = std::make_shared<CppType>(self);
                        first.fields.front().type = pointer;
                        second = first;
                        ok &= expect(compareClosure() && comparison == metadata_detail::CRecordContractComparison::Insufficient,
                                     "C closure does not accept a missing record dependency");
                    }
                    else ok &= expect(false, "C closure test record can hold a pointer");
                    first = *comparisonRecord;
                    first.clangUsr = "test-record-usr";
                    first.sourceProvenance = CppRecordDeclaration::SourceProvenance{
                        actualFile, nativesharp::basic::sha256Hex(digest), 0};
                    second = first;
                    if (!enumCatalog.empty() && enumCatalog.front().declaration.clangUsr &&
                        !enumCatalog.front().declaration.enumerators.empty())
                    {
                        auto leftEnum = enumCatalog.front();
                        auto rightEnum = leftEnum;
                        CppType enumField;
                        enumField.kind = CppTypeKind::Enumeration;
                        enumField.sizeBits = leftEnum.declaration.sizeBits;
                        enumField.enumIdentity = CppEnumTypeIdentity{
                            leftEnum.declaration.name, leftEnum.declaration.qualifiedName, false,
                            leftEnum.declaration.clangUsr};
                        first.fields.front().offsetBits = 0;
                        first.fields.front().type = enumField;
                        second = first;
                        const auto enumClosure = [&] {
                            return metadata_detail::compareCRecordClosureEvidence(std::span(&first, 1),
                                std::span(&leftEnum, 1), first.identityKey, std::span(&second, 1),
                                std::span(&rightEnum, 1), second.identityKey, comparison, codecError);
                        };
                        ok &= expect(enumClosure() && comparison == metadata_detail::CRecordContractComparison::Matching,
                                     "C closure checks a matching enum field dependency");
                        rightEnum.declaration.enumerators.front().decimalValue = "77";
                        ok &= expect(enumClosure() && comparison == metadata_detail::CRecordContractComparison::Different,
                                     "C closure rejects changed enum constants");
                        first = *comparisonRecord;
                        first.clangUsr = "test-record-usr";
                        first.sourceProvenance = CppRecordDeclaration::SourceProvenance{
                            actualFile, nativesharp::basic::sha256Hex(digest), 0};
                        second = first;
                    }
                    else ok &= expect(false, "C closure test includes an enum with identity");
                    second.fields.front().name += "_different";
                    ok &= expect(compareClosure() && comparison == metadata_detail::CRecordContractComparison::Different,
                                 "C closure rejects differing record contracts");
                    second = first;
                    second.sourceProvenance->byteOffset = 1;
                    ok &= expect(sourceEvidence() && comparison == metadata_detail::CRecordContractComparison::Different,
                                 "C root evidence distinguishes declaration offsets");
                    second = first;
                    first.sourceProvenance->bufferSha256 = std::string(64, '0');
                    second.sourceProvenance->bufferSha256 = first.sourceProvenance->bufferSha256;
                    ok &= expect(sourceEvidence() && comparison == metadata_detail::CRecordContractComparison::Insufficient,
                                 "C root evidence refuses stale source contents");
                    second.sourceProvenance.reset();
                    ok &= expect(sourceEvidence() && comparison == metadata_detail::CRecordContractComparison::Insufficient,
                                 "C root evidence refuses missing provenance");
                }
                else ok &= expect(false, "C record contract comparison has a complete test record");
                LibraryMetadata catalogMetadata;
                catalogMetadata.formatMinor = 42;
                catalogMetadata.targetTriple = "x86_64-pc-windows-msvc";
                catalogMetadata.moduleName = "Semantic";
                catalogMetadata.producerVersion = "catalog-test";
                const bool combinedEncoded = metadata_detail::encodeCTypeCatalog(*catalog, {}, catalogMetadata.cppCRecordCatalog, codecError);
                ok &= expect(roundTrip && combinedEncoded && metadata_detail::verifyBinaryClosureMetadata(catalogMetadata, codecError),
                             "metadata validates a canonical C record catalogue for its target");
                {
                    auto withOrigin = *catalog;
                    for (auto& record : withOrigin)
                        if (!record.complete) record.sourceProvenance.reset();
                    auto completeRecord = std::find_if(withOrigin.begin(), withOrigin.end(),
                        [](const auto& record) { return record.complete; });
                    if (completeRecord != withOrigin.end())
                    {
                        completeRecord->sourceProvenance = CppRecordDeclaration::SourceProvenance{
                            std::filesystem::path("headers/source.h"), std::string(64, 'b'), 12};
                        auto newerMetadata = catalogMetadata;
                        const bool encodedOrigin = metadata_detail::encodeCTypeCatalogWithProvenance(
                            withOrigin, {}, newerMetadata.cppCRecordCatalog, codecError);
                        newerMetadata.formatMinor = 42;
                        const bool oldRejected = encodedOrigin &&
                            !metadata_detail::verifyBinaryClosureMetadata(newerMetadata, codecError);
                        newerMetadata.formatMinor = 43;
                        const bool newAccepted = encodedOrigin &&
                            metadata_detail::verifyBinaryClosureMetadata(newerMetadata, codecError);
                        std::vector<CppCRecordSnapshot> decodedVersioned;
                        std::vector<CppCEnumSnapshot> enumsVersioned;
                        const bool readNew = metadata_detail::decodeVersionedCTypeCatalog(
                            newerMetadata.cppCRecordCatalog, decodedVersioned, enumsVersioned, codecError);
                        const bool readOld = metadata_detail::decodeVersionedCTypeCatalog(
                            catalogMetadata.cppCRecordCatalog, decodedVersioned, enumsVersioned, codecError);
                        ok &= expect(oldRejected && newAccepted && readNew && readOld,
                                     "versioned C provenance requires metadata 1.43 and legacy catalogues remain readable");
                        ok &= expect(metadata_detail::encodeAbiProjection(newerMetadata) ==
                                     metadata_detail::encodeAbiProjection(catalogMetadata),
                                     "C source provenance does not change the public ABI projection");
                        LibraryMetadataCodec metadataCodec;
                        LibraryMetadata loadedOrigin;
                        const auto originFile = std::filesystem::path(argv[2]) / "cpp-catalog-43.nsmeta";
                        const bool originRoundTrip = metadataCodec.writeModel(newerMetadata, originFile, codecError) &&
                            metadataCodec.read(originFile, loadedOrigin, codecError) &&
                            metadataCodec.verifyCurrentCompiler(loadedOrigin, codecError) &&
                            loadedOrigin.formatMinor == 43 && metadata_detail::decodeVersionedCTypeCatalog(
                                loadedOrigin.cppCRecordCatalog, decodedVersioned, enumsVersioned, codecError);
                        const auto foundOrigin = std::find_if(decodedVersioned.begin(), decodedVersioned.end(),
                            [&](const auto& item) { return item.identityKey == completeRecord->identityKey; });
                        ok &= expect(originRoundTrip && foundOrigin != decodedVersioned.end() &&
                                     foundOrigin->sourceProvenance == completeRecord->sourceProvenance,
                                     "metadata 1.43 writes and reads a C source provenance catalogue");
                    }
                    else ok &= expect(false, "versioned catalogue has a complete record");
                }
                {
                    LibraryMetadataCodec codec;
                    const auto metadataFile = std::filesystem::path(argv[2]) / "cpp-catalog-42.nsmeta";
                    LibraryMetadata reread;
                    const bool roundtrip42 = combinedEncoded && codec.writeModel(catalogMetadata, metadataFile, codecError) &&
                        codec.read(metadataFile, reread, codecError) && codec.verifyCurrentCompiler(reread, codecError);
                    ok &= expect(roundtrip42 && reread.formatMinor == 42 &&
                                 reread.cppCRecordCatalog == catalogMetadata.cppCRecordCatalog,
                                 "public metadata codec writes and authenticates the 1.42 C catalogue");
                    auto legacy = catalogMetadata;
                    legacy.formatMinor = 41;
                    ok &= expect(!codec.writeModel(legacy, metadataFile, codecError),
                                 "public metadata codec rejects C catalogue in 1.41");
                    legacy.cppCRecordCatalog.clear();
                    const bool legacyRoundtrip = codec.writeModel(legacy, metadataFile, codecError) &&
                        codec.read(metadataFile, reread, codecError) && codec.verifyCurrentCompiler(reread, codecError);
                    ok &= expect(legacyRoundtrip && reread.formatMinor == 41 && reread.cppCRecordCatalog.empty(),
                                 "public metadata codec retains 1.41 compatibility");
                }
                const auto payload = metadata_detail::encodePayload(catalogMetadata, 3, true);
                LibraryMetadata decodedPayload;
                decodedPayload.formatMinor = 42;
                const bool payloadRead = metadata_detail::decodePayload(payload, decodedPayload, codecError);
                ok &= expect(payloadRead && decodedPayload.cppCRecordCatalog == catalogMetadata.cppCRecordCatalog,
                             "prepared metadata payload preserves the C record catalogue");
                auto withoutCatalog = catalogMetadata;
                withoutCatalog.cppCRecordCatalog.clear();
                ok &= expect(metadata_detail::encodeAbiProjection(catalogMetadata) !=
                                 metadata_detail::encodeAbiProjection(withoutCatalog),
                             "public C record catalogue contributes to the ABI projection");
                auto truncatedPayload = payload;
                if (!truncatedPayload.empty()) truncatedPayload.pop_back();
                LibraryMetadata truncatedDecoded;
                truncatedDecoded.formatMinor = 42;
                ok &= expect(!metadata_detail::decodePayload(truncatedPayload, truncatedDecoded, codecError),
                             "metadata payload rejects a truncated C record catalogue");
                catalogMetadata.targetTriple = "different-target";
                ok &= expect(!metadata_detail::verifyBinaryClosureMetadata(catalogMetadata, codecError),
                             "metadata rejects a C record catalogue from another target");
                auto nestedCatalog = *catalog;
                const auto nestedRecord = std::find_if(nestedCatalog.begin(), nestedCatalog.end(),
                    [](const auto& record) { return record.complete && !record.fields.empty(); });
                if (nestedRecord != nestedCatalog.end())
                {
                    auto& type = nestedRecord->fields.front().type;
                    type = CppType{};
                    type.kind = CppTypeKind::Pointer;
                    type.isConst = true;
                    type.isRestrict = true;
                    type.sizeBits = 8;
                    auto functionType = std::make_shared<CppType>();
                    functionType->kind = CppTypeKind::Function;
                    auto signature = std::make_shared<CppFunctionType>();
                    signature->returnType.kind = CppTypeKind::Record;
                    signature->returnType.recordIdentity = CppRecordTypeIdentity{};
                    signature->returnType.recordIdentity->name = "Nested";
                    signature->returnType.recordIdentity->clangUsr = "record-usr";
                    CppType parameter;
                    parameter.kind = CppTypeKind::Enumeration;
                    parameter.isVolatile = true;
                    parameter.enumIdentity = CppEnumTypeIdentity{};
                    parameter.enumIdentity->name = "Shade";
                    parameter.enumIdentity->clangUsr = "enum-usr";
                    signature->parameterTypes.push_back(std::move(parameter));
                    signature->callingConvention = "cdecl";
                    signature->hasPrototype = true;
                    functionType->function = std::move(signature);
                    type.element = std::move(functionType);
                    std::vector<std::uint8_t> nestedBytes;
                    std::vector<CppCRecordSnapshot> nestedDecoded;
                    const bool nestedRoundTrip = metadata_detail::encodeCRecordCatalog(nestedCatalog, nestedBytes, codecError) &&
                        metadata_detail::decodeCRecordCatalog(nestedBytes, nestedDecoded, codecError);
                    const auto position = static_cast<std::size_t>(nestedRecord - nestedCatalog.begin());
                    const CppType* recovered = nestedRoundTrip ? &nestedDecoded[position].fields.front().type : nullptr;
                    const CppFunctionType* recoveredFunction = recovered && recovered->element ? recovered->element->function.get() : nullptr;
                    ok &= expect(recovered && recovered->isConst && recovered->isRestrict && recoveredFunction &&
                                 recoveredFunction->returnType.recordIdentity &&
                                 recoveredFunction->returnType.recordIdentity->clangUsr == "record-usr" &&
                                 recoveredFunction->parameterTypes.size() == 1 &&
                                 recoveredFunction->parameterTypes[0].isVolatile &&
                                 recoveredFunction->parameterTypes[0].enumIdentity &&
                                 recoveredFunction->parameterTypes[0].enumIdentity->clangUsr == "enum-usr" &&
                                 recoveredFunction->callingConvention == "cdecl" && recoveredFunction->hasPrototype,
                                 "C record codec preserves nested function types, qualifiers and identities");
                }
                else ok &= expect(false, "C record codec nested-type test has a complete record");
                if (!encoded.empty()) encoded.pop_back();
                const auto priorSize = decoded.size();
                ok &= expect(!metadata_detail::decodeCRecordCatalog(encoded, decoded, codecError) &&
                             decoded.size() == priorSize,
                             "C record catalog codec rejects truncation without changing output");
                auto invalid = *catalog;
                invalid.push_back(invalid.back());
                std::vector<std::uint8_t> preserved{0x42};
                ok &= expect(!metadata_detail::encodeCRecordCatalog(invalid, preserved, codecError) &&
                             preserved == std::vector<std::uint8_t>{0x42},
                             "C record codec rejects duplicate identities without changing output");
                invalid = *catalog;
                invalid.front().name = "DifferentName";
                ok &= expect(!metadata_detail::encodeCRecordCatalog(invalid, preserved, codecError) &&
                             preserved == std::vector<std::uint8_t>{0x42},
                             "C record codec rejects identity components that disagree with the key");
                invalid = *catalog;
                const auto incomplete = std::find_if(invalid.begin(), invalid.end(),
                    [](const auto& record) { return !record.complete; });
                if (incomplete != invalid.end())
                {
                    incomplete->sizeBits = 8;
                    ok &= expect(!metadata_detail::encodeCRecordCatalog(invalid, preserved, codecError) &&
                                 preserved == std::vector<std::uint8_t>{0x42},
                                 "C record codec rejects layout on an incomplete record");
                }
                else ok &= expect(false, "C record codec test includes an incomplete record");
                invalid = *catalog;
                const auto complete = std::find_if(invalid.begin(), invalid.end(),
                    [](const auto& record) { return record.complete && !record.fields.empty(); });
                if (complete != invalid.end())
                {
                    complete->fields.front().offsetBits = *complete->sizeBits + 1;
                    ok &= expect(!metadata_detail::encodeCRecordCatalog(invalid, preserved, codecError),
                                 "C record codec rejects an out-of-bounds field");
                    complete->fields.front().offsetBits = 0;
                    complete->fields.front().type.sizeBits = *complete->sizeBits + 8;
                    ok &= expect(!metadata_detail::encodeCRecordCatalog(invalid, preserved, codecError),
                                 "C record codec rejects a field whose size exceeds its record");
                }
                else ok &= expect(false, "C record codec test includes a complete record with fields");
            }
            if (!catalogGroups.empty())
            {
                catalogGroups.push_back(catalogGroups.front());
                ok &= expect(std::holds_alternative<CppCRecordSnapshotFailure>(
                                 captureCppCRecordCatalog(catalogEntries, catalogGroups)),
                             "C record catalog rejects duplicate identities");
                catalogGroups.back().inventoryIndices.front() = catalogEntries.size();
                ok &= expect(std::holds_alternative<CppCRecordSnapshotFailure>(
                                 captureCppCRecordCatalog(catalogEntries, catalogGroups)),
                             "C record catalog rejects an invalid group without returning partial data");
            }
            std::vector<CppSemanticRecordEntry> closureEntries;
            for (const std::string_view name : {"Pair", "OuterPair"})
            {
                const auto found = std::find_if(cRecords->begin(), cRecords->end(),
                    [&](const auto& value) { return value.name == name; });
                if (found != cRecords->end())
                    closureEntries.push_back({{}, 0, 0, "x86_64-pc-windows-msvc", *found,
                                              projectCppRecordIdentity(*found, "Semantic")});
            }
            const auto closureGroups = groupCppSemanticRecords(closureEntries);
            if (closureEntries.size() == 2 && closureEntries.back().identity)
            {
                const std::vector<std::string> seeds{closureEntries.back().identity->key()};
                const auto closureResult = collectCppCRecordClosure(closureEntries, closureGroups.groups, 0, seeds);
                const auto* closure = std::get_if<CppCRecordClosure>(&closureResult);
                ok &= expect(closure && closure->records.size() == 2 && closure->enumIdentityKeys.empty(),
                             "C record closure includes nested records in fields and arrays");
                auto missingGroups = closureGroups.groups;
                missingGroups.erase(std::remove_if(missingGroups.begin(), missingGroups.end(),
                    [](const auto& group) { return group.identity.name() == "Pair"; }), missingGroups.end());
                ok &= expect(std::holds_alternative<CppCRecordSnapshotFailure>(
                                 collectCppCRecordClosure(closureEntries, missingGroups, 0, seeds)),
                             "C record closure rejects a missing nested record");
            }
            else ok &= expect(false, "C record closure test has both required observations");
            const auto pair = std::find_if(cRecords->begin(), cRecords->end(),
                [](const auto& value) { return value.name == "Pair"; });
            if (pair != cRecords->end())
            {
                const auto identity = projectCppRecordIdentity(*pair, "Semantic");
                if (identity)
                {
                    std::vector<CppSemanticRecordEntry> entries{{{}, 0, 0, "x86_64-pc-windows-msvc", *pair, identity}};
                    entries.push_back(entries.front());
                    entries.back().declaration.isCxx = true;
                    entries.back().targetTriple = "different-target";
                    const auto result = prepareCppRecordObservation(entries, groupCppSemanticRecords(entries));
                    const auto* prepared = std::get_if<CppRecordObservationInputs>(&result);
                    ok &= expect(prepared && prepared->types.size() == 1 &&
                                 prepared->types[0].origin == CppImportedTypeOrigin::Unknown &&
                                 !prepared->types[0].targetTriple,
                                 "conflicting observations do not invent an origin or target");
                    const auto conflictingGroups = groupCppSemanticRecords(entries);
                    ok &= expect(conflictingGroups.groups.size() == 1 &&
                                 std::holds_alternative<CppCRecordSnapshotFailure>(
                                     captureCppCRecordSnapshot(entries, conflictingGroups.groups.front())),
                                 "C record snapshot rejects conflicting observations");
                    std::vector<CppSemanticRecordEntry> nestedEntries{{{}, 0, 0, "x86_64-pc-windows-msvc", *pair, identity}};
                    nestedEntries.push_back(nestedEntries.front());
                    const auto nestedHeader = std::filesystem::path(argv[2]) / "cpp-record-snapshot-fixture.h";
                    { std::ofstream file(nestedHeader); file << "/* snapshot identity fixture */\n"; }
                    for (std::size_t i = 0; i < nestedEntries.size(); ++i)
                    {
                        nestedEntries[i].declaration.location = CppSourcePoint{nestedHeader, 1, 1};
                        auto& field = nestedEntries[i].declaration.fields.front();
                        field.type = CppType{};
                        field.type.kind = CppTypeKind::Pointer;
                        auto nested = std::make_shared<CppType>();
                        nested->kind = CppTypeKind::Function;
                        auto signature = std::make_shared<CppFunctionType>();
                        signature->returnType.kind = CppTypeKind::Record;
                        signature->returnType.recordIdentity = CppRecordTypeIdentity{};
                        signature->returnType.recordIdentity->name = "Nested";
                        CppType argument;
                        argument.kind = CppTypeKind::Enumeration;
                        argument.enumIdentity = CppEnumTypeIdentity{};
                        argument.enumIdentity->name = "Color";
                        if (i == 1)
                        {
                            signature->returnType.recordIdentity->clangUsr = "nested-record";
                            argument.enumIdentity->clangUsr = "nested-enum";
                        }
                        signature->parameterTypes.push_back(std::move(argument));
                        nested->function = std::move(signature);
                        field.type.element = std::move(nested);
                    }
                    auto nestedGroups = groupCppSemanticRecords(nestedEntries);
                    if (nestedGroups.groups.size() == 1)
                    {
                        nestedGroups.groups.front().status = CppRecordGroupStatus::MultipleDefinitionsPointerCompatible;
                        nestedEntries.front().microsoftVolatileSemantics = true;
                        nestedEntries.back().microsoftVolatileSemantics = false;
                        const auto nestedResult = captureCppCRecordSnapshot(nestedEntries, nestedGroups.groups.front());
                        if (const auto* failure = std::get_if<CppCRecordSnapshotFailure>(&nestedResult))
                            std::cerr << "nested C snapshot: " << failure->message << '\n';
                        const auto* nestedSnapshot = std::get_if<CppCRecordSnapshot>(&nestedResult);
                        const auto* function = nestedSnapshot && nestedSnapshot->fields.front().type.element ?
                            nestedSnapshot->fields.front().type.element->function.get() : nullptr;
                        const auto& original = nestedEntries.front().declaration.fields.front().type.element->function;
                        ok &= expect(function && function->returnType.recordIdentity->clangUsr == "nested-record" &&
                                     function->parameterTypes.front().enumIdentity->clangUsr == "nested-enum" &&
                                     !original->returnType.recordIdentity->clangUsr &&
                                     !original->parameterTypes.front().enumIdentity->clangUsr,
                                     "C record snapshot reconciles nested identities without changing observations");
                        ok &= expect(nestedSnapshot && !nestedSnapshot->microsoftVolatileSemantics,
                                     "C record snapshot does not invent a conflicting volatile model");
                        ok &= expect(nestedSnapshot && !nestedSnapshot->valueLayoutUsable,
                                     "repeated C record observations do not claim value layout");
                        nestedEntries.back().microsoftVolatileSemantics = true;
                        const auto agreedResult = captureCppCRecordSnapshot(nestedEntries, nestedGroups.groups.front());
                        const auto* agreed = std::get_if<CppCRecordSnapshot>(&agreedResult);
                        std::vector<std::uint8_t> volatileBytes;
                        std::vector<CppCRecordSnapshot> volatileDecoded;
                        std::string volatileError;
                        const std::vector<CppCRecordSnapshot> agreedCatalog = agreed ?
                            std::vector<CppCRecordSnapshot>{*agreed} : std::vector<CppCRecordSnapshot>{};
                        ok &= expect(agreed && agreed->microsoftVolatileSemantics == true &&
                                     metadata_detail::encodeCRecordCatalog(agreedCatalog, volatileBytes, volatileError) &&
                                     metadata_detail::decodeCRecordCatalog(volatileBytes, volatileDecoded, volatileError) &&
                                     volatileDecoded.size() == 1 && volatileDecoded[0].microsoftVolatileSemantics == true &&
                                     !volatileDecoded[0].valueLayoutUsable,
                                     "C record snapshot and codec preserve a consistent volatile model");
                    }
                    else ok &= expect(false, "C record snapshot groups nested identity observations");
                }
            }
        }
        cGlobals = c->globalsSnapshot();
        const auto* global = cGlobals ? globalByName(*cGlobals, "c_global") : nullptr;
        const auto* local = cGlobals ? globalByName(*cGlobals, "c_local") : nullptr;
        const auto* volatileGlobal = cGlobals ? globalByName(*cGlobals, "c_volatile") : nullptr;
        const auto* qualifiedFloat = cGlobals ? globalByName(*cGlobals, "c_float") : nullptr;
        ok &= expect(global && global->externalSymbol == "c_global" && global->type.sizeBits == 32 &&
                     global->location && std::count_if(cGlobals->begin(), cGlobals->end(), [](const auto& value) {
                         return value.qualifiedName == "c_global";
                     }) == 1, "C global declaration preserves symbol, type and canonical identity");
        ok &= expect(local && !local->externalLinkage && !local->externalSymbol && local->type.isConst,
                     "internal C global preserves const without an external symbol");
        const auto globalMapping = global ? mapCppScalarType(global->type) : std::nullopt;
        const auto constMapping = local ? mapCppScalarType(local->type) : std::nullopt;
        const auto volatileMapping = volatileGlobal ? mapCppScalarType(volatileGlobal->type) : std::nullopt;
        const auto floatMapping = qualifiedFloat ? mapCppScalarType(qualifiedFloat->type) : std::nullopt;
        ok &= expect(globalMapping && globalMapping->nativeSharpTypeKey == "builtin:int" &&
                     !globalMapping->isConst && !globalMapping->isVolatile && constMapping &&
                     constMapping->nativeSharpTypeKey == "builtin:int" && constMapping->isConst &&
                     volatileMapping && volatileMapping->nativeSharpTypeKey == "builtin:int" &&
                     volatileMapping->isVolatile && floatMapping && floatMapping->isConst &&
                     floatMapping->isVolatile && floatMapping->nativeSharpTypeKey == "builtin:float",
                     "scalar representation mapping retains const and volatile from Clang");
        const auto* pointerRead = cFunctions ? functionByName(*cFunctions, "ptr_read") : nullptr;
        const auto* invoke = cFunctions ? functionByName(*cFunctions, "c_invoke") : nullptr;
        const auto projectedInvoke = invoke ? projectCppSemanticFunction(*invoke, 0, "Semantic")
                                             : CppSemanticFunctionResult{CppSemanticFunctionFailure{"fixture unavailable"}};
        const auto* invokeSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projectedInvoke);
        const auto callbackIdentity = invokeSymbol && !invokeSymbol->parameters.empty()
            ? nativesharp::language::parseFunctionTypeIdentity(invokeSymbol->parameters[0].typeKey) : std::nullopt;
        ok &= expect(callbackIdentity && callbackIdentity->callingConvention == nativesharp::language::CallingConvention::C &&
                     callbackIdentity->result == "builtin:int" && callbackIdentity->parameters.size() == 1 &&
                     callbackIdentity->parameters[0] == "builtin:int",
                     "C callback parameter retains its structural function signature");
        const auto* pointerConst = cFunctions ? functionByName(*cFunctions, "ptr_const") : nullptr;
        const auto* pointerVolatile = cFunctions ? functionByName(*cFunctions, "ptr_volatile") : nullptr;
        const auto* pointerNested = cFunctions ? functionByName(*cFunctions, "ptr_nested") : nullptr;
        const auto* pointerVoid = cFunctions ? functionByName(*cFunctions, "ptr_void") : nullptr;
        const auto* noPrototype = cFunctions ? functionByName(*cFunctions, "c_no_prototype") : nullptr;
        const auto* voidPrototype = cFunctions ? functionByName(*cFunctions, "c_void_prototype") : nullptr;
        const auto noPrototypeProjection = noPrototype ? projectCppSemanticFunction(*noPrototype, 0, "Semantic")
            : CppSemanticFunctionResult{CppSemanticFunctionFailure{"fixture unavailable"}};
        const auto voidPrototypeProjection = voidPrototype ? projectCppSemanticFunction(*voidPrototype, 0, "Semantic")
            : CppSemanticFunctionResult{CppSemanticFunctionFailure{"fixture unavailable"}};
        const auto* noPrototypeSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&noPrototypeProjection);
        const auto* voidPrototypeSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&voidPrototypeProjection);
        ok &= expect(noPrototype && !noPrototype->hasPrototype && noPrototypeSymbol && noPrototypeSymbol->isVariadic &&
                     voidPrototype && voidPrototype->hasPrototype && voidPrototypeSymbol && !voidPrototypeSymbol->isVariadic &&
                     noPrototypeSymbol->parameters.empty() && voidPrototypeSymbol->parameters.empty(),
                     "C no-prototype declaration differs from a void parameter prototype");
        for (const char* name : {"c_refined", "c_reverse"})
        {
            const auto* declaration = cFunctions ? functionByName(*cFunctions, name) : nullptr;
            const auto projection = declaration ? projectCppSemanticFunction(*declaration, 0, "Semantic")
                : CppSemanticFunctionResult{CppSemanticFunctionFailure{"fixture unavailable"}};
            const auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projection);
            ok &= expect(declaration && declaration->hasPrototype && symbol && !symbol->isVariadic &&
                         symbol->parameters.size() == 1 && symbol->parameters[0].typeKey == "builtin:int",
                         "compatible C redeclarations retain a known prototype regardless of order");
        }
        const auto* pointerNestedConst = cFunctions ? functionByName(*cFunctions, "ptr_nested_const") : nullptr;
        const auto projectedVolatile = pointerVolatile
            ? projectCppSemanticFunction(*pointerVolatile, 0, "Semantic")
            : CppSemanticFunctionResult{CppSemanticFunctionFailure{"fixture unavailable"}};
        const auto* volatileSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projectedVolatile);
        ok &= expect(volatileSymbol && volatileSymbol->parameters.size() == 1 &&
                     volatileSymbol->parameters[0].typeKey == "builtin:int*" &&
                     volatileSymbol->parameters[0].pointerPointeeVolatile &&
                     volatileSymbol->cppParameterTypes.size() == 1 &&
                     volatileSymbol->cppParameterTypes[0].element &&
                     volatileSymbol->cppParameterTypes[0].element->isVolatile,
                     "C volatile scalar pointer input retains the original Clang qualifier");
        ok &= expect(pointerNestedConst &&
                     std::holds_alternative<CppSemanticFunctionFailure>(projectCppSemanticFunction(*pointerNestedConst, 0, "Semantic")),
                     "nested qualified pointees are not silently mapped to mutable pointers");
        const auto* volatileResult = cFunctions ? functionByName(*cFunctions, "ptr_volatile_result") : nullptr;
        const auto projectedVolatileResult = volatileResult
            ? projectCppSemanticFunction(*volatileResult, 0, "Semantic")
            : CppSemanticFunctionResult{CppSemanticFunctionFailure{"fixture unavailable"}};
        const auto* resultSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projectedVolatileResult);
        ok &= expect(resultSymbol && resultSymbol->declaredTypeKey == "builtin:int*" &&
                     resultSymbol->cppReturnType && resultSymbol->cppReturnType->element &&
                     resultSymbol->cppReturnType->element->isVolatile,
                     "C volatile scalar pointer result retains the original Clang qualifier");
        for (const auto* name : {"ptr_volatile_nested"})
        {
            const auto* declaration = cFunctions ? functionByName(*cFunctions, name) : nullptr;
            ok &= expect(declaration && std::holds_alternative<CppSemanticFunctionFailure>(
                             projectCppSemanticFunction(*declaration, 0, "Semantic")),
                         "nested volatile pointer remains deferred");
        }
        if (pointerRead && volatileSymbol)
        {
            const auto projectedRead = projectCppSemanticFunction(*pointerRead, 0, "Semantic");
            const auto* readSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projectedRead);
            for (int scenario = 0; scenario < 2; ++scenario)
            {
                nativesharp::frontend::NamespaceStateSymbol state;
                state.projectOrdinal = 0;
                state.moduleName = "Semantic";
                state.name = "volatile_ptr";
                state.declaredTypeKey = "builtin:int*";
                state.storageName = "volatile_ptr";
                state.isCppExternal = true;
                state.cppPointerPointeeVolatile = true;
                state.accessibility = nativesharp::frontend::NamespaceFunctionAccessibility::Public;
                nativesharp::frontend::ProjectCompilation check;
                check.setSourceBufferProvider([scenario](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() != "Main.ns") return std::nullopt;
                    return scenario == 0 ? "int Main() { return ptr_volatile(volatile_ptr); }\n"
                                         : "int Main() { return ptr_read(volatile_ptr); }\n";
                });
                std::string error;
                const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                const bool configured = readSymbol && check.setCppImportedFunctions({*volatileSymbol, *readSymbol}, error) &&
                    check.setExperimentalCppImportedStates({state}, error);
                const bool ready = configured && check.compileProjectMethodBodies(path, error);
                bool expectedDiagnostic = false;
                for (const auto& diagnostic : check.diagnostics().diagnostics())
                    expectedDiagnostic |= diagnostic.message().find("no viable overload") != std::string::npos;
                ok &= expect(configured && (scenario == 0 ? ready : !ready && expectedDiagnostic),
                             "volatile pointee call requires a matching parameter contract");
            }
        }
        if (pointerConst)
        {
            const auto projectedConst = projectCppSemanticFunction(*pointerConst, 0, "Semantic");
            const auto* constSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projectedConst);
            ok &= expect(constSymbol && constSymbol->parameters.size() == 1 &&
                         constSymbol->parameters[0].typeKey == "builtin:int*" &&
                         constSymbol->parameters[0].pointerPointeeReadonly,
                         "C const pointer parameter preserves pointee readonly");
            if (constSymbol && pointerRead)
            {
                const auto mutableProjection = projectCppSemanticFunction(*pointerRead, 0, "Semantic");
                const auto* mutableSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&mutableProjection);
                for (const bool readonlyTarget : {true, false})
                {
                    nativesharp::frontend::ProjectCompilation compilation;
                    compilation.setSourceBufferProvider([readonlyTarget](const std::filesystem::path& path)
                        -> std::optional<std::string> {
                        if (path.filename() == "Main.ns")
                            return readonlyTarget
                                ? "int Main() { int value = 41; int const* p = &value; return ptr_const(p); }\n"
                                : "int Main() { int value = 41; int const* p = &value; return ptr_read(p); }\n";
                        return std::nullopt;
                    });
                    std::string error;
                    const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                    const bool configured = mutableSymbol && compilation.setCppImportedFunctions(
                        {*constSymbol, *mutableSymbol}, error);
                    const bool ready = configured && (readonlyTarget
                        ? compilation.compileProjectOopMir(path, error)
                        : compilation.compileProjectMethodBodies(path, error));
                    bool expectedDiagnostic = false;
                    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
                        expectedDiagnostic |= diagnostic.message().find("no viable overload of namespace function 'ptr_read'") !=
                            std::string::npos;
                    ok &= expect(configured && (readonlyTarget ? ready && !compilation.diagnostics().hasErrors()
                                                                : !ready && expectedDiagnostic),
                                 readonlyTarget ? "readonly pointer argument calls a const C parameter"
                                                : "readonly pointer argument cannot call a mutable C parameter");
                    if (readonlyTarget && ready && compilation.nameLookup() && compilation.oopMirModel())
                    {
                        nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                        auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
                        std::string llvmError;
                        const bool valid = module && module->verify(llvmError);
                        auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(llvmError);
                        const bool emitted = valid && target && target->emit(*module,
                            std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/const-pointer-call.obj",
                            nativesharp::llvm_backend::LlvmCodeGenFileType::Object, llvmError);
                        if (!emitted) std::cerr << "Const pointer LLVM/object failed: " << lowerer.error() << ' ' << llvmError << '\n';
                        ok &= expect(emitted, "readonly pointer call emits a verified Windows x64 object");
                    }
                }
            }
        }
        else ok &= expect(false, "Clang exposes the const pointer parameter fixture");
        if (pointerVoid)
        {
            const auto projectedVoid = projectCppSemanticFunction(*pointerVoid, 0, "Semantic");
            const auto* voidSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projectedVoid);
            ok &= expect(voidSymbol && voidSymbol->declaredTypeKey == "builtin:void*" &&
                         voidSymbol->parameters.size() == 1 && voidSymbol->parameters[0].typeKey == "builtin:void*",
                         "void pointer return and parameter retain pointer type");
        }
        else ok &= expect(false, "Clang exposes the void pointer fixture");
        if (pointerNested)
        {
            const auto nested = projectCppSemanticFunction(*pointerNested, 0, "Semantic");
            const auto* nestedSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&nested);
            ok &= expect(nestedSymbol && nestedSymbol->declaredTypeKey == "builtin:int**" &&
                         nestedSymbol->parameters.size() == 1 && nestedSymbol->parameters[0].typeKey == "builtin:int**",
                         "nested pointer return and parameter retain both levels");
        }
        else ok &= expect(false, "Clang exposes the nested pointer fixture");
        if (pointerRead)
        {
            const auto projectedPointer = projectCppSemanticFunction(*pointerRead, 0, "Semantic");
            const auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projectedPointer);
            ok &= expect(symbol && symbol->parameters.size() == 1 &&
                         symbol->parameters[0].typeKey == "builtin:int*",
                         "C pointer to int maps to a NativeSharp pointer parameter");
            if (symbol)
            {
                nativesharp::frontend::ProjectCompilation pointerCompilation;
                pointerCompilation.setSourceBufferProvider([](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() == "Main.ns")
                        return "int Main() { int value = 41; return ptr_read(&value); }\n";
                    return std::nullopt;
                });
                std::string error;
                const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                const bool ready = pointerCompilation.setCppImportedFunctions({*symbol}, error) &&
                    pointerCompilation.compileProjectOopMir(path, error);
                if (!ready) std::cerr << "C pointer MIR failed: " << error << '\n';
                ok &= expect(ready && pointerCompilation.oopMirModel(),
                             "C pointer call reaches MIR from NativeSharp");
                if (ready && pointerCompilation.oopMirModel() && pointerCompilation.nameLookup())
                {
                    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                    auto module = lowerer.lower(*pointerCompilation.nameLookup(), *pointerCompilation.oopMirModel());
                    std::string llvmError;
                    const bool valid = module && module->verify(llvmError);
                    const std::string ir = module ? module->text() : std::string{};
                    ok &= expect(valid && ir.find("declare i32 @ptr_read(ptr)") != std::string::npos &&
                                 ir.find("call i32 @ptr_read(ptr") != std::string::npos,
                                 "C pointer call reaches verified LLVM with pointer ABI");
                    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(llvmError);
                    const bool emitted = valid && target && target->emit(*module,
                        std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/pointer-call.obj",
                        nativesharp::llvm_backend::LlvmCodeGenFileType::Object, llvmError);
                    if (!emitted) std::cerr << "C pointer LLVM/object failed: " << lowerer.error() << ' ' << llvmError << '\n';
                    ok &= expect(emitted, "C pointer call emits a Windows x64 object");
                }
            }
        }
        else ok &= expect(false, "Clang exposes the pointer parameter fixture");
        const auto* add = cFunctions ? functionByName(*cFunctions, "c_add") : nullptr;
        if (add && add->externalSymbol && add->parameters.size() == 2)
        {
            ok &= expect(std::holds_alternative<CppSemanticFunctionFailure>(
                             projectCppSemanticFunction(*add, 0, "")),
                         "semantic conversion requires an owning module");
            auto projected = projectCppSemanticFunction(*add, 0, "Semantic");
            auto* projectedSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projected);
            ok &= expect(projectedSymbol != nullptr, "C function converts automatically for semantic indexing");
            if (projectedSymbol)
            {
                const auto imported = *projectedSymbol;
                {
                    nativesharp::frontend::ProjectCompilation bodies;
                    std::string bodyError;
                    const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                    const bool configured = bodies.setCppImportedFunctions({imported}, bodyError);
                    const bool ready = configured && bodies.compileProjectMethodBodies(path, bodyError);
                    if (!ready)
                        for (const auto& diagnostic : bodies.diagnostics().diagnostics())
                            std::cerr << "C++ import body diagnostic: " << diagnostic.message() << '\n';
                    ok &= expect(configured && ready && !bodies.diagnostics().hasErrors(),
                                 "Clang function is recognized as external in body analysis");
                }
                {
                    nativesharp::frontend::ProjectCompilation hir;
                    std::string hirError;
                    const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                    const bool ready = hir.setCppImportedFunctions({imported}, hirError) &&
                        hir.compileProjectOopHir(path, hirError);
                    bool found = false;
                    if (ready && hir.oopHirModel())
                        for (const auto& function : hir.oopHirModel()->functions())
                            found |= function.isImportedC && function.mangledName == "c_add" &&
                                function.cAbiName == "c_add";
                    if (!ready) std::cerr << "Clang C HIR setup failed: " << hirError << '\n';
                    ok &= expect(found, "Clang C external symbol reaches HIR as a declaration");
                    nativesharp::frontend::ProjectCompilation mir;
                    const bool mirReady = mir.setCppImportedFunctions({imported}, hirError) &&
                        mir.compileProjectOopMir(path, hirError);
                    bool mirFound = false;
                    if (mirReady && mir.oopMirModel())
                        for (const auto& function : mir.oopMirModel()->functions())
                            mirFound |= function.isImportedC && function.mangledName == "c_add" &&
                                function.cAbiName == "c_add";
                    if (!mirReady) std::cerr << "Clang C MIR setup failed: " << hirError << '\n';
                    ok &= expect(mirFound, "Clang C external symbol reaches MIR as a declaration");
                    if (mirFound && mir.nameLookup())
                    {
                        nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                        auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
                        std::string verifyError;
                        const bool valid = module && module->verify(verifyError);
                        const std::string ir = module ? module->text() : std::string{};
                        if (!valid) std::cerr << "Clang C LLVM failed: " << lowerer.error() << ' ' << verifyError << '\n';
                        ok &= expect(valid && ir.find("declare i32 @c_add(i32, i32)") != std::string::npos &&
                                     ir.find("call i32 @c_add(") != std::string::npos,
                                     "Clang C external declaration and call reach verified LLVM");
                        auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(verifyError);
                        const bool emitted = valid && target && target->emit(*module,
                            std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/c-call.obj",
                            nativesharp::llvm_backend::LlvmCodeGenFileType::Object, verifyError);
                        if (!emitted) std::cerr << "Clang C object failed: " << verifyError << '\n';
                        ok &= expect(emitted, "Clang C call emits a Windows x64 object");
                    }
                }
                {
                    nativesharp::frontend::ProjectCompilation ordinaryPrototype;
                    ordinaryPrototype.setSourceBufferProvider([](const std::filesystem::path& path)
                        -> std::optional<std::string> {
                        if (path.filename() == "Main.ns")
                            return "int missing(int value); int Main() { return missing(1); }\n";
                        return std::nullopt;
                    });
                    std::string bodyError;
                    const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                    const bool ready = ordinaryPrototype.compileProjectMethodBodies(path, bodyError);
                    bool missingDefinition = false;
                    for (const auto& diagnostic : ordinaryPrototype.diagnostics().diagnostics())
                        missingDefinition |= diagnostic.id() == nativesharp::diagnostics::DiagnosticId::MissingFunctionDefinition;
                    ok &= expect(!ready && missingDefinition,
                                 "ordinary NativeSharp prototype still requires a definition when called");
                }
                nativesharp::frontend::ProjectCompilation compilation;
                std::string error;
                const auto semanticPath = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                {
                    nativesharp::frontend::ProjectCompilation enumCompilation;
                    enumCompilation.setSourceBufferProvider([](const std::filesystem::path& path)
                        -> std::optional<std::string> {
                        if (path.filename() == "Main.ns")
                            return "int Main() { switch (0) { case CMinus: return 7; default: return 0; } }\n";
                        return std::nullopt;
                    });
                    nativesharp::frontend::NamespaceStateSymbol constant;
                    constant.projectOrdinal = 0;
                    constant.moduleName = "Semantic";
                    constant.name = "CMinus";
                    constant.declaredTypeKey = "builtin:int";
                    constant.storageName = "__nsc_cenum_8_Semantic_6_CMinus";
                    constant.isConst = true;
                    constant.isCppEnumConstant = true;
                    constant.initializerKind = nativesharp::frontend::NamespaceStateInitializerKind::Integer;
                    constant.integerValue = 0xfffffffdULL;
                    const bool configured = enumCompilation.setExperimentalCppImportedStates({constant}, error);
                    const bool ready = configured && enumCompilation.compileProjectMethodBodies(semanticPath, error);
                    if (!ready) std::cerr << "C enum switch setup failed: " << error << '\n';
                    ok &= expect(ready, "imported C enumerator is accepted as a switch constant");
                }
                {
                    nativesharp::frontend::ProjectCompilation aliasCompilation;
                    aliasCompilation.setSourceBufferProvider([](const std::filesystem::path& path)
                        -> std::optional<std::string> {
                        if (path.filename() == "Main.ns")
                            return "Count Echo(Count value) { return value; } int Use(RestrictedPtr value, PtrToRestricted nested, IncompleteArray* unknown) { return 0; } int Main() { Matrix value; return 0; }\n";
                        return std::nullopt;
                    });
                    bool supplied = false;
                    const auto snapshot = cAliases;
                    const bool configured = aliasCompilation.setCppTypeAliasProvider(
                        [snapshot, &supplied](const auto& graph, auto, auto& output, auto& why) {
                            if (!snapshot || graph.projects().empty()) { why = "alias fixture unavailable"; return false; }
                            for (const auto& alias : *snapshot)
                                if (alias.name == "Count" || alias.name == "Matrix" || alias.name == "RestrictedPtr" || alias.name == "PtrToRestricted" || alias.name == "IncompleteArray")
                                {
                                    output.push_back({0, graph.projects()[0].projectFile(), 0,
                                                      "x86_64-pc-windows-msvc", alias});
                                    supplied = true;
                                }
                            return supplied;
                        }, error);
                    const bool bound = configured && aliasCompilation.compileProjectNames(semanticPath, error);
                    bool expanded = false, matrixExpanded = false, restrictExpanded = false, nestedRestrictExpanded = false, incompleteExpanded = false;
                    if (bound && aliasCompilation.nameLookup())
                        for (const auto& binding : aliasCompilation.nameLookup()->bindings())
                        {
                            expanded |= binding.writtenName == "Count" && binding.syntax &&
                                binding.syntax->baseTokenKind() == nativesharp::lexer::TokenKind::KwUInt;
                            if (binding.syntax && binding.writtenName == "RestrictedPtr")
                                restrictExpanded |= binding.syntax->modifiers().size() == 1 &&
                                    binding.syntax->modifiers()[0].importedCRestrict;
                            if (binding.syntax && binding.writtenName == "PtrToRestricted")
                                nestedRestrictExpanded |= binding.syntax->modifiers().size() == 2 &&
                                    binding.syntax->modifiers()[0].importedCRestrict &&
                                    !binding.syntax->modifiers()[1].importedCRestrict;
                            if (binding.syntax && binding.writtenName == "IncompleteArray")
                                incompleteExpanded |= binding.syntax->modifiers().size() == 2 &&
                                    binding.syntax->modifiers()[0].kind == nativesharp::ast::TypeModifierSyntaxKind::IncompleteArray &&
                                    binding.syntax->modifiers()[1].kind == nativesharp::ast::TypeModifierSyntaxKind::Pointer;
                            if (binding.writtenName == "Matrix" && binding.syntax)
                            {
                                const auto& modifiers = binding.syntax->modifiers();
                                matrixExpanded |= modifiers.size() == 2 && modifiers[0].arrayCount && modifiers[1].arrayCount &&
                                    modifiers[0].arrayCount->integerValue() == 3 && modifiers[1].arrayCount->integerValue() == 2;
                            }
                        }
                    if (!bound) std::cerr << "C typedef semantic setup failed: " << error << '\n';
                    ok &= expect(bound && supplied && expanded && matrixExpanded,
                                 "scalar and multidimensional typedefs bind to underlying NativeSharp types");
                    ok &= expect(bound && restrictExpanded && nestedRestrictExpanded,
                                 "typedef expansion retains restrict on the correct pointer level");
                    ok &= expect(bound && incompleteExpanded, "pointer to incomplete C array retains the unknown bound");
                }
                {
                    nativesharp::frontend::ProjectCompilation recordAliasCompilation;
                    recordAliasCompilation.setSourceBufferProvider([](const std::filesystem::path& path)
                        -> std::optional<std::string> {
                        if (path.filename() == "Main.ns")
                            return "PairAlias* Echo(Pair* value) { return value; } int Use(PairMatrix value) { return 0; } int Main() { return 0; }\n";
                        return std::nullopt;
                    });
                    const auto* captured = cRecords ? recordByName(*cRecords, "Pair") : nullptr;
                    const auto recordSnapshot = captured ? std::optional<CppRecordDeclaration>(*captured) : std::nullopt;
                    const auto aliasesSnapshot = cAliases;
                    const bool configured = recordAliasCompilation.setExperimentalCppTypeProvider(
                        [recordSnapshot](const auto& graph, auto, auto& output, auto& why) {
                            if (!recordSnapshot || graph.projects().empty()) { why = "record fixture unavailable"; return false; }
                            const auto identity = projectCppRecordIdentity(*recordSnapshot, graph.projects()[0].moduleName());
                            if (!identity) { why = "record identity unavailable"; return false; }
                            output.push_back({*identity, nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                                nativesharp::frontend::NamedTypeAccessibility::Public, 0, "probe.c"});
                            output.back().clangUsr = recordSnapshot->clangUsr;
                            if (recordSnapshot->sizeBits && recordSnapshot->alignmentBits)
                                output.back().objectLayout = nativesharp::frontend::CppImportedObjectLayout{
                                    *recordSnapshot->sizeBits / 8, *recordSnapshot->alignmentBits / 8};
                            return true;
                        }, error) && recordAliasCompilation.setCppTypeAliasProvider(
                        [aliasesSnapshot](const auto& graph, auto, auto& output, auto& why) {
                            if (!aliasesSnapshot || graph.projects().empty()) { why = "alias fixture unavailable"; return false; }
                            for (const auto& alias : *aliasesSnapshot)
                                if (alias.name == "Pair" || alias.name == "PairAlias" || alias.name == "PairMatrix")
                                    output.push_back({0, graph.projects()[0].projectFile(), 0,
                                                      "x86_64-pc-windows-msvc", alias});
                            return true;
                        }, error);
                    const bool bound = configured && recordAliasCompilation.compileProjectNames(semanticPath, error);
                    bool aliasBound = false, tagBound = false, arrayBound = false;
                    if (bound && recordAliasCompilation.nameLookup())
                        for (const auto& binding : recordAliasCompilation.nameLookup()->bindings())
                        {
                            aliasBound |= binding.writtenName == "PairAlias" && binding.syntax &&
                                binding.syntax->baseName() == "Pair";
                            tagBound |= binding.writtenName == "Pair";
                            if (binding.writtenName == "PairMatrix" && binding.syntax)
                            {
                                const auto& modifiers = binding.syntax->modifiers();
                                arrayBound |= binding.syntax->baseName() == "Pair" && binding.targetTypeOrdinal &&
                                    modifiers.size() == 2 && modifiers[0].arrayCount && modifiers[1].arrayCount &&
                                    modifiers[0].arrayCount->integerValue() == 3 && modifiers[1].arrayCount->integerValue() == 2;
                            }
                        }
                    if (!bound) std::cerr << "C record typedef semantic setup failed: " << error << '\n';
                    ok &= expect(bound && aliasBound && tagBound,
                                 "record typedef and same-named C tag bind to the imported record");
                    ok &= expect(bound && arrayBound, "record array typedef preserves identity and both dimensions");
                    const auto* outer = cRecords ? recordByName(*cRecords, "OuterPair") : nullptr;
                    const auto nested = outer && recordAliasCompilation.nameLookup()
                        ? projectCppSemanticInstanceField(*outer, 0, 0, "Semantic", "x86_64-pc-windows-msvc",
                                                          recordAliasCompilation.nameLookup()) : std::nullopt;
                    const auto nestedArray = outer && recordAliasCompilation.nameLookup()
                        ? projectCppSemanticInstanceField(*outer, 1, 0, "Semantic", "x86_64-pc-windows-msvc",
                                                          recordAliasCompilation.nameLookup()) : std::nullopt;
                    const auto pairIdentity = recordSnapshot ? projectCppRecordIdentity(*recordSnapshot, "Semantic") : std::nullopt;
                    ok &= expect(nested && pairIdentity && nested->type.nativeSharpTypeKey == "type:" + pairIdentity->key() &&
                                 nested->byteOffset == 0 && nestedArray &&
                                 nestedArray->type.nativeSharpTypeKey == "type:" + pairIdentity->key() + "[3][2]" &&
                                 nestedArray->byteOffset == 8,
                                 "nested C record and array fields retain imported identities and offsets");
                    if (outer && nested && !outer->fields.empty())
                    {
                        const auto imported = toCppImportedInstanceField(*nested);
                        auto withoutUsr = *outer;
                        withoutUsr.fields[0].clangUsr = std::nullopt;
                        const auto unknown = projectCppSemanticInstanceField(withoutUsr, 0, 0, "Semantic",
                            "x86_64-pc-windows-msvc", recordAliasCompilation.nameLookup());
                        ok &= expect(outer->fields[0].clangUsr && nested->clangUsr == outer->fields[0].clangUsr &&
                                     imported.clangUsr == nested->clangUsr && unknown && !unknown->clangUsr &&
                                     !toCppImportedInstanceField(*unknown).clangUsr,
                                     "C field projection preserves present and absent Clang USRs");
                    }
                    else ok &= expect(false, "C field USR projection requires a projected field");
                    if (outer && !outer->fields.empty() && outer->fields[0].type.recordIdentity &&
                        recordAliasCompilation.nameLookup())
                    {
                        auto different = *outer;
                        different.fields[0].type.recordIdentity->clangUsr = "different-record-usr";
                        ok &= expect(recordSnapshot && recordSnapshot->clangUsr &&
                            !projectCppSemanticInstanceField(different, 0, 0, "Semantic", "x86_64-pc-windows-msvc",
                                                             recordAliasCompilation.nameLookup()),
                            "record field projection rejects a different known declaration USR");
                    }
                    else ok &= expect(false, "record field identity fixture is available");
                    const auto* device = cRecords ? recordByName(*cRecords, "CDevice") : nullptr;
                    const auto volatileField = device ? projectCppSemanticInstanceField(
                        *device, 0, 0, "Semantic", "x86_64-pc-windows-msvc", recordAliasCompilation.nameLookup()) : std::nullopt;
                    ok &= expect(volatileField && volatileField->type.isVolatile &&
                                 toCppImportedInstanceField(*volatileField).isVolatile,
                                 "C volatile field retains its qualifier through semantic projection");
                    nativesharp::frontend::ProjectCompilation wrongAliasCompilation;
                    wrongAliasCompilation.setSourceBufferProvider([](const std::filesystem::path& path)
                        -> std::optional<std::string> {
                        if (path.filename() == "Main.ns") return "int Use(PairAlias* value) { return 0; }\n";
                        return std::nullopt;
                    });
                    const bool wrongConfigured = wrongAliasCompilation.setExperimentalCppTypeProvider(
                        [recordSnapshot](const auto& graph, auto, auto& output, auto& why) {
                            if (!recordSnapshot || !recordSnapshot->clangUsr || graph.projects().empty())
                            { why = "record USR fixture unavailable"; return false; }
                            const auto identity = projectCppRecordIdentity(*recordSnapshot, graph.projects()[0].moduleName());
                            if (!identity) return false;
                            output.push_back({*identity, nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                                nativesharp::frontend::NamedTypeAccessibility::Public, 0, "probe.c"});
                            output.back().clangUsr = recordSnapshot->clangUsr;
                            return true;
                        }, error) && wrongAliasCompilation.setCppTypeAliasProvider(
                        [aliasesSnapshot](const auto& graph, auto, auto& output, auto& why) {
                            if (!aliasesSnapshot || graph.projects().empty()) { why = "alias fixture unavailable"; return false; }
                            for (auto alias : *aliasesSnapshot)
                                if (alias.name == "PairAlias" && alias.underlyingType.recordIdentity)
                                {
                                    alias.underlyingType.recordIdentity->clangUsr = "different-record-usr";
                                    output.push_back({0, graph.projects()[0].projectFile(), 0,
                                                      "x86_64-pc-windows-msvc", std::move(alias)});
                                }
                            return true;
                        }, error);
                    const bool wrongBound = wrongConfigured && wrongAliasCompilation.compileProjectNames(semanticPath, error);
                    ok &= expect(wrongConfigured && (!wrongBound || wrongAliasCompilation.diagnostics().hasErrors()),
                                 "alias resolution rejects an imported record with a different known USR");
                }
                {
                    nativesharp::frontend::ProjectCompilation enumAliasCompilation;
                    enumAliasCompilation.setSourceBufferProvider([](const std::filesystem::path& path)
                        -> std::optional<std::string> {
                        if (path.filename() == "Main.ns")
                            return "struct Holder { ColorAlias shade; int count; ColorMatrix matrix; } ColorAlias Echo(ColorAlias value) { return value; } int Use(ColorCallback cb) { return 0; } int UsePtr(ColorCallback* cb) { return 0; } int UseNested(NestedCallback cb) { return 0; } int Main() { return 0; }\n";
                        return std::nullopt;
                    });
                    nativesharp::compilation::TypeIdentity enumIdentity("Semantic", {}, {}, "Color");
                    nativesharp::frontend::CppImportedTypeDeclaration enumType{enumIdentity,
                        nativesharp::frontend::NamedTypeDeclarationKind::Enum,
                        nativesharp::frontend::NamedTypeAccessibility::Public, 0, "probe.c"};
                    enumType.enumBase = nativesharp::frontend::CppImportedEnumBase{
                        nativesharp::lexer::TokenKind::KwInt, "int"};
                    enumType.objectLayout = nativesharp::frontend::CppImportedObjectLayout{4, 4};
                    if (color) enumType.clangUsr = color->clangUsr;
                    const auto aliasesSnapshot = cAliases;
                    const bool configured = enumAliasCompilation.setExperimentalCppImportedTypes({enumType}, error) &&
                        enumAliasCompilation.setCppTypeAliasProvider(
                        [aliasesSnapshot](const auto& graph, auto, auto& output, auto& why) {
                            if (!aliasesSnapshot || graph.projects().empty()) { why = "alias fixture unavailable"; return false; }
                            for (const auto& alias : *aliasesSnapshot)
                                if (alias.name == "ColorAlias" || alias.name == "ColorMatrix" || alias.name == "ColorCallback" || alias.name == "NestedCallback")
                                    output.push_back({0, graph.projects()[0].projectFile(), 0,
                                                      "x86_64-pc-windows-msvc", alias});
                            return true;
                        }, error);
                    const bool bound = configured && enumAliasCompilation.compileProjectObjectLayouts(semanticPath, error);
                    bool enumBound = false, basePreserved = false, callbackAliasBound = false, callbackPointerBound = false, nestedAliasBound = false, matrixAliasBound = false;
                    if (bound && enumAliasCompilation.nameLookup())
                    {
                        for (const auto& binding : enumAliasCompilation.nameLookup()->bindings())
                        {
                            enumBound |= binding.writtenName == "ColorAlias" && binding.syntax &&
                                binding.syntax->baseName() == "Color" && binding.targetTypeOrdinal.has_value();
                            matrixAliasBound |= binding.writtenName == "ColorMatrix" && binding.syntax &&
                                binding.syntax->baseName() == "Color" && binding.syntax->modifiers().size() == 2 &&
                                binding.targetTypeOrdinal.has_value();
                            if (binding.writtenName == "ColorCallback" && binding.syntax)
                            {
                                const auto* type = binding.syntax;
                                callbackAliasBound |= type->isFunction() && type->functionReturnType() &&
                                    type->functionReturnType()->baseName() == "Color" &&
                                    type->typeArguments().size() == 1 && type->typeArguments()[0]->baseName() == "Color" &&
                                    type->functionCallingConvention() == nativesharp::ast::FunctionCallingConventionSyntaxKind::C;
                                callbackPointerBound |= type->isFunction() && type->functionReturnType() &&
                                    type->functionReturnType()->baseName() == "Color" &&
                                    type->typeArguments().size() == 1 && type->typeArguments()[0]->baseName() == "Color" &&
                                    type->functionCallingConvention() == nativesharp::ast::FunctionCallingConventionSyntaxKind::C &&
                                    type->modifiers().size() == 1 &&
                                    type->modifiers()[0].kind == nativesharp::ast::TypeModifierSyntaxKind::Pointer;
                            }
                            if (binding.writtenName == "NestedCallback" && binding.syntax &&
                                binding.syntax->isFunction() && binding.syntax->typeArguments().size() == 1)
                            {
                                const auto* inner = binding.syntax->typeArguments()[0];
                                nestedAliasBound |= inner && inner->isFunction() && inner->functionReturnType() &&
                                    inner->functionReturnType()->baseName() == "Color" &&
                                    inner->typeArguments().size() == 1 && inner->typeArguments()[0]->baseName() == "Color" &&
                                    inner->functionCallingConvention() == nativesharp::ast::FunctionCallingConventionSyntaxKind::C;
                            }
                        }
                        for (const auto& type : enumAliasCompilation.nameLookup()->namedTypes())
                            if (type.identity.name() == "Color")
                                basePreserved = nativesharp::frontend::enumUnderlyingTypeKey(type.syntax) == "builtin:int";
                    }
                    if (!bound) std::cerr << "C enum typedef semantic setup failed: " << error << '\n';
                    ok &= expect(bound && enumBound && basePreserved,
                                 "enum typedef binds to imported enum and preserves its underlying type");
                    ok &= expect(bound && callbackAliasBound,
                                 "callback typedef preserves imported enum in result and parameter");
                    ok &= expect(bound && callbackPointerBound,
                                 "pointer to callback typedef preserves imported enum signature and indirection");
                    ok &= expect(bound && nestedAliasBound,
                                 "nested callback typedef preserves inner signature and calling convention");
                    ok &= expect(bound && matrixAliasBound, "enum array typedef retains imported identity and dimensions");
                    const auto* echo = cFunctions ? functionByName(*cFunctions, "color_echo") : nullptr;
                    const CppRecordGroups noRecords;
                    const auto* restrictUse = cFunctions ? functionByName(*cFunctions, "restrict_use") : nullptr;
                    const auto restrictedProjection = restrictUse
                        ? projectCppSemanticFunction(*restrictUse, 0, "Semantic")
                        : CppSemanticFunctionResult{CppSemanticFunctionFailure{"fixture unavailable"}};
                    const auto* restrictedSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&restrictedProjection);
                    ok &= expect(restrictedSymbol && restrictedSymbol->cppReturnType &&
                                 restrictedSymbol->cppParameterTypes.size() == 1 && restrictedSymbol->parameters.size() == 1 &&
                                 restrictedSymbol->cppParameterTypes[0].isRestrict &&
                                 restrictedSymbol->cppParameterTypes[0].element &&
                                 !restrictedSymbol->cppParameterTypes[0].element->isRestrict &&
                                 restrictedSymbol->parameters[0].typeKey == "builtin:int*",
                                 "function projection retains restrict metadata without changing pointer representation");
                    const auto projected = echo && enumAliasCompilation.nameLookup()
                        ? projectCppSemanticFunctionWithRecords(*echo, 0, "Semantic", noRecords,
                                                               enumAliasCompilation.nameLookup())
                        : CppSemanticFunctionResult{CppSemanticFunctionFailure{"fixture unavailable"}};
                    const auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projected);
                    ok &= expect(symbol && symbol->declaredTypeKey == "type:" + enumIdentity.key() &&
                                 symbol->parameters.size() == 1 &&
                                 symbol->parameters[0].typeKey == symbol->declaredTypeKey,
                                 "C enum function preserves its imported type in parameter and result");
                    const auto* applyColor = cFunctions ? functionByName(*cFunctions, "apply_color") : nullptr;
                    const auto callbackProjection = applyColor && enumAliasCompilation.nameLookup()
                        ? projectCppSemanticFunctionWithRecords(*applyColor, 0, "Semantic", noRecords,
                                                               enumAliasCompilation.nameLookup())
                        : CppSemanticFunctionResult{CppSemanticFunctionFailure{"fixture unavailable"}};
                    const auto* callbackSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&callbackProjection);
                    const auto callback = callbackSymbol && !callbackSymbol->parameters.empty()
                        ? nativesharp::language::parseFunctionTypeIdentity(callbackSymbol->parameters[0].typeKey) : std::nullopt;
                    ok &= expect(callback && callback->result == "type:" + enumIdentity.key() &&
                                 callback->parameters.size() == 1 && callback->parameters[0] == callback->result &&
                                 callback->callingConvention == nativesharp::language::CallingConvention::C,
                                 "callback signature preserves the imported enum identity");
                    const auto* paint = cRecords ? recordByName(*cRecords, "Paint") : nullptr;
                    const auto shade = paint && enumAliasCompilation.nameLookup()
                        ? projectCppSemanticInstanceField(*paint, 0, 0, "Semantic", "x86_64-pc-windows-msvc",
                                                          enumAliasCompilation.nameLookup()) : std::nullopt;
                    const auto view = paint && enumAliasCompilation.nameLookup()
                        ? projectCppSemanticInstanceField(*paint, 1, 0, "Semantic", "x86_64-pc-windows-msvc",
                                                          enumAliasCompilation.nameLookup()) : std::nullopt;
                    const auto colors = paint && enumAliasCompilation.nameLookup()
                        ? projectCppSemanticInstanceField(*paint, 2, 0, "Semantic", "x86_64-pc-windows-msvc",
                                                          enumAliasCompilation.nameLookup()) : std::nullopt;
                    ok &= expect(shade && shade->type.nativeSharpTypeKey == "type:" + enumIdentity.key() &&
                                 view && view->type.nativeSharpTypeKey == "type:" + enumIdentity.key() + "*" &&
                                 view->pointeeReadonly && colors &&
                                 colors->type.nativeSharpTypeKey == "type:" + enumIdentity.key() + "[3][2]" &&
                                 colors->byteOffset == 16,
                                 "C enum fields preserve identity, readonly pointee and array dimensions");
                    const bool layoutsBuilt = bound;
                    const auto holderKey = nativesharp::compilation::TypeIdentity("Semantic", {}, {}, "Holder").key();
                    const auto holderOrdinal = layoutsBuilt && enumAliasCompilation.nameLookup()
                        ? enumAliasCompilation.nameLookup()->typeOrdinalForIdentity(holderKey) : std::nullopt;
                    const auto* holder = holderOrdinal && enumAliasCompilation.objectLayoutModel()
                        ? enumAliasCompilation.objectLayoutModel()->structLayoutForType(*holderOrdinal) : nullptr;
                    if (!holder || holder->size != 32 || holder->alignment != 4)
                    {
                        std::cerr << "C enum holder layout: built=" << layoutsBuilt << " error=" << error;
                        if (holder) std::cerr << " size=" << holder->size << " alignment=" << holder->alignment;
                        std::cerr << '\n';
                        for (const auto& diagnostic : enumAliasCompilation.diagnostics().diagnostics())
                            std::cerr << diagnostic.message() << '\n';
                    }
                    ok &= expect(holder && holder->size == 32 && holder->alignment == 4,
                                 "NativeSharp struct layout uses imported enum dimensions");
                }
                {
                    nativesharp::frontend::ProjectCompilation callbackCompilation;
                    callbackCompilation.setSourceBufferProvider([](const std::filesystem::path& path)
                        -> std::optional<std::string> {
                        if (path.filename() == "Main.ns")
                            return "int Use(Callback value) { return 0; } int Main() { return 0; }\n";
                        return std::nullopt;
                    });
                    const auto aliasesSnapshot = cAliases;
                    const bool configured = callbackCompilation.setCppTypeAliasProvider(
                        [aliasesSnapshot](const auto& graph, auto, auto& output, auto& why) {
                            if (!aliasesSnapshot || graph.projects().empty()) { why = "alias fixture unavailable"; return false; }
                            for (const auto& alias : *aliasesSnapshot)
                                if (alias.name == "Callback")
                                    output.push_back({0, graph.projects()[0].projectFile(), 0,
                                                      "x86_64-pc-windows-msvc", alias});
                            return true;
                        }, error);
                    const bool bound = configured && callbackCompilation.compileProjectNames(semanticPath, error);
                    bool callbackBound = false;
                    if (bound && callbackCompilation.nameLookup())
                        for (const auto& binding : callbackCompilation.nameLookup()->bindings())
                            if (binding.writtenName == "Callback" && binding.syntax)
                            {
                                const auto* type = binding.syntax;
                                callbackBound |= type->isFunction() && type->functionIsVariadic() &&
                                    type->functionCallingConvention() == nativesharp::ast::FunctionCallingConventionSyntaxKind::C &&
                                    type->functionReturnType() && type->functionReturnType()->baseName() == "int" &&
                                    type->typeArguments().size() == 1 && type->typeArguments()[0]->isPointeeConst();
                            }
                    if (!bound) std::cerr << "C callback typedef semantic setup failed: " << error << '\n';
                    ok &= expect(bound && callbackBound, "function pointer typedef binds to a structural C function type");
                }
                auto incompleteProjection = imported;
                incompleteProjection.mangledName.clear();
                ok &= expect(!compilation.setCppImportedFunctions({incompleteProjection}, error) &&
                             !compilation.setCppImportedFunctions({imported, imported}, error) &&
                             compilation.setCppImportedFunctions({imported}, error),
                             "compilation validates imported symbols before starting");
                const bool nativeReady = compilation.compileProjectMembers(semanticPath, error);
                if (!nativeReady) std::cerr << "Native semantic setup failed: " << error << '\n';
                ok &= expect(nativeReady && compilation.nameLookup() && compilation.memberModel(),
                             "ordinary project provides semantic models for C import test");
                if (nativeReady && compilation.nameLookup() && compilation.memberModel())
                {
                    const auto* compiledModel = compilation.namespaceFunctionModel();
                    ok &= expect(compiledModel && compiledModel->findFunctions(0, "", "c_add").size() == 1 &&
                                 !compilation.setCppImportedFunctions({}, error),
                                 "project compilation installs imported functions and rejects late changes");
                    nativesharp::frontend::ProjectNamespaceFunctionModel model;
                    auto incomplete = imported; incomplete.mangledName.clear();
                    ok &= expect(!model.setCppImportedFunctions({incomplete}, error),
                                 "semantic index rejects incomplete imported symbols");
                    ok &= expect(!model.setCppImportedFunctions({imported, imported}, error),
                                 "semantic index rejects duplicate Clang identities");
                    ok &= expect(model.setCppImportedFunctions({imported}, error) &&
                                 model.build(*compilation.nameLookup(), *compilation.memberModel(),
                                             compilation.units(), compilation.diagnostics(), compilation.genericModel()),
                                 "semantic model builds with a function projected from Clang");
                    const auto ordinals = model.findFunctions(0, "", "c_add");
                    ok &= expect(ordinals.size() == 1 && ordinals[0] < model.functions().size() &&
                                 model.functions()[ordinals[0]].mangledName == "c_add" &&
                                 model.functions()[ordinals[0]].cppCallingConvention == "cdecl" &&
                                 model.functions()[ordinals[0]].cppCanonicalIdentity.starts_with("c_add|"),
                                 "semantic lookup finds imported C function and retains its symbol");
                    ok &= expect(model.findFunctions(0, "", "Main").size() == 1,
                                 "importing a function preserves ordinary NativeSharp functions");
                    const auto bindings = model.referenceBindings();
                    const auto bound = std::find_if(bindings.begin(), bindings.end(), [](const auto& binding) {
                        return binding.writtenName == "c_add";
                    });
                    ok &= expect(bound != bindings.end() && bound->candidateFunctionOrdinals.size() == 1 &&
                                 ordinals.size() == 1 && bound->candidateFunctionOrdinals[0] == ordinals[0],
                                 "NativeSharp call name binds to the Clang-projected candidate");
                    nativesharp::frontend::ProjectNamespaceFunctionModel observer;
                    const bool observed = observer.setCaptureCppFunctionDemands(true) &&
                        observer.setCppImportedFunctions({imported}, error) &&
                        observer.build(*compilation.nameLookup(), *compilation.memberModel(), compilation.units(),
                                       compilation.diagnostics(), compilation.genericModel());
                    bool directDemand = false;
                    if (observed)
                        for (const auto& demand : observer.cppFunctionDemands())
                            directDemand |= demand.directCall && demand.writtenName == "c_add" &&
                                demand.projectOrdinal == 0 && demand.syntax != nullptr;
                    ok &= expect(directDemand, "demand capture includes an unqualified direct C call");
                    ok &= expect(!model.setCppImportedFunctions({}, error),
                                 "semantic imports cannot change after model construction");
                    nativesharp::frontend::ProjectNamespaceFunctionModel badOwnerModel;
                    nativesharp::diagnostics::DiagnosticEngine badOwnerDiagnostics;
                    auto badOwner = imported;
                    badOwner.projectOrdinal = 999999;
                    ok &= expect(badOwnerModel.setCppImportedFunctions({badOwner}, error) &&
                                 !badOwnerModel.build(*compilation.nameLookup(), *compilation.memberModel(),
                                     compilation.units(), badOwnerDiagnostics, compilation.genericModel()) &&
                                 badOwnerDiagnostics.hasErrors() &&
                                 badOwnerModel.findFunctions(badOwner.projectOrdinal, "", "c_add").empty(),
                                 "semantic index rejects an owner outside the project graph");
                }
                {
                    nativesharp::frontend::ProjectCompilation deferred;
                    CppProjectSessions group;
                    group.projectFile = std::filesystem::absolute(semanticPath);
                    group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::C, "int c_add(int left, int right);\n")));
                    std::vector<CppProjectSessions> groups;
                    groups.push_back(std::move(group));
                    std::size_t invocations = 0;
                    bool sawSyntax = false;
                    bool sawDemand = false;
                    auto provider = [&invocations, &sawSyntax, &sawDemand](const auto& graph, auto sessions, auto units,
                                                               auto demands, auto& output, auto&, auto& why) {
                        ++invocations;
                        sawSyntax = !units.empty() && units.front() && units.front()->syntaxTree();
                        for (const auto& demand : demands)
                            sawDemand |= demand.directCall && demand.writtenName == "c_add" &&
                                demand.projectOrdinal == 0 && demand.syntax != nullptr &&
                                demand.lookupName == "c_add" && demand.lookupNamespaces.size() == 1 &&
                                demand.lookupNamespaces[0].empty() &&
                                std::find(demand.visibleProjects.begin(), demand.visibleProjects.end(), 0) !=
                                    demand.visibleProjects.end();
                        auto inventory = collectCppSemanticFunctions(graph, sessions);
                        if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&inventory))
                        { why = failure->message; return false; }
                        for (auto& entry : std::get<std::vector<CppSemanticInventoryEntry>>(inventory))
                        {
                            auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(
                                &entry.projection);
                            if (!symbol) { why = "unrepresented declaration in test"; return false; }
                            output.push_back(std::move(*symbol));
                        }
                        return true;
                    };
                    nativesharp::frontend::ProjectCompilation syntaxOnly;
                    ok &= expect(syntaxOnly.setCppFunctionProvider(provider, error) &&
                                 syntaxOnly.compileProjectSyntax(semanticPath, error) && invocations == 0,
                                 "syntax-only compilation does not invoke the semantic function provider");
                    ok &= expect(deferred.setCppProjectSessions(std::move(groups), error) &&
                                 deferred.setCppFunctionProvider(provider, error) &&
                                 !deferred.setCppImportedFunctions({imported}, error),
                                 "deferred provider and direct symbols are mutually exclusive");
                    error.clear();
                    const bool deferredReady = deferred.compileProjectMembers(semanticPath, error);
                    if (!deferredReady) std::cerr << "Deferred semantic setup failed: " << error << '\n';
                    ok &= expect(deferredReady && invocations == 1 && sawSyntax && sawDemand && deferred.namespaceFunctionModel() &&
                                 deferred.namespaceFunctionModel()->findFunctions(0, "", "c_add").size() == 1 &&
                                 !deferred.setCppFunctionProvider({}, error),
                                 "provider projects real Clang functions after the graph is prepared");
                    nativesharp::frontend::ProjectCompilation failedProvider;
                    ok &= expect(failedProvider.setCppFunctionProvider(
                        [](const auto&, auto, auto, auto, auto&, auto&, auto& why) { why = "provider test failure"; return false; },
                        error), "failing provider installs before compilation");
                    error.clear();
                    ok &= expect(!failedProvider.compileProjectMembers(semanticPath, error) &&
                                 error == "provider test failure",
                                 "provider failure stops compilation and preserves its diagnostic");
                    nativesharp::frontend::ProjectCompilation duplicateDefinition;
                    duplicateDefinition.setSourceBufferProvider([](const std::filesystem::path& path)
                        -> std::optional<std::string> {
                        if (path.filename() == "Main.ns")
                            return "int duplicate() { return 1; } int duplicate() { return 2; } int Main() { return 0; }\n";
                        return std::nullopt;
                    });
                    bool duplicateProviderCalled = false;
                    ok &= expect(duplicateDefinition.setCppFunctionProvider(
                        [&duplicateProviderCalled](const auto&, auto, auto, auto, auto&, auto&, auto&) {
                            duplicateProviderCalled = true; return true;
                        }, error), "duplicate-definition test installs a provider");
                    error.clear();
                    const bool duplicateReady = duplicateDefinition.compileProjectMembers(semanticPath, error);
                    bool duplicateReported = false;
                    for (const auto& diagnostic : duplicateDefinition.diagnostics().diagnostics())
                        duplicateReported |= diagnostic.message().find("already has a definition") != std::string::npos;
                    ok &= expect(!duplicateReady && !duplicateProviderCalled && duplicateReported,
                                 "observer failure preserves real diagnostics without invoking provider");
                    nativesharp::frontend::ProjectCompilation invalidProvider;
                    ok &= expect(invalidProvider.setCppFunctionProvider(
                        [incompleteProjection](const auto&, auto, auto, auto, auto& output, auto&, auto&) {
                            output.push_back(incompleteProjection); return true;
                        }, error), "provider of invalid symbols installs before compilation");
                    error.clear();
                    ok &= expect(!invalidProvider.compileProjectMembers(semanticPath, error) &&
                                 error.find("incomplete imported") != std::string::npos,
                                 "provider output is validated before semantic installation");
                    nativesharp::frontend::ProjectCompilation directFirst;
                    ok &= expect(directFirst.setCppImportedFunctions({imported}, error) &&
                                 !directFirst.setCppFunctionProvider(provider, error),
                                 "direct symbols also prevent a conflicting provider");
                }
            }
        }
        else ok &= expect(false, "Clang exposes the C function used in semantic test");
        const auto* pair = cRecords ? recordByName(*cRecords, "Pair") : nullptr;
        const auto* cUnion = cRecords ? recordByName(*cRecords, "CValue") : nullptr;
        const auto* incomplete = cRecords ? recordByName(*cRecords, "Incomplete") : nullptr;
        const auto* flags = cRecords ? recordByName(*cRecords, "Flags") : nullptr;
        const auto* later = cRecords ? recordByName(*cRecords, "Later") : nullptr;
        const auto* matrixHolder = cRecords ? recordByName(*cRecords, "MatrixHolder") : nullptr;
        const auto* flexible = cRecords ? recordByName(*cRecords, "Flexible") : nullptr;
        const auto matrixField = matrixHolder ? projectCppSemanticInstanceField(
            *matrixHolder, 0, 0, "Semantic", "x86_64-pc-windows-msvc") : std::nullopt;
        const auto flexibleField = flexible ? projectCppSemanticInstanceField(
            *flexible, 1, 0, "Semantic", "x86_64-pc-windows-msvc") : std::nullopt;
        ok &= expect(matrixField && matrixField->type.nativeSharpTypeKey == "builtin:int[3][2]" &&
                     matrixField->byteOffset == 0 && !flexibleField,
                     "C array field retains fixed dimensions and rejects incomplete bounds");
        ok &= expect(pair && pair->complete && pair->odrHash && pair->sizeBits == 64 && pair->fields.size() == 2 &&
                     pair->fields[0].offsetBits == 0 && pair->fields[1].offsetBits == 32,
                     "C struct size and field offsets match the target");
        const auto unionInteger = cUnion ? projectCppSemanticInstanceField(
            *cUnion, 0, 0, "Semantic", "x86_64-pc-windows-msvc") : std::nullopt;
        const auto unionReal = cUnion ? projectCppSemanticInstanceField(
            *cUnion, 1, 0, "Semantic", "x86_64-pc-windows-msvc") : std::nullopt;
        ok &= expect(pair && !pair->isCxx && pair->fields[0].access == CppAccess::Public &&
                     cUnion && !cUnion->isCxx && cUnion->kind == CppRecordKind::Union &&
                     cUnion->fields.size() == 2 && cUnion->fields[0].offsetBits == 0 &&
                     cUnion->fields[1].offsetBits == 0 && unionInteger && unionReal &&
                     unionInteger->byteOffset == 0 && unionReal->byteOffset == 0,
                     "C record fields are public and overlapping union fields retain Clang offsets");
        ok &= expect(incomplete && !incomplete->complete && !incomplete->sizeBits && !incomplete->odrHash,
                     "incomplete record has no invented layout");
        ok &= expect(flags && flags->fields.size() == 2 && flags->fields[0].offsetBits == 0 &&
                     flags->fields[0].bitWidth == 3 && flags->fields[1].offsetBits == 3 &&
                     flags->fields[1].bitWidth == 5, "bitfield widths and offsets come from Clang");
        ok &= expect(later && later->complete && later->sizeBits == 32 &&
                     std::count_if(cRecords->begin(), cRecords->end(), [](const auto& value) {
                         return value.qualifiedName == "Later";
                     }) == 1, "forward declaration resolves once to its definition");
    }
    auto namespaceSession = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
        "namespace empty {} namespace reopened { namespace first {} } "
        "namespace reopened { namespace second {} } inline namespace versioned {} "
        "namespace { namespace hidden {} }\n"));
    ok &= expect(static_cast<bool>(namespaceSession), "namespace fixture opens");
    if (namespaceSession)
    {
        const auto namespaces = namespaceSession->namespacesSnapshot();
        auto count = [&](std::string_view name) {
            return namespaces ? std::count_if(namespaces->begin(), namespaces->end(), [&](const auto& value) {
                return value.qualifiedName == name;
            }) : 0;
        };
        bool inlineFound = false, anonymousFound = false, hiddenFound = false, allLocated = namespaces.has_value();
        if (namespaces)
            for (const auto& value : *namespaces)
            {
                inlineFound |= value.qualifiedName == "versioned" && value.isInline;
                anonymousFound |= value.isAnonymous && !value.insideAnonymous;
                hiddenFound |= !value.isAnonymous && value.insideAnonymous &&
                    value.qualifiedName.find("hidden") != std::string::npos;
                allLocated &= value.location.has_value();
            }
        ok &= expect(namespaces && count("empty") == 1 && count("reopened") == 1 &&
                     count("reopened::first") == 1 && count("reopened::second") == 1 &&
                     inlineFound && anonymousFound && hiddenFound && allLocated,
                     "namespace snapshot keeps empty, reopened, nested, inline and anonymous declarations");
        namespaceSession->close();
        ok &= expect(!namespaceSession->namespacesSnapshot() && namespaces && count("reopened::second") == 1,
                     "copied namespace declarations survive session closure");
    }
    {
        auto identitySession = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
            "namespace alpha { struct Node; using Alias = Node; int direct(Node*); int alias(Alias*); "
            "struct Outer { struct Inner; }; int nested(Outer::Inner*); } "
            "namespace { struct Hidden; int hidden(Hidden*); } "
            "template<class T> struct Box {}; int specialized(Box<int>*);\n"));
        const auto functions = identitySession ? identitySession->freeFunctionsSnapshot() : std::nullopt;
        const auto recordIdentity = [&](std::string_view name) -> const CppRecordTypeIdentity* {
            if (!functions) return nullptr;
            for (const auto& function : *functions)
                if (function.name == name && function.parameters.size() == 1 &&
                    function.parameters[0].type.element && function.parameters[0].type.element->recordIdentity)
                    return &*function.parameters[0].type.element->recordIdentity;
            return nullptr;
        };
        const auto* direct = recordIdentity("direct");
        const auto* alias = recordIdentity("alias");
        const auto* nestedIdentity = recordIdentity("nested");
        const auto* hidden = recordIdentity("hidden");
        const auto* specialized = recordIdentity("specialized");
        ok &= expect(direct && alias && direct->name == "Node" && alias->name == "Node" &&
                     direct->namespaceSegments == std::vector<std::string>{"alpha"} &&
                     direct->namespaceSegments == alias->namespaceSegments && !direct->anonymousContext &&
                     nestedIdentity && nestedIdentity->name == "Inner" &&
                     nestedIdentity->namespaceSegments == std::vector<std::string>{"alpha"} &&
                     nestedIdentity->containingRecords == std::vector<std::string>{"Outer"} &&
                     hidden && hidden->anonymousContext && specialized && specialized->specializationContext,
                     "record pointer signatures retain canonical identity, nesting, aliases and restricted contexts");
        const auto records = identitySession ? identitySession->recordsSnapshot() : std::nullopt;
        bool matchedNode = false, matchedNested = false, restrictedRejected = false;
        if (functions && records)
            for (const auto& function : *functions)
            {
                if (function.parameters.size() != 1 || !function.parameters[0].type.element) continue;
                const auto& type = *function.parameters[0].type.element;
                const auto projected = projectCppRecordTypeIdentity(type, "Semantic");
                const auto* declaration = function.name == "direct" ? recordByName(*records, "alpha::Node") :
                    function.name == "nested" ? recordByName(*records, "alpha::Outer::Inner") : nullptr;
                const auto expected = declaration ? projectCppRecordIdentity(*declaration, "Semantic") : std::nullopt;
                if (function.name == "direct") matchedNode = projected && expected && projected->key() == expected->key();
                if (function.name == "nested") matchedNested = projected && expected && projected->key() == expected->key();
                if (function.name == "specialized") restrictedRejected = !projected;
            }
        ok &= expect(matchedNode && matchedNested && restrictedRejected,
                     "record identities from function types match inventory identities without string parsing");
    }
    {
        auto nested = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
            "namespace A { struct B { struct C {}; }; } namespace { struct Hidden {}; } "
            "template<class T> struct Box; template<> struct Box<int> { int x; };\n"));
        const auto recordHeader = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/record-api.h";
        std::filesystem::create_directories(recordHeader.parent_path());
        { std::ofstream file(recordHeader); file <<
            "namespace A { namespace B { struct C { long long data; int other; }; int consume(C*); C* create(); } }\n"; }
        auto recordOptions = options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp, "");
        recordOptions.input = CppFileInput{recordHeader};
        auto namespaced = open(factory, std::move(recordOptions));
        auto nestedRecords = nested ? nested->recordsSnapshot() : std::nullopt;
        auto namespaceRecords = namespaced ? namespaced->recordsSnapshot() : std::nullopt;
        const auto* nestedC = nestedRecords ? recordByName(*nestedRecords, "A::B::C") : nullptr;
        const auto* namespaceC = namespaceRecords ? recordByName(*namespaceRecords, "A::B::C") : nullptr;
        auto nestedIdentity = nestedC ? projectCppRecordIdentity(*nestedC, "Semantic") : std::nullopt;
        auto namespaceIdentity = namespaceC ? projectCppRecordIdentity(*namespaceC, "Semantic") : std::nullopt;
        const CppRecordDeclaration* hidden = nullptr;
        const CppRecordDeclaration* specialization = nullptr;
        if (nestedRecords)
            for (const auto& record : *nestedRecords)
            {
                if (record.name == "Hidden") hidden = &record;
                if (record.name == "Box" && record.specializationContext) specialization = &record;
            }
        ok &= expect(nestedIdentity && namespaceIdentity &&
                     nestedIdentity->key() == "Semantic::A.B+C" &&
                     namespaceIdentity->key() == "Semantic::A.B.C" &&
                     hidden && hidden->anonymousContext && !projectCppRecordIdentity(*hidden, "Semantic") &&
                     specialization && !specialization->odrHash &&
                     !projectCppRecordIdentity(*specialization, "Semantic") &&
                     !projectCppRecordIdentity(*nestedC, ""),
                     "record identities distinguish namespace and type contexts and reject anonymous contexts");
        if (namespaceIdentity)
        {
            nativesharp::frontend::ProjectCompilation compilation;
            compilation.setSourceBufferProvider([](const std::filesystem::path& path)
                -> std::optional<std::string> {
                if (path.filename() == "Main.ns") return "int Use(A.B.C* item) { return 0; } int Main() { return 0; }\n";
                return std::nullopt;
            });
            std::string error;
            const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
            const bool parsed = compilation.compileProjectSyntax(path, error);
            nativesharp::diagnostics::DiagnosticEngine observationDiagnostics;
            nativesharp::frontend::ProjectNameLookup observer;
            const bool observerConfigured = parsed && compilation.projectGraph() &&
                observer.setImportedNamespaces({{0, "A.B"}});
            const bool observerReady = observerConfigured && observer.build(
                *compilation.projectGraph(), compilation.units(), observationDiagnostics);
            bool observedDemand = false;
            for (const auto& binding : observer.bindings())
                observedDemand |= binding.kind == nativesharp::frontend::TypeNameBindingKind::Unresolved &&
                    binding.writtenName == "A.B.C" && binding.syntax != nullptr && binding.projectOrdinal == 0;
            ok &= expect(observerConfigured && !observerReady && observationDiagnostics.hasErrors() &&
                         observedDemand && !compilation.diagnostics().hasErrors(),
                         "isolated name observation retains unresolved record demand without polluting compilation");
            nativesharp::frontend::ProjectNameLookup lookup;
            const bool indexed = parsed && compilation.projectGraph() &&
                lookup.setImportedNamespaces({{0, "A.B"}}) &&
                lookup.setCppImportedTypes({{*namespaceIdentity,
                    nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                    nativesharp::frontend::NamedTypeAccessibility::Public, 0,
                    namespaceC->location ? namespaceC->location->file.generic_string() : ""}}) &&
                lookup.build(*compilation.projectGraph(), compilation.units(), compilation.diagnostics());
            const auto ordinal = indexed ? lookup.typeOrdinalForIdentity(namespaceIdentity->key()) : std::nullopt;
            bool bound = false;
            if (ordinal)
                for (const auto& binding : lookup.bindings())
                    bound |= binding.targetTypeOrdinal == ordinal && binding.writtenName == "A.B.C";
            if (!(indexed && ordinal && bound))
            {
                std::cerr << "record lookup parsed=" << parsed << " indexed=" << indexed
                          << " ordinal=" << ordinal.has_value() << " error=" << error << '\n';
                for (const auto& diagnostic : compilation.diagnostics().diagnostics())
                    std::cerr << diagnostic.message() << '\n';
                for (const auto& binding : lookup.bindings())
                    std::cerr << "binding " << binding.writtenName << " target="
                              << binding.targetTypeOrdinal.has_value() << '\n';
            }
            ok &= expect(indexed && ordinal && bound && !lookup.setCppImportedTypes({}),
                         "NativeSharp type syntax resolves an explicitly imported Clang record");
            const auto namespaceFunctions = namespaced ? namespaced->freeFunctionsSnapshot() : std::nullopt;
            const auto* consume = namespaceFunctions ? functionByName(*namespaceFunctions, "A::B::consume") : nullptr;
            const auto* create = namespaceFunctions ? functionByName(*namespaceFunctions, "A::B::create") : nullptr;
            CppRecordGroups canonicalGroups;
            canonicalGroups.groups.push_back({0, *namespaceIdentity, {}, CppRecordGroupStatus::OneDefinition});
            const auto consumeProjection = consume
                ? projectCppSemanticFunctionWithRecords(*consume, 0, "Semantic", canonicalGroups)
                : CppSemanticFunctionResult{CppSemanticFunctionFailure{"missing function"}};
            const auto* consumeSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&consumeProjection);
            const auto createProjection = create
                ? projectCppSemanticFunctionWithRecords(*create, 0, "Semantic", canonicalGroups)
                : CppSemanticFunctionResult{CppSemanticFunctionFailure{"missing function"}};
            const auto* createSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&createProjection);
            nativesharp::frontend::ProjectMemberModel canonicalModel;
            bool canonicalMatches = false;
            for (const auto& binding : lookup.bindings())
                if (binding.syntax && binding.targetTypeOrdinal == ordinal && binding.writtenName == "A.B.C")
                    canonicalMatches |= consumeSymbol && consumeSymbol->parameters.size() == 1 &&
                        canonicalModel.canonicalTypeKey(binding.syntax, lookup) == consumeSymbol->parameters[0].typeKey;
            ok &= expect(canonicalMatches, "C++ record pointer projection equals the actual NativeSharp canonical type key");
            if (consumeSymbol && createSymbol && consume && consume->externalSymbol && namespaceC->sizeBits && namespaceC->alignmentBits)
            {
                // The experimental type is obtained from the owned Clang session and final graph;
                // no NativeSharp declaration or manually injected type stands in for it.
                // Only opaque pointer transport is exercised, not layout or lifetime.
                nativesharp::frontend::ProjectCompilation callCompilation;
                callCompilation.setSourceBufferProvider([](const std::filesystem::path& source)
                    -> std::optional<std::string> {
                    if (source.filename() == "Main.ns")
                        return "using Alias = A.B.C; using Ptr = A.B.C*; int Main() { Ptr p = A.B.create(); return (int)alignof(Alias) * 1000000 + (int)sizeof(Alias) * 10000 + A.B.consume(null) * 100 + A.B.consume(p + 1); }\n";
                    return std::nullopt;
                });
                std::string callError;
                CppProjectSessions recordSessions;
                recordSessions.projectFile = std::filesystem::absolute(path);
                recordSessions.sessions.push_back(std::move(namespaced));
                std::vector<CppProjectSessions> recordSessionGroups;
                recordSessionGroups.push_back(std::move(recordSessions));
                const bool providerReady = callCompilation.setCppProjectSessions(std::move(recordSessionGroups), callError) &&
                    callCompilation.setExperimentalCppTypeProvider(provideExperimentalCppRecordCandidates, callError) &&
                    callCompilation.setExperimentalCppTypeSelector(selectExperimentalCppRecordTypesForCompilation, callError);
                ok &= expect(providerReady && !callCompilation.setExperimentalCppImportedTypes({}, callError),
                             "experimental type provider cannot be combined with an explicit type list");
                const bool callReady = providerReady &&
                    callCompilation.setCppNamespaceProvider(
                        [](const auto& graph, auto sessions, auto& output, auto& error) {
                            auto inventory = collectCppNamespaces(graph, sessions);
                            if (const auto* failure = std::get_if<CppNamespaceInventoryFailure>(&inventory))
                            { error = failure->message; return false; }
                            auto visible = visibleCppNamespaces(std::get<std::vector<CppNamespaceInventoryEntry>>(inventory));
                            if (const auto* failure = std::get_if<CppNamespaceInventoryFailure>(&visible))
                            { error = failure->message; return false; }
                            output = std::move(std::get<std::vector<std::pair<std::size_t, std::string>>>(visible));
                            return true;
                        }, callError) &&
                    callCompilation.setCppFunctionProvider(
                        [&](const auto& graph, auto sessions, auto, auto demands, auto& projected,
                            auto& deferred, auto& error) {
                            const auto* names = callCompilation.nameLookup();
                            if (!names) { error = "name lookup is unavailable"; return false; }
                            return provideExperimentalCppFunctionsWithRecords(
                                graph, sessions, *names, demands, projected, deferred, error);
                        }, callError) &&
                    callCompilation.compileProjectOopMir(path, callError);
                if (!callReady)
                {
                    std::cerr << "record pointer call: " << callError << '\n';
                    for (const auto& diagnostic : callCompilation.diagnostics().diagnostics())
                        std::cerr << diagnostic.message() << '\n';
                }
                ok &= expect(callReady && callCompilation.oopMirModel(),
                             "record pointer function call reaches MIR with matching nominal identity");
                const auto* objectLayouts = callCompilation.objectLayoutModel();
                const auto importedLayout = callReady && objectLayouts && callCompilation.nameLookup()
                    ? objectLayouts->valueLayoutForTypeKey(*callCompilation.nameLookup(), "type:" + namespaceIdentity->key())
                    : std::nullopt;
                const auto importedOrdinal = callReady && callCompilation.nameLookup()
                    ? callCompilation.nameLookup()->typeOrdinalForIdentity(namespaceIdentity->key()) : std::nullopt;
                ok &= expect(importedLayout && namespaceC->sizeBits && namespaceC->alignmentBits &&
                             importedLayout->first * 8 == *namespaceC->sizeBits &&
                             importedLayout->second * 8 == *namespaceC->alignmentBits && importedOrdinal &&
                             !objectLayouts->structLayoutForType(*importedOrdinal),
                             "C++ dimensions come from Clang without fabricating a NativeSharp struct layout");
                nativesharp::diagnostics::DiagnosticEngine clonedDiagnostics;
                std::vector<std::unique_ptr<nativesharp::frontend::ProjectSyntaxUnit>> clonedUnits;
                std::string cloneError;
                ok &= expect(callReady && callCompilation.cloneSyntaxForObservation(clonedUnits, clonedDiagnostics, cloneError) &&
                             !clonedUnits.empty() && !clonedDiagnostics.hasErrors(),
                             "record alias remains parseable in the isolated syntax copy");
                ok &= expect(!callCompilation.setExperimentalCppImportedTypes({}, callError),
                             "experimental imported types cannot change after compilation begins");
                ok &= expect(!callCompilation.setExperimentalCppTypeProvider({}, callError),
                             "experimental type provider cannot change after compilation begins");
                ok &= expect(!callCompilation.setExperimentalCppTypeSelector({}, callError),
                             "experimental type selector cannot change after compilation begins");
                if (callReady && callCompilation.oopMirModel() && callCompilation.nameLookup())
                {
                    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                    auto module = lowerer.lower(*callCompilation.nameLookup(), *callCompilation.oopMirModel());
                    std::string llvmError;
                    const bool valid = module && module->verify(llvmError);
                    const std::string ir = module ? module->text() : std::string{};
                    const std::string name = "@\"" + *consume->externalSymbol + "\"";
                    ok &= expect(valid && ir.find("declare i32 " + name + "(ptr)") != std::string::npos &&
                                 ir.find("call i32 " + name + "(ptr") != std::string::npos,
                                 "record pointer call lowers to verified LLVM with the Clang symbol");
                    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(llvmError);
                    const bool emitted = valid && target && target->emit(*module,
                        std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/cpp-record-pointer-call.obj",
                        nativesharp::llvm_backend::LlvmCodeGenFileType::Object, llvmError);
                    if (!emitted) std::cerr << "record pointer object: " << lowerer.error() << ' ' << llvmError << '\n';
                    ok &= expect(emitted, "record pointer call emits a Windows x64 object");
                }
            }
            else ok &= expect(false, "record pointer consumer and producer project successfully");
            if (consume && consume->externalSymbol)
            {
                nativesharp::frontend::ProjectCompilation nativeCollision;
                nativeCollision.setSourceBufferProvider([](const std::filesystem::path& source)
                    -> std::optional<std::string> {
                    if (source.filename() == "Main.ns")
                        return "namespace A.B { struct C {} } int Main() { return A.B.consume(null); }\n";
                    return std::nullopt;
                });
                CppProjectSessions group;
                group.projectFile = std::filesystem::absolute(path);
                group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
                    "namespace A { namespace B { struct C {}; int consume(C*); } }\n")));
                std::vector<CppProjectSessions> groups;
                groups.push_back(std::move(group));
                bool checkedNativeOnly = false;
                std::string collisionError;
                const bool configured = nativeCollision.setCppProjectSessions(std::move(groups), collisionError) &&
                    nativeCollision.setCppFunctionProvider(
                        [&](const auto& graph, auto sessions, auto, auto demands, auto& projected,
                            auto& deferred, auto& error) {
                            const auto* names = nativeCollision.nameLookup();
                            if (!names || !provideExperimentalCppFunctionsWithRecords(
                                    graph, sessions, *names, demands, projected, deferred, error)) return false;
                            const bool projectedConsume = std::any_of(projected.begin(), projected.end(), [&](const auto& f) {
                                return f.mangledName == *consume->externalSymbol;
                            });
                            const bool deferredConsume = std::any_of(deferred.begin(), deferred.end(), [](const auto& f) {
                                return f.declaration.qualifiedName == "A::B::consume";
                            });
                            checkedNativeOnly = !projectedConsume && deferredConsume;
                            return true;
                        }, collisionError);
                if (configured) nativeCollision.compileProjectMethodBodies(path, collisionError);
                ok &= expect(configured && checkedNativeOnly,
                             "native homonym cannot authorize a C++ record pointer function projection");
            }
            {
                nativesharp::frontend::ProjectCompilation conflicting;
                CppProjectSessions group;
                group.projectFile = std::filesystem::absolute(path);
                group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
                    "namespace A { namespace B { struct C { int x; }; } }\n")));
                group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
                    "namespace A { namespace B { struct C { long long x; }; } }\n")));
                std::vector<CppProjectSessions> groups;
                groups.push_back(std::move(group));
                std::string conflictError;
                const bool configured = conflicting.setCppProjectSessions(std::move(groups), conflictError) &&
                    conflicting.setExperimentalCppTypeProvider(provideExperimentalCppRecordTypes, conflictError);
                const bool parsedConflict = configured && conflicting.compileProjectSyntax(path, conflictError);
                ok &= expect(configured && !parsedConflict &&
                             conflictError.find("conflicting or unverified definitions") != std::string::npos,
                             "experimental record provider rejects conflicting definitions before parsing");
                nativesharp::frontend::ProjectCompilation explicitTypes;
                ok &= expect(explicitTypes.setExperimentalCppImportedTypes({}, conflictError) &&
                             !explicitTypes.setExperimentalCppTypeProvider(provideExperimentalCppRecordTypes, conflictError),
                             "explicit type list cannot be combined with an experimental type provider");
            }
            for (int scenario : {0, 1, 2})
            {
                nativesharp::frontend::ProjectCompilation selectedCompilation;
                selectedCompilation.setSourceBufferProvider([scenario](const std::filesystem::path& source)
                    -> std::optional<std::string> {
                    if (source.filename() != "Main.ns") return std::nullopt;
                    if (scenario == 1) return "int Use(A.B.C* item) { return 0; } int Main() { return 0; }\n";
                    if (scenario == 2) return "namespace A.B { struct C {} } int Use(A.B.C* item) { return 0; } int Main() { return 0; }\n";
                    return "int Main() { return 0; }\n";
                });
                CppProjectSessions group;
                group.projectFile = std::filesystem::absolute(path);
                group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
                    "namespace A { namespace B { struct C { int x; }; } }\n")));
                group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
                    "namespace A { namespace B { struct C { long long x; }; } }\n")));
                std::vector<CppProjectSessions> groups;
                groups.push_back(std::move(group));
                std::string selectedError;
                const bool configured = selectedCompilation.setCppProjectSessions(std::move(groups), selectedError) &&
                    selectedCompilation.setExperimentalCppTypeProvider(provideExperimentalCppRecordCandidates, selectedError) &&
                    selectedCompilation.setExperimentalCppTypeSelector(selectExperimentalCppRecordTypesForCompilation, selectedError);
                const bool ready = configured && selectedCompilation.compileProjectNames(path, selectedError);
                if (scenario == 1)
                    ok &= expect(!ready && selectedError.find("conflicting or unverified definitions") != std::string::npos,
                                 "referenced conflicting C++ record is rejected at experimental name selection");
                else
                {
                    const auto* names = selectedCompilation.nameLookup();
                    const auto ordinal = names ? names->typeOrdinalForIdentity(namespaceIdentity->key()) : std::nullopt;
                    ok &= expect(ready && !selectedCompilation.diagnostics().hasErrors() &&
                                 (scenario == 0 ? !ordinal : ordinal && !names->cppImportedType(*ordinal)),
                                 "unused C++ conflict and native collision do not block experimental name lookup");
                }
            }
            for (int implicitScenario : {0, 1, 2, 3, 4, 5})
            {
                const bool conflicting = implicitScenario == 1 || implicitScenario == 3;
                const bool inferred = implicitScenario >= 2;
                nativesharp::frontend::ProjectCompilation implicitRecord;
                implicitRecord.setSourceBufferProvider([inferred, implicitScenario](const std::filesystem::path& source)
                    -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return implicitScenario == 4
                        ? "int Main() { var p = A.B.create(); return A.B.consume(p); }\n" : inferred
                        ? "int Main() { var p = A.B.create(); return A.B.consume(p + 1); }\n"
                        : "int Main() { return A.B.consume(null); }\n";
                    return std::nullopt;
                });
                CppProjectSessions group;
                group.projectFile = std::filesystem::absolute(path);
                auto implicitOptions = options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp, "");
                implicitOptions.input = CppFileInput{recordHeader};
                group.sessions.push_back(open(factory, std::move(implicitOptions)));
                if (implicitScenario >= 4)
                {
                    auto repeatedOptions = options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp, "");
                    repeatedOptions.input = CppFileInput{recordHeader};
                    group.sessions.push_back(open(factory, std::move(repeatedOptions)));
                }
                if (conflicting)
                    group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
                        "namespace A { namespace B { struct C { long long x; }; int consume(C*); C* create(); } }\n")));
                std::vector<CppProjectSessions> groups;
                groups.push_back(std::move(group));
                std::string implicitError;
                const bool configured = implicitRecord.setCppProjectSessions(std::move(groups), implicitError) &&
                    implicitRecord.setExperimentalCppTypeProvider(provideExperimentalCppRecordCandidates, implicitError) &&
                    implicitRecord.setExperimentalCppTypeSelector(selectExperimentalCppRecordTypesForCompilation, implicitError) &&
                    implicitRecord.setCppNamespaceProvider([](const auto& graph, auto sessions, auto& output, auto& error) {
                        auto inventory = collectCppNamespaces(graph, sessions);
                        if (const auto* failure = std::get_if<CppNamespaceInventoryFailure>(&inventory))
                        { error = failure->message; return false; }
                        auto visible = visibleCppNamespaces(std::get<std::vector<CppNamespaceInventoryEntry>>(inventory));
                        if (const auto* failure = std::get_if<CppNamespaceInventoryFailure>(&visible))
                        { error = failure->message; return false; }
                        output = std::move(std::get<std::vector<std::pair<std::size_t, std::string>>>(visible));
                        return true;
                    }, implicitError) &&
                    implicitRecord.setCppFunctionProvider([&](const auto& graph, auto sessions, auto, auto demands,
                        auto& projected, auto& deferred, auto& error) {
                        const auto* names = implicitRecord.nameLookup();
                        if (!names) { error = "name lookup is unavailable"; return false; }
                        return provideExperimentalCppFunctionsWithRecords(graph, sessions, *names, demands,
                                                                           projected, deferred, error);
                    }, implicitError);
                const bool ready = configured && implicitRecord.compileProjectOopMir(path, implicitError);
                bool deferredRecord = false;
                if (const auto* functions = implicitRecord.namespaceFunctionModel())
                    for (const auto& candidate : functions->cppDeferredFunctions())
                        deferredRecord |= candidate.directCall &&
                            (candidate.declaration.qualifiedName == "A::B::consume" ||
                             (inferred && candidate.declaration.qualifiedName == "A::B::create")) &&
                            !candidate.reason.empty();
                if (conflicting && ready)
                    std::cerr << "conflicting implicit record was incorrectly accepted\n";
                if (!conflicting && implicitScenario != 5 && !ready)
                {
                    std::cerr << "implicit record projection: " << implicitError << '\n';
                    for (const auto& diagnostic : implicitRecord.diagnostics().diagnostics())
                        std::cerr << diagnostic.message() << '\n';
                }
                bool layoutError = implicitError.find("layout") != std::string::npos;
                for (const auto& diagnostic : implicitRecord.diagnostics().diagnostics())
                    layoutError |= diagnostic.message().find("layout") != std::string::npos;
                ok &= expect(configured && (conflicting ? !ready && deferredRecord && implicitRecord.diagnostics().hasErrors()
                    : implicitScenario == 5 ? !ready && layoutError
                    : ready && implicitRecord.oopMirModel() && !deferredRecord),
                             "implicit signature record is projected only with a valid C++ definition");
                if (inferred && !conflicting && ready && implicitRecord.oopMirModel() && implicitRecord.nameLookup())
                {
                    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                    auto module = lowerer.lower(*implicitRecord.nameLookup(), *implicitRecord.oopMirModel());
                    std::string llvmError;
                    const bool valid = module && module->verify(llvmError);
                    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(llvmError);
                    const bool emitted = valid && target && target->emit(*module,
                        std::filesystem::path(argv[2]) / (implicitScenario == 4
                            ? "cpp-fixtures/semantic/cpp-record-repeated-call.obj"
                            : "cpp-fixtures/semantic/cpp-record-inferred-call.obj"),
                        nativesharp::llvm_backend::LlvmCodeGenFileType::Object, llvmError);
                    if (!emitted) std::cerr << "inferred record object: " << lowerer.error() << ' ' << llvmError << '\n';
                    ok &= expect(emitted, "inferred C++ record pointer lowers to a verified Windows object");
                }
            }
            {
                const auto methodHeader = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/method-api.h";
                { std::ofstream file(methodHeader); file <<
                    "namespace Demo { class Widget { public: Widget(int) noexcept; Widget(long) noexcept; Widget(int*) noexcept; Widget(int&) noexcept; ~Widget() noexcept; int padding; int value; const int immutable; volatile int changing; static int shared; static const int limit; static volatile int io; static thread_local int tls; int ping(int) const noexcept; int mutate(int) noexcept; double factor(double) const noexcept; int mixed(int, double, int, double) const noexcept; int skip(int) volatile; "
                    "int merge(int); static int merge(double); static int scale(int) noexcept; static int scale(double) noexcept; }; Widget* create(); "
                    "struct Thing { int marker; int compute(int) const noexcept; }; Thing* createThing(); "
                    "struct Ready { Ready(int) noexcept; ~Ready() noexcept; }; struct Risky { Risky(int); ~Risky() noexcept(false); }; "
                    "struct Implicit { int value; }; struct Deleted { const int value; }; "
                    "struct CountedMember { int value; CountedMember() noexcept(false); ~CountedMember() noexcept(false); }; "
                    "struct FailingMember { int value; FailingMember() noexcept(false); ~FailingMember() noexcept(false); }; "
                    "struct Observed { CountedMember member; FailingMember second; }; } extern \"C\" void cpp_local_throw();\n"; }
                auto configure = [&](nativesharp::frontend::ProjectCompilation& compilation, std::string& error) {
                    compilation.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                        if (source.filename() == "Main.ns") return "int Main() { return 0; }\n";
                        return std::nullopt;
                    });
                    CppProjectSessions group;
                    group.projectFile = std::filesystem::absolute(path);
                    auto methodOptions = options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp, "");
                    methodOptions.input = CppFileInput{methodHeader};
                    group.sessions.push_back(open(factory, std::move(methodOptions)));
                    std::vector<CppProjectSessions> groups;
                    groups.push_back(std::move(group));
                    return compilation.setCppProjectSessions(std::move(groups), error) &&
                        compilation.setExperimentalCppTypeProvider(provideExperimentalCppRecordCandidates, error) &&
                        compilation.setExperimentalCppTypeSelector(selectExperimentalCppRecordTypesForCompilation, error) &&
                        compilation.setExperimentalCppInstanceMethodProvider(provideExperimentalCppInstanceMethods, error);
                };
                nativesharp::frontend::ProjectCompilation automaticMethods;
                std::string automaticError;
                const bool ready = configure(automaticMethods, automaticError) &&
                    automaticMethods.compileProjectMembers(path, automaticError);
                const auto* names = automaticMethods.nameLookup();
                const auto* members = automaticMethods.memberModel();
                const auto ordinal = names ? names->typeOrdinalForIdentity("Semantic::Demo.Widget") : std::nullopt;
                const auto methods = ordinal && members ? members->findMembers(*ordinal, "ping") : std::span<const std::size_t>{};
                const auto* indexed = methods.size() == 1 ? &members->members()[methods[0]] : nullptr;
                ok &= expect(ready && indexed && indexed->isCppExternal && indexed->isReadonly &&
                             indexed->cppExternalSymbol == "?ping@Widget@Demo@@QEBAHH@Z" &&
                             members->findMembers(*ordinal, "skip").empty() &&
                             members->findMembers(*ordinal, "merge").empty() &&
                             !automaticMethods.setExperimentalCppInstanceMethodProvider({}, automaticError),
                             "project compilation obtains instance method metadata from a physical C++ header");
                std::vector<CppSemanticLifetimeFamily> lifetimeFamilies;
                std::string lifetimeError;
                const bool lifetimeCollected = ready && names && automaticMethods.projectGraph() &&
                    collectCppSemanticLifetimeFamilies(*automaticMethods.projectGraph(),
                        automaticMethods.cppProjectSessions(), *names, lifetimeFamilies, lifetimeError);
                const auto family = std::find_if(lifetimeFamilies.begin(), lifetimeFamilies.end(), [](const auto& item) {
                    return item.ownerIdentityKey == "Semantic::Demo.Widget";
                });
                bool indexedCtor = false, indexedPointerCtor = false, indexedDtor = false, deferredCtor = false, implicitCopy = false;
                if (family != lifetimeFamilies.end())
                {
                    for (const auto& item : family->methods)
                    {
                        indexedCtor |= item.kind == CppMethodKind::Constructor &&
                            item.externalSymbol == "??0Widget@Demo@@QEAA@H@Z" && item.parameters.size() == 1 &&
                            item.returnAbi == CppLifetimeReturnAbi::ThisPointer;
                        indexedPointerCtor |= item.kind == CppMethodKind::Constructor && item.parameters.size() == 1 &&
                            item.parameters[0].nativeSharpTypeKey == "builtin:int*";
                        indexedDtor |= item.kind == CppMethodKind::Destructor &&
                            item.externalSymbol == "??1Widget@Demo@@QEAA@XZ" &&
                            item.returnAbi == CppLifetimeReturnAbi::Void;
                    }
                    for (const auto& item : family->deferred)
                        deferredCtor |= item.kind == CppMethodKind::Constructor && !item.reason.empty();
                    if (family->implicitDeclarations) for (const auto& item : *family->implicitDeclarations)
                        implicitCopy |= item.kind == CppImplicitLifetimeKind::CopyConstructor && item.parameters.size() == 1;
                }
                ok &= expect(lifetimeCollected && indexedCtor && indexedPointerCtor && indexedDtor && deferredCtor && implicitCopy &&
                             family->hasProjectedSignatureCollision,
                             "project lifetime family retains explicit, implicit, deferred and colliding overloads");
                if (family != lifetimeFamilies.end() && names)
                {
                    std::vector<nativesharp::frontend::CppImportedLifetimeMethod> physicalMembers;
                    for (const auto& item : family->methods)
                        if (item.kind == CppMethodKind::Destructor || item.externalSymbol == "??0Widget@Demo@@QEAA@H@Z")
                            physicalMembers.push_back(toCppImportedLifetimeMethod(item));
                    nativesharp::frontend::ProjectMemberModel model;
                    std::string memberError;
                    nativesharp::diagnostics::DiagnosticEngine memberDiagnostics;
                    const bool configured = model.setExperimentalCppLifetimeMethods(physicalMembers, memberError);
                    auto duplicated = physicalMembers;
                    if (!duplicated.empty()) duplicated.push_back(duplicated.front());
                    const bool duplicateRejected = !model.setExperimentalCppLifetimeMethods(duplicated, memberError);
                    if (!duplicated.empty()) duplicated.back().externalSymbol += "_other";
                    const bool ambiguousRejected = !model.setExperimentalCppLifetimeMethods(duplicated, memberError);
                    const bool built = configured && model.build(*names, automaticMethods.units(), memberDiagnostics,
                                                                 automaticMethods.genericModel());
                    const auto constructors = ordinal ? model.constructorsOfType(*ordinal) : std::span<const std::size_t>{};
                    const auto* ctor = constructors.size() == 1 ? &model.members()[constructors[0]] : nullptr;
                    bool dtor = false;
                    if (ordinal) for (const auto index : model.membersOfType(*ordinal))
                    {
                        const auto& member = model.members()[index];
                        dtor |= member.kind == nativesharp::frontend::ClassMemberKind::Destructor && member.isCppExternal &&
                            member.cppExternalSymbol == "??1Widget@Demo@@QEAA@XZ" && member.cppLifetimeNonThrowing;
                    }
                    ok &= expect(built && duplicateRejected && ambiguousRejected && ctor && ctor->isCppExternal &&
                                 ctor->cppExternalSymbol == "??0Widget@Demo@@QEAA@H@Z" &&
                                 ctor->parameters.size() == 1 && ctor->parameters[0].typeKey == "builtin:int" &&
                                 ctor->cppLifetimeNonThrowing && dtor &&
                                 !model.setExperimentalCppLifetimeMethods({}, memberError),
                                 "isolated member model indexes imported C++ constructor and destructor metadata");
                    if (!physicalMembers.empty())
                    {
                        auto wrongMembers = physicalMembers;
                        wrongMembers.front().projectOrdinal = 999;
                        nativesharp::frontend::ProjectMemberModel wrongOwner;
                        nativesharp::diagnostics::DiagnosticEngine wrongDiagnostics;
                        const bool wrongConfigured = wrongOwner.setExperimentalCppLifetimeMethods(std::move(wrongMembers), memberError);
                        ok &= expect(wrongConfigured && !wrongOwner.build(*names, automaticMethods.units(), wrongDiagnostics,
                                                                         automaticMethods.genericModel()) &&
                                     wrongDiagnostics.hasErrors(),
                                     "imported C++ lifetime methods reject an owner from another project");
                    }
                }
                nativesharp::frontend::ProjectCompilation lifetimeProject;
                std::string projectLifetimeError;
                const bool lifetimeConfigured = configure(lifetimeProject, projectLifetimeError) &&
                    lifetimeProject.setExperimentalCppLifetimeMethodProvider(provideExperimentalCppLifetimeMethods, projectLifetimeError);
                lifetimeProject.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return "int Main() { Demo.Ready* p = null; Demo.Widget* q = null; Demo.Implicit* r = null; Demo.Deleted* s = null; return 0; }\n";
                    return std::nullopt;
                });
                const bool lifetimeReady = lifetimeConfigured && lifetimeProject.compileProjectMembers(path, projectLifetimeError);
                const auto* lifetimeNames = lifetimeProject.nameLookup();
                const auto* lifetimeMembers = lifetimeProject.memberModel();
                const auto readyType = lifetimeNames ? lifetimeNames->typeOrdinalForIdentity("Semantic::Demo.Ready") : std::nullopt;
                const auto widgetType = lifetimeNames ? lifetimeNames->typeOrdinalForIdentity("Semantic::Demo.Widget") : std::nullopt;
                const auto implicitType = lifetimeNames ? lifetimeNames->typeOrdinalForIdentity("Semantic::Demo.Implicit") : std::nullopt;
                const auto deletedType = lifetimeNames ? lifetimeNames->typeOrdinalForIdentity("Semantic::Demo.Deleted") : std::nullopt;
                const auto readyConstructors = readyType && lifetimeMembers ? lifetimeMembers->constructorsOfType(*readyType) : std::span<const std::size_t>{};
                const auto widgetConstructors = widgetType && lifetimeMembers ? lifetimeMembers->constructorsOfType(*widgetType) : std::span<const std::size_t>{};
                const auto* readyCtor = readyConstructors.size() == 1 ? &lifetimeMembers->members()[readyConstructors[0]] : nullptr;
                const auto implicitCtors = implicitType && lifetimeMembers ? lifetimeMembers->constructorsOfType(*implicitType) : std::span<const std::size_t>{};
                const auto implicitDtors = implicitType && lifetimeMembers ? lifetimeMembers->findMembers(*implicitType, ".dtor") : std::span<const std::size_t>{};
                const auto deletedCtors = deletedType && lifetimeMembers ? lifetimeMembers->constructorsOfType(*deletedType) : std::span<const std::size_t>{};
                const auto* implicitCtor = implicitCtors.size() == 1 ? &lifetimeMembers->members()[implicitCtors[0]] : nullptr;
                const auto* implicitDtor = implicitDtors.size() == 1 ? &lifetimeMembers->members()[implicitDtors[0]] : nullptr;
                ok &= expect(implicitCtor && implicitCtor->cppLifetimeImplicit && implicitCtor->cppLifetimeTrivial &&
                             implicitCtor->cppLifetimeNonThrowing &&
                             implicitCtor->cppExternalSymbol == "??0Implicit@Demo@@QEAA@XZ" &&
                             implicitDtor && implicitDtor->cppLifetimeImplicit && implicitDtor->cppLifetimeTrivial &&
                             implicitDtor->cppLifetimeNonThrowing &&
                             implicitDtor->cppExternalSymbol == "??1Implicit@Demo@@QEAA@XZ" &&
                             deletedType && deletedCtors.empty(),
                             "project indexes implicit default constructor and destructor but not a deleted default constructor");
                if (lifetimeReady && lifetimeNames && lifetimeMembers && implicitCtor && implicitDtor &&
                    readyCtor && lifetimeProject.projectGraph())
                {
                    std::vector<CppImplicitLifetimeEmissionRequest> plan;
                    const std::vector<std::size_t> selected{implicitCtors[0], implicitDtors[0], implicitCtors[0]};
                    const bool planned = planCppImplicitLifetimeEmission(*lifetimeProject.projectGraph(),
                        lifetimeProject.cppProjectSessions(), *lifetimeNames, *lifetimeMembers, selected, plan, projectLifetimeError);
                    const auto directory = std::filesystem::path(argv[2]) / "cpp-fixtures" / "lifetime-emission";
                    std::error_code ec;
                    std::filesystem::create_directories(directory, ec);
                    const bool emittedPlan = planned && plan.size() == 2 && plan[0].session &&
                        plan[0].qualifiedType == "Demo::Implicit" && plan[0].abiSymbol == implicitCtor->cppExternalSymbol &&
                        plan[1].abiSymbol == implicitDtor->cppExternalSymbol &&
                        nativesharp::llvm_backend::emitCppImplicitLifetimeObject(*plan[0].session, plan[0].qualifiedType,
                            plan[0].kind, nativesharp::llvm_backend::LlvmOptimizationLevel::O0,
                            directory / "planned-implicit.obj", projectLifetimeError);
                    const std::vector<std::size_t> explicitSelection{readyConstructors[0]};
                    const bool rejectsExplicit = !planCppImplicitLifetimeEmission(*lifetimeProject.projectGraph(),
                        lifetimeProject.cppProjectSessions(), *lifetimeNames, *lifetimeMembers, explicitSelection, plan, projectLifetimeError) &&
                        plan.size() == 2 && !projectLifetimeError.empty();
                    const std::vector<std::size_t> invalidSelection{lifetimeMembers->members().size()};
                    const bool rejectsInvalid = !planCppImplicitLifetimeEmission(*lifetimeProject.projectGraph(),
                        lifetimeProject.cppProjectSessions(), *lifetimeNames, *lifetimeMembers, invalidSelection, plan, projectLifetimeError) &&
                        plan.size() == 2 && !projectLifetimeError.empty();
                    nativesharp::frontend::ObjectConstructionBinding construction;
                    construction.targetTypeOrdinal = *implicitType;
                    construction.resolution = nativesharp::frontend::ConstructionResolutionKind::ResolvedExplicit;
                    construction.selectedConstructorMemberOrdinal = implicitCtors[0];
                    nativesharp::frontend::ClassDestructionPlan destruction;
                    destruction.typeOrdinal = *implicitType;
                    destruction.explicitDestructorMemberOrdinal = implicitDtors[0];
                    destruction.destructorMemberOrdinals.push_back(implicitDtors[0]);
                    const std::vector constructions{construction, construction};
                    const std::vector destructions{destruction};
                    const bool plannedBindings = planCppImplicitLifetimeFromBindings(*lifetimeProject.projectGraph(),
                        lifetimeProject.cppProjectSessions(), *lifetimeNames, *lifetimeMembers,
                        constructions, destructions, plan, projectLifetimeError) &&
                        plan.size() == 2 && plan[0].memberOrdinal == implicitCtors[0] && plan[1].memberOrdinal == implicitDtors[0];
                    construction.resolution = nativesharp::frontend::ConstructionResolutionKind::ResolvedImplicitDefault;
                    const std::vector invalidConstructions{construction};
                    const bool rejectsNativeDefault = !planCppImplicitLifetimeFromBindings(*lifetimeProject.projectGraph(),
                        lifetimeProject.cppProjectSessions(), *lifetimeNames, *lifetimeMembers,
                        invalidConstructions, destructions, plan, projectLifetimeError) && plan.size() == 2;
                    destruction.explicitDestructorMemberOrdinal.reset();
                    destruction.destructorMemberOrdinals.clear();
                    const std::vector missingDestruction{destruction};
                    const bool rejectsMissingDestructor = !planCppImplicitLifetimeFromBindings(*lifetimeProject.projectGraph(),
                        lifetimeProject.cppProjectSessions(), *lifetimeNames, *lifetimeMembers,
                        constructions, missingDestruction, plan, projectLifetimeError) && plan.size() == 2;
                    ok &= expect(!ec && emittedPlan && rejectsExplicit && rejectsInvalid && plannedBindings &&
                                 rejectsNativeDefault && rejectsMissingDestructor,
                                 "selected implicit members resolve their owner session and emit without duplicates");
                }
                ok &= expect(lifetimeReady && readyCtor && readyCtor->cppExternalSymbol == "??0Ready@Demo@@QEAA@H@Z" &&
                             widgetType && widgetConstructors.empty() &&
                             !lifetimeProject.setExperimentalCppLifetimeMethodProvider({}, projectLifetimeError),
                             "project lifetime provider indexes complete observed families and defers ambiguous ones");
                CppProjectSessions noImplicitGroup;
                noImplicitGroup.projectFile = std::filesystem::absolute(path);
                auto noImplicitOptions = options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp, "");
                noImplicitOptions.input = CppFileInput{methodHeader};
                auto underlyingSession = open(factory, std::move(noImplicitOptions));
                if (underlyingSession) noImplicitGroup.sessions.push_back(std::make_unique<NoImplicitSession>(std::move(underlyingSession)));
                std::vector<CppProjectSessions> noImplicitGroups;
                noImplicitGroups.push_back(std::move(noImplicitGroup));
                std::vector<CppSemanticLifetimeFamily> unavailableFamilies;
                std::vector<nativesharp::frontend::CppImportedLifetimeMethod> unavailableMethods;
                const bool unavailableCollected = lifetimeReady && lifetimeNames && lifetimeProject.projectGraph() &&
                    collectCppSemanticLifetimeFamilies(*lifetimeProject.projectGraph(), noImplicitGroups, *lifetimeNames,
                                                       unavailableFamilies, projectLifetimeError) &&
                    provideExperimentalCppLifetimeMethods(*lifetimeProject.projectGraph(), noImplicitGroups, *lifetimeNames,
                                                          unavailableMethods, projectLifetimeError);
                const auto unavailableReady = std::find_if(unavailableFamilies.begin(), unavailableFamilies.end(), [](const auto& item) {
                    return item.ownerIdentityKey == "Semantic::Demo.Ready";
                });
                ok &= expect(unavailableCollected && unavailableReady != unavailableFamilies.end() &&
                             !unavailableReady->implicitDeclarations &&
                             std::none_of(unavailableMethods.begin(), unavailableMethods.end(), [](const auto& item) {
                                 return item.ownerIdentityKey == "Semantic::Demo.Ready";
                             }), "an unavailable implicit-member observation is not treated as an empty result");
                nativesharp::frontend::ProjectCompilation lifetimeGuardProject;
                std::string lifetimeGuardError;
                const bool lifetimeGuardConfigured = configure(lifetimeGuardProject, lifetimeGuardError) &&
                    lifetimeGuardProject.setExperimentalCppLifetimeMethodProvider(provideExperimentalCppLifetimeMethods, lifetimeGuardError);
                const bool escapedLifetimeGuard = lifetimeGuardConfigured && lifetimeGuardProject.compileProjectInheritance(path, lifetimeGuardError);
                ok &= expect(lifetimeGuardConfigured && !escapedLifetimeGuard && lifetimeGuardError.find("lifetime members") != std::string::npos,
                             "experimental lifetime indexing cannot proceed to code generation");
                auto configureRealConstruction = [&](nativesharp::frontend::ProjectCompilation& project, std::string& error) {
                    const bool configured = configure(project, error) &&
                        project.setExperimentalCppLifetimeMethodProvider(provideExperimentalCppLifetimeMethods, error) &&
                        project.enableExperimentalCppLifetimeAnalysisForTests(error) &&
                        project.enableExperimentalCppInstanceMethodLoweringForTests(error) &&
                        project.enableExperimentalCppInstanceFieldLoweringForTests(error);
                    project.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                        if (source.filename() == "Main.ns") return "int Main() { new Demo.Implicit(); return 0; }\n";
                        return std::nullopt;
                    });
                    return configured;
                };
                nativesharp::frontend::ProjectCompilation localLifetime;
                std::string localLifetimeError;
                const bool localLifetimeConfigured = configureRealConstruction(localLifetime, localLifetimeError) &&
                    localLifetime.enableExperimentalCppLocalLifetimeAnalysisForTests(localLifetimeError);
                localLifetime.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return "int Main() { Demo.Ready value = new Demo.Ready(1); return 0; }\n";
                    return std::nullopt;
                });
                const bool localLifetimeReady = localLifetimeConfigured &&
                    localLifetime.compileProjectOopHir(path, localLifetimeError);
                if (!localLifetimeReady) std::cerr << "C++ local lifetime analysis: " << localLifetimeError << '\n';
                bool hasLocalDrop = false;
                if (localLifetimeReady && localLifetime.methodBodyModel())
                {
                    const auto locals = localLifetime.methodBodyModel()->locals();
                    for (std::size_t index = 0; index < locals.size(); ++index)
                        if (locals[index].name == "value" && locals[index].requiresValueDrop)
                            for (const auto& region : localLifetime.methodBodyModel()->cleanupRegions())
                                hasLocalDrop |= region.kind == nativesharp::frontend::MethodCleanupRegionKind::ValueLocalDrop &&
                                    region.resourceLocalOrdinal == index;
                }
                ok &= expect(localLifetimeReady && hasLocalDrop,
                             "C++ value local registers its destructor cleanup in semantic analysis");
                bool localPhysicalLayout = false;
                if (localLifetimeReady && localLifetime.methodBodyModel() && localLifetime.nameLookup() &&
                    localLifetime.objectLayoutModel())
                    for (const auto& local : localLifetime.methodBodyModel()->locals())
                    {
                        if (local.name != "value" || !local.typeKey.starts_with("type:")) continue;
                        const auto ordinal = localLifetime.nameLookup()->typeOrdinalForIdentity(
                            std::string_view(local.typeKey).substr(5));
                        const auto* imported = ordinal ? localLifetime.nameLookup()->cppImportedType(*ordinal) : nullptr;
                        const auto physical = localLifetime.objectLayoutModel()->valueLayoutForTypeKey(
                            *localLifetime.nameLookup(), local.typeKey);
                        localPhysicalLayout = imported && imported->objectLayout && physical &&
                            physical->first == imported->objectLayout->sizeBytes &&
                            physical->second == imported->objectLayout->alignmentBytes &&
                            !localLifetime.objectLayoutModel()->structLayoutForType(*ordinal);
                    }
                ok &= expect(localPhysicalLayout,
                             "C++ value local retains Clang storage dimensions without a NativeSharp struct layout");
                bool localHirLifetime = false;
                if (localLifetimeReady && localLifetime.oopHirModel())
                    for (const auto& function : localLifetime.oopHirModel()->functions())
                    {
                        std::vector<const nativesharp::frontend::ProjectOopHirNode*> pending;
                        if (function.body) pending.push_back(function.body.get());
                        while (!pending.empty())
                        {
                            const auto* node = pending.back();
                            pending.pop_back();
                            for (const auto& child : node->children) if (child) pending.push_back(child.get());
                            if (node->kind != nativesharp::frontend::ProjectOopHirNodeKind::LocalDeclaration ||
                                node->stringValue != "value" || !node->localOrdinal || !node->cleanupRegionOrdinal ||
                                !node->automaticDestruction || node->children.size() != 1 || !node->children[0]) continue;
                            const auto* construction = node->children[0].get();
                            localHirLifetime = construction->kind == nativesharp::frontend::ProjectOopHirNodeKind::ValueConstruction &&
                                construction->memberOrdinal.has_value() && construction->targetTypeOrdinal.has_value();
                        }
                    }
                ok &= expect(localHirLifetime, "C++ value local reaches HIR with construction and cleanup identity");
                nativesharp::frontend::ProjectOopMirModel localMir;
                std::string localMirError;
                const bool localMirBuilt = localLifetimeReady && localLifetime.nameLookup() &&
                    localLifetime.memberModel() && localLifetime.objectLayoutModel() &&
                    localLifetime.virtualDispatchModel() && localLifetime.interfaceModel() &&
                    localLifetime.destructionModel() && localLifetime.oopHirModel() &&
                    localLifetime.namespaceFunctionModel() && localMir.build(*localLifetime.nameLookup(),
                        *localLifetime.memberModel(), *localLifetime.objectLayoutModel(),
                        *localLifetime.virtualDispatchModel(), *localLifetime.interfaceModel(),
                        *localLifetime.destructionModel(), *localLifetime.oopHirModel(),
                        *localLifetime.namespaceFunctionModel(), localMirError);
                if (!localMirBuilt) std::cerr << "Isolated C++ local MIR: " << localMirError << '\n';
                bool localMirLifetime = false;
                if (localMirBuilt)
                    for (const auto& function : localMir.functions())
                    {
                        std::vector<const nativesharp::frontend::ProjectOopMirNode*> pending;
                        if (function.body) pending.push_back(function.body.get());
                        while (!pending.empty())
                        {
                            const auto* node = pending.back();
                            pending.pop_back();
                            for (const auto& child : node->children) if (child) pending.push_back(child.get());
                            if (node->kind != nativesharp::frontend::ProjectOopMirNodeKind::LocalDeclaration ||
                                node->stringValue != "value" || !node->localOrdinal || !node->cleanupRegionOrdinal ||
                                node->children.size() != 1 || !node->children[0]) continue;
                            const auto* construction = node->children[0].get();
                            bool matchingCleanup = false;
                            for (const auto& region : function.cleanupRegions)
                                matchingCleanup |= region.ordinal == *node->cleanupRegionOrdinal &&
                                    region.localOrdinal == node->localOrdinal && region.automaticDestruction &&
                                    region.typeKey == node->typeKey;
                            localMirLifetime = construction->kind == nativesharp::frontend::ProjectOopMirNodeKind::ValueConstruct &&
                                construction->memberOrdinal.has_value() && !construction->calleeName.empty() &&
                                construction->immediate > 0 && construction->alignment > 0 && matchingCleanup;
                        }
                    }
                ok &= expect(localMirBuilt && localMirLifetime,
                             "isolated C++ local MIR retains construction, layout and cleanup identity");
                nativesharp::frontend::ProjectCompilation implicitLocals;
                std::string implicitLocalError;
                const bool implicitConfigured = configureRealConstruction(implicitLocals, implicitLocalError) &&
                    implicitLocals.enableExperimentalCppLocalLifetimeAnalysisForTests(implicitLocalError);
                implicitLocals.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return "int Main() { Demo.Observed a = new Demo.Observed(); Demo.Observed b = new Demo.Observed(); return 0; }\n";
                    return std::nullopt;
                });
                const bool implicitHir = implicitConfigured && implicitLocals.compileProjectOopHir(path, implicitLocalError);
                nativesharp::frontend::ProjectOopMirModel implicitMir;
                const bool implicitMirBuilt = implicitHir && implicitMir.build(*implicitLocals.nameLookup(),
                    *implicitLocals.memberModel(), *implicitLocals.objectLayoutModel(), *implicitLocals.virtualDispatchModel(),
                    *implicitLocals.interfaceModel(), *implicitLocals.destructionModel(), *implicitLocals.oopHirModel(),
                    *implicitLocals.namespaceFunctionModel(), implicitLocalError);
                std::vector<CppImplicitLifetimeEmissionRequest> implicitLocalPlan;
                const bool implicitPlanned = implicitMirBuilt && planCppImplicitLifetimeFromValues(
                    *implicitLocals.projectGraph(), implicitLocals.cppProjectSessions(), *implicitLocals.nameLookup(),
                    *implicitLocals.memberModel(), implicitLocals.constructionModel()->constructions(), implicitMir,
                    implicitLocalPlan, implicitLocalError);
                if (!implicitPlanned) std::cerr << "Implicit C++ local plan: " << implicitLocalError << '\n';
                bool localCtorPlanned = false, localDtorPlanned = false;
                for (const auto& item : implicitLocalPlan)
                {
                    localCtorPlanned |= item.qualifiedType == "Demo::Observed" && item.kind == CppImplicitLifetimeKind::DefaultConstructor;
                    localDtorPlanned |= item.qualifiedType == "Demo::Observed" && item.kind == CppImplicitLifetimeKind::Destructor;
                }
                ok &= expect(implicitPlanned && implicitLocalPlan.size() == 2 && localCtorPlanned && localDtorPlanned,
                             "implicit C++ locals request one constructor and one destructor body");
                nativesharp::llvm_backend::ProjectOopLlvmLowerer localLlvmLowerer;
                auto localLlvmModule = localMirBuilt ? localLlvmLowerer.lower(
                    *localLifetime.nameLookup(), *localLifetime.memberModel(), localMir,
                    localLifetime.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::None,
                    nativesharp::llvm_backend::LlvmOptimizationLevel::O0) : nullptr;
                std::string localLlvmVerifyError;
                const bool localLlvmValid = localLlvmModule && localLlvmModule->verify(localLlvmVerifyError);
                const auto localIr = localLlvmModule ? localLlvmModule->text() : std::string{};
                if (!localLlvmValid) std::cerr << "Isolated C++ local LLVM: " << localLlvmLowerer.error()
                                               << " " << localLlvmVerifyError << '\n';
                const bool localCtor = localIr.find("call ptr @\"??0Ready@Demo@@") != std::string::npos ||
                    localIr.find("invoke ptr @\"??0Ready@Demo@@") != std::string::npos;
                const bool localDtor = localIr.find("call void @\"??1Ready@Demo@@") != std::string::npos ||
                    localIr.find("invoke void @\"??1Ready@Demo@@") != std::string::npos;
                ok &= expect(localLlvmValid && localCtor && localDtor &&
                             localIr.find("struct.new.value") == std::string::npos,
                             "C++ local is constructed and destroyed in place in isolated LLVM");
                nativesharp::frontend::ProjectCompilation throwingLocal;
                std::string throwingError;
                const bool throwingConfigured = configureRealConstruction(throwingLocal, throwingError) &&
                    throwingLocal.enableExperimentalCppLocalLifetimeAnalysisForTests(throwingError) &&
                    throwingLocal.setCppFunctionProvider([](const auto& graph, auto sessions, auto, auto demands,
                        auto& projected, auto& deferred, auto& why) {
                            return provideCppSemanticFunctions(graph, sessions, demands, projected, deferred, why);
                        }, throwingError);
                throwingLocal.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return "int Main() { Demo.Risky a = new Demo.Risky(1); Demo.Risky b = new Demo.Risky(2); Demo.Risky c = new Demo.Risky(3); cpp_local_throw(); return 0; }\n";
                    return std::nullopt;
                });
                const bool throwingHir = throwingConfigured && throwingLocal.compileProjectOopHir(path, throwingError);
                nativesharp::frontend::ProjectOopMirModel throwingMir;
                const bool throwingMirReady = throwingHir && throwingMir.build(*throwingLocal.nameLookup(),
                    *throwingLocal.memberModel(), *throwingLocal.objectLayoutModel(), *throwingLocal.virtualDispatchModel(),
                    *throwingLocal.interfaceModel(), *throwingLocal.destructionModel(), *throwingLocal.oopHirModel(),
                    *throwingLocal.namespaceFunctionModel(), throwingError);
                nativesharp::llvm_backend::ProjectOopLlvmLowerer throwingLowerer;
                auto throwingModule = throwingMirReady ? throwingLowerer.lower(*throwingLocal.nameLookup(),
                    *throwingLocal.memberModel(), throwingMir, throwingLocal.sourceManager(),
                    nativesharp::llvm_backend::DebugInformationKind::None,
                    nativesharp::llvm_backend::LlvmOptimizationLevel::O0) : nullptr;
                const bool throwingValid = throwingModule && throwingModule->verify(throwingError);
                if (!throwingValid) std::cerr << "C++ local exception setup: " << throwingError << ' ' << throwingLowerer.error() << '\n';
                ok &= expect(throwingValid, "isolated C++ locals with throwing constructor lower to verified LLVM");
                if (argc > 4 && (std::string_view(argv[4]) == "lifetime" || std::string_view(argv[4]) == "bundle-link"))
                {
                    const auto output = std::filesystem::path(argv[2]) / "cpp-fixtures" / "lifetime-emission";
                    nativesharp::llvm_backend::ProjectOopLlvmLowerer implicitLowerer;
                    auto implicitModule = implicitPlanned ? implicitLowerer.lower(*implicitLocals.nameLookup(),
                        *implicitLocals.memberModel(), implicitMir, implicitLocals.sourceManager(),
                        nativesharp::llvm_backend::DebugInformationKind::None,
                        nativesharp::llvm_backend::LlvmOptimizationLevel::O0) : nullptr;
                    bool implicitEmitted = implicitModule && implicitModule->verify(implicitLocalError);
                    for (int opt : {0, 2})
                    {
                        const auto level = opt == 0 ? nativesharp::llvm_backend::LlvmOptimizationLevel::O0 :
                            nativesharp::llvm_backend::LlvmOptimizationLevel::O2;
                        auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(level, implicitLocalError);
                        implicitEmitted &= target && implicitModule && target->emit(*implicitModule,
                            output / ("implicit-local-ns-O" + std::to_string(opt) + ".obj"),
                            nativesharp::llvm_backend::LlvmCodeGenFileType::Object, implicitLocalError);
                        std::vector<std::filesystem::path> paths;
                        for (const auto& request : implicitLocalPlan)
                            paths.push_back(output / (std::string("implicit-local-") +
                                (request.kind == CppImplicitLifetimeKind::DefaultConstructor ? "ctor-O" : "dtor-O") +
                                std::to_string(opt) + ".obj"));
                        std::vector<std::filesystem::path> emitted;
                        implicitEmitted &= nativesharp::llvm_backend::emitCppImplicitLifetimeObjects(
                            implicitLocalPlan, paths, level, emitted, implicitLocalError) && emitted.size() == 2;
                    }
                    bool implicitBundlesReady = implicitPlanned;
                    for (int opt : {0, 2})
                    {
                        const auto level = opt == 0 ? nativesharp::llvm_backend::LlvmOptimizationLevel::O0 :
                            nativesharp::llvm_backend::LlvmOptimizationLevel::O2;
                        nativesharp::llvm_backend::ProjectOopObjectGenerator generator;
                        auto bundle = generator.generate(*implicitLocals.nameLookup(), *implicitLocals.memberModel(),
                            implicitMir, implicitLocals.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::None,
                            level, 1, {}, implicitLocalError);
                        const auto before = bundle ? bundle->objectFiles().size() : 0;
                        const bool appended = bundle && nativesharp::driver::appendCppImplicitLifetimeObjects(
                            implicitLocals, implicitMir, *bundle, level, implicitLocalError) &&
                            bundle->objectFiles().size() == before + 2;
                        std::vector<std::filesystem::path> temporary;
                        if (appended) temporary.assign(bundle->objectFiles().begin() + before, bundle->objectFiles().end());
                        nativesharp::linker::WindowsLibraryBuilder archiveBuilder;
                        archiveBuilder.setRuntimeLibrary("A:/nsc-interop-build/tanda1-ON/generated/NativeSharp.Runtime.Win32.lib");
                        const bool archived = appended && archiveBuilder.buildStaticObjects(bundle->objectFiles(),
                            output / ("implicit-local-bundle-O" + std::to_string(opt) + ".lib"), level, implicitLocalError);
                        bundle.reset();
                        const bool removed = temporary.size() == 2 && std::all_of(temporary.begin(), temporary.end(),
                            [](const auto& path) { return !std::filesystem::exists(path); });
                        implicitBundlesReady &= archived && removed;
                    }
                    if (!implicitBundlesReady) std::cerr << "Implicit C++ locals bundle: " << implicitLocalError << '\n';
                    ok &= expect(implicitBundlesReady, "implicit C++ local bundle includes and owns generated lifetime objects");
                    std::string implicitEntry;
                    if (implicitMirBuilt)
                        for (const auto& function : implicitMir.functions())
                            if (function.namespaceFunctionOrdinal && function.returnTypeKey == "builtin:int")
                                implicitEntry = function.mangledName;
                    std::ofstream implicitHarness(output / "implicit-local-harness.cpp");
                    implicitHarness << "namespace Demo { struct CountedMember { int value; CountedMember() noexcept(false); ~CountedMember() noexcept(false); }; struct FailingMember { int value; FailingMember() noexcept(false); ~FailingMember() noexcept(false); }; }\n"
                        "static void* c[2]; static void* f[2]; static int ci,fi,cd,fd,bad; "
                        "Demo::CountedMember::CountedMember() noexcept(false){ if(ci>=2) bad=1; else c[ci++]=this; }\n"
                        "Demo::FailingMember::FailingMember() noexcept(false){ if(fi>=2) bad=1; else f[fi++]=this; }\n"
                        "Demo::CountedMember::~CountedMember() noexcept(false){ if(cd>=2 || this!=c[1-cd] || fd!=cd+1) bad=1; ++cd; }\n"
                        "Demo::FailingMember::~FailingMember() noexcept(false){ if(fd>=2 || this!=f[1-fd] || cd!=fd) bad=1; ++fd; }\n"
                        "extern \"C\" int invoke_ns() __asm__(\"" << implicitEntry << "\");\n"
                        "int main(){ int r=invoke_ns(); return r==0 && ci==2 && fi==2 && cd==2 && fd==2 && !bad ? 0 : 1; }\n";
                    implicitHarness.close();
                    if (!implicitEmitted) std::cerr << "Implicit C++ locals object emission: " << implicitLocalError << ' ' << implicitLowerer.error() << '\n';
                    ok &= expect(implicitEmitted && !implicitEntry.empty() && static_cast<bool>(implicitHarness),
                                 "implicit C++ local objects and constructor/destructor bodies are emitted");
                    bool emittedLocal = localLlvmValid;
                    for (int opt : {0, 2})
                    {
                        const auto level = opt == 0 ? nativesharp::llvm_backend::LlvmOptimizationLevel::O0 :
                            nativesharp::llvm_backend::LlvmOptimizationLevel::O2;
                        auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(level, localLlvmVerifyError);
                        emittedLocal &= target && localLlvmModule && target->emit(*localLlvmModule,
                            output / ("local-ready-ns-O" + std::to_string(opt) + ".obj"),
                            nativesharp::llvm_backend::LlvmCodeGenFileType::Object, localLlvmVerifyError);
                    }
                    std::string entry;
                    for (const auto& function : localMir.functions())
                        if (function.namespaceFunctionOrdinal && function.returnTypeKey == "builtin:int")
                            entry = function.mangledName;
                    std::ofstream harness(output / "local-ready-harness.cpp");
                    harness << "namespace Demo { struct Ready { Ready(int) noexcept; ~Ready() noexcept; }; }\n"
                        "static void* created; static int stage; "
                        "Demo::Ready::Ready(int v) noexcept { created = this; stage = v == 1 ? 1 : 99; }\n"
                        "Demo::Ready::~Ready() noexcept { stage = (stage == 1 && created == this) ? 2 : 99; }\n"
                        "extern \"C\" int invoke_ns() __asm__(\"" << entry << "\");\n"
                        "extern \"C\" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);\n"
                        "extern \"C\" void* memset(void* d, int v, unsigned long long n) { auto p = (unsigned char*)d; for (unsigned long long i=0; i<n; ++i) p[i]=(unsigned char)v; return d; }\n"
                        "extern \"C\" void mainCRTStartup() { int r=invoke_ns(); ExitProcess(r == 0 && stage == 2 ? 0 : 1); }\n";
                    harness.close();
                    if (!emittedLocal) std::cerr << "C++ local object emission: " << localLlvmVerifyError << '\n';
                    ok &= expect(emittedLocal && !entry.empty() && static_cast<bool>(harness),
                                 "isolated C++ local objects and address-check harness are emitted");
                    bool throwingEmitted = throwingValid;
                    for (int opt : {0, 2})
                    {
                        const auto level = opt == 0 ? nativesharp::llvm_backend::LlvmOptimizationLevel::O0 :
                            nativesharp::llvm_backend::LlvmOptimizationLevel::O2;
                        auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(level, throwingError);
                        throwingEmitted &= target && throwingModule && target->emit(*throwingModule,
                            output / ("local-throw-ns-O" + std::to_string(opt) + ".obj"),
                            nativesharp::llvm_backend::LlvmCodeGenFileType::Object, throwingError);
                    }
                    std::string throwingEntry;
                    if (throwingMirReady)
                        for (const auto& function : throwingMir.functions())
                            if (function.namespaceFunctionOrdinal && function.returnTypeKey == "builtin:int")
                                throwingEntry = function.mangledName;
                    std::ofstream throwingHarness(output / "local-throw-harness.cpp");
                    throwingHarness << "namespace Demo { struct Risky { Risky(int); ~Risky() noexcept(false); }; }\n"
                        "static void* objects[4]; static int failAt; static int count; static int bad; "
                        "Demo::Risky::Risky(int v) { objects[v]=this; if(v==failAt) throw 42; }\n"
                        "Demo::Risky::~Risky() noexcept(false) { int n=failAt>4 ? 3 : failAt-1; if(count>=n || this != objects[n-count]) bad=1; ++count; if(failAt==6 && this==objects[2]) throw 43; }\n"
                        "extern \"C\" void cpp_local_throw() { if(failAt==4) throw 42; }\n"
                        "extern \"C\" int invoke_ns() __asm__(\"" << throwingEntry << "\");\n"
                        "extern \"C\" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);\n"
                        "extern \"C\" void mainCRTStartup() { for(failAt=2; failAt<=6; ++failAt) { count=0; bad=0; int caught=0; try { invoke_ns(); } catch(int v) { caught=v; } int n=failAt>4 ? 3 : failAt-1; int expected=failAt==5 ? 0 : (failAt==6 ? 43 : 42); if(caught!=expected || count!=n || bad) ExitProcess(1); } ExitProcess(0); }\n";
                    throwingHarness.close();
                    if (!throwingEmitted) std::cerr << "C++ local exception object: " << throwingError << '\n';
                    ok &= expect(throwingEmitted && !throwingEntry.empty() && static_cast<bool>(throwingHarness),
                                 "throwing C++ local objects and harness are emitted");
                }
                nativesharp::frontend::ProjectCompilation localGuard;
                std::string localGuardError;
                const bool localGuardConfigured = configureRealConstruction(localGuard, localGuardError) &&
                    localGuard.enableExperimentalCppLocalLifetimeAnalysisForTests(localGuardError);
                localGuard.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return "int Main() { Demo.Ready value = new Demo.Ready(1); new Demo.Ready(1); return 0; }\n";
                    return std::nullopt;
                });
                const bool localDiscardedEnabled = localGuardConfigured &&
                    localGuard.enableExperimentalCppDiscardedLifetimeLoweringForTests(localGuardError);
                localGuardError.clear();
                const bool localMirIntegrated = localDiscardedEnabled && localGuard.compileProjectOopMir(path, localGuardError);
                if (!localMirIntegrated) std::cerr << "C++ integrated local MIR: " << localGuardError << '\n';
                ok &= expect(localMirIntegrated && localGuard.oopMirModel(),
                             "experimental local and discarded C++ constructions reach MIR together");
                if (argc > 4 && (std::string_view(argv[4]) == "lifetime" || std::string_view(argv[4]) == "bundle-link"))
                {
                    const auto output = std::filesystem::path(argv[2]) / "cpp-fixtures" / "lifetime-emission";
                    bool integratedArchives = localMirIntegrated;
                    for (int opt : {0, 2})
                    {
                        const auto level = opt == 0 ? nativesharp::llvm_backend::LlvmOptimizationLevel::O0 :
                            nativesharp::llvm_backend::LlvmOptimizationLevel::O2;
                        nativesharp::llvm_backend::ProjectOopObjectGenerator generator;
                        auto bundle = localMirIntegrated ? generator.generate(*localGuard.nameLookup(), *localGuard.memberModel(),
                            *localGuard.oopMirModel(), localGuard.sourceManager(), nativesharp::llvm_backend::DebugInformationKind::None,
                            level, 1, {}, localGuardError) : nullptr;
                        const bool appended = bundle && nativesharp::driver::appendCppImplicitLifetimeObjects(
                            localGuard, *localGuard.oopMirModel(), *bundle, level, localGuardError);
                        nativesharp::linker::WindowsLibraryBuilder builder;
                        builder.setRuntimeLibrary("A:/nsc-interop-build/tanda1-ON/generated/NativeSharp.Runtime.Win32.lib");
                        integratedArchives &= appended && builder.buildStaticObjects(bundle->objectFiles(),
                            output / ("integrated-local-O" + std::to_string(opt) + ".lib"), level, localGuardError);
                    }
                    std::string entry;
                    if (localMirIntegrated)
                        for (const auto& function : localGuard.oopMirModel()->functions())
                            if (function.namespaceFunctionOrdinal && function.returnTypeKey == "builtin:int") entry = function.mangledName;
                    std::ofstream harness(output / "integrated-local-harness.cpp");
                    harness << "namespace Demo { struct Ready { Ready(int) noexcept; ~Ready() noexcept; }; }\n"
                        "static void* slots[2]; static int step; static int bad; "
                        "Demo::Ready::Ready(int v) noexcept { if(v!=1 || step>1) bad=1; else slots[step]=this; ++step; }\n"
                        "Demo::Ready::~Ready() noexcept { if(step==2 && this==slots[1]) ++step; else if(step==3 && this==slots[0]) ++step; else bad=1; }\n"
                        "extern \"C\" int invoke_ns() __asm__(\"" << entry << "\");\n"
                        "extern \"C\" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);\n"
                        "extern \"C\" void* memset(void* d,int v,unsigned long long n){auto p=(unsigned char*)d; for(unsigned long long i=0;i<n;++i)p[i]=(unsigned char)v; return d;}\n"
                        "extern \"C\" void mainCRTStartup(){int r=invoke_ns(); ExitProcess(r==0 && step==4 && !bad && slots[0]!=slots[1] ? 0 : 1);}\n";
                    harness.close();
                    if (!integratedArchives) std::cerr << "Integrated local archives: " << localGuardError << '\n';
                    ok &= expect(integratedArchives && !entry.empty() && static_cast<bool>(harness),
                                 "integrated local and temporary objects are archived with lifetime bodies");
                }
                nativesharp::frontend::ProjectCompilation localCopy;
                std::string localCopyError;
                const bool localCopyConfigured = configureRealConstruction(localCopy, localCopyError) &&
                    localCopy.enableExperimentalCppLocalLifetimeAnalysisForTests(localCopyError);
                localCopy.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return "int Main() { Demo.Ready a = new Demo.Ready(1); Demo.Ready b = a; return 0; }\n";
                    return std::nullopt;
                });
                const bool localCopyCompiled = localCopyConfigured && localCopy.compileProjectOopMir(path, localCopyError);
                bool localCopyDiagnostic = false;
                for (const auto& diagnostic : localCopy.diagnostics().diagnostics())
                    localCopyDiagnostic |= diagnostic.message().find("cannot be copied into local 'b'") != std::string::npos;
                ok &= expect(localCopyConfigured && !localCopyCompiled && localCopyDiagnostic,
                             "experimental C++ local copies cannot reach MIR");
                nativesharp::frontend::ProjectCompilation realConstruction;
                std::string realConstructionError;
                const bool realConfigured = configureRealConstruction(realConstruction, realConstructionError);
                const bool realReady = realConfigured && realConstruction.compileProjectConstructions(path, realConstructionError);
                std::vector<CppImplicitLifetimeEmissionRequest> realPlan;
                const bool realSelected = realReady && realConstruction.constructionModel() &&
                    realConstruction.nameLookup() && realConstruction.memberModel() && realConstruction.projectGraph() &&
                    planCppImplicitLifetimeFromBindings(*realConstruction.projectGraph(), realConstruction.cppProjectSessions(),
                        *realConstruction.nameLookup(), *realConstruction.memberModel(),
                        realConstruction.constructionModel()->constructions(), {}, realPlan, realConstructionError) &&
                    realPlan.size() == 1 && realPlan[0].qualifiedType == "Demo::Implicit" &&
                    realPlan[0].kind == CppImplicitLifetimeKind::DefaultConstructor;
                const auto realOutput = std::filesystem::path(argv[2]) / "cpp-fixtures" / "lifetime-emission" / "real-selection.obj";
                const bool emittedRealSelection = realSelected &&
                    nativesharp::llvm_backend::emitCppImplicitLifetimeObject(*realPlan[0].session, realPlan[0].qualifiedType,
                        realPlan[0].kind, nativesharp::llvm_backend::LlvmOptimizationLevel::O0, realOutput, realConstructionError);
                nativesharp::frontend::ProjectObjectLayoutModel isolatedLayout;
                nativesharp::diagnostics::DiagnosticEngine isolatedLayoutDiagnostics;
                const bool isolatedLayoutBuilt = realReady && realConstruction.inheritanceModel() &&
                    isolatedLayout.build(*realConstruction.nameLookup(), *realConstruction.memberModel(),
                        *realConstruction.inheritanceModel(), *realConstruction.constructionModel(), isolatedLayoutDiagnostics);
                const auto realBindings = realReady && realConstruction.constructionModel()
                    ? realConstruction.constructionModel()->constructions()
                    : std::span<const nativesharp::frontend::ObjectConstructionBinding>{};
                const auto* cppConstruction = realBindings.empty() ? nullptr : &realBindings.front();
                const auto importedPhysicalLayout = isolatedLayoutBuilt && cppConstruction
                    ? isolatedLayout.valueLayoutForTypeKey(*realConstruction.nameLookup(), cppConstruction->targetTypeKey)
                    : std::nullopt;
                ok &= expect(isolatedLayoutBuilt && cppConstruction && importedPhysicalLayout.has_value() &&
                             importedPhysicalLayout->first > 0 && importedPhysicalLayout->second > 0 &&
                             isolatedLayout.structLayoutForType(cppConstruction->targetTypeOrdinal) == nullptr,
                             "C++ physical layout is available independently from the NativeSharp struct layout");
                nativesharp::frontend::ProjectOopHirModel isolatedHir;
                std::string isolatedHirError;
                const bool isolatedHirBuilt = isolatedLayoutBuilt && realConstruction.interfaceModel() &&
                    realConstruction.methodBodyModel() && isolatedHir.build(*realConstruction.nameLookup(),
                        *realConstruction.memberModel(), *realConstruction.inheritanceModel(),
                        *realConstruction.interfaceModel(), *realConstruction.methodBodyModel(),
                        *realConstruction.constructionModel(), isolatedLayout, isolatedHirError,
                        realConstruction.genericConstraintModel(), 1, realConstruction.namespaceFunctionModel());
                if (!isolatedHirBuilt) std::cerr << "Isolated C++ construction HIR: " << isolatedHirError << '\n';
                const auto* selectedHirConstructor = isolatedHirBuilt && cppConstruction &&
                    cppConstruction->selectedConstructorMemberOrdinal
                    ? isolatedHir.functionForMember(*cppConstruction->selectedConstructorMemberOrdinal) : nullptr;
                bool hirConstructionObserved = false;
                if (isolatedHirBuilt && cppConstruction)
                {
                    std::vector<const nativesharp::frontend::ProjectOopHirNode*> pending;
                    for (const auto& function : isolatedHir.functions())
                        if (function.body) pending.push_back(function.body.get());
                    while (!pending.empty())
                    {
                        const auto* node = pending.back();
                        pending.pop_back();
                        if (node->kind == nativesharp::frontend::ProjectOopHirNodeKind::ValueConstruction &&
                            node->memberOrdinal == cppConstruction->selectedConstructorMemberOrdinal &&
                            node->targetTypeOrdinal == cppConstruction->targetTypeOrdinal)
                            hirConstructionObserved = true;
                        for (const auto& child : node->children) if (child) pending.push_back(child.get());
                    }
                }
                ok &= expect(isolatedHirBuilt && hirConstructionObserved && selectedHirConstructor &&
                             selectedHirConstructor->isCppExternal &&
                             selectedHirConstructor->mangledName == "??0Implicit@Demo@@QEAA@XZ",
                             "isolated HIR preserves the selected external C++ constructor and its ABI symbol");
                const auto realDestructors = cppConstruction
                    ? realConstruction.memberModel()->findMembers(cppConstruction->targetTypeOrdinal, ".dtor")
                    : std::span<const std::size_t>{};
                const auto* selectedHirDestructor = isolatedHirBuilt && realDestructors.size() == 1
                    ? isolatedHir.functionForMember(realDestructors[0]) : nullptr;
                ok &= expect(selectedHirDestructor && selectedHirDestructor->destructor &&
                             !selectedHirDestructor->constructor && selectedHirDestructor->isCppExternal &&
                             selectedHirDestructor->returnTypeKey == "builtin:void" &&
                             selectedHirDestructor->mangledName == "??1Implicit@Demo@@QEAA@XZ",
                             "isolated HIR preserves the C++ destructor identity and ABI symbol");
                nativesharp::frontend::ProjectOopMirModel isolatedMir;
                std::string isolatedMirError;
                const bool isolatedMirBuilt = isolatedHirBuilt && realConstruction.virtualDispatchModel() &&
                    realConstruction.destructionModel() && isolatedMir.build(*realConstruction.nameLookup(),
                        *realConstruction.memberModel(), isolatedLayout, *realConstruction.virtualDispatchModel(),
                        *realConstruction.interfaceModel(), *realConstruction.destructionModel(), isolatedHir,
                        *realConstruction.namespaceFunctionModel(), isolatedMirError);
                if (!isolatedMirBuilt) std::cerr << "Isolated C++ construction MIR: " << isolatedMirError << '\n';
                bool mirConstructionObserved = false;
                if (isolatedMirBuilt && cppConstruction && importedPhysicalLayout)
                {
                    std::vector<const nativesharp::frontend::ProjectOopMirNode*> pending;
                    for (const auto& function : isolatedMir.functions())
                        if (function.body) pending.push_back(function.body.get());
                    while (!pending.empty())
                    {
                        const auto* node = pending.back();
                        pending.pop_back();
                        if (node->kind == nativesharp::frontend::ProjectOopMirNodeKind::ValueConstruct &&
                            node->memberOrdinal == cppConstruction->selectedConstructorMemberOrdinal &&
                            node->immediate == importedPhysicalLayout->first &&
                            node->alignment == importedPhysicalLayout->second && node->zeroInitialize &&
                            node->calleeName == "??0Implicit@Demo@@QEAA@XZ")
                            mirConstructionObserved = true;
                        for (const auto& child : node->children) if (child) pending.push_back(child.get());
                    }
                }
                ok &= expect(isolatedMirBuilt && mirConstructionObserved,
                             "isolated MIR uses Clang layout and selected implicit C++ constructor");
                bool mirDestructorObserved = false;
                if (isolatedMirBuilt && realDestructors.size() == 1)
                    for (const auto& function : isolatedMir.functions())
                        mirDestructorObserved |= function.memberOrdinal == realDestructors[0] &&
                            function.destructor && function.isCppExternal &&
                            function.returnTypeKey == "builtin:void" &&
                            function.mangledName == "??1Implicit@Demo@@QEAA@XZ";
                ok &= expect(mirDestructorObserved, "isolated MIR preserves the external C++ destructor identity");
                nativesharp::llvm_backend::ProjectOopLlvmLowerer isolatedLowerer;
                auto isolatedModule = isolatedMirBuilt
                    ? isolatedLowerer.lower(*realConstruction.nameLookup(), isolatedMir) : nullptr;
                std::string isolatedLlvmError;
                const bool isolatedModuleValid = isolatedModule && isolatedModule->verify(isolatedLlvmError);
                const auto isolatedIr = isolatedModule ? isolatedModule->text() : std::string{};
                const bool constructorCalled =
                    isolatedIr.find("call ptr @\"??0Implicit@Demo@@QEAA@XZ\"") != std::string::npos ||
                    isolatedIr.find("invoke ptr @\"??0Implicit@Demo@@QEAA@XZ\"") != std::string::npos;
                const auto constructorCall = isolatedIr.find("call ptr @\"??0Implicit@Demo@@QEAA@XZ\"") != std::string::npos
                    ? isolatedIr.find("call ptr @\"??0Implicit@Demo@@QEAA@XZ\"")
                    : isolatedIr.find("invoke ptr @\"??0Implicit@Demo@@QEAA@XZ\"");
                const auto destructorCall = isolatedIr.find("call void @\"??1Implicit@Demo@@QEAA@XZ\"") != std::string::npos
                    ? isolatedIr.find("call void @\"??1Implicit@Demo@@QEAA@XZ\"")
                    : isolatedIr.find("invoke void @\"??1Implicit@Demo@@QEAA@XZ\"");
                if (!isolatedModuleValid) std::cerr << "Isolated C++ construction LLVM: " << isolatedLlvmError << '\n';
                ok &= expect(isolatedModuleValid && isolatedIr.find("alloca [4 x i8]") != std::string::npos &&
                             isolatedIr.find("declare ptr @\"??0Implicit@Demo@@QEAA@XZ\"(ptr") != std::string::npos &&
                             isolatedIr.find("declare void @\"??1Implicit@Demo@@QEAA@XZ\"(ptr") != std::string::npos &&
                             constructorCalled && destructorCall != std::string::npos &&
                             constructorCall < destructorCall && isolatedIr.find("struct.new.value") == std::string::npos,
                             "discarded C++ temporary is destroyed in place after successful construction");
                nativesharp::frontend::ProjectCompilation observedProject;
                std::string observedError;
                const bool observedConfigured = configureRealConstruction(observedProject, observedError) &&
                    observedProject.enableExperimentalCppDiscardedLifetimeLoweringForTests(observedError);
                observedProject.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return "int Main() { new Demo.Observed(); new Demo.Observed(); return 0; }\n";
                    return std::nullopt;
                });
                const bool observedMirBuilt = observedConfigured && observedProject.compileProjectOopMir(path, observedError) &&
                    observedProject.oopMirModel();
                const auto* observedMir = observedProject.oopMirModel();
                std::vector<CppImplicitLifetimeEmissionRequest> observedPlan;
                const bool observedPlanned = observedMirBuilt &&
                    planCppImplicitLifetimeFromValues(*observedProject.projectGraph(), observedProject.cppProjectSessions(),
                        *observedProject.nameLookup(), *observedProject.memberModel(),
                        observedProject.constructionModel()->constructions(), *observedMir, observedPlan, observedError) &&
                    observedPlan.size() == 2 && observedPlan[0].qualifiedType == "Demo::Observed" &&
                    observedPlan[0].kind == CppImplicitLifetimeKind::DefaultConstructor &&
                    observedPlan[1].qualifiedType == "Demo::Observed" &&
                    observedPlan[1].kind == CppImplicitLifetimeKind::Destructor;
                const bool rejectsMissingConstruction = observedPlanned &&
                    !planCppImplicitLifetimeFromValues(*observedProject.projectGraph(), observedProject.cppProjectSessions(),
                        *observedProject.nameLookup(), *observedProject.memberModel(), {}, *observedMir, observedPlan, observedError) &&
                    observedPlan.size() == 2;
                ok &= expect(rejectsMissingConstruction,
                             "discarded value lifetime planning rejects missing bindings without changing the prior plan");
                nativesharp::llvm_backend::ProjectOopObjectGenerator observedGenerator;
                auto ownedBundle = observedPlanned ? observedGenerator.generate(*observedProject.nameLookup(),
                    *observedProject.memberModel(), *observedMir, observedProject.sourceManager(),
                    nativesharp::llvm_backend::DebugInformationKind::None,
                    nativesharp::llvm_backend::LlvmOptimizationLevel::O0, 1, {}, observedError) : nullptr;
                bool bundlePublished = false, bundleFailurePreserved = false, bundleCleaned = false;
                std::vector<std::filesystem::path> ownedPaths;
                if (ownedBundle)
                {
                    const auto initialCount = ownedBundle->objectFiles().size();
                    bundleFailurePreserved = !ownedBundle->appendGeneratedObjects(1,
                        [](std::span<const std::filesystem::path>, std::string& error) {
                            error = "intentional emitter failure"; return false;
                        }, observedError) && ownedBundle->objectFiles().size() == initialCount;
                    nativesharp::frontend::ProjectOopMirModel unbuiltMir;
                    const bool rejectedUnbuiltMir = !nativesharp::driver::appendCppImplicitLifetimeObjects(
                        observedProject, unbuiltMir, *ownedBundle, nativesharp::llvm_backend::LlvmOptimizationLevel::O0,
                        observedError) && ownedBundle->objectFiles().size() == initialCount;
                    nativesharp::frontend::ProjectCompilation projectWithoutCppSessions;
                    const bool noSessionsIsNoOp = nativesharp::driver::appendCppImplicitLifetimeObjects(
                        projectWithoutCppSessions, unbuiltMir, *ownedBundle,
                        nativesharp::llvm_backend::LlvmOptimizationLevel::O0, observedError) &&
                        ownedBundle->objectFiles().size() == initialCount;
                    bundleFailurePreserved &= rejectedUnbuiltMir && noSessionsIsNoOp;
                    bundlePublished = nativesharp::driver::appendCppImplicitLifetimeObjects(observedProject,
                        *observedMir, *ownedBundle, nativesharp::llvm_backend::LlvmOptimizationLevel::O0, observedError) &&
                        ownedBundle->objectFiles().size() == initialCount + observedPlan.size();
                    if (bundlePublished)
                        ownedPaths.assign(ownedBundle->objectFiles().begin() + initialCount, ownedBundle->objectFiles().end());
                    bundlePublished &= ownedPaths.size() == observedPlan.size() &&
                        std::all_of(ownedPaths.begin(), ownedPaths.end(), [](const auto& path) { return std::filesystem::is_regular_file(path); });
                    if (argc > 4 && (std::string_view(argv[4]) == "lifetime" || std::string_view(argv[4]) == "bundle-link"))
                    {
                        nativesharp::linker::WindowsLibraryBuilder archiveBuilder;
                        archiveBuilder.setRuntimeLibrary("A:/nsc-interop-build/tanda1-ON/generated/NativeSharp.Runtime.Win32.lib");
                        const auto archivePath = std::filesystem::path(argv[2]) / "cpp-fixtures" / "lifetime-emission" / "observed-bundle.lib";
                        const bool archiveBuilt = bundlePublished && archiveBuilder.buildStaticObjects(
                            ownedBundle->objectFiles(), archivePath,
                            nativesharp::llvm_backend::LlvmOptimizationLevel::O0, observedError);
                        if (!archiveBuilt) std::cerr << "C++ lifetime archive: " << observedError << '\n';
                        ok &= expect(archiveBuilt, "NativeSharp static archive includes the C++ lifetime object bundle");
                    }
                    ownedBundle.reset();
                    bundleCleaned = std::all_of(ownedPaths.begin(), ownedPaths.end(), [](const auto& path) { return !std::filesystem::exists(path); });
                }
                if (!bundlePublished) std::cerr << "Owned C++ object bundle: " << observedError << '\n';
                ok &= expect(bundleFailurePreserved && bundlePublished && bundleCleaned,
                             "C++ lifetime objects publish atomically in an owned bundle and are removed with it");
                for (const auto level : {nativesharp::llvm_backend::LlvmOptimizationLevel::O0,
                                         nativesharp::llvm_backend::LlvmOptimizationLevel::O2})
                {
                    auto thinBundle = observedPlanned ? observedGenerator.generate(*observedProject.nameLookup(),
                        *observedProject.memberModel(), *observedMir, observedProject.sourceManager(),
                        nativesharp::llvm_backend::DebugInformationKind::None, level, 1, {},
                        nativesharp::llvm_backend::LinkTimeOptimizationMode::Thin, observedError) : nullptr;
                    const auto before = thinBundle ? thinBundle->objectFiles().size() : 0;
                    const bool rejected = thinBundle && before != 0 &&
                        !nativesharp::driver::appendCppImplicitLifetimeObjects(observedProject, *observedMir,
                            *thinBundle, level, observedError) &&
                        observedError.find("generated native objects are not yet supported in ThinLTO bundles") != std::string::npos &&
                        thinBundle->objectFiles().size() == before;
                    if (!rejected) std::cerr << "ThinLTO C++ lifetime gate: " << observedError << '\n';
                    ok &= expect(rejected, "ThinLTO bundle rejects C++ lifetime objects without publishing them");
                }
                if (argc > 4 && std::string_view(argv[4]) == "bundle-link")
                {
                nativesharp::frontend::ProjectCompilation linkProject;
                std::string linkError;
                const bool linkConfigured = configureRealConstruction(linkProject, linkError) &&
                    linkProject.enableExperimentalCppDiscardedLifetimeLoweringForTests(linkError);
                linkProject.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return "int Main() { new Demo.Observed(); new Demo.Observed(); return 17; }\n";
                    return std::nullopt;
                });
                const bool linkMirBuilt = linkConfigured && linkProject.compileProjectOopMir(path, linkError) &&
                    linkProject.oopMirModel();
                auto linkBundle = linkMirBuilt ? observedGenerator.generate(*linkProject.nameLookup(),
                    *linkProject.memberModel(), *linkProject.oopMirModel(), linkProject.sourceManager(),
                    nativesharp::llvm_backend::DebugInformationKind::None,
                    nativesharp::llvm_backend::LlvmOptimizationLevel::O0, 1, {}, linkError) : nullptr;
                const bool linkObjectsAdded = linkBundle && nativesharp::driver::appendCppImplicitLifetimeObjects(
                    linkProject, *linkProject.oopMirModel(), *linkBundle,
                    nativesharp::llvm_backend::LlvmOptimizationLevel::O0, linkError);
                std::string linkMain;
                if (linkMirBuilt)
                    for (const auto& function : linkProject.oopMirModel()->functions())
                        if (function.namespaceFunctionOrdinal && function.returnTypeKey == "builtin:int") linkMain = function.mangledName;
                const auto linkDir = std::filesystem::path(argv[2]) / "cpp-fixtures" / "lifetime-emission";
                nativesharp::linker::WindowsExecutableBuilder linkBuilder;
                linkBuilder.setExtraLinkLibraries({"A:/vc2026-runtime/lib/x64/release/libcmt.lib",
                    "A:/vc2026-runtime/lib/x64/release/libvcruntime.lib",
                    "A:/vc2026-runtime/windows-sdk/10.0.28000.0/lib/ucrt/x64/libucrt.lib",
                    "A:/vc2026-runtime/windows-sdk/10.0.28000.0/lib/um/x64/kernel32.lib"});
                std::vector<std::filesystem::path> linkObjects;
                if (linkObjectsAdded) linkObjects.assign(linkBundle->objectFiles().begin(), linkBundle->objectFiles().end());
                linkObjects.push_back(linkDir / "bundle-members.obj");
                const bool executableLinked = linkObjectsAdded && !linkMain.empty() && linkBuilder.buildObjects(
                    linkObjects, linkBundle->combinedLlvmText(), linkDir / "bundle-linked.exe",
                    nativesharp::llvm_backend::LlvmOptimizationLevel::O0, linkMain, linkError);
                if (!executableLinked) std::cerr << "NativeSharp bundle link: " << linkError << '\n';
                ok &= expect(executableLinked, "NativeSharp executable builder links the generated C++ lifetime bundle");
                }
                nativesharp::llvm_backend::ProjectOopLlvmLowerer observedLowerer;
                auto observedModule = observedPlanned ? observedLowerer.lower(*observedProject.nameLookup(), *observedMir) : nullptr;
                const bool observedVerified = observedModule && observedModule->verify(observedError);
                const auto observedDir = std::filesystem::path(argv[2]) / "cpp-fixtures" / "lifetime-emission";
                bool observedEmitted = observedVerified;
                for (int opt : {0, 2})
                {
                    const auto level = opt == 0 ? nativesharp::llvm_backend::LlvmOptimizationLevel::O0 : nativesharp::llvm_backend::LlvmOptimizationLevel::O2;
                    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(level, observedError);
                    observedEmitted &= target && observedModule && target->emit(*observedModule,
                        observedDir / ("observed-ns-O" + std::to_string(opt) + ".obj"),
                        nativesharp::llvm_backend::LlvmCodeGenFileType::Object, observedError);
                    std::vector<std::filesystem::path> destinations;
                    for (const auto& request : observedPlan)
                    {
                        const auto part = request.kind == CppImplicitLifetimeKind::DefaultConstructor ? "ctor" : "dtor";
                        destinations.push_back(observedDir / ("observed-" + std::string(part) + "-O" + std::to_string(opt) + ".obj"));
                    }
                    std::vector<std::filesystem::path> published;
                    observedEmitted &= nativesharp::llvm_backend::emitCppImplicitLifetimeObjects(
                        observedPlan, destinations, level, published, observedError) && published == destinations;
                    if (destinations.size() == 2)
                    {
                        const auto before = published;
                        destinations[1] = destinations[0];
                        ok &= expect(!nativesharp::llvm_backend::emitCppImplicitLifetimeObjects(
                                         observedPlan, destinations, level, published, observedError) && published == before,
                                     "lifetime batch rejects duplicate outputs without publishing a partial list");
                        destinations[1] = destinations[0].parent_path() /
                            ("OBSERVED-CTOR-O" + std::to_string(opt) + ".OBJ");
                        ok &= expect(!nativesharp::llvm_backend::emitCppImplicitLifetimeObjects(
                                         observedPlan, destinations, level, published, observedError) && published == before,
                                     "lifetime batch rejects case aliases on Windows");
                        const auto hardLink = observedDir / ("observed-hardlink-" +
                            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".obj");
                        std::error_code linkError;
                        std::filesystem::create_hard_link(destinations[0], hardLink, linkError);
                        if (!linkError)
                        {
                            destinations[1] = hardLink;
                            ok &= expect(!nativesharp::llvm_backend::emitCppImplicitLifetimeObjects(
                                             observedPlan, destinations, level, published, observedError) && published == before,
                                         "lifetime batch rejects hard-link aliases");
                            std::filesystem::remove(hardLink, linkError);
                            ok &= expect(!linkError, "temporary hard-link fixture is removed");
                        }
                        else std::cerr << "Hard-link alias test skipped: " << linkError.message() << '\n';
                    }
                }
                std::string observedMain;
                if (observedMirBuilt)
                    for (const auto& function : observedMir->functions())
                        if (function.namespaceFunctionOrdinal && function.returnTypeKey == "builtin:int") observedMain = function.mangledName;
                std::ofstream observedHarness(observedDir / "observed-harness.cpp");
                observedHarness << "namespace Demo { struct CountedMember { int value; CountedMember() noexcept(false); ~CountedMember() noexcept(false); }; struct FailingMember { int value; FailingMember() noexcept(false); ~FailingMember() noexcept(false); }; }\n"
                    "extern \"C\" __declspec(dllimport) void __stdcall RaiseException(unsigned long, unsigned long, unsigned long, const unsigned long long*);\n"
                    "static int stage = 0; Demo::CountedMember::CountedMember() noexcept(false) { stage = (stage == 0 || stage == 4) ? stage + 1 : 99;\n"
                    "#ifdef TRIGGER_FAILURE\n RaiseException(0xE0424242, 0, 0, nullptr);\n#endif\n }\n"
                    "Demo::CountedMember::~CountedMember() noexcept(false) {\n#ifdef CPP_THROW\n stage = stage == 2 ? 3 : 99;\n#else\n stage = (stage == 3 || stage == 7) ? stage + 1 : 99;\n#endif\n }\n"
                    "Demo::FailingMember::FailingMember() noexcept(false) { stage = (stage == 1 || stage == 5) ? stage + 1 : 99;\n#ifdef CPP_THROW\n throw 123;\n#endif\n }\n"
                    "Demo::FailingMember::~FailingMember() noexcept(false) { stage = (stage == 2 || stage == 6) ? stage + 1 : 99; }\n"
                    "extern \"C\" int invoke_ns() __asm__(\"" << observedMain << "\");\n"
                    "#ifdef CPP_THROW\n int main() { try { invoke_ns(); } catch (int value) { return value == 123 && stage == 3 ? 0 : 3; } return 4; }\n"
                    "#else\n int main() {\n#ifdef TRIGGER_FAILURE\n int caught = 0; __try { invoke_ns(); } __except(1) { caught = 1; } return caught && stage == 1 ? 0 : 2;\n#else\n int result = invoke_ns(); return result == 0 && stage == 8 ? 0 : 1;\n#endif\n }\n#endif\n";
                observedHarness.close();
                if (!observedEmitted || observedMain.empty())
                    std::cerr << "Observed lifetime object: " << observedError << ' ' << observedLowerer.error() << '\n';
                ok &= expect(observedEmitted && !observedMain.empty() && static_cast<bool>(observedHarness),
                             "real NativeSharp temporary emits objects for nontrivial implicit C++ lifetime");
                nativesharp::frontend::ProjectCompilation declarationProject;
                std::string declarationError;
                const bool declarationConfigured = configureRealConstruction(declarationProject, declarationError);
                declarationProject.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return "int Main() { return 0; }\n";
                    return std::nullopt;
                });
                const bool declarationReady = declarationConfigured && declarationProject.compileProjectConstructions(path, declarationError);
                nativesharp::diagnostics::DiagnosticEngine declarationDiagnostics;
                nativesharp::frontend::ProjectObjectLayoutModel declarationLayout;
                const bool declarationLayoutBuilt = declarationReady && declarationLayout.build(
                    *declarationProject.nameLookup(), *declarationProject.memberModel(),
                    *declarationProject.inheritanceModel(), *declarationProject.constructionModel(), declarationDiagnostics);
                nativesharp::frontend::ProjectOopHirModel declarationHir;
                const bool declarationHirBuilt = declarationLayoutBuilt && declarationHir.build(
                    *declarationProject.nameLookup(), *declarationProject.memberModel(),
                    *declarationProject.inheritanceModel(), *declarationProject.interfaceModel(),
                    *declarationProject.methodBodyModel(), *declarationProject.constructionModel(),
                    declarationLayout, declarationError, declarationProject.genericConstraintModel(), 1,
                    declarationProject.namespaceFunctionModel());
                nativesharp::frontend::ProjectOopMirModel declarationMir;
                const bool declarationMirBuilt = declarationHirBuilt && declarationProject.virtualDispatchModel() &&
                    declarationProject.destructionModel() && declarationMir.build(*declarationProject.nameLookup(),
                        *declarationProject.memberModel(), declarationLayout, *declarationProject.virtualDispatchModel(),
                        *declarationProject.interfaceModel(), *declarationProject.destructionModel(), declarationHir,
                        *declarationProject.namespaceFunctionModel(), declarationError);
                nativesharp::llvm_backend::ProjectOopLlvmLowerer declarationLowerer;
                auto declarationModule = declarationMirBuilt
                    ? declarationLowerer.lower(*declarationProject.nameLookup(), declarationMir) : nullptr;
                const bool declarationVerified = declarationModule && declarationModule->verify(declarationError);
                const auto declarationIr = declarationModule ? declarationModule->text() : std::string{};
                if (!declarationVerified) std::cerr << "Isolated C++ constructor declaration: " << declarationError << '\n';
                ok &= expect(declarationVerified && declarationIr.find("declare ptr @\"??0Implicit@Demo@@QEAA@XZ\"(ptr") != std::string::npos,
                             "external C++ constructor declaration returns this pointer in Windows ABI");
                if (!realSelected) std::cerr << "Real C++ construction analysis: " << realConstructionError << '\n';
                ok &= expect(realSelected && emittedRealSelection &&
                             !realConstruction.enableExperimentalCppLifetimeAnalysisForTests(realConstructionError),
                             "NativeSharp construction selects and emits the observed implicit C++ constructor");
                nativesharp::frontend::ProjectCompilation missingLifetimeProvider;
                std::string missingProviderError;
                ok &= expect(!missingLifetimeProvider.enableExperimentalCppLifetimeAnalysisForTests(missingProviderError),
                             "experimental lifetime analysis requires its provider");
                ok &= expect(!missingLifetimeProvider.enableExperimentalCppDiscardedLifetimeLoweringForTests(missingProviderError),
                             "experimental discarded lifetime lowering requires its provider");
                nativesharp::frontend::ProjectCompilation unsupportedValue;
                std::string unsupportedValueError;
                const bool unsupportedConfigured = configureRealConstruction(unsupportedValue, unsupportedValueError) &&
                    unsupportedValue.enableExperimentalCppDiscardedLifetimeLoweringForTests(unsupportedValueError);
                unsupportedValue.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return "int Main() { Demo.Observed value = new Demo.Observed(); return 0; }\n";
                    return std::nullopt;
                });
                const bool unsupportedReachedMir = unsupportedConfigured && unsupportedValue.compileProjectOopMir(path, unsupportedValueError);
                ok &= expect(unsupportedConfigured && !unsupportedReachedMir,
                             "C++ value stored in a local does not reach MIR through discarded-only lowering");
                nativesharp::frontend::ProjectCompilation explicitConstruction;
                std::string explicitConstructionError;
                const bool explicitConfigured = configureRealConstruction(explicitConstruction, explicitConstructionError) &&
                    explicitConstruction.enableExperimentalCppDiscardedLifetimeLoweringForTests(explicitConstructionError);
                explicitConstruction.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return "int Main() { new Demo.Ready(1); return 17; }\n";
                    return std::nullopt;
                });
                const bool explicitReachedMir = explicitConfigured && explicitConstruction.compileProjectOopMir(path, explicitConstructionError);
                std::vector<CppImplicitLifetimeEmissionRequest> explicitPlan;
                const bool explicitPlanned = explicitReachedMir && explicitConstruction.oopMirModel() &&
                    planCppImplicitLifetimeFromValues(*explicitConstruction.projectGraph(),
                        explicitConstruction.cppProjectSessions(), *explicitConstruction.nameLookup(),
                        *explicitConstruction.memberModel(), explicitConstruction.constructionModel()->constructions(),
                        *explicitConstruction.oopMirModel(), explicitPlan, explicitConstructionError) && explicitPlan.empty();
                nativesharp::llvm_backend::ProjectOopLlvmLowerer explicitLowerer;
                auto explicitModule = explicitPlanned ? explicitLowerer.lower(*explicitConstruction.nameLookup(),
                    *explicitConstruction.oopMirModel()) : nullptr;
                bool explicitEmitted = explicitModule && explicitModule->verify(explicitConstructionError);
                for (int opt : {0, 2})
                {
                    const auto level = opt == 0 ? nativesharp::llvm_backend::LlvmOptimizationLevel::O0 :
                        nativesharp::llvm_backend::LlvmOptimizationLevel::O2;
                    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(level, explicitConstructionError);
                    explicitEmitted &= target && explicitModule && target->emit(*explicitModule,
                        observedDir / ("explicit-ready-ns-O" + std::to_string(opt) + ".obj"),
                        nativesharp::llvm_backend::LlvmCodeGenFileType::Object, explicitConstructionError);
                }
                std::string explicitMain;
                if (explicitReachedMir)
                    for (const auto& function : explicitConstruction.oopMirModel()->functions())
                        if (function.namespaceFunctionOrdinal && function.returnTypeKey == "builtin:int") explicitMain = function.mangledName;
                std::ofstream explicitHarness(observedDir / "explicit-ready-harness.cpp");
                explicitHarness << "namespace Demo { struct Ready { Ready(int) noexcept; ~Ready() noexcept; }; }\n"
                    "static int stage = 0; Demo::Ready::Ready(int value) noexcept { stage = value == 1 ? 1 : 99; }\n"
                    "Demo::Ready::~Ready() noexcept { stage = stage == 1 ? 2 : 99; }\n"
                    "extern \"C\" int invoke_ns() __asm__(\"" << explicitMain << "\");\n"
                    "extern \"C\" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);\n"
                    "extern \"C\" void* memset(void* dst, int value, unsigned long long size) { unsigned char* p = (unsigned char*)dst; for (unsigned long long i = 0; i < size; ++i) p[i] = (unsigned char)value; return dst; }\n"
                    "extern \"C\" void mainCRTStartup() { int result = invoke_ns(); ExitProcess(result == 17 && stage == 2 ? 0 : 1); }\n";
                explicitHarness.close();
                if (!explicitEmitted || explicitMain.empty())
                    std::cerr << "Explicit C++ construction: " << explicitConstructionError << ' ' << explicitLowerer.error() << '\n';
                ok &= expect(explicitEmitted && !explicitMain.empty() && static_cast<bool>(explicitHarness),
                             "discarded C++ value selects external int constructor and destructor without implicit emission");
                if (argc > 4 && (std::string_view(argv[4]) == "lifetime" || std::string_view(argv[4]) == "bundle-link"))
                {
                auto explicitBundle = explicitPlanned ? observedGenerator.generate(*explicitConstruction.nameLookup(),
                    *explicitConstruction.memberModel(), *explicitConstruction.oopMirModel(), explicitConstruction.sourceManager(),
                    nativesharp::llvm_backend::DebugInformationKind::None,
                    nativesharp::llvm_backend::LlvmOptimizationLevel::O0, 1, {}, explicitConstructionError) : nullptr;
                const bool explicitBundleReady = explicitBundle && nativesharp::driver::appendCppImplicitLifetimeObjects(
                    explicitConstruction, *explicitConstruction.oopMirModel(), *explicitBundle,
                    nativesharp::llvm_backend::LlvmOptimizationLevel::O0, explicitConstructionError);
                std::vector<std::filesystem::path> explicitObjects;
                if (explicitBundleReady)
                    explicitObjects.assign(explicitBundle->objectFiles().begin(), explicitBundle->objectFiles().end());
                explicitObjects.push_back(observedDir / "explicit-ready-members.obj");
                nativesharp::linker::WindowsExecutableBuilder explicitBuilder;
                const bool explicitLinked = explicitBundleReady && !explicitMain.empty() && explicitBuilder.buildObjects(
                    explicitObjects, explicitBundle->combinedLlvmText(), observedDir / "explicit-ready-builder.exe",
                    nativesharp::llvm_backend::LlvmOptimizationLevel::O0, explicitMain, explicitConstructionError);
                if (!explicitLinked) std::cerr << "Explicit C++ builder link: " << explicitConstructionError << '\n';
                ok &= expect(explicitLinked, "NativeSharp executable builder links explicit C++ lifetime");
                }
                nativesharp::frontend::ProjectCompilation realConstructionGuard;
                std::string realGuardError;
                const bool realGuardConfigured = configureRealConstruction(realConstructionGuard, realGuardError);
                const bool realGuardEscaped = realGuardConfigured && realConstructionGuard.compileProjectObjectLayouts(path, realGuardError);
                ok &= expect(realGuardConfigured && !realGuardEscaped &&
                             realGuardError.find("cannot proceed to layout") != std::string::npos,
                             "experimental C++ construction analysis remains blocked before code generation");
                nativesharp::frontend::ProjectCompilation guardedMethods;
                std::string guardError;
                const bool guardConfigured = configure(guardedMethods, guardError);
                const bool emitted = guardConfigured && guardedMethods.compileProjectOopMir(path, guardError);
                ok &= expect(guardConfigured && !emitted && guardError.find("not yet supported beyond") != std::string::npos,
                             "experimental C++ instance methods cannot reach code generation yet");
                nativesharp::frontend::ProjectCompilation automaticFields;
                std::string fieldsError;
                const bool fieldConfigured = configure(automaticFields, fieldsError) &&
                    automaticFields.setExperimentalCppInstanceFieldProvider(provideExperimentalCppInstanceFields, fieldsError) &&
                    automaticFields.enableExperimentalCppInstanceMethodLoweringForTests(fieldsError);
                const bool fieldsReady = fieldConfigured && automaticFields.compileProjectMembers(path, fieldsError);
                const auto* fieldNames = automaticFields.nameLookup();
                const auto* fieldMembers = automaticFields.memberModel();
                const auto fieldOwner = fieldNames ? fieldNames->typeOrdinalForIdentity("Semantic::Demo.Widget") : std::nullopt;
                const auto valueFields = fieldOwner && fieldMembers ? fieldMembers->findMembers(*fieldOwner, "value") : std::span<const std::size_t>{};
                const auto* valueField = valueFields.size() == 1 ? &fieldMembers->members()[valueFields[0]] : nullptr;
                ok &= expect(fieldsReady && valueField && valueField->isCppExternal &&
                             valueField->explicitFieldOffset == 4 && valueField->explicitFieldAlignment == 4 &&
                             valueField->declaredTypeKey == "builtin:int" &&
                             !automaticFields.setExperimentalCppInstanceFieldProvider({}, fieldsError),
                             "project compilation obtains instance field metadata from a physical C++ header");
                bool fieldModePreserved = false;
                bool fieldUsrPreserved = false;
                if (fieldsReady && valueField && automaticFields.projectGraph())
                {
                    auto fieldRecords = collectCppSemanticRecords(*automaticFields.projectGraph(), automaticFields.cppProjectSessions());
                    if (const auto* observed = std::get_if<std::vector<CppSemanticRecordEntry>>(&fieldRecords))
                    {
                        std::optional<bool> commonMode;
                        bool found = false;
                        for (const auto& entry : *observed)
                            if (entry.identity && entry.identity->key() == "Semantic::Demo.Widget")
                            {
                                if (!found) { commonMode = entry.microsoftVolatileSemantics; found = true; }
                                else if (commonMode != entry.microsoftVolatileSemantics) commonMode = std::nullopt;
                                for (const auto& field : entry.declaration.fields)
                                    if (field.name == "value" && field.clangUsr &&
                                        field.clangUsr == valueField->cppClangUsr) fieldUsrPreserved = true;
                            }
                        fieldModePreserved = found && valueField->cppMicrosoftVolatileSemantics == commonMode;
                    }
                }
                ok &= expect(fieldModePreserved, "imported field preserves its record group's volatile mode");
                ok &= expect(fieldUsrPreserved, "imported field symbol preserves its Clang USR");
                {
                    nativesharp::frontend::ProjectCompilation recoveredFields;
                    recoveredFields.setSourceBufferProvider([](const std::filesystem::path& source)
                        -> std::optional<std::string> {
                        if (source.filename() == "Main.ns") return "int Main() { return 0; }\n";
                        return std::nullopt;
                    });
                    auto firstOptions = options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp, "");
                    auto secondOptions = options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp, "");
                    firstOptions.input = CppFileInput{methodHeader};
                    secondOptions.input = CppFileInput{methodHeader};
                    auto first = open(factory, std::move(firstOptions));
                    auto second = open(factory, std::move(secondOptions));
                    std::optional<std::string> expectedUsr;
                    if (second)
                        if (auto records = second->recordsSnapshot())
                            for (const auto& record : *records)
                                if (record.name == "Widget")
                                    for (const auto& field : record.fields)
                                        if (field.name == "value") expectedUsr = field.clangUsr;
                    bool recovered = false;
                    if (first && second && expectedUsr)
                    {
                        CppProjectSessions group;
                        group.projectFile = std::filesystem::absolute(path);
                        group.sessions.push_back(std::make_unique<MissingFieldUsrSession>(std::move(first)));
                        group.sessions.push_back(std::move(second));
                        std::vector<CppProjectSessions> groups;
                        groups.push_back(std::move(group));
                        std::string error;
                        const bool ready = recoveredFields.setCppProjectSessions(std::move(groups), error) &&
                            recoveredFields.setExperimentalCppTypeProvider(provideExperimentalCppRecordCandidates, error) &&
                            recoveredFields.setExperimentalCppTypeSelector(selectExperimentalCppRecordTypesForCompilation, error) &&
                            recoveredFields.setExperimentalCppInstanceFieldProvider(provideExperimentalCppInstanceFields, error) &&
                            recoveredFields.enableExperimentalCppInstanceFieldLoweringForTests(error) &&
                            recoveredFields.compileProjectMembers(path, error);
                        const auto* names = recoveredFields.nameLookup();
                        const auto* members = recoveredFields.memberModel();
                        const auto owner = names ? names->typeOrdinalForIdentity("Semantic::Demo.Widget") : std::nullopt;
                        const auto fields = owner && members ? members->findMembers(*owner, "value") : std::span<const std::size_t>{};
                        recovered = ready && fields.size() == 1 && members->members()[fields[0]].cppClangUsr == expectedUsr;
                    }
                    ok &= expect(recovered, "field provider recovers a later known USR into the imported symbol");
                }
                nativesharp::frontend::ProjectCompilation guardedFields;
                std::string fieldGuardError;
                const bool fieldGuardConfigured = configure(guardedFields, fieldGuardError) &&
                    guardedFields.setExperimentalCppInstanceFieldProvider(provideExperimentalCppInstanceFields, fieldGuardError) &&
                    guardedFields.enableExperimentalCppInstanceMethodLoweringForTests(fieldGuardError);
                const bool fieldsEmitted = fieldGuardConfigured && guardedFields.compileProjectOopMir(path, fieldGuardError);
                ok &= expect(fieldGuardConfigured && !fieldsEmitted && fieldGuardError.find("C++ fields") != std::string::npos,
                             "instance fields stay blocked even when method lowering tests are enabled");
                nativesharp::frontend::ProjectCompilation automaticStatics;
                std::string staticsError;
                const bool staticConfigured = configure(automaticStatics, staticsError) &&
                    automaticStatics.setExperimentalCppStaticMethodProvider(provideExperimentalCppStaticMethods, staticsError);
                const bool staticsReady = staticConfigured && automaticStatics.compileProjectMembers(path, staticsError);
                const auto* staticNames = automaticStatics.nameLookup();
                const auto* staticMembers = automaticStatics.memberModel();
                const auto staticOwner = staticNames ? staticNames->typeOrdinalForIdentity("Semantic::Demo.Widget") : std::nullopt;
                const auto scaleMethods = staticOwner && staticMembers ? staticMembers->findMembers(*staticOwner, "scale") : std::span<const std::size_t>{};
                bool scaleIndexed = scaleMethods.size() == 2;
                for (const auto index : scaleMethods)
                    scaleIndexed &= staticMembers->members()[index].isCppExternal && staticMembers->members()[index].isStatic &&
                        !staticMembers->members()[index].hasImplicitThis;
                ok &= expect(staticsReady && scaleIndexed && staticMembers->findMembers(*staticOwner, "merge").empty() &&
                             !automaticStatics.setExperimentalCppStaticMethodProvider({}, staticsError),
                             "project compilation indexes Clang static overloads without publishing mixed families");
                nativesharp::frontend::ProjectCompilation guardedStatics;
                std::string staticGuardError;
                const bool staticGuardConfigured = configure(guardedStatics, staticGuardError) &&
                    guardedStatics.setExperimentalCppStaticMethodProvider(provideExperimentalCppStaticMethods, staticGuardError);
                const bool staticsEmitted = staticGuardConfigured && guardedStatics.compileProjectOopMir(path, staticGuardError);
                ok &= expect(staticGuardConfigured && !staticsEmitted && staticGuardError.find("not yet supported beyond") != std::string::npos,
                             "static C++ methods remain protected before code generation");
                nativesharp::frontend::ProjectCompilation guardedStaticFields;
                std::string staticFieldGuardError;
                const bool staticFieldGuardConfigured = configure(guardedStaticFields, staticFieldGuardError) &&
                    guardedStaticFields.setExperimentalCppStaticFieldProvider(provideExperimentalCppStaticFields, staticFieldGuardError) &&
                    guardedStaticFields.enableExperimentalCppInstanceMethodLoweringForTests(staticFieldGuardError);
                const bool staticFieldsEmitted = staticFieldGuardConfigured && guardedStaticFields.compileProjectOopMir(path, staticFieldGuardError);
                ok &= expect(staticFieldGuardConfigured && !staticFieldsEmitted &&
                             staticFieldGuardError.find("C++ fields") != std::string::npos,
                             "static C++ fields stay protected without the field test option");
                nativesharp::frontend::ProjectCompilation constWrite;
                std::string constError;
                const bool constConfigured = configure(constWrite, constError) &&
                    constWrite.setExperimentalCppInstanceFieldProvider(provideExperimentalCppInstanceFields, constError) &&
                    constWrite.enableExperimentalCppInstanceMethodLoweringForTests(constError) &&
                    constWrite.enableExperimentalCppInstanceFieldLoweringForTests(constError);
                constWrite.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return "int Main() { Demo.Widget* p = null; p->immutable = 4; return 0; }\n";
                    return std::nullopt;
                });
                const bool constCompiled = constConfigured && constWrite.compileProjectOopMir(path, constError);
                bool readonlyDiagnostic = false;
                for (const auto& diagnostic : constWrite.diagnostics().diagnostics())
                    readonlyDiagnostic |= diagnostic.message().find("not assignable") != std::string::npos;
                ok &= expect(constConfigured && !constCompiled && readonlyDiagnostic,
                             "C++ const field cannot be assigned from NativeSharp");
                nativesharp::frontend::ProjectCompilation staticConstWrite;
                std::string staticConstError;
                const bool staticConstConfigured = configure(staticConstWrite, staticConstError) &&
                    staticConstWrite.setExperimentalCppStaticFieldProvider(provideExperimentalCppStaticFields, staticConstError) &&
                    staticConstWrite.enableExperimentalCppInstanceMethodLoweringForTests(staticConstError) &&
                    staticConstWrite.enableExperimentalCppInstanceFieldLoweringForTests(staticConstError);
                staticConstWrite.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return "int Main() { Demo.Widget.limit = 4; return 0; }\n";
                    return std::nullopt;
                });
                const bool staticConstCompiled = staticConstConfigured && staticConstWrite.compileProjectOopMir(path, staticConstError);
                bool staticReadonlyDiagnostic = false;
                for (const auto& diagnostic : staticConstWrite.diagnostics().diagnostics())
                    staticReadonlyDiagnostic |= diagnostic.message().find("not assignable") != std::string::npos;
                ok &= expect(staticConstConfigured && !staticConstCompiled && staticReadonlyDiagnostic,
                             "C++ const static field cannot be assigned from NativeSharp");
                for (int scenario : {0, 1, 2, 3, 4, 5})
                {
                    nativesharp::frontend::ProjectCompilation addressProbe;
                    std::string addressError;
                    const bool addressConfigured = configure(addressProbe, addressError) &&
                        addressProbe.setExperimentalCppInstanceFieldProvider(provideExperimentalCppInstanceFields, addressError) &&
                        addressProbe.setExperimentalCppStaticFieldProvider(provideExperimentalCppStaticFields, addressError) &&
                        addressProbe.enableExperimentalCppInstanceMethodLoweringForTests(addressError) &&
                        addressProbe.enableExperimentalCppInstanceFieldLoweringForTests(addressError);
                    addressProbe.setSourceBufferProvider([scenario](const std::filesystem::path& source) -> std::optional<std::string> {
                        if (source.filename() != "Main.ns") return std::nullopt;
                        if (scenario == 0) return "int Main() { Demo.Widget* p = null; int* q = &p->immutable; return 0; }\n";
                        if (scenario == 1) return "int Main() { Demo.Widget* p = null; int* q = &p->changing; return 0; }\n";
                        if (scenario == 2) return "int Main() { Demo.Widget* p = null; int* q = &p->value; return 0; }\n";
                        if (scenario == 3) return "int Main() { int* q = &Demo.Widget.limit; return 0; }\n";
                        if (scenario == 4) return "int Main() { int* q = &Demo.Widget.io; return 0; }\n";
                        return "int Main() { int* q = &Demo.Widget.shared; return 0; }\n";
                    });
                    const bool addressCompiled = addressConfigured && addressProbe.compileProjectOopMir(path, addressError);
                    ok &= expect(addressConfigured && (scenario == 2 || scenario == 5 ? addressCompiled : !addressCompiled && addressProbe.diagnostics().hasErrors()),
                                 "qualified C++ field addresses are deferred while ordinary field addresses remain available");
                }
                nativesharp::frontend::ProjectCompilation methodCall;
                std::string methodError;
                const bool callConfigured = configure(methodCall, methodError) &&
                    methodCall.enableExperimentalCppInstanceMethodLoweringForTests(methodError) &&
                    methodCall.setExperimentalCppStaticMethodProvider(provideExperimentalCppStaticMethods, methodError) &&
                    methodCall.setExperimentalCppStaticFieldProvider(provideExperimentalCppStaticFields, methodError) &&
                    methodCall.setExperimentalCppInstanceFieldProvider(provideExperimentalCppInstanceFields, methodError) &&
                    methodCall.enableExperimentalCppInstanceFieldLoweringForTests(methodError) &&
                    methodCall.setCppNamespaceProvider([](const auto&, auto, auto& out, auto&) {
                        out.emplace_back(0, "Demo"); return true;
                    }, methodError) &&
                    methodCall.setCppFunctionProvider([&](const auto& graph, auto sessions, auto, auto demands,
                        auto& projected, auto& deferred, auto& error) {
                        const auto* lookup = methodCall.nameLookup();
                        if (!lookup) { error = "name lookup is unavailable"; return false; }
                        return provideExperimentalCppFunctionsWithRecords(graph, sessions, *lookup, demands,
                                                                           projected, deferred, error);
                    }, methodError);
                methodCall.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                    if (source.filename() == "Main.ns")
                        return "int Main() { var p = Demo.create(); var q = Demo.createThing(); int m = p->mutate(7); p->value = 19; p->changing = 22; Demo.Widget.shared = 23; Demo.Widget.io = 4; return m + p->ping(5) + (int)p->factor(1.5) + p->mixed(3, 2.5, 9, 4.5) + q->compute(8) + p->value + p->immutable + p->changing + Demo.Widget.scale(3) + Demo.Widget.scale(2.5) + Demo.Widget.shared + Demo.Widget.limit + Demo.Widget.io; }\n";
                    return std::nullopt;
                });
                const bool callReady = callConfigured && methodCall.compileProjectOopMir(path, methodError);
                if (!callReady)
                {
                    std::cerr << "experimental instance call: " << methodError << '\n';
                    for (const auto& diagnostic : methodCall.diagnostics().diagnostics())
                        std::cerr << diagnostic.message() << '\n';
                }
                ok &= expect(callReady, "experimental instance method reaches MIR for a pointer receiver");
                if (callReady)
                {
                    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                    auto module = lowerer.lower(*methodCall.nameLookup(), *methodCall.oopMirModel());
                    const bool valid = module && module->verify(methodError);
                    const auto ir = module ? module->text() : std::string{};
                    if (!valid) std::cerr << "experimental instance lowering: " << lowerer.error() << ' ' << methodError << '\n';
                    const auto symbol = ir.find("?ping@Widget@Demo@@QEBAHH@Z");
                    const auto second = symbol == std::string::npos ? symbol : ir.find("?ping@Widget@Demo@@QEBAHH@Z", symbol + 1);
                    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(methodError);
                    const bool objectEmitted = valid && target && target->emit(*module,
                        std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/cpp-instance-call.obj",
                        nativesharp::llvm_backend::LlvmCodeGenFileType::Object, methodError);
                    ok &= expect(objectEmitted && second != std::string::npos &&
                                 ir.find("load volatile i32") != std::string::npos &&
                                 ir.find("store volatile i32") != std::string::npos &&
                                 ir.find("?shared@Widget@Demo@@2HA\" = external global i32") != std::string::npos,
                                 "experimental instance call and declaration retain the Clang symbol in LLVM IR");
                }
                nativesharp::frontend::ProjectCompilation dllFieldCall;
                dllFieldCall.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                    if (source.filename() == "Main.ns")
                        return "int Main() { Plugin.Imported.value = 28; Plugin.shared = 14; var p = Plugin.make(); return Plugin.Imported.value + Plugin.shared + Plugin.increment(39) - 54 + Plugin.Imported.scale(3) + p->add(3) - 11; }\n";
                    return std::nullopt;
                });
                CppProjectSessions dllGroup;
                dllGroup.projectFile = std::filesystem::absolute(path);
                dllGroup.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
                    "namespace Plugin { __declspec(dllimport) extern int shared; __declspec(dllimport) int increment(int) noexcept; class Imported { public: __declspec(dllimport) static int value; __declspec(dllimport) static int scale(int); __declspec(dllimport) int add(int) const; }; __declspec(dllimport) Imported* make() noexcept; }\n")));
                std::vector<CppProjectSessions> dllGroups;
                dllGroups.push_back(std::move(dllGroup));
                std::string dllError;
                const bool dllConfigured = dllFieldCall.setCppProjectSessions(std::move(dllGroups), dllError) &&
                    dllFieldCall.setExperimentalCppTypeProvider(provideExperimentalCppRecordCandidates, dllError) &&
                    dllFieldCall.setExperimentalCppTypeSelector(selectExperimentalCppRecordTypesForCompilation, dllError) &&
                    dllFieldCall.setExperimentalCppStaticFieldProvider(provideExperimentalCppStaticFields, dllError) &&
                    dllFieldCall.setExperimentalCppStaticMethodProvider(provideExperimentalCppStaticMethods, dllError) &&
                    dllFieldCall.setExperimentalCppInstanceMethodProvider(provideExperimentalCppInstanceMethods, dllError) &&
                    dllFieldCall.enableExperimentalCppInstanceMethodLoweringForTests(dllError) &&
                    dllFieldCall.setCppGlobalProvider(provideCppSemanticGlobals, dllError) &&
                    dllFieldCall.setCppFunctionProvider([&](const auto& graph, auto sessions, auto, auto demands, auto& projected, auto& deferred, auto& why) {
                        const auto* lookup = dllFieldCall.nameLookup();
                        return lookup && provideExperimentalCppFunctionsWithRecords(graph, sessions, *lookup, demands, projected, deferred, why);
                    }, dllError) &&
                    dllFieldCall.enableExperimentalCppInstanceFieldLoweringForTests(dllError) &&
                    dllFieldCall.setCppNamespaceProvider([](const auto&, auto, auto& out, auto&) {
                        out.emplace_back(0, "Plugin"); return true;
                    }, dllError);
                const bool dllReady = dllConfigured && dllFieldCall.compileProjectOopMir(path, dllError);
                if (!dllReady)
                {
                    std::cerr << "DLL static field compilation: " << dllError << '\n';
                    for (const auto& diagnostic : dllFieldCall.diagnostics().diagnostics()) std::cerr << diagnostic.message() << '\n';
                }
                if (dllReady)
                {
                    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                    auto module = lowerer.lower(*dllFieldCall.nameLookup(), *dllFieldCall.oopMirModel());
                    const bool valid = module && module->verify(dllError);
                    const auto ir = module ? module->text() : std::string{};
                    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(dllError);
                    const bool emittedDllObject = valid && target && target->emit(*module,
                        std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/cpp-dll-field-call.obj",
                        nativesharp::llvm_backend::LlvmCodeGenFileType::Object, dllError);
                    ok &= expect(emittedDllObject && ir.find("?value@Imported@Plugin@@2HA\" = external dllimport global i32") != std::string::npos &&
                                 ir.find("?shared@Plugin@@3HA\" = external dllimport global i32") != std::string::npos &&
                                 ir.find("dllimport i32 @\"?increment@Plugin@@YAHH@Z\"") != std::string::npos &&
                                 ir.find("dllimport i32 @\"?scale@Imported@Plugin@@SAHH@Z\"") != std::string::npos &&
                                 ir.find("dllimport i32 @\"?add@Imported@Plugin@@QEBAHH@Z\"") != std::string::npos,
                                 "DLL function, methods, static field and global retain their symbols and import storage");
                }
                else ok &= expect(false, "DLL static field reaches isolated MIR generation");
            }
            for (int allocationCase : {0, 1, 2, 3})
            {
                nativesharp::frontend::ProjectCompilation allocation;
                allocation.setSourceBufferProvider([allocationCase](const std::filesystem::path& source)
                    -> std::optional<std::string> {
                    if (source.filename() != "Main.ns") return std::nullopt;
                    switch (allocationCase)
                    {
                    case 0: return "int Main() { A.B.C* p = stackalloc A.B.C[1]; return 0; }\n";
                    case 1: return "int Main() { A.B.C* p = new A.B.C; return 0; }\n";
                    case 2: return "int Main() { A.B.C** p = stackalloc A.B.C*[1]; return 0; }\n";
                    default: return "int Main() { int* p = stackalloc int[1]; return 0; }\n";
                    }
                });
                CppProjectSessions group;
                group.projectFile = std::filesystem::absolute(path);
                group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
                    "namespace A { namespace B { struct C { C(); ~C(); int value; }; } }\n")));
                std::vector<CppProjectSessions> groups;
                groups.push_back(std::move(group));
                std::string allocationError;
                const bool configured = allocation.setCppProjectSessions(std::move(groups), allocationError) &&
                    allocation.setExperimentalCppTypeProvider(provideExperimentalCppRecordCandidates, allocationError) &&
                    allocation.setExperimentalCppTypeSelector(selectExperimentalCppRecordTypesForCompilation, allocationError) &&
                    allocation.setCppNamespaceProvider([](const auto& graph, auto sessions, auto& output, auto& error) {
                        auto inventory = collectCppNamespaces(graph, sessions);
                        if (const auto* failure = std::get_if<CppNamespaceInventoryFailure>(&inventory))
                        { error = failure->message; return false; }
                        auto visible = visibleCppNamespaces(std::get<std::vector<CppNamespaceInventoryEntry>>(inventory));
                        if (const auto* failure = std::get_if<CppNamespaceInventoryFailure>(&visible))
                        { error = failure->message; return false; }
                        output = std::move(std::get<std::vector<std::pair<std::size_t, std::string>>>(visible));
                        return true;
                    }, allocationError);
                const bool ready = configured && allocation.compileProjectOopMir(path, allocationError);
                bool lifetimeDiagnostic = false;
                for (const auto& diagnostic : allocation.diagnostics().diagnostics())
                    lifetimeDiagnostic |= diagnostic.message().find("lifetime") != std::string::npos;
                if (allocationCase < 2 ? ready || !lifetimeDiagnostic : !ready)
                {
                    std::cerr << "imported allocation case=" << allocationCase << " ready=" << ready << " error=" << allocationError << '\n';
                    for (const auto& diagnostic : allocation.diagnostics().diagnostics())
                        std::cerr << diagnostic.message() << '\n';
                }
                ok &= expect(configured && (allocationCase < 2 ? !ready && lifetimeDiagnostic : ready),
                             "raw allocation rejects imported C++ objects while preserving pointer and native storage");
            }
            {
                nativesharp::frontend::ProjectCompilation byValue;
                byValue.setSourceBufferProvider([](const std::filesystem::path& source) -> std::optional<std::string> {
                    if (source.filename() == "Main.ns")
                        return "struct Wrapper { A.B.C value; } int Main() { return 0; }\n";
                    return std::nullopt;
                });
                std::string valueError;
                const bool configured = byValue.setExperimentalCppImportedTypes({{*namespaceIdentity,
                    nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                    nativesharp::frontend::NamedTypeAccessibility::Public, 0,
                    namespaceC->location ? namespaceC->location->file.generic_string() : ""}}, valueError);
                const bool valueReady = configured && byValue.compileProjectObjectLayouts(path, valueError);
                bool missingLayout = false;
                for (const auto& diagnostic : byValue.diagnostics().diagnostics())
                    missingLayout |= diagnostic.message().find("has no imported object layout yet") != std::string::npos;
                ok &= expect(configured && !valueReady && missingLayout,
                             "a C++ record field by value is rejected until its actual layout is imported");
            }
            ok &= expect(ordinal && lookup.cppImportedTypeIndex(*ordinal) == std::optional<std::size_t>(0) &&
                         !lookup.cppImportedTypeIndex(lookup.namedTypes().size()),
                         "successful lookup retains the imported type input index by ordinal");
            CppRecordGroups selectionGroups;
            selectionGroups.groups.push_back({0, *namespaceIdentity, {}, CppRecordGroupStatus::Conflict});
            const std::vector<std::size_t> mapping{0};
            const auto selected = selectCppRecordBindings(lookup, selectionGroups, mapping);
            const auto* selection = std::get_if<CppRecordBindingSelection>(&selected);
            ok &= expect(selection && selection->referencedGroups == mapping &&
                         selection->expressionCandidateGroups.empty() &&
                         selectionGroups.groups[selection->referencedGroups[0]].status == CppRecordGroupStatus::Conflict,
                         "resolved type selects its explicit record group without hiding conflicts");
            const std::vector<std::size_t> invalidMapping{1};
            const std::vector<std::size_t> missingMapping;
            ok &= expect(std::holds_alternative<CppRecordBindingFailure>(
                             selectCppRecordBindings(lookup, selectionGroups, invalidMapping)) &&
                         std::holds_alternative<CppRecordBindingFailure>(
                             selectCppRecordBindings(lookup, selectionGroups, missingMapping)),
                         "record selection rejects invalid and missing installed mappings");
            auto wrongIdentityGroups = selectionGroups;
            if (nestedIdentity) wrongIdentityGroups.groups[0].identity = *nestedIdentity;
            auto wrongProjectGroups = selectionGroups;
            wrongProjectGroups.groups[0].projectOrdinal = 1;
            ok &= expect(nestedIdentity && std::holds_alternative<CppRecordBindingFailure>(
                             selectCppRecordBindings(lookup, wrongIdentityGroups, mapping)) &&
                         std::holds_alternative<CppRecordBindingFailure>(
                             selectCppRecordBindings(lookup, wrongProjectGroups, mapping)),
                         "record selection rejects mismatched identity and owning project");
            nativesharp::frontend::ProjectNameLookup wrongOwner;
            const bool ownerInstalled = wrongOwner.setCppImportedTypes({{*namespaceIdentity,
                nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                nativesharp::frontend::NamedTypeAccessibility::Public, 99,
                namespaceC->location ? namespaceC->location->file.generic_string() : ""}});
            ok &= expect(ownerInstalled && !wrongOwner.build(
                             *compilation.projectGraph(), compilation.units(), compilation.diagnostics()),
                         "imported record cannot be assigned to a nonexistent project");
            ok &= expect(!wrongOwner.cppImportedTypeIndex(0), "failed lookup does not publish imported provenance");
            {
                const nativesharp::compilation::TypeIdentity producerIdentity("ProducerModule",
                    namespaceIdentity->namespaceSegments(), namespaceIdentity->containingTypes(), namespaceIdentity->name());
                nativesharp::frontend::CppImportedTypeDeclaration live{*namespaceIdentity,
                    nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                    nativesharp::frontend::NamedTypeAccessibility::Public, 0, "live.h"};
                auto producer = live;
                producer.identity = producerIdentity;
                producer.projectOrdinal = 1;
                producer.sourcePath = "producer.nsmeta";
                for (auto* item : {&live, &producer})
                {
                    item->origin = nativesharp::frontend::CppImportedTypeOrigin::CStruct;
                    item->targetTriple = "x86_64-pc-windows-msvc";
                    item->clangUsr = "record-usr";
                    item->objectLayout = nativesharp::frontend::CppImportedObjectLayout{8, 4};
                }
                live.reconciledIdentityKey = producer.identity.key();
                nativesharp::frontend::ProjectNameLookup reconciledLookup;
                auto conflicting = producer;
                conflicting.clangUsr = "different-usr";
                ok &= expect(!reconciledLookup.setCppImportedTypes({live, conflicting}) &&
                             !reconciledLookup.setCppImportedTypes({live}) &&
                             reconciledLookup.setCppImportedTypes({live, producer}),
                             "C reconciled identities require a matching unique target without poisoning lookup");
                auto chained = producer;
                chained.reconciledIdentityKey = live.identity.key();
                nativesharp::frontend::ProjectNameLookup chainedLookup;
                ok &= expect(!chainedLookup.setCppImportedTypes({live, chained}),
                             "C reconciled identities reject cyclic target chains");
            }
            nativesharp::frontend::ProjectCompilation duplicateCompilation;
            duplicateCompilation.setSourceBufferProvider([](const std::filesystem::path& source)
                -> std::optional<std::string> {
                if (source.filename() == "Main.ns") return "namespace A.B { struct C {} } int Main() { return 0; }\n";
                return std::nullopt;
            });
            std::string duplicateError;
            const bool duplicateParsed = duplicateCompilation.compileProjectSyntax(path, duplicateError);
            nativesharp::frontend::ProjectNameLookup duplicateLookup;
            const bool duplicateConfigured = duplicateParsed && duplicateLookup.setImportedNamespaces({{0, "A.B"}}) &&
                duplicateLookup.setCppImportedTypes({{*namespaceIdentity,
                    nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                    nativesharp::frontend::NamedTypeAccessibility::Public, 0,
                    namespaceC->location ? namespaceC->location->file.generic_string() : ""}});
            const bool duplicateReady = duplicateConfigured && duplicateLookup.build(
                *duplicateCompilation.projectGraph(), duplicateCompilation.units(), duplicateCompilation.diagnostics());
            ok &= expect(duplicateConfigured && !duplicateReady && duplicateCompilation.diagnostics().hasErrors(),
                         "imported record colliding with a NativeSharp declaration is diagnosed");
            const auto duplicateOrdinal = duplicateLookup.typeOrdinalForIdentity(namespaceIdentity->key());
            ok &= expect(!duplicateOrdinal || !duplicateLookup.cppImportedTypeIndex(*duplicateOrdinal),
                         "a conflicting lookup does not publish imported provenance");
            {
                nativesharp::frontend::ProjectCompilation hooked;
                hooked.setSourceBufferProvider([](const std::filesystem::path& source)
                    -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return "int Main() { return 0; }\n";
                    return std::nullopt;
                });
                CppProjectSessions sessionGroup;
                sessionGroup.projectFile = std::filesystem::absolute(path);
                sessionGroup.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                    CppLanguageMode::Cpp, "namespace A { namespace B { struct C {}; } }\n")));
                std::vector<CppProjectSessions> sessions;
                sessions.push_back(std::move(sessionGroup));
                std::optional<CppRecordObservationRunResult> observed;
                int calls = 0;
                std::string hookError;
                const bool hookReady = hooked.setCppProjectSessions(std::move(sessions), hookError) &&
                    hooked.setCppRecordObserver([&](const auto& source) { ++calls; observed = observeCppRecords(source); }, hookError) &&
                    hooked.compileProjectNames(path, hookError);
                const auto* hookResult = observed ? std::get_if<CppRecordObservation>(&*observed) : nullptr;
                std::string lateError;
                ok &= expect(hookReady && calls == 1 && hookResult && !hookResult->inventory.empty() &&
                             hooked.nameLookup() && hooked.nameLookup()->cppImportedTypeIndex(0) == std::nullopt &&
                             !hooked.diagnostics().hasErrors() && !hooked.setCppRecordObserver({}, lateError) && !lateError.empty(),
                             "optional record observer runs once and does not install placeholders into production lookup");
            }
            {
                const auto checkAliasUsr = [&](bool conflict) {
                    nativesharp::frontend::ProjectCompilation compilation;
                    compilation.setSourceBufferProvider([](const std::filesystem::path& source)
                        -> std::optional<std::string> {
                        if (source.filename() == "Main.ns")
                            return "int Use(Count value) { return value; } int Main() { return 0; }\n";
                        return std::nullopt;
                    });
                    CppProjectSessions group;
                    group.projectFile = std::filesystem::absolute(path);
                    group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::C, "typedef int Count;\n")));
                    std::vector<CppProjectSessions> groups;
                    groups.push_back(std::move(group));
                    std::string error;
                    const bool configured = compilation.setCppProjectSessions(std::move(groups), error) &&
                        compilation.setCppTypeAliasProvider([&](const auto& graph, auto sessions, auto& aliases, auto& providerError) {
                            if (!provideCppTypeAliases(graph, sessions, aliases, providerError) || aliases.empty()) return false;
                            auto first = aliases.front();
                            first.declaration.clangUsr = "test:count";
                            auto missing = first;
                            missing.declaration.clangUsr.reset();
                            auto last = first;
                            if (conflict) last.declaration.clangUsr = "test:other-count";
                            aliases = {std::move(first), std::move(missing), std::move(last)};
                            return true;
                        }, error);
                    const bool bound = configured && compilation.compileProjectNames(path, error);
                    return configured && (conflict ? !bound && compilation.diagnostics().hasErrors() :
                                                     bound && !compilation.diagnostics().hasErrors());
                };
                ok &= expect(checkAliasUsr(false), "matching alias USRs resolve across an unknown observation");
                ok &= expect(checkAliasUsr(true), "conflicting alias USRs remain ambiguous across an unknown observation");
            }
            {
                nativesharp::frontend::ProjectCompilation aliasObservation;
                aliasObservation.setSourceBufferProvider([](const std::filesystem::path& source)
                    -> std::optional<std::string> {
                    if (source.filename() == "Main.ns")
                        return "int Use(PairAlias* value) { return 0; } Color Echo(Color value) { return value; } int Main() { return 0; }\n";
                    return std::nullopt;
                });
                CppProjectSessions aliasSession;
                aliasSession.projectFile = std::filesystem::absolute(path);
                aliasSession.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                    CppLanguageMode::C, "struct Pair { int value; }; typedef struct Pair PairAlias; enum Color { Red = 1 }; typedef enum Color Color;\n")));
                std::vector<CppProjectSessions> aliasSessions;
                aliasSessions.push_back(std::move(aliasSession));
                std::string aliasError;
                const bool aliasParsed = aliasObservation.setCppProjectSessions(std::move(aliasSessions), aliasError) &&
                    aliasObservation.setCppTypeAliasProvider(provideCppTypeAliases, aliasError) &&
                    aliasObservation.compileProjectSyntax(path, aliasError);
                const auto aliasObserved = aliasParsed ? observeCppRecords(aliasObservation) :
                    CppRecordObservationRunResult{CppRecordObservationFailure{aliasError}};
                const auto* aliasResult = std::get_if<CppRecordObservation>(&aliasObserved);
                bool aliasReferenced = false;
                if (aliasResult)
                    for (const auto index : aliasResult->selection.referencedGroups)
                        aliasReferenced |= index < aliasResult->groups.groups.size() &&
                            aliasResult->groups.groups[index].identity.name() == "Pair";
                bool enumReferenced = false;
                if (aliasResult)
                    for (const auto& type : aliasResult->referencedEnumTypes)
                        enumReferenced |= type.identity.name() == "Color" && type.enumBase.has_value();
                ok &= expect(aliasResult && aliasReferenced && enumReferenced,
                             "record observation selects C record and enum typedefs together");
                {
                    nativesharp::frontend::ProjectCompilation fieldDependency;
                    fieldDependency.setSourceBufferProvider([](const std::filesystem::path& source)
                        -> std::optional<std::string> {
                        if (source.filename() == "Main.ns")
                            return "int Use(Box* value) { return 0; } int Main() { return 0; }\n";
                        return std::nullopt;
                    });
                    CppProjectSessions group;
                    group.projectFile = std::filesystem::absolute(path);
                    group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::C, "enum Shade { Dark = 1 }; struct Box { enum Shade color; }; "
                        "struct Box make_box(void); void take_box(struct Box value);\n")));
                    std::vector<CppProjectSessions> groups;
                    groups.push_back(std::move(group));
                    std::string dependencyError;
                    const bool ready = fieldDependency.setCppProjectSessions(std::move(groups), dependencyError) &&
                        fieldDependency.setExperimentalCppTypeProvider(provideCppInteropTypes, dependencyError) &&
                        fieldDependency.setExperimentalCppTypeSelector(selectExperimentalCppRecordTypesForCompilation, dependencyError) &&
                        fieldDependency.compileProjectNames(path, dependencyError);
                    bool selectedShade = false;
                    if (ready && fieldDependency.nameLookup())
                        for (const auto& type : fieldDependency.nameLookup()->namedTypes())
                            selectedShade |= type.identity.name() == "Shade" &&
                                type.kind == nativesharp::frontend::NamedTypeDeclarationKind::Enum;
                    if (!ready) std::cerr << "C record enum dependency: " << dependencyError << '\n';
                    ok &= expect(ready && selectedShade, "record field selects its enum dependency without a direct type use");
                    if (ready && selectedShade && fieldDependency.nameLookup() && fieldDependency.projectGraph())
                    {
                        const auto& lookup = *fieldDependency.nameLookup();
                        auto recordInventory = collectCppSemanticRecords(*fieldDependency.projectGraph(), fieldDependency.cppProjectSessions());
                        auto functionInventory = collectCppSemanticFunctions(*fieldDependency.projectGraph(), fieldDependency.cppProjectSessions());
                        const auto* recordEntries = std::get_if<std::vector<CppSemanticRecordEntry>>(&recordInventory);
                        const auto* functionEntries = std::get_if<std::vector<CppSemanticInventoryEntry>>(&functionInventory);
                        bool returnedRecord = false, parameterRecord = false, rejectedLayout = false, rejectedCpp = false;
                        if (recordEntries && functionEntries)
                        {
                            const auto recordGroups = groupCppSemanticRecords(*recordEntries);
                            for (const auto& entry : *functionEntries)
                            {
                                if (entry.declaration.name != "make_box" && entry.declaration.name != "take_box") continue;
                                const auto& moduleName = fieldDependency.projectGraph()->projects()[entry.projectOrdinal].moduleName();
                                auto projection = projectCppSemanticFunctionWithRecords(entry.declaration,
                                    entry.projectOrdinal, moduleName, recordGroups, &lookup);
                                const auto* function = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projection);
                                if (entry.declaration.name == "make_box")
                                {
                                    returnedRecord = function && function->declaredTypeKey.starts_with("type:");
                                    auto wrong = entry.declaration;
                                    wrong.returnType.sizeBits = 128;
                                    rejectedLayout = std::holds_alternative<CppSemanticFunctionFailure>(
                                        projectCppSemanticFunctionWithRecords(wrong, entry.projectOrdinal, moduleName, recordGroups, &lookup));
                                    wrong = entry.declaration;
                                    wrong.languageLinkage = CppLanguageLinkage::Cpp;
                                    rejectedCpp = std::holds_alternative<CppSemanticFunctionFailure>(
                                        projectCppSemanticFunctionWithRecords(wrong, entry.projectOrdinal, moduleName, recordGroups, &lookup));
                                }
                                else parameterRecord = function && function->parameters.size() == 1 &&
                                    function->parameters[0].typeKey.starts_with("type:");
                            }
                        }
                        ok &= expect(returnedRecord && parameterRecord && rejectedLayout && rejectedCpp,
                                     "C record value projection checks return, parameter, layout and language linkage");
                        LibraryMetadata api;
                        LibraryMetadataNamespaceFunction function;
                        for (const auto& type : lookup.namedTypes())
                            if (type.identity.name() == "Box")
                                function.declaredTypeKey = "type:" + type.identity.key() + "*";
                        api.namespaceFunctions.push_back(std::move(function));
                        std::vector<std::uint8_t> bytes;
                        std::vector<CppCRecordSnapshot> records;
                        std::vector<CppCEnumSnapshot> enums;
                        const bool supplied = provideCppCRecordMetadataCatalog(fieldDependency, api, bytes, dependencyError) &&
                            metadata_detail::decodeVersionedCTypeCatalog(bytes, records, enums, dependencyError);
                        ok &= expect(supplied && records.size() == 1 && enums.size() == 1 &&
                                     enums[0].declaration.name == "Shade",
                                     "C metadata provider includes enum dependency of record field");
                        if (supplied && records.size() == 1 && enums.size() == 1)
                        {
                            auto closureMetadata = api;
                            closureMetadata.formatMinor = api.formatMinor;
                            closureMetadata.moduleName = records[0].moduleName;
                            closureMetadata.targetTriple = records[0].targetTriple;
                            closureMetadata.cppCRecordCatalog = bytes;
                            const auto checkClosure = [&](const LibraryMetadata& metadata) {
                                std::unordered_set<std::string> declared, specialized, external;
                                return metadata_detail::verifyGenericAndExternalTypeCatalogues(
                                           metadata, declared, specialized, external, dependencyError) &&
                                    metadata_detail::verifyAllMetadataTypeReferences(
                                           metadata, declared, specialized, external, dependencyError);
                            };
                            ok &= expect(checkClosure(closureMetadata),
                                         "metadata closure accepts a local C record from its validated catalogue");
                            auto missingCatalog = closureMetadata;
                            missingCatalog.cppCRecordCatalog.clear();
                            ok &= expect(!checkClosure(missingCatalog),
                                         "metadata closure still rejects the missing local C record");
                            auto duplicateType = closureMetadata;
                            LibraryMetadataType duplicate;
                            duplicate.identityKey = records[0].identityKey;
                            duplicateType.types.push_back(std::move(duplicate));
                            ok &= expect(!checkClosure(duplicateType),
                                         "metadata closure rejects a C and NativeSharp type identity collision");
                            auto wrongTarget = closureMetadata;
                            wrongTarget.targetTriple = "different-target";
                            ok &= expect(!checkClosure(wrongTarget),
                                         "metadata closure rejects a catalogue from another target");
                            auto externalCatalog = closureMetadata;
                            externalCatalog.moduleName = "ExternalConsumer";
                            ok &= expect(!checkClosure(externalCatalog),
                                         "metadata closure rejects C types owned by another module without dependencies");
                            externalCatalog.externalTypeKeys = {
                                "type:" + records[0].identityKey, "type:" + enums[0].identityKey};
                            ok &= expect(checkClosure(externalCatalog),
                                         "metadata closure accepts C types of another module only when dependencies are recorded");
                            const auto provider = std::make_shared<LibraryMetadata>(closureMetadata);
                            const auto user = std::make_shared<LibraryMetadata>(externalCatalog);
                            const std::vector<nativesharp::driver::LoadedPrecompiledMetadata> loaded{
                                {{}, {}, provider, {}}, {{}, {}, user, {}}};
                            const std::unordered_map<std::string, std::filesystem::path> noSources;
                            ok &= expect(nativesharp::driver::validatePrecompiledExternalTypeClosure(
                                             loaded, noSources, dependencyError),
                                         "external closure recognizes record and enum C in the owning provider");
                            const std::vector<nativesharp::driver::LoadedPrecompiledMetadata> missingProvider{
                                {{}, {}, user, {}}};
                            ok &= expect(!nativesharp::driver::validatePrecompiledExternalTypeClosure(
                                             missingProvider, noSources, dependencyError),
                                         "external closure rejects C types without their provider");
                            auto invalidProvider = std::make_shared<LibraryMetadata>(closureMetadata);
                            invalidProvider->targetTriple = "different-target";
                            const std::vector<nativesharp::driver::LoadedPrecompiledMetadata> invalidLoaded{
                                {{}, {}, invalidProvider, {}}, {{}, {}, user, {}}};
                            ok &= expect(!nativesharp::driver::validatePrecompiledExternalTypeClosure(
                                             invalidLoaded, noSources, dependencyError),
                                         "external closure rejects a provider with an incompatible C catalogue");
                            nativesharp::frontend::PrecompiledSyntaxModule module;
                            module.metadataPath = std::filesystem::path(argv[2]) / "shade.nsmeta";
                            module.cppCTypeCatalog = bytes;
                            const std::vector<nativesharp::frontend::PrecompiledSyntaxModule> modules{module};
                            std::vector<nativesharp::frontend::CppImportedInstanceField> fields;
                            const bool restored = provideCppFieldsWithPrecompiledCatalogs(*fieldDependency.projectGraph(),
                                {}, modules, lookup, fields, dependencyError);
                            ok &= expect(restored && fields.size() == 1 && fields[0].name == "color",
                                         "combined field provider restores a precompiled C field");
                            const auto priorFields = fields.size();
                            ok &= expect(!provideCppFieldsWithPrecompiledCatalogs(*fieldDependency.projectGraph(),
                                             fieldDependency.cppProjectSessions(), modules, lookup, fields, dependencyError) &&
                                         fields.size() == priorFields,
                                         "combined field provider rejects duplicate live fields transactionally");
                            nativesharp::frontend::CppFunctionDemand demand;
                            demand.lookupName = "Dark";
                            demand.lookupNamespaces.push_back("");
                            for (const auto& type : lookup.namedTypes())
                                if (type.identity.name() == "Shade") demand.visibleProjects.push_back(type.projectOrdinal);
                            std::vector<nativesharp::frontend::NamespaceStateSymbol> values;
                            const std::vector<nativesharp::frontend::CppFunctionDemand> demands{demand};
                            const bool found = provideCppValuesWithPrecompiledCatalogs(*fieldDependency.projectGraph(),
                                {}, modules, lookup, demands, values, dependencyError);
                            ok &= expect(found && values.size() == 1 && values[0].name == "Dark" && values[0].integerValue == 1,
                                         "combined value provider returns demanded visible precompiled enumerator");
                            demand.visibleProjects.clear();
                            const std::vector<nativesharp::frontend::CppFunctionDemand> hidden{demand};
                            ok &= expect(provideCppValuesWithPrecompiledCatalogs(*fieldDependency.projectGraph(),
                                             {}, modules, lookup, hidden, values, dependencyError) && values.empty(),
                                         "combined value provider hides enumerators outside visible projects");
                            demand = demands[0];
                            demand.lookupNamespaces = {"other"};
                            const std::vector<nativesharp::frontend::CppFunctionDemand> wrongNamespace{demand};
                            ok &= expect(provideCppValuesWithPrecompiledCatalogs(*fieldDependency.projectGraph(),
                                             {}, modules, lookup, wrongNamespace, values, dependencyError) && values.empty(),
                                         "combined value provider respects demanded namespaces");
                            const bool collision = provideCppValuesWithPrecompiledCatalogs(*fieldDependency.projectGraph(),
                                fieldDependency.cppProjectSessions(), modules, lookup, demands, values, dependencyError);
                            ok &= expect(collision && values.size() == 1 && !values[0].cppImportError.empty(),
                                         "combined value provider defers live and precompiled value collisions");
                            const auto consumerDirectory = std::filesystem::path(argv[2]) / "cpp-fixtures" / "precompiled-c-consumer";
                            std::filesystem::create_directories(consumerDirectory);
                            const auto consumerProject = std::filesystem::absolute(consumerDirectory / "Consumer.nsproj");
                            { std::ofstream file(consumerProject); file << "<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup></Project>\n"; }
                            { std::ofstream file(consumerDirectory / "Main.ns"); file << "int Main() { return 0; }\n"; }
                            module.ownerProjectFile = consumerProject;
                            module.moduleName = records[0].moduleName;
                            module.sourceText = "namespace Published { public struct Value {} }\n";
                            { std::ofstream file(module.metadataPath); file << "synthetic metadata fixture"; }
                            nativesharp::frontend::ProjectCompilation consumer;
                            consumer.setSourceBufferProvider([](const std::filesystem::path& source)
                                -> std::optional<std::string> {
                                if (source.filename() == "Main.ns")
                                    return "int Use(Box* value) { return value.color; } int Main() { return Dark; }\n";
                                return std::nullopt;
                            });
                            const bool configured = consumer.setPrecompiledModules({module}, dependencyError) &&
                                consumer.setExperimentalCppTypeSelector(selectCppTypesWithPrecompiledCatalogs, dependencyError) &&
                                consumer.setExperimentalCppInstanceFieldProvider(
                                    [&consumer](const auto& graph, auto projects, const auto& names, auto& out, auto& why) {
                                        return provideCppFieldsWithPrecompiledCatalogs(graph, projects,
                                            consumer.precompiledModules(), names, out, why);
                                    }, dependencyError) &&
                                consumer.setCppGlobalProvider(
                                    [&consumer](const auto& graph, auto projects, auto wanted, auto& out, auto& why) {
                                        const auto* names = consumer.nameLookup();
                                        if (!names) { why = "consumer name lookup is unavailable"; return false; }
                                        return provideCppValuesWithPrecompiledCatalogs(graph, projects,
                                            consumer.precompiledModules(), *names, wanted, out, why);
                                    }, dependencyError);
                            const bool consumed = configured && consumer.compileProjectMembers(consumerProject, dependencyError);
                            bool hasNativeType = false, hasCType = false;
                            if (consumed && consumer.nameLookup())
                                for (const auto& type : consumer.nameLookup()->namedTypes())
                                {
                                    hasNativeType |= type.identity.name() == "Value";
                                    hasCType |= type.identity.name() == "Box";
                                }
                            const auto* memberModel = consumer.memberModel();
                            const auto* namespaceModel = consumer.namespaceFunctionModel();
                            bool hasEnumerator = false;
                            if (namespaceModel)
                                for (const auto& state : namespaceModel->states())
                                    hasEnumerator |= state.name == "Dark" && state.isCppEnumConstant && state.integerValue == 1;
                            if (!consumed) std::cerr << "precompiled C consumer: " << dependencyError << '\n';
                            ok &= expect(consumed && hasNativeType && hasCType && memberModel &&
                                         !memberModel->findMembers("color").empty() && hasEnumerator,
                                         "consumer installs precompiled C types, fields and demanded enumerators alongside NativeSharp");
                        }
                        for (const auto& type : lookup.namedTypes())
                        {
                            if (type.identity.name() != "Shade") continue;
                            auto inventory = collectCppSemanticEnums(*fieldDependency.projectGraph(), fieldDependency.cppProjectSessions());
                            const auto* entries = std::get_if<std::vector<CppSemanticEnumEntry>>(&inventory);
                            if (!entries) { ok &= expect(false, "enum reconstruction inventory is available"); break; }
                            const std::vector<std::string> wanted{type.identity.key()};
                            auto captured = captureCppCEnumCatalog(*entries, type.projectOrdinal, type.identity.moduleName(), wanted);
                            const auto* values = std::get_if<std::vector<CppCEnumSnapshot>>(&captured);
                            std::vector<nativesharp::frontend::NamespaceStateSymbol> constants;
                            const bool rebuilt = values && reconstructCEnumConstants(*values, type.projectOrdinal,
                                type.identity.moduleName(), "x86_64-pc-windows-msvc", lookup, constants, dependencyError);
                            ok &= expect(rebuilt && constants.size() == 1 && constants[0].name == "Dark" &&
                                         constants[0].integerValue == 1 && constants[0].isCppEnumConstant,
                                         "C enum metadata reconstructs its constant with the original value");
                            break;
                        }
                    }
                }
                {
                    nativesharp::frontend::ProjectCompilation functionDependency;
                    functionDependency.setSourceBufferProvider([](const std::filesystem::path& source)
                        -> std::optional<std::string> {
                        if (source.filename() == "Main.ns")
                            return "int Main() { current_mood(); return 0; }\n";
                        return std::nullopt;
                    });
                    CppProjectSessions group;
                    group.projectFile = std::filesystem::absolute(path);
                    group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::C, "enum Mood { Happy = 1 }; enum Mood current_mood(void);\n")));
                    std::vector<CppProjectSessions> groups;
                    groups.push_back(std::move(group));
                    std::string dependencyError;
                    const bool ready = functionDependency.setCppProjectSessions(std::move(groups), dependencyError) &&
                        functionDependency.setExperimentalCppTypeProvider(provideCppInteropTypes, dependencyError) &&
                        functionDependency.setExperimentalCppTypeSelector(selectExperimentalCppRecordTypesForCompilation, dependencyError) &&
                        functionDependency.compileProjectNames(path, dependencyError);
                    bool selectedMood = false;
                    if (ready && functionDependency.nameLookup())
                        for (const auto& type : functionDependency.nameLookup()->namedTypes())
                            selectedMood |= type.identity.name() == "Mood" &&
                                type.kind == nativesharp::frontend::NamedTypeDeclarationKind::Enum;
                    if (!ready) std::cerr << "C function enum dependency: " << dependencyError << '\n';
                    ok &= expect(ready && selectedMood,
                                 "function demand selects its enum result without an explicit type use");
                    if (ready && selectedMood && functionDependency.nameLookup())
                    {
                        LibraryMetadata api;
                        LibraryMetadataNamespaceFunction function;
                        for (const auto& type : functionDependency.nameLookup()->namedTypes())
                            if (type.identity.name() == "Mood")
                                function.declaredTypeKey = "type:" + type.identity.key();
                        api.namespaceFunctions.push_back(std::move(function));
                        std::vector<std::uint8_t> bytes;
                        std::vector<CppCRecordSnapshot> records;
                        std::vector<CppCEnumSnapshot> enums;
                        const bool supplied = provideCppCRecordMetadataCatalog(functionDependency, api, bytes, dependencyError) &&
                            metadata_detail::decodeVersionedCTypeCatalog(bytes, records, enums, dependencyError);
                        ok &= expect(supplied && records.empty() && enums.size() == 1 &&
                                     enums[0].declaration.name == "Mood",
                                     "C metadata provider includes enum used directly in a signature");
                    }
                }
                {
                    nativesharp::frontend::ProjectCompilation callbackDependency;
                    callbackDependency.setSourceBufferProvider([](const std::filesystem::path& source)
                        -> std::optional<std::string> {
                        if (source.filename() == "Main.ns")
                            return "int Use(Visitor cb) { return 0; } int Main() { return 0; }\n";
                        return std::nullopt;
                    });
                    CppProjectSessions group;
                    group.projectFile = std::filesystem::absolute(path);
                    group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::C,
                        "enum Tone { Low = 1 }; struct Node { int value; }; "
                        "typedef enum Tone (*Visitor)(struct Node*);\n")));
                    std::vector<CppProjectSessions> groups;
                    groups.push_back(std::move(group));
                    std::string dependencyError;
                    const bool ready = callbackDependency.setCppProjectSessions(std::move(groups), dependencyError) &&
                        callbackDependency.setCppTypeAliasProvider(provideCppTypeAliases, dependencyError) &&
                        callbackDependency.setExperimentalCppTypeProvider(provideCppInteropTypes, dependencyError) &&
                        callbackDependency.setExperimentalCppTypeSelector(selectExperimentalCppRecordTypesForCompilation, dependencyError) &&
                        callbackDependency.compileProjectNames(path, dependencyError);
                    bool selectedTone = false, selectedNode = false;
                    if (ready && callbackDependency.nameLookup())
                        for (const auto& type : callbackDependency.nameLookup()->namedTypes())
                        {
                            selectedTone |= type.identity.name() == "Tone" &&
                                type.kind == nativesharp::frontend::NamedTypeDeclarationKind::Enum;
                            selectedNode |= type.identity.name() == "Node" &&
                                type.kind == nativesharp::frontend::NamedTypeDeclarationKind::Struct;
                        }
                    if (!ready) std::cerr << "C callback typedef dependencies: " << dependencyError << '\n';
                    ok &= expect(ready && selectedTone && selectedNode,
                                 "callback typedef selects enum and record used only inside its signature");
                }
                {
                    nativesharp::frontend::ProjectCompilation arrayDependency;
                    arrayDependency.setSourceBufferProvider([](const std::filesystem::path& source)
                        -> std::optional<std::string> {
                        if (source.filename() == "Main.ns")
                            return "int Use(ShadeArray shades, NodeArray nodes) { return 0; } int Main() { return 0; }\n";
                        return std::nullopt;
                    });
                    CppProjectSessions group;
                    group.projectFile = std::filesystem::absolute(path);
                    group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::C,
                        "enum Shade { Light = 1 }; struct Node { int value; }; "
                        "typedef enum Shade ShadeArray[2]; typedef struct Node NodeArray[2];\n")));
                    std::vector<CppProjectSessions> groups;
                    groups.push_back(std::move(group));
                    std::string dependencyError;
                    const bool ready = arrayDependency.setCppProjectSessions(std::move(groups), dependencyError) &&
                        arrayDependency.setCppTypeAliasProvider(provideCppTypeAliases, dependencyError) &&
                        arrayDependency.setExperimentalCppTypeProvider(provideCppInteropTypes, dependencyError) &&
                        arrayDependency.setExperimentalCppTypeSelector(selectExperimentalCppRecordTypesForCompilation, dependencyError) &&
                        arrayDependency.compileProjectNames(path, dependencyError);
                    bool selectedShade = false, selectedNode = false;
                    if (ready && arrayDependency.nameLookup())
                        for (const auto& type : arrayDependency.nameLookup()->namedTypes())
                        {
                            selectedShade |= type.identity.name() == "Shade" &&
                                type.kind == nativesharp::frontend::NamedTypeDeclarationKind::Enum;
                            selectedNode |= type.identity.name() == "Node" &&
                                type.kind == nativesharp::frontend::NamedTypeDeclarationKind::Struct;
                        }
                    if (!ready) std::cerr << "C array typedef dependencies: " << dependencyError << '\n';
                    ok &= expect(ready && selectedShade && selectedNode,
                                 "array typedef selects its enum and record element dependencies");
                }
                for (bool collide : {false, true})
                {
                    nativesharp::frontend::ProjectCompilation recordDependencies;
                    recordDependencies.setSourceBufferProvider([collide](const std::filesystem::path& source)
                        -> std::optional<std::string> {
                        if (source.filename() == "Main.ns")
                            return collide ? "struct Leaf {} int Use(Branch* value) { return 0; } int Main() { return 0; }\n"
                                           : "int Use(Branch* value) { return 0; } int Main() { return 0; }\n";
                        return std::nullopt;
                    });
                    CppProjectSessions group;
                    group.projectFile = std::filesystem::absolute(path);
                    group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::C,
                        "struct Leaf { int value; }; struct Branch { struct Leaf items[2]; struct Branch* next; };\n")));
                    std::vector<CppProjectSessions> groups;
                    groups.push_back(std::move(group));
                    std::string dependencyError;
                    const bool ready = recordDependencies.setCppProjectSessions(std::move(groups), dependencyError) &&
                        recordDependencies.setExperimentalCppTypeProvider(provideCppInteropTypes, dependencyError) &&
                        recordDependencies.setExperimentalCppTypeSelector(selectExperimentalCppRecordTypesForCompilation, dependencyError) &&
                        recordDependencies.compileProjectNames(path, dependencyError);
                    bool branch = false, leaf = false;
                    if (ready && recordDependencies.nameLookup())
                        for (const auto& type : recordDependencies.nameLookup()->namedTypes())
                        {
                            branch |= type.identity.name() == "Branch" && type.kind == nativesharp::frontend::NamedTypeDeclarationKind::Struct;
                            leaf |= type.identity.name() == "Leaf" && type.kind == nativesharp::frontend::NamedTypeDeclarationKind::Struct;
                        }
                    if (!ready && !collide) std::cerr << "C nested record dependencies: " << dependencyError << '\n';
                    ok &= expect(collide ? !ready && dependencyError.find("C field record dependency conflicts") != std::string::npos
                                         : ready && branch && leaf,
                                 "C record dependencies handle cycles and reject a conflicting native type");
                    if (ready && !collide && recordDependencies.nameLookup())
                    {
                        std::vector<nativesharp::frontend::CppImportedTypeDeclaration> selectedTypes;
                        const bool selectedOk = selectCppTypesWithPrecompiledCatalogs(recordDependencies, selectedTypes, dependencyError);
                        const bool hasBranch = std::any_of(selectedTypes.begin(), selectedTypes.end(),
                            [](const auto& type) { return type.identity.name() == "Branch"; });
                        const bool hasLeaf = std::any_of(selectedTypes.begin(), selectedTypes.end(),
                            [](const auto& type) { return type.identity.name() == "Leaf"; });
                        ok &= expect(selectedOk && hasBranch && hasLeaf,
                                     "combined C type selector preserves live-session dependencies");
                        LibraryMetadata api;
                        LibraryMetadataNamespaceFunction function;
                        for (const auto& type : recordDependencies.nameLookup()->namedTypes())
                            if (type.identity.name() == "Branch")
                                function.declaredTypeKey = "type:" + type.identity.key() + "*";
                        api.namespaceFunctions.push_back(std::move(function));
                        std::vector<std::uint8_t> catalogBytes;
                        std::vector<CppCRecordSnapshot> catalogRecords;
                        std::vector<CppCEnumSnapshot> catalogEnums;
                        const bool supplied = provideCppCRecordMetadataCatalog(recordDependencies, api, catalogBytes, dependencyError) &&
                            metadata_detail::decodeVersionedCTypeCatalog(catalogBytes, catalogRecords, catalogEnums, dependencyError);
                        ok &= expect(supplied && catalogRecords.size() == 2 && catalogEnums.empty(),
                                     "C metadata provider selects public record and transitive field dependency");
                        if (supplied && !catalogRecords.empty())
                        {
                            const auto ordinal = recordDependencies.nameLookup()->typeOrdinalForIdentity(catalogRecords.front().identityKey);
                            const auto* imported = ordinal ? recordDependencies.nameLookup()->cppImportedType(*ordinal) : nullptr;
                            std::vector<nativesharp::frontend::CppImportedInstanceField> fields;
                            const bool rebuilt = imported && reconstructCImportedFields(catalogRecords, imported->projectOrdinal,
                                catalogRecords.front().moduleName, "x86_64-pc-windows-msvc", *recordDependencies.nameLookup(), fields, dependencyError);
                            ok &= expect(rebuilt && fields.size() == 3,
                                         "C metadata reconstructs scalar, array and pointer fields");
                            nativesharp::frontend::PrecompiledSyntaxModule module;
                            module.metadataPath = std::filesystem::path(argv[2]) / "first.nsmeta";
                            module.cppCTypeCatalog = catalogBytes;
                            const std::vector<nativesharp::frontend::PrecompiledSyntaxModule> duplicates{module, module};
                            std::vector<nativesharp::frontend::CppImportedTypeDeclaration> restored;
                            const bool restoredOk = metadata_detail::reconstructPrecompiledCImportedTypes(
                                duplicates, *recordDependencies.projectGraph(), "x86_64-pc-windows-msvc", restored, dependencyError);
                            ok &= expect(restoredOk && restored.size() == 2,
                                         "precompiled C catalogues deduplicate identical declarations");
                            std::vector<nativesharp::frontend::CppImportedInstanceField> restoredFields;
                            std::vector<nativesharp::frontend::NamespaceStateSymbol> restoredConstants;
                            const bool membersOk = reconstructPrecompiledCMembers(duplicates,
                                *recordDependencies.projectGraph(), "x86_64-pc-windows-msvc",
                                *recordDependencies.nameLookup(), restoredFields, restoredConstants, dependencyError);
                            ok &= expect(membersOk && restoredFields.size() == 3 && restoredConstants.empty(),
                                         "precompiled C catalogues deduplicate reconstructed fields");
                            auto conflictingRecords = catalogRecords;
                            conflictingRecords.front().clangUsr = "conflicting-usr";
                            auto conflict = module;
                            const bool encodedConflict = metadata_detail::encodeCTypeCatalog(
                                conflictingRecords, catalogEnums, conflict.cppCTypeCatalog, dependencyError);
                            const std::vector<nativesharp::frontend::PrecompiledSyntaxModule> conflicting{module, conflict};
                            const auto priorCount = restored.size();
                            ok &= expect(encodedConflict && !metadata_detail::reconstructPrecompiledCImportedTypes(
                                             conflicting, *recordDependencies.projectGraph(), "x86_64-pc-windows-msvc", restored, dependencyError) &&
                                         restored.size() == priorCount,
                                         "precompiled C catalogues reject conflicting declarations without partial output");
                            const auto priorFields = restoredFields.size();
                            ok &= expect(encodedConflict && !reconstructPrecompiledCMembers(conflicting,
                                             *recordDependencies.projectGraph(), "x86_64-pc-windows-msvc",
                                             *recordDependencies.nameLookup(), restoredFields, restoredConstants, dependencyError) &&
                                         restoredFields.size() == priorFields,
                                         "conflicting precompiled C catalogues preserve previous members on failure");
                        }
                    }
                }
                {
                    nativesharp::frontend::ProjectCompilation mismatchedDependency;
                    mismatchedDependency.setSourceBufferProvider([](const std::filesystem::path& source)
                        -> std::optional<std::string> {
                        if (source.filename() == "Main.ns") return "int Use(Branch* value) { return 0; } int Main() { return 0; }\n";
                        return std::nullopt;
                    });
                    auto realSession = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::C,
                        "struct Leaf { int value; }; struct Branch { struct Leaf items[2]; struct Branch* next; };\n"));
                    CppProjectSessions group;
                    group.projectFile = std::filesystem::absolute(path);
                    if (realSession) group.sessions.push_back(std::make_unique<AlteredFieldUsrSession>(std::move(realSession)));
                    std::vector<CppProjectSessions> groups;
                    groups.push_back(std::move(group));
                    std::string error;
                    const bool configured = !groups.front().sessions.empty() &&
                        mismatchedDependency.setCppProjectSessions(std::move(groups), error) &&
                        mismatchedDependency.setExperimentalCppTypeProvider(provideCppInteropTypes, error) &&
                        mismatchedDependency.setExperimentalCppTypeSelector(selectExperimentalCppRecordTypesForCompilation, error);
                    const bool ready = configured && mismatchedDependency.compileProjectNames(path, error);
                    ok &= expect(configured && !ready && error.find("different Clang USR") != std::string::npos,
                                 "C field dependency closure rejects a different known record USR");
                }
                for (bool useCollision : {false, true})
                {
                    nativesharp::frontend::ProjectCompilation collisionCompilation;
                    collisionCompilation.setSourceBufferProvider([useCollision](const std::filesystem::path& source)
                        -> std::optional<std::string> {
                        if (source.filename() != "Main.ns") return std::nullopt;
                        return useCollision ? "struct Collision {} int Use(Collision* x) { return 0; } int Main() { return 0; }\n"
                                            : "struct Collision {} int Main() { return 0; }\n";
                    });
                    CppProjectSessions group;
                    group.projectFile = std::filesystem::absolute(path);
                    group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::C, "enum Collision { CValue = 1 };\n")));
                    std::vector<CppProjectSessions> groups;
                    groups.push_back(std::move(group));
                    std::string failure;
                    const bool parsed = collisionCompilation.setCppProjectSessions(std::move(groups), failure) &&
                        collisionCompilation.compileProjectSyntax(path, failure);
                    const auto observed = parsed ? observeCppRecords(collisionCompilation) :
                        CppRecordObservationRunResult{CppRecordObservationFailure{failure}};
                    const auto* collisionError = std::get_if<CppRecordObservationFailure>(&observed);
                    ok &= expect(useCollision ? collisionError && collisionError->message.find("C enum conflicts") != std::string::npos
                                              : std::holds_alternative<CppRecordObservation>(observed),
                                 "C enum collision is diagnosed only when the conflicting type is used");
                }
                {
                    nativesharp::frontend::ProjectCompilation dependencyCollision;
                    dependencyCollision.setSourceBufferProvider([](const std::filesystem::path& source)
                        -> std::optional<std::string> {
                        if (source.filename() == "Main.ns")
                            return "struct Shade {} int Use(Box* value) { return 0; } int Main() { return 0; }\n";
                        return std::nullopt;
                    });
                    CppProjectSessions group;
                    group.projectFile = std::filesystem::absolute(path);
                    group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::C, "enum Shade { Dark = 1 }; struct Box { enum Shade color; };\n")));
                    std::vector<CppProjectSessions> groups;
                    groups.push_back(std::move(group));
                    std::string failure;
                    const bool configured = dependencyCollision.setCppProjectSessions(std::move(groups), failure) &&
                        dependencyCollision.setExperimentalCppTypeProvider(provideCppInteropTypes, failure) &&
                        dependencyCollision.setExperimentalCppTypeSelector(selectExperimentalCppRecordTypesForCompilation, failure);
                    const bool ready = configured && dependencyCollision.compileProjectNames(path, failure);
                    ok &= expect(configured && !ready && failure.find("C enum field conflicts") != std::string::npos,
                                 "record enum dependency cannot replace a colliding NativeSharp type");
                }
                nativesharp::frontend::ProjectCompilation unpreparedObservation;
                ok &= expect(std::holds_alternative<CppRecordObservationFailure>(observeCppRecords(unpreparedObservation)),
                             "record observation rejects an unprepared compilation");
                for (int collision : {0, 1, 2, 3, 4})
                {
                    nativesharp::frontend::ProjectCompilation observedCompilation;
                    observedCompilation.setSourceBufferProvider([collision](const std::filesystem::path& source)
                        -> std::optional<std::string> {
                        if (source.filename() != "Main.ns") return std::nullopt;
                        if (collision == 1) return "namespace A.B { struct C {} } int Main() { return 0; }\n";
                        if (collision == 2) return "namespace A.B { struct C {} } int Use(A.B.C* item) { return 0; } int Main() { return 0; }\n";
                        if (collision == 3) return "namespace A.B { struct C { public static int X; } } int Main() { return A.B.C.X; }\n";
                        if (collision == 4) return "using A.B; namespace A.B { struct C {} } class Holder { public int X; } int Main() { Holder C; return C.X; }\n";
                        return "using Alias = A.B.C; int Use(Alias* item) { return 0; } int Main() { return 0; }\n";
                    });
                    CppProjectSessions sessionGroup;
                    sessionGroup.projectFile = std::filesystem::absolute(path);
                    sessionGroup.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::Cpp, "namespace A { namespace B { struct C {}; } }\n")));
                    std::vector<CppProjectSessions> sessions;
                    sessions.push_back(std::move(sessionGroup));
                    std::string observedError;
                    const bool observedParsed = observedCompilation.setCppProjectSessions(std::move(sessions), observedError) &&
                        observedCompilation.compileProjectSyntax(path, observedError);
                    const nativesharp::ast::TypeSyntax* observedAlias = nullptr;
                    std::vector<const nativesharp::ast::AstNode*> nodes;
                    std::unordered_set<const nativesharp::ast::AstNode*> seenNodes;
                    for (const auto& unit : observedCompilation.units())
                        if (unit && unit->syntaxTree()) nodes.push_back(unit->syntaxTree());
                    while (!nodes.empty())
                    {
                        const auto* node = nodes.back(); nodes.pop_back();
                        if (!node || !seenNodes.insert(node).second) continue;
                        if (node->typeSyntax() && node->typeSyntax()->baseName() == "Alias") observedAlias = node->typeSyntax();
                        nodes.push_back(node->first()); nodes.push_back(node->second()); nodes.push_back(node->third());
                        for (const auto* child : node->children()) nodes.push_back(child);
                    }
                    const auto observed = observedParsed ? observeCppRecords(observedCompilation) :
                        CppRecordObservationRunResult{CppRecordObservationFailure{observedError}};
                    const auto* observedResult = std::get_if<CppRecordObservation>(&observed);
                    bool referenced = false;
                    if (observedResult)
                        for (const auto groupIndex : observedResult->selection.referencedGroups)
                            referenced |= groupIndex < observedResult->groups.groups.size() &&
                                observedResult->groups.groups[groupIndex].identity.key() == namespaceIdentity->key();
                    const bool collisionTracked = observedResult && observedResult->collidingGroups.size() == 1 &&
                        (collision != 2 || observedResult->referencedCollidingGroups == observedResult->collidingGroups) &&
                        ((collision != 3 && collision != 4) || observedResult->expressionCollidingGroups == observedResult->collidingGroups) &&
                        (collision != 1 || (observedResult->referencedCollidingGroups.empty() &&
                                            observedResult->expressionCollidingGroups.empty()));
                    ok &= expect(observedParsed && !observedCompilation.diagnostics().hasErrors() &&
                                 (collision ? collisionTracked : referenced && observedAlias && observedAlias->baseName() == "Alias"),
                                 "real compilation observation separates unused and referenced collisions without mutating syntax");
                    if (observedResult)
                    {
                        const auto selected = selectExperimentalCppRecordTypes(*observedResult);
                        const auto* inputs = std::get_if<CppRecordObservationInputs>(&selected);
                        ok &= expect(inputs && (collision ? inputs->types.empty() : inputs->types.size() == 1),
                                     "record selection uses real observation to retain imported or native identity");
                    }
                }
            }
            {
                const auto root = std::filesystem::path(argv[2]) / "cpp-fixtures/record-visibility";
                for (int scenario : {0, 1, 2})
                {
                    const auto base = root / std::to_string(scenario);
                    const auto core = base / "Core";
                    const auto app = base / "App";
                    std::filesystem::create_directories(core);
                    std::filesystem::create_directories(app);
                    { std::ofstream file(core / "Core.nsproj"); file << "<Project><PropertyGroup><Name>Core</Name><OutputType>Library</OutputType></PropertyGroup></Project>"; }
                    { std::ofstream file(core / "Core.ns"); file << (scenario == 2 ? "namespace Shared { public struct Token {} }\n" : "int CoreValue() { return 1; }\n"); }
                    { std::ofstream file(app / "App.nsproj"); file << "<Project><PropertyGroup><Name>App</Name></PropertyGroup><ItemGroup><ProjectReference Include=\"../Core/Core.nsproj\" /></ItemGroup></Project>"; }
                    { std::ofstream file(app / "App.ns");
                      if (scenario == 1) file << "namespace Shared { public struct Token {} } ";
                      file << "int Use(Shared.Token* item) { return 0; } int Main() { return 0; }\n"; }
                    nativesharp::frontend::ProjectCompilation visibilityCompilation;
                    CppProjectSessions visibilityGroup;
                    visibilityGroup.projectFile = std::filesystem::absolute(core / "Core.nsproj");
                    visibilityGroup.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                        CppLanguageMode::Cpp, "namespace Shared { struct Token {}; }\n")));
                    std::vector<CppProjectSessions> visibilitySessions;
                    visibilitySessions.push_back(std::move(visibilityGroup));
                    std::string visibilityError;
                    const bool visibilityParsed = visibilityCompilation.setCppProjectSessions(std::move(visibilitySessions), visibilityError) &&
                        visibilityCompilation.compileProjectSyntax(app / "App.nsproj", visibilityError);
                    const auto visibleResult = visibilityParsed ? observeCppRecords(visibilityCompilation) :
                        CppRecordObservationRunResult{CppRecordObservationFailure{visibilityError}};
                    const auto* observation = std::get_if<CppRecordObservation>(&visibleResult);
                    bool valid = observation && !visibilityCompilation.diagnostics().hasErrors();
                    if (observation && scenario == 0)
                        valid &= observation->collidingGroups.empty() && observation->selection.referencedGroups.size() == 1;
                    else if (observation && scenario == 1)
                        valid &= observation->collidingGroups.empty() && observation->selection.referencedGroups.empty();
                    else if (observation && scenario == 2)
                        valid &= observation->collidingGroups.size() == 1 &&
                            observation->referencedCollidingGroups == observation->collidingGroups;
                    if (!valid)
                    {
                        if (const auto* failure = std::get_if<CppRecordObservationFailure>(&visibleResult))
                            std::cerr << "record visibility scenario " << scenario << ": " << failure->message << '\n';
                    }
                    ok &= expect(valid, "record observation preserves project visibility and local type precedence");
                }
            }
            {
                const auto root = std::filesystem::path(argv[2]) / "cpp-fixtures/parser-visibility";
                const auto core = root / "Core";
                const auto app = root / "App";
                std::filesystem::create_directories(core);
                std::filesystem::create_directories(app);
                { std::ofstream file(core / "Core.nsproj"); file << "<Project><PropertyGroup><Name>Core</Name><OutputType>Library</OutputType></PropertyGroup></Project>"; }
                { std::ofstream file(core / "Core.ns"); file << "int CoreValue() { int Token = 2; int p = 3; Token * p; return Token; }\n"; }
                { std::ofstream file(app / "App.nsproj"); file << "<Project><PropertyGroup><Name>App</Name></PropertyGroup><ItemGroup><ProjectReference Include=\"../Core/Core.nsproj\" /></ItemGroup></Project>"; }
                { std::ofstream file(app / "App.ns"); file << "using Shared; int Main() { Token* item = null; return 0; }\n"; }
                nativesharp::compilation::ProjectLoadOptions loadOptions;
                nativesharp::compilation::ProjectGraphLoader graphLoader(1, {}, loadOptions);
                std::string visibilityError;
                auto graph = graphLoader.load(app / "App.nsproj", visibilityError);
                std::size_t appOrdinal = 0;
                if (graph)
                    for (std::size_t i = 0; i < graph->projects().size(); ++i)
                        if (graph->projects()[i].moduleName() == "App") appOrdinal = i;
                auto recordSession = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
                    "namespace Shared { struct Token {}; }\n"));
                const auto records = recordSession ? recordSession->recordsSnapshot() : std::nullopt;
                const auto* token = records ? recordByName(*records, "Shared::Token") : nullptr;
                const auto identity = token ? projectCppRecordIdentity(*token, "App") : std::nullopt;
                if (graph && identity)
                {
                    std::size_t ownerCore = 0;
                    for (std::size_t i = 0; i < graph->projects().size(); ++i)
                        if (graph->projects()[i].moduleName() == "Core") ownerCore = i;
                    const nativesharp::FileId inventoryFile{81};
                    nativesharp::source::SourceFile inventorySource(inventoryFile, "inventory.ns",
                        "namespace Native.Inner { public struct Data {} }");
                    auto inventoryUnit = std::make_unique<nativesharp::frontend::ProjectSyntaxUnit>(
                        ownerCore, 0, "Core", inventoryFile);
                    nativesharp::diagnostics::DiagnosticEngine inventoryDiagnostics;
                    const bool inventoryLexed = inventoryUnit->lex(inventorySource, inventoryDiagnostics);
                    std::vector<std::unique_ptr<nativesharp::frontend::ProjectSyntaxUnit>> inventoryUnits;
                    inventoryUnits.push_back(std::move(inventoryUnit));
                    const std::vector<nativesharp::frontend::CppImportedTypeDeclaration> imported{{*identity,
                        nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                        nativesharp::frontend::NamedTypeAccessibility::Public, appOrdinal, "sample.h"}};
                    const std::vector<std::pair<std::size_t, std::string>> extra{{appOrdinal, "External.More"}};
                    const auto inventory = nativesharp::frontend::cppParserProjectInventories(
                        *graph, inventoryUnits, imported, extra);
                    CppTypeAliasDeclaration parserAlias;
                    parserAlias.name = "CNumber";
                    parserAlias.qualifiedName = "External::CNumber";
                    CppTypeAliasDeclaration hiddenParserAlias;
                    hiddenParserAlias.name = "HiddenAlias";
                    hiddenParserAlias.qualifiedName = "HiddenAlias";
                    hiddenParserAlias.insideAnonymousNamespace = true;
                    const std::vector<nativesharp::frontend::CppImportedTypeAlias> parserAliases{{
                        appOrdinal, graph->projects()[appOrdinal].projectFile(), 0,
                        "x86_64-pc-windows-msvc", parserAlias}, {
                        appOrdinal, graph->projects()[appOrdinal].projectFile(), 0,
                        "x86_64-pc-windows-msvc", hiddenParserAlias}};
                    const auto withAlias = nativesharp::frontend::cppParserProjectInventories(
                        *graph, inventoryUnits, imported, extra, parserAliases);
                    auto contains = [](const auto& values, const auto& value) {
                        return std::find(values.begin(), values.end(), value) != values.end();
                    };
                    ok &= expect(inventoryLexed && inventory && inventory->size() == graph->projects().size() &&
                        (*inventory)[ownerCore].cppTypes.empty() &&
                        !contains((*inventory)[ownerCore].namespaces, "External") &&
                        contains((*inventory)[appOrdinal].cppTypes, "Shared.Token") &&
                        contains((*inventory)[appOrdinal].namespaces, "Native.Inner") &&
                        contains((*inventory)[appOrdinal].namespaces, "Native") &&
                        contains((*inventory)[appOrdinal].namespaces, "External.More") &&
                        contains((*inventory)[appOrdinal].namespaces, "External"),
                        "parser inventory follows project references and retains native and C++ namespace prefixes");
                    ok &= expect(withAlias &&
                        contains((*withAlias)[appOrdinal].cppTypes, "External.CNumber") &&
                        !contains((*withAlias)[appOrdinal].cppTypes, "HiddenAlias") &&
                        !contains((*withAlias)[ownerCore].cppTypes, "External.CNumber"),
                        "parser alias names follow project visibility and C++ namespace spelling");
                    const auto consumer = root / "Consumer";
                    std::filesystem::create_directories(consumer);
                    { std::ofstream file(consumer / "Consumer.nsproj");
                      file << "<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup><ItemGroup>"
                              "<ProjectReference Include=\"../App/App.nsproj\" /></ItemGroup></Project>"; }
                    { std::ofstream file(consumer / "Main.ns"); file << "int Main() { return 0; }\n"; }
                    auto transitive = graphLoader.load(consumer / "Consumer.nsproj", visibilityError);
                    std::size_t transitiveCore = 0, transitiveApp = 0, transitiveConsumer = 0;
                    if (transitive)
                        for (std::size_t i = 0; i < transitive->projects().size(); ++i)
                        {
                            const auto& project = transitive->projects()[i];
                            if (project.moduleName() == "Core") transitiveCore = i;
                            if (project.moduleName() == "App") transitiveApp = i;
                            if (project.moduleName() == "Consumer") transitiveConsumer = i;
                        }
                    const std::vector<nativesharp::frontend::CppImportedTypeDeclaration> transitiveTypes{{*identity,
                        nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                        nativesharp::frontend::NamedTypeAccessibility::Public, transitiveApp, "sample.h"}};
                    const auto transitiveInventory = transitive ?
                        nativesharp::frontend::cppParserProjectInventories(*transitive, {}, transitiveTypes, {}) :
                        std::nullopt;
                    ok &= expect(transitiveInventory &&
                        contains((*transitiveInventory)[transitiveConsumer].visibleProjects, transitiveCore) &&
                        contains((*transitiveInventory)[transitiveConsumer].cppTypes, "Shared.Token") &&
                        (*transitiveInventory)[transitiveCore].cppTypes.empty(),
                        "parser inventory propagates types transitively without reverse visibility");
                }
                nativesharp::frontend::ProjectCompilation compilation;
                const bool ready = graph && identity && token && compilation.setPreloadedProjectGraph(std::move(*graph), visibilityError) &&
                    compilation.setExperimentalCppImportedTypes({{*identity,
                        nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                        nativesharp::frontend::NamedTypeAccessibility::Public, appOrdinal,
                        token->location ? token->location->file.generic_string() : ""}}, visibilityError) &&
                    compilation.compileProjectSyntax(app / "App.nsproj", visibilityError);
                auto hasCoreMultiplication = [](const auto& units) {
                    std::vector<const nativesharp::ast::AstNode*> pending;
                    std::unordered_set<const nativesharp::ast::AstNode*> seen;
                    for (const auto& unit : units)
                        if (unit && unit->moduleName() == "Core" && unit->syntaxTree()) pending.push_back(unit->syntaxTree());
                    while (!pending.empty())
                    {
                        const auto* node = pending.back(); pending.pop_back();
                        if (!node || !seen.insert(node).second) continue;
                        if (node->kind() == nativesharp::ast::AstNodeKind::BinaryExpression &&
                            node->tokenKind() == nativesharp::lexer::TokenKind::Star) return true;
                        pending.push_back(node->first()); pending.push_back(node->second()); pending.push_back(node->third());
                        for (const auto* child : node->children()) pending.push_back(child);
                    }
                    return false;
                };
                auto hasAppPointer = [](const auto& units) {
                    std::vector<const nativesharp::ast::AstNode*> pending;
                    std::unordered_set<const nativesharp::ast::AstNode*> seen;
                    for (const auto& unit : units)
                        if (unit && unit->moduleName() == "App" && unit->syntaxTree()) pending.push_back(unit->syntaxTree());
                    while (!pending.empty())
                    {
                        const auto* node = pending.back(); pending.pop_back();
                        if (!node || !seen.insert(node).second) continue;
                        if (node->kind() == nativesharp::ast::AstNodeKind::VariableDeclarationStatement &&
                            node->text() == "item" && node->typeSyntax() &&
                            (node->typeSyntax()->baseName() == "Token" ||
                             node->typeSyntax()->baseName() == "Shared.Token")) return true;
                        pending.push_back(node->first()); pending.push_back(node->second()); pending.push_back(node->third());
                        for (const auto* child : node->children()) pending.push_back(child);
                    }
                    return false;
                };
                auto hasAppMultiplication = [](const auto& units) {
                    std::vector<const nativesharp::ast::AstNode*> pending;
                    std::unordered_set<const nativesharp::ast::AstNode*> seen;
                    for (const auto& unit : units)
                        if (unit && unit->moduleName() == "App" && unit->syntaxTree()) pending.push_back(unit->syntaxTree());
                    while (!pending.empty())
                    {
                        const auto* node = pending.back(); pending.pop_back();
                        if (!node || !seen.insert(node).second) continue;
                        if (node->kind() == nativesharp::ast::AstNodeKind::BinaryExpression &&
                            node->tokenKind() == nativesharp::lexer::TokenKind::Star) return true;
                        pending.push_back(node->first()); pending.push_back(node->second()); pending.push_back(node->third());
                        for (const auto* child : node->children()) pending.push_back(child);
                    }
                    return false;
                };
                std::vector<std::unique_ptr<nativesharp::frontend::ProjectSyntaxUnit>> copies;
                nativesharp::diagnostics::DiagnosticEngine copyDiagnostics;
                const bool copied = ready && compilation.cloneSyntaxForObservation(copies, copyDiagnostics, visibilityError);
                ok &= expect(ready && copied && hasCoreMultiplication(compilation.units()) && hasCoreMultiplication(copies) &&
                             hasAppPointer(compilation.units()) && hasAppPointer(copies),
                             "C++ parser seed is visible to its owner without changing a dependency's multiplication");
                { std::ofstream file(core / "Core.ns"); file << "int CoreValue() { return 2; }\n"; }
                auto dependencyGraph = graphLoader.load(app / "App.nsproj", visibilityError);
                std::size_t coreOrdinal = 0;
                if (dependencyGraph)
                    for (std::size_t i = 0; i < dependencyGraph->projects().size(); ++i)
                        if (dependencyGraph->projects()[i].moduleName() == "Core") coreOrdinal = i;
                const auto dependencyIdentity = token ? projectCppRecordIdentity(*token, "Core") : std::nullopt;
                nativesharp::frontend::ProjectCompilation dependencyCompilation;
                const bool dependencyReady = dependencyGraph && dependencyIdentity && token &&
                    dependencyCompilation.setPreloadedProjectGraph(std::move(*dependencyGraph), visibilityError) &&
                    dependencyCompilation.setExperimentalCppImportedTypes({{*dependencyIdentity,
                        nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                        nativesharp::frontend::NamedTypeAccessibility::Public, coreOrdinal,
                        token->location ? token->location->file.generic_string() : ""}}, visibilityError) &&
                    dependencyCompilation.compileProjectSyntax(app / "App.nsproj", visibilityError);
                std::vector<std::unique_ptr<nativesharp::frontend::ProjectSyntaxUnit>> dependencyCopies;
                nativesharp::diagnostics::DiagnosticEngine dependencyDiagnostics;
                const bool dependencyCopied = dependencyReady && dependencyCompilation.cloneSyntaxForObservation(
                    dependencyCopies, dependencyDiagnostics, visibilityError);
                ok &= expect(dependencyReady && dependencyCopied && hasAppPointer(dependencyCompilation.units()) &&
                             hasAppPointer(dependencyCopies),
                             "C++ parser seed is visible through a project reference in original and copied syntax");

                { std::ofstream file(app / "App.ns");
                  file << "using Shared; int Main() { int Token = 2; int p = 3; Token * p; return Token; }\n"; }
                auto sameProjectHasMultiplication = [&](bool withImportedType, bool* hasPointer = nullptr,
                                                        bool* cloneMultiplication = nullptr) {
                    auto sameGraph = graphLoader.load(app / "App.nsproj", visibilityError);
                    if (!sameGraph) return false;
                    std::size_t owner = 0;
                    for (std::size_t i = 0; i < sameGraph->projects().size(); ++i)
                        if (sameGraph->projects()[i].moduleName() == "App") owner = i;
                    nativesharp::frontend::ProjectCompilation sameCompilation;
                    if (!sameCompilation.setPreloadedProjectGraph(std::move(*sameGraph), visibilityError)) return false;
                    if (withImportedType && !sameCompilation.setExperimentalCppImportedTypes({{*identity,
                            nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                            nativesharp::frontend::NamedTypeAccessibility::Public, owner,
                            token->location ? token->location->file.generic_string() : ""}}, visibilityError)) return false;
                    if (!sameCompilation.compileProjectSyntax(app / "App.nsproj", visibilityError)) return false;
                    if (hasPointer) *hasPointer = hasAppPointer(sameCompilation.units());
                    if (cloneMultiplication)
                    {
                        std::vector<std::unique_ptr<nativesharp::frontend::ProjectSyntaxUnit>> clones;
                        nativesharp::diagnostics::DiagnosticEngine diagnostics;
                        *cloneMultiplication = sameCompilation.cloneSyntaxForObservation(clones, diagnostics, visibilityError) &&
                            !diagnostics.hasErrors() && hasAppMultiplication(clones);
                    }
                    std::vector<const nativesharp::ast::AstNode*> pending;
                    std::unordered_set<const nativesharp::ast::AstNode*> seen;
                    for (const auto& unit : sameCompilation.units())
                        if (unit && unit->moduleName() == "App" && unit->syntaxTree()) pending.push_back(unit->syntaxTree());
                    while (!pending.empty())
                    {
                        const auto* node = pending.back(); pending.pop_back();
                        if (!node || !seen.insert(node).second) continue;
                        if (node->kind() == nativesharp::ast::AstNodeKind::BinaryExpression &&
                            node->tokenKind() == nativesharp::lexer::TokenKind::Star) return true;
                        pending.push_back(node->first()); pending.push_back(node->second()); pending.push_back(node->third());
                        for (const auto* child : node->children()) pending.push_back(child);
                    }
                    return false;
                };
                const bool plainMultiplication = sameProjectHasMultiplication(false);
                const bool importedMultiplication = identity && token && sameProjectHasMultiplication(true);
                { std::ofstream file(app / "App.ns");
                  file << "using Shared; namespace Shared { public struct Token {} } "
                          "int Main() { int Token = 2; int p = 3; Token * p; return Token; }\n"; }
                const bool nativeTypeMultiplication = sameProjectHasMultiplication(false);
                std::cerr << "same-project multiplication: plain=" << plainMultiplication
                          << " imported=" << importedMultiplication
                          << " native=" << nativeTypeMultiplication << '\n';
                ok &= expect(plainMultiplication && importedMultiplication == nativeTypeMultiplication,
                             "C++ parser seed follows the existing NativeSharp type-name disambiguation");
                { std::ofstream file(app / "App.ns");
                  file << "int Main() { int Token = 2; int p = 3; Token * p; return Token; }\n"; }
                const bool unrelatedPlain = sameProjectHasMultiplication(false);
                bool clonedUnrelated = false;
                const bool unrelatedImported = identity && token &&
                    sameProjectHasMultiplication(true, nullptr, &clonedUnrelated);
                { std::ofstream file(app / "App.ns");
                  file << "int Main() { Shared.Token* item = null; int Token = 2; int p = 3; "
                          "Token * p; return Token; }\n"; }
                bool qualifiedPointer = false;
                if (identity && token) sameProjectHasMultiplication(true, &qualifiedPointer);
                std::cerr << "unqualified name without using: plain=" << unrelatedPlain
                          << " imported=" << unrelatedImported << " qualified=" << qualifiedPointer << '\n';
                ok &= expect(unrelatedPlain && unrelatedImported && clonedUnrelated,
                             "C++ type in an unimported namespace must not change original or cloned expressions");
                ok &= expect(qualifiedPointer,
                             "qualified C++ pointer remains a declaration without a using directive");
            }
            {
                const auto metadata = std::filesystem::path(argv[2]) / "cpp-fixtures/observation-module.nsmeta";
                { std::ofstream file(metadata); file << "observation fixture"; }
                std::string moduleError;
                nativesharp::frontend::PrecompiledSyntaxModule module;
                module.ownerProjectFile = path;
                module.metadataPath = metadata;
                module.moduleName = "ObservedModule";
                module.sourceText = "namespace Published { public struct Value {} }\n";
                nativesharp::frontend::ProjectCompilation moduleCompilation;
                moduleCompilation.setSourceBufferProvider([](const std::filesystem::path& source)
                    -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return "int Use(Published.Value* value, A.B.C* item) { return 0; } int Main() { return 0; }\n";
                    return std::nullopt;
                });
                CppProjectSessions moduleSessions;
                moduleSessions.projectFile = std::filesystem::absolute(path);
                moduleSessions.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                    CppLanguageMode::Cpp, "namespace A { namespace B { struct C {}; } }\n")));
                std::vector<CppProjectSessions> moduleSessionGroups;
                moduleSessionGroups.push_back(std::move(moduleSessions));
                const bool moduleReady = moduleCompilation.setPrecompiledModules({module}, moduleError) &&
                    moduleCompilation.setCppProjectSessions(std::move(moduleSessionGroups), moduleError) &&
                    moduleCompilation.compileProjectSyntax(path, moduleError);
                std::vector<std::unique_ptr<nativesharp::frontend::ProjectSyntaxUnit>> moduleCopies;
                nativesharp::diagnostics::DiagnosticEngine moduleDiagnostics;
                const bool moduleCloned = moduleReady && moduleCompilation.cloneSyntaxForObservation(
                    moduleCopies, moduleDiagnostics, moduleError);
                bool independentModule = false;
                for (std::size_t i = 0; moduleCloned && i < moduleCopies.size(); ++i)
                    if (moduleCompilation.units()[i]->isPrecompiled())
                        independentModule |= moduleCopies[i]->isPrecompiled() &&
                            moduleCopies[i]->fileId() == moduleCompilation.units()[i]->fileId() &&
                            moduleCopies[i]->syntaxTree() != moduleCompilation.units()[i]->syntaxTree();
                const auto moduleObserved = moduleCloned ? observeCppRecords(moduleCompilation) :
                    CppRecordObservationRunResult{CppRecordObservationFailure{moduleError}};
                const auto* moduleResult = std::get_if<CppRecordObservation>(&moduleObserved);
                if (!moduleResult) std::cerr << "precompiled observation: " << moduleError << '\n';
                ok &= expect(moduleReady && independentModule && moduleResult &&
                             moduleResult->selection.referencedGroups.size() == 1 &&
                             !moduleDiagnostics.hasErrors() && !moduleCompilation.diagnostics().hasErrors(),
                             "observation preserves independent precompiled units and resolves C++ records");

                // Scoped C++ names depend on project and unit visibility, so precompiled syntax
                // caching is disabled for this experimental configuration until keyed completely.
                const auto cacheRoot = std::filesystem::path(argv[2]) / "cpp-fixtures" /
                    ("record-seed-cache-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
                module.productCacheKey = "record-seed-cache-fixture";
                auto runCache = [&](bool withType) {
                    nativesharp::frontend::ProjectCompilation cached;
                    cached.setTemplateCacheRoot(cacheRoot);
                    cached.setSourceBufferProvider([](const std::filesystem::path& source)
                        -> std::optional<std::string> {
                        if (source.filename() == "Main.ns") return "int Main() { return 0; }\n";
                        return std::nullopt;
                    });
                    std::string cacheError;
                    const bool configured = cached.setPrecompiledModules({module}, cacheError) &&
                        (!withType || cached.setExperimentalCppImportedTypes({{*namespaceIdentity,
                            nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                            nativesharp::frontend::NamedTypeAccessibility::Public, 0,
                            namespaceC->location ? namespaceC->location->file.generic_string() : ""}}, cacheError));
                    const bool ready = configured && cached.compileProjectGenerics(path, cacheError);
                    if (!ready) std::cerr << "record seed cache fixture: " << cacheError << '\n';
                    return ready;
                };
                auto countEntries = [&]() {
                    const auto directory = cacheRoot / "precompiled-products" / "syntax" / "templates";
                    std::size_t count = 0;
                    std::error_code ec;
                    if (std::filesystem::is_directory(directory, ec))
                        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
                            if (entry.is_regular_file() && entry.path().extension() == ".tpl") ++count;
                    return count;
                };
                const bool coldWithout = runCache(false);
                const auto withoutCount = countEntries();
                const bool coldWith = runCache(true);
                const auto withCount = countEntries();
                std::vector<std::pair<std::filesystem::path, std::filesystem::file_time_type>> cachedTimes;
                const auto syntaxDirectory = cacheRoot / "precompiled-products" / "syntax" / "templates";
                if (std::filesystem::is_directory(syntaxDirectory))
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(syntaxDirectory))
                        if (entry.is_regular_file() && entry.path().extension() == ".tpl")
                        {
                            const auto timestamp = std::filesystem::file_time_type::clock::now() - std::chrono::hours(24);
                            std::filesystem::last_write_time(entry.path(), timestamp);
                            cachedTimes.emplace_back(entry.path(), std::filesystem::last_write_time(entry.path()));
                        }
                const bool repeatWith = runCache(true);
                const auto repeatCount = countEntries();
                bool preserved = cachedTimes.size() == 1;
                for (const auto& [file, timestamp] : cachedTimes)
                    preserved &= std::filesystem::last_write_time(file) == timestamp;
                ok &= expect(coldWithout && coldWith && repeatWith && withoutCount == 1 && withCount == 1 && repeatCount == 1 && preserved,
                             "experimental scoped C++ names bypass precompiled syntax cache and preserve the native entry");

                nativesharp::frontend::ProjectCompilation providerCompilation;
                providerCompilation.setSourceBufferProvider([](const std::filesystem::path& source)
                    -> std::optional<std::string> {
                    if (source.filename() == "Main.ns")
                        return "int Main() { A.B.C* item = null; return 0; }\n";
                    return std::nullopt;
                });
                CppProjectSessions providerSessions;
                providerSessions.projectFile = std::filesystem::absolute(path);
                providerSessions.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                    CppLanguageMode::Cpp, "namespace A { namespace B { struct C { int value; }; } }\n")));
                std::vector<CppProjectSessions> providerGroups;
                providerGroups.push_back(std::move(providerSessions));
                std::string providerError;
                bool ownerMatches = false;
                const bool providerConfigured = providerCompilation.setPrecompiledModules({module}, providerError) &&
                    providerCompilation.setCppProjectSessions(std::move(providerGroups), providerError) &&
                    providerCompilation.setExperimentalCppTypeProvider(
                        [&](const auto& finalGraph, auto sessions, auto& output, auto& error) {
                            if (!provideExperimentalCppRecordTypes(finalGraph, sessions, output, error)) return false;
                            if (output.size() != 1 || output[0].projectOrdinal >= finalGraph.projects().size()) return false;
                            std::error_code ec;
                            ownerMatches = std::filesystem::equivalent(
                                finalGraph.projects()[output[0].projectOrdinal].projectFile(), path, ec) && !ec;
                            return ownerMatches;
                        }, providerError);
                const bool providerParsed = providerConfigured && providerCompilation.compileProjectSyntax(path, providerError);
                if (!providerParsed) std::cerr << "record provider precompiled fixture: " << providerError << '\n';
                ok &= expect(providerParsed && ownerMatches && !providerCompilation.diagnostics().hasErrors(),
                             "record provider receives final graph after precompiled projects and retains the real owner");
            }
            {
                std::vector<CppSemanticRecordEntry> realRecords{
                    {path, 0, 0, "x86_64-pc-windows-msvc", *namespaceC, namespaceIdentity},
                    {path, 1, 0, "different-target", *namespaceC, namespaceIdentity}};
                const auto realGroups = groupCppSemanticRecords(realRecords);
                auto prepared = prepareCppRecordObservation(realRecords, realGroups);
                auto* automaticInputs = std::get_if<CppRecordObservationInputs>(&prepared);
                std::vector<CppSemanticRecordEntry> singleRecord{
                    {path, 0, 0, "x86_64-pc-windows-msvc", *namespaceC, namespaceIdentity}};
                const auto singleGroups = groupCppSemanticRecords(singleRecord);
                CppRecordObservation selectionFixture;
                selectionFixture.inventory = singleRecord;
                selectionFixture.groups = singleGroups;
                auto selected = selectExperimentalCppRecordTypes(selectionFixture);
                auto* selectedInputs = std::get_if<CppRecordObservationInputs>(&selected);
                ok &= expect(selectedInputs && selectedInputs->types.size() == 1 &&
                             selectedInputs->groupByImportedType == std::vector<std::size_t>{0},
                             "experimental record selection preserves a unique valid candidate");
                selectionFixture.collidingGroups = {0};
                selectionFixture.referencedCollidingGroups = {0};
                selected = selectExperimentalCppRecordTypes(selectionFixture);
                selectedInputs = std::get_if<CppRecordObservationInputs>(&selected);
                ok &= expect(selectedInputs && selectedInputs->types.empty(),
                             "native record collision excludes C++ candidate while preserving native uses");
                selectionFixture.collidingGroups.clear();
                selectionFixture.referencedCollidingGroups.clear();
                selectionFixture.inventory = realRecords;
                selectionFixture.groups = realGroups;
                selected = selectExperimentalCppRecordTypes(selectionFixture);
                selectedInputs = std::get_if<CppRecordObservationInputs>(&selected);
                ok &= expect(selectedInputs && selectedInputs->types.empty() &&
                             selectedInputs->deferredGroups == std::vector<std::size_t>{0},
                             "unreferenced conflicting record is deferred instead of rejecting selection");
                selectionFixture.selection.referencedGroups = {0};
                ok &= expect(std::holds_alternative<CppRecordObservationFailure>(
                                 selectExperimentalCppRecordTypes(selectionFixture)),
                             "referenced conflicting record is rejected");
                selectionFixture.selection.referencedGroups.clear();
                selectionFixture.selection.expressionCandidateGroups = {0};
                ok &= expect(std::holds_alternative<CppRecordObservationFailure>(
                                 selectExperimentalCppRecordTypes(selectionFixture)),
                             "potential expression use of conflicting record is rejected conservatively");
                selectionFixture.selection.expressionCandidateGroups = {1};
                ok &= expect(std::holds_alternative<CppRecordObservationFailure>(
                                 selectExperimentalCppRecordTypes(selectionFixture)),
                             "record selection rejects invalid observation indices");
                selectionFixture.selection.expressionCandidateGroups.clear();
                selectionFixture.referencedCollidingGroups = {0};
                ok &= expect(std::holds_alternative<CppRecordObservationFailure>(
                                 selectExperimentalCppRecordTypes(selectionFixture)),
                             "record selection rejects a native collision without a collision group");
                const auto singlePrepared = prepareCppRecordObservation(singleRecord, singleGroups);
                const auto* singleInputs = std::get_if<CppRecordObservationInputs>(&singlePrepared);
                if (singleInputs && !singleInputs->types.empty())
                {
                    auto invalid = singleInputs->types;
                    invalid[0].objectLayout = nativesharp::frontend::CppImportedObjectLayout{16, 3};
                    nativesharp::frontend::ProjectNameLookup malformedLookup;
                    ok &= expect(!malformedLookup.setCppImportedTypes(invalid) &&
                                 malformedLookup.setCppImportedTypes(singleInputs->types),
                                 "invalid imported dimensions are rejected without poisoning lookup configuration");
                }
                const bool clangDimensions = singleInputs && singleInputs->types.size() == 1 &&
                    singleInputs->types[0].objectLayout && namespaceC->sizeBits && namespaceC->alignmentBits &&
                    singleInputs->types[0].objectLayout->sizeBytes * 8 == *namespaceC->sizeBits &&
                    singleInputs->types[0].objectLayout->alignmentBytes * 8 == *namespaceC->alignmentBits;
                ok &= expect(clangDimensions && automaticInputs && automaticInputs->types.size() == 1 &&
                             !automaticInputs->types[0].objectLayout,
                             "unique Clang definition retains object dimensions while a conflicting group has none");
                nativesharp::frontend::ProjectCompilation automaticCompilation;
                automaticCompilation.setSourceBufferProvider([](const std::filesystem::path& source)
                    -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return "int Use(A.B.C* item) { return 0; } int Main() { return 0; }\n";
                    return std::nullopt;
                });
                std::string automaticError;
                const bool automaticParsed = automaticCompilation.compileProjectSyntax(path, automaticError);
                nativesharp::diagnostics::DiagnosticEngine automaticDiagnostics;
                std::vector<std::unique_ptr<nativesharp::frontend::ProjectSyntaxUnit>> automaticCopies;
                const bool automaticCopied = automaticParsed && automaticCompilation.cloneSyntaxForObservation(
                    automaticCopies, automaticDiagnostics, automaticError);
                nativesharp::frontend::ProjectNameLookup automaticLookup;
                const bool automaticReady = automaticInputs && automaticCopied &&
                    automaticLookup.setImportedNamespaces({{0, "A.B"}}) &&
                    automaticLookup.setCppImportedTypes(automaticInputs->types) &&
                    automaticLookup.build(*automaticCompilation.projectGraph(), automaticCopies, automaticDiagnostics);
                nativesharp::frontend::ProjectNameLookup dimensionsLookup;
                const bool dimensionsReady = singleInputs && automaticCopied &&
                    dimensionsLookup.setImportedNamespaces({{0, "A.B"}}) &&
                    dimensionsLookup.setCppImportedTypes(singleInputs->types) &&
                    dimensionsLookup.build(*automaticCompilation.projectGraph(), automaticCopies, automaticDiagnostics);
                const auto dimensionsOrdinal = dimensionsReady ? dimensionsLookup.typeOrdinalForIdentity(namespaceIdentity->key()) : std::nullopt;
                const auto* installedDimensions = dimensionsOrdinal ? dimensionsLookup.cppImportedType(*dimensionsOrdinal) : nullptr;
                ok &= expect(installedDimensions && installedDimensions->objectLayout && namespaceC->sizeBits &&
                             namespaceC->alignmentBits && installedDimensions->objectLayout->sizeBytes * 8 == *namespaceC->sizeBits &&
                             installedDimensions->objectLayout->alignmentBytes * 8 == *namespaceC->alignmentBits &&
                             !dimensionsLookup.cppImportedType(dimensionsLookup.namedTypes().size()),
                             "registered C++ type retains Clang dimensions and rejects unknown ordinals");
                const auto automaticSelected = automaticReady
                    ? selectCppRecordBindings(automaticLookup, realGroups, automaticInputs->groupByImportedType)
                    : CppRecordBindingResult{CppRecordBindingFailure{"observation failed"}};
                const auto* automaticSelection = std::get_if<CppRecordBindingSelection>(&automaticSelected);
                ok &= expect(automaticSelection && automaticSelection->referencedGroups.size() == 1 &&
                             realGroups.groups[automaticSelection->referencedGroups[0]].status == CppRecordGroupStatus::Conflict &&
                             !automaticCompilation.diagnostics().hasErrors(),
                             "real Clang inventory flows through automatic preparation and isolated type selection");
                if (nestedIdentity && nestedC)
                {
                    std::vector<CppSemanticRecordEntry> nestedEntries{
                        {path, 0, 0, "x86_64-pc-windows-msvc", *nestedC, nestedIdentity}};
                    const auto nestedGroups = groupCppSemanticRecords(nestedEntries);
                    const auto nestedPrepared = prepareCppRecordObservation(nestedEntries, nestedGroups);
                    const auto* nestedInputs = std::get_if<CppRecordObservationInputs>(&nestedPrepared);
                    ok &= expect(nestedInputs && nestedInputs->types.empty() && nestedInputs->deferredGroups.size() == 1,
                                 "nested record remains explicitly deferred in observation preparation");
                }
                auto missingLocation = realRecords;
                for (auto& entry : missingLocation) entry.declaration.location.reset();
                const auto missingPrepared = prepareCppRecordObservation(missingLocation, realGroups);
                const auto* missingInputs = std::get_if<CppRecordObservationInputs>(&missingPrepared);
                ok &= expect(missingInputs && missingInputs->types.empty() && missingInputs->deferredGroups.size() == 1,
                             "record with no source location remains explicitly deferred");
            }
            for (const auto& [source, expected] : std::vector<std::pair<std::string, std::string>>{
                     {"using A.B; int Use(C* item) { return 0; } int Main() { return 0; }\n", "C"},
                     {"using X = A.B; int Use(X.C* item) { return 0; } int Main() { return 0; }\n", "X.C"}})
            {
                nativesharp::frontend::ProjectCompilation scopedCompilation;
                scopedCompilation.setSourceBufferProvider([source](const std::filesystem::path& file)
                    -> std::optional<std::string> {
                    if (file.filename() == "Main.ns") return source;
                    return std::nullopt;
                });
                std::string scopeError;
                const bool scopeParsed = scopedCompilation.compileProjectSyntax(path, scopeError);
                nativesharp::diagnostics::DiagnosticEngine scopeObservationDiagnostics;
                nativesharp::frontend::ProjectNameLookup scopeObserver;
                const bool scopeObserverConfigured = scopeParsed && scopeObserver.setImportedNamespaces({{0, "A.B"}});
                const bool scopeObserverReady = scopeObserverConfigured && scopeObserver.build(
                    *scopedCompilation.projectGraph(), scopedCompilation.units(), scopeObservationDiagnostics);
                bool scopeDemand = false;
                for (const auto& binding : scopeObserver.bindings())
                    scopeDemand |= binding.kind == nativesharp::frontend::TypeNameBindingKind::Unresolved &&
                        binding.writtenName == expected && binding.syntax && binding.projectOrdinal == 0;
                ok &= expect(scopeObserverConfigured && !scopeObserverReady && scopeDemand &&
                             !scopedCompilation.diagnostics().hasErrors(),
                             "isolated observation retains using or alias type demand");
                nativesharp::frontend::ProjectNameLookup scopedLookup;
                const bool scopeReady = scopeParsed && scopedLookup.setImportedNamespaces({{0, "A.B"}}) &&
                    scopedLookup.setCppImportedTypes({{*namespaceIdentity,
                        nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                        nativesharp::frontend::NamedTypeAccessibility::Public, 0,
                        namespaceC->location ? namespaceC->location->file.generic_string() : ""}}) &&
                    scopedLookup.build(*scopedCompilation.projectGraph(), scopedCompilation.units(), scopedCompilation.diagnostics());
                const auto scopeOrdinal = scopeReady ? scopedLookup.typeOrdinalForIdentity(namespaceIdentity->key()) : std::nullopt;
                bool scopeBound = false;
                if (scopeOrdinal)
                    for (const auto& binding : scopedLookup.bindings())
                        scopeBound |= binding.writtenName == expected && binding.targetTypeOrdinal == scopeOrdinal;
                ok &= expect(scopeReady && scopeBound, "imported record resolves through using or namespace alias");
            }
            {
                const auto directory = std::filesystem::path(argv[2]) / "cpp-fixtures/observation-multi";
                std::filesystem::create_directories(directory);
                { std::ofstream file(directory / "Multi.nsproj"); file << "<Project><PropertyGroup><Name>Multi</Name></PropertyGroup></Project>"; }
                { std::ofstream file(directory / "Types.ns"); file << "namespace game { struct Node {} }\n"; }
                { std::ofstream file(directory / "Main.ns"); file << "using N = game.Node; int Use(N* item) { return 0; } int Main() { return 0; }\n"; }
                nativesharp::frontend::ProjectCompilation multi;
                std::string multiError;
                const bool parsedMulti = multi.compileProjectSyntax(directory / "Multi.nsproj", multiError);
                std::vector<std::unique_ptr<nativesharp::frontend::ProjectSyntaxUnit>> copies;
                nativesharp::diagnostics::DiagnosticEngine copyDiagnostics;
                const bool copied = parsedMulti && multi.cloneSyntaxForObservation(copies, copyDiagnostics, multiError);
                bool identitiesMatch = copied && copies.size() == 2 && multi.units().size() == copies.size();
                for (std::size_t i = 0; identitiesMatch && i < copies.size(); ++i)
                    identitiesMatch &= copies[i]->projectOrdinal() == multi.units()[i]->projectOrdinal() &&
                        copies[i]->sourceOrdinal() == multi.units()[i]->sourceOrdinal() &&
                        copies[i]->fileId() == multi.units()[i]->fileId() &&
                        copies[i]->moduleName() == multi.units()[i]->moduleName() &&
                        copies[i]->syntaxTree() != multi.units()[i]->syntaxTree();
                const nativesharp::ast::TypeSyntax* originalMultiAlias = nullptr;
                std::vector<const nativesharp::ast::AstNode*> multiPending;
                std::unordered_set<const nativesharp::ast::AstNode*> multiVisited;
                for (const auto& unit : multi.units())
                    if (unit && unit->syntaxTree()) multiPending.push_back(unit->syntaxTree());
                while (!multiPending.empty())
                {
                    const auto* node = multiPending.back(); multiPending.pop_back();
                    if (!node || !multiVisited.insert(node).second) continue;
                    if (node->typeSyntax() && node->typeSyntax()->baseName() == "N") originalMultiAlias = node->typeSyntax();
                    multiPending.push_back(node->first()); multiPending.push_back(node->second()); multiPending.push_back(node->third());
                    for (const auto* child : node->children()) multiPending.push_back(child);
                }
                nativesharp::frontend::ProjectNameLookup copyLookup;
                const bool copyReady = copied && copyLookup.build(*multi.projectGraph(), copies, copyDiagnostics);
                bool copiedBinding = false;
                for (const auto& binding : copyLookup.bindings())
                    copiedBinding |= binding.syntax && binding.syntax->baseName() == "game.Node" && binding.targetTypeOrdinal.has_value();
                const bool originalUnchanged = originalMultiAlias && originalMultiAlias->baseName() == "N";
                nativesharp::frontend::ProjectNameLookup originalLookup;
                const bool originalReady = copyReady && originalLookup.build(*multi.projectGraph(), multi.units(), multi.diagnostics());
                bool originalBinding = false;
                for (const auto& binding : originalLookup.bindings())
                    originalBinding |= binding.syntax && binding.syntax->baseName() == "game.Node" && binding.targetTypeOrdinal.has_value();
                bool nativeWithoutCppOrigin = false;
                for (std::size_t i = 0; i < originalLookup.namedTypes().size(); ++i)
                    if (originalLookup.namedTypes()[i].identity.name() == "Node")
                        nativeWithoutCppOrigin = !originalLookup.cppImportedTypeIndex(i);
                if (!(identitiesMatch && copyReady && copiedBinding && originalReady && originalBinding))
                    std::cerr << "multi observation: " << multiError << '\n';
                ok &= expect(identitiesMatch && copyReady && copiedBinding && originalUnchanged && originalReady && originalBinding && nativeWithoutCppOrigin &&
                             originalMultiAlias->baseName() == "game.Node" &&
                             !copyDiagnostics.hasErrors() && !multi.diagnostics().hasErrors(),
                             "isolated observation preserves cross-file type resolution and source identities");
            }
            {
                for (bool shadowed : {false, true})
                {
                    nativesharp::frontend::ProjectCompilation receiverCompilation;
                    receiverCompilation.setSourceBufferProvider([shadowed](const std::filesystem::path& file)
                        -> std::optional<std::string> {
                        if (file.filename() != "Main.ns") return std::nullopt;
                        return shadowed
                            ? "using A.B; class Holder { public int value; } int Main() { Holder C; return C.value; }\n"
                            : "using A.B; int Main() { return C.value; }\n";
                    });
                    std::string receiverError;
                    const bool receiverParsed = receiverCompilation.compileProjectSyntax(path, receiverError);
                    std::vector<std::unique_ptr<nativesharp::frontend::ProjectSyntaxUnit>> receiverCopies;
                    nativesharp::diagnostics::DiagnosticEngine receiverDiagnostics;
                    const bool receiverCopied = receiverParsed && receiverCompilation.cloneSyntaxForObservation(
                        receiverCopies, receiverDiagnostics, receiverError);
                    nativesharp::frontend::ProjectNameLookup receiverLookup;
                    const bool receiverReady = receiverCopied && receiverLookup.setImportedNamespaces({{0, "A.B"}}) &&
                        receiverLookup.setCppImportedTypes({{*namespaceIdentity,
                            nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                            nativesharp::frontend::NamedTypeAccessibility::Public, 0,
                            namespaceC->location ? namespaceC->location->file.generic_string() : ""}}) &&
                        receiverLookup.build(*receiverCompilation.projectGraph(), receiverCopies, receiverDiagnostics);
                    const auto receiverOrdinal = receiverReady ? receiverLookup.typeOrdinalForIdentity(namespaceIdentity->key()) : std::nullopt;
                    bool candidate = false;
                    if (receiverOrdinal)
                        for (const auto& binding : receiverLookup.expressionTypeBindings())
                            if (binding.writtenName == "C")
                                candidate |= std::find(binding.candidateTypeOrdinals.begin(), binding.candidateTypeOrdinals.end(),
                                                       *receiverOrdinal) != binding.candidateTypeOrdinals.end();
                    ok &= expect(receiverReady && candidate && !receiverDiagnostics.hasErrors(),
                                 "expression type lookup retains a C++ candidate even when a local may shadow it");
                    const auto receiverSelected = selectCppRecordBindings(receiverLookup, selectionGroups, mapping);
                    const auto* receiverSelection = std::get_if<CppRecordBindingSelection>(&receiverSelected);
                    ok &= expect(receiverSelection && receiverSelection->referencedGroups.empty() &&
                                 receiverSelection->expressionCandidateGroups == mapping,
                                 "expression record candidates remain separate from confirmed type references");
                }
                for (bool shadowed : {false, true})
                {
                    nativesharp::frontend::ProjectCompilation bodyCompilation;
                    bodyCompilation.setSourceBufferProvider([shadowed](const std::filesystem::path& file)
                        -> std::optional<std::string> {
                        if (file.filename() != "Main.ns") return std::nullopt;
                        return shadowed
                            ? "class C { public static int Get() { return 1; } } class Holder { public int Get() { return 2; } } class Program { static int Main() { Holder C = new Holder(); return C.Get(); } }\n"
                            : "class C { public static int Get() { return 1; } } class Program { static int Main() { return C.Get(); } }\n";
                    });
                    std::string bodyError;
                    const bool bodyReady = bodyCompilation.compileProjectMethodBodies(path, bodyError);
                    bool local = false, type = false;
                    if (bodyReady && bodyCompilation.methodBodyModel())
                        for (const auto& binding : bodyCompilation.methodBodyModel()->nameBindings())
                            if (binding.name == "C")
                            {
                                local |= binding.kind == nativesharp::frontend::MethodNameBindingKind::Local;
                                type |= binding.kind == nativesharp::frontend::MethodNameBindingKind::TypeReference;
                            }
                    if (!bodyReady) std::cerr << "receiver body: " << bodyError << '\n';
                    ok &= expect(bodyReady && (shadowed ? local && !type : type && !local),
                                 "body analysis distinguishes a local receiver from a static type receiver");
                }
            }
            {
                nativesharp::frontend::ProjectCompilation aliasCompilation;
                aliasCompilation.setSourceBufferProvider([](const std::filesystem::path& source)
                    -> std::optional<std::string> {
                    if (source.filename() == "Main.ns")
                        return "using Alias = A.B.C; int Use(Alias* item) { return 0; } int Main() { return 0; }\n";
                    return std::nullopt;
                });
                std::string aliasError;
                const bool parsedAlias = aliasCompilation.compileProjectSyntax(path, aliasError);
                const nativesharp::ast::TypeSyntax* originalAlias = nullptr;
                std::vector<const nativesharp::ast::AstNode*> pending;
                std::unordered_set<const nativesharp::ast::AstNode*> visited;
                for (const auto& unit : aliasCompilation.units())
                    if (unit && unit->syntaxTree()) pending.push_back(unit->syntaxTree());
                while (!pending.empty())
                {
                    const auto* node = pending.back(); pending.pop_back();
                    if (!node || !visited.insert(node).second) continue;
                    if (node->typeSyntax() && node->typeSyntax()->baseName() == "Alias") originalAlias = node->typeSyntax();
                    pending.push_back(node->first()); pending.push_back(node->second()); pending.push_back(node->third());
                    for (const auto* child : node->children()) pending.push_back(child);
                }
                std::vector<std::unique_ptr<nativesharp::frontend::ProjectSyntaxUnit>> observerUnits;
                nativesharp::diagnostics::DiagnosticEngine cloneDiagnostics;
                const bool cloned = parsedAlias && aliasCompilation.cloneSyntaxForObservation(
                    observerUnits, cloneDiagnostics, aliasError);
                const auto* savedUnit = observerUnits.empty() ? nullptr : observerUnits[0].get();
                nativesharp::frontend::ProjectCompilation unprepared;
                std::string rejectedError;
                ok &= expect(cloned && savedUnit && !unprepared.cloneSyntaxForObservation(
                                 observerUnits, cloneDiagnostics, rejectedError) &&
                             !rejectedError.empty() && observerUnits[0].get() == savedUnit,
                             "syntax observation rejects an unparsed project without replacing existing output");
                nativesharp::diagnostics::DiagnosticEngine aliasDiagnostics;
                nativesharp::frontend::ProjectNameLookup aliasObserver;
                const bool aliasConfigured = cloned && aliasObserver.setImportedNamespaces({{0, "A.B"}}) &&
                    aliasObserver.setCppImportedTypes({{*namespaceIdentity,
                        nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                        nativesharp::frontend::NamedTypeAccessibility::Public, 0,
                        namespaceC->location ? namespaceC->location->file.generic_string() : ""}});
                const bool aliasReady = aliasConfigured && aliasObserver.build(
                    *aliasCompilation.projectGraph(), observerUnits, aliasDiagnostics);
                bool expanded = false;
                for (const auto& binding : aliasObserver.bindings())
                    expanded |= binding.syntax && binding.syntax->baseName() == "A.B.C";
                ok &= expect(aliasConfigured && aliasReady && expanded && originalAlias &&
                             originalAlias->baseName() == "Alias" && !aliasCompilation.diagnostics().hasErrors(),
                             "isolated syntax observation expands aliases without mutating the original tree");
                nativesharp::frontend::ProjectNameLookup finalLookup;
                const bool finalReady = finalLookup.setImportedNamespaces({{0, "A.B"}}) &&
                    finalLookup.setCppImportedTypes({{*namespaceIdentity,
                        nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                        nativesharp::frontend::NamedTypeAccessibility::Public, 0,
                        namespaceC->location ? namespaceC->location->file.generic_string() : ""}}) &&
                    finalLookup.build(*aliasCompilation.projectGraph(), aliasCompilation.units(), aliasCompilation.diagnostics());
                const auto finalOrdinal = finalReady ? finalLookup.typeOrdinalForIdentity(namespaceIdentity->key()) : std::nullopt;
                bool finalBound = false;
                if (finalOrdinal)
                    for (const auto& binding : finalLookup.bindings())
                        finalBound |= binding.targetTypeOrdinal == finalOrdinal;
                ok &= expect(finalReady && finalBound && originalAlias && originalAlias->baseName() == "A.B.C",
                             "production syntax resolves the alias normally after isolated observation");
            }
        }
    }
    {
        auto incompleteSession = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
            "namespace Missing { struct C; C* fetch(); }\n"));
        const auto records = incompleteSession ? incompleteSession->recordsSnapshot() : std::nullopt;
        const auto functions = incompleteSession ? incompleteSession->freeFunctionsSnapshot() : std::nullopt;
        const auto* record = records ? recordByName(*records, "Missing::C") : nullptr;
        const auto* getter = functions ? functionByName(*functions, "Missing::fetch") : nullptr;
        const auto identity = record ? projectCppRecordIdentity(*record, "Semantic") : std::nullopt;
        const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
        if (record && getter && identity)
        {
            const std::vector<CppSemanticRecordEntry> entries{{
                path, 0, 0, "x86_64-pc-windows-msvc", *record, identity}};
            const auto groups = groupCppSemanticRecords(entries);
            const auto prepared = prepareCppRecordObservation(entries, groups);
            const auto* inputs = std::get_if<CppRecordObservationInputs>(&prepared);
            const auto projection = projectCppSemanticFunctionWithRecords(*getter, 0, "Semantic", groups);
            const auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projection);
            ok &= expect(inputs && inputs->types.size() == 1 && !inputs->types[0].objectLayout && symbol,
                         "forward-declared C++ record has no object dimensions but its pointer can be projected");
            if (inputs && symbol)
                for (int scenario = 0; scenario < 7; ++scenario)
                {
                    nativesharp::frontend::ProjectCompilation compilation;
                    compilation.setSourceBufferProvider([scenario](const std::filesystem::path& source)
                        -> std::optional<std::string> {
                        if (source.filename() != "Main.ns") return std::nullopt;
                        switch (scenario)
                        {
                            case 0: return "int Main() { Missing.C* p = Missing.fetch(); return p == null ? 0 : 1; }\n";
                            case 1: return "int Main() { return (int)sizeof(Missing.C); }\n";
                            case 2: return "int Main() { return (int)alignof(Missing.C); }\n";
                            case 3: return "int Main() { Missing.C* p = Missing.fetch(); p = p + 1; return 0; }\n";
                            case 4: return "int Main() { Missing.C* p = Missing.fetch(); p += 1; return 0; }\n";
                            case 5: return "int Main() { Missing.C* p = Missing.fetch(); Missing.C* q = &p[1]; return q == null ? 0 : 1; }\n";
                            default: return "int Main() { Missing.C value; return 0; }\n";
                        }
                    });
                    std::string error;
                    const bool configured = compilation.setExperimentalCppImportedTypes(inputs->types, error) &&
                        compilation.setCppImportedFunctions({*symbol}, error);
                    const bool ready = configured && compilation.compileProjectOopMir(path, error);
                    bool layoutError = error.find("no value layout") != std::string::npos ||
                        error.find("cannot determine the element layout") != std::string::npos ||
                        error.find("cannot determine raw element layout") != std::string::npos;
                    bool localError = false;
                    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
                    {
                        layoutError |= diagnostic.message().find("no value layout") != std::string::npos ||
                            diagnostic.message().find("cannot determine the element layout") != std::string::npos ||
                            diagnostic.message().find("cannot determine raw element layout") != std::string::npos ||
                            diagnostic.message().find("is incomplete and has no object layout") != std::string::npos;
                        localError |= diagnostic.message().find("cannot be created by value until object lifetime is implemented") != std::string::npos;
                    }
                    const bool rejectedAsExpected = !ready && (scenario == 6 ? localError : layoutError);
                    if (!(configured && (scenario == 0 ? ready : rejectedAsExpected)))
                    {
                        std::cerr << "incomplete record scenario " << scenario << " ready=" << ready << " error=" << error << '\n';
                        for (const auto& diagnostic : compilation.diagnostics().diagnostics())
                            std::cerr << diagnostic.message() << '\n';
                    }
                    ok &= expect(configured && (scenario == 0 ? ready : rejectedAsExpected),
                                 "incomplete record allows pointer transport but rejects layout-dependent operations");
                }
        }
        else ok &= expect(false, "Clang provides the forward-declared record fixture");
    }
    auto cpp = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
                                    "class MyClass { int hidden; int hiddenMethod(); static int secret; static int HiddenStatic() noexcept; public: int count; static int shared; static const int limit; static volatile int io; static thread_local int tls; MyClass(int); "
                                    "~MyClass(); int A(int) const noexcept; static int B(double) noexcept; virtual int C(); "
                                    "int Q() &; int Q() &&; int Volatile() volatile; "
                                    "int Safe() noexcept; int Throwing() noexcept(false); int Unknown(); "
                                    "static int ThrowStatic() noexcept(false); static int VarStatic(int, ...) noexcept; "
                                    "static int __vectorcall V(int) noexcept; };\n"
                                    "struct VBase { virtual ~VBase(); }; "
                                    "struct Derived : virtual VBase { ~Derived(); };\n"
                                    "struct BaseA { int a; }; struct BaseB { int b; }; "
                                    "struct DeletedLifetime { int& ref; }; "
                                    "struct ThrowingMember { ThrowingMember() noexcept(false); ~ThrowingMember() noexcept(false); }; "
                                    "struct ImplicitThrowing { ThrowingMember member; }; "
                                    "struct UsedLifetime { int value; }; inline UsedLifetime useLifetime() { "
                                    "UsedLifetime a; UsedLifetime b(a); UsedLifetime c(static_cast<UsedLifetime&&>(a)); return b; } "
                                    "struct PlainLifetime { PlainLifetime(int) noexcept; ~PlainLifetime() noexcept; }; "
                                    "class LockedLifetime { LockedLifetime() noexcept; ~LockedLifetime() noexcept; }; "
                                    "struct BasedLifetime : BaseA { BasedLifetime() noexcept; ~BasedLifetime() noexcept; }; "
                                    "__declspec(dllimport) int dll_value; __declspec(dllimport) int dll_function(int); "
                                    "class __declspec(dllimport) ImportedClass { public: static int data; int method(int) noexcept; }; "
                                    "struct QualifiedFields { const int c; volatile int v; unsigned bits:3; }; "
                                    "struct Defaults { void invoke(int a, int b = 3); }; int defaults(int a, int b = 2); "
                                    "\n#pragma pack(push, 1)\nstruct PackedField { char prefix; int data; };\n#pragma pack(pop)\n"
                                    "class Multi : public BaseA, private BaseB {};\n"
                                    "class Outer { public: struct Inner; struct Inner { int value; }; "
                                    "private: class Secret { int value; }; };\n"
                                    "extern \"C\" int c_api(int); extern \"C\" int c_api_noexcept(int) noexcept;\n"
                                    "int cpp_noexcept(int) noexcept; int cpp_throwing(int) noexcept(false); int __vectorcall vector_fn(int); int cpp_var(int, ...);\n"
                                    "namespace root { namespace sub { int foo(int); } }\n"
                                    "namespace tools { extern int cpp_global; extern volatile int cpp_volatile; "
                                    "extern const int cpp_constant; extern const volatile int cpp_const_volatile; static int cpp_internal; "
                                    "extern int* cpp_pointer; extern int** cpp_double_pointer; extern void* cpp_void_pointer; extern int* const cpp_const_pointer; "
                                    "extern int* volatile cpp_volatile_pointer; extern const int* cpp_readonly_pointee; "
                                    "extern volatile int* cpp_volatile_pointee; extern int* const* cpp_nested_const; "
                                    "extern thread_local int tls_value; }\n"
                                    "namespace tools { int blend(int); double blend(double); int* pointer_identity(int* p); "
                                    "int cpp_volatile_argument(volatile int*); volatile int* cpp_volatile_result(); "
                                    "int qualify(int*); int qualify(const int*); "
                                    "static int local(int); const int* pointer(const int* p); "
                                    "struct Opaque; void handle(Opaque* p); long widths(long long v); }\n"
                                    "namespace scalars { void v(); bool b(); signed char i8(); unsigned char u8(); "
                                    "short i16(); unsigned short u16(); unsigned int u32(); "
                                    "unsigned long long u64(); float f32(); double f64(); }\n"));
    ok &= expect(static_cast<bool>(cpp), "valid C++ translation unit opens");
    if (cpp)
    {
        ok &= expect(cpp->topLevelDeclarationCount().value_or(0) > 0, "C++ AST remains available");
        auto functions = cpp->freeFunctionsSnapshot();
        for (const auto* name : {"tools::cpp_volatile_argument", "tools::cpp_volatile_result"})
        {
            const auto* declaration = functions ? functionByName(*functions, name) : nullptr;
            ok &= expect(declaration && std::holds_alternative<CppSemanticFunctionFailure>(
                             projectCppSemanticFunction(*declaration, 0, "Semantic")),
                         "C++ volatile pointer signatures remain deferred");
        }
        const auto* cppVar = functions ? functionByName(*functions, "cpp_var") : nullptr;
        ok &= expect(cppVar && std::holds_alternative<CppSemanticFunctionFailure>(
                         projectCppSemanticFunction(*cppVar, 0, "Semantic")),
                     "C++ variadic function remains deferred until its semantics are verified");
        auto records = cpp->recordsSnapshot();
        const auto* defaultFunction = functions ? functionByName(*functions, "defaults") : nullptr;
        const auto* defaultRecord = records ? recordByName(*records, "Defaults") : nullptr;
        bool defaultMethod = false;
        if (defaultRecord)
            for (const auto& method : defaultRecord->methods)
                if (method.name == "invoke" && method.parameters.size() == 2)
                    defaultMethod = !method.parameters[0].hasDefaultArgument && method.parameters[1].hasDefaultArgument;
        ok &= expect(defaultFunction && defaultFunction->parameters.size() == 2 &&
                     !defaultFunction->parameters[0].hasDefaultArgument && defaultFunction->parameters[1].hasDefaultArgument &&
                     defaultMethod, "Clang snapshots retain which function and method arguments have defaults");
        auto globals = cpp->globalsSnapshot();
        const auto* cppGlobal = globals ? globalByName(*globals, "tools::cpp_global") : nullptr;
        const auto* cppVolatile = globals ? globalByName(*globals, "tools::cpp_volatile") : nullptr;
        const auto* cppConstant = globals ? globalByName(*globals, "tools::cpp_constant") : nullptr;
        const auto* cppConstVolatile = globals ? globalByName(*globals, "tools::cpp_const_volatile") : nullptr;
        if (globals)
        {
            const auto checkPointer = [&](const char* name, const char* key, bool readonly, bool volatilePointer) {
                const auto* declaration = globalByName(*globals, name);
                if (!declaration) return false;
                auto projection = projectCppSemanticGlobal(*declaration, 0, "Semantic");
                const auto* symbol = std::get_if<CppSemanticGlobal>(&projection);
                return symbol && symbol->typeKey == key && symbol->isConst == readonly &&
                    symbol->isVolatile == volatilePointer;
            };
            ok &= expect(checkPointer("tools::cpp_pointer", "builtin:int*", false, false) &&
                         checkPointer("tools::cpp_double_pointer", "builtin:int**", false, false) &&
                         checkPointer("tools::cpp_void_pointer", "builtin:void*", false, false) &&
                         checkPointer("tools::cpp_const_pointer", "builtin:int*", true, false) &&
                         checkPointer("tools::cpp_volatile_pointer", "builtin:int*", false, true),
                         "external pointer globals preserve depth and top-level qualifiers");
            for (const auto* name : {"tools::cpp_volatile_pointee", "tools::cpp_nested_const"})
            {
                const auto* declaration = globalByName(*globals, name);
                ok &= expect(declaration && std::holds_alternative<CppSemanticGlobalFailure>(
                                 projectCppSemanticGlobal(*declaration, 0, "Semantic")),
                             "external pointer globals do not discard nested qualifiers");
            }
            const auto* readonlyDecl = globalByName(*globals, "tools::cpp_readonly_pointee");
            if (readonlyDecl)
            {
                auto projection = projectCppSemanticGlobal(*readonlyDecl, 0, "Semantic");
                const auto* symbol = std::get_if<CppSemanticGlobal>(&projection);
                ok &= expect(symbol && symbol->typeKey == "builtin:int*" && symbol->pointerPointeeReadonly &&
                             !symbol->isConst, "pointer global retains pointee const independently of pointer const");
                if (symbol)
                for (int scenario = 0; scenario < 5; ++scenario)
                {
                    nativesharp::frontend::NamespaceStateSymbol state;
                    state.projectOrdinal = symbol->projectOrdinal;
                    state.moduleName = symbol->moduleName;
                    state.namespaceName = symbol->namespaceName;
                    state.name = symbol->name;
                    state.declaredTypeKey = symbol->typeKey;
                    state.storageName = symbol->externalSymbol;
                    state.isCppExternal = true;
                    state.cppPointerPointeeReadonly = true;
                    state.accessibility = nativesharp::frontend::NamespaceFunctionAccessibility::Public;
                    nativesharp::frontend::ProjectCompilation check;
                    check.setSourceBufferProvider([scenario](const std::filesystem::path& path)
                        -> std::optional<std::string> {
                        if (path.filename() != "Main.ns") return std::nullopt;
                        switch (scenario)
                        {
                            case 0: return "int Main() { int const* p = tools.cpp_readonly_pointee; return *p; }\n";
                            case 1: return "int Main() { *tools.cpp_readonly_pointee = 1; return 0; }\n";
                            case 2: return "int Main() { int* p = tools.cpp_readonly_pointee; return 0; }\n";
                            case 3: return "int Main() { int** p = &tools.cpp_readonly_pointee; return 0; }\n";
                            default: return "int Main() { tools.cpp_readonly_pointee = null; return 0; }\n";
                        }
                    });
                    std::string error;
                    const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                    const bool configured = check.setExperimentalCppImportedStates({state}, error);
                    const bool ready = configured && check.compileProjectOopMir(path, error);
                    const char* expected = scenario == 1 ? "left side of assignment is not assignable" :
                        scenario == 2 ? "would discard pointee constness" :
                        "cannot take the address of a qualified C/C++ global";
                    bool expectedDiagnostic = false;
                    for (const auto& diagnostic : check.diagnostics().diagnostics())
                        expectedDiagnostic |= diagnostic.message().find(expected) != std::string::npos;
                    if (!configured || ((scenario == 0 || scenario == 4) ? !ready : ready || !expectedDiagnostic))
                    {
                        std::cerr << "readonly pointee scenario " << scenario << ": " << error << '\n';
                        for (const auto& diagnostic : check.diagnostics().diagnostics())
                            std::cerr << diagnostic.message() << '\n';
                    }
                    ok &= expect(configured && ((scenario == 0 || scenario == 4) ? ready : !ready && expectedDiagnostic),
                                 "readonly pointee preserves read, reassignment and qualifier safety");
                }
            }
            else ok &= expect(false, "Clang exposes pointer to readonly scalar");
            const auto* volatilePointee = globalByName(*globals, "tools::cpp_volatile_pointee");
            if (volatilePointee && volatilePointee->externalSymbol)
            for (int scenario = 0; scenario < 5; ++scenario)
            {
                nativesharp::frontend::NamespaceStateSymbol state;
                state.projectOrdinal = 0;
                state.moduleName = "Semantic";
                state.namespaceName = "tools";
                state.name = "cpp_volatile_pointee";
                state.declaredTypeKey = "builtin:int*";
                state.storageName = *volatilePointee->externalSymbol;
                state.isCppExternal = true;
                state.cppPointerPointeeVolatile = true;
                state.accessibility = nativesharp::frontend::NamespaceFunctionAccessibility::Public;
                nativesharp::frontend::ProjectCompilation check;
                check.setSourceBufferProvider([scenario](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() != "Main.ns") return std::nullopt;
                    if (scenario == 0)
                        return "int Main() { var p = tools.cpp_volatile_pointee; *p = 42; return *p + p[0]; }\n";
                    if (scenario == 1)
                        return "int Main() { int* p = tools.cpp_volatile_pointee; return 0; }\n";
                    if (scenario == 2)
                        return "int Main() { int value = 0; int* p = &value; p = tools.cpp_volatile_pointee; return 0; }\n";
                    if (scenario == 4)
                        return "int Main() { var p = tools.cpp_volatile_pointee; var q = move(p); *q = 42; return *q; }\n";
                    return "int* result() { return tools.cpp_volatile_pointee; } int Main() { return 0; }\n";
                });
                std::string error;
                const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                const bool configured = check.setExperimentalCppImportedStates({state}, error);
                const bool ready = configured && (scenario == 0 || scenario == 4
                    ? check.compileProjectOopMir(path, error) : check.compileProjectMethodBodies(path, error));
                bool expectedDiagnostic = false;
                for (const auto& diagnostic : check.diagnostics().diagnostics())
                    expectedDiagnostic |= diagnostic.message().find("volatile qualification") != std::string::npos;
                ok &= expect(configured && (scenario == 0 || scenario == 4 ? ready : !ready && expectedDiagnostic),
                             "volatile pointee global preserves access and rejects qualifier loss");
                if ((scenario == 0 || scenario == 4) && ready && check.oopMirModel() && check.nameLookup())
                {
                    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                    auto module = lowerer.lower(*check.nameLookup(), *check.oopMirModel());
                    const bool valid = module && module->verify(error);
                    const auto ir = module ? module->text() : std::string{};
                    ok &= expect(valid && ir.find("load volatile i32") != std::string::npos &&
                                 ir.find("store volatile i32") != std::string::npos,
                                 "injected volatile pointee emits volatile load and store");
                }
            }
            else ok &= expect(false, "Clang exposes the volatile pointee fixture");
            std::vector<nativesharp::frontend::NamespaceStateSymbol> pointerStates;
            for (const auto* name : {"tools::cpp_const_pointer", "tools::cpp_volatile_pointer"})
            {
                const auto* declaration = globalByName(*globals, name);
                if (!declaration) continue;
                auto projection = projectCppSemanticGlobal(*declaration, 0, "Semantic");
                const auto* symbol = std::get_if<CppSemanticGlobal>(&projection);
                if (!symbol) continue;
                nativesharp::frontend::NamespaceStateSymbol state;
                state.projectOrdinal = symbol->projectOrdinal;
                state.moduleName = symbol->moduleName;
                state.namespaceName = symbol->namespaceName;
                state.name = symbol->name;
                state.declaredTypeKey = symbol->typeKey;
                state.storageName = symbol->externalSymbol;
                state.isCppExternal = true;
                state.isCppReadonly = symbol->isConst;
                state.isVolatile = symbol->isVolatile;
                state.accessibility = nativesharp::frontend::NamespaceFunctionAccessibility::Public;
                pointerStates.push_back(std::move(state));
            }
            nativesharp::frontend::ProjectCompilation pointerCompilation;
            pointerCompilation.setSourceBufferProvider([](const std::filesystem::path& path)
                -> std::optional<std::string> {
                if (path.filename() == "Main.ns")
                    return "int Main() { tools.cpp_volatile_pointer = tools.cpp_const_pointer; return *tools.cpp_volatile_pointer; }\n";
                return std::nullopt;
            });
            std::string pointerError;
            const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
            const bool ready = pointerStates.size() == 2 &&
                pointerCompilation.setExperimentalCppImportedStates(std::move(pointerStates), pointerError) &&
                pointerCompilation.compileProjectOopMir(path, pointerError);
            ok &= expect(ready, "qualified pointer globals reach MIR");
            if (ready && pointerCompilation.oopMirModel() && pointerCompilation.nameLookup())
            {
                nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                auto module = lowerer.lower(*pointerCompilation.nameLookup(), *pointerCompilation.oopMirModel());
                const bool valid = module && module->verify(pointerError);
                const auto ir = module ? module->text() : std::string{};
                ok &= expect(valid && ir.find("load volatile ptr") != std::string::npos &&
                             ir.find("store volatile ptr") != std::string::npos,
                             "volatile pointer global emits volatile pointer loads and stores");
            }
        }
        const auto* cppInternal = globals ? globalByName(*globals, "tools::cpp_internal") : nullptr;
        const auto* tlsValue = globals ? globalByName(*globals, "tools::tls_value") : nullptr;
        ok &= expect(cppGlobal && cppGlobal->externalSymbol == "?cpp_global@tools@@3HA" &&
                     cppGlobal->type.sizeBits == 32 && !cppGlobal->threadLocal,
                     "C++ global has the expected target symbol and type");
        if (cppVolatile)
        {
            auto projected = projectCppSemanticGlobal(*cppVolatile, 0, "Semantic");
            const auto* symbol = std::get_if<CppSemanticGlobal>(&projected);
            ok &= expect(symbol && symbol->isVolatile, "C++ global retains Clang volatile qualification");
            if (symbol)
            for (int scenario = 0; scenario < 3; ++scenario)
            {
                nativesharp::frontend::NamespaceStateSymbol state;
                state.projectOrdinal = symbol->projectOrdinal;
                state.moduleName = symbol->moduleName;
                state.namespaceName = symbol->namespaceName;
                state.name = symbol->name;
                state.declaredTypeKey = symbol->typeKey;
                state.storageName = symbol->externalSymbol;
                state.isCppExternal = true;
                state.isVolatile = symbol->isVolatile;
                state.accessibility = nativesharp::frontend::NamespaceFunctionAccessibility::Public;
                nativesharp::frontend::ProjectCompilation compilation;
                compilation.setSourceBufferProvider([scenario](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() != "Main.ns") return std::nullopt;
                    if (scenario == 0)
                        return "int Main() { tools.cpp_volatile = 5; tools.cpp_volatile += 2; return tools.cpp_volatile; }\n";
                    if (scenario == 1) return "int Main() { int* p = &tools.cpp_volatile; return 0; }\n";
                    return "int Main() { ref int alias = ref tools.cpp_volatile; return alias; }\n";
                });
                std::string error;
                const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                const bool configured = compilation.setExperimentalCppImportedStates({state}, error);
                const bool ready = configured && compilation.compileProjectOopMir(path, error);
                bool addressError = false;
                for (const auto& diagnostic : compilation.diagnostics().diagnostics())
                    addressError |= diagnostic.message().find(scenario == 1
                        ? "cannot take the address of a qualified C/C++ global"
                        : "reference-local initializer must be a mutable addressable storage location") != std::string::npos;
                ok &= expect(configured && (scenario == 0 ? ready : !ready && addressError),
                             "volatile globals allow direct operations and reject an unqualified address");
                if (scenario == 0 && ready && compilation.oopMirModel() && compilation.nameLookup())
                {
                    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
                    const bool valid = module && module->verify(error);
                    const auto ir = module ? module->text() : std::string{};
                    const auto firstLoad = ir.find("load volatile i32");
                    const auto firstStore = ir.find("store volatile i32");
                    ok &= expect(valid && firstLoad != std::string::npos && firstStore != std::string::npos &&
                                 ir.find("load volatile i32", firstLoad + 1) != std::string::npos &&
                                 ir.find("store volatile i32", firstStore + 1) != std::string::npos,
                                 "direct and compound global accesses reach LLVM as volatile operations");
                }
            }
        }
        else ok &= expect(false, "Clang exposes the volatile global");
        if (cppConstant && cppConstVolatile)
        {
            auto first = projectCppSemanticGlobal(*cppConstant, 0, "Semantic");
            auto second = projectCppSemanticGlobal(*cppConstVolatile, 0, "Semantic");
            const auto* constant = std::get_if<CppSemanticGlobal>(&first);
            const auto* constVolatile = std::get_if<CppSemanticGlobal>(&second);
            ok &= expect(constant && constant->isConst && !constant->isVolatile && constVolatile &&
                         constVolatile->isConst && constVolatile->isVolatile,
                         "Clang const and const volatile qualifiers survive projection");
            if (constant && constVolatile)
            for (int scenario = 0; scenario < 4; ++scenario)
            {
                std::vector<nativesharp::frontend::NamespaceStateSymbol> states;
                for (const auto* symbol : {constant, constVolatile})
                {
                    nativesharp::frontend::NamespaceStateSymbol state;
                    state.projectOrdinal = symbol->projectOrdinal;
                    state.moduleName = symbol->moduleName;
                    state.namespaceName = symbol->namespaceName;
                    state.name = symbol->name;
                    state.declaredTypeKey = symbol->typeKey;
                    state.storageName = symbol->externalSymbol;
                    state.isCppExternal = true;
                    state.isCppReadonly = symbol->isConst;
                    state.isVolatile = symbol->isVolatile;
                    state.accessibility = nativesharp::frontend::NamespaceFunctionAccessibility::Public;
                    states.push_back(std::move(state));
                }
                nativesharp::frontend::ProjectCompilation compilation;
                compilation.setSourceBufferProvider([scenario](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() != "Main.ns") return std::nullopt;
                    switch (scenario)
                    {
                        case 0: return "int Main() { return tools.cpp_constant + tools.cpp_const_volatile; }\n";
                        case 1: return "int Main() { tools.cpp_constant = 1; return 0; }\n";
                        case 2: return "int Main() { int* p = &tools.cpp_constant; return 0; }\n";
                        default: return "int Main() { ref int p = ref tools.cpp_constant; return p; }\n";
                    }
                });
                std::string error;
                const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                const bool configured = compilation.setExperimentalCppImportedStates(std::move(states), error);
                const bool ready = configured && compilation.compileProjectOopMir(path, error);
                bool expectedDiagnostic = false;
                const char* expected = scenario == 1 ? "left side of assignment is not assignable" :
                    scenario == 2 ? "cannot take the address of a qualified C/C++ global" :
                    "reference-local initializer must be a mutable addressable storage location";
                for (const auto& diagnostic : compilation.diagnostics().diagnostics())
                    expectedDiagnostic |= diagnostic.message().find(expected) != std::string::npos;
                if (!configured || (scenario == 0 ? !ready : ready || !expectedDiagnostic))
                {
                    std::cerr << "const global scenario " << scenario << ": " << error << '\n';
                    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
                        std::cerr << diagnostic.message() << '\n';
                }
                ok &= expect(configured && (scenario == 0 ? ready : !ready && expectedDiagnostic),
                             "const global reads succeed and mutable uses are rejected");
                if (scenario == 0 && ready && compilation.oopMirModel() && compilation.nameLookup())
                {
                    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
                    const bool valid = module && module->verify(error);
                    const auto ir = module ? module->text() : std::string{};
                    ok &= expect(valid && ir.find("@\"" + constant->externalSymbol + "\" = external global i32") != std::string::npos &&
                                 ir.find("@\"" + constVolatile->externalSymbol + "\" = external global i32") != std::string::npos &&
                                 ir.find("load volatile i32") != std::string::npos,
                                 "const globals use external data and const volatile reads remain volatile");
                }
            }
        }
        else ok &= expect(false, "Clang exposes const global fixtures");
        if (cppGlobal)
        {
            auto projected = projectCppSemanticGlobal(*cppGlobal, 0, "Semantic");
            const auto* symbol = std::get_if<CppSemanticGlobal>(&projected);
            if (symbol)
            {
                nativesharp::frontend::NamespaceStateSymbol state;
                state.projectOrdinal = symbol->projectOrdinal;
                state.moduleName = symbol->moduleName;
                state.namespaceName = symbol->namespaceName;
                state.name = symbol->name;
                state.declaredTypeKey = symbol->typeKey;
                state.storageName = symbol->externalSymbol;
                state.isCppExternal = true;
                state.accessibility = nativesharp::frontend::NamespaceFunctionAccessibility::Public;
                for (int scenario = 0; scenario < 4; ++scenario)
                {
                    auto deferred = state;
                    deferred.namespaceName.clear();
                    deferred.storageName.clear();
                    deferred.cppImportError = "volatile C/C++ global is not yet projected";
                    nativesharp::frontend::ProjectCompilation check;
                    check.setSourceBufferProvider([scenario](const std::filesystem::path& path)
                        -> std::optional<std::string> {
                        if (path.filename() != "Main.ns") return std::nullopt;
                        switch (scenario)
                        {
                            case 0: return "int Main() { int cpp_global = 7; return cpp_global; }\n";
                            case 1: return "int Value(int cpp_global) { return cpp_global; } int Main() { return Value(7); }\n";
                            case 2: return "int Main() { return cpp_global; }\n";
                            default: return "int Main() { return 7; }\n";
                        }
                    });
                    std::string checkError;
                    const auto projectPath = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                    const bool configured = check.setExperimentalCppImportedStates({deferred}, checkError);
                    const bool ready = configured && check.compileProjectOopMir(projectPath, checkError);
                    bool deferredDiagnostic = false;
                    for (const auto& diagnostic : check.diagnostics().diagnostics())
                        deferredDiagnostic |= diagnostic.message().find(deferred.cppImportError) != std::string::npos;
                    if (!configured || (scenario == 2 ? ready || !deferredDiagnostic : !ready))
                    {
                        std::cerr << "deferred global scenario " << scenario << ": " << checkError << '\n';
                        for (const auto& diagnostic : check.diagnostics().diagnostics())
                            std::cerr << diagnostic.message() << '\n';
                    }
                    ok &= expect(configured && (scenario == 2 ? !ready && deferredDiagnostic : ready),
                                 "unsupported C++ global is diagnosed only when selected");
                }
                for (int scenario = 0; scenario < 3; ++scenario)
                {
                    auto imported = state;
                    imported.namespaceName = "Clash";
                    nativesharp::frontend::ProjectCompilation check;
                    check.setSourceBufferProvider([scenario](const std::filesystem::path& path)
                        -> std::optional<std::string> {
                        if (path.filename() != "Main.ns") return std::nullopt;
                        if (scenario == 0)
                            return "namespace Clash { public int cpp_global = 3; } int Main() { return 7; }\n";
                        if (scenario == 1)
                            return "namespace Clash { public int cpp_global = 3; int Run() { int cpp_global = 7; return cpp_global; } } int Main() { return Clash.Run(); }\n";
                        return "namespace Clash { public int cpp_global = 3; } int Main() { return Clash.cpp_global; }\n";
                    });
                    std::string checkError;
                    const auto projectPath = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                    const bool configured = check.setExperimentalCppImportedStates({imported}, checkError);
                    const bool ready = configured && check.compileProjectOopMir(projectPath, checkError);
                    bool ambiguity = false;
                    for (const auto& diagnostic : check.diagnostics().diagnostics())
                        ambiguity |= diagnostic.message().find("ambiguous namespace state reference") != std::string::npos;
                    if (!configured || (scenario == 2 ? ready || !ambiguity : !ready))
                    {
                        std::cerr << "global collision scenario " << scenario << ": " << checkError << '\n';
                        for (const auto& diagnostic : check.diagnostics().diagnostics())
                            std::cerr << diagnostic.message() << '\n';
                    }
                    ok &= expect(configured && (scenario == 2 ? !ready && ambiguity : ready),
                                 "C++ and NativeSharp global collision is diagnosed only when selected");
                }
                {
                    nativesharp::frontend::ProjectCompilation invalid;
                    std::string validationError;
                    auto malformed = state;
                    malformed.storageName.clear();
                    ok &= expect(!invalid.setExperimentalCppImportedStates({malformed}, validationError) &&
                                 !invalid.setExperimentalCppImportedStates({state, state}, validationError),
                                 "C++ globals reject incomplete and duplicate declarations");
                }
                nativesharp::frontend::ProjectCompilation compilation;
                compilation.setSourceBufferProvider([](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() == "Main.ns")
                        return "int Main() { tools.cpp_global = 41; return tools.cpp_global; }\n";
                    return std::nullopt;
                });
                std::string error;
                const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                const bool ready = compilation.setExperimentalCppImportedStates({state}, error) &&
                    compilation.compileProjectOopMir(path, error);
                if (!ready)
                {
                    std::cerr << "C++ global MIR: " << error << '\n';
                    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
                        std::cerr << diagnostic.message() << '\n';
                }
                ok &= expect(ready, "C++ global reads and writes reach MIR");
                ok &= expect(!compilation.setExperimentalCppImportedStates({}, error),
                             "C++ globals cannot change after compilation starts");
                if (ready && compilation.oopMirModel() && compilation.nameLookup())
                {
                    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
                    const bool valid = module && module->verify(error);
                    const auto ir = module ? module->text() : std::string{};
                    ok &= expect(valid && ir.find("@\"?cpp_global@tools@@3HA\" = external global i32") != std::string::npos &&
                                 ir.find("__nsc_get_?cpp_global") == std::string::npos,
                                 "C++ global is addressed directly by its Clang symbol");
                    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(error);
                    const bool emitted = valid && target && target->emit(*module,
                        std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/cpp-global-call.obj",
                        nativesharp::llvm_backend::LlvmCodeGenFileType::Object, error);
                    if (!emitted) std::cerr << "C++ global object: " << lowerer.error() << ' ' << error << '\n';
                    ok &= expect(emitted, "C++ global emits a Windows x64 object");
                }
            }
            else ok &= expect(false, "C++ global projects to NativeSharp scalar");
        }
        ok &= expect(cppInternal && !cppInternal->externalLinkage && !cppInternal->externalSymbol,
                     "internal C++ global does not promise an external symbol");
        ok &= expect(tlsValue && tlsValue->threadLocal && tlsValue->externalSymbol == "?tls_value@tools@@3HA",
                     "thread-local global retains its storage property and symbol");
        const auto* record = records ? recordByName(*records, "MyClass") : nullptr;
        const auto* dllRecord = records ? recordByName(*records, "ImportedClass") : nullptr;
        const auto* dllFunction = functions ? functionByName(*functions, "dll_function") : nullptr;
        const auto* dllGlobal = globals ? globalByName(*globals, "dll_value") : nullptr;
        bool dllStaticField = false, dllMethod = false;
        bool dllStaticProjected = false, dllMethodProjected = false;
        if (dllRecord)
        {
            for (std::size_t i = 0; i < dllRecord->staticFields.size(); ++i)
            {
                const auto& field = dllRecord->staticFields[i];
                if (field.name == "data")
                {
                    dllStaticField = field.dllImport;
                    const auto projected = projectCppSemanticStaticField(*dllRecord, i, 0, "Semantic", "x86_64-pc-windows-msvc");
                    dllStaticProjected = projected && projected->dllImport && toCppImportedStaticField(*projected).dllImport;
                }
            }
            for (std::size_t i = 0; i < dllRecord->methods.size(); ++i)
            {
                const auto& method = dllRecord->methods[i];
                if (method.name == "method")
                { dllMethod = method.dllImport; const auto projected = projectCppSemanticInstanceMethod(*dllRecord, i, 0, "Semantic", "x86_64-pc-windows-msvc");
                  const auto* value = std::get_if<CppSemanticInstanceMethod>(&projected);
                  dllMethodProjected = value && value->dllImport && toCppImportedInstanceMethod(*value).dllImport; }
            }
        }
        ok &= expect(dllFunction && dllFunction->dllImport && dllGlobal && dllGlobal->dllImport &&
                     dllStaticField && dllMethod && dllStaticProjected && dllMethodProjected &&
                     std::holds_alternative<nativesharp::frontend::NamespaceFunctionSymbol>(projectCppSemanticFunction(*dllFunction, 0, "Semantic")) &&
                     std::get<nativesharp::frontend::NamespaceFunctionSymbol>(projectCppSemanticFunction(*dllFunction, 0, "Semantic")).cppDllImport &&
                     std::holds_alternative<CppSemanticGlobal>(projectCppSemanticGlobal(*dllGlobal, 0, "Semantic")) &&
                     std::get<CppSemanticGlobal>(projectCppSemanticGlobal(*dllGlobal, 0, "Semantic")).dllImport,
                     "Clang snapshots retain dllimport and data projections preserve it");
        const auto* derived = records ? recordByName(*records, "Derived") : nullptr;
        const auto* multi = records ? recordByName(*records, "Multi") : nullptr;
        const auto* plainLifetime = records ? recordByName(*records, "PlainLifetime") : nullptr;
        const auto* implicitLifetime = records ? recordByName(*records, "BaseA") : nullptr;
        const auto* usedLifetime = records ? recordByName(*records, "UsedLifetime") : nullptr;
        const auto* lockedLifetime = records ? recordByName(*records, "LockedLifetime") : nullptr;
        const auto* basedLifetime = records ? recordByName(*records, "BasedLifetime") : nullptr;
        bool lifetimeCtor = false, lifetimeDtor = false, privateLifetimeDeferred = false, basedLifetimeDeferred = false;
        ok &= expect(implicitLifetime && implicitLifetime->implicitLifetimeMembers &&
                     implicitLifetime->implicitLifetimeMembers->defaultConstructor &&
                     implicitLifetime->implicitLifetimeMembers->copyConstructor &&
                     implicitLifetime->implicitLifetimeMembers->moveConstructor &&
                     implicitLifetime->implicitLifetimeMembers->destructor &&
                     usedLifetime && usedLifetime->implicitLifetimeMembers &&
                     usedLifetime->implicitLifetimeMembers->defaultConstructor &&
                     usedLifetime->implicitLifetimeMembers->copyConstructor &&
                     usedLifetime->implicitLifetimeMembers->moveConstructor &&
                     usedLifetime->implicitLifetimeMembers->destructor &&
                     plainLifetime && plainLifetime->implicitLifetimeMembers &&
                     !plainLifetime->implicitLifetimeMembers->defaultConstructor &&
                     plainLifetime->implicitLifetimeMembers->copyConstructor &&
                     !plainLifetime->implicitLifetimeMembers->moveConstructor &&
                     !plainLifetime->implicitLifetimeMembers->destructor,
                     "Clang record snapshots retain lazily declared and materialized implicit lifetime members");
        const auto implicitDeclarations = cpp->implicitLifetimeDeclarations("BaseA");
        const auto repeatedDeclarations = cpp->implicitLifetimeDeclarations("BaseA");
        const auto deletedDeclarations = cpp->implicitLifetimeDeclarations("DeletedLifetime");
        const auto throwingDeclarations = cpp->implicitLifetimeDeclarations("ImplicitThrowing");
        bool defaultCtor = false, copyCtor = false, moveCtor = false, destructor = false, deletedDefault = false;
        if (implicitDeclarations) for (const auto& method : *implicitDeclarations)
        {
            defaultCtor |= method.kind == CppImplicitLifetimeKind::DefaultConstructor && method.parameters.empty() &&
                method.access == CppAccess::Public && method.isTrivial && !method.isDeleted && !method.canonicalFunctionType.empty() &&
                method.exceptionSpecification == CppExceptionSpecification::NonThrowing &&
                method.abiSymbol == "??0BaseA@@QEAA@XZ";
            copyCtor |= method.kind == CppImplicitLifetimeKind::CopyConstructor && method.parameters.size() == 1;
            moveCtor |= method.kind == CppImplicitLifetimeKind::MoveConstructor && method.parameters.size() == 1;
            destructor |= method.kind == CppImplicitLifetimeKind::Destructor && method.parameters.empty() &&
                method.exceptionSpecification == CppExceptionSpecification::NonThrowing &&
                method.abiSymbol == "??1BaseA@@QEAA@XZ";
        }
        if (deletedDeclarations) for (const auto& method : *deletedDeclarations)
            deletedDefault |= method.kind == CppImplicitLifetimeKind::DefaultConstructor && method.isDeleted && !method.abiSymbol;
        bool throwingCtor = false, throwingDtor = false;
        if (throwingDeclarations) for (const auto& method : *throwingDeclarations)
        {
            throwingCtor |= method.kind == CppImplicitLifetimeKind::DefaultConstructor &&
                method.exceptionSpecification == CppExceptionSpecification::PotentiallyThrowing;
            throwingDtor |= method.kind == CppImplicitLifetimeKind::Destructor &&
                method.exceptionSpecification == CppExceptionSpecification::PotentiallyThrowing;
        }
        bool repeatedSymbols = implicitDeclarations && repeatedDeclarations &&
            implicitDeclarations->size() == repeatedDeclarations->size();
        if (repeatedSymbols) for (std::size_t i = 0; i < implicitDeclarations->size(); ++i)
            repeatedSymbols &= (*implicitDeclarations)[i].kind == (*repeatedDeclarations)[i].kind &&
                               (*implicitDeclarations)[i].exceptionSpecification == (*repeatedDeclarations)[i].exceptionSpecification &&
                               (*implicitDeclarations)[i].abiSymbol == (*repeatedDeclarations)[i].abiSymbol;
        const auto recordsAfterImplicit = cpp->recordsSnapshot();
        const auto* baseAfterImplicit = recordsAfterImplicit ? recordByName(*recordsAfterImplicit, "BaseA") : nullptr;
        ok &= expect(defaultCtor && copyCtor && moveCtor && destructor && deletedDefault && throwingCtor && throwingDtor && repeatedSymbols &&
                     implicitDeclarations->size() == repeatedDeclarations->size() &&
                     implicitLifetime && baseAfterImplicit && implicitLifetime->odrHash == baseAfterImplicit->odrHash &&
                     implicitLifetime->methods.size() == baseAfterImplicit->methods.size() &&
                     implicitLifetime->implicitLifetimeMembers == baseAfterImplicit->implicitLifetimeMembers &&
                     !cpp->implicitLifetimeDeclarations("MissingRecord"),
                     "Clang Sema materializes implicit lifetime declarations, including deleted defaults, repeatably");
        const auto defaultIr = cpp->emitImplicitLifetimeIR("BaseA", CppImplicitLifetimeKind::DefaultConstructor);
        const auto destructorIr = cpp->emitImplicitLifetimeIR("BaseA", CppImplicitLifetimeKind::Destructor);
        const auto nontrivialCtorIr = cpp->emitImplicitLifetimeIR("ImplicitThrowing", CppImplicitLifetimeKind::DefaultConstructor);
        const auto nontrivialDtorIr = cpp->emitImplicitLifetimeIR("ImplicitThrowing", CppImplicitLifetimeKind::Destructor);
        const auto repeatedDefaultIr = cpp->emitImplicitLifetimeIR("BaseA", CppImplicitLifetimeKind::DefaultConstructor);
        bool persistedLifetimeIr = false;
        if (nontrivialCtorIr && nontrivialDtorIr)
        {
            const auto directory = std::filesystem::path(argv[2]) / "cpp-fixtures" / "lifetime-emission";
            std::error_code ec;
            std::filesystem::create_directories(directory, ec);
            std::ofstream ctorFile(directory / "implicit-ctor.ll", std::ios::binary);
            std::ofstream dtorFile(directory / "implicit-dtor.ll", std::ios::binary);
            ctorFile << *nontrivialCtorIr;
            dtorFile << *nontrivialDtorIr;
            persistedLifetimeIr = !ec && ctorFile.good() && dtorFile.good();
        }
        bool nativeObjectEmission = persistedLifetimeIr;
        if (nativeObjectEmission)
        {
            const auto directory = std::filesystem::path(argv[2]) / "cpp-fixtures" / "lifetime-emission";
            for (const auto level : {nativesharp::llvm_backend::LlvmOptimizationLevel::O0,
                                     nativesharp::llvm_backend::LlvmOptimizationLevel::O2})
            {
                std::string emitError;
                const auto suffix = level == nativesharp::llvm_backend::LlvmOptimizationLevel::O0 ? "O0" : "O2";
                for (const auto& entry : {std::pair{"ctor", CppImplicitLifetimeKind::DefaultConstructor},
                                          std::pair{"dtor", CppImplicitLifetimeKind::Destructor}})
                {
                    nativeObjectEmission &= nativesharp::llvm_backend::emitCppImplicitLifetimeObject(
                        *cpp, "ImplicitThrowing", entry.second, level,
                        directory / (std::string("nsc-") + entry.first + "-" + suffix + ".obj"),
                        emitError);
                }
                if (!nativeObjectEmission) std::cerr << "Lifetime object emission: " << emitError << '\n';
            }
            std::string invalidError;
            nativeObjectEmission &= !nativesharp::llvm_backend::LlvmModule::parseText("invalid LLVM IR", invalidError) &&
                                    !invalidError.empty();
            nativeObjectEmission &= !nativesharp::llvm_backend::emitCppImplicitLifetimeObject(
                *cpp, "MissingRecord", CppImplicitLifetimeKind::Destructor,
                nativesharp::llvm_backend::LlvmOptimizationLevel::O0, directory / "missing.obj", invalidError) &&
                !invalidError.empty();
        }
        const auto definesSymbol = [](const std::optional<std::string>& ir, std::string_view symbol) {
            if (!ir) return false;
            for (auto position = ir->find(symbol); position != std::string::npos;
                 position = ir->find(symbol, position + symbol.size()))
            {
                const auto previous = ir->rfind('\n', position);
                const auto line = previous == std::string::npos ? 0 : previous + 1;
                if (ir->compare(line, 7, "define ") == 0) return true;
            }
            return false;
        };
        ok &= expect(definesSymbol(defaultIr, "??0BaseA@@QEAA@XZ") &&
                     definesSymbol(destructorIr, "??1BaseA@@QEAA@XZ") &&
                     definesSymbol(nontrivialCtorIr, "??0ImplicitThrowing@@QEAA@XZ") &&
                     nontrivialCtorIr->find("??0ThrowingMember@@QEAA@XZ") != std::string::npos &&
                     definesSymbol(nontrivialDtorIr, "??1ImplicitThrowing@@QEAA@XZ") &&
                     nontrivialDtorIr->find("??1ThrowingMember@@QEAA@XZ") != std::string::npos &&
                     repeatedDefaultIr && *repeatedDefaultIr == *defaultIr && nativeObjectEmission &&
                     !cpp->emitImplicitLifetimeIR("DeletedLifetime", CppImplicitLifetimeKind::DefaultConstructor) &&
                     !cpp->emitImplicitLifetimeIR("MissingRecord", CppImplicitLifetimeKind::Destructor) &&
                     !cpp->emitImplicitLifetimeIR("BaseA", CppImplicitLifetimeKind::CopyConstructor),
                     "Clang CodeGen emits verified IR for implicit default construction and destruction");
        if (plainLifetime)
            for (std::size_t i = 0; i < plainLifetime->methods.size(); ++i)
            {
                auto projected = projectCppSemanticLifetimeMethod(*plainLifetime, i, 7, "Semantic", "x86_64-pc-windows-msvc");
                const auto* method = std::get_if<CppSemanticLifetimeMethod>(&projected);
                if (!method) continue;
                if (method->kind == CppMethodKind::Constructor)
                    lifetimeCtor = method->ownerIdentityKey == "Semantic::PlainLifetime" &&
                        method->externalSymbol == "??0PlainLifetime@@QEAA@H@Z" && method->parameters.size() == 1 &&
                        method->parameters[0].nativeSharpTypeKey == "builtin:int" &&
                        method->returnAbi == CppLifetimeReturnAbi::ThisPointer &&
                        !method->dllImport && method->isNonThrowing;
                if (method->kind == CppMethodKind::Destructor)
                    lifetimeDtor = method->externalSymbol == "??1PlainLifetime@@QEAA@XZ" && method->parameters.empty() &&
                        method->returnAbi == CppLifetimeReturnAbi::Void && !method->dllImport && method->isNonThrowing;
                auto importedRecord = *plainLifetime;
                importedRecord.methods[i].dllImport = true;
                importedRecord.methods[i].isNonThrowing = false;
                const auto imported = projectCppSemanticLifetimeMethod(importedRecord, i, 7, "Semantic", "x86_64-pc-windows-msvc");
                const auto* importedMethod = std::get_if<CppSemanticLifetimeMethod>(&imported);
                ok &= expect(importedMethod && importedMethod->dllImport && !importedMethod->isNonThrowing &&
                             importedMethod->returnAbi == method->returnAbi,
                             "lifetime metadata preserves DLL and throwing contract independently of return ABI");
            }
        if (lockedLifetime && !lockedLifetime->methods.empty())
        {
            privateLifetimeDeferred = true;
            for (std::size_t i = 0; i < lockedLifetime->methods.size(); ++i)
                privateLifetimeDeferred &= std::holds_alternative<CppSemanticFunctionFailure>(
                    projectCppSemanticLifetimeMethod(*lockedLifetime, i, 7, "Semantic", "x86_64-pc-windows-msvc"));
        }
        if (basedLifetime && !basedLifetime->methods.empty())
        {
            basedLifetimeDeferred = true;
            for (std::size_t i = 0; i < basedLifetime->methods.size(); ++i)
                basedLifetimeDeferred &= std::holds_alternative<CppSemanticFunctionFailure>(
                    projectCppSemanticLifetimeMethod(*basedLifetime, i, 7, "Semantic", "x86_64-pc-windows-msvc"));
        }
        ok &= expect(lifetimeCtor && lifetimeDtor && privateLifetimeDeferred && basedLifetimeDeferred,
                     "explicit C++ lifetime method metadata preserves symbols and defers private or based types");
        ok &= expect(derived && derived->bases.size() == 1 && derived->bases[0].isVirtual &&
                     derived->bases[0].access == CppAccess::Public && derived->bases[0].offsetBits &&
                     multi && multi->bases.size() == 2 && !multi->bases[0].isVirtual &&
                     multi->bases[0].access == CppAccess::Public && multi->bases[0].offsetBits == 0 &&
                     multi->bases[1].access == CppAccess::Private && multi->bases[1].offsetBits == 32 &&
                     multi->bases[0].type.canonicalSpelling.find("BaseA") != std::string::npos &&
                     multi->bases[1].type.canonicalSpelling.find("BaseB") != std::string::npos,
                     "direct C++ bases preserve order, access, virtual status and Clang offsets");
        const auto* nestedPublic = records ? recordByName(*records, "Outer::Inner") : nullptr;
        const auto* nestedPrivate = records ? recordByName(*records, "Outer::Secret") : nullptr;
        ok &= expect(nestedPublic && nestedPublic->complete && nestedPublic->sizeBits == 32 &&
                     nestedPublic->access == CppAccess::Public && nestedPrivate && nestedPrivate->complete &&
                     nestedPrivate->access == CppAccess::Private &&
                     std::count_if(records->begin(), records->end(), [](const auto& item) {
                         return item.qualifiedName == "Outer::Inner";
                     }) == 1,
                     "nested records retain qualified identity, access, definition and layout");
        ok &= expect(record && record->kind == CppRecordKind::Class && record->complete &&
                     record->sizeBits == 128 && record->fields.size() == 2 &&
                     record->fields[0].offsetBits == 64 && record->fields[0].access == CppAccess::Private &&
                     record->fields[1].offsetBits == 96 && record->fields[1].access == CppAccess::Public,
                     "C++ class layout and field access match the target");
        if (record)
        {
            const auto publicField = projectCppSemanticInstanceField(*record, 1, 7, "Semantic", "x86_64-pc-windows-msvc");
            auto badLayout = *record;
            badLayout.fields[1].offsetBits = *badLayout.sizeBits;
            auto missingLayout = *record;
            missingLayout.sizeBits.reset();
            auto missingOffset = *record;
            missingOffset.fields[1].offsetBits.reset();
            auto partialByte = *record;
            partialByte.fields[1].offsetBits = 1;
            auto unsupportedUnion = *record;
            unsupportedUnion.kind = CppRecordKind::Union;
            ok &= expect(publicField && publicField->ownerIdentityKey == "Semantic::MyClass" &&
                         publicField->projectOrdinal == 7 && publicField->name == "count" &&
                         publicField->byteOffset == 12 && publicField->type.nativeSharpTypeKey == "builtin:int" &&
                         !projectCppSemanticInstanceField(*record, 0, 7, "Semantic", "x86_64-pc-windows-msvc") &&
                         !projectCppSemanticInstanceField(badLayout, 1, 7, "Semantic", "x86_64-pc-windows-msvc") &&
                         !projectCppSemanticInstanceField(missingLayout, 1, 7, "Semantic", "x86_64-pc-windows-msvc") &&
                         !projectCppSemanticInstanceField(missingOffset, 1, 7, "Semantic", "x86_64-pc-windows-msvc") &&
                         !projectCppSemanticInstanceField(partialByte, 1, 7, "Semantic", "x86_64-pc-windows-msvc") &&
                         !projectCppSemanticInstanceField(unsupportedUnion, 1, 7, "Semantic", "x86_64-pc-windows-msvc") &&
                         !projectCppSemanticInstanceField(*record, 1, 7, "Semantic", "other-target"),
                         "isolated field projection retains Clang layout and rejects private or invalid fields");
            const auto* qualified = records ? recordByName(*records, "QualifiedFields") : nullptr;
            const auto* packedField = records ? recordByName(*records, "PackedField") : nullptr;
            const auto constField = qualified ? projectCppSemanticInstanceField(*qualified, 0, 7, "Semantic", "x86_64-pc-windows-msvc") : std::nullopt;
            const auto volatileField = qualified ? projectCppSemanticInstanceField(*qualified, 1, 7, "Semantic", "x86_64-pc-windows-msvc") : std::nullopt;
            ok &= expect(qualified && constField && constField->type.isConst && volatileField &&
                         volatileField->type.isVolatile &&
                         !projectCppSemanticInstanceField(*qualified, 2, 7, "Semantic", "x86_64-pc-windows-msvc"),
                         "field projection retains qualifiers and defers bitfields");
            ok &= expect(packedField && packedField->fields.size() == 2 &&
                         !projectCppSemanticInstanceField(*packedField, 1, 7, "Semantic", "x86_64-pc-windows-msvc"),
                         "field projection defers packed fields without guaranteed natural alignment");
            bool staticShared = false, staticConst = false, staticVolatile = false, staticTls = false, staticPrivate = false;
            std::optional<CppSemanticStaticField> sharedMetadata, constStaticMetadata, volatileStaticMetadata;
            for (std::size_t index = 0; index < record->staticFields.size(); ++index)
            {
                const auto& field = record->staticFields[index];
                const auto projected = projectCppSemanticStaticField(*record, index, 7, "Semantic", "x86_64-pc-windows-msvc");
                if (field.name == "shared") staticShared = projected && projected->projectOrdinal == 7 &&
                    projected->ownerIdentityKey == "Semantic::MyClass" && projected->externalSymbol == "?shared@MyClass@@2HA" &&
                    projected->type.nativeSharpTypeKey == "builtin:int" && field.type.sizeBits == 32;
                if (field.name == "shared" && projected) sharedMetadata = *projected;
                if (field.name == "limit") { staticConst = projected && projected->type.isConst; if (projected) constStaticMetadata = *projected; }
                if (field.name == "io") { staticVolatile = projected && projected->type.isVolatile; if (projected) volatileStaticMetadata = *projected; }
                if (field.name == "tls") staticTls = field.threadLocal && !projected;
                if (field.name == "secret") staticPrivate = field.access == CppAccess::Private && !projected;
            }
            ok &= expect(staticShared && staticConst && staticVolatile && staticTls && staticPrivate &&
                         !projectCppSemanticStaticField(*record, record->staticFields.size(), 7, "Semantic", "x86_64-pc-windows-msvc") &&
                         !projectCppSemanticStaticField(*record, 0, 7, "Semantic", "other-target"),
                         "static fields retain symbols and qualifiers while private and TLS fields are deferred");
            bool sawCtor = false, sawDtor = false, sawStatic = false, sawVirtual = false;
            bool sawLvalue = false, sawRvalue = false, sawVectorMethod = false;
            for (const auto& method : record->methods)
            {
                sawLvalue |= method.name == "Q" && method.refQualifier == CppMethodRefQualifier::LValue &&
                    !method.canonicalFunctionType.empty() && method.callingConvention == "cdecl";
                sawRvalue |= method.name == "Q" && method.refQualifier == CppMethodRefQualifier::RValue &&
                    !method.canonicalFunctionType.empty() && method.callingConvention == "cdecl";
                sawVectorMethod |= method.name == "V" && method.isStatic && method.callingConvention == "vectorcall" &&
                    !method.canonicalFunctionType.empty();
                sawCtor |= method.kind == CppMethodKind::Constructor && method.externalSymbol == "??0MyClass@@QEAA@H@Z";
                sawDtor |= method.kind == CppMethodKind::Destructor && method.externalSymbol == "??1MyClass@@QEAA@XZ";
                sawStatic |= method.name == "B" && method.isStatic && method.externalSymbol == "?B@MyClass@@SAHN@Z";
                sawVirtual |= method.name == "C" && method.isVirtual && method.externalSymbol == "?C@MyClass@@UEAAHXZ";
            }
            if (!(sawCtor && sawDtor && sawStatic && sawVirtual))
                for (const auto& method : record->methods)
                    std::cerr << "method " << method.name << ": "
                              << method.externalSymbol.value_or("<no symbol>") << '\n';
            ok &= expect(sawCtor && sawDtor && sawStatic && sawVirtual,
                         "explicit constructors, destructor, static and virtual methods are projected");
            ok &= expect(sawLvalue && sawRvalue && sawVectorMethod,
                         "method snapshots preserve ref qualifiers, canonical types and calling conventions");
            bool mappedInstance = false, volatilePreserved = false, restrictionsRetained = true;
            bool sawNoThrow = false, sawExplicitThrow = false, sawUnknownThrow = false;
            bool staticMapped = false, staticThrowMapped = false, staticRejected = true;
            std::optional<CppSemanticInstanceMethod> instanceMetadata;
            std::optional<CppSemanticStaticMethod> staticMetadata;
            for (std::size_t index = 0; index < record->methods.size(); ++index)
            {
                const auto& method = record->methods[index];
                const auto projected = projectCppSemanticInstanceMethod(*record, index, 7, "Semantic", "x86_64-pc-windows-msvc");
                const auto* instance = std::get_if<CppSemanticInstanceMethod>(&projected);
                const auto staticProjection = projectCppSemanticStaticMethod(*record, index, 7, "Semantic", "x86_64-pc-windows-msvc");
                const auto* staticMethod = std::get_if<CppSemanticStaticMethod>(&staticProjection);
                if (method.name == "B")
                {
                    staticMapped = staticMethod && staticMethod->ownerIdentityKey == "Semantic::MyClass" &&
                        staticMethod->externalSymbol == "?B@MyClass@@SAHN@Z" &&
                        staticMethod->result.nativeSharpTypeKey == "builtin:int" && staticMethod->parameters.size() == 1 &&
                        staticMethod->parameters[0].nativeSharpTypeKey == "builtin:double";
                    if (staticMethod) staticMetadata = *staticMethod;
                }
                if (method.name == "A" || method.name == "HiddenStatic" ||
                    method.name == "VarStatic" || method.name == "V") staticRejected &= !staticMethod;
                if (method.name == "ThrowStatic") staticThrowMapped = staticMethod != nullptr;
                if (method.name == "A")
                {
                    mappedInstance = instance && instance->projectOrdinal == 7 &&
                        instance->ownerIdentityKey == "Semantic::MyClass" && instance->receiverConst &&
                        instance->externalSymbol == method.externalSymbol &&
                        instance->result.nativeSharpTypeKey == "builtin:int" && instance->parameters.size() == 1 &&
                        instance->parameters[0].nativeSharpTypeKey == "builtin:int";
                    if (instance) instanceMetadata = *instance;
                }
                if (method.name == "Volatile") volatilePreserved = method.isVolatile && !instance;
                if (method.name == "Safe") sawNoThrow = method.isNonThrowing;
                if (method.name == "Throwing") sawExplicitThrow = !method.isNonThrowing && instance;
                if (method.name == "Unknown") sawUnknownThrow = !method.isNonThrowing && instance;
                if (method.name == "hiddenMethod" || method.name == "B" || method.name == "C" || method.name == "Q")
                    restrictionsRetained &= !instance;
            }
            const auto invalidTarget = projectCppSemanticInstanceMethod(*record, 0, 7, "Semantic", "other-target");
            const auto invalidIndex = projectCppSemanticInstanceMethod(*record, record->methods.size(), 7, "Semantic", "x86_64-pc-windows-msvc");
            ok &= expect(mappedInstance && volatilePreserved && restrictionsRetained &&
                         std::holds_alternative<CppSemanticFunctionFailure>(invalidTarget) &&
                         std::holds_alternative<CppSemanticFunctionFailure>(invalidIndex),
                         "instance method metadata retains receiver and symbol while deferring unsupported contracts");
            ok &= expect(sawNoThrow && sawExplicitThrow && sawUnknownThrow,
                         "Clang method snapshots conservatively retain resolved non-throwing specifications");
            ok &= expect(staticMapped && staticThrowMapped && staticRejected,
                         "static method metadata preserves its symbol and defers unsupported contracts");
            if (instanceMetadata && staticMetadata && publicField && sharedMetadata && constStaticMetadata && volatileStaticMetadata && record->alignmentBits)
            {
                const auto identity = projectCppRecordIdentity(*record, "Semantic");
                nativesharp::frontend::ProjectCompilation methodCompilation;
                methodCompilation.setSourceBufferProvider([](const std::filesystem::path& source)
                    -> std::optional<std::string> {
                    if (source.filename() == "Main.ns") return "int Main() { return 0; }\n";
                    return std::nullopt;
                });
                const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                std::string error;
                const bool configured = identity && methodCompilation.setExperimentalCppImportedTypes({{*identity,
                    nativesharp::frontend::NamedTypeDeclarationKind::Struct,
                    nativesharp::frontend::NamedTypeAccessibility::Public, 0,
                    record->location ? record->location->file.generic_string() : "",
                    nativesharp::frontend::CppImportedObjectLayout{*record->sizeBits / 8, *record->alignmentBits / 8}}}, error);
                const bool namesReady = configured && methodCompilation.compileProjectNames(path, error);
                auto imported = toCppImportedInstanceMethod(*instanceMetadata);
                imported.projectOrdinal = 0;
                nativesharp::frontend::ProjectMemberModel model;
                const bool installed = namesReady && model.setExperimentalCppInstanceMethods({imported}, error);
                const bool duplicateRejected = !model.setExperimentalCppInstanceMethods({imported, imported}, error);
                auto importedStatic = toCppImportedStaticMethod(*staticMetadata);
                importedStatic.projectOrdinal = 0;
                auto overloadStatic = importedStatic;
                overloadStatic.externalSymbol = "synthetic-overload-symbol";
                overloadStatic.parameterTypeKeys[0] = "builtin:int";
                const bool staticInstalled = model.setExperimentalCppStaticMethods({importedStatic, overloadStatic}, error);
                const bool duplicateStaticRejected = !model.setExperimentalCppStaticMethods({importedStatic, importedStatic}, error);
                auto sharedField = toCppImportedStaticField(*sharedMetadata);
                auto limitField = toCppImportedStaticField(*constStaticMetadata);
                auto ioField = toCppImportedStaticField(*volatileStaticMetadata);
                sharedField.projectOrdinal = limitField.projectOrdinal = ioField.projectOrdinal = 0;
                const bool staticFieldsInstalled = model.setExperimentalCppStaticFields({sharedField, limitField, ioField}, error);
                const bool duplicateStaticFieldRejected = !model.setExperimentalCppStaticFields({sharedField, sharedField}, error);
                auto importedField = toCppImportedInstanceField(*publicField);
                importedField.projectOrdinal = 0;
                const bool fieldInstalled = model.setExperimentalCppInstanceFields({importedField}, error);
                const bool duplicateFieldRejected = !model.setExperimentalCppInstanceFields({importedField, importedField}, error);
                for (bool isStatic : {false, true})
                {
                    nativesharp::frontend::ProjectMemberModel missingTypeModel;
                    nativesharp::diagnostics::DiagnosticEngine missingTypeDiagnostics;
                    auto missingInstance = imported;
                    auto missingStatic = importedStatic;
                    missingInstance.returnTypeKey = "type:Semantic::Missing*";
                    missingStatic.returnTypeKey = "type:Semantic::Missing*";
                    const bool missingConfigured = isStatic
                        ? missingTypeModel.setExperimentalCppStaticMethods({missingStatic}, error)
                        : missingTypeModel.setExperimentalCppInstanceMethods({missingInstance}, error);
                    ok &= expect(missingConfigured && namesReady &&
                                 !missingTypeModel.build(*methodCompilation.nameLookup(), methodCompilation.units(),
                                                         missingTypeDiagnostics, methodCompilation.genericModel()) &&
                                 missingTypeDiagnostics.hasErrors(),
                                 "C++ method indexing rejects a record pointer absent from the project lookup");
                }
                nativesharp::diagnostics::DiagnosticEngine diagnostics;
                const bool built = installed && model.build(*methodCompilation.nameLookup(), methodCompilation.units(), diagnostics,
                                                            methodCompilation.genericModel());
                const auto ordinal = namesReady ? methodCompilation.nameLookup()->typeOrdinalForIdentity(identity->key()) : std::nullopt;
                const auto matches = ordinal ? model.findMembers(*ordinal, "A") : std::span<const std::size_t>{};
                const auto* indexed = matches.size() == 1 ? &model.members()[matches[0]] : nullptr;
                const auto fieldMatches = ordinal ? model.findMembers(*ordinal, "count") : std::span<const std::size_t>{};
                const auto* indexedField = fieldMatches.size() == 1 ? &model.members()[fieldMatches[0]] : nullptr;
                const auto staticMatches = ordinal ? model.findMembers(*ordinal, "B") : std::span<const std::size_t>{};
                const auto sharedMatches = ordinal ? model.findMembers(*ordinal, "shared") : std::span<const std::size_t>{};
                const auto limitMatches = ordinal ? model.findMembers(*ordinal, "limit") : std::span<const std::size_t>{};
                const auto ioMatches = ordinal ? model.findMembers(*ordinal, "io") : std::span<const std::size_t>{};
                ok &= expect(built && duplicateRejected && indexed && indexed->isCppExternal && indexed->hasImplicitThis &&
                             indexed->isReadonly && indexed->cppExternalSymbol == imported.externalSymbol &&
                             indexed->declaredTypeKey == "builtin:int" && indexed->parameters.size() == 1 &&
                             indexed->parameters[0].typeKey == "builtin:int" && !diagnostics.hasErrors() &&
                             !model.setExperimentalCppInstanceMethods({}, error),
                             "isolated member model indexes a Clang method and freezes its identity");
                ok &= expect(built && fieldInstalled && duplicateFieldRejected && indexedField &&
                             indexedField->isCppExternal && indexedField->kind == nativesharp::frontend::ClassMemberKind::Field &&
                             indexedField->explicitFieldOffset == 12 && indexedField->declaredTypeKey == "builtin:int" &&
                             !model.setExperimentalCppInstanceFields({}, error),
                             "isolated member model indexes a Clang field with its physical offset");
                bool staticSymbols = staticMatches.size() == 2;
                for (const auto index : staticMatches)
                    staticSymbols &= model.members()[index].isStatic && !model.members()[index].hasImplicitThis &&
                        model.members()[index].isCppExternal && !model.members()[index].cppExternalSymbol.empty();
                ok &= expect(built && staticInstalled && duplicateStaticRejected && staticSymbols &&
                             !model.setExperimentalCppStaticMethods({}, error),
                             "isolated member model indexes static C++ overloads without a receiver");
                ok &= expect(built && staticFieldsInstalled && duplicateStaticFieldRejected && sharedMatches.size() == 1 &&
                             limitMatches.size() == 1 && ioMatches.size() == 1 &&
                             model.members()[sharedMatches[0]].isStatic && model.members()[sharedMatches[0]].isCppExternal &&
                             model.members()[sharedMatches[0]].cppExternalSymbol == "?shared@MyClass@@2HA" &&
                             model.members()[limitMatches[0]].isReadonly && model.members()[ioMatches[0]].isVolatile &&
                             !model.setExperimentalCppStaticFields({}, error),
                             "isolated member model indexes external C++ static fields and qualifiers");
                nativesharp::frontend::ProjectMemberModel wrongStaticOwner;
                auto invalidStatic = importedStatic;
                invalidStatic.projectOrdinal = 99;
                nativesharp::diagnostics::DiagnosticEngine staticDiagnostics;
                const bool staticOwnerConfigured = wrongStaticOwner.setExperimentalCppStaticMethods({invalidStatic}, error);
                ok &= expect(staticOwnerConfigured && !wrongStaticOwner.build(*methodCompilation.nameLookup(),
                             methodCompilation.units(), staticDiagnostics, methodCompilation.genericModel()) &&
                             staticDiagnostics.hasErrors(), "static C++ method rejects an incorrect owner");
                nativesharp::frontend::ProjectMemberModel wrongStaticFieldOwner;
                sharedField.projectOrdinal = 99;
                nativesharp::diagnostics::DiagnosticEngine staticFieldDiagnostics;
                const bool staticFieldOwnerConfigured = wrongStaticFieldOwner.setExperimentalCppStaticFields({sharedField}, error);
                ok &= expect(staticFieldOwnerConfigured && !wrongStaticFieldOwner.build(*methodCompilation.nameLookup(),
                             methodCompilation.units(), staticFieldDiagnostics, methodCompilation.genericModel()) &&
                             staticFieldDiagnostics.hasErrors(), "static C++ field rejects an incorrect owner");
                for (int scenario : {0, 1, 2, 3, 4, 5})
                {
                    nativesharp::frontend::ProjectMemberModel fieldModel;
                    auto probe = importedField;
                    if (scenario == 0) probe.projectOrdinal = 99;
                    if (scenario == 1) probe.byteOffset = *record->sizeBits / 8;
                    if (scenario == 2) probe.name = imported.name;
                    if (scenario == 3) { probe.isConst = true; probe.isVolatile = true; }
                    if (scenario == 4) probe.byteAlignment = 3;
                    if (scenario == 5) { probe.byteOffset = 9; probe.byteAlignment = 1; }
                    const bool configuredFields = fieldModel.setExperimentalCppInstanceFields({probe}, error) &&
                        fieldModel.setExperimentalCppInstanceMethods({imported}, error);
                    nativesharp::diagnostics::DiagnosticEngine fieldDiagnostics;
                    const bool fieldBuilt = configuredFields && fieldModel.build(*methodCompilation.nameLookup(),
                        methodCompilation.units(), fieldDiagnostics, methodCompilation.genericModel());
                    const auto qualifiedMatches = ordinal ? fieldModel.findMembers(*ordinal, "count") : std::span<const std::size_t>{};
                    const auto* qualifiedField = qualifiedMatches.size() == 1 ? &fieldModel.members()[qualifiedMatches[0]] : nullptr;
                    ok &= expect(scenario == 3
                        ? fieldBuilt && qualifiedField && qualifiedField->isReadonly && qualifiedField->isVolatile
                        : scenario == 5
                        ? fieldBuilt && qualifiedField && qualifiedField->explicitFieldOffset == 9 &&
                            qualifiedField->explicitFieldAlignment == 1
                        : !fieldBuilt && fieldDiagnostics.hasErrors(),
                        "field index validates ownership, bounds and names while retaining qualifiers");
                }
                nativesharp::frontend::ProjectMemberModel wrongOwner;
                imported.projectOrdinal = 99;
                nativesharp::diagnostics::DiagnosticEngine badDiagnostics;
                const bool badConfigured = wrongOwner.setExperimentalCppInstanceMethods({imported}, error);
                ok &= expect(badConfigured && namesReady &&
                             !wrongOwner.build(*methodCompilation.nameLookup(), methodCompilation.units(), badDiagnostics,
                                               methodCompilation.genericModel()) && badDiagnostics.hasErrors(),
                             "member indexing rejects an instance method owned by a different project");
            }
        }
        bool sawVirtualBaseDtor = false;
        if (derived)
            for (const auto& method : derived->methods)
                sawVirtualBaseDtor |= method.kind == CppMethodKind::Destructor &&
                    method.externalSymbol == "??_DDerived@@QEAAXXZ";
        ok &= expect(sawVirtualBaseDtor, "complete destructor with virtual base matches Clang object");
        const auto* cApi = functions ? functionByName(*functions, "c_api") : nullptr;
        const auto* noThrow = functions ? functionByName(*functions, "cpp_noexcept") : nullptr;
        const auto* vectorCall = functions ? functionByName(*functions, "vector_fn") : nullptr;
        const auto* cNoThrow = functions ? functionByName(*functions, "c_api_noexcept") : nullptr;
        const auto* canThrow = functions ? functionByName(*functions, "cpp_throwing") : nullptr;
        ok &= expect(noThrow && noThrow->isNonThrowing && cApi && !cApi->isNonThrowing &&
                     vectorCall && !vectorCall->isNonThrowing && cNoThrow && cNoThrow->isNonThrowing &&
                     canThrow && !canThrow->isNonThrowing,
                     "free function snapshots retain resolved exception specifications without inferring from C linkage");
        const auto* nestedFunction = functions ? functionByName(*functions, "root::sub::foo") : nullptr;
        const auto* local = functions ? functionByName(*functions, "tools::local") : nullptr;
        const auto* pointer = functions ? functionByName(*functions, "tools::pointer") : nullptr;
        const auto* handle = functions ? functionByName(*functions, "tools::handle") : nullptr;
        const auto* widths = functions ? functionByName(*functions, "tools::widths") : nullptr;
        if (handle)
        {
            nativesharp::frontend::ProjectCompilation compilation;
            compilation.setSourceBufferProvider([](const std::filesystem::path& path)
                -> std::optional<std::string> {
                if (path.filename() == "Main.ns") return "int Main() { tools.handle(1); return 0; }\n";
                return std::nullopt;
            });
            std::string error;
            const bool configured = compilation.setCppNamespaceProvider(
                [](const auto&, auto, auto& out, auto&) { out.emplace_back(0, "tools"); return true; }, error);
            const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
            const bool ready = configured && compilation.compileProjectMembers(path, error);
            nativesharp::frontend::ProjectNamespaceFunctionModel observer;
            const bool observed = ready && observer.setCaptureCppFunctionDemands(true) &&
                observer.build(*compilation.nameLookup(), *compilation.memberModel(), compilation.units(),
                               compilation.diagnostics(), compilation.genericModel());
            std::vector<CppDeferredFunctionCandidate> retained;
            bool rejectedBadIndex = false, rejectedNullSyntax = false, rejectedWrongOwner = false,
                rejectedProjectedEntry = false, rejectedBadDemand = false;
            if (observed)
            {
                std::vector<CppSemanticInventoryEntry> inventory{{path, 3, 0, "x86_64-pc-windows-msvc",
                    *handle, projectCppSemanticFunction(*handle, 0, "Semantic")}};
                auto selection = selectCppSemanticDemands(inventory, observer.cppFunctionDemands());
                if (auto* matches = std::get_if<std::vector<CppSemanticDemandCandidates>>(&selection);
                    matches && matches->size() == 1 && !matches->front().candidates.unprojectedIndices.empty())
                {
                    auto copied = materializeCppDeferredFunctions(inventory, observer.cppFunctionDemands(), *matches);
                    if (auto* candidates = std::get_if<std::vector<CppDeferredFunctionCandidate>>(&copied))
                        retained = std::move(*candidates);
                    auto bad = *matches;
                    bad[0].candidates.unprojectedIndices[0] = inventory.size();
                    rejectedBadIndex = std::holds_alternative<CppDeferredFunctionFailure>(
                        materializeCppDeferredFunctions(inventory, observer.cppFunctionDemands(), bad));
                    std::vector<nativesharp::frontend::CppFunctionDemand> badDemands(
                        observer.cppFunctionDemands().begin(), observer.cppFunctionDemands().end());
                    badDemands[(*matches)[0].demandIndex].syntax = nullptr;
                    rejectedNullSyntax = std::holds_alternative<CppDeferredFunctionFailure>(
                        materializeCppDeferredFunctions(inventory, badDemands, *matches));
                    bad = *matches;
                    bad[0].projectOrdinal = 99;
                    rejectedWrongOwner = std::holds_alternative<CppDeferredFunctionFailure>(
                        materializeCppDeferredFunctions(inventory, observer.cppFunctionDemands(), bad));
                    bad = *matches;
                    bad[0].demandIndex = observer.cppFunctionDemands().size();
                    rejectedBadDemand = std::holds_alternative<CppDeferredFunctionFailure>(
                        materializeCppDeferredFunctions(inventory, observer.cppFunctionDemands(), bad));
                    auto projectedInventory = inventory;
                    projectedInventory[0].projection = nativesharp::frontend::NamespaceFunctionSymbol{};
                    rejectedProjectedEntry = std::holds_alternative<CppDeferredFunctionFailure>(
                        materializeCppDeferredFunctions(projectedInventory, observer.cppFunctionDemands(), *matches));
                }
            }
            ok &= expect(retained.size() == 1 && retained[0].syntax && retained[0].directCall &&
                         retained[0].consumerProject == 0 && retained[0].declarationProject == 0 &&
                         retained[0].referenceIndex == 3 && retained[0].declaration.qualifiedName == "tools::handle" &&
                         !retained[0].reason.empty() && rejectedBadIndex && rejectedNullSyntax &&
                         rejectedWrongOwner && rejectedBadDemand && rejectedProjectedEntry,
                         "unprojected Clang candidate survives inventory lifetime and rejects invalid provenance");
        }
        bool transactionalFailure = false;
        for (const int invalid : {0, 1, 2, 3, 4, 5, 6, 7, 8})
        {
            nativesharp::frontend::ProjectCompilation compilation;
            compilation.setSourceBufferProvider([invalid](const std::filesystem::path& path)
                -> std::optional<std::string> {
                if (path.filename() == "Main.ns" && (invalid == 4 || invalid == 8))
                    return "class Program { static int shadow(int x) { return x; } static int Main() { return shadow(42); } }\n";
                if (path.filename() == "Main.ns" && invalid == 7)
                    return "int Main() { return shadow(42); }\n";
                if (path.filename() == "Main.ns") return "int Main() { int shadow = 42; return shadow; }\n";
                return std::nullopt;
            });
            const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
            CppProjectSessions projectSessions;
            projectSessions.projectFile = std::filesystem::absolute(path);
            if (invalid >= 5)
            {
                projectSessions.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                    CppLanguageMode::C, "int shadow(int);\n")));
                projectSessions.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                    CppLanguageMode::C, "double shadow(int);\n")));
            }
            else projectSessions.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                CppLanguageMode::Cpp, "struct Opaque; int shadow(int); int shadow(Opaque*);\n")));
            std::vector<CppProjectSessions> projects;
            projects.push_back(std::move(projectSessions));
            std::string error;
            const bool configured = compilation.setCppProjectSessions(std::move(projects), error) &&
                compilation.setCppFunctionProvider([invalid, &transactionalFailure](const auto& graph, auto sessions, auto, auto demands,
                    auto& output, auto& deferred, auto& why) {
                    if (invalid == 0)
                    {
                        std::vector<CppProjectSessions> bad(1);
                        bad[0].projectFile = "nonexistent-project.nsproj";
                        std::vector<nativesharp::frontend::NamespaceFunctionSymbol> saved(1);
                        saved[0].name = "sentinel";
                        std::vector<CppDeferredFunctionCandidate> savedDeferred(1);
                        savedDeferred[0].reason = "sentinel";
                        std::string ignored;
                        transactionalFailure = !provideCppSemanticFunctions(graph, bad, demands, saved, savedDeferred, ignored) &&
                            saved.size() == 1 && saved[0].name == "sentinel" && savedDeferred.size() == 1 &&
                            savedDeferred[0].reason == "sentinel" && !ignored.empty();
                    }
                    if (!provideCppSemanticFunctions(graph, sessions, demands, output, deferred, why)) return false;
                    if (!deferred.empty())
                    {
                        if (invalid == 1) deferred[0].syntax = nullptr;
                        if (invalid == 2) deferred[0].referenceIndex = 999999;
                        if (invalid == 3) deferred[0].declaration.qualifiedName = "unrelated";
                        if (invalid == 6 && !deferred[0].conflictingOrigins.empty())
                            deferred[0].conflictingOrigins[0].referenceIndex = 999999;
                    }
                    return true;
                }, error);
            const bool ready = configured && compilation.compileProjectMethodBodies(path, error);
            if ((invalid >= 1 && invalid <= 3) || invalid == 6)
                ok &= expect(!ready && error.find("does not belong to a visible demand") != std::string::npos,
                             "frontend rejects deferred candidates with invalid syntax provenance");
            else if (invalid == 7)
            {
                bool conflictDiagnostic = false;
                for (const auto& diagnostic : compilation.diagnostics().diagnostics())
                    conflictDiagnostic |= diagnostic.message().find("conflicting") != std::string::npos;
                ok &= expect(configured && !ready && conflictDiagnostic,
                             "actual call reports conflicting deferred C declarations");
            }
            else
            {
                const auto* model = compilation.namespaceFunctionModel();
                const auto deferred = model ? model->cppDeferredFunctions() :
                    std::span<const CppDeferredFunctionCandidate>{};
                const bool conflictRetained = invalid != 5 || (!deferred.empty() &&
                    deferred[0].conflictingOrigins.size() == 2 &&
                    deferred[0].conflictingOrigins[0].referenceIndex == 0 &&
                    deferred[0].conflictingOrigins[1].referenceIndex == 1 &&
                    deferred[0].conflictingOrigins[0].declaration.canonicalFunctionType !=
                        deferred[0].conflictingOrigins[1].declaration.canonicalFunctionType);
                ok &= expect(ready && transactionalFailure && !compilation.diagnostics().hasErrors() && !deferred.empty() &&
                             deferred[0].declaration.qualifiedName == "shadow" && !deferred[0].reason.empty() &&
                             conflictRetained,
                             (invalid == 4 || invalid == 8) ? "member method shadows C/C++ declarations without a false error"
                             : invalid == 5 ? "local shadowing does not eagerly reject conflicting C declarations"
                                            : "local shadowing remains valid while frontend retains unprojected candidates");
            }
        }
        for (const bool localFirst : {false, true})
        {
            nativesharp::frontend::ProjectCompilation compilation;
            compilation.setSourceBufferProvider([localFirst](const std::filesystem::path& path)
                -> std::optional<std::string> {
                if (path.filename() == "Main.ns") return localFirst
                    ? "using tools; int mix(int x) { return x; } int Main() { return mix(1); }\n"
                    : "int Main() { return tools.mix(1); }\n";
                return std::nullopt;
            });
            const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
            CppProjectSessions projectSessions;
            projectSessions.projectFile = std::filesystem::absolute(path);
            projectSessions.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
                CppLanguageMode::Cpp, "namespace tools { struct Opaque; int mix(int); int mix(Opaque*); }\n")));
            std::vector<CppProjectSessions> projects;
            projects.push_back(std::move(projectSessions));
            std::string error;
            const bool configured = compilation.setCppProjectSessions(std::move(projects), error) &&
                compilation.setCppNamespaceProvider([](const auto&, auto, auto& names, auto&) {
                    names.emplace_back(0, "tools"); return true;
                }, error) && compilation.setCppFunctionProvider(
                    [](const auto& graph, auto sessions, auto, auto demands, auto& projected, auto& deferred, auto& why) {
                        return provideCppSemanticFunctions(graph, sessions, demands, projected, deferred, why);
                    }, error);
            const bool ready = configured && compilation.compileProjectMethodBodies(path, error);
            bool incomplete = false;
            for (const auto& diagnostic : compilation.diagnostics().diagnostics())
                incomplete |= diagnostic.message().find("C/C++ overload set is incomplete") != std::string::npos;
            ok &= expect(configured && (localFirst ? ready && !incomplete : !ready && incomplete),
                         localFirst ? "namespace precedence avoids unrelated deferred overloads"
                                    : "real call cannot silently ignore an unprojected C++ overload");
        }
        ok &= expect(cApi && cApi->externalSymbol == "c_api" &&
                     cApi->languageLinkage == CppLanguageLinkage::C && cApi->callingConvention == "cdecl",
                     "extern C retains the C symbol and language linkage");
        ok &= expect(noThrow && noThrow->languageLinkage == CppLanguageLinkage::Cpp &&
                     noThrow->canonicalFunctionType.find("noexcept") != std::string::npos &&
                     vectorCall && vectorCall->languageLinkage == CppLanguageLinkage::Cpp &&
                     vectorCall->callingConvention == "vectorcall" &&
                     !vectorCall->canonicalFunctionType.empty(),
                     "C++ function type and calling convention come from Clang");
        if (vectorCall)
        {
            auto projectedVector = projectCppSemanticFunction(*vectorCall, 0, "Semantic");
            const auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projectedVector);
            nativesharp::frontend::ProjectCompilation hir;
            hir.setSourceBufferProvider([](const std::filesystem::path& path)
                -> std::optional<std::string> {
                if (path.filename() == "Main.ns") return "int Main() { return vector_fn(1); }\n";
                return std::nullopt;
            });
            std::string error;
            const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
            const bool ready = symbol && symbol->cppCallingConvention == "vectorcall" &&
                hir.setCppImportedFunctions({*symbol}, error) && hir.compileProjectOopHir(path, error);
            ok &= expect(!ready && error.find("vectorcall") != std::string::npos,
                         "unimplemented vectorcall is rejected before incorrect HIR emission");
        }
        else ok &= expect(false, "vectorcall declaration is available from Clang");
        if (nestedFunction)
        {
            auto projectedNested = projectCppSemanticFunction(*nestedFunction, 0, "Semantic");
            const auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projectedNested);
            nativesharp::frontend::ProjectCompilation nestedCompilation;
            std::string error;
            nestedCompilation.setSourceBufferProvider([](const std::filesystem::path& path)
                -> std::optional<std::string> {
                if (path.filename() == "Main.ns") return "int Main() { return root.sub.foo(1); }\n";
                return std::nullopt;
            });
            const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
            const bool ready = symbol && nestedCompilation.setCppImportedFunctions({*symbol}, error) &&
                nestedCompilation.compileProjectMembers(path, error);
            bool found = false;
            if (ready && nestedCompilation.namespaceFunctionModel())
                for (const auto& binding : nestedCompilation.namespaceFunctionModel()->referenceBindings())
                    found |= binding.writtenName == "root.sub.foo" && binding.candidateFunctionOrdinals.size() == 1;
            ok &= expect(found, "nested C++ namespace resolves in a NativeSharp qualified call");
        }
        else ok &= expect(false, "nested C++ function is available from Clang");
        ok &= expect(local && !local->externalLinkage && !local->externalSymbol,
                     "internal functions do not promise a public symbol");
        ok &= expect(pointer && pointer->returnType.kind == CppTypeKind::Pointer &&
                     pointer->returnType.sizeBits == 64 && pointer->returnType.element &&
                     pointer->returnType.element->isConst && pointer->returnType.element->sizeBits == 32,
                     "pointer and const pointee retain Windows x64 layout");
        ok &= expect(handle && handle->parameters.size() == 1 && handle->parameters[0].type.element &&
                     handle->parameters[0].type.element->kind == CppTypeKind::Record &&
                     !handle->parameters[0].type.element->sizeBits,
                     "incomplete pointee does not invent a size");
        ok &= expect(widths && widths->returnType.kind == CppTypeKind::SignedInteger &&
                     widths->returnType.sizeBits == 32 && widths->parameters.size() == 1 &&
                     widths->parameters[0].type.sizeBits == 64,
                     "long and long long use Windows x64 sizes");
        const auto* pointerIdentity = functions ? functionByName(*functions, "tools::pointer_identity") : nullptr;
        if (pointerIdentity && pointerIdentity->externalSymbol)
        {
            const auto projected = projectCppSemanticFunction(*pointerIdentity, 0, "Semantic");
            const auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&projected);
            ok &= expect(symbol && symbol->declaredTypeKey == "builtin:int*" && symbol->parameters.size() == 1 &&
                         symbol->parameters[0].typeKey == "builtin:int*",
                         "C++ pointer parameter and return are projected");
            if (symbol)
            {
                nativesharp::frontend::ProjectCompilation compilation;
                compilation.setSourceBufferProvider([](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() == "Main.ns")
                        return "int Main() { int value = 41; int* p = tools.pointer_identity(&value); return *p; }\n";
                    return std::nullopt;
                });
                std::string error;
                const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                const bool ready = compilation.setCppImportedFunctions({*symbol}, error) &&
                    compilation.compileProjectOopMir(path, error);
                if (!ready) std::cerr << "C++ pointer MIR failed: " << error << '\n';
                ok &= expect(ready && compilation.oopMirModel(), "C++ pointer call reaches MIR");
                if (ready && compilation.oopMirModel() && compilation.nameLookup())
                {
                    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                    auto module = lowerer.lower(*compilation.nameLookup(), *compilation.oopMirModel());
                    std::string llvmError;
                    const bool valid = module && module->verify(llvmError);
                    const std::string ir = module ? module->text() : std::string{};
                    const std::string name = "@\"" + *pointerIdentity->externalSymbol + "\"";
                    ok &= expect(valid && ir.find("declare ptr " + name + "(ptr)") != std::string::npos &&
                                 ir.find("call ptr " + name + "(ptr") != std::string::npos,
                                 "C++ pointer call reaches verified LLVM with exact external symbol");
                    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(llvmError);
                    const bool emitted = valid && target && target->emit(*module,
                        std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/cpp-pointer-call.obj",
                        nativesharp::llvm_backend::LlvmCodeGenFileType::Object, llvmError);
                    if (!emitted) std::cerr << "C++ pointer LLVM/object failed: " << lowerer.error() << ' ' << llvmError << '\n';
                    ok &= expect(emitted, "C++ pointer call emits a Windows x64 object");
                }
            }
        }
        else ok &= expect(false, "Clang exposes the C++ pointer identity fixture");
        if (functions)
        {
            std::vector<nativesharp::frontend::NamespaceFunctionSymbol> overloads;
            for (const auto& function : *functions)
                if (function.qualifiedName == "tools::qualify")
                {
                    auto converted = projectCppSemanticFunction(function, 0, "Semantic");
                    if (auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&converted))
                        overloads.push_back(std::move(*symbol));
                }
            for (const int scenario : {0, 1, 2})
            {
                nativesharp::frontend::ProjectCompilation compilation;
                compilation.setSourceBufferProvider([scenario](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() != "Main.ns") return std::nullopt;
                    if (scenario == 0) return "int Main() { int value = 1; return tools.qualify(&value); }\n";
                    if (scenario == 1) return "int Main() { int value = 1; int const* p = &value; return tools.qualify(p); }\n";
                    return "int Main() { return tools.qualify(null); }\n";
                });
                std::string error;
                const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
                const bool ready = overloads.size() == 2 && compilation.setCppImportedFunctions(overloads, error) &&
                    compilation.compileProjectMethodBodies(path, error);
                bool selected = false, ambiguous = false;
                for (const auto& diagnostic : compilation.diagnostics().diagnostics())
                    ambiguous |= diagnostic.message().find("ambiguous namespace function call") != std::string::npos;
                if (ready && compilation.methodBodyModel() && compilation.namespaceFunctionModel())
                    for (const auto& call : compilation.methodBodyModel()->namespaceFunctionCalls())
                        if (call.writtenName == "tools.qualify" && call.selectedFunctionOrdinal &&
                            *call.selectedFunctionOrdinal < compilation.namespaceFunctionModel()->functions().size())
                            selected |= compilation.namespaceFunctionModel()->functions()[*call.selectedFunctionOrdinal]
                                .parameters[0].pointerPointeeReadonly == (scenario == 1);
                ok &= expect(scenario == 2 ? !ready && ambiguous : ready && selected,
                             "C++ pointer overload chooses the proper qualifier and preserves null ambiguity");
            }
            nativesharp::frontend::ProjectCompilation executable;
            executable.setSourceBufferProvider([](const std::filesystem::path& path)
                -> std::optional<std::string> {
                if (path.filename() == "Main.ns")
                    return "int Main() { int value = 1; int const* p = &value; return tools.qualify(&value) * 10 + tools.qualify(p); }\n";
                return std::nullopt;
            });
            std::string error;
            const auto path = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
            const bool ready = overloads.size() == 2 && executable.setCppImportedFunctions(overloads, error) &&
                executable.compileProjectOopMir(path, error);
            if (!ready) std::cerr << "C++ const overload MIR failed: " << error << '\n';
            ok &= expect(ready && executable.oopMirModel(), "both C++ pointer overload calls reach MIR");
            if (ready && executable.oopMirModel() && executable.nameLookup())
            {
                nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                auto module = lowerer.lower(*executable.nameLookup(), *executable.oopMirModel());
                std::string llvmError;
                const bool valid = module && module->verify(llvmError);
                auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(llvmError);
                const bool emitted = valid && target && target->emit(*module,
                    std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/cpp-const-overload-call.obj",
                    nativesharp::llvm_backend::LlvmCodeGenFileType::Object, llvmError);
                if (!emitted) std::cerr << "C++ const overload object failed: " << lowerer.error() << ' ' << llvmError << '\n';
                ok &= expect(emitted, "both C++ pointer overload calls emit a verified object");
            }
        }
        const auto* intBlend = functions ? functionByName(*functions, "tools::blend") : nullptr;
        const auto longMap = widths ? mapCppScalarType(widths->returnType) : std::nullopt;
        const auto intMap = intBlend ? mapCppScalarType(intBlend->parameters[0].type) : std::nullopt;
        const auto wideMap = widths && !widths->parameters.empty()
            ? mapCppScalarType(widths->parameters[0].type) : std::nullopt;
        ok &= expect(longMap && intMap && wideMap && longMap->nativeSharpTypeKey == "builtin:int" &&
                     intMap->nativeSharpTypeKey == "builtin:int" &&
                     longMap->canonicalCppType != intMap->canonicalCppType &&
                     wideMap->nativeSharpTypeKey == "builtin:long",
                     "same-width C++ int and long retain distinct canonical identities");
        ok &= expect(pointer && !mapCppScalarType(pointer->returnType),
                     "scalar mapping does not misclassify pointers");
        CppType unusualFloat;
        unusualFloat.kind = CppTypeKind::Floating;
        unusualFloat.canonicalSpelling = "long double";
        unusualFloat.unqualifiedCanonicalSpelling = "long double";
        unusualFloat.sizeBits = 64;
        ok &= expect(!mapCppScalarType(unusualFloat),
                     "equal width alone does not establish floating-point representation");
        unusualFloat.ieeeBinary64 = true;
        const auto binary64LongDouble = mapCppScalarType(unusualFloat);
        ok &= expect(binary64LongDouble && binary64LongDouble->nativeSharpTypeKey == "builtin:double",
                     "Clang-confirmed IEEE binary64 long double maps to the double carrier");
        for (const auto& [name, key] : std::vector<std::pair<const char*, const char*>>{
                 {"scalars::v", "builtin:void"}, {"scalars::b", "builtin:bool"},
                 {"scalars::i8", "builtin:sbyte"}, {"scalars::u8", "builtin:byte"},
                 {"scalars::i16", "builtin:short"}, {"scalars::u16", "builtin:ushort"},
                 {"scalars::u32", "builtin:uint"}, {"scalars::u64", "builtin:ulong"},
                 {"scalars::f32", "builtin:float"}, {"scalars::f64", "builtin:double"}})
        {
            const auto* function = functions ? functionByName(*functions, name) : nullptr;
            const auto mapping = function ? mapCppScalarType(function->returnType) : std::nullopt;
            if (!mapping || mapping->nativeSharpTypeKey != key)
                std::cerr << "Unexpected scalar mapping for " << name << '\n';
            ok &= expect(mapping && mapping->nativeSharpTypeKey == key,
                         "Clang scalar representation maps to the matching NativeSharp key");
        }
        std::vector<std::string> symbols;
        if (functions)
            for (const auto& function : *functions)
                if (function.qualifiedName == "tools::blend" && function.externalSymbol)
                    symbols.push_back(*function.externalSymbol);
        ok &= expect(symbols.size() == 2 && symbols[0] != symbols[1],
                     "C++ overloads have distinct Clang symbols");
        ok &= expect(std::find(symbols.begin(), symbols.end(), "?blend@tools@@YAHH@Z") != symbols.end() &&
                     std::find(symbols.begin(), symbols.end(), "?blend@tools@@YANN@Z") != symbols.end(),
                     "overload symbols match the Windows x64 Clang object fixture");
        if (functions)
        {
            std::vector<nativesharp::frontend::NamespaceFunctionSymbol> projected;
            for (const auto& function : *functions)
            {
                if (function.qualifiedName != "tools::blend" || !function.externalSymbol ||
                    function.parameters.size() != 1) continue;
                auto converted = projectCppSemanticFunction(function, 0, "Semantic");
                auto* symbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&converted);
                ok &= expect(symbol && symbol->namespaceName == "tools" &&
                             symbol->mangledName == *function.externalSymbol,
                             "C++ overload converts with namespace and Clang symbol");
                if (symbol) projected.push_back(std::move(*symbol));
            }
            const auto* constPointer = functionByName(*functions, "tools::pointer");
            const auto constPointerProjection = constPointer
                ? projectCppSemanticFunction(*constPointer, 0, "Semantic")
                : CppSemanticFunctionResult{CppSemanticFunctionFailure{"missing const pointer"}};
            const auto* constPointerSymbol = std::get_if<nativesharp::frontend::NamespaceFunctionSymbol>(&constPointerProjection);
            ok &= expect(constPointerSymbol && constPointerSymbol->declaredTypeKey == "builtin:int*" &&
                         constPointerSymbol->cppReturnType && constPointerSymbol->cppReturnType->element &&
                         constPointerSymbol->cppReturnType->element->isConst,
                         "const pointer return projects while retaining Clang pointee qualification");
            for (const auto* name : {"tools::local", "tools::handle"})
            {
                const auto* function = functionByName(*functions, name);
                ok &= expect(function && std::holds_alternative<CppSemanticFunctionFailure>(
                                 projectCppSemanticFunction(*function, 0, "Semantic")),
                             "unsupported or internal C++ function is rejected explicitly");
            }
            nativesharp::frontend::ProjectCompilation compilation;
            std::string error;
            const auto projectPath = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/Semantic.nsproj";
            {
                nativesharp::frontend::ProjectCompilation cppBodies;
                cppBodies.setSourceBufferProvider([](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() == "Main.ns") return "int Main() { return tools.blend(1); }\n";
                    return std::nullopt;
                });
                const bool ready = cppBodies.setCppImportedFunctions(projected, error) &&
                    cppBodies.compileProjectMethodBodies(projectPath, error);
                if (!ready)
                    for (const auto& diagnostic : cppBodies.diagnostics().diagnostics())
                        std::cerr << "C++ overload body diagnostic: " << diagnostic.message() << '\n';
                ok &= expect(ready && !cppBodies.diagnostics().hasErrors(),
                             "C++ overload call is analyzed as an external declaration");
            }
            {
                nativesharp::frontend::ProjectCompilation hir;
                hir.setSourceBufferProvider([](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() == "Main.ns") return "int Main() { return tools.blend(1); }\n";
                    return std::nullopt;
                });
                const bool ready = hir.setCppImportedFunctions(projected, error) &&
                    hir.compileProjectOopHir(projectPath, error);
                bool found = false;
                if (ready && hir.oopHirModel())
                    for (const auto& function : hir.oopHirModel()->functions())
                        found |= function.isImportedC && function.mangledName == "?blend@tools@@YAHH@Z" &&
                            function.cAbiName == "?blend@tools@@YAHH@Z";
                if (!ready) std::cerr << "Clang C++ HIR setup failed: " << error << '\n';
                ok &= expect(found, "Clang C++ mangled external symbol reaches HIR as a declaration");
                nativesharp::frontend::ProjectCompilation mir;
                mir.setSourceBufferProvider([](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() == "Main.ns") return "int Main() { return tools.blend(1); }\n";
                    return std::nullopt;
                });
                const bool mirReady = mir.setCppImportedFunctions(projected, error) &&
                    mir.compileProjectOopMir(projectPath, error);
                bool mirFound = false;
                if (mirReady && mir.oopMirModel())
                    for (const auto& function : mir.oopMirModel()->functions())
                        mirFound |= function.isImportedC && function.mangledName == "?blend@tools@@YAHH@Z" &&
                            function.cAbiName == "?blend@tools@@YAHH@Z";
                if (!mirReady) std::cerr << "Clang C++ MIR setup failed: " << error << '\n';
                ok &= expect(mirFound, "Clang C++ mangled external symbol reaches MIR as a declaration");
                if (mirFound && mir.nameLookup())
                {
                    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
                    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
                    std::string verifyError;
                    const bool valid = module && module->verify(verifyError);
                    const std::string ir = module ? module->text() : std::string{};
                    if (!valid) std::cerr << "Clang C++ LLVM failed: " << lowerer.error() << ' ' << verifyError << '\n';
                    ok &= expect(valid && ir.find("declare i32 @\"?blend@tools@@YAHH@Z\"(i32)") != std::string::npos &&
                                 ir.find("call i32 @\"?blend@tools@@YAHH@Z\"(") != std::string::npos,
                                 "Clang C++ external declaration and call reach verified LLVM");
                    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(verifyError);
                    const bool emitted = valid && target && target->emit(*module,
                        std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/cpp-call.obj",
                        nativesharp::llvm_backend::LlvmCodeGenFileType::Object, verifyError);
                    if (!emitted) std::cerr << "Clang C++ object failed: " << verifyError << '\n';
                    ok &= expect(emitted, "Clang C++ call emits a Windows x64 object");
                }
            }
            {
                nativesharp::frontend::ProjectCompilation valueCompilation;
                valueCompilation.setSourceBufferProvider([](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() == "Main.ns")
                        return "delegate int Transform(int value); int Main() { Transform t = tools.blend; return 0; }\n";
                    return std::nullopt;
                });
                const bool ready = valueCompilation.setCppImportedFunctions(projected, error) &&
                    valueCompilation.compileProjectMembers(projectPath, error);
                nativesharp::frontend::ProjectNamespaceFunctionModel observer;
                const bool observed = ready && observer.setCaptureCppFunctionDemands(true) &&
                    observer.setCppImportedFunctions(projected, error) &&
                    observer.build(*valueCompilation.nameLookup(), *valueCompilation.memberModel(),
                        valueCompilation.units(), valueCompilation.diagnostics(), valueCompilation.genericModel());
                bool valueDemand = false;
                if (observed)
                    for (const auto& demand : observer.cppFunctionDemands())
                        valueDemand |= !demand.directCall && demand.writtenName == "tools.blend" && demand.syntax &&
                            demand.lookupName == "blend" && demand.lookupNamespaces.size() == 1 &&
                            demand.lookupNamespaces[0] == "tools";
                ok &= expect(valueDemand, "demand capture distinguishes a function value from a direct call");
            }
            {
                nativesharp::frontend::ProjectCompilation valueProviderCompilation;
                valueProviderCompilation.setSourceBufferProvider([](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() == "Main.ns")
                        return "delegate int Transform(int value); int Main() { Transform t = tools.blend; return 0; }\n";
                    return std::nullopt;
                });
                bool sawValue = false;
                const bool configured = valueProviderCompilation.setCppFunctionProvider(
                    [&sawValue, &projected](const auto&, auto, auto, auto demands, auto& output, auto&, auto&) {
                        for (const auto& demand : demands)
                            sawValue |= !demand.directCall && demand.writtenName == "tools.blend" &&
                                demand.projectOrdinal == 0 && demand.syntax;
                        output = projected;
                        return true;
                    }, error);
                const bool ready = configured && valueProviderCompilation.compileProjectMembers(projectPath, error);
                ok &= expect(ready && sawValue, "provider receives function-value demands before binding");
            }
            for (const auto& [source, expected] : std::vector<std::pair<std::string, std::string>>{
                     {"using tools; int Main() { return blend(1); }\n", "blend"},
                     {"using T = tools; int Main() { return T.blend(1); }\n", "T.blend"}})
            {
                nativesharp::frontend::ProjectCompilation usingCompilation;
                usingCompilation.setSourceBufferProvider([source](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() == "Main.ns") return source;
                    return std::nullopt;
                });
                const bool configured = usingCompilation.setCppImportedFunctions(projected, error);
                const bool usingReady = configured && usingCompilation.compileProjectMembers(projectPath, error);
                bool bound = false;
                if (usingReady && usingCompilation.namespaceFunctionModel())
                    for (const auto& binding : usingCompilation.namespaceFunctionModel()->referenceBindings())
                        if (binding.writtenName == expected && binding.candidateFunctionOrdinals.size() == 2)
                            bound = true;
                ok &= expect(bound, "NativeSharp using directive resolves an imported C++ namespace");
                if (usingReady && usingCompilation.nameLookup() && usingCompilation.memberModel())
                {
                    nativesharp::frontend::ProjectNamespaceFunctionModel observer;
                    const bool observed = observer.setCaptureCppFunctionDemands(true) &&
                        observer.setCppImportedFunctions(projected, error) &&
                        observer.build(*usingCompilation.nameLookup(), *usingCompilation.memberModel(),
                            usingCompilation.units(), usingCompilation.diagnostics(), usingCompilation.genericModel());
                    bool demandFound = false;
                    if (observed)
                        for (const auto& demand : observer.cppFunctionDemands())
                            if (demand.directCall && demand.projectOrdinal == 0 && demand.syntax &&
                                demand.writtenName == expected && demand.lexicalNamespace.empty())
                            {
                                if (expected == "blend")
                                    demandFound = std::find(demand.importedNamespaces.begin(),
                                        demand.importedNamespaces.end(), "tools") != demand.importedNamespaces.end();
                                else
                                {
                                    const auto alias = demand.aliases.find("T");
                                    demandFound = alias != demand.aliases.end() && alias->second == "tools";
                                }
                            }
                    ok &= expect(demandFound && !observer.setCaptureCppFunctionDemands(false) &&
                                 usingCompilation.namespaceFunctionModel()->cppFunctionDemands().empty(),
                                 "optional demand capture preserves using and alias context without changing defaults");
                }
            }
            for (const auto& [source, expected] : std::vector<std::pair<std::string, std::string>>{
                     {"using tools; int Main() { return blend(1); }\n", "blend"},
                     {"using T = tools; int Main() { return T.blend(1); }\n", "T.blend"},
                     {"int Main() { return tools.blend(1); }\n", "tools.blend"}})
            {
                nativesharp::frontend::ProjectCompilation automatic;
                automatic.setSourceBufferProvider([source](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() == "Main.ns") return source;
                    return std::nullopt;
                });
                CppProjectSessions group;
                group.projectFile = std::filesystem::absolute(projectPath);
                group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
                    "namespace tools { int blend(int); double blend(double); }\n")));
                std::vector<CppProjectSessions> sessions;
                sessions.push_back(std::move(group));
                bool namespaceProviderCalled = false;
                auto namespaceProvider = [&namespaceProviderCalled](const auto& graph, auto projectSessions,
                                                                    auto& output, auto& why) {
                    namespaceProviderCalled = true;
                    auto inventory = collectCppNamespaces(graph, projectSessions);
                    if (const auto* failure = std::get_if<CppNamespaceInventoryFailure>(&inventory))
                    { why = failure->message; return false; }
                    auto names = visibleCppNamespaces(std::get<std::vector<CppNamespaceInventoryEntry>>(inventory));
                    if (const auto* failure = std::get_if<CppNamespaceInventoryFailure>(&names))
                    { why = failure->message; return false; }
                    auto& extracted = std::get<std::vector<std::pair<std::size_t, std::string>>>(names);
                    output.insert(output.end(), extracted.begin(), extracted.end());
                    return true;
                };
                bool sawDemand = false;
                auto functionProvider = [&sawDemand, &expected](const auto& graph, auto projectSessions, auto, auto demands, auto& output, auto& deferred, auto& why) {
                    for (const auto& demand : demands)
                        if (demand.directCall && demand.writtenName == expected && demand.syntax)
                        {
                            if (demand.lookupName != "blend" ||
                                std::find(demand.lookupNamespaces.begin(), demand.lookupNamespaces.end(), "tools") ==
                                    demand.lookupNamespaces.end()) continue;
                            if (expected == "blend")
                                sawDemand |= std::find(demand.importedNamespaces.begin(),
                                    demand.importedNamespaces.end(), "tools") != demand.importedNamespaces.end();
                            else if (expected == "T.blend")
                            {
                                const auto alias = demand.aliases.find("T");
                                sawDemand |= alias != demand.aliases.end() && alias->second == "tools";
                            }
                            else sawDemand = true;
                        }
                    return provideCppSemanticFunctions(graph, projectSessions, demands, output, deferred, why);
                };
                const bool configured = automatic.setCppProjectSessions(std::move(sessions), error) &&
                    automatic.setCppNamespaceProvider(namespaceProvider, error) &&
                    automatic.setCppFunctionProvider(functionProvider, error);
                const bool ready = configured && automatic.compileProjectMembers(projectPath, error);
                bool bound = false;
                if (ready && automatic.namespaceFunctionModel())
                    for (const auto& binding : automatic.namespaceFunctionModel()->referenceBindings())
                        bound |= binding.writtenName == expected && binding.candidateFunctionOrdinals.size() == 2;
                ok &= expect(ready && namespaceProviderCalled && sawDemand && bound &&
                             !automatic.setCppNamespaceProvider({}, error),
                             "Clang sessions provide namespaces before using and functions before binding");
            }
            {
                nativesharp::frontend::ProjectCompilation failedNamespaceProvider;
                ok &= expect(failedNamespaceProvider.setCppNamespaceProvider(
                    [](const auto&, auto, auto&, auto& why) {
                        why = "namespace provider failure"; return false;
                    }, error), "namespace provider installs before compilation");
                error.clear();
                ok &= expect(!failedNamespaceProvider.compileProjectNames(projectPath, error) &&
                             error == "namespace provider failure",
                             "namespace provider failure stops name binding with its diagnostic");
            }
            if (!projected.empty())
            {
                auto wrongOwner = projected.front();
                wrongOwner.moduleName = "WrongModule";
                nativesharp::frontend::ProjectCompilation wrongCompilation;
                ok &= expect(wrongCompilation.setCppImportedFunctions({wrongOwner}, error) &&
                             !wrongCompilation.compileProjectNames(projectPath, error) &&
                             error.find("invalid project owner") != std::string::npos,
                             "imported namespace cannot be installed for the wrong module");
            }
            {
                nativesharp::frontend::ProjectCompilation unknownNamespace;
                unknownNamespace.setSourceBufferProvider([](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() == "Main.ns") return "using nonexistent; int Main() { return 0; }\n";
                    return std::nullopt;
                });
                error.clear();
                ok &= expect(!unknownNamespace.compileProjectNames(projectPath, error) &&
                             unknownNamespace.diagnostics().hasErrors(),
                             "unimported namespaces remain invalid in ordinary projects");
            }
            {
                nativesharp::frontend::ProjectCompilation qualifiedCompilation;
                qualifiedCompilation.setSourceBufferProvider([](const std::filesystem::path& path)
                    -> std::optional<std::string> {
                    if (path.filename() == "Main.ns") return "int Main() { return tools.blend(1); }\n";
                    return std::nullopt;
                });
                const bool configured = qualifiedCompilation.setCppImportedFunctions(projected, error);
                const bool qualifiedReady = configured && qualifiedCompilation.compileProjectMembers(projectPath, error);
                const auto* qualifiedModel = qualifiedCompilation.namespaceFunctionModel();
                bool bound = false;
                if (qualifiedReady && qualifiedModel)
                    for (const auto& binding : qualifiedModel->referenceBindings())
                        if (binding.writtenName == "tools.blend" && binding.candidateFunctionOrdinals.size() == 2)
                            bound = true;
                ok &= expect(bound, "qualified NativeSharp call finds the imported C++ namespace and overloads");
                if (qualifiedReady && qualifiedCompilation.nameLookup() && qualifiedCompilation.memberModel())
                {
                    nativesharp::frontend::ProjectNamespaceFunctionModel observer;
                    const bool observed = observer.setCaptureCppFunctionDemands(true) &&
                        observer.setCppImportedFunctions(projected, error) &&
                        observer.build(*qualifiedCompilation.nameLookup(), *qualifiedCompilation.memberModel(),
                            qualifiedCompilation.units(), qualifiedCompilation.diagnostics(), qualifiedCompilation.genericModel());
                    bool qualifiedDemand = false;
                    if (observed)
                        for (const auto& demand : observer.cppFunctionDemands())
                            qualifiedDemand |= demand.directCall && demand.writtenName == "tools.blend" &&
                                demand.projectOrdinal == 0 && demand.syntax != nullptr;
                    ok &= expect(qualifiedDemand, "demand capture includes qualified function calls");
                }
            }
            const bool ready = compilation.compileProjectMembers(projectPath, error);
            ok &= expect(ready && projected.size() == 2, "C++ overload test has native model and both projections");
            if (ready && projected.size() == 2)
            {
                nativesharp::frontend::ProjectNamespaceFunctionModel model;
                ok &= expect(model.setCppImportedFunctions(std::move(projected), error) &&
                             model.build(*compilation.nameLookup(), *compilation.memberModel(), compilation.units(),
                                         compilation.diagnostics(), compilation.genericModel()),
                             "semantic index accepts C++ overload projections");
                const auto overloads = model.findFunctions(0, "tools", "blend");
                ok &= expect(overloads.size() == 2 &&
                             model.functions()[overloads[0]].cppCanonicalIdentity !=
                                 model.functions()[overloads[1]].cppCanonicalIdentity,
                             "semantic index preserves distinct Clang overload identities");
            }
        }
    }

    {
        const auto header = std::filesystem::path(argv[2]) / "cpp-fixtures/semantic/equivalence.h";
        std::filesystem::create_directories(header.parent_path());
        { std::ofstream file(header); file <<
            "struct Shared {\n#if FIELD_FLOAT\nfloat value;\n#else\nint value;\n#endif\n};\n"; }
        auto makeOptions = [&](bool fieldFloat, std::string standard = "c++20") {
            auto value = options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp, "");
            value.input = CppFileInput{header};
            value.languageStandard = std::move(standard);
            value.macros.push_back({CppMacroAction::Define, fieldFloat ? "FIELD_FLOAT=1" : "FIELD_FLOAT=0"});
            return value;
        };
        auto first = open(factory, makeOptions(false));
        auto same = open(factory, makeOptions(false));
        auto different = open(factory, makeOptions(true));
        auto differentStandard = open(factory, makeOptions(false, "c++17"));
        ok &= expect(first && same && different && differentStandard,
                     "structural comparison fixture opens independent sessions");
        if (first && same && different && differentStandard)
        {
            ok &= expect(first->structurallyEquivalentRecord(*same, "Shared") == std::optional<bool>(true) &&
                         first->structurallyEquivalentRecord(*first, "Shared") == std::optional<bool>(true),
                         "Clang structurally compares repeated definitions including the same session");
            ok &= expect(first->structurallyEquivalentRecord(*different, "Shared") == std::optional<bool>(false),
                         "same header and layout with different macro types are not structurally equivalent");
            ok &= expect(!first->structurallyEquivalentRecord(*same, "Missing") &&
                         !first->structurallyEquivalentRecord(*differentStandard, "Shared"),
                         "missing records and incompatible language standards are not compared");
            same->close();
            ok &= expect(!first->structurallyEquivalentRecord(*same, "Shared"),
                         "closed session cannot participate in structural comparison");
        }
    }
    auto fromFileOptions = options(argv[1], argv[2], argv[3], CppLanguageMode::C, "");
    fromFileOptions.input = CppFileInput{"cpp-fixtures/probe.c"};
    fromFileOptions.includePaths.push_back({"cpp-fixtures", CppIncludeKind::User});
    fromFileOptions.macros.push_back({CppMacroAction::Define, "SESSION_FLAG=7"});
    fromFileOptions.macros.push_back({CppMacroAction::Undefine, "SESSION_FLAG"});
    fromFileOptions.macros.push_back({CppMacroAction::Define, "SESSION_FLAG=9"});
    auto fromFile = open(factory, std::move(fromFileOptions));
    ok &= expect(static_cast<bool>(fromFile), "file input, relative include and ordered macros work");
    if (fromFile)
    {
        ok &= expect(fromFile->optionsSnapshot().macros.size() == 3, "session retains its options");
        ok &= expect(fromFile->microsoftVolatileSemantics().has_value(),
                     "live Clang session exposes its effective MS volatile option");
    }

    const std::string usrSource =
        "typedef int Count; enum Color { Red, Blue }; typedef enum Color ColorAlias; "
        "struct Point { int x; int y; }; typedef struct Point PointAlias; "
        "extern int counter; int measure(struct Point*); enum Color paint(enum Color); int other(void);";
    auto usrFirst = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::C, usrSource));
    auto usrSecond = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::C, usrSource));
    ok &= expect(usrFirst && usrSecond, "independent sessions open for Clang declaration identities");
    if (usrFirst && usrSecond)
    {
        const auto functions = usrFirst->freeFunctionsSnapshot();
        const auto functionsAgain = usrSecond->freeFunctionsSnapshot();
        const auto globals = usrFirst->globalsSnapshot();
        const auto records = usrFirst->recordsSnapshot();
        const auto enums = usrFirst->enumsSnapshot();
        const auto aliases = usrFirst->typeAliasesSnapshot();
        const auto* measure = functions ? functionByName(*functions, "measure") : nullptr;
        const auto* measureAgain = functionsAgain ? functionByName(*functionsAgain, "measure") : nullptr;
        const auto* other = functions ? functionByName(*functions, "other") : nullptr;
        const auto* paint = functions ? functionByName(*functions, "paint") : nullptr;
        const auto* global = globals ? globalByName(*globals, "counter") : nullptr;
        const auto* record = records ? recordByName(*records, "Point") : nullptr;
        ok &= expect(measure && measureAgain && measure->clangUsr &&
                     measure->clangUsr == measureAgain->clangUsr && other && other->clangUsr &&
                     measure->clangUsr != other->clangUsr,
                     "Clang function USRs persist across sessions and distinguish names");
        ok &= expect(global && global->clangUsr && record && record->clangUsr &&
                     record->fields.size() == 2 && record->fields[0].clangUsr &&
                     record->fields[1].clangUsr && record->fields[0].clangUsr != record->fields[1].clangUsr &&
                     enums && !enums->empty() && enums->front().clangUsr &&
                     enums->front().enumerators.size() == 2 && enums->front().enumerators[0].clangUsr &&
                     enums->front().enumerators[1].clangUsr &&
                     enums->front().enumerators[0].clangUsr != enums->front().enumerators[1].clangUsr &&
                     aliases && !aliases->empty() && aliases->front().clangUsr,
                     "C global, record, fields, enum, enumerators and alias preserve Clang USRs");
        ok &= expect(measure && measure->parameters.size() == 1 && measure->parameters[0].type.element &&
                     measure->parameters[0].type.element->recordIdentity && record && record->clangUsr &&
                     measure->parameters[0].type.element->recordIdentity->clangUsr == record->clangUsr &&
                     paint && paint->returnType.enumIdentity && enums && !enums->empty() &&
                     enums->front().clangUsr && paint->returnType.enumIdentity->clangUsr == enums->front().clangUsr &&
                     paint->parameters.size() == 1 && paint->parameters[0].type.enumIdentity &&
                     paint->parameters[0].type.enumIdentity->clangUsr == enums->front().clangUsr,
                     "C function signatures preserve the USRs of referenced record and enum declarations");
        bool recordAliasUsr = false, enumAliasUsr = false;
        if (aliases)
            for (const auto& alias : *aliases)
            {
                if (alias.name == "PointAlias")
                {
                    const auto mapped = nativesharp::frontend::cppAliasRecordType(alias.underlyingType);
                    recordAliasUsr = mapped && record && record->clangUsr && mapped->clangUsr == record->clangUsr;
                }
                if (alias.name == "ColorAlias")
                {
                    const auto mapped = nativesharp::frontend::cppAliasEnumType(alias.underlyingType);
                    enumAliasUsr = mapped && enums && !enums->empty() && enums->front().clangUsr &&
                        mapped->clangUsr == enums->front().clangUsr;
                }
            }
        ok &= expect(recordAliasUsr && enumAliasUsr, "C aliases retain USRs of their referenced record and enum");
        usrFirst->close();
        ok &= expect(measure && measure->clangUsr && record && record->clangUsr,
                     "copied USRs remain available after closing their session");
    }

    auto usrCpp = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
        "namespace UsrNS { struct Thing { int x; static int data; void work(int); void work(double); }; }"));
    ok &= expect(static_cast<bool>(usrCpp), "C++ session opens for declaration identity tests");
    if (usrCpp)
    {
        const auto records = usrCpp->recordsSnapshot();
        const auto namespaces = usrCpp->namespacesSnapshot();
        const auto* record = records ? recordByName(*records, "UsrNS::Thing") : nullptr;
        const auto ns = namespaces ? std::find_if(namespaces->begin(), namespaces->end(),
            [](const auto& value) { return value.qualifiedName == "UsrNS"; }) :
            std::vector<CppNamespaceDeclaration>::const_iterator{};
        ok &= expect(record && record->clangUsr && record->fields.size() == 1 &&
                     record->fields[0].clangUsr && record->staticFields.size() == 1 &&
                     record->staticFields[0].clangUsr && record->methods.size() == 2 &&
                     record->methods[0].clangUsr && record->methods[1].clangUsr &&
                     record->methods[0].clangUsr != record->methods[1].clangUsr &&
                     namespaces && ns != namespaces->end() && ns->clangUsr,
                     "Clang USRs distinguish overloaded C++ methods and identify their members and namespace");
    }

    auto alignedSession = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::C,
        "__declspec(align(16)) extern volatile int aligned_global; extern int incomplete_global[];\n"));
    ok &= expect(static_cast<bool>(alignedSession), "Clang opens aligned and incomplete global declarations");
    if (alignedSession)
    {
        const auto globals = alignedSession->globalsSnapshot();
        const auto* aligned = globals ? globalByName(*globals, "aligned_global") : nullptr;
        const auto* incomplete = globals ? globalByName(*globals, "incomplete_global") : nullptr;
        ok &= expect(aligned && aligned->declarationAlignmentBits == std::optional<std::uint64_t>{128} &&
                     incomplete && !incomplete->declarationAlignmentBits.has_value(),
                     "global snapshots preserve explicit declaration alignment and unknown incomplete alignment");
    }

    auto firstOptions = options(argv[1], argv[2], argv[3], CppLanguageMode::C,
        "#if SESSION_PICK == 1\nint first_session(void);\n#else\nint second_session(void);\n#endif\n");
    auto secondOptions = firstOptions;
    firstOptions.macros.push_back({CppMacroAction::Define, "SESSION_PICK=1"});
    secondOptions.macros.push_back({CppMacroAction::Define, "SESSION_PICK=2"});
    auto firstSession = open(factory, std::move(firstOptions));
    auto secondSession = open(factory, std::move(secondOptions));
    ok &= expect(firstSession && secondSession, "independent macro sessions open");
    if (firstSession && secondSession)
    {
        const auto firstFunctions = firstSession->freeFunctionsSnapshot();
        const auto secondFunctions = secondSession->freeFunctionsSnapshot();
        ok &= expect(firstFunctions && secondFunctions && functionByName(*firstFunctions, "first_session") &&
                     !functionByName(*firstFunctions, "second_session") &&
                     functionByName(*secondFunctions, "second_session") &&
                     !functionByName(*secondFunctions, "first_session"),
                     "sessions independently apply macros while both ASTs remain alive");
        firstSession->close();
        ok &= expect(secondSession->freeFunctionsSnapshot().has_value(),
                     "closing the first macro session leaves the second available");
    }

    auto withWarning = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::C,
        "#warning session_warning_marker\nint warning_probe(void);\n"));
    ok &= expect(static_cast<bool>(withWarning), "warning does not prevent opening a session");
    if (withWarning)
    {
        const auto diagnostics = withWarning->diagnosticsSnapshot();
        const auto warning = std::find_if(diagnostics.begin(), diagnostics.end(), [](const auto& d) {
            return d.severity == CppSessionDiagnosticSeverity::Warning &&
                   d.message.find("session_warning_marker") != std::string::npos;
        });
        ok &= expect(warning != diagnostics.end() && warning->location && warning->location->line == 1 &&
                     warning->location->column > 0 && warning->originalId != 0,
                     "warning preserves severity, identifier and source location");
        withWarning->close();
        ok &= expect(!diagnostics.empty() && diagnostics.front().message.find("session_warning_marker") != std::string::npos,
                     "copied warning remains valid after session close");
    }

    auto duplicate = factory.open(options(argv[1], argv[2], argv[3], CppLanguageMode::C,
        "int duplicate_name;\ndouble duplicate_name;\n"), {});
    const auto* duplicateFailure = std::get_if<CppSessionOpenFailure>(&duplicate);
    bool errorWithNote = false;
    if (duplicateFailure)
        for (const auto& diagnostic : duplicateFailure->diagnostics)
            if (diagnostic.severity == CppSessionDiagnosticSeverity::Error && diagnostic.location &&
                diagnostic.location->line == 2)
                for (const auto& note : diagnostic.notes)
                    errorWithNote |= note.severity == CppSessionDiagnosticSeverity::Note &&
                                     note.location && note.location->line == 1;
    ok &= expect(duplicateFailure && duplicateFailure->phase == "parse" && errorWithNote,
                 "error preserves its related note and both locations");

    auto invalid = factory.open(options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
                                        "class Broken { public: int ;\n"), {});
    auto* invalidFailure = std::get_if<CppSessionOpenFailure>(&invalid);
    ok &= expect(invalidFailure && invalidFailure->phase == "parse" &&
                 !invalidFailure->diagnostics.empty(), "invalid C++ returns diagnostics");
    if (invalidFailure && !invalidFailure->diagnostics.empty())
        ok &= expect(invalidFailure->diagnostics.front().location.has_value(), "diagnostic has a location");

    auto missing = options(argv[1], argv[2], argv[3], CppLanguageMode::C, "int x;\n");
    missing.resourceDirectory /= "not-a-resource-directory";
    auto missingResult = factory.open(std::move(missing), {});
    auto* missingFailure = std::get_if<CppSessionOpenFailure>(&missingResult);
    ok &= expect(missingFailure && missingFailure->phase == "options", "missing resources are rejected");

    CppCancellationToken token;
    token.cancel();
    auto cancelled = factory.open(options(argv[1], argv[2], argv[3], CppLanguageMode::C, "int x;\n"), token);
    auto* cancelledFailure = std::get_if<CppSessionOpenFailure>(&cancelled);
    ok &= expect(cancelledFailure && cancelledFailure->phase == "cancel", "cancelled opening is rejected");
    if (c)
    {
        ok &= expect(c->close().succeeded && c->close().succeeded, "close is idempotent");
        ok &= expect(c->state() == CppSessionState::Closed && !c->topLevelDeclarationCount(),
                     "closed session does not expose an AST");
        ok &= expect(!c->microsoftVolatileSemantics(),
                     "closed Clang session does not expose an effective volatile option");
        ok &= expect(!c->typeAliasesSnapshot() && cAliases && !cAliases->empty(),
                     "copied C typedef snapshots survive closing the Clang session");
        ok &= expect(!c->enumsSnapshot() && cEnums && !cEnums->empty(),
                     "copied C enum snapshots survive closing the Clang session");
        ok &= expect(!c->freeFunctionsSnapshot() && !c->recordsSnapshot() && !c->globalsSnapshot() && cFunctions &&
                     functionByName(*cFunctions, "bridge_probe") && cRecords &&
                     recordByName(*cRecords, "Pair") && cGlobals && globalByName(*cGlobals, "c_global"),
                     "closed session cannot query AST; copied declarations remain valid");
    }
    if (cpp)
    {
        ok &= expect(cpp->topLevelDeclarationCount().has_value(), "closing C session keeps C++ session alive");
        nativesharp::frontend::ProjectCompilation compilation;
        std::string error;
        auto* original = cpp.get();
        ok &= expect(compilation.setCppInteropSession(std::move(cpp), error),
                     "real Clang session is accepted by ProjectCompilation");
        ok &= expect(compilation.cppInteropSession() == original &&
                     compilation.cppInteropSession()->topLevelDeclarationCount().has_value(),
                     "injected Clang AST remains accessible");
    }
    {
        auto session = open(factory, options(argv[1], argv[2], argv[3], CppLanguageMode::Cpp,
                                             "struct SnapshotLifetime {};\n"));
        auto compilation = std::make_shared<nativesharp::frontend::ProjectCompilation>();
        std::string error;
        const bool injected = session && compilation->setCppInteropSession(std::move(session), error);
        std::weak_ptr<nativesharp::frontend::ProjectCompilation> weak = compilation;
        auto snapshotOwner = compilation;
        compilation.reset();
        ok &= expect(injected && !weak.expired() && snapshotOwner->cppInteropSession() &&
                     snapshotOwner->cppInteropSession()->topLevelDeclarationCount().has_value(),
                     "shared snapshot ownership preserves its compilation and real Clang session");
        snapshotOwner.reset();
        ok &= expect(weak.expired(), "compilation and owned session can be released after the last snapshot");
    }
    if (argc == 5 && std::string_view(argv[4]) != "bundle-link" && std::string_view(argv[4]) != "lifetime")
    {
        const std::filesystem::path project = std::filesystem::absolute(argv[4]);
        nativesharp::compilation::ProjectLoadOptions loadOptions;
        std::string error;
        nativesharp::compilation::ProjectGraphLoader loader(1, {}, loadOptions);
        auto graph = loader.load(project, error);
        nativesharp::frontend::ProjectCompilation compilation;
        std::optional<CppRecordObservationRunResult> observedInFrontend;
        int observerCalls = 0;
        const bool observerConfigured = compilation.setCppRecordObserver([&](const auto& current) {
            ++observerCalls;
            observedInFrontend = observeCppRecords(current);
        }, error);
        std::vector<std::string> snapshotReports;
        compilation.setReportSink([&snapshotReports](std::string_view line) { snapshotReports.emplace_back(line); });
        CppProjectSessions group;
        group.projectFile = project;
        group.sessions.push_back(open(factory, options(argv[1], argv[2], argv[3],
            CppLanguageMode::Cpp, "namespace NativeProbe { struct Unused {}; }\n")));
        std::vector<CppProjectSessions> groups;
        groups.push_back(std::move(group));
        const bool ready = observerConfigured && graph && compilation.setCppProjectSessions(std::move(groups), error) &&
            nativesharp::compilation::preparePrecompiledProject(std::move(*graph), loadOptions, compilation, error) &&
            compilation.compileProjectNames(project, error);
        const auto observed = ready && observedInFrontend ? std::move(*observedInFrontend) :
            CppRecordObservationRunResult{CppRecordObservationFailure{error}};
        const auto* result = std::get_if<CppRecordObservation>(&observed);
        bool installed = ready && !compilation.precompiledSnapshots().empty() && compilation.nameLookup() &&
            compilation.nameLookup()->installedSnapshotUnitCount() > 0;
        const auto sessionsAfterSnapshot = compilation.cppProjectSessions();
        const bool sessionAliveWithSnapshot = installed && sessionsAfterSnapshot.size() == 1 &&
            sessionsAfterSnapshot[0].sessions.size() == 1 && sessionsAfterSnapshot[0].sessions[0] &&
            sessionsAfterSnapshot[0].sessions[0]->topLevelDeclarationCount().has_value();
        ok &= expect(sessionAliveWithSnapshot,
                     "a real C++ session remains usable while a published semantic snapshot is installed");
        bool resolved = false;
        if (installed)
            for (const auto& binding : compilation.nameLookup()->bindings())
                resolved |= binding.writtenName == "Published.Value" && binding.targetTypeOrdinal.has_value();
        if (!result)
            if (const auto* failure = std::get_if<CppRecordObservationFailure>(&observed))
                std::cerr << "published snapshot observation: " << failure->message << '\n';
        if (!(installed && resolved && result))
        {
            std::cerr << "published snapshot ready=" << ready << " installed=" << installed
                      << " resolved=" << resolved << " snapshot-count=" << compilation.precompiledSnapshots().size()
                      << " error=" << error << '\n';
            for (const auto& report : snapshotReports) std::cerr << report << '\n';
            for (const auto& item : compilation.precompiledModules())
                std::cerr << "module " << item.moduleName << " sidecar=" << item.productSidecarPath.string()
                          << " snapshot-bytes=" << item.publicApiSnapshot.size() << '\n';
            if (compilation.projectGraph() && compilation.projectGraph()->rootProject())
                for (const auto& ref : compilation.projectGraph()->rootProject()->libraryReferences())
                {
                    std::cerr << "reference sidecar=" << ref.productSidecarPath.string() << '\n';
                    const auto* meta = compilation.libraryMetadataRepository().load(ref.metadataPath, error);
                    nativesharp::compilation::PrecompiledProductSidecar sidecar;
                    if (meta && !ref.productSidecarPath.empty())
                    {
                        const bool read = nativesharp::compilation::PrecompiledProductSidecarCodec::read(
                            ref.productSidecarPath, meta->bytes(), sidecar, error);
                        std::cerr << "sidecar read=" << read << " snapshot-bytes=" << sidecar.publicApiSnapshot.size()
                                  << " error=" << error << '\n';
                    }
                }
        }
        ok &= expect(installed && resolved && result && observerCalls == 1 && !compilation.diagnostics().hasErrors(),
                     "published semantic snapshot installs and remains usable in isolated C++ observation");
    }
    return ok ? 0 : 1;
}
