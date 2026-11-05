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

void collectNamespaces(const clang::DeclContext& context, clang::ASTContext& astContext,
                       bool insideAnonymous, std::set<const clang::NamespaceDecl*>& seen,
                       std::vector<CppNamespaceDeclaration>& result)
{
    for (const clang::Decl* declaration : context.decls())
    {
        const auto* item = llvm::dyn_cast<clang::NamespaceDecl>(declaration);
        if (!item) continue;
        const bool anonymous = item->isAnonymousNamespace();
        if (seen.insert(item->getCanonicalDecl()).second)
        {
            CppNamespaceDeclaration value;
            value.qualifiedName = item->getQualifiedNameAsString();
            value.isInline = item->isInline();
            value.isAnonymous = anonymous;
            value.insideAnonymous = insideAnonymous;
            const auto location = astContext.getSourceManager().getPresumedLoc(item->getLocation());
            if (location.isValid())
                value.location = CppSourcePoint{
                    pathFromUtf8(location.getFilename()), location.getLine(), location.getColumn()};
            result.push_back(std::move(value));
        }
        // Every reopening may introduce different children, even when the namespace is canonicalized.
        collectNamespaces(*item, astContext, insideAnonymous || anonymous, seen, result);
    }
}

void collectFreeFunctions(const clang::DeclContext& context, clang::ASTContext& astContext,
                          clang::MangleContext& mangler,
                          std::set<const clang::FunctionDecl*>& seen,
                          std::vector<CppFunctionDeclaration>& result)
{
    for (const clang::Decl* declaration : context.decls())
    {
        if (const auto* nested = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
        {
            collectFreeFunctions(*nested, astContext, mangler, seen, result);
            continue;
        }
        if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
        {
            collectFreeFunctions(*linkage, astContext, mangler, seen, result);
            continue;
        }
        const auto* function = llvm::dyn_cast<clang::FunctionDecl>(declaration);
        if (!function || function->isImplicit() || function->getDeclName().isEmpty() ||
            function->getDescribedFunctionTemplate() || function->isDependentContext() ||
            !seen.insert(function->getCanonicalDecl()).second)
            continue;

        CppFunctionDeclaration value;
        value.name = function->getNameAsString();
        value.qualifiedName = function->getQualifiedNameAsString();
        value.canonicalFunctionType = function->getType().getCanonicalType().getAsString();
        if (const auto* type = function->getType()->getAs<clang::FunctionType>())
            value.callingConvention = clang::FunctionType::getNameForCallConv(type->getCallConv()).str();
        switch (function->getLanguageLinkage())
        {
        case clang::CLanguageLinkage: value.languageLinkage = CppLanguageLinkage::C; break;
        case clang::CXXLanguageLinkage: value.languageLinkage = CppLanguageLinkage::Cpp; break;
        case clang::NoLanguageLinkage: value.languageLinkage = CppLanguageLinkage::None; break;
        }
        value.returnTypeSpelling = function->getReturnType().getAsString();
        value.canonicalReturnTypeSpelling = function->getReturnType().getCanonicalType().getAsString();
        value.returnType = describeType(function->getReturnType(), astContext);
        value.variadic = function->isVariadic();
        value.externalLinkage = function->isExternallyVisible();
        value.dllImport = function->hasAttr<clang::DLLImportAttr>();
        if (const auto* prototype = function->getType()->getAs<clang::FunctionProtoType>())
            if (!clang::isUnresolvedExceptionSpec(prototype->getExceptionSpecType()))
                value.isNonThrowing = prototype->isNothrow();
        for (const clang::ParmVarDecl* parameter : function->parameters())
        {
            value.parameters.push_back(CppFunctionParameter{
                parameter->getNameAsString(), parameter->getType().getAsString(),
                parameter->getType().getCanonicalType().getAsString(),
                describeType(parameter->getType(), astContext), parameter->hasDefaultArg()});
        }
        if (value.externalLinkage)
        {
            if (mangler.shouldMangleDeclName(function))
            {
                llvm::SmallString<128> symbol;
                llvm::raw_svector_ostream stream(symbol);
                mangler.mangleName(clang::GlobalDecl(function), stream);
                value.externalSymbol = std::string(symbol.str());
            }
            else
                value.externalSymbol = value.name;
        }
        const auto location = astContext.getSourceManager().getPresumedLoc(function->getLocation());
        if (location.isValid())
            value.location = CppSourcePoint{
                pathFromUtf8(location.getFilename()), location.getLine(), location.getColumn()};
        result.push_back(std::move(value));
    }
}

}
}
