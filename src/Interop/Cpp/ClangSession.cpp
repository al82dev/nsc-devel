#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "nativesharp/Interop/CppRegCallAbiValidation.h"
#include "nativesharp/Interop/CppMemberThunk.h"
#include "../Internal/ClangAbiPlan.h"
#include "ClangSessionSupport.h"
#include "nativesharp/Basic/Sha256.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/ASTStructuralEquivalence.h>
#include <clang/AST/Attr.h>
#include <clang/AST/CXXInheritance.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/DeclGroup.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/ODRHash.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticSema.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/CodeGen/CodeGenABITypes.h>
#include <clang/CodeGen/ModuleBuilder.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Sema/Sema.h>
#include <clang/Sema/TemplateDeduction.h>
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
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iterator>
#include <map>
#include <mutex>
#include <set>
#include <span>
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

// EN: T13-004. A class template specialization declared at namespace scope is an imported record whose
//     NativeSharp name is its template name followed by a hash of its canonical C++ type: the same name in
//     every session that sees the same specialization, and an identifier NativeSharp can bind.
// ES: T13-004. Una especializacion de plantilla de clase declarada en un namespace es un record importado
//     cuyo nombre NativeSharp es el de su plantilla seguido de un hash de su tipo C++ canonico: el mismo
//     nombre en toda sesion que ve la misma especializacion, y un identificador que NativeSharp enlaza.
std::optional<std::string> cppSpecializationName(const clang::ClassTemplateSpecializationDecl& specialization)
{
    if (llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(specialization) ||
        specialization.isDependentContext() || !specialization.getIdentifier()) return std::nullopt;
    for (const auto* parent = specialization.getDeclContext(); parent; parent = parent->getParent())
        if (!parent->isNamespace() && !parent->isTranslationUnit() && !llvm::isa<clang::LinkageSpecDecl>(parent) &&
            !llvm::isa<clang::ExportDecl>(parent)) return std::nullopt;
    auto& context = specialization.getASTContext();
    clang::PrintingPolicy policy(context.getLangOpts());
    policy.SuppressTagKeyword = true;
    policy.FullyQualifiedName = true;
    const auto spelling = clang::QualType(context.getCanonicalTagType(&specialization)).getAsString(policy);
    if (spelling.find("(anonymous") != std::string::npos || spelling.find("(lambda") != std::string::npos)
        return std::nullopt;
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char c : spelling) { hash ^= c; hash *= 1099511628211ULL; }
    static constexpr char digits[] = "0123456789abcdef";
    std::string name = specialization.getNameAsString() + "__t";
    for (int shift = 60; shift >= 0; shift -= 4) name.push_back(digits[(hash >> shift) & 0xF]);
    return name;
}

// T13-004: the qualified name Clang gives the members of a specialization, without the member name.
// T13-004: el nombre calificado que Clang da a los miembros de una especializacion, sin el del miembro.
std::string cppSpecializationQualifiedName(const clang::ClassTemplateSpecializationDecl& specialization)
{
    for (const auto* declaration : specialization.decls())
        if (const auto* injected = llvm::dyn_cast<clang::CXXRecordDecl>(declaration);
            injected && injected->isInjectedClassName())
        {
            auto name = injected->getQualifiedNameAsString();
            const auto suffix = "::" + injected->getNameAsString();
            if (name.size() > suffix.size() && name.ends_with(suffix)) name.resize(name.size() - suffix.size());
            return name;
        }
    return specialization.getQualifiedNameAsString();
}

// EN: T13-004. A class template specialization that the header names (an alias, the signature of a function
//     or method, a field, a variable) is instantiated by Sema, as a C++ translation unit that uses it would
//     do. Clang selects the primary template or a partial or explicit specialization, checks constraints
//     and lays the record out; an instantiation it rejects stays incomplete and is not imported.
// ES: T13-004. Una especializacion de plantilla de clase que nombra el encabezado (un alias, la firma de una
//     funcion o metodo, un campo, una variable) la instancia Sema, como lo haria una unidad de traduccion
//     C++ que la usa. Clang elige la plantilla primaria o una especializacion parcial o explicita, comprueba
//     las restricciones y calcula el layout; una instanciacion que rechaza queda incompleta y no se importa.
// T13-006: with a seed, only the types that the seed record names. / Con semilla, solo los que nombra ese record.
void instantiateNamedSpecializations(clang::ASTContext& context, clang::Sema& sema,
                                     clang::QualType seed = {}, clang::SourceLocation seedLocation = {})
{
    std::vector<std::pair<clang::QualType, clang::SourceLocation>> pending;
    std::set<const clang::Type*> visited;
    const auto add = [&](clang::QualType type, clang::SourceLocation location) {
        for (int depth = 0; depth < 32 && !type.isNull(); ++depth)
        {
            type = type.getCanonicalType();
            if (type->isDependentType()) return;
            if (const auto* pointer = type->getAs<clang::PointerType>()) { type = pointer->getPointeeType(); continue; }
            if (const auto* reference = type->getAs<clang::ReferenceType>()) { type = reference->getPointeeType(); continue; }
            if (const auto* array = context.getAsArrayType(type)) { type = array->getElementType(); continue; }
            break;
        }
        if (type.isNull() || !type->getAsCXXRecordDecl() || !visited.insert(type.getTypePtr()).second) return;
        pending.emplace_back(type, location);
    };
    std::function<void(const clang::DeclContext&)> visit = [&](const clang::DeclContext& scope) {
        for (const auto* declaration : scope.decls())
        {
            if (llvm::isa<clang::NamespaceDecl>(declaration) || llvm::isa<clang::LinkageSpecDecl>(declaration) ||
                llvm::isa<clang::ExportDecl>(declaration))
            { visit(*llvm::cast<clang::DeclContext>(declaration)); continue; }
            if (declaration->isInvalidDecl()) continue;
            if (const auto* alias = llvm::dyn_cast<clang::TypedefNameDecl>(declaration))
                add(alias->getUnderlyingType(), alias->getLocation());
            else if (const auto* function = llvm::dyn_cast<clang::FunctionDecl>(declaration);
                     function && !function->isDependentContext())
            {
                add(function->getReturnType(), function->getLocation());
                for (const auto* parameter : function->parameters()) add(parameter->getType(), function->getLocation());
            }
            else if (const auto* variable = llvm::dyn_cast<clang::VarDecl>(declaration);
                     variable && !variable->getDeclContext()->isDependentContext())
                add(variable->getType(), variable->getLocation());
            else if (const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(declaration);
                     record && record->isThisDeclarationADefinition() && !record->isDependentContext())
                add(context.getCanonicalTagType(record), record->getLocation());
        }
    };
    if (seed.isNull()) visit(*context.getTranslationUnitDecl());
    else add(seed, seedLocation);
    // A C++ translation unit that only names these types would not instantiate them, so an instantiation Clang
    // rejects is not an error of the header: its diagnostics are suppressed and the record stays out.
    // Una unidad C++ que solo nombra estos tipos no los instanciaria: los diagnosticos se suprimen.
    auto& diagnostics = sema.getDiagnostics();
    const bool suppressed = diagnostics.getSuppressAllDiagnostics();
    diagnostics.setSuppressAllDiagnostics(true);
    // A completed record names further types in its bases, fields and member signatures.
    for (std::size_t index = 0; index < pending.size() && index < 4096; ++index)
    {
        const auto [type, location] = pending[index];
        auto* record = type->getAsCXXRecordDecl();
        if (!record) continue;
        if (llvm::isa<clang::ClassTemplateSpecializationDecl>(record) && !record->hasDefinition())
            if (!sema.isCompleteType(location, type)) continue;
        record = record->getDefinition();
        if (!record || record->isDependentContext() || record->isInvalidDecl()) continue;
        for (const auto& base : record->bases()) add(base.getType(), location);
        for (const auto* field : record->fields()) add(field->getType(), location);
        for (const auto* method : record->methods())
        {
            if (method->isDependentContext() || method->getDescribedFunctionTemplate()) continue;
            add(method->getReturnType(), location);
            for (const auto* parameter : method->parameters()) add(parameter->getType(), location);
        }
    }
    diagnostics.setSuppressAllDiagnostics(suppressed);
}

// EN: T13-006. A record or enum named by its Clang identity, also among the specializations of class templates.
// ES: T13-006. Un record o enum nombrado por su identidad de Clang, tambien entre las especializaciones.
const clang::TagDecl* findTagByUsr(const clang::DeclContext& scope, std::string_view usr, unsigned depth = 0)
{
    if (depth > 64) return nullptr;
    for (const auto* declaration : scope.decls())
    {
        if (llvm::isa<clang::NamespaceDecl>(declaration) || llvm::isa<clang::LinkageSpecDecl>(declaration) ||
            llvm::isa<clang::ExportDecl>(declaration))
        {
            if (const auto* found = findTagByUsr(*llvm::cast<clang::DeclContext>(declaration), usr, depth + 1))
                return found;
            continue;
        }
        if (const auto* classTemplate = llvm::dyn_cast<clang::ClassTemplateDecl>(declaration))
        {
            for (const auto* specialization : classTemplate->specializations())
            {
                if (specialization->isInvalidDecl()) continue;
                if (declarationUsr(specialization) == usr) return specialization;
                if (specialization->isCompleteDefinition())
                    if (const auto* found = findTagByUsr(*specialization, usr, depth + 1)) return found;
            }
            continue;
        }
        const auto* tag = llvm::dyn_cast<clang::TagDecl>(declaration);
        if (!tag || tag->isDependentContext() || tag->isInvalidDecl()) continue;
        if (declarationUsr(tag) == usr) return tag->getDefinition() ? tag->getDefinition() : tag;
        if (const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(tag);
            record && record->isThisDeclarationADefinition())
            if (const auto* found = findTagByUsr(*record, usr, depth + 1)) return found;
    }
    return nullptr;
}

// T13-006: the C++ builtin types a NativeSharp keyword names. / Los tipos basicos C++ que nombra una palabra clave.
clang::QualType builtinTemplateArgumentType(const clang::ASTContext& context, std::string_view name)
{
    if (name == "bool") return context.BoolTy;
    if (name == "signed char") return context.SignedCharTy;
    if (name == "unsigned char") return context.UnsignedCharTy;
    if (name == "char16_t") return context.Char16Ty;
    if (name == "short") return context.ShortTy;
    if (name == "unsigned short") return context.UnsignedShortTy;
    if (name == "int") return context.IntTy;
    if (name == "unsigned int") return context.UnsignedIntTy;
    if (name == "long long") return context.LongLongTy;
    if (name == "unsigned long long") return context.UnsignedLongLongTy;
    if (name == "float") return context.FloatTy;
    if (name == "double") return context.DoubleTy;
    if (name == "void") return context.VoidTy;
    return {};
}

std::string_view describeDeduction(clang::TemplateDeductionResult result)
{
    using R = clang::TemplateDeductionResult;
    switch (result)
    {
    case R::ConstraintsNotSatisfied: return "constraints not satisfied";
    case R::InvalidExplicitArguments: return "invalid explicit template arguments";
    case R::TooManyArguments: return "too many template arguments";
    case R::TooFewArguments: return "too few template arguments";
    case R::SubstitutionFailure: return "substitution failure";
    case R::Incomplete: case R::IncompletePack: return "the template arguments cannot all be determined";
    default: return "template argument deduction failed";
    }
}

// EN: T13-005. The NativeSharp name of a function template specialization: the template name followed by a
//     hash of the qualified template name and the arguments, so overloaded templates share it and NativeSharp
//     overload resolution chooses among their specializations by the call arguments.
// ES: T13-005. El nombre NativeSharp de una especializacion de plantilla de funcion: el nombre de la plantilla
//     seguido de un hash del nombre calificado y de los argumentos, asi lo comparten las plantillas
//     sobrecargadas y la resolucion de sobrecarga de NativeSharp elige entre sus especializaciones por los
//     argumentos de la llamada.
std::string cppFunctionSpecializationName(std::string_view templateName, std::string_view key)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char c : key) { hash ^= c; hash *= 1099511628211ULL; }
    static constexpr char digits[] = "0123456789abcdef";
    std::string name(templateName);
    name += "__f";
    for (int shift = 60; shift >= 0; shift -= 4) name.push_back(digits[(hash >> shift) & 0xF]);
    return name;
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

// EN: T10-010. A default argument is kept only when Clang reduces it to a constant of the parameter's own
//     kind: an integer, a bool, a real floating value or a null pointer. Enumerations, records and any
//     other value stay Unevaluated, so NativeSharp keeps requiring that argument.
// ES: T10-010. Un argumento predeterminado se conserva solo si Clang lo reduce a una constante del tipo del
//     parametro: entero, bool, flotante real o puntero nulo. Enums, records y cualquier otro valor quedan
//     sin evaluar, asi que NativeSharp sigue exigiendo ese argumento.
void captureDefaultArgument(const clang::ParmVarDecl& parameter, clang::ASTContext& context,
                            CppFunctionParameter& captured)
{
    if (!parameter.hasDefaultArg()) return;
    captured.defaultArgumentKind = CppDefaultArgumentKind::Unevaluated;
    if (parameter.hasUnparsedDefaultArg() || parameter.hasUninstantiatedDefaultArg()) return;
    const clang::Expr* expression = parameter.getDefaultArg();
    const clang::QualType type = parameter.getType().getCanonicalType();
    clang::Expr::EvalResult result;
    if (!expression || expression->isValueDependent() || expression->isTypeDependent() ||
        !expression->EvaluateAsRValue(result, context) || result.HasSideEffects) return;
    if (type->isBooleanType() && result.Val.isInt())
    {
        captured.defaultArgumentKind = CppDefaultArgumentKind::Boolean;
        captured.defaultArgumentBits = result.Val.getInt().getBoolValue() ? 1 : 0;
    }
    else if (type->isIntegerType() && !type->isEnumeralType() && result.Val.isInt() &&
             result.Val.getInt().getBitWidth() <= 64)
    {
        const bool isSigned = type->isSignedIntegerType();
        captured.defaultArgumentKind = isSigned ? CppDefaultArgumentKind::Signed : CppDefaultArgumentKind::Unsigned;
        captured.defaultArgumentBits = isSigned
            ? static_cast<std::uint64_t>(result.Val.getInt().getSExtValue()) : result.Val.getInt().getZExtValue();
    }
    else if (type->isRealFloatingType() && result.Val.isFloat())
    {
        llvm::APFloat value = result.Val.getFloat();
        bool lost = false;
        value.convert(llvm::APFloat::IEEEdouble(), llvm::APFloat::rmNearestTiesToEven, &lost);
        if (lost || !value.isFinite()) return;
        captured.defaultArgumentKind = CppDefaultArgumentKind::Floating;
        captured.defaultArgumentBits = value.bitcastToAPInt().getZExtValue();
    }
    else if (type->isPointerType() && result.Val.isLValue() && result.Val.isNullPointer())
        captured.defaultArgumentKind = CppDefaultArgumentKind::NullPointer;
}

