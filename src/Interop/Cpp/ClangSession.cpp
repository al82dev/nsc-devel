#include "nativesharp/Interop/CppClangSessionFactory.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/ASTStructuralEquivalence.h>
#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/DeclGroup.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/ODRHash.h>
#include <clang/AST/RecordLayout.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/CodeGen/ModuleBuilder.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Sema/Sema.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <mutex>
#include <set>
#include <string_view>
#include <utility>

namespace nativesharp::interop
{
namespace
{
std::string utf8(const std::filesystem::path& path)
{
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

std::filesystem::path pathFromUtf8(const char* text)
{
    const std::string_view bytes(text);
    return std::filesystem::path(std::u8string(
        reinterpret_cast<const char8_t*>(bytes.data()), bytes.size()));
}

class DiagnosticCollector final : public clang::DiagnosticConsumer
{
public:
    explicit DiagnosticCollector(std::vector<CppSessionDiagnostic>& diagnostics)
        : diagnostics_(diagnostics) {}

    void HandleDiagnostic(clang::DiagnosticsEngine::Level level, const clang::Diagnostic& info) override
    {
        clang::DiagnosticConsumer::HandleDiagnostic(level, info);
        if (level == clang::DiagnosticsEngine::Ignored) return;
        CppSessionDiagnostic diagnostic;
        switch (level)
        {
        case clang::DiagnosticsEngine::Note: diagnostic.severity = CppSessionDiagnosticSeverity::Note; break;
        case clang::DiagnosticsEngine::Remark: diagnostic.severity = CppSessionDiagnosticSeverity::Remark; break;
        case clang::DiagnosticsEngine::Warning: diagnostic.severity = CppSessionDiagnosticSeverity::Warning; break;
        case clang::DiagnosticsEngine::Fatal: diagnostic.severity = CppSessionDiagnosticSeverity::Fatal; break;
        default: diagnostic.severity = CppSessionDiagnosticSeverity::Error; break;
        }
        llvm::SmallString<256> message;
        info.FormatDiagnostic(message);
        diagnostic.message = std::string(message.str());
        diagnostic.originalId = info.getID();
        if (info.hasSourceManager() && info.getLocation().isValid())
        {
            const auto location = info.getSourceManager().getPresumedLoc(info.getLocation());
            if (location.isValid())
                diagnostic.location = CppSourcePoint{
                    pathFromUtf8(location.getFilename()), location.getLine(), location.getColumn()};
        }
        if (level == clang::DiagnosticsEngine::Note && !diagnostics_.empty())
            diagnostics_.back().notes.push_back(std::move(diagnostic));
        else
            diagnostics_.push_back(std::move(diagnostic));
    }

private:
    std::vector<CppSessionDiagnostic>& diagnostics_;
};

CppType describeType(clang::QualType type, clang::ASTContext& context, unsigned depth = 0)
{
    CppType result;
    if (type.isNull()) return result;
    result.spelling = type.getAsString();
    const clang::QualType canonical = type.getCanonicalType();
    result.canonicalSpelling = canonical.getAsString();
    result.unqualifiedCanonicalSpelling = canonical.getUnqualifiedType().getAsString();
    result.isConst = canonical.isConstQualified();
    result.isVolatile = canonical.isVolatileQualified();
    const clang::Type* raw = canonical.getTypePtr();
    clang::QualType element;
    if (raw->isVoidType()) result.kind = CppTypeKind::Void;
    else if (raw->isBooleanType()) result.kind = CppTypeKind::Boolean;
    else if (raw->isEnumeralType()) result.kind = CppTypeKind::Enumeration;
    else if (raw->isSignedIntegerType()) result.kind = CppTypeKind::SignedInteger;
    else if (raw->isUnsignedIntegerType()) result.kind = CppTypeKind::UnsignedInteger;
    else if (raw->isRealFloatingType()) result.kind = CppTypeKind::Floating;
    else if (raw->isPointerType()) { result.kind = CppTypeKind::Pointer; element = raw->getPointeeType(); }
    else if (raw->isLValueReferenceType()) { result.kind = CppTypeKind::LValueReference; element = raw->getPointeeType(); }
    else if (raw->isRValueReferenceType()) { result.kind = CppTypeKind::RValueReference; element = raw->getPointeeType(); }
    else if (raw->isArrayType())
    {
        result.kind = CppTypeKind::Array;
        if (const auto* array = context.getAsConstantArrayType(canonical))
        {
            element = array->getElementType();
            if (array->getSize().getActiveBits() <= 64)
                result.arrayCount = array->getSize().getZExtValue();
        }
    }
    else if (const auto* recordType = raw->getAs<clang::RecordType>())
    {
        result.kind = CppTypeKind::Record;
        const auto* record = recordType->getDecl()->getCanonicalDecl();
        CppRecordTypeIdentity identity;
        identity.name = record->getNameAsString();
        identity.anonymousContext = identity.name.empty();
        for (const clang::DeclContext* current = record; current != nullptr; current = current->getParent())
        {
            identity.specializationContext |= llvm::isa<clang::ClassTemplateSpecializationDecl>(current);
            if (const auto* ns = llvm::dyn_cast<clang::NamespaceDecl>(current))
            {
                if (ns->isAnonymousNamespace()) identity.anonymousContext = true;
                else identity.namespaceSegments.push_back(ns->getNameAsString());
            }
            else if (current != record)
                if (const auto* enclosing = llvm::dyn_cast<clang::RecordDecl>(current))
                {
                    const auto name = enclosing->getNameAsString();
                    if (name.empty()) identity.anonymousContext = true;
                    else identity.containingRecords.push_back(name);
                }
        }
        std::reverse(identity.namespaceSegments.begin(), identity.namespaceSegments.end());
        std::reverse(identity.containingRecords.begin(), identity.containingRecords.end());
        result.recordIdentity = std::move(identity);
    }
    else if (raw->isFunctionType()) result.kind = CppTypeKind::Function;

    if (!element.isNull() && depth < 64)
        result.element = std::make_shared<CppType>(describeType(element, context, depth + 1));
    if (result.kind != CppTypeKind::Unsupported && result.kind != CppTypeKind::Void &&
        result.kind != CppTypeKind::Function && result.kind != CppTypeKind::LValueReference &&
        result.kind != CppTypeKind::RValueReference && !raw->isIncompleteType() &&
        !raw->isDependentType() && !raw->isUndeducedType() && !raw->isSizelessType() &&
        !raw->isVariablyModifiedType())
    {
        result.sizeBits = context.getTypeSize(canonical);
        result.alignmentBits = context.getTypeAlign(canonical);
    }
    return result;
}

}
}
