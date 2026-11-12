#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "../Internal/ClangAbiPlan.h"
#include "ClangSessionSupport.h"
#include "nativesharp/Basic/Sha256.h"

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
#include <clang/UnifiedSymbolResolution/USRGeneration.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iterator>
#include <map>
#include <mutex>
#include <set>
#include <string_view>
#include <utility>

namespace nativesharp::interop
{
namespace
{
std::optional<std::string> declarationUsr(const clang::Decl* declaration)
{
    if (!declaration) return std::nullopt;
    llvm::SmallString<128> value;
    if (clang::index::generateUSRForDecl(declaration->getCanonicalDecl(), value) || value.empty())
        return std::nullopt;
    return std::string(value.str());
}

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
    result.isRestrict = canonical.isRestrictQualified();
    const clang::Type* raw = canonical.getTypePtr();
    clang::QualType element;
    if (const auto* atomic = llvm::dyn_cast<clang::AtomicType>(raw))
    {
        result.kind = CppTypeKind::Atomic;
        element = atomic->getValueType();
    }
    else if (raw->isVoidType()) result.kind = CppTypeKind::Void;
    else if (raw->isBooleanType()) result.kind = CppTypeKind::Boolean;
    else if (const auto* enumType = raw->getAs<clang::EnumType>())
    {
        result.kind = CppTypeKind::Enumeration;
        const auto* declaration = enumType->getDecl();
        if (const auto* definition = declaration->getDefinition()) declaration = definition;
        const auto* anonymousAlias = !context.getLangOpts().CPlusPlus && declaration->getName().empty()
            ? declaration->getTypedefNameForAnonDecl() : nullptr;
        result.enumIdentity = CppEnumTypeIdentity{
            anonymousAlias ? anonymousAlias->getNameAsString() : declaration->getNameAsString(),
            anonymousAlias ? anonymousAlias->getQualifiedNameAsString() : declaration->getQualifiedNameAsString(),
            (declaration->getName().empty() && !anonymousAlias) || declaration->isInAnonymousNamespace(),
            declarationUsr(declaration)};
        if (depth < 64 && !declaration->getIntegerType().isNull())
            result.enumUnderlying = std::make_shared<CppType>(describeType(declaration->getIntegerType(), context, depth + 1));
    }
    else if (raw->isSignedIntegerType()) result.kind = CppTypeKind::SignedInteger;
    else if (raw->isUnsignedIntegerType()) result.kind = CppTypeKind::UnsignedInteger;
    else if (raw->isRealFloatingType())
    {
        result.kind = CppTypeKind::Floating;
        result.ieeeBinary64 = result.unqualifiedCanonicalSpelling == "long double" &&
            &context.getFloatTypeSemantics(canonical) == &llvm::APFloat::IEEEdouble();
    }
    else if (raw->isPointerType()) { result.kind = CppTypeKind::Pointer; element = raw->getPointeeType(); }
    else if (raw->isLValueReferenceType()) { result.kind = CppTypeKind::LValueReference; element = raw->getPointeeType(); }
    else if (raw->isRValueReferenceType()) { result.kind = CppTypeKind::RValueReference; element = raw->getPointeeType(); }
    else if (raw->isArrayType())
    {
        result.kind = CppTypeKind::Array;
        result.isIncompleteArray = context.getAsIncompleteArrayType(canonical) != nullptr;
        if (const auto* array = context.getAsArrayType(canonical))
            element = array->getElementType();
        if (const auto* array = context.getAsConstantArrayType(canonical))
        {
            if (array->getSize().getActiveBits() <= 64)
                result.arrayCount = array->getSize().getZExtValue();
        }
    }
    else if (const auto* recordType = raw->getAs<clang::RecordType>())
    {
        result.kind = CppTypeKind::Record;
        const auto* record = recordType->getDecl()->getCanonicalDecl();
        if (const auto* definition = record->getDefinition()) record = definition;
        CppRecordTypeIdentity identity;
        identity.clangUsr = declarationUsr(record);
        identity.name = record->getNameAsString();
        if (identity.name.empty() && !context.getLangOpts().CPlusPlus)
            if (const auto* alias = record->getTypedefNameForAnonDecl())
                identity.name = alias->getNameAsString();
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
    else if (const auto* function = raw->getAs<clang::FunctionType>())
    {
        result.kind = CppTypeKind::Function;
        if (depth < 64)
        {
            auto signature = std::make_shared<CppFunctionType>();
            signature->returnType = describeType(function->getReturnType(), context, depth + 1);
            signature->callingConvention = clang::FunctionType::getNameForCallConv(function->getCallConv()).str();
            if (const auto* prototype = llvm::dyn_cast<clang::FunctionProtoType>(function))
            {
                signature->hasPrototype = true;
                signature->variadic = prototype->isVariadic();
                for (const auto parameter : prototype->param_types())
                    signature->parameterTypes.push_back(describeType(parameter, context, depth + 1));
            }
            result.function = std::move(signature);
        }
    }

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

void collectEnums(const clang::DeclContext& context, clang::ASTContext& astContext,
                  std::set<const clang::EnumDecl*>& seen, std::vector<CppEnumDeclaration>& result,
                  std::map<unsigned, std::string>& bufferHashes)
{
    for (const clang::Decl* declaration : context.decls())
    {
        if (const auto* nested = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
        {
            collectEnums(*nested, astContext, seen, result, bufferHashes);
            continue;
        }
        if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
        {
            collectEnums(*linkage, astContext, seen, result, bufferHashes);
            continue;
        }
        if (!astContext.getLangOpts().CPlusPlus)
            if (const auto* record = llvm::dyn_cast<clang::RecordDecl>(declaration))
            {
                collectEnums(*record, astContext, seen, result, bufferHashes);
                continue;
            }
        const auto* enumeration = llvm::dyn_cast<clang::EnumDecl>(declaration);
        if (!enumeration || enumeration->isImplicit() || enumeration->isDependentContext() ||
            !seen.insert(enumeration->getCanonicalDecl()).second)
            continue;
        if (const auto* definition = enumeration->getDefinition()) enumeration = definition;
        CppEnumDeclaration value;
        value.clangUsr = declarationUsr(enumeration);
        value.isCxx = astContext.getLangOpts().CPlusPlus;
        const auto* anonymousAlias = !value.isCxx && enumeration->getName().empty()
            ? enumeration->getTypedefNameForAnonDecl() : nullptr;
        value.name = anonymousAlias ? anonymousAlias->getNameAsString() : enumeration->getNameAsString();
        value.qualifiedName = anonymousAlias ? anonymousAlias->getQualifiedNameAsString()
                                             : enumeration->getQualifiedNameAsString();
        value.complete = enumeration->isCompleteDefinition();
        value.scoped = enumeration->isScoped();
        const clang::QualType enumType = astContext.getCanonicalTagType(enumeration);
        if (!enumType->isIncompleteType() && !enumType->isDependentType())
        {
            value.sizeBits = astContext.getTypeSize(enumType);
            value.alignmentBits = astContext.getTypeAlign(enumType);
        }
        if (!enumeration->getIntegerType().isNull())
            value.underlyingType = describeType(enumeration->getIntegerType(), astContext);
        const auto location = astContext.getSourceManager().getPresumedLoc(enumeration->getLocation());
        if (location.isValid())
            value.location = CppSourcePoint{pathFromUtf8(location.getFilename()), location.getLine(), location.getColumn()};
        const auto physical = enumeration->getLocation();
        if (physical.isFileID())
        {
            const auto& source = astContext.getSourceManager();
            const auto filename = source.getFilename(physical);
            const auto file = source.getFileID(physical);
            bool invalid = false;
            const auto contents = source.getBufferData(file, &invalid);
            if (!invalid && !filename.empty() && filename.front() != '<')
            {
                auto [position, inserted] = bufferHashes.try_emplace(file.getHashValue());
                if (inserted)
                {
                    const auto* data = reinterpret_cast<const std::uint8_t*>(contents.data());
                    position->second = basic::sha256Hex(basic::sha256(std::span(data, contents.size())));
                }
                value.sourceProvenance = CppEnumDeclaration::SourceProvenance{
                    pathFromUtf8(filename.str().c_str()), position->second, source.getFileOffset(physical)};
            }
        }
        if (value.complete)
            for (const auto* enumerator : enumeration->enumerators())
            {
                const auto integer = enumerator->getInitVal();
                llvm::SmallString<32> digits;
                integer.toString(digits, 10);
                CppEnumeratorDeclaration item;
                item.clangUsr = declarationUsr(enumerator);
                item.name = enumerator->getNameAsString();
                item.decimalValue = std::string(digits.str());
                item.valueBits = integer.getBitWidth();
                item.valueIsUnsigned = integer.isUnsigned();
                item.valueType = describeType(enumerator->getType(), astContext);
                const auto point = astContext.getSourceManager().getPresumedLoc(enumerator->getLocation());
                if (point.isValid())
                    item.location = CppSourcePoint{pathFromUtf8(point.getFilename()), point.getLine(), point.getColumn()};
                value.enumerators.push_back(std::move(item));
            }
        result.push_back(std::move(value));
    }
}

void collectTypeAliases(const clang::DeclContext& context, clang::ASTContext& astContext,
                        std::set<const clang::TypedefNameDecl*>& seen,
                        std::vector<CppTypeAliasDeclaration>& result)
{
    for (const clang::Decl* declaration : context.decls())
    {
        if (const auto* nested = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
        {
            collectTypeAliases(*nested, astContext, seen, result);
            continue;
        }
        if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
        {
            collectTypeAliases(*linkage, astContext, seen, result);
            continue;
        }
        const auto* alias = llvm::dyn_cast<clang::TypedefNameDecl>(declaration);
        if (!alias || alias->isImplicit() || alias->getName().empty() || context.isDependentContext() ||
            !seen.insert(alias->getCanonicalDecl()).second)
            continue;
        CppTypeAliasDeclaration value;
        value.clangUsr = declarationUsr(alias);
        value.name = alias->getNameAsString();
        value.qualifiedName = alias->getQualifiedNameAsString();
        value.insideAnonymousNamespace = alias->isInAnonymousNamespace();
        value.underlyingType = describeType(alias->getUnderlyingType(), astContext);
        const auto location = astContext.getSourceManager().getPresumedLoc(alias->getLocation());
        if (location.isValid())
            value.location = CppSourcePoint{
                pathFromUtf8(location.getFilename()), location.getLine(), location.getColumn()};
        result.push_back(std::move(value));
    }
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
            value.clangUsr = declarationUsr(item);
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
                          clang::CodeGenerator* generator,
                          std::set<const clang::FunctionDecl*>& seen,
                          std::vector<CppFunctionDeclaration>& result)
{
    for (const clang::Decl* declaration : context.decls())
    {
        if (const auto* nested = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
        {
            collectFreeFunctions(*nested, astContext, mangler, generator, seen, result);
            continue;
        }
        if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
        {
            collectFreeFunctions(*linkage, astContext, mangler, generator, seen, result);
            continue;
        }
        const auto* function = llvm::dyn_cast<clang::FunctionDecl>(declaration);
        if (!function || function->isImplicit() || function->getDeclName().isEmpty() ||
            function->getDescribedFunctionTemplate() || function->isDependentContext() ||
            !seen.insert(function->getCanonicalDecl()).second)
            continue;

        // A C entity can be declared first without a prototype and later with
        // one. Clang has already checked the redeclarations for compatibility;
        // prefer the most recent declaration carrying its composite prototype.
        if (function->getLanguageLinkage() == clang::CLanguageLinkage)
        {
            const auto* latest = function->getMostRecentDecl();
            if (latest->getType()->getAs<clang::FunctionProtoType>()) function = latest;
            else for (const auto* redeclaration : function->redecls())
                if (redeclaration->getType()->getAs<clang::FunctionProtoType>())
                { function = redeclaration; break; }
        }

        CppFunctionDeclaration value;
        value.clangUsr = declarationUsr(function);
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
        value.hasPrototype = function->getType()->getAs<clang::FunctionProtoType>() != nullptr;
        value.externalLinkage = function->isExternallyVisible();
        const auto* definition = function->getDefinition();
        // A C `static` definition in a header gives every translation unit its own copy, with or without
        // `inline`; both are imported the same way. Una definición C `static` en un encabezado da a cada
        // unidad de traducción su propia copia, con o sin `inline`; ambas se importan igual.
        value.isCStaticInlineDefinition = !astContext.getLangOpts().CPlusPlus &&
            !value.externalLinkage && definition && definition->getStorageClass() == clang::SC_Static &&
            definition->hasBody();
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
            if (value.callingConvention == "regcall" && generator)
                value.externalSymbol = generator->GetMangledName(clang::GlobalDecl(function)).str();
            else if (mangler.shouldMangleDeclName(function))
            {
                llvm::SmallString<128> symbol;
                llvm::raw_svector_ostream stream(symbol);
                mangler.mangleName(clang::GlobalDecl(function), stream);
                value.externalSymbol = std::string(symbol.str());
            }
            else
                value.externalSymbol = value.name;
            // The generator observes this exact C++ declaration, including its namespace
            // and overload. Do not select a different declaration by unqualified name.
            if (astContext.getLangOpts().CPlusPlus && generator && value.callingConvention == "regcall" &&
                !value.variadic && value.hasPrototype && value.externalSymbol &&
                (function->getReturnType()->isVoidType() || !function->getReturnType()->isIncompleteType()) &&
                std::all_of(function->param_begin(), function->param_end(), [](const auto* parameter) {
                    return !parameter->getType()->isIncompleteType();
                }) &&
                (function->getReturnType()->isRecordType() ||
                 std::any_of(function->param_begin(), function->param_end(), [](const auto* parameter) {
                     return parameter->getType()->isRecordType();
                 })))
            {
                std::string error;
                auto plan = abi_plan::copyPlan(*generator, *function, error);
                if (plan && plan->symbol == *value.externalSymbol)
                    value.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(*plan));
            }
        }
        const auto location = astContext.getSourceManager().getPresumedLoc(function->getLocation());
        if (location.isValid())
            value.location = CppSourcePoint{
                pathFromUtf8(location.getFilename()), location.getLine(), location.getColumn()};
        result.push_back(std::move(value));
    }
}

void collectGlobals(const clang::DeclContext& context, clang::ASTContext& astContext,
                    clang::MangleContext& mangler, std::set<const clang::VarDecl*>& seen,
                    std::vector<CppGlobalDeclaration>& result)
{
    for (const clang::Decl* declaration : context.decls())
    {
        if (const auto* nested = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
        {
            collectGlobals(*nested, astContext, mangler, seen, result);
            continue;
        }
        if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
        {
            collectGlobals(*linkage, astContext, mangler, seen, result);
            continue;
        }
        const auto* variable = llvm::dyn_cast<clang::VarDecl>(declaration);
        if (!variable || !variable->isFileVarDecl() || variable->isImplicit() ||
            variable->getDeclName().isEmpty() || variable->getDeclContext()->isDependentContext() ||
            !seen.insert(variable->getCanonicalDecl()).second)
            continue;
        CppGlobalDeclaration value;
        value.clangUsr = declarationUsr(variable);
        value.name = variable->getNameAsString();
        value.qualifiedName = variable->getQualifiedNameAsString();
        value.type = describeType(variable->getType(), astContext);
        if (!variable->getType()->isIncompleteType())
            value.declarationAlignmentBits =
                astContext.getDeclAlign(variable).getQuantity() * astContext.getCharWidth();
        // `extern T a[];` has no layout, but an explicit `_Alignas` is still part of its contract.
        // `extern T a[];` no tiene layout, pero un `_Alignas` explícito sigue siendo parte de su contrato.
        else if (variable->getType()->isIncompleteArrayType() && variable->getMaxAlignment() != 0)
            value.declarationAlignmentBits = variable->getMaxAlignment();
        value.externalLinkage = variable->isExternallyVisible();
        value.threadLocal = variable->getTLSKind() != clang::VarDecl::TLS_None;
        value.dllImport = variable->hasAttr<clang::DLLImportAttr>();
        if (value.externalLinkage)
        {
            if (mangler.shouldMangleDeclName(variable))
            {
                llvm::SmallString<128> symbol;
                llvm::raw_svector_ostream stream(symbol);
                mangler.mangleName(clang::GlobalDecl(variable), stream);
                value.externalSymbol = std::string(symbol.str());
            }
            else value.externalSymbol = value.name;
        }
        const auto location = astContext.getSourceManager().getPresumedLoc(variable->getLocation());
        if (location.isValid())
            value.location = CppSourcePoint{
                pathFromUtf8(location.getFilename()), location.getLine(), location.getColumn()};
        result.push_back(std::move(value));
    }
}

CppAccess describeAccess(clang::AccessSpecifier access)
{
    switch (access)
    {
    case clang::AS_public: return CppAccess::Public;
    case clang::AS_protected: return CppAccess::Protected;
    case clang::AS_private: return CppAccess::Private;
    default: return CppAccess::None;
    }
}

// Capture nested C11 aggregates separately until their identities can be projected and persisted.
// EN: T08-D14: besides anonymous carriers, also the unnamed struct or union type of a named member, once per type.
// ES: T08-D14: ademas de los portadores anonimos, tambien el tipo struct o union sin nombre de un miembro con
//     nombre, una vez por tipo.
void collectAnonymousCRecords(const clang::RecordDecl& owner, clang::ASTContext& context,
                              std::vector<CppAnonymousCRecord>& output)
{
    struct Pending
    {
        const clang::RecordDecl* record;
        std::vector<std::size_t> path;
        std::string memberName;
        std::vector<std::size_t> siblings;
    };
    std::vector<Pending> pending;
    const auto enqueueFields = [&pending, &context](const clang::RecordDecl& parent,
                                                    const std::vector<std::size_t>& base) {
        std::map<const clang::RecordDecl*, std::size_t> namedTypes;
        std::size_t index = 0;
        for (const auto* field : parent.fields())
        {
            auto path = base;
            path.push_back(index++);
            if (field->isAnonymousStructOrUnion())
            {
                const auto* type = field->getType()->getAs<clang::RecordType>();
                const auto* definition = type ? type->getDecl()->getDefinition() : nullptr;
                if (definition) pending.push_back({definition, std::move(path), {}, {}});
                continue;
            }
            // EN: the unnamed record may be the member's type or the element of its arrays and pointers.
            // ES: el record sin nombre puede ser el tipo del miembro o el elemento de sus arreglos y punteros.
            clang::QualType element = field->getType();
            for (unsigned depth = 0; depth < 64; ++depth)
            {
                if (const auto* array = context.getAsArrayType(element)) element = array->getElementType();
                else if (element->isPointerType()) element = element->getPointeeType();
                else break;
            }
            const auto* type = element->getAs<clang::RecordType>();
            const auto* definition = type ? type->getDecl()->getDefinition() : nullptr;
            if (!definition) continue;
            if (field->getName().empty() || !definition->getName().empty() ||
                definition->getTypedefNameForAnonDecl()) continue;
            const auto known = namedTypes.find(definition);
            if (known != namedTypes.end())
            {
                pending[known->second].siblings.push_back(path.back());
                continue;
            }
            namedTypes.emplace(definition, pending.size());
            pending.push_back({definition, std::move(path), field->getNameAsString(), {}});
        }
    };
    enqueueFields(owner, {});
    while (!pending.empty())
    {
        auto item = std::move(pending.back());
        pending.pop_back();
        const auto& layout = context.getASTRecordLayout(item.record);
        CppAnonymousCRecord result;
        result.carrierPath = item.path;
        result.memberName = item.memberName;
        result.siblingMemberIndices = item.siblings;
        result.kind = item.record->isUnion() ? CppRecordKind::Union : CppRecordKind::Struct;
        result.sizeBits = layout.getSize().getQuantity() * context.getCharWidth();
        result.alignmentBits = layout.getAlignment().getQuantity() * context.getCharWidth();
        unsigned index = 0;
        for (const auto* field : item.record->fields())
        {
            CppRecordField projected;
            projected.clangUsr = declarationUsr(field);
            projected.name = field->getNameAsString();
            projected.type = describeType(field->getType(), context);
            projected.access = CppAccess::Public;
            projected.offsetBits = layout.getFieldOffset(index);
            projected.isAnonymousCarrier = field->isAnonymousStructOrUnion();
            if (field->isBitField()) projected.bitWidth = field->getBitWidthValue();
            ++index;
            result.fields.push_back(std::move(projected));
        }
        enqueueFields(*item.record, item.path);
        output.push_back(std::move(result));
    }
}

void collectRecords(const clang::DeclContext& context, clang::ASTContext& astContext,
                    clang::MangleContext& mangler, std::set<const clang::TagDecl*>& seen,
                    std::vector<CppRecordDeclaration>& result, std::map<unsigned, std::string>& bufferHashes)
{
    for (const clang::Decl* declaration : context.decls())
    {
        if (const auto* nested = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
        {
            collectRecords(*nested, astContext, mangler, seen, result, bufferHashes);
            continue;
        }
        if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
        {
            collectRecords(*linkage, astContext, mangler, seen, result, bufferHashes);
            continue;
        }
        const auto* record = llvm::dyn_cast<clang::RecordDecl>(declaration);
        if (!record || record->isImplicit() ||
            record->isDependentContext() || !seen.insert(record->getCanonicalDecl()).second)
            continue;
        if (const auto* definition = record->getDefinition()) record = definition;

        const auto* anonymousAlias = !astContext.getLangOpts().CPlusPlus && record->getName().empty()
            ? record->getTypedefNameForAnonDecl() : nullptr;
        if (record->getName().empty() && !anonymousAlias) continue;

        CppRecordDeclaration value;
        value.clangUsr = declarationUsr(record);
        value.isCxx = astContext.getLangOpts().CPlusPlus;
        value.name = anonymousAlias ? anonymousAlias->getNameAsString() : record->getNameAsString();
        value.qualifiedName = anonymousAlias ? anonymousAlias->getQualifiedNameAsString()
                                             : record->getQualifiedNameAsString();
        for (const clang::DeclContext* parent = record->getDeclContext(); parent != nullptr;
             parent = parent->getParent())
        {
            if (const auto* ns = llvm::dyn_cast<clang::NamespaceDecl>(parent))
            {
                if (ns->isAnonymousNamespace()) value.anonymousContext = true;
                else value.namespaceSegments.push_back(ns->getNameAsString());
            }
            else if (const auto* enclosing = llvm::dyn_cast<clang::RecordDecl>(parent))
            {
                const auto name = enclosing->getNameAsString();
                if (name.empty()) value.anonymousContext = true;
                else value.containingRecords.push_back(name);
            }
        }
        std::reverse(value.namespaceSegments.begin(), value.namespaceSegments.end());
        std::reverse(value.containingRecords.begin(), value.containingRecords.end());
        value.kind = record->isUnion() ? CppRecordKind::Union :
                     record->isClass() ? CppRecordKind::Class : CppRecordKind::Struct;
        value.access = describeAccess(record->getAccess());
        value.complete = record->isCompleteDefinition();
        for (const clang::DeclContext* current = record; current != nullptr; current = current->getParent())
            value.specializationContext |= llvm::isa<clang::ClassTemplateSpecializationDecl>(current);
        const auto location = astContext.getSourceManager().getPresumedLoc(record->getLocation());
        if (location.isValid())
            value.location = CppSourcePoint{
                pathFromUtf8(location.getFilename()), location.getLine(), location.getColumn()};
        const auto physical = record->getLocation();
        if (physical.isFileID())
        {
            const auto& source = astContext.getSourceManager();
            const auto filename = source.getFilename(physical);
            const auto file = source.getFileID(physical);
            bool invalid = false;
            const auto contents = source.getBufferData(file, &invalid);
            if (!invalid && !filename.empty() && filename.front() != '<')
            {
                const auto key = file.getHashValue();
                auto [position, inserted] = bufferHashes.try_emplace(key);
                if (inserted)
                {
                    const auto* data = reinterpret_cast<const std::uint8_t*>(contents.data());
                    position->second = basic::sha256Hex(basic::sha256(std::span(data, contents.size())));
                }
                value.sourceProvenance = CppRecordDeclaration::SourceProvenance{
                    pathFromUtf8(filename.str().c_str()), position->second, source.getFileOffset(physical)};
            }
        }
        if (value.complete)
        {
            if (!value.specializationContext)
            {
                clang::ODRHash hash;
                if (const auto* cppRecord = llvm::dyn_cast<clang::CXXRecordDecl>(record))
                    hash.AddCXXRecordDecl(cppRecord);
                else hash.AddRecordDecl(record);
                value.odrHash = hash.CalculateHash();
            }
            const clang::ASTRecordLayout& layout = astContext.getASTRecordLayout(record);
            value.sizeBits = static_cast<std::uint64_t>(layout.getSize().getQuantity()) * astContext.getCharWidth();
            value.alignmentBits = static_cast<std::uint64_t>(layout.getAlignment().getQuantity()) * astContext.getCharWidth();
            unsigned index = 0;
            for (const clang::FieldDecl* field : record->fields())
            {
                CppRecordField projected;
                projected.clangUsr = declarationUsr(field);
                projected.name = field->getNameAsString();
                projected.type = describeType(field->getType(), astContext);
                projected.access = value.isCxx ? describeAccess(field->getAccess()) : CppAccess::Public;
                projected.isAnonymousCarrier = !value.isCxx && field->isAnonymousStructOrUnion();
                projected.offsetBits = layout.getFieldOffset(index++);
                if (field->isBitField()) projected.bitWidth = field->getBitWidthValue();
                value.fields.push_back(std::move(projected));
            }
            if (const auto* cppRecord = llvm::dyn_cast<clang::CXXRecordDecl>(record))
            {
                value.valueTraits = CppRecordValueTraits{
                    cppRecord->isTriviallyCopyable(), cppRecord->isStandardLayout(),
                    cppRecord->hasTrivialDefaultConstructor(), cppRecord->hasTrivialDestructor(), cppRecord->isPOD(),
                    cppRecord->hasNonLiteralTypeFieldsOrBases()};
                CppImplicitLifetimeMembers implicit;
                implicit.defaultConstructor = cppRecord->needsImplicitDefaultConstructor();
                implicit.copyConstructor = cppRecord->needsImplicitCopyConstructor();
                implicit.moveConstructor = cppRecord->needsImplicitMoveConstructor();
                implicit.destructor = cppRecord->needsImplicitDestructor();
                for (const auto* method : cppRecord->methods())
                {
                    if (!method->isImplicit()) continue;
                    if (const auto* ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(method))
                    {
                        implicit.defaultConstructor |= ctor->isDefaultConstructor();
                        implicit.copyConstructor |= ctor->isCopyConstructor();
                        implicit.moveConstructor |= ctor->isMoveConstructor();
                    }
                    else if (llvm::isa<clang::CXXDestructorDecl>(method)) implicit.destructor = true;
                }
                value.implicitLifetimeMembers = implicit;
                for (const clang::CXXBaseSpecifier& base : cppRecord->bases())
                {
                    CppBaseDeclaration projected;
                    projected.type = describeType(base.getType(), astContext);
                    projected.access = describeAccess(base.getAccessSpecifier());
                    projected.isVirtual = base.isVirtual();
                    if (const auto* baseRecord = base.getType()->getAsCXXRecordDecl())
                    {
                        const clang::CharUnits offset = base.isVirtual()
                            ? layout.getVBaseClassOffset(baseRecord)
                            : layout.getBaseClassOffset(baseRecord);
                        projected.offsetBits = static_cast<std::uint64_t>(offset.getQuantity()) * astContext.getCharWidth();
                    }
                    value.bases.push_back(std::move(projected));
                }
                for (const clang::Decl* member : cppRecord->decls())
                {
                    const auto* field = llvm::dyn_cast<clang::VarDecl>(member);
                    if (!field || !field->isStaticDataMember() || field->isImplicit()) continue;
                    CppStaticField projected;
                    projected.clangUsr = declarationUsr(field);
                    projected.name = field->getNameAsString();
                    projected.type = describeType(field->getType(), astContext);
                    projected.access = describeAccess(field->getAccess());
                    projected.threadLocal = field->getTLSKind() != clang::VarDecl::TLS_None;
                    projected.dllImport = field->hasAttr<clang::DLLImportAttr>();
                    if (field->isExternallyVisible())
                    {
                        if (mangler.shouldMangleDeclName(field))
                        {
                            llvm::SmallString<128> symbol;
                            llvm::raw_svector_ostream stream(symbol);
                            mangler.mangleName(clang::GlobalDecl(field), stream);
                            projected.externalSymbol = std::string(symbol.str());
                        }
                        else projected.externalSymbol = projected.name;
                    }
                    value.staticFields.push_back(std::move(projected));
                }
                for (const clang::CXXMethodDecl* method : cppRecord->methods())
                {
                    if (method->isImplicit() || method->isDependentContext() ||
                        method->getDescribedFunctionTemplate()) continue;
                    CppMethodDeclaration projected;
                    projected.clangUsr = declarationUsr(method);
                    const auto* ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(method);
                    const auto* dtor = llvm::dyn_cast<clang::CXXDestructorDecl>(method);
                    projected.kind = ctor ? CppMethodKind::Constructor :
                                     dtor ? CppMethodKind::Destructor : CppMethodKind::Method;
                    projected.name = method->getNameAsString();
                    projected.qualifiedName = method->getQualifiedNameAsString();
                    projected.canonicalFunctionType = method->getType().getCanonicalType().getAsString();
                    if (const auto* type = method->getType()->getAs<clang::FunctionType>())
                        projected.callingConvention = clang::FunctionType::getNameForCallConv(type->getCallConv()).str();
                    switch (method->getRefQualifier())
                    {
                    case clang::RQ_LValue: projected.refQualifier = CppMethodRefQualifier::LValue; break;
                    case clang::RQ_RValue: projected.refQualifier = CppMethodRefQualifier::RValue; break;
                    case clang::RQ_None: break;
                    }
                    projected.returnType = describeType(method->getReturnType(), astContext);
                    projected.access = describeAccess(method->getAccess());
                    projected.isStatic = method->isStatic();
                    projected.isConst = method->isConst();
                    projected.isVolatile = method->isVolatile();
                    projected.dllImport = method->hasAttr<clang::DLLImportAttr>();
                    if (const auto* prototype = method->getType()->getAs<clang::FunctionProtoType>())
                        if (!clang::isUnresolvedExceptionSpec(prototype->getExceptionSpecType()))
                            projected.isNonThrowing = prototype->isNothrow();
                    projected.isVirtual = method->isVirtual();
                    projected.isPureVirtual = method->isPureVirtual();
                    projected.isDeleted = method->isDeleted();
                    projected.variadic = method->isVariadic();
                    for (const clang::ParmVarDecl* parameter : method->parameters())
                        projected.parameters.push_back(CppFunctionParameter{
                            parameter->getNameAsString(), parameter->getType().getAsString(),
                            parameter->getType().getCanonicalType().getAsString(),
                            describeType(parameter->getType(), astContext), parameter->hasDefaultArg()});
                    if (method->isExternallyVisible() && !method->isDeleted())
                    {
                        llvm::SmallString<128> symbol;
                        llvm::raw_svector_ostream stream(symbol);
                        if (ctor) mangler.mangleName(clang::GlobalDecl(ctor, clang::Ctor_Complete), stream);
                        else if (dtor)
                        {
                            // Clang's MS ABI uses the base destructor for a complete object
                            // when no virtual bases exist (MicrosoftCXXABI.cpp:1703).
                            const auto variant = astContext.getTargetInfo().getCXXABI().isMicrosoft() &&
                                dtor->getParent()->getNumVBases() == 0 ? clang::Dtor_Base : clang::Dtor_Complete;
                            mangler.mangleName(clang::GlobalDecl(dtor, variant), stream);
                        }
                        else mangler.mangleName(clang::GlobalDecl(method), stream);
                        projected.externalSymbol = std::string(symbol.str());
                    }
                    value.methods.push_back(std::move(projected));
                }
            }
        }
        if (!value.isCxx && value.complete)
            collectAnonymousCRecords(*record, astContext, value.anonymousCRecords);
        result.push_back(std::move(value));
        if (record->isCompleteDefinition())
            collectRecords(*record, astContext, mangler, seen, result, bufferHashes);
    }
}

void findComparableRecords(const clang::DeclContext& context, std::string_view name,
                           std::set<const clang::TagDecl*>& seen,
                           std::vector<const clang::RecordDecl*>& matches)
{
    for (const auto* declaration : context.decls())
    {
        if (const auto* ns = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
        {
            if (!ns->isAnonymousNamespace()) findComparableRecords(*ns, name, seen, matches);
            continue;
        }
        if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
        {
            findComparableRecords(*linkage, name, seen, matches);
            continue;
        }
        const auto* record = llvm::dyn_cast<clang::RecordDecl>(declaration);
        if (!record || record->isImplicit() || record->isDependentContext() ||
            llvm::isa<clang::ClassTemplateSpecializationDecl>(record) ||
            !seen.insert(record->getCanonicalDecl()).second) continue;
        if (const auto* definition = record->getDefinition()) record = definition;
        if (record->isCompleteDefinition() && record->getQualifiedNameAsString() == name)
            matches.push_back(record);
        if (record->isCompleteDefinition()) findComparableRecords(*record, name, seen, matches);
    }
}

bool hasReplayBuiltinOrArray(clang::QualType type, unsigned depth = 0)
{
    if (depth > 16 || type.isNull() || type.hasQualifiers()) return false;
    if (const auto* array = llvm::dyn_cast<clang::ConstantArrayType>(type.getTypePtr()))
    {
        const auto& count = array->getSize();
        return count.getActiveBits() <= 32 && count.getZExtValue() > 0 && count.getZExtValue() <= 1024 &&
            hasReplayBuiltinOrArray(array->getElementType(), depth + 1);
    }
    if (const auto* alias = llvm::dyn_cast<clang::TypedefType>(type.getTypePtr()))
    {
        const auto* declaration = alias->getDecl();
        return declaration && !declaration->hasAttrs() &&
            hasReplayBuiltinOrArray(declaration->getUnderlyingType(), depth + 1);
    }
    if (llvm::isa<clang::BuiltinType>(type.getTypePtr())) return true;
    if (const auto* record = llvm::dyn_cast<clang::RecordType>(type.getTypePtr()))
        return record->getDecl() && record->getDecl()->getIdentifier() && record->getDecl()->isCompleteDefinition();
    return false;
}

class ClangSession final : public CppInteropSession
{
public:
    explicit ClangSession(CppSessionOptions options)
        : options_(std::move(options)), consumer_(diagnostics_) {}

    CppSessionState state() const noexcept override
    {
        std::lock_guard lock(mutex_);
        return state_;
    }
    CppSessionOptions optionsSnapshot() const override
    {
        std::lock_guard lock(mutex_);
        return options_;
    }
    std::vector<CppSessionDiagnostic> diagnosticsSnapshot() const override
    {
        std::lock_guard lock(mutex_);
        return diagnostics_;
    }
    std::optional<bool> microsoftVolatileSemantics() const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_) return std::nullopt;
        return ast_->getASTContext().getLangOpts().MSVolatile;
    }
    std::optional<std::size_t> topLevelDeclarationCount() const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_) return std::nullopt;
        const auto* unit = ast_->getASTContext().getTranslationUnitDecl();
        return static_cast<std::size_t>(std::distance(unit->decls_begin(), unit->decls_end()));
    }
    std::optional<std::vector<CppFunctionDeclaration>> freeFunctionsSnapshot() const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_) return std::nullopt;
        clang::ASTContext& context = ast_->getASTContext();
        std::unique_ptr<clang::MangleContext> mangler(context.createMangleContext());
        std::vector<CppFunctionDeclaration> result;
        std::set<const clang::FunctionDecl*> seen;
        collectFreeFunctions(*context.getTranslationUnitDecl(), context, *mangler, nullptr, seen, result);
        if (std::any_of(result.begin(), result.end(), [](const auto& value) { return value.callingConvention == "regcall"; }))
        {
            llvm::LLVMContext llvmContext;
            auto generator = clang::CreateLLVMCodeGen(ast_->getDiagnostics(), "nsc.regcall.symbol",
                ast_->getVirtualFileSystemPtr(), ast_->getHeaderSearchOpts(), ast_->getPreprocessorOpts(),
                ast_->getCodeGenOpts(), llvmContext);
            if (!generator) return std::nullopt;
            generator->Initialize(context);
            seen.clear();
            result.clear();
            collectFreeFunctions(*context.getTranslationUnitDecl(), context, *mangler, generator.get(), seen, result);
        }
        return result;
    }
    std::optional<abi_plan::Plan> cFunctionAbiPlan(
        std::string_view name, std::string_view canonicalType) const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_ || name.empty() || canonicalType.empty()) return std::nullopt;
        auto& context = ast_->getASTContext();
        if (context.getLangOpts().CPlusPlus) return std::nullopt;
        const clang::FunctionDecl* match = nullptr;
        for (const auto* declaration : context.getTranslationUnitDecl()->decls())
        {
            const auto* candidate = llvm::dyn_cast<clang::FunctionDecl>(declaration);
            if (!candidate || candidate->isImplicit() || !candidate->isExternallyVisible() ||
                candidate->getNameAsString() != name || candidate->isDependentContext()) continue;
            candidate = candidate->getMostRecentDecl();
            if (candidate->getType().getCanonicalType().getAsString() != canonicalType) continue;
            if (match && match->getCanonicalDecl() != candidate->getCanonicalDecl()) return std::nullopt;
            match = candidate;
        }
        if (!match || !match->getType()->getAs<clang::FunctionProtoType>()) return std::nullopt;
        const clang::QualType result = match->getReturnType();
        if (result.isNull() || (!result->isVoidType() && result->isIncompleteType())) return std::nullopt;
        for (const clang::ParmVarDecl* parameter : match->parameters())
            if (parameter->getType().isNull() || parameter->getType()->isIncompleteType()) return std::nullopt;
        llvm::LLVMContext llvmContext;
        auto generator = clang::CreateLLVMCodeGen(ast_->getDiagnostics(), "nsc.c.abi.plan",
            ast_->getVirtualFileSystemPtr(), ast_->getHeaderSearchOpts(), ast_->getPreprocessorOpts(),
            ast_->getCodeGenOpts(), llvmContext);
        if (!generator) return std::nullopt;
        generator->Initialize(context);
        std::string error;
        return abi_plan::copyPlan(*generator, *match, error);
    }
    std::optional<abi_plan::Plan> cFunctionPointerAbiPlan(
        std::string_view name, std::string_view canonicalType) const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_ || name.empty() || canonicalType.empty()) return std::nullopt;
        auto& context = ast_->getASTContext();
        if (context.getLangOpts().CPlusPlus) return std::nullopt;
        const clang::TypedefNameDecl* match = nullptr;
        for (const auto* declaration : context.getTranslationUnitDecl()->decls())
        {
            const auto* candidate = llvm::dyn_cast<clang::TypedefNameDecl>(declaration);
            if (!candidate || candidate->isImplicit() || candidate->hasAttrs() ||
                candidate->getNameAsString() != name ||
                candidate->getUnderlyingType().getCanonicalType().getAsString() != canonicalType) continue;
            if (match && match->getCanonicalDecl() != candidate->getCanonicalDecl()) return std::nullopt;
            match = candidate;
        }
        if (!match) return std::nullopt;
        const auto pointer = match->getUnderlyingType().getCanonicalType();
        if (pointer.hasQualifiers() || !pointer->isPointerType()) return std::nullopt;
        const auto functionType = pointer->getPointeeType();
        const auto* prototype = functionType->getAs<clang::FunctionProtoType>();
        if (!prototype || prototype->isVariadic() || prototype->hasExtParameterInfos() ||
            (prototype->getCallConv() != clang::CC_X86_64SysV &&
             prototype->getCallConv() != clang::CC_X86RegCall)) return std::nullopt;
        const auto resultType = prototype->getReturnType();
        if (resultType.isNull() || (!resultType->isVoidType() && resultType->isIncompleteType())) return std::nullopt;
        for (const auto type : prototype->param_types())
            if (type.isNull() || type->isIncompleteType()) return std::nullopt;
        auto* declaration = clang::FunctionDecl::Create(context, context.getTranslationUnitDecl(), {}, {},
            &context.Idents.get("__nsc_pointer_abi_probe"), functionType,
            context.getTrivialTypeSourceInfo(functionType), clang::SC_Extern);
        if (!declaration) return std::nullopt;
        std::vector<clang::ParmVarDecl*> parameters;
        parameters.reserve(prototype->getNumParams());
        for (const auto type : prototype->param_types())
            parameters.push_back(clang::ParmVarDecl::Create(context, declaration, {}, {}, nullptr, type,
                context.getTrivialTypeSourceInfo(type), clang::SC_None, nullptr));
        declaration->setParams(parameters);
        llvm::LLVMContext llvmContext;
        auto generator = clang::CreateLLVMCodeGen(ast_->getDiagnostics(), "nsc.c.pointer.abi.plan",
            ast_->getVirtualFileSystemPtr(), ast_->getHeaderSearchOpts(), ast_->getPreprocessorOpts(),
            ast_->getCodeGenOpts(), llvmContext);
        if (!generator) return std::nullopt;
        generator->Initialize(context);
        std::string error;
        auto plan = abi_plan::copyPlan(*generator, *declaration, error);
        if (!plan || !plan->targetTriple.starts_with("x86_64-pc-windows-msvc")) return std::nullopt;
        return plan;
    }
    std::optional<abi_plan::Plan> cppFunctionPointerAbiPlan(
        std::string_view qualifiedName, std::string_view usr, std::string_view canonicalType) const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_ || qualifiedName.empty() || usr.empty() || canonicalType.empty()) return std::nullopt;
        auto& context = ast_->getASTContext();
        if (!context.getLangOpts().CPlusPlus) return std::nullopt;
        const clang::TypedefNameDecl* match = nullptr;
        bool ambiguous = false;
        std::function<void(const clang::DeclContext&)> visit = [&](const clang::DeclContext& declarations) {
            for (const auto* declaration : declarations.decls())
            {
                if (const auto* nested = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
                { visit(*nested); continue; }
                if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
                { visit(*linkage); continue; }
                const auto* candidate = llvm::dyn_cast<clang::TypedefNameDecl>(declaration);
                if (!candidate || candidate->isImplicit() || candidate->hasAttrs() ||
                    candidate->getQualifiedNameAsString() != qualifiedName ||
                    candidate->getUnderlyingType().getCanonicalType().getAsString() != canonicalType ||
                    declarationUsr(candidate) != std::optional<std::string>(std::string(usr))) continue;
                if (match && match->getCanonicalDecl() != candidate->getCanonicalDecl()) ambiguous = true;
                match = candidate;
            }
        };
        visit(*context.getTranslationUnitDecl());
        if (!match || ambiguous) return std::nullopt;
        const auto pointer = match->getUnderlyingType().getCanonicalType();
        if (pointer.hasQualifiers() || !pointer->isPointerType()) return std::nullopt;
        const auto functionType = pointer->getPointeeType();
        const auto* prototype = functionType->getAs<clang::FunctionProtoType>();
        if (!prototype || prototype->isVariadic() || prototype->hasExtParameterInfos() ||
            prototype->getCallConv() != clang::CC_X86RegCall) return std::nullopt;
        const auto resultType = prototype->getReturnType();
        if (resultType.isNull() || (!resultType->isVoidType() && resultType->isIncompleteType())) return std::nullopt;
        for (const auto type : prototype->param_types())
            if (type.isNull() || type->isIncompleteType()) return std::nullopt;
        auto* declaration = clang::FunctionDecl::Create(context, context.getTranslationUnitDecl(), {}, {},
            &context.Idents.get("__nsc_cpp_pointer_abi_probe"), functionType,
            context.getTrivialTypeSourceInfo(functionType), clang::SC_Extern);
        if (!declaration) return std::nullopt;
        std::vector<clang::ParmVarDecl*> parameters;
        parameters.reserve(prototype->getNumParams());
        for (const auto type : prototype->param_types())
            parameters.push_back(clang::ParmVarDecl::Create(context, declaration, {}, {}, nullptr, type,
                context.getTrivialTypeSourceInfo(type), clang::SC_None, nullptr));
        declaration->setParams(parameters);
        llvm::LLVMContext llvmContext;
        auto generator = clang::CreateLLVMCodeGen(ast_->getDiagnostics(), "nsc.cpp.pointer.abi.plan",
            ast_->getVirtualFileSystemPtr(), ast_->getHeaderSearchOpts(), ast_->getPreprocessorOpts(),
            ast_->getCodeGenOpts(), llvmContext);
        if (!generator) return std::nullopt;
        generator->Initialize(context);
        std::string error;
        auto plan = abi_plan::copyPlan(*generator, *declaration, error);
        if (!plan || !plan->targetTriple.starts_with("x86_64-pc-windows-msvc")) return std::nullopt;
        // This identity denotes an ABI probe, never a linkable C++ symbol.
        plan->symbol = "__regcall3____nsc_pointer_abi_probe";
        return plan;
    }
    std::optional<abi_plan::VariadicCallPlan> cVariadicCallAbiPlan(
        std::string_view name, std::string_view canonicalType,
        const std::vector<CppType>& optionalArguments) const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_ || name.empty() || canonicalType.empty()) return std::nullopt;
        auto& context = ast_->getASTContext();
        if (context.getLangOpts().CPlusPlus) return std::nullopt;
        const clang::FunctionDecl* match = nullptr;
        for (const auto* declaration : context.getTranslationUnitDecl()->decls())
        {
            const auto* candidate = llvm::dyn_cast<clang::FunctionDecl>(declaration);
            if (!candidate || candidate->isImplicit() || !candidate->isExternallyVisible() ||
                candidate->getNameAsString() != name || candidate->isDependentContext()) continue;
            candidate = candidate->getMostRecentDecl();
            if (candidate->getType().getCanonicalType().getAsString() != canonicalType) continue;
            if (match && match->getCanonicalDecl() != candidate->getCanonicalDecl()) return std::nullopt;
            match = candidate;
        }
        if (!match) return std::nullopt;
        return copyVariadicPlanForDeclaration(match, optionalArguments);
    }
    std::optional<abi_plan::VariadicCallPlan> cVariadicFunctionPointerCallAbiPlan(
        std::string_view name, std::string_view canonicalType,
        const std::vector<CppType>& optionalArguments) const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_ || name.empty() || canonicalType.empty()) return std::nullopt;
        auto& context = ast_->getASTContext();
        if (context.getLangOpts().CPlusPlus) return std::nullopt;
        const clang::TypedefNameDecl* match = nullptr;
        for (const auto* declaration : context.getTranslationUnitDecl()->decls())
        {
            const auto* candidate = llvm::dyn_cast<clang::TypedefNameDecl>(declaration);
            if (!candidate || candidate->isImplicit() || candidate->hasAttrs() ||
                candidate->getNameAsString() != name ||
                candidate->getUnderlyingType().getCanonicalType().getAsString() != canonicalType) continue;
            if (match && match->getCanonicalDecl() != candidate->getCanonicalDecl()) return std::nullopt;
            match = candidate;
        }
        if (!match) return std::nullopt;
        const auto pointer = match->getUnderlyingType().getCanonicalType();
        if (pointer.hasQualifiers() || !pointer->isPointerType()) return std::nullopt;
        const auto functionType = pointer->getPointeeType();
        const auto* prototype = functionType->getAs<clang::FunctionProtoType>();
        if (!prototype || !prototype->isVariadic() || prototype->hasExtParameterInfos() ||
            prototype->getCallConv() != clang::CC_X86_64SysV) return std::nullopt;
        const auto resultType = prototype->getReturnType();
        if (resultType.isNull() || (!resultType->isVoidType() && resultType->isIncompleteType())) return std::nullopt;
        for (const auto type : prototype->param_types())
            if (type.isNull() || type->isIncompleteType()) return std::nullopt;
        auto* declaration = clang::FunctionDecl::Create(context, context.getTranslationUnitDecl(), {}, {},
            &context.Idents.get("__nsc_variadic_pointer_abi_probe"), functionType,
            context.getTrivialTypeSourceInfo(functionType), clang::SC_Extern);
        if (!declaration) return std::nullopt;
        std::vector<clang::ParmVarDecl*> parameters;
        parameters.reserve(prototype->getNumParams());
        for (const auto type : prototype->param_types())
            parameters.push_back(clang::ParmVarDecl::Create(context, declaration, {}, {}, nullptr, type,
                context.getTrivialTypeSourceInfo(type), clang::SC_None, nullptr));
        declaration->setParams(parameters);
        return copyVariadicPlanForDeclaration(declaration, optionalArguments);
    }
    // Caller holds mutex_ and owns the declaration for the duration of this observation.
    std::optional<abi_plan::VariadicCallPlan> copyVariadicPlanForDeclaration(
        const clang::FunctionDecl* match, const std::vector<CppType>& optionalArguments) const
    {
        if (!ast_ || !match) return std::nullopt;
        auto& context = ast_->getASTContext();
        const auto* prototype = match->getType()->getAs<clang::FunctionProtoType>();
        if (!prototype || !prototype->isVariadic()) return std::nullopt;
        std::function<std::optional<clang::QualType>(const CppType&, unsigned)> resolve =
            [&](const CppType& input, unsigned depth) -> std::optional<clang::QualType>
        {
            if (depth > 16 || input.isConst || input.isVolatile || input.isRestrict ||
                input.canonicalSpelling.empty()) return std::nullopt;
            clang::QualType result;
            if (input.kind == CppTypeKind::Record)
            {
                if (!input.recordIdentity || input.recordIdentity->name.empty() ||
                    !input.recordIdentity->clangUsr || input.recordIdentity->anonymousContext ||
                    input.recordIdentity->specializationContext || !input.recordIdentity->namespaceSegments.empty() ||
                    !input.recordIdentity->containingRecords.empty()) return std::nullopt;
                std::set<const clang::TagDecl*> seen;
                std::vector<const clang::RecordDecl*> candidates;
                findComparableRecords(*context.getTranslationUnitDecl(), input.recordIdentity->name, seen, candidates);
                if (candidates.size() != 1 || declarationUsr(candidates[0]) != input.recordIdentity->clangUsr)
                    return std::nullopt;
                result = context.getTypeDeclType(static_cast<const clang::TypeDecl*>(candidates[0]));
            }
            else if (input.kind == CppTypeKind::Pointer)
            {
                if (!input.element) return std::nullopt;
                auto element = resolve(*input.element, depth + 1);
                if (!element) return std::nullopt;
                result = context.getPointerType(*element);
            }
            else
            {
                const clang::QualType builtins[] = {context.VoidTy, context.BoolTy, context.CharTy,
                    context.SignedCharTy, context.UnsignedCharTy, context.ShortTy, context.UnsignedShortTy,
                    context.IntTy, context.UnsignedIntTy, context.LongTy, context.UnsignedLongTy,
                    context.LongLongTy, context.UnsignedLongLongTy, context.FloatTy, context.DoubleTy};
                for (const auto type : builtins)
                    if (type.getCanonicalType().getAsString() == input.canonicalSpelling) { result = type; break; }
            }
            if (result.isNull() || result.getCanonicalType().getAsString() != input.canonicalSpelling)
                return std::nullopt;
            if (!((input.kind == CppTypeKind::Void && result->isVoidType()) ||
                  (input.kind == CppTypeKind::Boolean && result->isBooleanType()) ||
                  (input.kind == CppTypeKind::SignedInteger && result->isSignedIntegerType()) ||
                  (input.kind == CppTypeKind::UnsignedInteger && result->isUnsignedIntegerType() && !result->isBooleanType()) ||
                  (input.kind == CppTypeKind::Floating && result->isRealFloatingType()) ||
                  (input.kind == CppTypeKind::Pointer && result->isPointerType()) ||
                  (input.kind == CppTypeKind::Record && result->isRecordType()))) return std::nullopt;
            if (!result->isVoidType() && (!input.sizeBits || !input.alignmentBits ||
                *input.sizeBits != context.getTypeSize(result) || *input.alignmentBits != context.getTypeAlign(result)))
                return std::nullopt;
            return result;
        };
        std::vector<clang::QualType> arguments;
        for (const auto* parameter : match->parameters()) arguments.push_back(parameter->getType());
        for (const auto& argument : optionalArguments)
        {
            auto resolved = resolve(argument, 0);
            if (!resolved) return std::nullopt;
            arguments.push_back(*resolved);
        }
        llvm::LLVMContext llvmContext;
        auto generator = clang::CreateLLVMCodeGen(ast_->getDiagnostics(), "nsc.c.variadic.call.abi",
            ast_->getVirtualFileSystemPtr(), ast_->getHeaderSearchOpts(), ast_->getPreprocessorOpts(),
            ast_->getCodeGenOpts(), llvmContext);
        if (!generator) return std::nullopt;
        generator->Initialize(context);
        std::string error;
        return abi_plan::copyVariadicCallPlan(*generator, *match, arguments, error);
    }
    std::optional<std::vector<CppRecordDeclaration>> recordsSnapshot() const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_) return std::nullopt;
        clang::ASTContext& context = ast_->getASTContext();
        std::unique_ptr<clang::MangleContext> mangler(context.createMangleContext());
        std::vector<CppRecordDeclaration> result;
        std::set<const clang::TagDecl*> seen;
        std::map<unsigned, std::string> bufferHashes;
        collectRecords(*context.getTranslationUnitDecl(), context, *mangler, seen, result, bufferHashes);
        return result;
    }
    std::optional<bool> cRecordHasPlainReplaySyntax(std::string_view name) const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_ || name.empty() || ast_->getASTContext().getLangOpts().CPlusPlus) return std::nullopt;
        auto& context = ast_->getASTContext();
        std::set<const clang::TagDecl*> seen;
        std::vector<const clang::RecordDecl*> records;
        findComparableRecords(*context.getTranslationUnitDecl(), name, seen, records);
        if (records.size() != 1) return std::nullopt;
        const auto* record = records.front();
        if (record->hasAttrs() || record->field_empty()) return false;
        for (const auto* field : record->fields())
        {
            const auto type = field->getType();
            if (type.isNull() || field->hasAttrs() || field->isAnonymousStructOrUnion() ||
                (field->isBitField() && ((field->getIdentifier() && field->getBitWidthValue() == 0) ||
                                         !type->isIntegerType())) ||
                !hasReplayBuiltinOrArray(type))
                return false;
        }
        return true;
    }
    std::optional<bool> cRecordHasPackedReplaySyntax(std::string_view name) const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_ || name.empty() || ast_->getASTContext().getLangOpts().CPlusPlus) return std::nullopt;
        auto& context = ast_->getASTContext();
        std::set<const clang::TagDecl*> seen;
        std::vector<const clang::RecordDecl*> records;
        findComparableRecords(*context.getTranslationUnitDecl(), name, seen, records);
        if (records.size() != 1) return std::nullopt;
        const auto* record = records.front();
        if (!record->hasAttr<clang::PackedAttr>() || record->field_empty()) return false;
        for (const auto* attribute : record->attrs())
            if (!llvm::isa<clang::PackedAttr>(attribute)) return false;
        for (const auto* field : record->fields())
        {
            const auto type = field->getType();
            if (type.isNull() || field->hasAttrs() || field->isAnonymousStructOrUnion() ||
                (field->isBitField() && ((field->getIdentifier() && field->getBitWidthValue() == 0) ||
                                         !type->isIntegerType())) ||
                !hasReplayBuiltinOrArray(type))
                return false;
        }
        return true;
    }
    std::optional<std::vector<CppTypeAliasDeclaration>> typeAliasesSnapshot() const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_) return std::nullopt;
        auto& context = ast_->getASTContext();
        std::vector<CppTypeAliasDeclaration> result;
        std::set<const clang::TypedefNameDecl*> seen;
        collectTypeAliases(*context.getTranslationUnitDecl(), context, seen, result);
        return result;
    }
    std::optional<std::vector<CppEnumDeclaration>> enumsSnapshot() const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_) return std::nullopt;
        auto& context = ast_->getASTContext();
        std::vector<CppEnumDeclaration> result;
        std::set<const clang::EnumDecl*> seen;
        std::map<unsigned, std::string> bufferHashes;
        collectEnums(*context.getTranslationUnitDecl(), context, seen, result, bufferHashes);
        if (const auto* input = std::get_if<CppBufferInput>(&options_.input))
        {
            auto path = input->virtualPath;
            if (path.is_relative()) path = options_.workingDirectory / path;
            path = path.lexically_normal();
            auto contents = std::make_shared<const std::string>(input->contents);
            for (auto& item : result)
                if (item.sourceProvenance && item.sourceProvenance->sourcePath.lexically_normal() == path)
                    item.transientSourceBuffer = contents;
        }
        return result;
    }
    std::optional<std::vector<CppImplicitLifetimeDeclaration>> implicitLifetimeDeclarations(
        std::string_view name) const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_ || !ast_->hasSema() || name.empty() || !ast_->getASTContext().getLangOpts().CPlusPlus)
            return std::nullopt;
        auto& context = ast_->getASTContext();
        std::set<const clang::TagDecl*> seen;
        std::vector<const clang::RecordDecl*> records;
        findComparableRecords(*context.getTranslationUnitDecl(), name, seen, records);
        if (records.size() != 1) return std::nullopt;
        auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(const_cast<clang::RecordDecl*>(records.front()));
        if (!record || !record->isCompleteDefinition() || record->isDependentContext()) return std::nullopt;
        ast_->getSema().ForceDeclarationOfImplicitMembers(record);
        std::vector<CppImplicitLifetimeDeclaration> result;
        std::unique_ptr<clang::MangleContext> mangler(context.createMangleContext());
        for (const auto* method : record->methods())
        {
            if (!method->isImplicit()) continue;
            std::optional<CppImplicitLifetimeKind> kind;
            if (const auto* ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(method))
            {
                if (ctor->isDefaultConstructor()) kind = CppImplicitLifetimeKind::DefaultConstructor;
                else if (ctor->isCopyConstructor()) kind = CppImplicitLifetimeKind::CopyConstructor;
                else if (ctor->isMoveConstructor()) kind = CppImplicitLifetimeKind::MoveConstructor;
            }
            else if (llvm::isa<clang::CXXDestructorDecl>(method)) kind = CppImplicitLifetimeKind::Destructor;
            if (!kind) continue;
            CppExceptionSpecification specification = CppExceptionSpecification::Unresolved;
            if (const auto* prototype = method->getType()->getAs<clang::FunctionProtoType>())
            {
                if (!method->isDeleted() && prototype->getExceptionSpecType() == clang::EST_Unevaluated)
                    prototype = ast_->getSema().ResolveExceptionSpec(method->getLocation(), prototype);
                if (prototype)
                if (!clang::isUnresolvedExceptionSpec(prototype->getExceptionSpecType()))
                    specification = prototype->isNothrow() ? CppExceptionSpecification::NonThrowing
                                                           : CppExceptionSpecification::PotentiallyThrowing;
            }
            CppImplicitLifetimeDeclaration item{*kind, method->getType().getCanonicalType().getAsString(), {},
                describeAccess(method->getAccess()), method->isDeleted(), method->isTrivial(), specification, std::nullopt,
                method->hasAttr<clang::DLLImportAttr>()};
            if (method->isExternallyVisible() && !method->isDeleted())
            {
                llvm::SmallString<128> symbol;
                llvm::raw_svector_ostream stream(symbol);
                if (const auto* ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(method))
                    mangler->mangleName(clang::GlobalDecl(ctor, clang::Ctor_Complete), stream);
                else if (const auto* dtor = llvm::dyn_cast<clang::CXXDestructorDecl>(method))
                {
                    const auto variant = context.getTargetInfo().getCXXABI().isMicrosoft() &&
                        dtor->getParent()->getNumVBases() == 0 ? clang::Dtor_Base : clang::Dtor_Complete;
                    mangler->mangleName(clang::GlobalDecl(dtor, variant), stream);
                }
                item.abiSymbol = std::string(symbol.str());
            }
            for (const auto* parameter : method->parameters())
                item.parameters.push_back(CppFunctionParameter{parameter->getNameAsString(),
                    parameter->getType().getAsString(), parameter->getType().getCanonicalType().getAsString(),
                    describeType(parameter->getType(), context), parameter->hasDefaultArg()});
            result.push_back(std::move(item));
        }
        return result;
    }
    std::optional<CppRecordValueOperations> recordValueOperations(std::string_view name) const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_ || !ast_->hasSema() || name.empty() || !ast_->getASTContext().getLangOpts().CPlusPlus)
            return std::nullopt;
        auto& context = ast_->getASTContext();
        std::set<const clang::TagDecl*> seen;
        std::vector<const clang::RecordDecl*> records;
        findComparableRecords(*context.getTranslationUnitDecl(), name, seen, records);
        if (records.size() != 1) return std::nullopt;
        auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(const_cast<clang::RecordDecl*>(records.front()));
        if (!record || !record->isCompleteDefinition() || record->isDependentContext()) return std::nullopt;
        auto& sema = ast_->getSema();
        sema.ForceDeclarationOfImplicitMembers(record);
        const auto select = [&](clang::CXXSpecialMemberKind kind, bool constArgument) {
            const auto result = sema.LookupSpecialMember(record, kind, constArgument, false, false, false, false);
            CppSpecialMemberSelection selected;
            using Status = clang::Sema::SpecialMemberOverloadResult;
            if (result.getKind() == Status::Success) selected.status = CppSpecialMemberSelectionStatus::Selected;
            else if (result.getKind() == Status::Ambiguous) selected.status = CppSpecialMemberSelectionStatus::Ambiguous;
            if (const auto* method = result.getMethod())
            {
                selected.access = describeAccess(method->getAccess());
                selected.isDeleted = method->isDeleted();
                selected.isTrivial = method->isTrivial();
            }
            return selected;
        };
        return CppRecordValueOperations{
            select(clang::CXXSpecialMemberKind::CopyConstructor, true),
            select(clang::CXXSpecialMemberKind::CopyConstructor, false),
            select(clang::CXXSpecialMemberKind::Destructor, false),
            select(clang::CXXSpecialMemberKind::DefaultConstructor, false),
            select(clang::CXXSpecialMemberKind::CopyAssignment, true),
            select(clang::CXXSpecialMemberKind::CopyAssignment, false), declarationUsr(record)};
    }
    std::optional<std::string> emitImplicitLifetimeIR(
        std::string_view name, CppImplicitLifetimeKind requested) const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_ || !ast_->hasSema() || name.empty() || !ast_->getASTContext().getLangOpts().CPlusPlus ||
            (requested != CppImplicitLifetimeKind::DefaultConstructor &&
             requested != CppImplicitLifetimeKind::Destructor)) return std::nullopt;
        auto& context = ast_->getASTContext();
        std::set<const clang::TagDecl*> seen;
        std::vector<const clang::RecordDecl*> records;
        findComparableRecords(*context.getTranslationUnitDecl(), name, seen, records);
        if (records.size() != 1) return std::nullopt;
        auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(const_cast<clang::RecordDecl*>(records.front()));
        if (!record || !record->isCompleteDefinition() || record->isDependentContext() ||
            record->getNumVBases() != 0) return std::nullopt;
        auto& sema = ast_->getSema();
        sema.ForceDeclarationOfImplicitMembers(record);
        clang::CXXMethodDecl* selected = nullptr;
        for (auto* method : record->methods())
        {
            if (!method->isImplicit() || method->isDeleted() || method->getAccess() != clang::AS_public)
                continue;
            const auto* ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(method);
            const bool matches = requested == CppImplicitLifetimeKind::DefaultConstructor
                ? ctor && ctor->isDefaultConstructor() : llvm::isa<clang::CXXDestructorDecl>(method);
            if (!matches) continue;
            if (selected) return std::nullopt;
            selected = method;
        }
        if (!selected) return std::nullopt;
        if (!selected->doesThisDeclarationHaveABody())
        {
            if (auto* ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(selected))
                sema.DefineImplicitDefaultConstructor(selected->getLocation(), ctor);
            else
                sema.DefineImplicitDestructor(selected->getLocation(), llvm::cast<clang::CXXDestructorDecl>(selected));
        }
        if (selected->isInvalidDecl() || !selected->doesThisDeclarationHaveABody()) return std::nullopt;
        llvm::LLVMContext llvmContext;
        auto generator = clang::CreateLLVMCodeGen(ast_->getDiagnostics(), "nsc.cpp.lifetime",
            ast_->getVirtualFileSystemPtr(), ast_->getHeaderSearchOpts(), ast_->getPreprocessorOpts(),
            ast_->getCodeGenOpts(), llvmContext);
        if (!generator) return std::nullopt;
        generator->Initialize(context);
        if (!generator->HandleTopLevelDecl(clang::DeclGroupRef(selected))) return std::nullopt;
        std::string requestedSymbol;
        if (auto* ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(selected))
        {
            const clang::GlobalDecl declaration(ctor, clang::Ctor_Complete);
            requestedSymbol = generator->GetMangledName(declaration).str();
            generator->GetAddrOfGlobal(declaration, false);
        }
        else
        {
            auto* dtor = llvm::cast<clang::CXXDestructorDecl>(selected);
            const auto variant = context.getTargetInfo().getCXXABI().isMicrosoft()
                ? clang::Dtor_Base : clang::Dtor_Complete;
            const clang::GlobalDecl declaration(dtor, variant);
            requestedSymbol = generator->GetMangledName(declaration).str();
            generator->GetAddrOfGlobal(declaration, false);
        }
        generator->HandleTranslationUnit(context);
        auto module = generator->ReleaseModule();
        if (!module || requestedSymbol.empty()) return std::nullopt;
        auto* emitted = module->getFunction(requestedSymbol);
        if (!emitted || emitted->isDeclaration()) return std::nullopt;
        // An isolated linkonce_odr definition can be discarded as unused at O2 before
        // another object is linked. Keep COMDAT deduplication while retaining this export.
        if (emitted->hasLinkOnceODRLinkage()) emitted->setLinkage(llvm::GlobalValue::WeakODRLinkage);
        if (!module || llvm::verifyModule(*module, &llvm::errs())) return std::nullopt;
        std::string ir;
        llvm::raw_string_ostream stream(ir);
        module->print(stream, nullptr);
        return ir;
    }
    std::optional<std::string> emitCStaticInlineIR(
        const std::vector<CppCInlineIRExport>& requests,
        const std::vector<CppCInlineIRExport>& globals) const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_ || (requests.empty() && globals.empty()) || ast_->getASTContext().getLangOpts().CPlusPlus)
            return std::nullopt;
        auto& context = ast_->getASTContext();
        std::vector<const clang::FunctionDecl*> selected;
        std::set<std::string> names, symbols;
        for (const auto& request : requests)
        {
            if (request.name.empty() || request.symbol.empty() ||
                !names.insert(request.name).second || !symbols.insert(request.symbol).second)
                return std::nullopt;
            const clang::FunctionDecl* match = nullptr;
            for (const auto* declaration : context.getTranslationUnitDecl()->decls())
            {
                const auto* candidate = llvm::dyn_cast<clang::FunctionDecl>(declaration);
                if (!candidate || candidate->getNameAsString() != request.name ||
                    candidate->getStorageClass() != clang::SC_Static) continue;
                const auto* definition = candidate->getDefinition();
                if (!definition || !definition->hasBody() || (match && match != definition)) return std::nullopt;
                match = definition;
            }
            if (!match) return std::nullopt;
            selected.push_back(match);
        }
        std::vector<const clang::VarDecl*> selectedGlobals;
        for (const auto& request : globals)
        {
            if (request.name.empty() || request.symbol.empty() ||
                !names.insert(request.name).second || !symbols.insert(request.symbol).second)
                return std::nullopt;
            const clang::VarDecl* match = nullptr;
            for (const auto* declaration : context.getTranslationUnitDecl()->decls())
            {
                const auto* candidate = llvm::dyn_cast<clang::VarDecl>(declaration);
                if (!candidate || candidate->getNameAsString() != request.name ||
                    !candidate->isFileVarDecl() || candidate->isExternallyVisible()) continue;
                const auto* definition = candidate->getDefinition();
                if (!definition) definition = candidate->getActingDefinition();
                if (!definition || (match && match != definition)) return std::nullopt;
                match = definition;
            }
            if (!match) return std::nullopt;
            selectedGlobals.push_back(match);
        }
        llvm::LLVMContext llvmContext;
        auto generator = clang::CreateLLVMCodeGen(ast_->getDiagnostics(), "nsc.c.inline",
            ast_->getVirtualFileSystemPtr(), ast_->getHeaderSearchOpts(), ast_->getPreprocessorOpts(),
            ast_->getCodeGenOpts(), llvmContext);
        if (!generator) return std::nullopt;
        generator->Initialize(context);
        // Register internal definitions so a selected inline body can refer to
        // another static helper or file-scope static without exporting them.
        for (const auto* declaration : context.getTranslationUnitDecl()->decls())
        {
            if (const auto* function = llvm::dyn_cast<clang::FunctionDecl>(declaration))
            {
                if (!function->isExternallyVisible() && function->doesThisDeclarationHaveABody() &&
                    !generator->HandleTopLevelDecl(clang::DeclGroupRef(const_cast<clang::FunctionDecl*>(function))))
                    return std::nullopt;
            }
            else if (const auto* variable = llvm::dyn_cast<clang::VarDecl>(declaration))
            {
                if (variable->isFileVarDecl() && !variable->isExternallyVisible() &&
                    !generator->HandleTopLevelDecl(clang::DeclGroupRef(const_cast<clang::VarDecl*>(variable))))
                    return std::nullopt;
                if (variable->isFileVarDecl() && !variable->isExternallyVisible() &&
                    variable->getActingDefinition() == variable)
                    generator->CompleteTentativeDefinition(const_cast<clang::VarDecl*>(variable));
            }
        }
        std::vector<std::string> originalNames;
        for (const auto* function : selected)
        {
            const clang::GlobalDecl declaration(function);
            originalNames.push_back(generator->GetMangledName(declaration).str());
            if (!generator->GetAddrOfGlobal(declaration, false)) return std::nullopt;
        }
        std::vector<std::string> originalGlobals;
        for (const auto* variable : selectedGlobals)
        {
            const clang::GlobalDecl declaration(variable);
            originalGlobals.push_back(generator->GetMangledName(declaration).str());
            if (!generator->GetAddrOfGlobal(declaration, false)) return std::nullopt;
        }
        generator->HandleTranslationUnit(context);
        auto module = generator->ReleaseModule();
        if (!module) return std::nullopt;
        for (std::size_t i = 0; i < requests.size(); ++i)
        {
            auto* function = module->getFunction(originalNames[i]);
            if (!function || function->isDeclaration() || module->getNamedValue(requests[i].symbol))
                return std::nullopt;
            function->setName(requests[i].symbol);
            function->setLinkage(llvm::GlobalValue::ExternalLinkage);
        }
        for (std::size_t i = 0; i < globals.size(); ++i)
        {
            auto* variable = module->getGlobalVariable(originalGlobals[i], true);
            if (!variable || variable->isDeclaration() || module->getNamedValue(globals[i].symbol))
                return std::nullopt;
            variable->setName(globals[i].symbol);
            variable->setLinkage(llvm::GlobalValue::ExternalLinkage);
        }
        if (llvm::verifyModule(*module, &llvm::errs())) return std::nullopt;
        std::string ir;
        llvm::raw_string_ostream stream(ir);
        module->print(stream, nullptr);
        return ir;
    }
    std::optional<bool> structurallyEquivalentRecord(
        const CppInteropSession& other, std::string_view name) const override
    {
        const auto* rhs = dynamic_cast<const ClangSession*>(&other);
        if (!rhs || name.empty()) return std::nullopt;
        auto compare = [&]() -> std::optional<bool> {
            if (!ast_ || !rhs->ast_ || options_.targetTriple != rhs->options_.targetTriple ||
                options_.language != rhs->options_.language ||
                options_.languageStandard != rhs->options_.languageStandard) return std::nullopt;
            auto& left = ast_->getASTContext();
            auto& right = rhs->ast_->getASTContext();
            std::set<const clang::TagDecl*> leftSeen, rightSeen;
            std::vector<const clang::RecordDecl*> leftRecords, rightRecords;
            findComparableRecords(*left.getTranslationUnitDecl(), name, leftSeen, leftRecords);
            findComparableRecords(*right.getTranslationUnitDecl(), name, rightSeen, rightRecords);
            if (leftRecords.size() != 1 || rightRecords.size() != 1) return std::nullopt;
            const auto& a = left.getASTRecordLayout(leftRecords[0]);
            const auto& b = right.getASTRecordLayout(rightRecords[0]);
            if (left.getCharWidth() != right.getCharWidth() || a.getSize() != b.getSize() ||
                a.getAlignment() != b.getAlignment()) return false;
            clang::StructuralEquivalenceContext::NonEquivalentDeclSet unequal;
            clang::StructuralEquivalenceContext context(left.getLangOpts(), left, right, unequal,
                clang::StructuralEquivalenceKind::Default, false, false);
            return context.IsEquivalent(const_cast<clang::RecordDecl*>(leftRecords[0]),
                                        const_cast<clang::RecordDecl*>(rightRecords[0]));
        };
        if (rhs == this) { std::lock_guard lock(mutex_); return compare(); }
        std::scoped_lock lock(mutex_, rhs->mutex_);
        return compare();
    }
    std::optional<std::vector<CppGlobalDeclaration>> globalsSnapshot() const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_) return std::nullopt;
        clang::ASTContext& context = ast_->getASTContext();
        std::unique_ptr<clang::MangleContext> mangler(context.createMangleContext());
        std::vector<CppGlobalDeclaration> result;
        std::set<const clang::VarDecl*> seen;
        collectGlobals(*context.getTranslationUnitDecl(), context, *mangler, seen, result);
        return result;
    }
    std::optional<std::vector<CppNamespaceDeclaration>> namespacesSnapshot() const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_) return std::nullopt;
        clang::ASTContext& context = ast_->getASTContext();
        std::vector<CppNamespaceDeclaration> result;
        std::set<const clang::NamespaceDecl*> seen;
        collectNamespaces(*context.getTranslationUnitDecl(), context, false, seen, result);
        return result;
    }
    std::optional<std::vector<std::filesystem::path>> sourceFilesSnapshot() const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_) return std::nullopt;
        const auto& sources = ast_->getSourceManager();
        std::vector<std::filesystem::path> result;
        for (auto it = sources.fileinfo_begin(); it != sources.fileinfo_end(); ++it)
        {
            auto path = pathFromUtf8(it->first.getName().str().c_str());
            if (path.is_relative()) path = options_.workingDirectory / path;
            result.push_back(path.lexically_normal());
        }
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }
    std::optional<std::vector<CppSourceFileContent>> sourceFileContentsSnapshot() const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_) return std::nullopt;
        const auto& sources = ast_->getSourceManager();
        std::vector<CppSourceFileContent> result;
        for (auto it = sources.fileinfo_begin(); it != sources.fileinfo_end(); ++it)
        {
            if (!it->second) return std::nullopt;
            auto contents = it->second->getBufferDataIfLoaded();
            if (!contents) return std::nullopt;
            auto path = pathFromUtf8(it->first.getName().str().c_str());
            if (path.is_relative()) path = options_.workingDirectory / path;
            const auto* bytes = reinterpret_cast<const std::uint8_t*>(contents->data());
            result.push_back({path.lexically_normal(), contents->size(),
                              basic::sha256(std::span<const std::uint8_t>(bytes, contents->size()))});
        }
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.path < b.path; });
        for (std::size_t i = 1; i < result.size(); ++i)
            if (result[i - 1].path == result[i].path) return std::nullopt;
        return result;
    }
    std::optional<basic::Sha256Digest> preprocessingFingerprint() const override
    {
        std::lock_guard lock(mutex_);
        return ast_ ? preprocessingFingerprint_ : std::nullopt;
    }
    CppSessionCloseResult close() override
    {
        std::lock_guard lock(mutex_);
        if (state_ == CppSessionState::Closed) return {};
        state_ = CppSessionState::Closing;
        ast_.reset();
        state_ = CppSessionState::Closed;
        return {};
    }

    CppSessionOpenFailure initialize(const CppCancellationToken& cancellation)
    {
        auto failure = [this](std::string phase, std::string message) {
            return CppSessionOpenFailure{std::move(phase), std::move(message), diagnostics_};
        };
        if (cancellation.cancelled()) return failure("cancel", "C/C++ session opening was cancelled");
        std::error_code ec;
        if (options_.languageStandard.empty() || options_.targetTriple.empty() ||
            options_.driverReferencePath.empty() || !options_.workingDirectory.is_absolute() ||
            !options_.resourceDirectory.is_absolute())
            return failure("options", "language standard, target, driver reference and absolute directories are required");
        if (options_.volatileMode != CppVolatileMode::Default &&
            options_.volatileMode != CppVolatileMode::Microsoft &&
            options_.volatileMode != CppVolatileMode::Iso)
            return failure("options", "invalid C/C++ volatile mode");
        if (!std::filesystem::is_directory(options_.workingDirectory, ec) || ec)
            return failure("options", "working directory is not accessible");
        ec.clear();
        if (!std::filesystem::is_directory(options_.resourceDirectory / "include", ec) || ec)
            return failure("options", "Clang resource include directory is not accessible");

        llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> filesystem(
            llvm::vfs::createPhysicalFileSystem().release());
        if (const auto error = filesystem->setCurrentWorkingDirectory(utf8(options_.workingDirectory)))
            return failure("options", "cannot set session working directory: " + error.message());
        filesystem = detail::makeCancellationFileSystem(std::move(filesystem), cancellation);

        std::filesystem::path inputPath;
        std::string source;
        if (const auto* file = std::get_if<CppFileInput>(&options_.input))
        {
            inputPath = file->path;
            if (inputPath.empty()) return failure("input", "input file path is empty");
            if (inputPath.is_relative()) inputPath = options_.workingDirectory / inputPath;
            auto contents = filesystem->getBufferForFile(utf8(inputPath));
            if (cancellation.cancelled()) return failure("cancel", "C/C++ session opening was cancelled");
            if (!contents) return failure("input", "cannot read input file: " + contents.getError().message());
            source = (*contents)->getBuffer().str();
        }
        else
        {
            const auto& buffer = std::get<CppBufferInput>(options_.input);
            inputPath = buffer.virtualPath;
            if (inputPath.empty()) return failure("input", "virtual input file path is empty");
            if (inputPath.is_relative()) inputPath = options_.workingDirectory / inputPath;
            source = buffer.contents;
        }
        const auto args = detail::buildClangInvocationArguments(options_);

        ast_ = detail::runClangSessionAction(
            source, args, utf8(inputPath), utf8(options_.driverReferencePath),
            &consumer_, std::move(filesystem), cancellation, nullptr,
            options_.capturePreprocessingFingerprint ? &preprocessingFingerprint_ : nullptr);
        if (cancellation.cancelled())
        {
            ast_.reset();
            return failure("cancel", "C/C++ session opening was cancelled");
        }
        const bool capturedError = std::any_of(diagnostics_.begin(), diagnostics_.end(), [](const auto& diagnostic) {
            return diagnostic.severity == CppSessionDiagnosticSeverity::Error ||
                   diagnostic.severity == CppSessionDiagnosticSeverity::Fatal;
        });
        if (!ast_ || capturedError || ast_->getDiagnostics().hasErrorOccurred())
        {
            ast_.reset();
            return failure("parse", "Clang could not analyze the translation unit");
        }
        if (!ast_->hasSema())
        {
            ast_.reset();
            return failure("parse", "Clang did not retain the semantic context");
        }
        state_ = CppSessionState::Ready;
        return {};
    }

private:
    mutable std::mutex mutex_;
    CppSessionOptions options_;
    std::vector<CppSessionDiagnostic> diagnostics_;
    detail::DiagnosticCollector consumer_;
    // Destroy before consumer_: Clang's diagnostic engine borrows the consumer.
    std::unique_ptr<clang::ASTUnit> ast_;
    std::optional<basic::Sha256Digest> preprocessingFingerprint_;
    CppSessionState state_ = CppSessionState::Closed;
};
}

CppSessionOpenResult CppClangSessionFactory::open(
    CppSessionOptions options, CppCancellationToken cancellation)
{
    auto session = std::make_unique<ClangSession>(std::move(options));
    auto failure = session->initialize(cancellation);
    if (!failure.phase.empty()) return failure;
    return std::unique_ptr<CppInteropSession>(std::move(session));
}
}