// EN: T10-005. Named C++ records that enclose a declaration, outermost first; empty for C, where a tag
//     declared in a struct belongs to the file scope.
// ES: T10-005. Records C++ con nombre que encierran una declaracion, del mas externo al mas interno; vacio
//     en C, donde una etiqueta declarada en un struct pertenece al ambito del archivo.
std::vector<std::string> enclosingCppRecords(const clang::Decl& declaration, const clang::ASTContext& context)
{
    std::vector<std::string> records;
    if (!context.getLangOpts().CPlusPlus) return records;
    for (const clang::DeclContext* current = declaration.getDeclContext(); current; current = current->getParent())
        if (const auto* record = llvm::dyn_cast<clang::RecordDecl>(current))
            records.push_back(record->getNameAsString());
    std::reverse(records.begin(), records.end());
    return records;
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
        result.enumIdentity->containingRecords = enclosingCppRecords(*declaration, context);
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
    // T14-007: `T C::*` and `R (C::*)(A)` keep no kind. / `T C::*` y `R (C::*)(A)` no tienen tipo.
    else if (raw->isMemberPointerType()) result.memberPointer = true;
    else if (raw->isLValueReferenceType()) { result.kind = CppTypeKind::LValueReference; element = raw->getPointeeType(); }
    else if (raw->isRValueReferenceType()) { result.kind = CppTypeKind::RValueReference; element = raw->getPointeeType(); }
    else if (const auto* vector = raw->getAs<clang::VectorType>())
    {
        const auto lane = vector->getElementType();
        if (!raw->isSizelessType() && lane->isBuiltinType() &&
            ((lane->isIntegerType() && !lane->isBooleanType()) ||
             lane->isSpecificBuiltinType(clang::BuiltinType::Float) ||
             lane->isSpecificBuiltinType(clang::BuiltinType::Double)) &&
            context.getTypeSize(lane) >= 8 && context.getTypeSize(lane) <= 64)
        {
            result.kind = CppTypeKind::FixedVector;
            result.arrayCount = vector->getNumElements();
            element = lane;
        }
    }
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
        const auto* specialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record);
        const auto specializationName = specialization ? cppSpecializationName(*specialization) : std::nullopt;
        identity.name = specializationName ? *specializationName : record->getNameAsString();
        if (identity.name.empty() && !context.getLangOpts().CPlusPlus)
            if (const auto* alias = record->getTypedefNameForAnonDecl())
                identity.name = alias->getNameAsString();
        identity.anonymousContext = identity.name.empty();
        for (const clang::DeclContext* current = record; current != nullptr; current = current->getParent())
        {
            identity.specializationContext |= llvm::isa<clang::ClassTemplateSpecializationDecl>(current) &&
                !(current == record && specializationName);
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
        // EN: T10-005. A public enum of a named, non-template C++ record definition, at any public depth.
        // ES: T10-005. Un enum publico de una definicion de record C++ con nombre y sin plantilla, a cualquier
        //     profundidad publica.
        if (const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(declaration))
        {
            if (record->isThisDeclarationADefinition() && !record->isImplicit() && !record->isDependentContext() &&
                !record->getName().empty() && !record->isLambda() && !record->getDescribedClassTemplate() &&
                !llvm::isa<clang::ClassTemplateSpecializationDecl>(record) &&
                (record->getAccess() == clang::AS_none || record->getAccess() == clang::AS_public))
                collectEnums(*record, astContext, seen, result, bufferHashes);
            continue;
        }
        const auto* enumeration = llvm::dyn_cast<clang::EnumDecl>(declaration);
        if (!enumeration || enumeration->isImplicit() || enumeration->isDependentContext() ||
            (llvm::isa<clang::CXXRecordDecl>(enumeration->getDeclContext()) &&
             (enumeration->getAccess() != clang::AS_public || enumeration->getName().empty())) ||
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
        value.containingRecords = enclosingCppRecords(*enumeration, astContext);
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

// EN: T10-002. The C++ namespace path of a context, including inline namespaces; nullopt inside an
//     anonymous namespace, a record or a function, where a name is not reachable by qualification.
// ES: T10-002. La ruta de namespaces C++ de un contexto, incluidos los inline; nullopt dentro de un
//     namespace anonimo, un record o una funcion, donde un nombre no se alcanza por calificacion.
std::optional<std::string> fullNamespacePath(const clang::DeclContext* context)
{
    std::vector<std::string> segments;
    for (; context && !context->isTranslationUnit(); context = context->getParent())
    {
        if (const auto* ns = llvm::dyn_cast<clang::NamespaceDecl>(context))
        {
            if (ns->isAnonymousNamespace()) return std::nullopt;
            segments.push_back(ns->getNameAsString());
        }
        else if (!llvm::isa<clang::LinkageSpecDecl>(context) && !llvm::isa<clang::ExportDecl>(context))
            return std::nullopt;
    }
    std::string path;
    for (auto segment = segments.rbegin(); segment != segments.rend(); ++segment)
        path += (path.empty() ? "" : "::") + *segment;
    return path;
}

// EN: T10-002. Namespace aliases, using-declarations and using-directives at namespace scope give a
//     function further qualified names. A namespace alias reaches every function in its target namespace
//     and in the namespaces nested in it; a using-declaration names only the functions it brings in. A
//     using-directive in a named namespace N makes N::f find the function f of the nominated namespace,
//     transitively, unless N itself declares an f, as C++ qualified lookup does.
// ES: T10-002. Los alias de namespace y las declaraciones y directivas using en un namespace dan a una
//     funcion otros nombres calificados. Un alias alcanza cada funcion de su namespace destino y de los
//     anidados; una declaracion using nombra solo las funciones que introduce. Una directiva using en un
//     namespace con nombre N hace que N::f encuentre la funcion f del namespace nominado, de forma
//     transitiva, salvo que N declare su propia f, como la busqueda calificada de C++.
void addAlternateFunctionNames(clang::ASTContext& context, std::vector<CppFunctionDeclaration>& functions)
{
    std::vector<std::pair<std::string, std::string>> aliases;   // alias path, target path
    std::vector<std::pair<std::string, std::string>> directives;   // namespace path, nominated path
    std::vector<std::pair<std::string, std::string>> shadows;   // target USR, alternate qualified name
    std::function<void(const clang::DeclContext&)> visit = [&](const clang::DeclContext& scope) {
        for (const auto* declaration : scope.decls())
        {
            if (const auto* alias = llvm::dyn_cast<clang::NamespaceAliasDecl>(declaration))
            {
                const auto* target = alias->getNamespace();
                const auto here = fullNamespacePath(alias->getDeclContext());
                const auto there = target ? fullNamespacePath(target) : std::nullopt;
                if (here && there && !there->empty())
                    aliases.emplace_back((here->empty() ? "" : *here + "::") + alias->getNameAsString(), *there);
            }
            else if (const auto* directive = llvm::dyn_cast<clang::UsingDirectiveDecl>(declaration))
            {
                const auto* nominated = directive->getNominatedNamespace();
                const auto here = fullNamespacePath(directive->getDeclContext());
                const auto there = nominated ? fullNamespacePath(nominated) : std::nullopt;
                if (here && there && !here->empty() && !there->empty() && *here != *there)
                    directives.emplace_back(*here, *there);
            }
            else if (const auto* shadow = llvm::dyn_cast<clang::UsingShadowDecl>(declaration))
            {
                const auto* function = llvm::dyn_cast<clang::FunctionDecl>(shadow->getTargetDecl());
                const auto here = fullNamespacePath(shadow->getDeclContext());
                const auto usr = function ? declarationUsr(function) : std::nullopt;
                if (function && here && usr)
                    shadows.emplace_back(*usr, (here->empty() ? "" : *here + "::") + function->getNameAsString());
            }
            else if (const auto* nested = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
            {
                if (!nested->isAnonymousNamespace()) visit(*nested);
            }
            else if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
                visit(*linkage);
        }
    };
    visit(*context.getTranslationUnitDecl());
    if (aliases.empty() && shadows.empty() && directives.empty()) return;
    // A name a namespace declares itself (directly, through an inline namespace or a using-declaration)
    // hides the same name reached through a using-directive.
    std::set<std::string> declared;
    for (const auto& function : functions)
    {
        declared.insert(function.qualifiedName);
        declared.insert(function.alternateQualifiedNames.begin(), function.alternateQualifiedNames.end());
    }
    for (const auto& shadow : shadows) declared.insert(shadow.second);
    for (auto& function : functions)
    {
        if (function.languageLinkage == CppLanguageLinkage::C && !context.getLangOpts().CPlusPlus) continue;
        std::vector<std::string> names{function.qualifiedName};
        names.insert(names.end(), function.alternateQualifiedNames.begin(), function.alternateQualifiedNames.end());
        for (const auto& [usr, alternate] : shadows)
            if (function.clangUsr && *function.clangUsr == usr) names.push_back(alternate);
        // Each new name is expanded again, so aliases and directives compose; the bound keeps a cycle of
        // directives finite.
        for (std::size_t next = 0; next < names.size() && names.size() < 64; ++next)
        {
            const auto separator = names[next].rfind("::");
            if (separator == std::string::npos) continue;
            const auto path = names[next].substr(0, separator);
            std::vector<std::string> reached;
            for (const auto& [alias, target] : aliases)
                if (path == target || path.starts_with(target + "::"))
                    reached.push_back(alias + path.substr(target.size()) + "::" + function.name);
            for (const auto& [scope, target] : directives)
                if (path == target || path.starts_with(target + "::"))
                {
                    auto alternate = scope + path.substr(target.size()) + "::" + function.name;
                    if (!declared.contains(alternate)) reached.push_back(std::move(alternate));
                }
            for (auto& alternate : reached)
                if (std::find(names.begin(), names.end(), alternate) == names.end())
                    names.push_back(std::move(alternate));
        }
        for (auto& alternate : names)
            if (alternate != function.qualifiedName &&
                std::find(function.alternateQualifiedNames.begin(), function.alternateQualifiedNames.end(), alternate) ==
                    function.alternateQualifiedNames.end())
                function.alternateQualifiedNames.push_back(std::move(alternate));
    }
}

// EN: T13-005. The NativeSharp name of a function template specialization requested from NativeSharp, by its
//     canonical declaration. Only those specializations are imported, under that name.
// ES: T13-005. El nombre NativeSharp de una especializacion de plantilla de funcion pedida desde NativeSharp,
//     por su declaracion canonica. Solo esas especializaciones se importan, con ese nombre.
using CppFunctionSpecializationNames = std::map<const clang::FunctionDecl*, std::string>;

void collectFreeFunctions(const clang::DeclContext& context, clang::ASTContext& astContext,
                          clang::MangleContext& mangler,
                          clang::CodeGenerator* generator,
                          std::set<const clang::FunctionDecl*>& seen,
                          std::vector<CppFunctionDeclaration>& result,
                          const CppFunctionSpecializationNames* specializationNames = nullptr)
{
    const auto collect = [&](const clang::FunctionDecl* function) {
        const auto requested = function && specializationNames && function->getPrimaryTemplate() ?
            specializationNames->find(function->getCanonicalDecl()) : CppFunctionSpecializationNames::const_iterator{};
        const bool specialization = specializationNames && function && function->getPrimaryTemplate() &&
            requested != specializationNames->end();
        if (!function || function->isImplicit() || function->getDeclName().isEmpty() ||
            function->getDescribedFunctionTemplate() || function->isDependentContext() ||
            (function->getPrimaryTemplate() && function->getTemplateSpecializationKind() != clang::TSK_ExplicitSpecialization &&
             !specialization) ||
            !seen.insert(function->getCanonicalDecl()).second)
            return;

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
        // T13-005: a requested specialization is named like a function of the template's namespace.
        // T13-005: una especializacion pedida se nombra como una funcion del namespace de la plantilla.
        if (specialization)
        {
            const auto path = fullNamespacePath(function->getDeclContext());
            if (!path) return;
            value.name = requested->second;
            value.qualifiedName = path->empty() ? value.name : *path + "::" + value.name;
            value.templateSpecialization = true;
        }
        // T10-002: the path through inline namespaces names the same function.
        else if (const auto path = fullNamespacePath(function->getDeclContext()); path && !path->empty())
        {
            const auto spelled = *path + "::" + value.name;
            if (spelled != value.qualifiedName) value.alternateQualifiedNames.push_back(spelled);
        }
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
            CppFunctionParameter captured{
                parameter->getNameAsString(), parameter->getType().getAsString(),
                parameter->getType().getCanonicalType().getAsString(),
                describeType(parameter->getType(), astContext), parameter->hasDefaultArg()};
            captureDefaultArgument(*parameter, astContext, captured);
            if (const auto* size = parameter->getAttr<clang::PassObjectSizeAttr>())
            {
                captured.objectSizeMode = static_cast<std::uint8_t>(size->getType());
                captured.objectSizeDynamic = size->isDynamic();
            }
            value.parameters.push_back(std::move(captured));
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
            const bool plannedScalarConvention = abi_plan::cppPlannedScalarConventionId(value.callingConvention) != 0;
            if (astContext.getLangOpts().CPlusPlus && generator &&
                (value.callingConvention == "regcall" || value.callingConvention == "sysv_abi" ||
                 value.callingConvention == "preserve_none" || plannedScalarConvention) &&
                !value.variadic && value.hasPrototype && value.externalSymbol &&
                (function->getReturnType()->isVoidType() || !function->getReturnType()->isIncompleteType()) &&
                std::all_of(function->param_begin(), function->param_end(), [](const auto* parameter) {
                    return !parameter->getType()->isIncompleteType();
                }) &&
                (value.callingConvention == "preserve_none" || plannedScalarConvention ||
                 function->getReturnType()->isRecordType() ||
                 std::any_of(function->param_begin(), function->param_end(), [](const auto* parameter) {
                     return parameter->getType()->isRecordType();
                 })))
            {
                std::string error;
                auto plan = abi_plan::copyPlan(*generator, *function, error);
                CppFunctionType signature;
                signature.returnType = value.returnType;
                for (const auto& parameter : value.parameters) signature.parameterTypes.push_back(parameter.type);
                signature.callingConvention = value.callingConvention;
                signature.hasPrototype = value.hasPrototype;
                signature.variadic = value.variadic;
                // EN: T09-003. swiftcall and swiftasynccall keep only a checked scalar plan.
                // ES: T09-003. swiftcall y swiftasynccall solo conservan un plan escalar comprobado.
                if (plan && plan->symbol == *value.externalSymbol && plannedScalarConvention)
                {
                    if (abi_plan::isCppPlannedScalarConventionPlan(*plan, signature, *value.externalSymbol))
                        value.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(*plan));
                }
                else if (plan && plan->symbol == *value.externalSymbol &&
                    (value.callingConvention != "preserve_none" ||
                     abi_plan::isPreserveNoneCppRecordRoundTrip(*plan, signature, *value.externalSymbol) ||
                     abi_plan::isPreserveNoneCppScalarPlan(*plan, signature, *value.externalSymbol) ||
                     abi_plan::isPreserveNoneCppScalarWithRecord(*plan, signature, *value.externalSymbol)))
                    value.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(*plan));
            }
        }
        const auto location = astContext.getSourceManager().getPresumedLoc(function->getLocation());
        if (location.isValid())
            value.location = CppSourcePoint{
                pathFromUtf8(location.getFilename()), location.getLine(), location.getColumn()};
        result.push_back(std::move(value));
    };
    for (const clang::Decl* declaration : context.decls())
    {
        if (const auto* nested = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
        {
            collectFreeFunctions(*nested, astContext, mangler, generator, seen, result, specializationNames);
            continue;
        }
        if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
        {
            collectFreeFunctions(*linkage, astContext, mangler, generator, seen, result, specializationNames);
            continue;
        }
        if (const auto* functionTemplate = llvm::dyn_cast<clang::FunctionTemplateDecl>(declaration))
        {
            if (specializationNames)
                for (const auto* specialization : functionTemplate->specializations())
                    if (specializationNames->contains(specialization->getCanonicalDecl())) collect(specialization);
            continue;
        }
        collect(llvm::dyn_cast<clang::FunctionDecl>(declaration));
    }
}

// EN: T12-004. A public member operator of a C++ record is imported as the free operator NativeSharp
//     operator lookup consults (T10-016): its first operand is the object, by const reference for a const
//     operator. The call goes through a Clang adapter, a free function whose body calls the member on that
//     operand, so overload selection, this and the result follow Clang. The operator belongs to the innermost
//     namespace of its record, where argument-dependent lookup finds it.
// ES: T12-004. Un operador miembro publico de un record C++ se importa como el operador libre que consulta la
//     busqueda de operadores de NativeSharp (T10-016): su primer operando es el objeto, por referencia const
//     si el operador es const. La llamada pasa por un adaptador de Clang, una funcion libre cuyo cuerpo llama
//     al miembro sobre ese operando, asi que la seleccion de sobrecarga, this y el resultado siguen a Clang.
//     El operador pertenece al namespace mas interno de su record, donde lo encuentra la busqueda dependiente
//     de argumentos.
void collectMemberOperators(const clang::DeclContext& context, clang::ASTContext& astContext,
                            std::vector<CppFunctionDeclaration>& result)
{
    for (const clang::Decl* declaration : context.decls())
    {
        if (llvm::isa<clang::NamespaceDecl>(declaration) || llvm::isa<clang::LinkageSpecDecl>(declaration) ||
            llvm::isa<clang::ExportDecl>(declaration))
        {
            collectMemberOperators(*llvm::cast<clang::DeclContext>(declaration), astContext, result);
            continue;
        }
        const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(declaration);
        if (!record || !record->isThisDeclarationADefinition() || !record->isCompleteDefinition() ||
            record->isDependentContext() || record->isLambda() || !record->getIdentifier() ||
            !record->isExternallyVisible())
            continue;
        collectMemberOperators(*record, astContext, result);
        const auto recordUsr = declarationUsr(record);
        const auto* enclosing = record->getEnclosingNamespaceContext();
        const auto path = enclosing ? fullNamespacePath(enclosing) : std::nullopt;
        if (!recordUsr || !path) continue;
        for (const clang::CXXMethodDecl* method : record->methods())
        {
            if (!method->isOverloadedOperator() || method->isStatic() || method->isDeleted() ||
                method->isImplicit() || method->getAccess() != clang::AS_public || method->isVariadic() ||
                method->getDescribedFunctionTemplate() || method->isDependentContext() ||
                method->getRefQualifier() != clang::RQ_None || method->isVolatile() ||
                llvm::isa<clang::CXXConversionDecl>(method))
                continue;
            switch (method->getOverloadedOperator())
            {
            case clang::OO_Equal: case clang::OO_Call: case clang::OO_Subscript: case clang::OO_Arrow:
            case clang::OO_ArrowStar: case clang::OO_New: case clang::OO_Delete: case clang::OO_Array_New:
            case clang::OO_Array_Delete: case clang::OO_Coawait:
                continue;
            default: break;
            }
            const auto methodUsr = declarationUsr(method);
            if (!methodUsr) continue;
            clang::QualType self = astContext.getCanonicalTagType(record);
            if (method->isConst()) self.addConst();
            self = astContext.getLValueReferenceType(self);
            std::vector<clang::QualType> parameterTypes{self};
            for (const auto* parameter : method->parameters()) parameterTypes.push_back(parameter->getType());
            const auto* prototype = method->getType()->getAs<clang::FunctionProtoType>();
            clang::FunctionProtoType::ExtProtoInfo info;
            if (prototype) info.ExceptionSpec = prototype->getExtProtoInfo().ExceptionSpec;
            const auto functionType = astContext.getFunctionType(method->getReturnType(), parameterTypes, info);
            CppFunctionDeclaration value;
            value.clangUsr = methodUsr;
            value.name = method->getNameAsString();
            value.qualifiedName = path->empty() ? value.name : *path + "::" + value.name;
            value.canonicalFunctionType = functionType.getCanonicalType().getAsString();
            value.callingConvention = "cdecl";
            value.languageLinkage = CppLanguageLinkage::Cpp;
            value.returnTypeSpelling = method->getReturnType().getAsString();
            value.canonicalReturnTypeSpelling = method->getReturnType().getCanonicalType().getAsString();
            value.returnType = describeType(method->getReturnType(), astContext);
            value.variadic = false;
            value.hasPrototype = true;
            value.externalLinkage = true;
            value.dllImport = false;
            if (prototype && !clang::isUnresolvedExceptionSpec(prototype->getExceptionSpecType()))
                value.isNonThrowing = prototype->isNothrow();
            value.parameters.push_back(CppFunctionParameter{"self", self.getAsString(),
                self.getCanonicalType().getAsString(), describeType(self, astContext), false});
            for (const clang::ParmVarDecl* parameter : method->parameters())
            {
                CppFunctionParameter captured{
                    parameter->getNameAsString(), parameter->getType().getAsString(),
                    parameter->getType().getCanonicalType().getAsString(),
                    describeType(parameter->getType(), astContext), parameter->hasDefaultArg()};
                captureDefaultArgument(*parameter, astContext, captured);
                value.parameters.push_back(std::move(captured));
            }
            value.externalSymbol = cppMemberOperatorSymbol(*recordUsr, *methodUsr);
            const auto location = astContext.getSourceManager().getPresumedLoc(method->getLocation());
            if (location.isValid())
                value.location = CppSourcePoint{
                    pathFromUtf8(location.getFilename()), location.getLine(), location.getColumn()};
            result.push_back(std::move(value));
        }
    }
}

// T13-022, T14-002: the scalar value Clang's constant evaluator gives a const, constant-initialized variable.
// T13-022, T14-002: el valor escalar que da el evaluador de Clang a una variable const de inicializacion constante.
std::optional<CppConstantValue> evaluateScalarConstant(const clang::VarDecl& variable, clang::ASTContext& astContext)
{
    if (!variable.getType().isConstQualified() || variable.getType().isVolatileQualified() || variable.isInvalidDecl())
        return std::nullopt;
    const auto* definition = variable.getDefinition() ? variable.getDefinition() : &variable;
    const auto type = definition->getType().getCanonicalType().getUnqualifiedType();
    const clang::APValue* evaluated = definition->hasInit() && !type->isDependentType() &&
        definition->hasConstantInitialization() ? definition->evaluateValue() : nullptr;
    if (evaluated && evaluated->isInt() && type->isIntegralOrEnumerationType() && astContext.getTypeSize(type) <= 64)
    {
        CppConstantValue constant;
        const auto& integer = evaluated->getInt();
        if (type->isBooleanType())
        {
            constant.kind = CppConstantValue::Kind::Boolean;
            constant.boolean = integer.getBoolValue();
        }
        else constant.integerBits = integer.isSigned() ? static_cast<std::uint64_t>(integer.getExtValue())
                                                       : integer.getZExtValue();
        return constant;
    }
    if (evaluated && evaluated->isFloat() && type->isRealFloatingType() && astContext.getTypeSize(type) <= 64)
    {
        CppConstantValue constant;
        constant.kind = CppConstantValue::Kind::Floating;
        constant.floating = evaluated->getFloat().convertToDouble();
        return constant;
    }
    return std::nullopt;
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
        // EN: T15. A partial specialization of a variable template is a VarDecl of the namespace, but it is a
        //     template pattern: it has no symbol and no value (MSVC <type_traits>, `_Is_specialization_v`).
        // ES: T15. Una especializacion parcial de una plantilla de variable es un VarDecl del namespace, pero es un
        //     patron de plantilla: no tiene simbolo ni valor (<type_traits> de MSVC, `_Is_specialization_v`).
        if (!variable || !variable->isFileVarDecl() || variable->isImplicit() || variable->isTemplated() ||
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
        // EN: T10-004. Clang guards a C++ thread_local access with the CRT's on-demand TLS initialization unless
        //     a declaration is constinit or the visible definition is constant-initialized.
        // ES: T10-004. Clang protege el acceso a un thread_local C++ con la inicializacion TLS bajo demanda del
        //     CRT salvo que una declaracion sea constinit o la definicion visible tenga inicializacion constante.
        if (value.threadLocal && astContext.getLangOpts().CPlusPlus &&
            variable->getTLSKind() == clang::VarDecl::TLS_Dynamic)
        {
            bool constant = false;
            for (const auto* redeclaration : variable->redecls())
                constant |= redeclaration->hasAttr<clang::ConstInitAttr>();
            if (const auto* definition = variable->getDefinition())
                constant |= definition->hasInit() && definition->hasConstantInitialization();
            value.cppTlsOnDemand = !constant;
        }
        value.dllImport = variable->hasAttr<clang::DLLImportAttr>();
        // EN: T13-021, T13-022. A C++ variable with internal linkage (a namespace-scope constexpr or const one)
        //     has no symbol in any library. When it is const, not volatile, of scalar type and constant-
        //     initialized, it is exposed with the value that Clang's constant evaluator gives it.
        // ES: T13-021, T13-022. Una variable C++ con enlace interno (constexpr o const en un namespace) no tiene
        //     simbolo en ninguna biblioteca. Si es const, no volatile, escalar y de inicializacion constante, se
        //     expone con el valor que le da el evaluador de constantes de Clang.
        if (astContext.getLangOpts().CPlusPlus && !value.externalLinkage && !value.threadLocal)
            value.constantValue = evaluateScalarConstant(*variable, astContext);
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
                    std::vector<CppRecordDeclaration>& result, std::map<unsigned, std::string>& bufferHashes,
                    clang::Sema* sema = nullptr, std::vector<const clang::RecordDecl*>* observed = nullptr)
{
    for (const clang::Decl* declaration : context.decls())
    {
        if (const auto* nested = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
        {
            // T13-007: the mirrors of NativeSharp structs are NativeSharp types. / Los espejos son tipos NativeSharp.
            if (nested->getParent()->isTranslationUnit() && nested->getName() == llvm::StringRef(cppNativeStructNamespace.data(), cppNativeStructNamespace.size())) continue;
            collectRecords(*nested, astContext, mangler, seen, result, bufferHashes, sema, observed);
            continue;
        }
        if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
        {
            collectRecords(*linkage, astContext, mangler, seen, result, bufferHashes, sema, observed);
            continue;
        }
        // EN: T13-004. The specializations of a class template that Clang instantiated are records too.
        // ES: T13-004. Las especializaciones de una plantilla de clase que Clang instancio tambien son records.
        std::vector<const clang::RecordDecl*> candidates;
        if (const auto* classTemplate = llvm::dyn_cast<clang::ClassTemplateDecl>(declaration))
        {
            for (const auto* specialization : classTemplate->specializations())
                if (specialization->isCompleteDefinition() && !specialization->isInvalidDecl() &&
                    cppSpecializationName(*specialization))
                    candidates.push_back(specialization);
        }
        else if (const auto* plain = llvm::dyn_cast<clang::RecordDecl>(declaration)) candidates.push_back(plain);
        for (const clang::RecordDecl* record : candidates)
        {
        if (record->isImplicit() ||
            record->isDependentContext() || !seen.insert(record->getCanonicalDecl()).second)
            continue;
        if (const auto* definition = record->getDefinition()) record = definition;
        const auto* specialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record);
        const auto specializationName = specialization ? cppSpecializationName(*specialization) : std::nullopt;
        if (specialization && !specializationName) continue;

        const auto* anonymousAlias = !astContext.getLangOpts().CPlusPlus && record->getName().empty()
            ? record->getTypedefNameForAnonDecl() : nullptr;
        if (record->getName().empty() && !anonymousAlias) continue;

        CppRecordDeclaration value;
        value.clangUsr = declarationUsr(record);
        value.isCxx = astContext.getLangOpts().CPlusPlus;
        value.name = anonymousAlias ? anonymousAlias->getNameAsString() : record->getNameAsString();
        value.qualifiedName = anonymousAlias ? anonymousAlias->getQualifiedNameAsString()
                                             : record->getQualifiedNameAsString();
        if (specializationName)
        {
            value.name = *specializationName;
            value.qualifiedName = cppSpecializationQualifiedName(*specialization);
        }
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
            value.specializationContext |= llvm::isa<clang::ClassTemplateSpecializationDecl>(current) &&
                !(current == record && specializationName);
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
                const auto captureMethod = [&](const clang::CXXMethodDecl* method) {
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
                    // EN: T11-007. A destructor or a defaulted special member declared without an exception
                    //     specification is implicitly noexcept or not; Sema evaluates that specification.
                    // ES: T11-007. Un destructor o un miembro especial por defecto declarado sin especificacion
                    //     de excepciones es noexcept o no de forma implicita; Sema evalua esa especificacion.
                    if (const auto* prototype = method->getType()->getAs<clang::FunctionProtoType>())
                    {
                        if (sema && !method->isDeleted() && prototype->getExceptionSpecType() == clang::EST_Unevaluated)
                            prototype = sema->ResolveExceptionSpec(method->getLocation(), prototype);
                        if (prototype && !clang::isUnresolvedExceptionSpec(prototype->getExceptionSpecType()))
                            projected.isNonThrowing = prototype->isNothrow();
                    }
                    projected.isVirtual = method->isVirtual();
                    projected.isPureVirtual = method->isPureVirtual();
                    projected.isDeleted = method->isDeleted();
                    projected.isTrivial = method->isTrivial();
                    projected.variadic = method->isVariadic();
                    for (const clang::ParmVarDecl* parameter : method->parameters())
                    {
                        CppFunctionParameter captured{
                            parameter->getNameAsString(), parameter->getType().getAsString(),
                            parameter->getType().getCanonicalType().getAsString(),
                            describeType(parameter->getType(), astContext), parameter->hasDefaultArg()};
                        captureDefaultArgument(*parameter, astContext, captured);
                        if (const auto* size = parameter->getAttr<clang::PassObjectSizeAttr>())
                        {
                            captured.objectSizeMode = static_cast<std::uint8_t>(size->getType());
                            captured.objectSizeDynamic = size->isDynamic();
                        }
                        projected.parameters.push_back(std::move(captured));
                    }
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
                    return projected;
                };
                for (const clang::CXXMethodDecl* method : cppRecord->methods())
                {
                    if (method->isImplicit() || method->isDependentContext() ||
                        method->getDescribedFunctionTemplate()) continue;
                    CppMethodDeclaration projected = captureMethod(method);
                    value.methods.push_back(std::move(projected));
                }
                std::set<std::string> ownNames;
                for (const auto& field : value.fields) ownNames.insert(field.name);
                for (const auto& method : value.methods) ownNames.insert(method.name);
                const auto recordUsr = declarationUsr(cppRecord);
                // EN: T12-005. A virtual method is called through the Clang adapter, so the call dispatches
                //     through the vtable of the object as C++ does.
                // ES: T12-005. Un metodo virtual se llama mediante el adaptador de Clang, asi que la llamada
                //     despacha por la vtable del objeto como en C++.
                for (auto& method : value.methods)
                    if (method.isVirtual && method.kind == CppMethodKind::Method && !method.isStatic &&
                        recordUsr && method.clangUsr)
                        method.dispatchSymbol = cppMemberThunkSymbol(*recordUsr, *method.clangUsr);
                // EN: T12-001. The fields of public non-virtual bases are fields of the derived object at
                //     Clang's offset. A name the derived record or a nearer base declares hides the base
                //     field, and a name two bases reach at the same depth is ambiguous, as in C++, so neither
                //     is promoted. A virtual base has no fixed offset and stays out.
                // ES: T12-001. Los campos de las bases publicas no virtuales son campos del objeto derivado en
                //     el offset de Clang. Un nombre que declara el derivado o una base mas cercana oculta el
                //     campo de la base, y un nombre que dos bases alcanzan a la misma profundidad es ambiguo,
                //     como en C++, asi que no se promueve ninguno. Una base virtual no tiene offset fijo.
                std::set<std::string> declared;
                for (const auto& field : value.fields) declared.insert(field.name);
                for (const auto& method : value.methods) declared.insert(method.name);
                std::vector<std::pair<const clang::CXXRecordDecl*, std::uint64_t>> level{{cppRecord, 0}};
                for (std::size_t depth = 0; depth < 32 && !level.empty(); ++depth)
                {
                    std::vector<std::pair<const clang::CXXRecordDecl*, std::uint64_t>> next;
                    std::map<std::string, std::vector<CppRecordField>> reached;
                    std::set<std::string> methodNames;
                    for (const auto& [derived, derivedOffset] : level)
                    {
                        const auto& derivedLayout = astContext.getASTRecordLayout(derived);
                        for (const clang::CXXBaseSpecifier& base : derived->bases())
                        {
                            const auto* baseRecord = base.getType()->getAsCXXRecordDecl();
                            if (base.isVirtual() || base.getAccessSpecifier() != clang::AS_public || !baseRecord ||
                                !baseRecord->isCompleteDefinition() || baseRecord->isDependentContext()) continue;
                            const auto baseOffset = derivedOffset + static_cast<std::uint64_t>(
                                derivedLayout.getBaseClassOffset(baseRecord).getQuantity()) * astContext.getCharWidth();
                            const auto& baseLayout = astContext.getASTRecordLayout(baseRecord);
                            unsigned fieldIndex = 0;
                            for (const clang::FieldDecl* field : baseRecord->fields())
                            {
                                const auto offset = baseLayout.getFieldOffset(fieldIndex++);
                                if (field->getAccess() != clang::AS_public || field->isAnonymousStructOrUnion() ||
                                    field->getName().empty()) continue;
                                CppRecordField promoted;
                                promoted.clangUsr = declarationUsr(field);
                                promoted.name = field->getNameAsString();
                                promoted.type = describeType(field->getType(), astContext);
                                promoted.access = CppAccess::Public;
                                promoted.offsetBits = baseOffset + offset;
                                if (field->isBitField()) promoted.bitWidth = field->getBitWidthValue();
                                reached[promoted.name].push_back(std::move(promoted));
                            }
                            for (const auto* method : baseRecord->methods())
                                if (!method->isImplicit()) methodNames.insert(method->getNameAsString());
                            next.emplace_back(baseRecord, baseOffset);
                        }
                    }
                    for (auto& [name, fields] : reached)
                    {
                        if (declared.contains(name)) continue;
                        if (fields.size() == 1 && !methodNames.contains(name)) value.fields.push_back(std::move(fields.front()));
                    }
                    for (const auto& [name, fields] : reached) declared.insert(name);
                    declared.insert(methodNames.begin(), methodNames.end());
                    level = std::move(next);
                }
                // EN: T12-003. Every base reached only through public non-virtual derivation, by one path, is a
                //     pointer conversion target at the sum of Clang's base offsets along that path.
                // ES: T12-003. Toda base alcanzada solo por derivacion publica no virtual, por un camino, es
                //     destino de conversion de puntero, en la suma de los offsets de base de Clang del camino.
                {
                    std::vector<const clang::CXXRecordDecl*> pendingBases{cppRecord};
                    std::set<const clang::CXXRecordDecl*> reachedBases;
                    while (!pendingBases.empty())
                    {
                        const auto* current = pendingBases.back();
                        pendingBases.pop_back();
                        for (const clang::CXXBaseSpecifier& base : current->bases())
                            if (const auto* baseRecord = base.getType()->getAsCXXRecordDecl();
                                baseRecord && baseRecord->isCompleteDefinition() && !baseRecord->isDependentContext() &&
                                reachedBases.insert(baseRecord->getCanonicalDecl()).second)
                                pendingBases.push_back(baseRecord);
                    }
                    for (const auto* baseRecord : reachedBases)
                    {
                        const auto baseUsr = declarationUsr(baseRecord);
                        clang::CXXBasePaths paths(true, true, true);
                        if (!baseUsr || !cppRecord->isDerivedFrom(baseRecord, paths) ||
                            paths.isAmbiguous(astContext.getCanonicalType(astContext.getCanonicalTagType(baseRecord))))
                            continue;
                        const auto& path = paths.front();
                        std::uint64_t offset = 0;
                        bool direct = path.Access == clang::AS_public;
                        for (const auto& element : path)
                        {
                            const auto* step = element.Base->getType()->getAsCXXRecordDecl();
                            if (element.Base->isVirtual() || element.Base->getAccessSpecifier() != clang::AS_public || !step)
                            { direct = false; break; }
                            offset += static_cast<std::uint64_t>(
                                astContext.getASTRecordLayout(element.Class).getBaseClassOffset(step).getQuantity());
                        }
                        if (direct) value.upcasts.push_back({*baseUsr, offset});
                    }
                    std::sort(value.upcasts.begin(), value.upcasts.end(), [](const auto& a, const auto& b) {
                        return a.baseClangUsr < b.baseClangUsr; });
                }
                // The object layout reads fields in address order; base subobjects precede the derived fields.
                std::stable_sort(value.fields.begin(), value.fields.end(), [](const auto& a, const auto& b) {
                    return a.offsetBits.value_or(0) < b.offsetBits.value_or(0);
                });
                // EN: T12-004. The instance methods of public bases, virtual or not, are methods of the derived
                //     record called through the Clang adapter, which adjusts this. The same hiding and ambiguity
                //     rules as for fields apply, by name over every member a base declares.
                // ES: T12-004. Los metodos de instancia de las bases publicas, virtuales o no, son metodos del
                //     record derivado llamados mediante el adaptador de Clang, que ajusta this. Rigen las mismas
                //     reglas de ocultacion y ambiguedad que para los campos, por nombre sobre todo miembro que
                //     declara una base.
                if (recordUsr)
                {
                    std::set<std::string> hidden = ownNames;
                    std::vector<const clang::CXXRecordDecl*> bases{cppRecord};
                    for (std::size_t depth = 0; depth < 32 && !bases.empty(); ++depth)
                    {
                        std::vector<const clang::CXXRecordDecl*> next;
                        std::map<std::string, std::set<const clang::CXXRecordDecl*>> owners;
                        std::map<std::string, std::vector<const clang::CXXMethodDecl*>> overloads;
                        for (const auto* derived : bases)
                            for (const clang::CXXBaseSpecifier& base : derived->bases())
                            {
                                const auto* baseRecord = base.getType()->getAsCXXRecordDecl();
                                if (base.getAccessSpecifier() != clang::AS_public || !baseRecord ||
                                    !baseRecord->isCompleteDefinition() || baseRecord->isDependentContext() ||
                                    std::find(next.begin(), next.end(), baseRecord) != next.end()) continue;
                                next.push_back(baseRecord);
                                for (const auto* field : baseRecord->fields())
                                    owners[field->getNameAsString()].insert(baseRecord);
                                for (const auto* method : baseRecord->methods())
                                {
                                    if (method->isImplicit()) continue;
                                    owners[method->getNameAsString()].insert(baseRecord);
                                    if (method->isStatic() || method->isDependentContext() ||
                                        method->getDescribedFunctionTemplate() || method->isDeleted() ||
                                        method->getAccess() != clang::AS_public ||
                                        llvm::isa<clang::CXXConstructorDecl>(method) ||
                                        llvm::isa<clang::CXXDestructorDecl>(method)) continue;
                                    overloads[method->getNameAsString()].push_back(method);
                                }
                            }
                        for (const auto& [name, methods] : overloads)
                        {
                            if (hidden.contains(name) || owners[name].size() != 1) continue;
                            // Two subobjects of the declaring base (a non-virtual diamond) make the call
                            // ambiguous in C++. / Dos subobjetos de la base declarante la hacen ambigua.
                            clang::CXXBasePaths paths(true, true, false);
                            if (!cppRecord->isDerivedFrom(methods.front()->getParent(), paths) ||
                                paths.isAmbiguous(astContext.getCanonicalType(
                                    astContext.getCanonicalTagType(methods.front()->getParent())))) continue;
                            for (const auto* method : methods)
                            {
                                auto inherited = captureMethod(method);
                                const auto usr = declarationUsr(method);
                                if (!usr) continue;
                                inherited.inherited = true;
                                inherited.dllImport = false;
                                inherited.dispatchSymbol = cppMemberThunkSymbol(*recordUsr, *usr);
                                value.methods.push_back(std::move(inherited));
                            }
                        }
                        for (const auto& [name, records] : owners) hidden.insert(name);
                        bases = std::move(next);
                    }
                }
            }
        }
        if (!value.isCxx && value.complete)
            collectAnonymousCRecords(*record, astContext, value.anonymousCRecords);
        if (observed) observed->push_back(record);
        result.push_back(std::move(value));
        if (record->isCompleteDefinition())
            collectRecords(*record, astContext, mangler, seen, result, bufferHashes, sema, observed);
        }
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
        // T13-004: a specialization is found by the qualified name Clang gives its members.
        // T13-004: una especializacion se encuentra por el nombre calificado que Clang da a sus miembros.
        if (const auto* classTemplate = llvm::dyn_cast<clang::ClassTemplateDecl>(declaration))
            for (const auto* specialization : classTemplate->specializations())
                if (specialization->isCompleteDefinition() && !specialization->isInvalidDecl() &&
                    cppSpecializationName(*specialization) &&
                    cppSpecializationQualifiedName(*specialization) == name &&
                    seen.insert(specialization->getCanonicalDecl()).second)
                    matches.push_back(specialization->getDefinition());
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
    if (depth > 16 || type.isNull()) return false;
    // T09-003: const and volatile do not change how Clang classifies a field and the replay writes them
    // back; any other qualifier keeps the record out of the replay.
    auto qualifiers = type.getLocalQualifiers();
    qualifiers.removeConst();
    qualifiers.removeVolatile();
    if (qualifiers.hasQualifiers()) return false;
    type = type.getLocalUnqualifiedType();
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
        collectFreeFunctions(*context.getTranslationUnitDecl(), context, *mangler, nullptr, seen, result,
                             &functionSpecializationNames_);
        if (std::any_of(result.begin(), result.end(), [&](const auto& value) {
                return value.callingConvention == "regcall" ||
                    (context.getLangOpts().CPlusPlus &&
                     (value.callingConvention == "sysv_abi" || value.callingConvention == "preserve_none" ||
                      abi_plan::cppPlannedScalarConventionId(value.callingConvention) != 0));
            }))
        {
            llvm::LLVMContext llvmContext;
            auto generator = clang::CreateLLVMCodeGen(ast_->getDiagnostics(), "nsc.regcall.symbol",
                ast_->getVirtualFileSystemPtr(), ast_->getHeaderSearchOpts(), ast_->getPreprocessorOpts(),
                ast_->getCodeGenOpts(), llvmContext);
            if (!generator) return std::nullopt;
            generator->Initialize(context);
            seen.clear();
            result.clear();
            collectFreeFunctions(*context.getTranslationUnitDecl(), context, *mangler, generator.get(), seen, result,
                                 &functionSpecializationNames_);
        }
        if (context.getLangOpts().CPlusPlus) collectMemberOperators(*context.getTranslationUnitDecl(), context, result);
        addAlternateFunctionNames(context, result);
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
             prototype->getCallConv() != clang::CC_X86RegCall &&
             prototype->getCallConv() != clang::CC_PreserveNone)) return std::nullopt;
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
            (prototype->getCallConv() != clang::CC_X86RegCall &&
             prototype->getCallConv() != clang::CC_X86_64SysV &&
             prototype->getCallConv() != clang::CC_PreserveNone)) return std::nullopt;
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
        plan->symbol = prototype->getCallConv() == clang::CC_X86RegCall ?
            "__regcall3____nsc_pointer_abi_probe" : "__nsc_pointer_abi_probe";
        return plan;
    }
    std::optional<abi_plan::VariadicCallPlan> cVariadicCallAbiPlan(
        std::string_view name, std::string_view canonicalType,
        const std::vector<CppType>& optionalArguments) const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_ || name.empty() || canonicalType.empty()) return std::nullopt;
        auto& context = ast_->getASTContext();
        const clang::FunctionDecl* match = nullptr;
        bool ambiguous = false;
        std::function<void(const clang::DeclContext&)> visit = [&](const clang::DeclContext& scope) {
            for (const auto* declaration : scope.decls())
            {
                if (const auto* nested = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
                { visit(*nested); continue; }
                if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
                { visit(*linkage); continue; }
                const auto* candidate = llvm::dyn_cast<clang::FunctionDecl>(declaration);
                if (!candidate || candidate->isImplicit() || !candidate->isExternallyVisible() ||
                    candidate->getNameAsString() != name || candidate->isDependentContext()) continue;
                candidate = candidate->getMostRecentDecl();
                if (candidate->getType().getCanonicalType().getAsString() != canonicalType) continue;
                if (match && match->getCanonicalDecl() != candidate->getCanonicalDecl()) ambiguous = true;
                else match = candidate;
            }
        };
        visit(*context.getTranslationUnitDecl());
        if (!match || ambiguous) return std::nullopt;
        return copyVariadicPlanForDeclaration(match, optionalArguments);
    }
    std::optional<abi_plan::VariadicCallPlan> cVariadicFunctionPointerCallAbiPlan(
        std::string_view name, std::string_view canonicalType,
        const std::vector<CppType>& optionalArguments) const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_ || name.empty() || canonicalType.empty()) return std::nullopt;
        auto& context = ast_->getASTContext();
        const clang::TypedefNameDecl* match = nullptr;
        bool ambiguous = false;
        std::function<void(const clang::DeclContext&)> visit = [&](const clang::DeclContext& scope) {
            for (const auto* declaration : scope.decls())
            {
                if (const auto* ns = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
                { if (!ns->isAnonymousNamespace()) visit(*ns); continue; }
                if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
                { visit(*linkage); continue; }
                const auto* candidate = llvm::dyn_cast<clang::TypedefNameDecl>(declaration);
                if (!candidate || candidate->isImplicit() || candidate->hasAttrs() ||
                    candidate->getQualifiedNameAsString() != name ||
                    candidate->getUnderlyingType().getCanonicalType().getAsString() != canonicalType) continue;
                if (match && match->getCanonicalDecl() != candidate->getCanonicalDecl()) ambiguous = true;
                else match = candidate;
            }
        };
        visit(*context.getTranslationUnitDecl());
        if (!match || ambiguous) return std::nullopt;
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
        auto plan = copyVariadicPlanForDeclaration(declaration, optionalArguments);
        // The declaration is synthetic and the call is indirect: retain a stable probe identity.
        if (plan && context.getLangOpts().CPlusPlus) plan->symbol = "__nsc_variadic_pointer_abi_probe";
        return plan;
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
                    input.recordIdentity->specializationContext || !input.recordIdentity->containingRecords.empty()) return std::nullopt;
                std::string qualified;
                for (const auto& segment : input.recordIdentity->namespaceSegments)
                {
                    if (segment.empty()) return std::nullopt;
                    qualified += segment + "::";
                }
                qualified += input.recordIdentity->name;
                std::set<const clang::TagDecl*> seen;
                std::vector<const clang::RecordDecl*> candidates;
                findComparableRecords(*context.getTranslationUnitDecl(), qualified, seen, candidates);
                if (candidates.size() != 1 || declarationUsr(candidates[0]) != input.recordIdentity->clangUsr)
                    return std::nullopt;
                if (const auto* cpp = llvm::dyn_cast<clang::CXXRecordDecl>(candidates[0]))
                    if (!cpp->isCompleteDefinition() || !cpp->isTriviallyCopyable() ||
                        !cpp->isStandardLayout() || !cpp->hasTrivialDestructor()) return std::nullopt;
                result = context.getTypeDeclType(static_cast<const clang::TypeDecl*>(candidates[0]));
            }
            else if (input.kind == CppTypeKind::Enumeration)
            {
                if (!input.enumIdentity || input.enumIdentity->anonymousContext ||
                    !input.enumIdentity->clangUsr || input.enumIdentity->qualifiedName.empty()) return std::nullopt;
                const clang::EnumDecl* found = nullptr;
                bool ambiguous = false;
                std::function<void(const clang::DeclContext&)> visit = [&](const clang::DeclContext& scope) {
                    for (const auto* item : scope.decls())
                    {
                        if (const auto* ns = llvm::dyn_cast<clang::NamespaceDecl>(item))
                        { if (!ns->isAnonymousNamespace()) visit(*ns); continue; }
                        if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(item))
                        { visit(*linkage); continue; }
                        const auto* candidate = llvm::dyn_cast<clang::EnumDecl>(item);
                        if (!candidate || !candidate->isCompleteDefinition() || !candidate->isScoped() ||
                            candidate->getQualifiedNameAsString() != input.enumIdentity->qualifiedName ||
                            declarationUsr(candidate) != input.enumIdentity->clangUsr) continue;
                        if (found && found->getCanonicalDecl() != candidate->getCanonicalDecl()) ambiguous = true;
                        else found = candidate;
                    }
                };
                visit(*context.getTranslationUnitDecl());
                if (!found || ambiguous || !input.enumUnderlying || found->getIntegerType().isNull() ||
                    describeType(found->getIntegerType(), context).canonicalSpelling != input.enumUnderlying->canonicalSpelling)
                    return std::nullopt;
                result = context.getTypeDeclType(static_cast<const clang::TypeDecl*>(found));
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
                  (input.kind == CppTypeKind::Record && result->isRecordType()) ||
                  (input.kind == CppTypeKind::Enumeration && result->isEnumeralType()))) return std::nullopt;
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
        std::vector<const clang::RecordDecl*> observed;
        collectRecords(*context.getTranslationUnitDecl(), context, *mangler, seen, result, bufferHashes,
                       ast_->hasSema() ? &ast_->getSema() : nullptr, &observed);
        // EN: T12-001. A trivially copyable C++ record that the C rules may not describe (a base, a declared
        //     constructor, a non-public field, not an aggregate) is returned by value as Clang's code generator
        //     classifies a free function returning it: on the Microsoft ABI such a record goes through memory.
        // ES: T12-001. Un record C++ trivialmente copiable que las reglas de C quiza no describen (una base, un
        //     constructor declarado, un campo no publico, no agregado) se devuelve por valor como clasifica el
        //     generador de codigo de Clang una funcion libre que lo devuelve: en el ABI de Microsoft va en memoria.
        if (context.getLangOpts().CPlusPlus && observed.size() == result.size())
        {
            llvm::LLVMContext llvmContext;
            std::unique_ptr<clang::CodeGenerator> generator;
            for (std::size_t index = 0; index < result.size(); ++index)
            {
                auto& value = result[index];
                const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(observed[index]);
                if (!value.isCxx || !value.complete || !value.valueTraits || !value.valueTraits->triviallyCopyable ||
                    !record || !record->isCompleteDefinition() || record->isDependentContext() ||
                    (record->getNumBases() == 0 && !record->hasUserDeclaredConstructor() &&
                     !record->hasPrivateFields() && !record->hasProtectedFields() && record->isAggregate()))
                    continue;
                if (!generator)
                {
                    generator = clang::CreateLLVMCodeGen(ast_->getDiagnostics(), "nsc.cpp.record.return.abi",
                        ast_->getVirtualFileSystemPtr(), ast_->getHeaderSearchOpts(), ast_->getPreprocessorOpts(),
                        ast_->getCodeGenOpts(), llvmContext);
                    if (!generator) break;
                    generator->Initialize(context);
                }
                const clang::QualType recordType = context.getCanonicalTagType(record);
                const auto functionType = context.getCanonicalType(
                    context.getFunctionType(recordType, {}, clang::FunctionProtoType::ExtProtoInfo()));
                const auto prototype = functionType.getAs<clang::FunctionProtoType>();
                if (!prototype) continue;
                const auto& arranged = clang::CodeGen::arrangeFreeFunctionType(generator->CGM(), prototype);
                value.indirectReturn = arranged.getReturnInfo().isIndirect();
            }
        }
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
        // EN: T09-003. Alignment attributes (_Alignas, alignas, aligned, __declspec(align)) only raise the
        //     alignment the layout already shows; the replay rebuilds them from the offsets and the record
        //     alignment. Any other attribute, and alignment on a record with bitfields, keeps it out.
        // ES: T09-003. Los atributos de alineacion (_Alignas, alignas, aligned, __declspec(align)) solo elevan la
        //     alineacion que el layout ya muestra; el replay los reconstruye a partir de los desplazamientos y de
        //     la alineacion del record. Cualquier otro atributo, o alineacion con campos de bits, lo deja fuera.
        const auto onlyAlignment = [](const clang::Decl& declaration) {
            return std::all_of(declaration.attr_begin(), declaration.attr_end(), [](const clang::Attr* attribute) {
                const auto* aligned = llvm::dyn_cast<clang::AlignedAttr>(attribute);
                return aligned && !aligned->isAlignmentDependent();
            });
        };
        if (!onlyAlignment(*record) || record->field_empty()) return false;
        bool aligned = record->hasAttrs(), bitfield = false;
        for (const auto* field : record->fields())
        {
            const auto type = field->getType();
            if (type.isNull() || !onlyAlignment(*field) || field->isAnonymousStructOrUnion() ||
                (field->isBitField() && ((field->getIdentifier() && field->getBitWidthValue() == 0) ||
                                         !type->isIntegerType())) ||
                !hasReplayBuiltinOrArray(type))
                return false;
            aligned = aligned || field->hasAttrs();
            bitfield = bitfield || field->isBitField();
        }
        return !(aligned && bitfield);
    }
    std::optional<bool> cppRecordHasPlainReplaySyntax(std::string_view name) const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_ || name.empty() || !ast_->getASTContext().getLangOpts().CPlusPlus) return std::nullopt;
        auto& context = ast_->getASTContext();
        std::set<const clang::TagDecl*> seen;
        std::vector<const clang::RecordDecl*> records;
        findComparableRecords(*context.getTranslationUnitDecl(), name, seen, records);
        if (records.size() != 1) return std::nullopt;
        const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(records.front());
        if (!record || !record->isCompleteDefinition() || !record->isStruct() || record->hasAttrs() ||
            !record->isTriviallyCopyable() ||
            !record->isStandardLayout() || !record->hasTrivialDestructor() ||
            record->getNumBases() != 0 || record->field_empty()) return false;
        for (const auto* parent = record->getDeclContext(); !parent->isTranslationUnit(); parent = parent->getParent())
        {
            const auto* ns = llvm::dyn_cast<clang::NamespaceDecl>(parent);
            if (!ns || ns->isAnonymousNamespace() || !ns->getIdentifier()) return false;
        }
        for (const auto* field : record->fields())
        {
            const auto type = field->getType();
            if (type.isNull() || type.hasQualifiers() || field->hasAttrs() ||
                field->isAnonymousStructOrUnion() || field->isBitField()) return false;
            const auto* array = llvm::dyn_cast<clang::ConstantArrayType>(type.getTypePtr());
            if (!type->isBuiltinType() &&
                (!array || !array->getElementType()->isBuiltinType() || !hasReplayBuiltinOrArray(type))) return false;
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
            {
                CppFunctionParameter captured{parameter->getNameAsString(),
                    parameter->getType().getAsString(), parameter->getType().getCanonicalType().getAsString(),
                    describeType(parameter->getType(), context), parameter->hasDefaultArg()};
                captureDefaultArgument(*parameter, context, captured);
                if (const auto* size = parameter->getAttr<clang::PassObjectSizeAttr>())
                {
                    captured.objectSizeMode = static_cast<std::uint8_t>(size->getType());
                    captured.objectSizeDynamic = size->isDynamic();
                }
                item.parameters.push_back(std::move(captured));
            }
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
    std::vector<std::string> emitCppMemberThunkIR(
        const std::vector<std::string>& symbols, std::vector<std::string>& emitted) const override
    {
        // EN: T12-004. Each requested adapter names a record of this translation unit and a method it
        //     reaches (cppMemberThunkSymbol). Its source is appended to the analyzed source and Clang compiles
        //     the call `self->m(args)` on the derived type, so this adjustment, virtual dispatch and virtual
        //     bases are Clang's. A class result is returned through the pointer after self, which is the
        //     physical ABI of a Microsoft C++ instance method; a reference result as its address.
        // ES: T12-004. Cada adaptador pedido nombra un record de esta unidad de traduccion y un metodo que
        //     alcanza (cppMemberThunkSymbol). Su fuente se anade a la fuente analizada y Clang compila la
        //     llamada `self->m(args)` sobre el tipo derivado, asi que el ajuste de this, el despacho virtual y
        //     las bases virtuales son de Clang. Un resultado de clase se devuelve por el puntero que sigue a
        //     self, que es el ABI fisico de un metodo de instancia C++ de Microsoft; una referencia, por su
        //     direccion.
        emitted.clear();
        std::lock_guard lock(mutex_);
        if (!ast_ || source_.empty() || !ast_->getASTContext().getLangOpts().CPlusPlus) return {};
        std::set<std::string> wanted;
        for (const auto& symbol : symbols)
            if (std::string_view(symbol).starts_with(cppMemberThunkPrefix)) wanted.insert(symbol);
        if (wanted.empty()) return {};
        auto& context = ast_->getASTContext();
        struct Request { std::string symbol; const clang::CXXRecordDecl* record; const clang::CXXMethodDecl* method;
                         bool freeOperator = false; };
        std::vector<Request> requests;
        std::set<std::string> found;
        std::function<void(const clang::DeclContext&)> visit = [&](const clang::DeclContext& scope) {
            for (const auto* declaration : scope.decls())
            {
                if (const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(declaration))
                {
                    if (!record->isThisDeclarationADefinition() || !record->isCompleteDefinition() ||
                        record->isDependentContext() || record->getQualifiedNameAsString().find('(') != std::string::npos)
                        continue;
                    if (const auto recordUsr = declarationUsr(record))
                    {
                        std::vector<const clang::CXXRecordDecl*> pending{record};
                        std::set<const clang::CXXRecordDecl*> seen;
                        while (!pending.empty())
                        {
                            const auto* current = pending.back();
                            pending.pop_back();
                            if (!seen.insert(current).second) continue;
                            for (const auto* method : current->methods())
                            {
                                const auto usr = declarationUsr(method);
                                if (!usr) continue;
                                auto symbol = cppMemberThunkSymbol(*recordUsr, *usr);
                                if (wanted.contains(symbol) && found.insert(symbol).second)
                                    requests.push_back({std::move(symbol), record, method});
                                // T12-004: an own member operator, called as a free function.
                                if (current != record || !method->isOverloadedOperator()) continue;
                                auto operatorSymbol = cppMemberOperatorSymbol(*recordUsr, *usr);
                                if (wanted.contains(operatorSymbol) && found.insert(operatorSymbol).second)
                                    requests.push_back({std::move(operatorSymbol), record, method, true});
                            }
                            for (const auto& base : current->bases())
                                if (const auto* baseRecord = base.getType()->getAsCXXRecordDecl();
                                    baseRecord && baseRecord->isCompleteDefinition()) pending.push_back(baseRecord);
                        }
                    }
                    visit(*record);
                }
                else if (llvm::isa<clang::NamespaceDecl>(declaration) || llvm::isa<clang::LinkageSpecDecl>(declaration) ||
                         llvm::isa<clang::ExportDecl>(declaration))
                    visit(*llvm::cast<clang::DeclContext>(declaration));
            }
        };
        visit(*context.getTranslationUnitDecl());
        if (requests.empty()) return {};
        clang::PrintingPolicy policy(context.getLangOpts());
        policy.SuppressTagKeyword = false;
        policy.FullyQualifiedName = true;
        std::vector<std::pair<std::size_t, std::string>> snippets;
        std::set<std::string> written;
        for (std::size_t requestIndex = 0; requestIndex < requests.size(); ++requestIndex)
        {
            const auto& request = requests[requestIndex];
            const auto* method = request.method;
            const auto& name = request.symbol;
            // EN: Every type is named through an alias of its canonical spelling, so a function pointer or an
            //     array type spells correctly. A type without a spelling (anonymous, local, lambda) has no
            //     NativeSharp identity either, so no projected member uses it; its adapter is skipped.
            // ES: Cada tipo se nombra con un alias de su escritura canonica, asi un puntero a funcion o un array
            //     se escriben bien. Un tipo sin escritura (anonimo, local, lambda) tampoco tiene identidad
            //     NativeSharp, asi que ningun miembro proyectado lo usa; su adaptador se omite.
            std::string aliases;
            bool spellable = true;
            const auto alias = [&](std::string suffix, clang::QualType type) {
                const auto aliasName = name + "_" + suffix;
                const auto spelling = type.getCanonicalType().getAsString(policy);
                if (spelling.find("(anonymous") != std::string::npos || spelling.find("(unnamed") != std::string::npos ||
                    spelling.find("(lambda") != std::string::npos) spellable = false;
                aliases += "using " + aliasName + " = " + spelling + ";\n";
                return aliasName;
            };
            const auto* owner = method->getParent();
            const auto self = alias("s", context.getCanonicalTagType(request.record));
            const auto ownerType = alias("o", context.getCanonicalTagType(owner));
            const auto result = method->getReturnType();
            const auto resultAlias = alias("r", result);
            std::string parameterTypes;
            std::string parameters = std::string(method->isConst() ? "const " : "") + self +
                (request.freeOperator ? "& self" : "* self");
            const bool classResult = !request.freeOperator && result->isRecordType();
            const bool referenceResult = !request.freeOperator && result->isLValueReferenceType();
            if (classResult) parameters += ", " + resultAlias + "* out";
            std::string arguments;
            for (unsigned i = 0; i < method->getNumParams(); ++i)
            {
                const auto type = method->getParamDecl(i)->getType();
                const auto index = std::to_string(i);
                const auto parameterAlias = alias("p" + index, type);
                if (i) { parameterTypes += ", "; arguments += ", "; }
                parameterTypes += parameterAlias;
                parameters += ", " + parameterAlias + " a" + index;
                arguments += type->isRValueReferenceType() ? "static_cast<" + parameterAlias + ">(a" + index + ")"
                                                           : "a" + index;
            }
            if (!spellable || !written.insert(name).second) continue;
            // EN: The member is named by a pointer to member of its own class, cast to its exact type: overload
            //     resolution cannot choose another member of the same name, and the call through the pointer
            //     keeps virtual dispatch and converts self to the declaring base as Clang does.
            // ES: El miembro se nombra con un puntero a miembro de su propia clase, convertido a su tipo exacto:
            //     la resolucion de sobrecarga no puede elegir otro miembro del mismo nombre, y la llamada por el
            //     puntero conserva el despacho virtual y convierte self a la base declarante como lo hace Clang.
            const std::string member = "static_cast<" + resultAlias + " (" + ownerType + "::*)(" + parameterTypes + ")" +
                (method->isConst() ? " const" : "") + ">(&" + ownerType + "::" + method->getNameAsString() + ")";
            const std::string call = (request.freeOperator ? "(self.*" : "(self->*") + member + ")(" + arguments + ")";
            std::string resultType = "void";
            std::string body = call + ";";
            // A class result is trivially copyable (the projection only maps those): its bytes are copied to the
            // caller's memory. / Un resultado de clase es trivialmente copiable: se copian sus bytes.
            if (classResult) body = "auto&& r = " + call + "; __builtin_memcpy(out, &r, sizeof(r));";
            else if (referenceResult) { resultType = alias("q", result.getNonReferenceType()) + "*"; body = "return &" + call + ";"; }
            else if (!result->isVoidType()) { resultType = resultAlias; body = "return " + call + ";"; }
            snippets.emplace_back(requestIndex, aliases + "extern \"C\" inline " + resultType + " " + name + "(" +
                parameters + ") { " + body + " }\n");
        }
        // EN: All adapters are compiled with one reparse of the source. If Clang rejects one, each is compiled
        //     alone, so an adapter Clang rejects is not emitted and does not hide the others.
        // ES: Todos los adaptadores se compilan con un solo reanalisis de la fuente. Si Clang rechaza uno, se
        //     compila cada uno por separado: el que Clang rechaza no se emite ni oculta a los demas.
        std::vector<std::string> modules;
        const auto compile = [&](std::span<const std::pair<std::size_t, std::string>> selected) -> bool {
            std::string code = source_ + "\n";
            std::set<std::string> names;
            for (const auto& [index, snippet] : selected) { code += snippet; names.insert(requests[index].symbol); }
            llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> filesystem(llvm::vfs::createPhysicalFileSystem().release());
            if (filesystem->setCurrentWorkingDirectory(utf8(options_.workingDirectory))) return false;
            std::vector<CppSessionDiagnostic> thunkDiagnostics;
            detail::DiagnosticCollector collector(thunkDiagnostics);
            auto unit = detail::runClangSessionAction(code, detail::buildClangInvocationArguments(options_), inputPath_,
                utf8(options_.driverReferencePath), &collector, std::move(filesystem), CppCancellationToken{});
            if (!unit || unit->getDiagnostics().hasErrorOccurred()) return false;
            auto& thunkContext = unit->getASTContext();
            llvm::LLVMContext llvmContext;
            auto generator = clang::CreateLLVMCodeGen(unit->getDiagnostics(), "nsc.cpp.thunk",
                unit->getVirtualFileSystemPtr(), unit->getHeaderSearchOpts(), unit->getPreprocessorOpts(),
                unit->getCodeGenOpts(), llvmContext);
            if (!generator) return false;
            generator->Initialize(thunkContext);
            for (const auto* declaration : thunkContext.getTranslationUnitDecl()->decls())
            {
                const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration);
                if (!linkage) continue;
                for (const auto* inner : linkage->decls())
                {
                    const auto* function = llvm::dyn_cast<clang::FunctionDecl>(inner);
                    if (!function || !function->getIdentifier() || !names.contains(function->getName().str()) ||
                        !function->hasBody()) continue;
                    if (!generator->HandleTopLevelDecl(clang::DeclGroupRef(const_cast<clang::FunctionDecl*>(function))))
                        return false;
                    generator->GetAddrOfGlobal(clang::GlobalDecl(function), false);
                }
            }
            generator->HandleTranslationUnit(thunkContext);
            auto module = generator->ReleaseModule();
            if (!module) return false;
            std::vector<std::string> defined;
            for (const auto& name : names)
            {
                auto* function = module->getFunction(name);
                if (!function || function->isDeclaration()) continue;
                function->setLinkage(llvm::GlobalValue::WeakODRLinkage);
                if (!function->hasComdat()) function->setComdat(module->getOrInsertComdat(name));
                defined.push_back(name);
            }
            if (defined.empty() || llvm::verifyModule(*module, &llvm::errs())) return false;
            std::string ir;
            llvm::raw_string_ostream stream(ir);
            module->print(stream, nullptr);
            modules.push_back(std::move(ir));
            emitted.insert(emitted.end(), defined.begin(), defined.end());
            return true;
        };
        if (snippets.empty()) return modules;
        if (!compile(snippets) && snippets.size() > 1)
            for (const auto& snippet : snippets) compile(std::span(&snippet, 1));
        return modules;
    }
    // EN: T13-006. The class and function templates declared at namespace scope, by their qualified C++ names,
    //     also through inline namespaces. NativeSharp writes a specialization of one of them by that name.
    // ES: T13-006. Las plantillas de clase y de funcion declaradas en un namespace, por sus nombres C++
    //     calificados, tambien a traves de namespaces inline. NativeSharp escribe una especializacion con ese nombre.
    std::vector<CppTemplateName> templatesSnapshot() const override
    {
        std::lock_guard lock(mutex_);
        std::vector<CppTemplateName> result;
        if (!ast_ || !ast_->getASTContext().getLangOpts().CPlusPlus) return result;
        std::set<std::pair<std::string, bool>> seen;
        visitNamespaceTemplates([&](const clang::TemplateDecl& declaration, bool function) {
            for (auto& name : templateNames(declaration))
                if (seen.emplace(name, function).second) result.push_back({std::move(name), function});
        });
        return result;
    }

    // EN: T13-006, T13-016, T13-017, T13-005, T13-014. Sema instantiates the template with the arguments that
    //     NativeSharp wrote, as a C++ translation unit naming the specialization at the end of the header would:
    //     Clang converts the arguments, selects a partial or explicit specialization, checks constraints and
    //     completes the record, or deduces the remaining arguments of a function template, packs included, and
    //     instantiates its definition. A rejection carries the diagnostics of Clang.
    // ES: T13-006, T13-016, T13-017, T13-005, T13-014. Sema instancia la plantilla con los argumentos que
    //     escribio NativeSharp, como lo haria una unidad C++ que nombra la especializacion al final del
    //     encabezado: Clang convierte los argumentos, elige una especializacion parcial o explicita, comprueba
    //     las restricciones y completa el record, o deduce los argumentos restantes de una plantilla de funcion,
    //     paquetes incluidos, e instancia su definicion. Un rechazo lleva los diagnosticos de Clang.
    std::optional<CppTemplateInstantiation> instantiateTemplate(
        std::string_view qualifiedName, bool function, std::span<const CppTemplateArgument> arguments) const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_ || !ast_->hasSema() || !ast_->getASTContext().getLangOpts().CPlusPlus) return std::nullopt;
        auto& context = ast_->getASTContext();
        auto& sema = ast_->getSema();
        std::vector<clang::TemplateDecl*> templates;
        std::set<const clang::Decl*> seenTemplates;
        visitNamespaceTemplates([&](const clang::TemplateDecl& declaration, bool isFunction) {
            if (isFunction != function) return;
            const auto names = templateNames(declaration);
            if (std::find(names.begin(), names.end(), qualifiedName) != names.end() &&
                seenTemplates.insert(declaration.getCanonicalDecl()).second)
                templates.push_back(const_cast<clang::TemplateDecl*>(&declaration));
        });
        if (templates.empty()) return std::nullopt;

        CppTemplateInstantiation result;
        const auto& sources = context.getSourceManager();
        const auto location = sources.getLocForEndOfFile(sources.getMainFileID());
        const auto firstDiagnostic = diagnostics_.size();
        // A note Clang attaches to the diagnostic before these. / Una nota que Clang une al diagnostico anterior.
        const auto firstNote = diagnostics_.empty() ? std::size_t(0) : diagnostics_.back().notes.size();
        const auto collectDiagnostics = [&] {
            bool error = false;
            if (firstDiagnostic)
                for (auto note = firstNote; note < diagnostics_[firstDiagnostic - 1].notes.size(); ++note)
                    result.diagnostics.push_back(describeCppDiagnostic(diagnostics_[firstDiagnostic - 1].notes[note]));
            for (auto index = firstDiagnostic; index < diagnostics_.size(); ++index)
            {
                const auto& diagnostic = diagnostics_[index];
                const bool isError = diagnostic.severity == CppSessionDiagnosticSeverity::Error ||
                    diagnostic.severity == CppSessionDiagnosticSeverity::Fatal;
                error = error || isError;
                if (!isError && diagnostic.severity != CppSessionDiagnosticSeverity::Note) continue;
                result.diagnostics.push_back(describeCppDiagnostic(diagnostic));
                for (const auto& note : diagnostic.notes) result.diagnostics.push_back(describeCppDiagnostic(note));
            }
            return error;
        };

        // The arguments as a C++ translation unit writes them. / Los argumentos como los escribe una unidad C++.
        clang::PrintingPolicy policy(context.getLangOpts());
        policy.SuppressTagKeyword = true;
        policy.FullyQualifiedName = true;
        std::string key(qualifiedName);
        key += '<';
        clang::TemplateArgumentListInfo explicitArguments(location, location);
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            const auto& argument = arguments[index];
            if (index) key += ", ";
            // T13-013: a class template for a template template parameter.
            // T13-013: una plantilla de clase para un parametro plantilla de plantilla.
            if (argument.kind == CppTemplateArgument::Kind::Template)
            {
                clang::ClassTemplateDecl* argumentTemplate = nullptr;
                visitNamespaceTemplates([&](const clang::TemplateDecl& declaration, bool isFunction) {
                    const auto names = templateNames(declaration);
                    if (!isFunction && !argumentTemplate &&
                        std::find(names.begin(), names.end(), argument.templateName) != names.end())
                        argumentTemplate = const_cast<clang::ClassTemplateDecl*>(
                            llvm::cast<clang::ClassTemplateDecl>(&declaration));
                });
                if (!argumentTemplate) return std::nullopt;
                key += "template " + argumentTemplate->getQualifiedNameAsString();
                explicitArguments.addArgument(sema.getTrivialTemplateArgumentLoc(
                    clang::TemplateArgument(clang::TemplateName(argumentTemplate)), clang::QualType(), location));
                continue;
            }
            if (argument.kind == CppTemplateArgument::Kind::NativeStruct)
            {
                clang::QualType type = argument.nativeStruct ? nativeStructMirror(*argument.nativeStruct) : clang::QualType();
                if (type.isNull())
                {
                    result.diagnostics.push_back("template argument " + std::to_string(index + 1) +
                        " is a NativeSharp struct that has no C++ equivalent");
                    return result;
                }
                for (unsigned depth = 0; depth < argument.pointerDepth; ++depth) type = context.getPointerType(type);
                key += type.getCanonicalType().getAsString(policy);
                explicitArguments.addArgument(
                    sema.getTrivialTemplateArgumentLoc(clang::TemplateArgument(type), clang::QualType(), location));
                continue;
            }
            if (argument.kind == CppTemplateArgument::Kind::Type)
            {
                clang::QualType type;
                if (argument.recordUsr)
                {
                    const auto* tag = findTagByUsr(*context.getTranslationUnitDecl(), *argument.recordUsr);
                    // Another session of the project declares that type. / Otra sesion del proyecto lo declara.
                    if (!tag) return std::nullopt;
                    type = context.getCanonicalTagType(tag);
                }
                else type = builtinTemplateArgumentType(context, argument.builtin);
                if (type.isNull())
                {
                    result.diagnostics.push_back("template argument " + std::to_string(index + 1) +
                        " is not a type that a C++ template argument can name");
                    return result;
                }
                for (unsigned depth = 0; depth < argument.pointerDepth; ++depth) type = context.getPointerType(type);
                key += type.getCanonicalType().getAsString(policy);
                explicitArguments.addArgument(
                    sema.getTrivialTemplateArgumentLoc(clang::TemplateArgument(type), clang::QualType(), location));
                continue;
            }
            clang::Expr* value = nullptr;
            if (argument.kind == CppTemplateArgument::Kind::Boolean)
            {
                value = clang::CXXBoolLiteralExpr::Create(context, argument.boolean, context.BoolTy, location);
                key += argument.boolean ? "true" : "false";
            }
            else
            {
                const bool negative = argument.integral < 0;
                const auto magnitude = negative ? std::uint64_t(0) - static_cast<std::uint64_t>(argument.integral)
                                                : static_cast<std::uint64_t>(argument.integral);
                const bool unsignedMagnitude = magnitude > static_cast<std::uint64_t>(INT64_MAX);
                value = clang::IntegerLiteral::Create(context, llvm::APInt(64, magnitude),
                    unsignedMagnitude ? context.UnsignedLongLongTy : context.LongLongTy, location);
                if (negative)
                    value = clang::UnaryOperator::Create(context, value, clang::UO_Minus, value->getType(),
                        clang::VK_PRValue, clang::OK_Ordinary, location, false, clang::FPOptionsOverride());
                key += std::to_string(argument.integral);
            }
            explicitArguments.addArgument(clang::TemplateArgumentLoc(clang::TemplateArgument(value, false), value));
        }
        key += '>';

        if (!function)
        {
            auto* classTemplate = llvm::cast<clang::ClassTemplateDecl>(templates.front());
            const clang::QualType type = sema.CheckTemplateIdType(clang::ElaboratedTypeKeyword::None,
                clang::TemplateName(classTemplate), location, explicitArguments, nullptr, false);
            if (collectDiagnostics() || type.isNull() || type->isDependentType())
            {
                if (result.diagnostics.empty()) result.diagnostics.push_back("Clang rejected the template arguments");
                return result;
            }
            if (sema.RequireCompleteType(location, type, clang::diag::err_incomplete_type) || collectDiagnostics())
            {
                result.diagnostics.clear();
                collectDiagnostics();
                if (result.diagnostics.empty()) result.diagnostics.push_back("the specialization is incomplete");
                return result;
            }
            const auto* specialization =
                llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(type->getAsCXXRecordDecl());
            const auto name = specialization ? cppSpecializationName(*specialization) : std::nullopt;
            if (!specialization || specialization->isInvalidDecl() || !name)
            {
                result.diagnostics.push_back("the specialization cannot be imported as a NativeSharp type");
                return result;
            }
            // The types that the specialization names, as for one the header names.
            // Los tipos que nombra la especializacion, como para una que nombra el encabezado.
            instantiateNamedSpecializations(context, sema, type.getCanonicalType(), location);
            result.instantiated = true;
            result.name = *name;
            result.clangUsr = declarationUsr(specialization);
            result.nativeLayouts = nativeStructLayouts_;
            return result;
        }

        std::vector<clang::FunctionDecl*> found;
        std::vector<std::string> failures;
        for (auto* candidate : templates)
        {
            auto* functionTemplate = llvm::cast<clang::FunctionTemplateDecl>(candidate);
            clang::TemplateArgumentListInfo copy = explicitArguments;
            clang::sema::TemplateDeductionInfo info(location);
            clang::FunctionDecl* specialization = nullptr;
            const auto outcome = sema.DeduceTemplateArguments(functionTemplate, &copy, specialization, info);
            if (outcome == clang::TemplateDeductionResult::Success && specialization && !specialization->isInvalidDecl())
            {
                found.push_back(specialization);
                continue;
            }
            failures.emplace_back(describeDeduction(outcome));
            if (outcome == clang::TemplateDeductionResult::ConstraintsNotSatisfied)
                sema.DiagnoseUnsatisfiedConstraint(info.AssociatedConstraintsSatisfaction);
        }
        if (found.empty())
        {
            std::string message = "no function template '" + std::string(qualifiedName) +
                "' accepts the template arguments";
            for (std::size_t index = 0; index < failures.size(); ++index)
                message += (index ? "; " : ": ") + failures[index];
            result.diagnostics.push_back(std::move(message));
            collectDiagnostics();
            return result;
        }
        // The definitions, and the ones they use, as their first use would. / Las definiciones, como su primer uso.
        bool instantiated = false;
        for (auto* specialization : found)
            if (!specialization->isDefined() && specialization->isImplicitlyInstantiable())
            {
                sema.InstantiateFunctionDefinition(location, specialization, true, false, true);
                instantiated = true;
            }
        if (instantiated) sema.PerformPendingInstantiations();
        if (collectDiagnostics()) return result;
        result.instantiated = true;
        result.nativeLayouts = nativeStructLayouts_;
        result.name = cppFunctionSpecializationName(templates.front()->getNameAsString(), key);
        result.clangUsr = declarationUsr(found.front());
        for (auto* specialization : found) functionSpecializationNames_.emplace(specialization->getCanonicalDecl(), result.name);
        return result;
    }

    // EN: T13-005. A call to a function template without explicit arguments: each template with the name
    //     deduces its arguments from the types of the call's arguments (a scalar as a prvalue, a record as an
    //     lvalue, like a NativeSharp variable), as C++ deduction does; Sema instantiates each deduced
    //     specialization and NativeSharp overload resolution chooses among them by the arguments.
    // ES: T13-005. Una llamada a una plantilla de funcion sin argumentos explicitos: cada plantilla con el
    //     nombre deduce sus argumentos de los tipos de los argumentos de la llamada (un escalar como prvalue,
    //     un record como lvalue, como una variable NativeSharp), como la deduccion de C++; Sema instancia cada
    //     especializacion deducida y la resolucion de sobrecarga de NativeSharp elige entre ellas.
    std::optional<CppTemplateInstantiation> deduceFunctionTemplateCall(
        std::string_view qualifiedName, std::span<const CppTemplateArgument> argumentTypes) const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_ || !ast_->hasSema() || !ast_->getASTContext().getLangOpts().CPlusPlus) return std::nullopt;
        auto& context = ast_->getASTContext();
        auto& sema = ast_->getSema();
        std::vector<clang::FunctionTemplateDecl*> templates;
        std::set<const clang::Decl*> seenTemplates;
        visitNamespaceTemplates([&](const clang::TemplateDecl& declaration, bool isFunction) {
            if (!isFunction) return;
            const auto names = templateNames(declaration);
            if (std::find(names.begin(), names.end(), qualifiedName) != names.end() &&
                seenTemplates.insert(declaration.getCanonicalDecl()).second)
                templates.push_back(const_cast<clang::FunctionTemplateDecl*>(
                    llvm::cast<clang::FunctionTemplateDecl>(&declaration)));
        });
        if (templates.empty()) return std::nullopt;
        CppTemplateInstantiation result;
        const auto& sources = context.getSourceManager();
        const auto location = sources.getLocForEndOfFile(sources.getMainFileID());
        std::vector<clang::Expr*> arguments;
        for (const auto& argument : argumentTypes)
        {
            if (argument.kind != CppTemplateArgument::Kind::Type) return std::nullopt;
            clang::QualType type;
            if (argument.recordUsr)
            {
                const auto* tag = findTagByUsr(*context.getTranslationUnitDecl(), *argument.recordUsr);
                if (!tag) return std::nullopt;
                type = context.getCanonicalTagType(tag);
            }
            else type = builtinTemplateArgumentType(context, argument.builtin);
            if (type.isNull() || type->isVoidType())
            {
                result.diagnostics.push_back("a call argument has no C++ type");
                return result;
            }
            for (unsigned depth = 0; depth < argument.pointerDepth; ++depth) type = context.getPointerType(type);
            const auto valueKind = type->isRecordType() || argument.lvalue ? clang::VK_LValue : clang::VK_PRValue;
            arguments.push_back(new (context) clang::OpaqueValueExpr(location, type, valueKind));
        }
        const auto firstDiagnostic = diagnostics_.size();
        std::vector<clang::FunctionDecl*> found;
        std::vector<std::string> failures;
        for (auto* functionTemplate : templates)
        {
            clang::sema::TemplateDeductionInfo info(location);
            clang::FunctionDecl* specialization = nullptr;
            const auto outcome = sema.DeduceTemplateArguments(functionTemplate, nullptr, arguments, specialization,
                info, false, false, false, clang::QualType(), clang::Expr::Classification(), false,
                [](llvm::ArrayRef<clang::QualType>, bool) { return false; });
            if (outcome == clang::TemplateDeductionResult::Success && specialization && !specialization->isInvalidDecl())
            {
                found.push_back(specialization);
                continue;
            }
            failures.emplace_back(describeDeduction(outcome));
            if (outcome == clang::TemplateDeductionResult::ConstraintsNotSatisfied)
                sema.DiagnoseUnsatisfiedConstraint(info.AssociatedConstraintsSatisfaction);
        }
        const auto collectDiagnostics = [&] {
            bool error = false;
            for (auto index = firstDiagnostic; index < diagnostics_.size(); ++index)
            {
                const auto& diagnostic = diagnostics_[index];
                const bool isError = diagnostic.severity == CppSessionDiagnosticSeverity::Error ||
                    diagnostic.severity == CppSessionDiagnosticSeverity::Fatal;
                error = error || isError;
                if (!isError && diagnostic.severity != CppSessionDiagnosticSeverity::Note) continue;
                result.diagnostics.push_back(describeCppDiagnostic(diagnostic));
                for (const auto& note : diagnostic.notes) result.diagnostics.push_back(describeCppDiagnostic(note));
            }
            return error;
        };
        if (found.empty())
        {
            std::string message = "no function template '" + std::string(qualifiedName) +
                "' can deduce its template arguments from the call";
            for (std::size_t index = 0; index < failures.size(); ++index)
                message += (index ? "; " : ": ") + failures[index];
            result.diagnostics.push_back(std::move(message));
            collectDiagnostics();
            return result;
        }
        bool instantiated = false;
        for (auto* specialization : found)
            if (!specialization->isDefined() && specialization->isImplicitlyInstantiable())
            {
                sema.InstantiateFunctionDefinition(location, specialization, true, false, true);
                instantiated = true;
            }
        if (instantiated) sema.PerformPendingInstantiations();
        if (collectDiagnostics()) return result;
        result.instantiated = true;
        result.name = templates.front()->getNameAsString();
        result.clangUsr = declarationUsr(found.front());
        for (auto* specialization : found)
            functionSpecializationNames_.emplace(specialization->getCanonicalDecl(), result.name);
        return result;
    }

    std::optional<std::string> emitCppInlineIR(
        const std::vector<std::string>& symbols, std::vector<std::string>& emitted) const override
    {
        // EN: T10-012. A C++ inline function or inline variable is defined in every translation unit that
        //     uses it, so a library is not required to export it. For each requested symbol whose
        //     definition is inline in this header, Clang emits the definition; weak_odr keeps it through
        //     optimization while the COMDAT still merges it with a copy the library may also carry.
        // ES: T10-012. Una funcion o variable inline de C++ se define en cada unidad de traduccion que la
        //     usa, asi que la biblioteca no tiene por que exportarla. Para cada simbolo pedido cuya
        //     definicion es inline en este encabezado, Clang emite la definicion; weak_odr la conserva al
        //     optimizar y el COMDAT la sigue fusionando con una copia que la biblioteca tambien tenga.
        emitted.clear();
        std::lock_guard lock(mutex_);
        if (!ast_ || symbols.empty() || !ast_->getASTContext().getLangOpts().CPlusPlus) return std::nullopt;
        auto& context = ast_->getASTContext();
        const std::set<std::string> wanted(symbols.begin(), symbols.end());
        llvm::LLVMContext llvmContext;
        auto generator = clang::CreateLLVMCodeGen(ast_->getDiagnostics(), "nsc.cpp.inline",
            ast_->getVirtualFileSystemPtr(), ast_->getHeaderSearchOpts(), ast_->getPreprocessorOpts(),
            ast_->getCodeGenOpts(), llvmContext);
        if (!generator) return std::nullopt;
        generator->Initialize(context);
        std::vector<std::pair<clang::GlobalDecl, std::string>> selected;
        std::set<std::string> seen;
        // Microsoft names of plain identifiers start with "?name@" and C names are the identifier itself;
        // only declarations with one of those identifiers are mangled, unless an operator or special name is asked.
        std::set<std::string> identifiers;
        bool anyOther = false;
        for (const auto& symbol : symbols)
        {
            if (symbol.size() > 1 && symbol[0] == '?' && symbol[1] != '?')
                identifiers.insert(symbol.substr(1, symbol.find('@') - 1));
            else if (!symbol.empty() && symbol[0] != '?') identifiers.insert(symbol);
            else anyOther = true;
        }
        const auto consider = [&](clang::GlobalDecl declaration) {
            const auto* named = llvm::cast<clang::NamedDecl>(declaration.getDecl());
            if (const auto* identifier = named->getIdentifier())
                if (!anyOther && !identifiers.contains(identifier->getName().str())) return;
            const auto name = generator->GetMangledName(declaration).str();
            if (wanted.contains(name) && seen.insert(name).second) selected.emplace_back(declaration, name);
        };
        // EN: T15-014. The definitions with vague or internal linkage of this translation unit (inline functions,
        //     template instantiations, inline and internal variables) are offered to the code generator after the
        //     requested ones. Clang defers them and emits only those the emitted bodies reach, as when it
        //     compiles the whole unit; otherwise an inline function of the standard library that calls another
        //     (`std::current_exception`, `std::swap<int*>`) would leave it undefined.
        // ES: T15-014. Las definiciones con enlace vago o interno de esta unidad (funciones inline,
        //     instanciaciones de plantilla, variables inline e internas) se ofrecen al generador de codigo
        //     despues de las pedidas. Clang las difiere y emite solo las que alcanzan los cuerpos emitidos, como
        //     cuando compila la unidad entera; si no, una funcion inline de la biblioteca estandar que llama a otra
        //     (`std::current_exception`, `std::swap<int*>`) la dejaria sin definir.
        std::vector<clang::Decl*> closure;
        const auto offer = [&](const clang::Decl* declaration) {
            if (const auto* function = llvm::dyn_cast<clang::FunctionDecl>(declaration))
            {
                const auto* definition = function->getDefinition();
                if (definition != function || !definition->doesThisDeclarationHaveABody() ||
                    definition->isDependentContext() || definition->isDeleted() || definition->isInvalidDecl() ||
                    definition->hasAttr<clang::DLLImportAttr>() ||
                    definition->getTemplatedKind() == clang::FunctionDecl::TK_FunctionTemplate) return;
                if (definition->isInlined() || !definition->isExternallyVisible() ||
                    definition->isTemplateInstantiation())
                    closure.push_back(const_cast<clang::FunctionDecl*>(definition));
            }
            else if (const auto* variable = llvm::dyn_cast<clang::VarDecl>(declaration))
            {
                if (!variable->isThisDeclarationADefinition() || variable->isTemplated() ||
                    variable->getType()->isDependentType() || variable->isInvalidDecl() ||
                    variable->hasAttr<clang::DLLImportAttr>() || variable->isLocalVarDeclOrParm()) return;
                if (variable->isInline() || !variable->isExternallyVisible() ||
                    variable->getTemplateSpecializationKind() == clang::TSK_ImplicitInstantiation)
                    closure.push_back(const_cast<clang::VarDecl*>(variable));
            }
        };
        std::function<void(const clang::DeclContext&)> visit = [&](const clang::DeclContext& scope) {
            for (const auto* declaration : scope.decls())
            {
                offer(declaration);
                if (const auto* function = llvm::dyn_cast<clang::FunctionDecl>(declaration))
                {
                    // EN: T11-007. A constructor or destructor defined in the class, or defaulted on its first
                    //     declaration, is inline too: Clang emits its complete-object (MS: base) variant.
                    // ES: T11-007. Un constructor o destructor definido en la clase, o por defecto en su primera
                    //     declaracion, tambien es inline: Clang emite su variante de objeto completo (MS: base).
                    const auto* definition = function->getDefinition();
                    const auto* ctor = llvm::dyn_cast_or_null<clang::CXXConstructorDecl>(definition);
                    const auto* dtor = llvm::dyn_cast_or_null<clang::CXXDestructorDecl>(definition);
                    if (definition == function && definition->isInlined() &&
                        (definition->hasBody() || ((ctor || dtor) && definition->isExplicitlyDefaulted())) &&
                        !definition->isDependentContext() && !definition->isDeleted() &&
                        !definition->hasAttr<clang::DLLImportAttr>() &&
                        definition->getTemplatedKind() == clang::FunctionDecl::TK_NonTemplate &&
                        definition->isExternallyVisible())
                    {
                        if (ctor) consider(clang::GlobalDecl(ctor, clang::Ctor_Complete));
                        else if (dtor)
                            consider(clang::GlobalDecl(dtor, context.getTargetInfo().getCXXABI().isMicrosoft()
                                ? clang::Dtor_Base : clang::Dtor_Complete));
                        else consider(clang::GlobalDecl(definition));
                    }
                    // EN: T13-011. A member of a class template specialization, or a function template
                    //     specialization, has vague linkage like an inline function; Sema instantiates its
                    //     definition below. An explicit instantiation declaration leaves it to the library.
                    // ES: T13-011. Un miembro de una especializacion de plantilla de clase, o una
                    //     especializacion de plantilla de funcion, tiene enlace vago como una funcion inline;
                    //     Sema instancia su definicion abajo. Una declaracion de instanciacion explicita la
                    //     deja a la biblioteca.
                    else if ((function->getTemplatedKind() == clang::FunctionDecl::TK_MemberSpecialization ||
                              function->getTemplatedKind() == clang::FunctionDecl::TK_FunctionTemplateSpecialization) &&
                             !function->isDependentContext() && !function->isDeleted() &&
                             !function->hasAttr<clang::DLLImportAttr>() && function->isExternallyVisible() &&
                             function->getTemplateSpecializationKind() != clang::TSK_ExplicitInstantiationDeclaration &&
                             (function->getDefinition() || function->isImplicitlyInstantiable() ||
                              function->isDefaulted()))
                    {
                        const auto* member = function->getDefinition() ? function->getDefinition() : function;
                        if (const auto* memberCtor = llvm::dyn_cast<clang::CXXConstructorDecl>(member))
                            consider(clang::GlobalDecl(memberCtor, clang::Ctor_Complete));
                        else if (const auto* memberDtor = llvm::dyn_cast<clang::CXXDestructorDecl>(member))
                            consider(clang::GlobalDecl(memberDtor, context.getTargetInfo().getCXXABI().isMicrosoft()
                                ? clang::Dtor_Base : clang::Dtor_Complete));
                        else consider(clang::GlobalDecl(member));
                    }
                }
                else if (const auto* variable = llvm::dyn_cast<clang::VarDecl>(declaration))
                {
                    if (variable->isInline() && variable->isThisDeclarationADefinition() &&
                        !variable->getType()->isDependentType() && variable->isExternallyVisible() &&
                        !variable->hasAttr<clang::DLLImportAttr>() && variable->hasInit())
                        consider(clang::GlobalDecl(variable));
                    // EN: T13-021. A static data member of a class template specialization has vague linkage; Sema
                    //     instantiates its definition below.
                    // ES: T13-021. Un miembro de datos static de una especializacion tiene enlace vago; Sema
                    //     instancia su definicion abajo.
                    else if (variable->isStaticDataMember() &&
                             variable->getTemplateSpecializationKind() == clang::TSK_ImplicitInstantiation &&
                             !variable->getType()->isDependentType() && variable->isExternallyVisible() &&
                             !variable->hasAttr<clang::DLLImportAttr>())
                        consider(clang::GlobalDecl(variable));
                }
                if (const auto* nested = llvm::dyn_cast<clang::DeclContext>(declaration))
                {
                    if (llvm::isa<clang::NamespaceDecl>(declaration) || llvm::isa<clang::LinkageSpecDecl>(declaration) ||
                        llvm::isa<clang::ExportDecl>(declaration))
                        visit(*nested);
                    else if (const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(declaration))
                        if (record->isThisDeclarationADefinition() && !record->isDependentContext())
                            visit(*nested);
                }
                // T13-011: the instantiated specializations of a class template.
                // T13-011: las especializaciones instanciadas de una plantilla de clase.
                if (const auto* classTemplate = llvm::dyn_cast<clang::ClassTemplateDecl>(declaration))
                    for (const auto* specialization : classTemplate->specializations())
                        if (specialization->isCompleteDefinition() && !specialization->isInvalidDecl())
                            visit(*specialization);
                // T13-005: the specializations of a function template. / las de una plantilla de funcion.
                if (const auto* functionTemplate = llvm::dyn_cast<clang::FunctionTemplateDecl>(declaration))
                    for (const auto* specialization : functionTemplate->specializations())
                    {
                        offer(specialization);
                        if (!specialization->isInvalidDecl() && !specialization->isDependentContext() &&
                            specialization->isExternallyVisible() &&
                            specialization->getTemplateSpecializationKind() != clang::TSK_ExplicitInstantiationDeclaration)
                            consider(clang::GlobalDecl(specialization));
                    }
            }
        };
        visit(*context.getTranslationUnitDecl());
        if (selected.empty()) return std::nullopt;
        // EN: T13-011. Sema instantiates the definition of each selected template member, and the definitions
        //     those bodies use, before the code generator sees them.
        // ES: T13-011. Sema instancia la definicion de cada miembro de plantilla elegido, y las que usan sus
        //     cuerpos, antes de que las vea el generador de codigo.
        if (ast_->hasSema())
        {
            auto& sema = ast_->getSema();
            bool instantiated = false;
            for (const auto& [declaration, name] : selected)
                if (auto* function = llvm::dyn_cast<clang::FunctionDecl>(const_cast<clang::Decl*>(declaration.getDecl()));
                    function && !function->getDefinition() && function->isImplicitlyInstantiable())
                {
                    sema.InstantiateFunctionDefinition(function->getLocation(), function, true, false, true);
                    instantiated = true;
                }
                else if (auto* variable = llvm::dyn_cast<clang::VarDecl>(const_cast<clang::Decl*>(declaration.getDecl()));
                         variable && variable->isStaticDataMember() &&
                         variable->getTemplateSpecializationKind() == clang::TSK_ImplicitInstantiation)
                {
                    sema.InstantiateVariableDefinition(variable->getLocation(), variable, true, false, true);
                    instantiated = true;
                }
            if (instantiated) sema.PerformPendingInstantiations();
        }
        for (auto& [declaration, name] : selected)
        {
            auto* decl = const_cast<clang::Decl*>(declaration.getDecl());
            // A special member defaulted on its first declaration has no body until Sema defines it.
            if (auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(decl);
                method && method->isExplicitlyDefaulted() && !method->doesThisDeclarationHaveABody() &&
                ast_->hasSema())
            {
                auto& sema = ast_->getSema();
                if (auto* dtor = llvm::dyn_cast<clang::CXXDestructorDecl>(method))
                    sema.DefineImplicitDestructor(dtor->getLocation(), dtor);
                else if (auto* ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(method))
                {
                    if (ctor->isDefaultConstructor()) sema.DefineImplicitDefaultConstructor(ctor->getLocation(), ctor);
                    else if (ctor->isCopyConstructor()) sema.DefineImplicitCopyConstructor(ctor->getLocation(), ctor);
                    else if (ctor->isMoveConstructor()) sema.DefineImplicitMoveConstructor(ctor->getLocation(), ctor);
                }
                if (method->isInvalidDecl() || !method->doesThisDeclarationHaveABody()) continue;
            }
            if (!generator->HandleTopLevelDecl(clang::DeclGroupRef(decl))) return std::nullopt;
            if (!generator->GetAddrOfGlobal(declaration, false)) return std::nullopt;
        }
        // T15-014: the deferred definitions the emitted bodies may reach, collected after Sema instantiated the
        //     requested members and what their bodies use. / Las definiciones diferidas que pueden alcanzar los
        //     cuerpos emitidos, reunidas despues de que Sema instancio los miembros pedidos y lo que usan.
        {
            closure.clear();
            std::function<void(const clang::DeclContext&)> walk = [&](const clang::DeclContext& scope) {
                for (const auto* declaration : scope.decls())
                {
                    offer(declaration);
                    if (const auto* functionTemplate = llvm::dyn_cast<clang::FunctionTemplateDecl>(declaration))
                        for (const auto* specialization : functionTemplate->specializations()) offer(specialization);
                    if (const auto* classTemplate = llvm::dyn_cast<clang::ClassTemplateDecl>(declaration))
                        for (const auto* specialization : classTemplate->specializations())
                            if (specialization->isCompleteDefinition() && !specialization->isInvalidDecl())
                                walk(*specialization);
                    if (llvm::isa<clang::NamespaceDecl>(declaration) || llvm::isa<clang::LinkageSpecDecl>(declaration) ||
                        llvm::isa<clang::ExportDecl>(declaration))
                        walk(*llvm::cast<clang::DeclContext>(declaration));
                    else if (const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(declaration))
                        if (record->isThisDeclarationADefinition() && !record->isDependentContext()) walk(*record);
                }
            };
            walk(*context.getTranslationUnitDecl());
            std::set<const clang::Decl*> streamed;
            for (const auto& [declaration, name] : selected) streamed.insert(declaration.getDecl());
            for (auto* decl : closure)
                if (streamed.insert(decl).second && !generator->HandleTopLevelDecl(clang::DeclGroupRef(decl)))
                    return std::nullopt;
        }
        generator->HandleTranslationUnit(context);
        auto module = generator->ReleaseModule();
        if (!module) return std::nullopt;
        for (const auto& [declaration, name] : selected)
        {
            auto* value = module->getNamedValue(name);
            auto* global = value ? llvm::dyn_cast<llvm::GlobalObject>(value) : nullptr;
            if (!global || global->isDeclaration() || global->hasAvailableExternallyLinkage()) continue;
            if (global->hasLinkOnceODRLinkage()) global->setLinkage(llvm::GlobalValue::WeakODRLinkage);
            emitted.push_back(name);
        }
        if (emitted.empty() || llvm::verifyModule(*module, &llvm::errs())) { emitted.clear(); return std::nullopt; }
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
        const auto& macros = macroConstantsLocked();
        result.insert(result.end(), macros.constants.begin(), macros.constants.end());
        return result;
    }
    std::vector<CppMacroName> macrosSnapshot() const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_) return {};
        return macroConstantsLocked().names;
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
        source_ = source;
        inputPath_ = utf8(inputPath);

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
        if (ast_->getASTContext().getLangOpts().CPlusPlus)
            instantiateNamedSpecializations(ast_->getASTContext(), ast_->getSema());
        state_ = CppSessionState::Ready;
        return {};
    }

private:
    // T13-006: the names of a namespace-scope template. / Los nombres de una plantilla de un namespace.
    static std::vector<std::string> templateNames(const clang::TemplateDecl& declaration)
    {
        std::vector<std::string> names{declaration.getQualifiedNameAsString()};
        if (const auto path = fullNamespacePath(declaration.getDeclContext()))
        {
            auto spelled = path->empty() ? declaration.getNameAsString() : *path + "::" + declaration.getNameAsString();
            if (spelled != names.front()) names.push_back(std::move(spelled));
        }
        return names;
    }
    // EN: T14-001, T14-002. The object-like macros that the header and the headers it includes define (not the
    //     system headers, the compiler or the command line) are evaluated by Clang: the source is analyzed again
    //     with one constant per macro initialized with its expansion, and a macro whose expansion is a scalar
    //     constant (integer, enum, bool, floating) becomes a constant with the type Clang deduces. Function-like
    //     macros and other expansions are named so NativeSharp can say why they are not imported.
    // ES: T14-001, T14-002. Las macros de objeto que definen el encabezado y los que incluye (no los del sistema,
    //     el compilador ni la linea de ordenes) las evalua Clang: la fuente se analiza otra vez con una constante
    //     por macro inicializada con su expansion, y una macro cuya expansion es una constante escalar (entera,
    //     enum, bool, flotante) pasa a ser una constante con el tipo que deduce Clang. Las macros de funcion y
    //     las demas expansiones se nombran para que NativeSharp diga por que no se importan.
    struct MacroConstants
    {
        std::vector<CppGlobalDeclaration> constants;
        std::vector<CppMacroName> names;
    };
    const MacroConstants& macroConstantsLocked() const
    {
        if (macroConstants_) return *macroConstants_;
        macroConstants_.emplace();
        auto& preprocessor = ast_->getPreprocessor();
        const auto& sources = ast_->getSourceManager();
        std::vector<std::string> objectLike;
        for (const auto& [identifier, state] : preprocessor.macros())
        {
            const auto* info = identifier ? preprocessor.getMacroInfo(identifier) : nullptr;
            if (!info || info->isBuiltinMacro() || info->getNumTokens() == 0) continue;
            const auto location = info->getDefinitionLoc();
            if (location.isInvalid() || sources.isInSystemHeader(location) || sources.isWrittenInBuiltinFile(location) ||
                sources.isWrittenInCommandLineFile(location) || sources.isWrittenInScratchSpace(location)) continue;
            const std::string name = identifier->getName().str();
            if (name.starts_with("__nsc_")) continue;
            if (info->isFunctionLike()) macroConstants_->names.push_back({name, true, false});
            else objectLike.push_back(name);
        }
        std::sort(objectLike.begin(), objectLike.end());
        if (objectLike.empty()) return *macroConstants_;
        const bool cpp = ast_->getASTContext().getLangOpts().CPlusPlus;
        std::string code = source_ + "\n";
        for (const auto& name : objectLike)
            code += (cpp ? "constexpr auto __nsc_macro_" : "static const __auto_type __nsc_macro_") + name + " = (" +
                name + ");\n";
        llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> filesystem(llvm::vfs::createPhysicalFileSystem().release());
        std::vector<CppSessionDiagnostic> macroDiagnostics;
        detail::DiagnosticCollector collector(macroDiagnostics);
        auto arguments = detail::buildClangInvocationArguments(options_);
        arguments.push_back("-ferror-limit=0");
        std::unique_ptr<clang::ASTUnit> unit;
        if (!filesystem->setCurrentWorkingDirectory(utf8(options_.workingDirectory)))
            unit = detail::runClangSessionAction(code, arguments, inputPath_, utf8(options_.driverReferencePath),
                                                 &collector, std::move(filesystem), CppCancellationToken{});
        std::set<std::string> constants;
        if (unit)
        {
            auto& context = unit->getASTContext();
            for (const auto* declaration : context.getTranslationUnitDecl()->decls())
            {
                const auto* variable = llvm::dyn_cast<clang::VarDecl>(declaration);
                if (!variable || !variable->getIdentifier() || !variable->getName().starts_with("__nsc_macro_")) continue;
                const auto constant = evaluateScalarConstant(*variable, context);
                if (!constant) continue;
                CppGlobalDeclaration value;
                value.name = variable->getName().drop_front(12).str();
                value.qualifiedName = value.name;
                value.type = describeType(variable->getType(), context);
                value.constantValue = constant;
                value.macro = true;
                constants.insert(value.name);
                macroConstants_->constants.push_back(std::move(value));
            }
        }
        for (const auto& name : objectLike) macroConstants_->names.push_back({name, false, constants.contains(name)});
        return *macroConstants_;
    }

    // EN: T13-007. The C++ struct that mirrors a NativeSharp struct: declared once per session, in
    //     `__nsc::<module>::<namespace>`, with the fields of the struct in order; Clang lays it out.
    // ES: T13-007. El struct C++ espejo de un struct NativeSharp: se declara una vez por sesion, en
    //     `__nsc::<modulo>::<namespace>`, con los campos del struct en orden; Clang calcula su layout.
    clang::QualType nativeStructMirror(const CppNativeStruct& structure, unsigned depth = 0) const
    {
        auto& context = ast_->getASTContext();
        if (depth > 16 || structure.name.empty() || structure.moduleName.empty()) return {};
        std::string key(cppNativeStructNamespace);
        key += "::" + structure.moduleName;
        for (const auto& segment : structure.namespaceSegments) key += "::" + segment;
        key += "::" + structure.name;
        if (const auto found = nativeStructMirrors_.find(key); found != nativeStructMirrors_.end())
            return context.getCanonicalTagType(found->second);
        const auto identifier = [&](std::string_view text) -> clang::IdentifierInfo* {
            if (text.empty() || !(std::isalpha(static_cast<unsigned char>(text.front())) || text.front() == '_')) return nullptr;
            for (const char c : text)
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return nullptr;
            return &context.Idents.get(text);
        };
        std::vector<std::pair<clang::QualType, clang::IdentifierInfo*>> fields;
        for (const auto& field : structure.fields)
        {
            clang::QualType type = field.structure ? nativeStructMirror(*field.structure, depth + 1)
                                                   : builtinTemplateArgumentType(context, field.builtin);
            auto* name = identifier(field.name);
            if (type.isNull() || !name || (type->isVoidType() && field.pointerDepth == 0)) return {};
            for (unsigned level = 0; level < field.pointerDepth; ++level) type = context.getPointerType(type);
            fields.emplace_back(type, name);
        }
        auto* recordName = identifier(structure.name);
        if (!recordName || fields.empty()) return {};
        const auto& sources = context.getSourceManager();
        const auto location = sources.getLocForEndOfFile(sources.getMainFileID());
        clang::DeclContext* scope = context.getTranslationUnitDecl();
        std::vector<std::string> path{std::string(cppNativeStructNamespace), structure.moduleName};
        path.insert(path.end(), structure.namespaceSegments.begin(), structure.namespaceSegments.end());
        for (const auto& segment : path)
        {
            auto* name = identifier(segment);
            if (!name) return {};
            clang::NamespaceDecl* existing = nullptr;
            for (auto* declaration : scope->decls())
                if (auto* ns = llvm::dyn_cast<clang::NamespaceDecl>(declaration); ns && ns->getIdentifier() == name)
                { existing = ns; break; }
            if (!existing)
            {
                existing = clang::NamespaceDecl::Create(context, scope, false, location, location, name, nullptr, false);
                scope->addDecl(existing);
            }
            scope = existing;
        }
        auto* record = clang::CXXRecordDecl::Create(context, clang::TagTypeKind::Struct, scope, location, location,
                                                    recordName);
        record->startDefinition();
        for (const auto& [type, name] : fields)
        {
            auto* field = clang::FieldDecl::Create(context, record, location, location, name, type,
                context.getTrivialTypeSourceInfo(type, location), nullptr, false, clang::ICIS_NoInit);
            field->setAccess(clang::AS_public);
            record->addDecl(field);
        }
        record->completeDefinition();
        scope->addDecl(record);
        nativeStructMirrors_.emplace(key, record);
        const auto& layout = context.getASTRecordLayout(record);
        CppNativeStructLayout mirrored{structure.moduleName, structure.namespaceSegments, structure.name,
            static_cast<std::uint64_t>(layout.getSize().getQuantity()),
            static_cast<std::uint64_t>(layout.getAlignment().getQuantity()), {}};
        for (unsigned field = 0; field < layout.getFieldCount(); ++field)
            mirrored.fieldOffsets.push_back(layout.getFieldOffset(field) / context.getCharWidth());
        nativeStructLayouts_.push_back(std::move(mirrored));
        return context.getCanonicalTagType(record);
    }

    template <class Visitor>
    void visitNamespaceTemplates(Visitor&& visitor) const
    {
        std::function<void(const clang::DeclContext&)> visit = [&](const clang::DeclContext& scope) {
            for (const auto* declaration : scope.decls())
            {
                if (const auto* nested = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
                { if (!nested->isAnonymousNamespace()) visit(*nested); continue; }
                if (llvm::isa<clang::LinkageSpecDecl>(declaration) || llvm::isa<clang::ExportDecl>(declaration))
                { visit(*llvm::cast<clang::DeclContext>(declaration)); continue; }
                if (const auto* classTemplate = llvm::dyn_cast<clang::ClassTemplateDecl>(declaration);
                    classTemplate && !classTemplate->isInvalidDecl() && classTemplate->getTemplatedDecl())
                    visitor(*classTemplate, false);
                else if (const auto* functionTemplate = llvm::dyn_cast<clang::FunctionTemplateDecl>(declaration);
                         functionTemplate && !functionTemplate->isInvalidDecl() &&
                         !llvm::isa<clang::CXXMethodDecl>(functionTemplate->getTemplatedDecl()))
                    visitor(*functionTemplate, true);
            }
        };
        visit(*ast_->getASTContext().getTranslationUnitDecl());
    }

    mutable std::mutex mutex_;
    CppSessionOptions options_;
    std::vector<CppSessionDiagnostic> diagnostics_;
    detail::DiagnosticCollector consumer_;
    // Destroy before consumer_: Clang's diagnostic engine borrows the consumer.
    std::unique_ptr<clang::ASTUnit> ast_;
    // T12-004: the analyzed source, reparsed with the member adapters appended.
    std::string source_;
    std::string inputPath_;
    std::optional<basic::Sha256Digest> preprocessingFingerprint_;
    // T13-005: the function template specializations NativeSharp requested. / Las que pidio NativeSharp.
    mutable CppFunctionSpecializationNames functionSpecializationNames_;
    // T13-007: the mirrors of NativeSharp structs, by qualified C++ name. / Los espejos, por nombre C++.
    mutable std::map<std::string, clang::CXXRecordDecl*> nativeStructMirrors_;
    mutable std::vector<CppNativeStructLayout> nativeStructLayouts_;
    // T14-002: the macros evaluated once per session. / Las macros, evaluadas una vez por sesion.
    mutable std::optional<MacroConstants> macroConstants_;
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
