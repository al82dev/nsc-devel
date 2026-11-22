#include "nativesharp/Interop/CppClangSessionFactory.h"
#include "nativesharp/Interop/CppRegCallAbiValidation.h"
#include "nativesharp/Interop/CppMemberThunk.h"
#include "../Internal/ClangAbiPlan.h"
#include "ClangSessionSupport.h"
#include "nativesharp/Basic/Sha256.h"

#include <clang/AST/DeclFriend.h>
#include <llvm/IR/Constants.h>
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
#include <tuple>
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

// EN: T19-036. The NativeSharp name of a record that encloses another: a class template specialization at
//     namespace scope is named as when it is imported itself (T13-004), so the records nested in it keep an exact
//     identity; another specialization has none.
// ES: T19-036. El nombre NativeSharp de un record que encierra a otro: una especializacion de plantilla de clase en
//     un namespace se nombra como cuando se importa ella misma (T13-004), asi que los records anidados en ella
//     conservan una identidad exacta; otra especializacion no tiene ninguno.
std::optional<std::string> cppEnclosingRecordName(const clang::RecordDecl& enclosing)
{
    if (const auto* specialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(&enclosing))
        return cppSpecializationName(*specialization);
    return enclosing.getNameAsString();
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
        // EN: T19-036. A member class of a specialization is instantiated lazily too, as a C++ unit that uses it.
        // ES: T19-036. Una clase miembro de una especializacion tambien se instancia al usarla, como en C++.
        if ((llvm::isa<clang::ClassTemplateSpecializationDecl>(record) || record->getInstantiatedFromMemberClass()) &&
            !record->hasDefinition())
            if (!sema.isCompleteType(location, type)) continue;
        record = record->getDefinition();
        if (!record || record->isDependentContext() || record->isInvalidDecl()) continue;
        // EN: T19-036. The public member classes of a specialization can be named from NativeSharp
        //     (`Box<int>.Item`); they are instantiated with it.
        // ES: T19-036. Las clases miembro publicas de una especializacion se pueden nombrar desde NativeSharp
        //     (`Box<int>.Item`); se instancian con ella.
        if (llvm::isa<clang::ClassTemplateSpecializationDecl>(record) || record->getInstantiatedFromMemberClass())
            for (const auto* member : record->decls())
                if (const auto* nested = llvm::dyn_cast<clang::CXXRecordDecl>(member);
                    nested && !nested->isImplicit() && nested->getAccess() == clang::AS_public &&
                    nested->getInstantiatedFromMemberClass() && !nested->isDependentContext())
                    add(context.getCanonicalTagType(nested), location);
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

// EN: T19-006. The first parameter with a default argument when some default argument is not a constant that
//     NativeSharp writes (T10-010): an enumerator, a record, a global, a call. The function is then called through
//     one Clang adapter per number of written arguments (cppDefaultArgumentThunkSymbol), so Clang evaluates the
//     omitted ones at each call, as in C++.
// ES: T19-006. El primer parametro con argumento predeterminado cuando alguno no es una constante que escribe
//     NativeSharp (T10-010): un enumerador, un record, una global, una llamada. La funcion se llama entonces por un
//     adaptador de Clang por cada numero de argumentos escritos, para que Clang evalue los omitidos en cada
//     llamada, como en C++.
std::optional<std::size_t> defaultArgumentThunkStart(const std::vector<CppFunctionParameter>& parameters,
                                                     bool anyDefault = false)
{
    std::optional<std::size_t> first;
    bool unevaluated = false;
    for (std::size_t index = 0; index < parameters.size(); ++index)
    {
        if (!parameters[index].hasDefaultArgument) continue;
        if (!first) first = index;
        unevaluated = unevaluated || parameters[index].defaultArgumentKind == CppDefaultArgumentKind::Unevaluated;
    }
    return unevaluated || anyDefault ? first : std::nullopt;
}

// T19-006: the canonical type of a function with only its first parameters. / Con solo sus primeros parametros.
std::string truncatedCanonicalFunctionType(const clang::FunctionDecl& function, std::size_t arity,
                                           clang::ASTContext& context)
{
    std::vector<clang::QualType> types;
    for (std::size_t index = 0; index < arity && index < function.getNumParams(); ++index)
        types.push_back(function.getParamDecl(static_cast<unsigned>(index))->getType());
    clang::FunctionProtoType::ExtProtoInfo info;
    if (const auto* prototype = function.getType()->getAs<clang::FunctionProtoType>())
        info = prototype->getExtProtoInfo();
    return context.getFunctionType(function.getReturnType(), types, info).getCanonicalType().getAsString();
}

// T19-006: a parameter that has no default argument any more. / Un parametro que ya no tiene predeterminado.
void clearDefaultArgument(CppFunctionParameter& parameter)
{
    parameter.hasDefaultArgument = false;
    parameter.defaultArgumentKind = CppDefaultArgumentKind::None;
    parameter.defaultArgumentBits = 0;
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

// EN: T19-001. A pointer to a C variable length array (`double (*)[m]`, the adjusted type of a parameter
//     `double a[n][m]`) as a pointer to the element left after the last variable dimension: the dimensions up to
//     that one are flattened in row-major order, as a NativeSharp VLA `T[n, m]` lays them out, and the constant
//     ones inside it stay (`double (*)[m][3]` becomes `double (*)[3]`). The address is the same; Clang checks
//     no extent at a call either. Qualifiers of the elements and of each pointer stay. A type with no such
//     pointer comes back unchanged.
// ES: T19-001. Un puntero a un arreglo C de longitud variable (`double (*)[m]`, el tipo ajustado de un parametro
//     `double a[n][m]`) como puntero al elemento que queda despues de la ultima dimension variable: las dimensiones
//     hasta esa se aplanan en orden por filas, como dispone un VLA NativeSharp `T[n, m]`, y las constantes de
//     dentro se quedan (`double (*)[m][3]` pasa a `double (*)[3]`). La direccion es la misma; Clang tampoco
//     comprueba extensiones en una llamada. Los calificadores de los elementos y de cada puntero se conservan.
//     Un tipo sin ese puntero vuelve sin cambios.
clang::QualType flattenVariableArrayPointers(clang::QualType type, clang::ASTContext& context, unsigned depth = 0)
{
    if (type.isNull() || depth > 64 || !type->isVariablyModifiedType()) return type;
    const clang::QualType canonical = type.getCanonicalType();
    const auto* pointer = canonical->getAs<clang::PointerType>();
    if (!pointer) return type;
    clang::QualType pointee = pointer->getPointeeType();
    if (pointee->isVariableArrayType() || (pointee->isArrayType() && pointee->isVariablyModifiedType()))
    {
        clang::QualType current = pointee;
        clang::QualType afterVariable = pointee;
        while (const auto* array = context.getAsArrayType(current))
        {
            current = array->getElementType();
            if (llvm::isa<clang::VariableArrayType>(array)) afterVariable = current;
        }
        pointee = afterVariable;
    }
    pointee = flattenVariableArrayPointers(pointee, context, depth + 1);
    return context.getQualifiedType(context.getPointerType(pointee), canonical.getLocalQualifiers());
}

// EN: T19-040. How a function-like macro uses each parameter: as a value (an ordinary occurrence, also the GNU
//     `, ##__VA_ARGS__`), turned into a string (`#x`) or pasted to another token (`a##b`).
// ES: T19-040. Como usa una macro de funcion cada parametro: como valor (una aparicion ordinaria, tambien el
//     `, ##__VA_ARGS__` de GNU), convertido en cadena (`#x`) o pegado a otro token (`a##b`).
struct MacroParameterUses
{
    std::vector<bool> value;
    std::vector<bool> stringified;
    std::vector<bool> pasted;
    // EN: Item 5.3. How many times each parameter appears as a value, and whether the body has `?:`, `&&` or `||`,
    //     which may evaluate an occurrence conditionally.
    // ES: Item 5.3. Cuantas veces aparece cada parametro como valor, y si el cuerpo tiene `?:`, `&&` o `||`, que
    //     pueden evaluar una aparicion de forma condicional.
    std::vector<unsigned> valueCount;
    bool conditional = false;
    // EN: Item 5.10. Whether the macro writes the parameter (`x = ...`, `x += ...`, `++x`, `x--`) or takes its
    //     address (`&x`): it needs the caller's place, not a copy.
    // ES: Item 5.10. Si la macro escribe el parametro (`x = ...`, `x += ...`, `++x`, `x--`) o toma su direccion
    //     (`&x`): necesita el lugar del llamador, no una copia.
    std::vector<bool> written;
};

MacroParameterUses macroParameterUses(const clang::MacroInfo& info)
{
    MacroParameterUses uses;
    const unsigned count = info.getNumParams();
    uses.value.assign(count, false);
    uses.stringified.assign(count, false);
    uses.pasted.assign(count, false);
    uses.valueCount.assign(count, 0);
    uses.written.assign(count, false);
    const auto tokens = info.tokens();
    for (std::size_t index = 0; index < tokens.size(); ++index)
    {
        if (tokens[index].isOneOf(clang::tok::question, clang::tok::ampamp, clang::tok::pipepipe))
            uses.conditional = true;
        const auto* identifier = tokens[index].getIdentifierInfo();
        const int parameter = identifier ? info.getParameterNum(identifier) : -1;
        if (parameter < 0) continue;
        const bool hashBefore = index > 0 && tokens[index - 1].is(clang::tok::hash);
        const bool pasteBefore = index > 0 && tokens[index - 1].is(clang::tok::hashhash);
        const bool pasteAfter = index + 1 < tokens.size() && tokens[index + 1].is(clang::tok::hashhash);
        const bool elision = pasteBefore && info.isVariadic() && static_cast<unsigned>(parameter) + 1 == count &&
            index > 1 && tokens[index - 2].is(clang::tok::comma);
        if (hashBefore) uses.stringified[parameter] = true;
        else if ((pasteBefore && !elision) || pasteAfter) uses.pasted[parameter] = true;
        else
        {
            uses.value[parameter] = true;
            ++uses.valueCount[parameter];
            // The parameter may be wrapped in parentheses: `(x) = 0`. / El parametro puede ir entre parentesis.
            std::size_t after = index + 1;
            std::size_t before = index;
            while (after < tokens.size() && before > 0 && tokens[after].is(clang::tok::r_paren) &&
                   tokens[before - 1].is(clang::tok::l_paren)) { ++after; --before; }
            const bool assignedAfter = after < tokens.size() &&
                tokens[after].isOneOf(clang::tok::equal, clang::tok::plusequal, clang::tok::minusequal,
                    clang::tok::starequal, clang::tok::slashequal, clang::tok::percentequal, clang::tok::ampequal,
                    clang::tok::pipeequal, clang::tok::caretequal, clang::tok::lesslessequal,
                    clang::tok::greatergreaterequal, clang::tok::plusplus, clang::tok::minusminus);
            const bool modifiedBefore = before > 0 &&
                tokens[before - 1].isOneOf(clang::tok::plusplus, clang::tok::minusminus, clang::tok::amp) &&
                (before < 2 || !tokens[before - 2].isOneOf(clang::tok::identifier, clang::tok::r_paren,
                                                           clang::tok::numeric_constant));
            // EN: `*(p) = 5` writes through the pointer, not the parameter: a unary `*` before it is a dereference.
            // ES: `*(p) = 5` escribe a traves del puntero, no en el parametro: un `*` unario antes es una
            //     desreferencia.
            const bool dereferenced = before > 0 && tokens[before - 1].is(clang::tok::star) &&
                (before < 2 || !tokens[before - 2].isOneOf(clang::tok::identifier, clang::tok::r_paren,
                                                           clang::tok::numeric_constant));
            if ((assignedAfter && !dereferenced) || modifiedBefore) uses.written[parameter] = true;
        }
    }
    return uses;
}

// EN: T19-040. The named parameters NativeSharp passes as tokens: those the macro never uses as a value.
// ES: T19-040. Los parametros con nombre que NativeSharp pasa como tokens: los que la macro nunca usa como valor.
std::vector<unsigned> macroTokenParameters(const clang::MacroInfo& info)
{
    const auto uses = macroParameterUses(info);
    std::vector<unsigned> result;
    const unsigned named = info.isVariadic() ? info.getNumParams() - 1 : info.getNumParams();
    for (unsigned parameter = 0; parameter < named; ++parameter)
        if (!uses.value[parameter]) result.push_back(parameter);
    // C2-06: variable arguments used only with `#` or `##`. / Argumentos variables usados solo con `#` o `##`.
    if (info.isVariadic() && !uses.value.back() && (uses.stringified.back() || uses.pasted.back()))
        result.push_back(kCppVariadicTokenParameters + named);
    return result;
}

// EN: T19-040. A C string literal whose content is `text`, as `#` writes the spelling of an argument.
// ES: T19-040. Un literal de cadena C cuyo contenido es `text`, como `#` escribe la ortografia de un argumento.
std::string cStringLiteral(std::string_view text)
{
    std::string result = "\"";
    for (const char c : text)
    {
        if (c == '\\' || c == '"') result.push_back('\\');
        result.push_back(c);
    }
    return result + "\"";
}

// EN: T19-001. The name of the synthetic C struct that carries `_Complex T`: `__complex_` and the spelling of T
//     with `_` for each space (`__complex_double`, `__complex_unsigned_long_long`). Empty for an element that is
//     not a standard integer or real floating type.
// ES: T19-001. El nombre del struct C sintetico que lleva `_Complex T`: `__complex_` y la ortografia de T con `_`
//     por cada espacio (`__complex_double`, `__complex_unsigned_long_long`). Vacio para un elemento que no es un
//     tipo entero o flotante real estandar.
std::string complexCarrierName(clang::QualType element)
{
    const auto* builtin = element.getCanonicalType()->getAs<clang::BuiltinType>();
    if (!builtin) return {};
    switch (builtin->getKind())
    {
    case clang::BuiltinType::Char_S:
    case clang::BuiltinType::Char_U: return "__complex_char";
    case clang::BuiltinType::SChar: return "__complex_signed_char";
    case clang::BuiltinType::UChar: return "__complex_unsigned_char";
    case clang::BuiltinType::Short: return "__complex_short";
    case clang::BuiltinType::UShort: return "__complex_unsigned_short";
    case clang::BuiltinType::Int: return "__complex_int";
    case clang::BuiltinType::UInt: return "__complex_unsigned_int";
    case clang::BuiltinType::Long: return "__complex_long";
    case clang::BuiltinType::ULong: return "__complex_unsigned_long";
    case clang::BuiltinType::LongLong: return "__complex_long_long";
    case clang::BuiltinType::ULongLong: return "__complex_unsigned_long_long";
    case clang::BuiltinType::Float: return "__complex_float";
    case clang::BuiltinType::Double: return "__complex_double";
    case clang::BuiltinType::LongDouble: return "__complex_long_double";
    default: return {};
    }
}

// EN: T19-001. The synthetic C struct that carries `_Complex T` in this C translation unit, or null: the struct
//     with the name of complexCarrierName and exactly the fields `real` and `imag` of type T, the one that
//     synthesizeComplexCarriers declares.
// ES: T19-001. El struct C sintetico que lleva `_Complex T` en esta unidad C, o nulo: el struct con el nombre de
//     complexCarrierName y exactamente los campos `real` e `imag` de tipo T, el que declara
//     synthesizeComplexCarriers.
const clang::RecordDecl* complexCarrierRecord(clang::ASTContext& context, clang::QualType element)
{
    const std::string name = complexCarrierName(element);
    if (name.empty()) return nullptr;
    const clang::QualType expected = element.getCanonicalType().getUnqualifiedType();
    const auto found = context.getTranslationUnitDecl()->lookup(&context.Idents.get(name));
    for (const auto* declaration : found)
    {
        const auto* record = llvm::dyn_cast<clang::RecordDecl>(declaration);
        if (!record || !record->isStruct() || !record->isCompleteDefinition()) continue;
        std::vector<const clang::FieldDecl*> fields(record->field_begin(), record->field_end());
        if (fields.size() == 2 && fields[0]->getName() == "real" && fields[1]->getName() == "imag" &&
            !fields[0]->isBitField() && !fields[1]->isBitField() &&
            fields[0]->getType().getCanonicalType() == expected && fields[1]->getType().getCanonicalType() == expected)
        {
            // EN: Item 3.13. The name and fields are not enough: the carrier must have the size and alignment of
            //     `_Complex T` (a `#pragma pack` or an attribute in the header could change them).
            // ES: Item 3.13. El nombre y los campos no bastan: el portador debe tener el tamano y la alineacion de
            //     `_Complex T` (un `#pragma pack` o un atributo del encabezado podria cambiarlos).
            const clang::QualType complex = context.getComplexType(expected);
            const clang::QualType carrier = context.getCanonicalTagType(record);
            if (context.getTypeSize(carrier) != context.getTypeSize(complex) ||
                context.getTypeAlign(carrier) != context.getTypeAlign(complex)) continue;
            return record;
        }
    }
    return nullptr;
}

CppType describeType(clang::QualType type, clang::ASTContext& context, unsigned depth = 0)
{
    CppType result;
    if (type.isNull()) return result;
    // T19-001: the carrier of a pointer to a C variable length array. / El portador de un puntero a un arreglo C de
    //     longitud variable.
    if (const auto flattened = flattenVariableArrayPointers(type, context); flattened != type)
    {
        result = describeType(flattened, context, depth);
        result.spelling = type.getAsString();
        return result;
    }
    result.spelling = type.getAsString();
    const clang::QualType canonical = type.getCanonicalType();
    result.canonicalSpelling = canonical.getAsString();
    result.unqualifiedCanonicalSpelling = canonical.getUnqualifiedType().getAsString();
    result.isConst = canonical.isConstQualified();
    result.isVolatile = canonical.isVolatileQualified();
    result.isRestrict = canonical.isRestrictQualified();
    const clang::Type* raw = canonical.getTypePtr();
    // C3-02: the width of `_BitInt(N)`. / El ancho de `_BitInt(N)`.
    if (const auto* bitInt = raw->getAs<clang::BitIntType>()) result.bitIntWidth = bitInt->getNumBits();
    // EN: T19-001. A `_Complex T` is described as its synthetic struct `{ T real; T imag; }`, which has the
    //     representation C11 6.2.5p13 gives it and is passed like it in every calling convention of the target.
    //     In C++ (a Clang extension, usually a C header read as C++) the struct is a trivial C++ record.
    // ES: T19-001. Un `_Complex T` se describe como su struct sintetico `{ T real; T imag; }`, que tiene la
    //     representacion que le da C11 6.2.5p13 y se pasa como el en cada convencion de llamada del destino. En C++
    //     (una extension de Clang, por lo comun un encabezado C leido como C++) el struct es un record C++ trivial.
    if (const auto* complex = llvm::dyn_cast<clang::ComplexType>(raw))
        if (const auto* carrier = complexCarrierRecord(context, complex->getElementType()))
        {
            const clang::QualType record = context.getQualifiedType(
                context.getCanonicalTagType(carrier), canonical.getLocalQualifiers());
            CppType described = describeType(record, context, depth);
            described.spelling = result.spelling;
            return described;
        }
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
        // EN: T19-005. In C++ too, `typedef enum { ... } E;` gives the enum the name E for linkage, as Clang
        //     mangles it.
        // ES: T19-005. Tambien en C++, `typedef enum { ... } E;` da al enum el nombre E para el enlace, como lo
        //     decora Clang.
        const auto* anonymousAlias = declaration->getName().empty()
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
            // T19-036: only a specialization without a NativeSharp name leaves the identity inexact.
            // T19-036: solo una especializacion sin nombre NativeSharp deja la identidad inexacta.
            const auto* enclosingSpecialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(current);
            identity.specializationContext |= enclosingSpecialization &&
                !(current == record ? specializationName.has_value()
                                    : cppSpecializationName(*enclosingSpecialization).has_value());
            if (const auto* ns = llvm::dyn_cast<clang::NamespaceDecl>(current))
            {
                if (ns->isAnonymousNamespace()) identity.anonymousContext = true;
                else identity.namespaceSegments.push_back(ns->getNameAsString());
            }
            else if (current != record)
                if (const auto* enclosing = llvm::dyn_cast<clang::RecordDecl>(current))
                {
                    const auto name = cppEnclosingRecordName(*enclosing).value_or(enclosing->getNameAsString());
                    if (name.empty()) identity.anonymousContext = true;
                    else identity.containingRecords.push_back(name);
                }
        }
        std::reverse(identity.namespaceSegments.begin(), identity.namespaceSegments.end());
        std::reverse(identity.containingRecords.begin(), identity.containingRecords.end());
        result.recordIdentity = std::move(identity);
        // C3-06: the members of a transparent union. / Los miembros de una union transparente.
        if (record->isUnion() && record->hasAttr<clang::TransparentUnionAttr>() && depth < 64)
        {
            auto members = std::make_shared<std::vector<CppType>>();
            if (const auto* unionDecl = llvm::dyn_cast<clang::RecordDecl>(record))
            for (const auto* field : unionDecl->fields())
                members->push_back(describeType(field->getType(), context, depth + 1));
            if (!members->empty()) result.transparentUnionMembers = std::move(members);
        }
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
        // T19-001: a pointer has its size even when its pointee is variably modified (a callback with a VLA
        //     parameter). / Un puntero tiene su tamano aunque su apuntado sea variablemente modificado.
        (!raw->isVariablyModifiedType() || result.kind == CppTypeKind::Pointer))
    {
        result.sizeBits = context.getTypeSize(canonical);
        result.alignmentBits = context.getTypeAlign(canonical);
    }
    return result;
}

// EN: T19-001. Adds to the types reached from a C type the element of each `_Complex T` it contains, through
//     pointers, arrays, `_Atomic`, function prototypes and the fields of records.
// ES: T19-001. Agrega a los tipos alcanzados desde un tipo C el elemento de cada `_Complex T` que contiene, a traves
//     de punteros, arreglos, `_Atomic`, prototipos de funcion y los campos de records.
void collectComplexElements(clang::QualType type, clang::ASTContext& context, std::set<const clang::Type*>& visited,
                            std::vector<std::pair<clang::QualType, clang::SourceLocation>>& elements,
                            clang::SourceLocation use, unsigned depth = 0)
{
    if (type.isNull() || depth > 64) return;
    const clang::QualType canonical = type.getCanonicalType();
    const clang::Type* raw = canonical.getTypePtr();
    if (!visited.insert(raw).second) return;
    if (const auto* complex = llvm::dyn_cast<clang::ComplexType>(raw))
    {
        const clang::QualType element = complex->getElementType().getCanonicalType().getUnqualifiedType();
        bool known = false;
        for (const auto& existing : elements) known |= existing.first == element;
        if (!known) elements.emplace_back(element, use);
        return;
    }
    if (const auto* atomic = llvm::dyn_cast<clang::AtomicType>(raw))
        return collectComplexElements(atomic->getValueType(), context, visited, elements, use, depth + 1);
    if (raw->isPointerType())
        return collectComplexElements(raw->getPointeeType(), context, visited, elements, use, depth + 1);
    if (const auto* array = context.getAsArrayType(canonical))
        return collectComplexElements(array->getElementType(), context, visited, elements, use, depth + 1);
    if (const auto* function = raw->getAs<clang::FunctionType>())
    {
        collectComplexElements(function->getReturnType(), context, visited, elements, use, depth + 1);
        if (const auto* prototype = llvm::dyn_cast<clang::FunctionProtoType>(function))
            for (const auto parameter : prototype->param_types())
                collectComplexElements(parameter, context, visited, elements, use, depth + 1);
        return;
    }
    if (const auto* recordType = raw->getAs<clang::RecordType>())
    {
        const auto* record = recordType->getDecl()->getDefinition();
        if (!record) return;
        for (const auto* field : record->fields())
            collectComplexElements(field->getType(), context, visited, elements, use, depth + 1);
    }
}

// EN: T19-001. Declares, in a C translation unit, one synthetic C struct `{ T real; T imag; }` for each `_Complex T`
//     that its functions, variables, typedefs and records use, unless the unit already declares that name. The
//     struct is located at the first declaration that uses the type, so diagnostics and the inventory have a
//     source; the inventory imports it like any other C struct, and the same struct of another header is the same
//     C type by its Clang identity (T08-D09). A complex value takes the paths of a C record by value: parameters,
//     results, fields, globals, pointers, arrays and callbacks. In a C++ unit (`_Complex` is a Clang extension
//     there) the struct is a trivial C++ record, passed like the other trivial C++ records.
// ES: T19-001. Declara, en una unidad C, un struct C sintetico `{ T real; T imag; }` por cada `_Complex T` que usan
//     sus funciones, variables, typedefs y records, salvo que la unidad ya declare ese nombre. El struct se ubica en
//     la primera declaracion que usa el tipo, asi que los diagnosticos y el inventario tienen un origen; el
//     inventario lo importa como cualquier struct C, y el mismo struct de otro encabezado es el mismo tipo C por su
//     identidad de Clang (T08-D09). Un valor complejo sigue los caminos de un record C por valor: parametros,
//     resultados, campos, globales, punteros, arreglos y callbacks. En una unidad C++ (alli `_Complex` es una
//     extension de Clang) el struct es un record C++ trivial, que se pasa como los demas records C++ triviales.
void synthesizeComplexCarriers(clang::ASTContext& context)
{
    std::set<const clang::Type*> visited;
    std::vector<std::pair<clang::QualType, clang::SourceLocation>> elements;
    std::vector<const clang::DeclContext*> pending{context.getTranslationUnitDecl()};
    while (!pending.empty())
    {
        const clang::DeclContext* current = pending.back();
        pending.pop_back();
        for (const clang::Decl* declaration : current->decls())
        {
            if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
                pending.push_back(linkage);
            else if (const auto* nested = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
                pending.push_back(nested);
            else if (const auto* value = llvm::dyn_cast<clang::ValueDecl>(declaration))
                collectComplexElements(value->getType(), context, visited, elements, value->getLocation());
            else if (const auto* alias = llvm::dyn_cast<clang::TypedefNameDecl>(declaration))
                collectComplexElements(alias->getUnderlyingType(), context, visited, elements, alias->getLocation());
            else if (const auto* record = llvm::dyn_cast<clang::RecordDecl>(declaration))
                collectComplexElements(context.getCanonicalTagType(record), context, visited, elements,
                                       record->getLocation());
        }
    }
    clang::TranslationUnitDecl* unit = context.getTranslationUnitDecl();
    for (const auto& [element, use] : elements)
    {
        const std::string name = complexCarrierName(element);
        if (name.empty()) continue;
        auto& identifier = context.Idents.get(name);
        if (!unit->lookup(&identifier).empty()) continue;
        clang::RecordDecl* record = context.getLangOpts().CPlusPlus
            ? clang::CXXRecordDecl::Create(context, clang::TagTypeKind::Struct, unit, use, use, &identifier)
            : clang::RecordDecl::Create(context, clang::TagTypeKind::Struct, unit, use, use, &identifier);
        record->startDefinition();
        for (const char* fieldName : {"real", "imag"})
        {
            auto* field = clang::FieldDecl::Create(context, record, use, use, &context.Idents.get(fieldName), element,
                                                   context.getTrivialTypeSourceInfo(element), nullptr, false,
                                                   clang::ICIS_NoInit);
            field->setAccess(clang::AS_public);
            record->addDecl(field);
        }
        record->completeDefinition();
        unit->addDecl(record);
    }
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
        // EN: T19-005. A private or protected enum of such a record is collected with its access, so that
        //     NativeSharp reports the access when a program names it; an unnamed one gives its enumerators to the
        //     record.
        // ES: T19-005. Un enum privado o protegido de ese record se recoge con su acceso, para que NativeSharp
        //     informe el acceso cuando un programa lo nombra; uno sin nombre da sus enumeradores al record.
        const auto* enumeration = llvm::dyn_cast<clang::EnumDecl>(declaration);
        if (!enumeration || enumeration->isImplicit() || enumeration->isDependentContext() ||
            (llvm::isa<clang::CXXRecordDecl>(enumeration->getDeclContext()) &&
             enumeration->getAccess() == clang::AS_none) ||
            !seen.insert(enumeration->getCanonicalDecl()).second)
            continue;
        if (const auto* definition = enumeration->getDefinition()) enumeration = definition;
        CppEnumDeclaration value;
        value.clangUsr = declarationUsr(enumeration);
        value.isCxx = astContext.getLangOpts().CPlusPlus;
        // T19-005: also in C++. / Tambien en C++.
        const auto* anonymousAlias = enumeration->getName().empty()
            ? enumeration->getTypedefNameForAnonDecl() : nullptr;
        value.name = anonymousAlias ? anonymousAlias->getNameAsString() : enumeration->getNameAsString();
        value.qualifiedName = anonymousAlias ? anonymousAlias->getQualifiedNameAsString()
                                             : enumeration->getQualifiedNameAsString();
        value.containingRecords = enclosingCppRecords(*enumeration, astContext);
        if (llvm::isa<clang::CXXRecordDecl>(enumeration->getDeclContext()))
            value.access = enumeration->getAccess() == clang::AS_private ? CppAccess::Private :
                enumeration->getAccess() == clang::AS_protected ? CppAccess::Protected : CppAccess::Public;
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

// EN: T19-005. A namespace alias `namespace A = B;` names every type of B as A::T, and the types of the
//     namespaces nested in B as A::N::T, as C++ qualified lookup does. Each such name becomes a type alias of
//     the type it names, so NativeSharp resolves `A.T` like `B.T`. Inline namespaces add no segment. Class
//     templates, their specializations and the names B already has under the alias are left out.
// ES: T19-005. Un alias de namespace `namespace A = B;` nombra cada tipo de B como A::T, y los tipos de los
//     namespaces anidados en B como A::N::T, como la busqueda calificada de C++. Cada nombre se vuelve un alias
//     del tipo que nombra, asi que NativeSharp resuelve `A.T` como `B.T`. Los namespaces inline no agregan
//     segmento. Quedan fuera las plantillas de clase, sus especializaciones y los nombres que ya existen.
void addNamespaceAliasTypeNames(clang::ASTContext& astContext, std::vector<CppTypeAliasDeclaration>& result)
{
    // EN: T19-005. Also a using-directive `using namespace B;` in a namespace N or in the global namespace, which
    //     names the types of B as N::T (or T), and a using-declaration `using B::T;`, which names one type. The
    //     names a scope declares itself are kept, as C++ lookup does.
    // ES: T19-005. Tambien una directiva `using namespace B;` en un namespace N o en el global, que nombra los
    //     tipos de B como N::T (o T), y una declaracion `using B::T;`, que nombra un tipo. Se conservan los
    //     nombres que el ambito declara el mismo, como la busqueda de C++.
    std::vector<std::tuple<std::string, const clang::NamespaceDecl*, const clang::Decl*>> aliases;
    std::vector<std::pair<std::string, const clang::UsingShadowDecl*>> shadows;
    std::function<void(const clang::DeclContext&)> visit = [&](const clang::DeclContext& scope) {
        for (const auto* declaration : scope.decls())
        {
            if (const auto* alias = llvm::dyn_cast<clang::NamespaceAliasDecl>(declaration))
            {
                const auto* target = alias->getNamespace();
                const auto here = fullNamespacePath(alias->getDeclContext());
                if (target && here && fullNamespacePath(target))
                    aliases.emplace_back((here->empty() ? "" : *here + "::") + alias->getNameAsString(), target, alias);
            }
            else if (const auto* directive = llvm::dyn_cast<clang::UsingDirectiveDecl>(declaration))
            {
                const auto* target = directive->getNominatedNamespace();
                const auto here = fullNamespacePath(directive->getDeclContext());
                const auto there = target ? fullNamespacePath(target) : std::nullopt;
                if (target && here && there && !there->empty() && *here != *there)
                    aliases.emplace_back(*here, target, directive);
            }
            else if (const auto* shadow = llvm::dyn_cast<clang::UsingShadowDecl>(declaration))
            {
                const auto here = fullNamespacePath(shadow->getDeclContext());
                if (here && llvm::isa<clang::TypeDecl>(shadow->getTargetDecl()))
                    shadows.emplace_back(*here, shadow);
            }
            else if (const auto* nested = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
            {
                if (!nested->isAnonymousNamespace()) visit(*nested);
            }
            else if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
                visit(*linkage);
        }
    };
    visit(*astContext.getTranslationUnitDecl());
    if (aliases.empty() && shadows.empty()) return;
    std::set<std::string> known;
    for (const auto& value : result) known.insert(value.qualifiedName);
    const auto join = [](const std::string& prefix, const std::string& name) {
        return prefix.empty() ? name : prefix + "::" + name;
    };
    // EN: the types the scopes declare themselves hide the names a directive brings.
    // ES: los tipos que declaran los propios ambitos ocultan los nombres que trae una directiva.
    std::function<void(const clang::DeclContext&, const std::string&)> declare =
        [&](const clang::DeclContext& scope, const std::string& prefix) {
        for (const auto* declaration : scope.decls())
        {
            if (const auto* nested = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
            {
                if (!nested->isAnonymousNamespace())
                    declare(*nested, nested->isInline() ? prefix : join(prefix, nested->getNameAsString()));
            }
            else if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
                declare(*linkage, prefix);
            else if (const auto* type = llvm::dyn_cast<clang::TypeDecl>(declaration))
            {
                if (!type->isImplicit() && !type->getName().empty())
                    known.insert(join(prefix, type->getNameAsString()));
            }
        }
    };
    declare(*astContext.getTranslationUnitDecl(), "");
    const auto aliasedType = [&](const clang::TypeDecl& type) -> clang::QualType {
        if (const auto* typedefName = llvm::dyn_cast<clang::TypedefNameDecl>(&type))
            return typedefName->getUnderlyingType();
        if (const auto* tag = llvm::dyn_cast<clang::TagDecl>(&type))
        {
            const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(tag);
            if (llvm::isa<clang::ClassTemplateSpecializationDecl>(tag) ||
                (record && record->getDescribedClassTemplate())) return {};
            return astContext.getCanonicalTagType(tag);
        }
        return {};
    };
    const auto push = [&](const clang::TypeDecl& type, const std::string& qualified, const clang::Decl& origin) {
        const clang::QualType named = aliasedType(type);
        if (named.isNull()) return;
        CppTypeAliasDeclaration value;
        value.name = type.getNameAsString();
        value.qualifiedName = qualified;
        value.underlyingType = describeType(named, astContext);
        // EN: T19-005. The name has no declaration of its own: its identity is the named type's and the name, so a
        //     precompiled template can carry it.
        // ES: T19-005. El nombre no tiene declaracion propia: su identidad es la del tipo nombrado y el nombre, para
        //     que una plantilla precompilada pueda llevarlo.
        if (const auto usr = declarationUsr(&type)) value.clangUsr = *usr + "@nsc-name:" + qualified;
        const auto location = astContext.getSourceManager().getPresumedLoc(origin.getLocation());
        if (location.isValid())
            value.location = CppSourcePoint{
                pathFromUtf8(location.getFilename()), location.getLine(), location.getColumn()};
        result.push_back(std::move(value));
    };
    for (const auto& [here, shadow] : shadows)
    {
        const auto* type = llvm::cast<clang::TypeDecl>(shadow->getTargetDecl());
        if (type->getName().empty()) continue;
        const std::string qualified = join(here, type->getNameAsString());
        if (known.insert(qualified).second) push(*type, qualified, *shadow);
    }
    std::function<void(const clang::DeclContext&, const std::string&, const clang::Decl&, unsigned)>
        add = [&](const clang::DeclContext& scope, const std::string& prefix, const clang::Decl& alias,
                  unsigned depth) {
        if (depth > 16) return;
        for (const auto* declaration : scope.decls())
        {
            if (const auto* nested = llvm::dyn_cast<clang::NamespaceDecl>(declaration))
            {
                if (nested->isAnonymousNamespace()) continue;
                add(*nested, nested->isInline() ? prefix : join(prefix, nested->getNameAsString()), alias,
                    depth + 1);
                continue;
            }
            if (const auto* linkage = llvm::dyn_cast<clang::LinkageSpecDecl>(declaration))
            {
                add(*linkage, prefix, alias, depth + 1);
                continue;
            }
            const auto* type = llvm::dyn_cast<clang::TypeDecl>(declaration);
            if (!type || type->isImplicit() || type->getName().empty()) continue;
            const std::string qualified = join(prefix, type->getNameAsString());
            if (aliasedType(*type).isNull() || !known.insert(qualified).second) continue;
            push(*type, qualified, alias);
        }
    };
    for (const auto& [prefix, target, alias] : aliases)
        for (const auto* reopening : target->redecls())
            add(*reopening, prefix, *alias, 0);
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
                // EN: T19-005. Also a directive of the global namespace (`using namespace T;`), which names T::f as f.
                // ES: T19-005. Tambien una directiva del namespace global, que nombra T::f como f.
                if (here && there && !there->empty() && *here != *there)
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
                    if (alternate.starts_with("::")) alternate.erase(0, 2);
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
        // EN: Item 6.2. A C99 `inline` or `extern inline` definition in the header (`inline int f(int x) {...}`) is
        //     not guaranteed to have an external definition in any library; NativeSharp emits its own copy under a
        //     private symbol, as for `static inline`.
        // ES: Item 6.2. Una definicion C99 `inline` o `extern inline` en el encabezado no tiene garantizada una
        //     definicion externa en ninguna biblioteca; NativeSharp emite su propia copia con un simbolo privado,
        //     como para `static inline`.
        if (!astContext.getLangOpts().CPlusPlus && value.externalLinkage && definition && definition->hasBody() &&
            definition->isInlineSpecified() && !function->hasAttr<clang::DLLImportAttr>())
        {
            value.isCStaticInlineDefinition = true;
            value.externalLinkage = false;
            value.languageLinkage = CppLanguageLinkage::None;
        }
        value.dllImport = function->hasAttr<clang::DLLImportAttr>();
        value.returnsTwice = function->hasAttr<clang::ReturnsTwiceAttr>();
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
            // EN: Item 8.3. `__attribute__((nonnull))` on the parameter or on the function (all pointer parameters,
            //     or the listed ones) and the `_Nonnull` nullability of its type.
            // ES: Item 8.3. `__attribute__((nonnull))` en el parametro o en la funcion (todos los punteros, o los
            //     listados) y la nulabilidad `_Nonnull` de su tipo.
            if (parameter->getType()->isPointerType())
            {
                const auto nullability = parameter->getType()->getNullability();
                captured.nonNull = parameter->hasAttr<clang::NonNullAttr>() ||
                    (nullability && *nullability == clang::NullabilityKind::NonNull);
                for (const auto* attribute : function->specific_attrs<clang::NonNullAttr>())
                    if (attribute->args_size() == 0 ||
                        attribute->isNonNull(static_cast<unsigned>(value.parameters.size())))
                        captured.nonNull = true;
            }
            if (const auto* size = parameter->getAttr<clang::PassObjectSizeAttr>())
            {
                captured.objectSizeMode = static_cast<std::uint8_t>(size->getType());
                captured.objectSizeDynamic = size->isDynamic();
            }
            value.parameters.push_back(std::move(captured));
        }
        // Item 15.6: a C++ internal definition of the header is imported under its mangled name. / Una definicion
        //     interna C++ del encabezado se importa con su nombre decorado.
        if (astContext.getLangOpts().CPlusPlus && !value.externalLinkage && definition && definition->hasBody() &&
            definition->getTemplatedKind() == clang::FunctionDecl::TK_NonTemplate && !definition->isDependentContext() &&
            mangler.shouldMangleDeclName(function))
        {
            llvm::SmallString<128> symbol;
            llvm::raw_svector_ostream stream(symbol);
            mangler.mangleName(clang::GlobalDecl(function), stream);
            value.externalSymbol = std::string(symbol.str());
            value.cppInternalDefinition = true;
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
            // EN: T19-036. A C++ `__vectorcall` function with a record value keeps Clang's plan; the backend
            //     checks its own vectorcall classification against it before declaring the import.
            // ES: T19-036. Una funcion C++ `__vectorcall` con un record por valor conserva el plan de Clang; el
            //     backend compara con el su propia clasificacion vectorcall antes de declarar la importacion.
            const bool plannedScalarConvention = abi_plan::cppPlannedScalarConventionId(value.callingConvention) != 0;
            // EN: T19-036. A vector value keeps Clang's plan too: NativeSharp's carrier is an ordinary struct, so the
            //     vector ABI of each position comes from the plan of this declaration.
            // ES: T19-036. Un valor vector tambien conserva el plan de Clang: el portador NativeSharp es un struct
            //     ordinario, asi que el ABI vectorial de cada posicion sale del plan de esta declaracion.
            // T19-004: a 128-bit integer too. / Tambien un entero de 128 bits.
            const auto planned = [&](clang::QualType type) {
                return type->isVectorType() ||
                    (type->isIntegerType() && !type->isEnumeralType() && astContext.getTypeSize(type) == 128);
            };
            const bool vectorValue = planned(function->getReturnType()) ||
                std::any_of(function->param_begin(), function->param_end(), [&](const auto* parameter) {
                    return planned(parameter->getType());
                });
            if (astContext.getLangOpts().CPlusPlus && generator &&
                (value.callingConvention == "regcall" || value.callingConvention == "sysv_abi" ||
                 value.callingConvention == "vectorcall" || value.callingConvention == "preserve_none" ||
                 plannedScalarConvention ||
                 (vectorValue && (value.callingConvention == "cdecl" || value.callingConvention == "ms_abi"))) &&
                !value.variadic && value.hasPrototype && value.externalSymbol &&
                (function->getReturnType()->isVoidType() || !function->getReturnType()->isIncompleteType()) &&
                std::all_of(function->param_begin(), function->param_end(), [](const auto* parameter) {
                    return !parameter->getType()->isIncompleteType();
                }) &&
                // T19-001: a `_Complex` value is a record too (its synthetic carrier). / Un valor `_Complex`
                //     tambien es un record (su portador sintetico).
                (value.callingConvention == "preserve_none" || plannedScalarConvention || vectorValue ||
                 function->getReturnType()->isRecordType() || function->getReturnType()->isAnyComplexType() ||
                 std::any_of(function->param_begin(), function->param_end(), [](const auto* parameter) {
                     return parameter->getType()->isRecordType() || parameter->getType()->isAnyComplexType();
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
                    // T19-028: or intel_ocl_bicc records in the Win64 form. / O records intel_ocl_bicc Win64.
                    if (abi_plan::isCppPlannedScalarConventionPlan(*plan, signature, *value.externalSymbol) ||
                        abi_plan::isIntelOclWin64RecordPlan(*plan, signature, *value.externalSymbol) ||
                        abi_plan::isSwiftRecordPlan(*plan, signature, *value.externalSymbol))
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
        // EN: T19-006. One adapter per number of written arguments; the declaration itself then requires them all.
        // ES: T19-006. Un adaptador por cada numero de argumentos escritos; la declaracion exige entonces todos.
        const auto thunkStart = astContext.getLangOpts().CPlusPlus && value.externalSymbol && value.clangUsr &&
                !value.variadic && value.hasPrototype && value.callingConvention == "cdecl"
            ? defaultArgumentThunkStart(value.parameters) : std::nullopt;
        if (thunkStart)
        {
            for (std::size_t arity = *thunkStart; arity < value.parameters.size(); ++arity)
            {
                CppFunctionDeclaration thunk = value;
                thunk.parameters.resize(arity);
                thunk.externalSymbol = cppDefaultArgumentThunkSymbol("#function", *value.clangUsr, arity);
                thunk.clangUsr = cppDefaultArgumentThunkUsr(*value.clangUsr, arity);
                thunk.canonicalFunctionType = truncatedCanonicalFunctionType(*function, arity, astContext);
                thunk.languageLinkage = CppLanguageLinkage::Cpp;
                thunk.dllImport = false;
                thunk.isCStaticInlineDefinition = false;
                thunk.abiPlan.reset();
                result.push_back(std::move(thunk));
            }
            for (auto& parameter : value.parameters) clearDefaultArgument(parameter);
        }
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
        // EN: Item 9.5. A friend function defined inside a class (`friend int dot(const FP&, const FP&) {...}`)
        //     belongs to the enclosing namespace but only argument-dependent lookup finds it in C++; NativeSharp
        //     imports it as a free function of that namespace.
        // ES: Item 9.5. Una funcion friend definida dentro de una clase pertenece al namespace que la rodea pero en
        //     C++ solo la encuentra la busqueda dependiente de argumentos; NativeSharp la importa como funcion libre
        //     de ese namespace.
        if (const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(declaration);
            record && record->isThisDeclarationADefinition() && !record->isDependentContext())
            for (const auto* friendDeclaration : record->friends())
                if (const auto* function = llvm::dyn_cast_or_null<clang::FunctionDecl>(
                        friendDeclaration->getFriendDecl());
                    function && function->doesThisDeclarationHaveABody() && !function->isDependentContext())
                    collect(function);
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
                    std::vector<CppGlobalDeclaration>& result);

// EN: T19-005. The names a namespace alias, a using-directive (also of the global namespace) or a
//     using-declaration give a C++ global, as addAlternateFunctionNames does for functions. Each name is one
//     more declaration of the same storage (same symbol and USR), so NativeSharp finds `counter` after
//     `using T::counter;` and `A.counter` after `namespace A = T;`. A name the scope declares itself is kept.
// ES: T19-005. Los nombres que un alias de namespace, una directiva using (tambien del namespace global) o una
//     declaracion using dan a una global C++, como addAlternateFunctionNames para las funciones. Cada nombre es
//     una declaracion mas del mismo almacenamiento (mismo simbolo y USR), asi que NativeSharp encuentra
//     `counter` despues de `using T::counter;` y `A.counter` despues de `namespace A = T;`. Se conserva el nombre
//     que el ambito declara el mismo.
void addAlternateGlobalNames(clang::ASTContext& context, std::vector<CppGlobalDeclaration>& globals)
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
                if (here && there && !there->empty() && *here != *there) directives.emplace_back(*here, *there);
            }
            else if (const auto* shadow = llvm::dyn_cast<clang::UsingShadowDecl>(declaration))
            {
                const auto* variable = llvm::dyn_cast<clang::VarDecl>(shadow->getTargetDecl());
                const auto here = fullNamespacePath(shadow->getDeclContext());
                const auto usr = variable ? declarationUsr(variable) : std::nullopt;
                if (variable && here && usr)
                    shadows.emplace_back(*usr, (here->empty() ? "" : *here + "::") + variable->getNameAsString());
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
    std::set<std::string> declared;
    for (const auto& global : globals) declared.insert(global.qualifiedName);
    for (const auto& shadow : shadows) declared.insert(shadow.second);
    std::vector<CppGlobalDeclaration> alternates;
    for (const auto& global : globals)
    {
        if (!global.clangUsr || global.macro) continue;
        std::vector<std::string> names{global.qualifiedName};
        for (const auto& [usr, alternate] : shadows)
            if (*global.clangUsr == usr) names.push_back(alternate);
        // Each new name is expanded again, so aliases and directives compose; the bound keeps a cycle finite.
        for (std::size_t next = 0; next < names.size() && names.size() < 64; ++next)
        {
            const auto separator = names[next].rfind("::");
            if (separator == std::string::npos) continue;
            const auto path = names[next].substr(0, separator);
            std::vector<std::string> reached;
            for (const auto& [alias, target] : aliases)
                if (path == target || path.starts_with(target + "::"))
                    reached.push_back(alias + path.substr(target.size()) + "::" + global.name);
            for (const auto& [scope, target] : directives)
                if (path == target || path.starts_with(target + "::"))
                {
                    auto alternate = scope + path.substr(target.size()) + "::" + global.name;
                    if (alternate.starts_with("::")) alternate.erase(0, 2);
                    if (!declared.contains(alternate)) reached.push_back(std::move(alternate));
                }
            for (auto& alternate : reached)
                if (std::find(names.begin(), names.end(), alternate) == names.end())
                    names.push_back(std::move(alternate));
        }
        for (std::size_t index = 1; index < names.size(); ++index)
        {
            auto copy = global;
            copy.qualifiedName = names[index];
            alternates.push_back(std::move(copy));
        }
    }
    globals.insert(globals.end(), std::make_move_iterator(alternates.begin()),
                   std::make_move_iterator(alternates.end()));
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

// EN: T19-004. Whether every unnamed struct or union of a C++ record (an anonymous member, or the unnamed type of
//     a named member, also nested) is trivial: then it has the semantics of a C one and travels as a C carrier.
// ES: T19-004. Si todo struct o union sin nombre de un record C++ (un miembro anonimo, o el tipo sin nombre de un
//     miembro con nombre, tambien anidado) es trivial: entonces tiene la semantica de uno de C y viaja como un
//     portador C.
bool cppUnnamedRecordsAreTrivial(const clang::RecordDecl& owner, clang::ASTContext& context, unsigned depth = 0)
{
    if (depth > 32) return false;
    for (const auto* field : owner.fields())
    {
        clang::QualType element = field->getType();
        for (unsigned level = 0; level < 64; ++level)
        {
            if (const auto* array = context.getAsArrayType(element)) element = array->getElementType();
            else if (element->isPointerType()) element = element->getPointeeType();
            else break;
        }
        const auto* type = element->getAs<clang::RecordType>();
        const auto* definition = type ? type->getDecl()->getDefinition() : nullptr;
        if (!definition || !definition->getName().empty() || definition->getTypedefNameForAnonDecl()) continue;
        const auto* cpp = llvm::dyn_cast<clang::CXXRecordDecl>(definition);
        if (!cpp || !cpp->isTriviallyCopyable() || !cpp->hasTrivialDefaultConstructor() ||
            !cpp->hasTrivialDestructor() || cpp->getNumBases() != 0 ||
            !cppUnnamedRecordsAreTrivial(*definition, context, depth + 1)) return false;
    }
    return true;
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
        const bool cppUnnamedTrivial = value.isCxx && record->getDefinition() &&
            cppUnnamedRecordsAreTrivial(*record->getDefinition(), astContext);
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
                // T19-036: a specialization encloses with its NativeSharp name. / Con su nombre NativeSharp.
                const auto name = cppEnclosingRecordName(*enclosing).value_or(enclosing->getNameAsString());
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
        // EN: T19-036. A record nested in a named specialization has an exact identity; Clang's ODR hash is still
        //     left out inside any specialization, as before.
        // ES: T19-036. Un record anidado en una especializacion con nombre tiene identidad exacta; el hash ODR de Clang
        //     se sigue omitiendo dentro de cualquier especializacion.
        bool insideSpecialization = false;
        for (const clang::DeclContext* current = record; current != nullptr; current = current->getParent())
        {
            const auto* enclosingSpecialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(current);
            insideSpecialization |= enclosingSpecialization && !(current == record && specializationName);
            value.specializationContext |= enclosingSpecialization &&
                !(current == record ? specializationName.has_value()
                                    : cppSpecializationName(*enclosingSpecialization).has_value());
        }
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
            if (!insideSpecialization)
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
                // T19-004: also in a C++ record whose unnamed records are trivial. / Tambien en un record C++
                //     cuyos records sin nombre son triviales.
                projected.isAnonymousCarrier = (!value.isCxx || cppUnnamedTrivial) && field->isAnonymousStructOrUnion();
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
                // EN: Item 9.8. Static data members of public bases are members of the derived record too
                //     (`Derived::value` from `std::true_type`-like traits), unless the record hides the name; they
                //     keep the base's own storage symbol.
                // ES: Item 9.8. Los miembros de datos static de las bases publicas tambien son del record derivado,
                //     salvo que el record oculte el nombre; conservan el simbolo de almacenamiento de la base.
                std::vector<const clang::VarDecl*> staticMembers;
                std::set<std::string> staticNames;
                std::function<void(const clang::CXXRecordDecl&, unsigned)> gatherStatics =
                    [&](const clang::CXXRecordDecl& owner, unsigned depth) {
                        for (const clang::Decl* member : owner.decls())
                            if (const auto* field = llvm::dyn_cast<clang::VarDecl>(member);
                                field && field->isStaticDataMember() && !field->isImplicit() &&
                                (depth == 0 || field->getAccess() == clang::AS_public) &&
                                staticNames.insert(field->getNameAsString()).second)
                                staticMembers.push_back(field);
                        if (depth >= 16 || !owner.hasDefinition()) return;
                        for (const auto& base : owner.bases())
                            if (base.getAccessSpecifier() == clang::AS_public && !base.isVirtual())
                                if (const auto* baseRecord = base.getType()->getAsCXXRecordDecl();
                                    baseRecord && baseRecord->hasDefinition())
                                    gatherStatics(*baseRecord->getDefinition(), depth + 1);
                    };
                gatherStatics(*cppRecord, 0);
                for (const clang::VarDecl* field : staticMembers)
                {
                    CppStaticField projected;
                    projected.clangUsr = declarationUsr(field);
                    projected.name = field->getNameAsString();
                    projected.type = describeType(field->getType(), astContext);
                    projected.access = describeAccess(field->getAccess());
                    projected.threadLocal = field->getTLSKind() != clang::VarDecl::TLS_None;
                    // EN: T19-036. The same on-demand rule as a C++ global (T10-004).
                    // ES: T19-036. La misma regla bajo demanda que una global C++ (T10-004).
                    if (projected.threadLocal && field->getTLSKind() == clang::VarDecl::TLS_Dynamic)
                    {
                        bool constant = false;
                        for (const auto* redeclaration : field->redecls())
                            constant |= redeclaration->hasAttr<clang::ConstInitAttr>();
                        if (const auto* definition = field->getDefinition())
                            constant |= definition->hasInit() && definition->hasConstantInitialization();
                        projected.cppTlsOnDemand = !constant;
                    }
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
                const auto ownerUsr = declarationUsr(cppRecord);
                for (const clang::CXXMethodDecl* method : cppRecord->methods())
                {
                    if (method->isImplicit() || method->isDependentContext() ||
                        method->getDescribedFunctionTemplate()) continue;
                    CppMethodDeclaration projected = captureMethod(method);
                    // EN: T19-006. A public method with default arguments (a method carries none to NativeSharp) is
                    //     called with fewer arguments through one Clang adapter per number of written arguments: a
                    //     free adapter for a static method, a member adapter (self first) for an instance method.
                    // ES: T19-006. Un metodo publico con argumentos predeterminados (un metodo no lleva ninguno a
                    //     NativeSharp) se llama con menos argumentos por un adaptador de Clang por cada numero de
                    //     argumentos escritos: uno libre para un metodo estatico, uno de miembro para uno de
                    //     instancia.
                    // EN: T19-006. A public constructor with default arguments too, by a free adapter that constructs
                    //     the object in place; a copy or move constructor (a reference to the record first) stays
                    //     as it is.
                    // ES: T19-006. Tambien un constructor publico con argumentos predeterminados, por un adaptador
                    //     libre que construye el objeto en su lugar; un constructor de copia o de movimiento (una
                    //     referencia al record primero) queda como esta.
                    const auto* ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(method);
                    const bool adaptable = projected.kind == CppMethodKind::Method ||
                        (projected.kind == CppMethodKind::Constructor && ctor && !ctor->isCopyOrMoveConstructor());
                    const auto thunkStart = adaptable && !projected.variadic &&
                            !projected.isDeleted && projected.access == CppAccess::Public && projected.externalSymbol &&
                            projected.clangUsr && ownerUsr && projected.refQualifier == CppMethodRefQualifier::None
                        ? defaultArgumentThunkStart(projected.parameters, true) : std::nullopt;
                    if (thunkStart)
                    {
                        for (std::size_t arity = *thunkStart; arity < projected.parameters.size(); ++arity)
                        {
                            CppMethodDeclaration thunk = projected;
                            thunk.parameters.resize(arity);
                            const auto symbol = cppDefaultArgumentThunkSymbol(*ownerUsr, *projected.clangUsr, arity);
                            if (projected.isStatic || projected.kind == CppMethodKind::Constructor)
                                thunk.externalSymbol = symbol;
                            else thunk.dispatchSymbol = symbol;
                            thunk.isTrivial = false;
                            thunk.clangUsr = cppDefaultArgumentThunkUsr(*projected.clangUsr, arity);
                            thunk.canonicalFunctionType = truncatedCanonicalFunctionType(*method, arity, astContext);
                            thunk.isVirtual = false;
                            thunk.isPureVirtual = false;
                            thunk.dllImport = false;
                            value.methods.push_back(std::move(thunk));
                        }
                        for (auto& parameter : projected.parameters) clearDefaultArgument(parameter);
                    }
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
        if ((!value.isCxx || cppUnnamedTrivial) && value.complete)
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
            {
                if (!specialization->isCompleteDefinition() || specialization->isInvalidDecl() ||
                    !cppSpecializationName(*specialization)) continue;
                if (cppSpecializationQualifiedName(*specialization) == name &&
                    seen.insert(specialization->getCanonicalDecl()).second)
                    matches.push_back(specialization->getDefinition());
                // T19-036: the records nested in a named specialization. / Los records anidados en ella.
                findComparableRecords(*specialization, name, seen, matches);
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
    if (depth > 16 || type.isNull()) return false;
    // T09-003: const and volatile do not change how Clang classifies a field and the replay writes them
    // back; any other qualifier keeps the record out of the replay.
    auto qualifiers = type.getLocalQualifiers();
    qualifiers.removeConst();
    qualifiers.removeVolatile();
    // T19-030: `__unaligned` changes neither the layout nor how Clang classifies a field. / `__unaligned` no cambia
    //     el layout ni como Clang clasifica un campo.
    qualifiers.removeUnaligned();
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
        // EN: T19-036. A C++ function with a vector by value needs Clang's plan too, whatever its convention.
        // ES: T19-036. Una funcion C++ con un vector por valor tambien necesita el plan de Clang, sea cual sea su
        //     convencion.
        // T19-004: and one with a 128-bit integer. / Y una con un entero de 128 bits.
        const auto plannedType = [](const CppType& type) {
            return type.kind == CppTypeKind::FixedVector ||
                ((type.kind == CppTypeKind::SignedInteger || type.kind == CppTypeKind::UnsignedInteger ||
              // C3-03: an enum over a 128-bit integer travels as that integer. / Un enum de 128 bits viaja como su entero.
              type.kind == CppTypeKind::Enumeration) &&
                 type.sizeBits == 128);
        };
        const auto vectorValue = [&](const auto& value) {
            return plannedType(value.returnType) ||
                std::any_of(value.parameters.begin(), value.parameters.end(), [&](const auto& parameter) {
                    return plannedType(parameter.type);
                });
        };
        if (std::any_of(result.begin(), result.end(), [&](const auto& value) {
                return value.callingConvention == "regcall" ||
                    (context.getLangOpts().CPlusPlus &&
                     (value.callingConvention == "sysv_abi" || value.callingConvention == "preserve_none" ||
                      value.callingConvention == "vectorcall" ||
                      abi_plan::cppPlannedScalarConventionId(value.callingConvention) != 0 || vectorValue(value)));
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
        // EN: T19-039. A C operator of a `_Complex` carrier is reported as the C++ operator it stands for
        //     (`operator*`), so NativeSharp demands it from an expression as it demands a C++ free operator; it
        //     links by its own C name, as a macro wrapper does (cMacroWrapper).
        // ES: T19-039. Un operador C de un portador `_Complex` se informa como el operador C++ que representa
        //     (`operator*`), asi que NativeSharp lo demanda desde una expresion como a un operador libre C++;
        //     enlaza por su propio nombre C, como un envoltorio de macro (cMacroWrapper).
        if (!context.getLangOpts().CPlusPlus)
            for (auto& value : result)
            {
                // T19-039: every conversion shares one name, demanded once per unit. / Todas las conversiones
                //     comparten un nombre, demandado una vez por unidad.
                // T19-039: `op=` keeps one name per operator. / `op=` conserva un nombre por operador.
                if (value.name.starts_with("__nsc_complex_as_"))
                {
                    const auto end = value.name.find("__", 17);
                    if (end == std::string::npos) continue;
                    value.cMacroWrapper = value.name;
                    value.name = value.name.substr(0, end);
                    value.qualifiedName = value.name;
                    continue;
                }
                if (value.name.starts_with("__nsc_complex_cv__"))
                {
                    value.cMacroWrapper = value.name;
                    value.name = "__nsc_complex_cv";
                    value.qualifiedName = value.name;
                    continue;
                }
                if (!value.name.starts_with("__nsc_complex_op_")) continue;
                static const std::pair<std::string_view, std::string_view> spellings[] = {
                    {"Addition__", "+"}, {"Subtraction__", "-"}, {"Multiply__", "*"}, {"Division__", "/"},
                    {"Equality__", "=="}, {"Inequality__", "!="}, {"UnaryNegation__", "-"}, {"UnaryPlus__", "+"}};
                const std::string_view rest = std::string_view(value.name).substr(17);
                for (const auto& [prefix, spelling] : spellings)
                    if (rest.starts_with(prefix))
                    {
                        value.cMacroWrapper = value.name;
                        value.name = "operator" + std::string(spelling);
                        value.qualifiedName = value.name;
                        break;
                    }
            }
        // T19-003: a macro wrapper is imported under the macro's name. / Un envoltorio se importa con el nombre de
        //     la macro.
        for (auto& value : result)
            if (const auto found = macroOfWrapper_.find(value.name); found != macroOfWrapper_.end())
            {
                const auto& references = macroWrapperReferences_[value.name];
                for (std::size_t index = 0; index < value.parameters.size() && index < references.size(); ++index)
                    value.parameters[index].cMacroReference = references[index];
                value.cMacroWrapper = value.name;
                value.name = found->second;
                value.qualifiedName = found->second;
            }
        // T19-003: a macro expansion session reports only its wrappers. / Solo sus envoltorios.
        if (options_.macroExpansionOnly)
            std::erase_if(result, [](const CppFunctionDeclaration& value) { return value.cMacroWrapper.empty(); });
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
        // T19-003: a macro expansion session reports only its macros. / Solo sus macros.
        if (options_.macroExpansionOnly) return std::vector<CppRecordDeclaration>{};
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
        // EN: T19-030. Also `#pragma pack` (MaxFieldAlignmentAttr), which the replay infers from the offsets, and
        //     attributes that change neither the layout nor the passing of the record (deprecated, unavailable,
        //     unused, annotate). The caller compares the whole replayed layout with the catalogue.
        // ES: T19-030. Tambien `#pragma pack` (MaxFieldAlignmentAttr), que el replay infiere de los desplazamientos,
        //     y atributos que no cambian el layout ni el paso del record (deprecated, unavailable, unused,
        //     annotate). Quien llama compara todo el layout reproducido con el catalogo.
        const auto onlyAlignment = [](const clang::Decl& declaration) {
            return std::all_of(declaration.attr_begin(), declaration.attr_end(), [&](const clang::Attr* attribute) {
                if (const auto* aligned = llvm::dyn_cast<clang::AlignedAttr>(attribute))
                    return !aligned->isAlignmentDependent();
                // EN: Also may_alias, visibility, randomize_layout (no effect without a seed; the layout is
                //     compared anyway), ms_struct (the default layout of this target), transparent_union (on
                //     this target it changes neither layout nor register class) and packed on a field, which
                //     the replay writes back.
                // ES: Tambien may_alias, visibility, randomize_layout (sin efecto sin semilla; el layout se
                //     compara igual), ms_struct (el layout por defecto de este destino), transparent_union (en
                //     este destino no cambia el layout ni la clase de registro) y packed en un campo, que el
                //     replay reescribe.
                return llvm::isa<clang::DeprecatedAttr>(attribute) || llvm::isa<clang::UnavailableAttr>(attribute) ||
                    llvm::isa<clang::UnusedAttr>(attribute) || llvm::isa<clang::AnnotateAttr>(attribute) ||
                    llvm::isa<clang::MayAliasAttr>(attribute) || llvm::isa<clang::VisibilityAttr>(attribute) ||
                    llvm::isa<clang::TypeVisibilityAttr>(attribute) ||
                    llvm::isa<clang::RandomizeLayoutAttr>(attribute) ||
                    (llvm::isa<clang::RecordDecl>(declaration) &&
                     (llvm::isa<clang::MaxFieldAlignmentAttr>(attribute) || llvm::isa<clang::MSStructAttr>(attribute) ||
                      llvm::isa<clang::TransparentUnionAttr>(attribute))) ||
                    (llvm::isa<clang::FieldDecl>(declaration) && llvm::isa<clang::PackedAttr>(attribute));
            });
        };
        if (!onlyAlignment(*record) || record->field_empty()) return false;
        for (const auto* field : record->fields())
        {
            const auto type = field->getType();
            if (type.isNull() || !onlyAlignment(*field) || field->isAnonymousStructOrUnion() ||
                (field->isBitField() && ((field->getIdentifier() && field->getBitWidthValue() == 0) ||
                                         !type->isIntegerType())) ||
                !hasReplayBuiltinOrArray(type))
                return false;
        }
        // EN: T19-030. Alignment on a record with bit-fields enters too: the replay infers it and the caller
        //     compares the replayed layout.
        // ES: T19-030. La alineacion en un record con bit-fields tambien entra: el replay la infiere y quien llama
        //     compara el layout reproducido.
        return true;
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
        // T19-003: a macro expansion session reports only its macros. / Solo sus macros.
        if (options_.macroExpansionOnly) return std::vector<CppTypeAliasDeclaration>{};
        auto& context = ast_->getASTContext();
        std::vector<CppTypeAliasDeclaration> result;
        std::set<const clang::TypedefNameDecl*> seen;
        collectTypeAliases(*context.getTranslationUnitDecl(), context, seen, result);
        addNamespaceAliasTypeNames(context, result);
        // T19-003: a C macro that names a type is an alias with its name. / Una macro C que nombra un tipo es un alias
        //     con su nombre.
        for (auto& value : result)
            if (const auto found = macroOfType_.find(value.name); found != macroOfType_.end())
            {
                value.name = found->second;
                value.qualifiedName = found->second;
            }
        return result;
    }
    std::optional<std::vector<CppEnumDeclaration>> enumsSnapshot() const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_) return std::nullopt;
        // T19-003: a macro expansion session reports only its macros. / Solo sus macros.
        if (options_.macroExpansionOnly) return std::vector<CppEnumDeclaration>{};
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
        // T19-006: a default-argument adapter has its number of written arguments, and a free function has no
        // record. / Un adaptador de predeterminados tiene su numero de argumentos; una funcion libre, sin record.
        struct Request { std::string symbol; const clang::CXXRecordDecl* record; const clang::CXXMethodDecl* method;
                         bool freeOperator = false; const clang::FunctionDecl* function = nullptr;
                         std::optional<std::size_t> defaultArity{}; };
        const auto defaultArities = [](const clang::FunctionDecl& function) {
            std::vector<std::size_t> arities;
            for (unsigned index = 0; index < function.getNumParams(); ++index)
                if (function.getParamDecl(index)->hasDefaultArg()) arities.push_back(index);
            return arities;
        };
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
                                // T19-006: the record's own methods with default arguments. / Los propios.
                                if (current == record)
                                    for (const auto arity : defaultArities(*method))
                                    {
                                        auto adapter = cppDefaultArgumentThunkSymbol(*recordUsr, *usr, arity);
                                        if (wanted.contains(adapter) && found.insert(adapter).second)
                                            requests.push_back({std::move(adapter), record, method, false, nullptr,
                                                                arity});
                                    }
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
                // T19-006: a free function with default arguments. / Una funcion libre con predeterminados.
                else if (const auto* function = llvm::dyn_cast<clang::FunctionDecl>(declaration);
                         function && !llvm::isa<clang::CXXMethodDecl>(function) && !function->isDependentContext())
                {
                    const auto usr = declarationUsr(function);
                    if (usr)
                        for (const auto arity : defaultArities(*function))
                        {
                            auto adapter = cppDefaultArgumentThunkSymbol("#function", *usr, arity);
                            if (wanted.contains(adapter) && found.insert(adapter).second)
                                requests.push_back({std::move(adapter), nullptr, nullptr, false, function, arity});
                        }
                }
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
            // EN: T19-006. The call names the function or method with only the written arguments, so Clang's overload
            //     resolution and default arguments apply as in a C++ call. A free function or static method adapter
            //     has the callee's own parameters and result; an instance method adapter has the member adapter's
            //     physical ABI (self first, a class result in memory after it, a reference as its address).
            // ES: T19-006. La llamada nombra la funcion o el metodo solo con los argumentos escritos, asi que la
            //     resolucion de sobrecarga y los predeterminados de Clang se aplican como en una llamada C++. El
            //     adaptador de una funcion libre o un metodo estatico tiene los parametros y el resultado del
            //     llamado; el de un metodo de instancia, el ABI fisico del adaptador de miembro.
            if (request.defaultArity)
            {
                const clang::FunctionDecl* callee = method ? static_cast<const clang::FunctionDecl*>(method)
                                                           : request.function;
                if (!callee || *request.defaultArity > callee->getNumParams()) continue;
                std::string aliases;
                bool spellable = true;
                const auto alias = [&](std::string suffix, clang::QualType type) {
                    const auto aliasName = name + "_" + suffix;
                    const auto spelling = type.getCanonicalType().getAsString(policy);
                    if (spelling.find("(anonymous") != std::string::npos ||
                        spelling.find("(unnamed") != std::string::npos || spelling.find("(lambda") != std::string::npos)
                        spellable = false;
                    aliases += "using " + aliasName + " = " + spelling + ";\n";
                    return aliasName;
                };
                const auto* constructor = method ? llvm::dyn_cast<clang::CXXConstructorDecl>(method) : nullptr;
                const bool instance = method && !method->isStatic() && !constructor;
                const auto result = callee->getReturnType();
                const bool classResult = instance && result->isRecordType();
                const bool referenceResult = instance && result->isLValueReferenceType();
                std::string parameters;
                std::string arguments;
                // EN: T19-006. A constructor adapter constructs in the storage at self through a placement new of
                //     its own tag, which no header declares, and returns self as a Microsoft constructor does.
                // ES: T19-006. Un adaptador de constructor construye en el almacenamiento de self con un placement
                //     new de su propia etiqueta, que ningun encabezado declara, y devuelve self como un constructor
                //     de Microsoft.
                std::string selfType;
                if (constructor)
                {
                    selfType = alias("s", context.getCanonicalTagType(request.record));
                    parameters = selfType + "* self";
                }
                if (instance)
                {
                    parameters = std::string(method->isConst() ? "const " : "") +
                        alias("s", context.getCanonicalTagType(request.record)) + "* self";
                    if (classResult) parameters += ", " + alias("r", result) + "* out";
                }
                for (std::size_t index = 0; index < *request.defaultArity; ++index)
                {
                    const auto type = callee->getParamDecl(static_cast<unsigned>(index))->getType();
                    const auto parameterAlias = alias("p" + std::to_string(index), type);
                    if (!parameters.empty()) parameters += ", ";
                    if (index) arguments += ", ";
                    parameters += parameterAlias + " a" + std::to_string(index);
                    arguments += type->isRValueReferenceType()
                        ? "static_cast<" + parameterAlias + ">(a" + std::to_string(index) + ")"
                        : "a" + std::to_string(index);
                }
                std::string call;
                if (constructor) call = "::new (static_cast<void*>(self), __nsc_cpp_place_t{}) " + selfType + "(" +
                    arguments + ")";
                else if (instance) call = "self->" + method->getNameAsString() + "(" + arguments + ")";
                else if (method)
                    call = alias("o", context.getCanonicalTagType(method->getParent())) + "::" +
                        method->getNameAsString() + "(" + arguments + ")";
                else call = "::" + callee->getQualifiedNameAsString() + "(" + arguments + ")";
                std::string resultType = "void";
                std::string body = call + ";";
                if (constructor)
                {
                    resultType = selfType + "*";
                    body = "return " + call + ";";
                    aliases = "#ifndef NSC_CPP_PLACE_TAG\n#define NSC_CPP_PLACE_TAG\nstruct __nsc_cpp_place_t {};\n"
                        "inline void* operator new(decltype(sizeof 0), void* p, __nsc_cpp_place_t) noexcept "
                        "{ return p; }\ninline void operator delete(void*, void*, __nsc_cpp_place_t) noexcept {}\n"
                        "#endif\n" + aliases;
                }
                else if (classResult) body = "auto&& r = " + call + "; __builtin_memcpy(out, &r, sizeof(r));";
                else if (referenceResult)
                {
                    resultType = alias("q", result.getNonReferenceType()) + "*";
                    body = "return &" + call + ";";
                }
                else if (!result->isVoidType())
                {
                    resultType = alias("r", result);
                    body = "return " + call + ";";
                }
                if (!spellable || !written.insert(name).second) continue;
                snippets.emplace_back(requestIndex, aliases + "extern \"C\" inline " + resultType + " " + name + "(" +
                    parameters + ") { " + body + " }\n");
                continue;
            }
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
                // T19-006: the placement new of the constructor adapters. / El placement new de los adaptadores.
                if (const auto* place = llvm::dyn_cast<clang::FunctionDecl>(declaration);
                    place && place->isOverloadedOperator() && place->hasBody() && place->getNumParams() == 3 &&
                    place->getParamDecl(2)->getType().getAsString().find("__nsc_cpp_place_t") != std::string::npos &&
                    !generator->HandleTopLevelDecl(clang::DeclGroupRef(const_cast<clang::FunctionDecl*>(place))))
                    return false;
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
        // T19-003: a macro expansion session reports only its macros. / Solo sus macros.
        if (!ast_ || !ast_->getASTContext().getLangOpts().CPlusPlus || options_.macroExpansionOnly) return result;
        std::set<std::pair<std::string, bool>> seen;
        visitNamespaceTemplates([&](const clang::TemplateDecl& declaration, bool function) {
            for (auto& name : allTemplateNames(declaration))
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
        auto result = instantiateTemplateLocked(qualifiedName, function, arguments);
        // T19-003: repeated after a reparse that adds a C++ macro wrapper. / Se repite tras un nuevo analisis que
        //     agrega un envoltorio de macro C++.
        if (result && result->instantiated)
            templateUses_.push_back({std::string(qualifiedName), function, {arguments.begin(), arguments.end()}});
        return result;
    }

    // T13-010: the instantiation; the caller holds the mutex. / La instanciacion; quien llama tiene el mutex.
    std::optional<CppTemplateInstantiation> instantiateTemplateLocked(
        std::string_view qualifiedName, bool function, std::span<const CppTemplateArgument> arguments) const
    {
        if (!ast_ || !ast_->hasSema() || !ast_->getASTContext().getLangOpts().CPlusPlus) return std::nullopt;
        auto& context = ast_->getASTContext();
        auto& sema = ast_->getSema();
        std::vector<clang::TemplateDecl*> templates;
        std::set<const clang::Decl*> seenTemplates;
        visitNamespaceTemplates([&](const clang::TemplateDecl& declaration, bool isFunction) {
            if (isFunction != function) return;
            const auto names = allTemplateNames(declaration);
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
                // T19-005: also an alias template. / Tambien una plantilla alias.
                clang::TemplateDecl* argumentTemplate = nullptr;
                visitNamespaceTemplates([&](const clang::TemplateDecl& declaration, bool isFunction) {
                    const auto names = allTemplateNames(declaration);
                    if (!isFunction && !argumentTemplate &&
                        std::find(names.begin(), names.end(), argument.templateName) != names.end())
                        argumentTemplate = const_cast<clang::TemplateDecl*>(&declaration);
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
                const bool negative = argument.integral < 0 && !argument.integralUnsigned;
                const auto magnitude = negative ? std::uint64_t(0) - static_cast<std::uint64_t>(argument.integral)
                                                : static_cast<std::uint64_t>(argument.integral);
                const bool unsignedMagnitude = magnitude > static_cast<std::uint64_t>(INT64_MAX);
                value = clang::IntegerLiteral::Create(context, llvm::APInt(64, magnitude),
                    unsignedMagnitude ? context.UnsignedLongLongTy : context.LongLongTy, location);
                if (negative)
                    value = clang::UnaryOperator::Create(context, value, clang::UO_Minus, value->getType(),
                        clang::VK_PRValue, clang::OK_Ordinary, location, false, clang::FPOptionsOverride());
                key += negative ? std::to_string(argument.integral) : std::to_string(magnitude);
            }
            explicitArguments.addArgument(clang::TemplateArgumentLoc(clang::TemplateArgument(value, false), value));
        }
        key += '>';

        if (!function)
        {
            // EN: T19-005. An alias template is checked as C++ checks `A<args>`: Clang substitutes the arguments
            //     into the aliased type.
            // ES: T19-005. Una plantilla alias se comprueba como C++ comprueba `A<args>`: Clang sustituye los
            //     argumentos en el tipo al que da nombre.
            auto* classTemplate = templates.front();
            const bool aliasTemplate = llvm::isa<clang::TypeAliasTemplateDecl>(classTemplate);
            const clang::QualType type = sema.CheckTemplateIdType(clang::ElaboratedTypeKeyword::None,
                clang::TemplateName(classTemplate), location, explicitArguments, nullptr, false);
            if (collectDiagnostics() || type.isNull() || type->isDependentType())
            {
                if (result.diagnostics.empty()) result.diagnostics.push_back("Clang rejected the template arguments");
                return result;
            }
            if (aliasTemplate) return describeAliasTarget(context, sema, type, location, result);
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
            // EN: T19-005. Named through `using` or a namespace alias, the use is written with the namespaces of the
            //     specialization, where NativeSharp imports it.
            // ES: T19-005. Nombrada por `using` o un alias de namespace, el uso se escribe con los namespaces de la
            //     especializacion, donde NativeSharp la importa.
            const auto own = templateNames(*classTemplate);
            if (std::find(own.begin(), own.end(), qualifiedName) == own.end())
            {
                CppTemplateAliasTarget target;
                for (const clang::DeclContext* parent = specialization->getDeclContext();
                     parent && !parent->isTranslationUnit(); parent = parent->getParent())
                    if (const auto* ns = llvm::dyn_cast<clang::NamespaceDecl>(parent))
                        target.path.insert(target.path.begin(), ns->getNameAsString());
                target.path.push_back(*name);
                result.aliasTarget = std::move(target);
            }
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
        // T19-005: the name does not depend on the name the program wrote (`T::f`, `f` through `using`, `U::f`).
        // T19-005: el nombre no depende del que escribio el programa.
        const auto own = templateNames(*templates.front());
        result.name = cppFunctionSpecializationName(templates.front()->getNameAsString(),
            own.front() + key.substr(qualifiedName.size()));
        result.clangUsr = declarationUsr(found.front());
        // T19-005: named through `using`, as for a class template. / Nombrada por `using`, como una de clase.
        if (std::find(own.begin(), own.end(), qualifiedName) == own.end())
        {
            CppTemplateAliasTarget target;
            for (const clang::DeclContext* parent = found.front()->getDeclContext();
                 parent && !parent->isTranslationUnit(); parent = parent->getParent())
                if (const auto* ns = llvm::dyn_cast<clang::NamespaceDecl>(parent))
                    target.path.insert(target.path.begin(), ns->getNameAsString());
            target.path.push_back(result.name);
            result.aliasTarget = std::move(target);
        }
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
    std::optional<CppTemplateInstantiation> deduceFunctionMacroCall(
        std::string_view name, std::span<const CppTemplateArgument> argumentTypes,
        const CppMacroCallSite& site) const override
    {
        {
            std::lock_guard lock(mutex_);
            if (ast_)
                if (auto expanded = expandCFunctionMacroCall(name, argumentTypes, &site)) return expanded;
        }
        return deduceFunctionTemplateCall(name, argumentTypes);
    }
    std::optional<CppTemplateInstantiation> deduceFunctionTemplateCall(
        std::string_view qualifiedName, std::span<const CppTemplateArgument> argumentTypes) const override
    {
        std::lock_guard lock(mutex_);
        // T19-003: a call to a function-like macro. / Una llamada a una macro de funcion.
        if (ast_)
            if (auto expanded = expandCFunctionMacroCall(qualifiedName, argumentTypes)) return expanded;
        if (ast_ && !ast_->getASTContext().getLangOpts().CPlusPlus) return std::nullopt;
        if (!ast_ || !ast_->hasSema() || !ast_->getASTContext().getLangOpts().CPlusPlus) return std::nullopt;
        auto& context = ast_->getASTContext();
        auto& sema = ast_->getSema();
        std::vector<clang::FunctionTemplateDecl*> templates;
        std::set<const clang::Decl*> seenTemplates;
        visitNamespaceTemplates([&](const clang::TemplateDecl& declaration, bool isFunction) {
            if (!isFunction) return;
            const auto names = allTemplateNames(declaration);
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
        const std::vector<std::string>& symbols, std::vector<std::string>& emitted,
        CppDebugInformation debugInformation) const override
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
        // EN: T17-008. With -g the inline definitions carry CodeView, as Clang emits them with /Z7, so the
        //     debugger steps into the C++ code NSC emits from the header.
        // ES: T17-008. Con -g las definiciones inline llevan CodeView, como las emite Clang con /Z7, asi que el
        //     depurador entra en el codigo C++ que NSC emite desde el encabezado.
        clang::CodeGenOptions codeGenOptions = ast_->getCodeGenOpts();
        if (debugInformation != CppDebugInformation::None)
        {
            codeGenOptions.setDebugInfo(debugInformation == CppDebugInformation::Full
                ? llvm::codegenoptions::FullDebugInfo : llvm::codegenoptions::DebugLineTablesOnly);
            codeGenOptions.EmitCodeView = true;
            codeGenOptions.DebugColumnInfo = true;
        }
        auto generator = clang::CreateLLVMCodeGen(ast_->getDiagnostics(), "nsc.cpp.inline",
            ast_->getVirtualFileSystemPtr(), ast_->getHeaderSearchOpts(), ast_->getPreprocessorOpts(),
            codeGenOptions, llvmContext);
        if (!generator) return std::nullopt;
        generator->Initialize(context);
        std::vector<std::pair<clang::GlobalDecl, std::string>> selected;
        std::set<std::string> seen;
        std::map<std::string, llvm::APSInt> inClassConstants;
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
                // EN: Item 9.5. A friend function defined in its class is an inline function of the enclosing
                //     namespace; it is emitted like any other inline definition.
                // ES: Item 9.5. Una funcion friend definida en su clase es una funcion inline del namespace que la
                //     rodea; se emite como cualquier otra definicion inline.
                if (const auto* friendDeclaration = llvm::dyn_cast<clang::FriendDecl>(declaration))
                    if (const auto* function = llvm::dyn_cast_or_null<clang::FunctionDecl>(
                            friendDeclaration->getFriendDecl());
                        function && function->doesThisDeclarationHaveABody() && !function->isDependentContext() &&
                        function->isInlined() && function->isExternallyVisible())
                    {
                        offer(function);
                        consider(clang::GlobalDecl(function));
                    }
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
                    // Item 15.6: an internal definition (`static inline`) is emitted too. / Tambien una definicion
                    //     interna.
                    const bool internalDefinition = definition == function && definition->hasBody() &&
                        !definition->isExternallyVisible() && !ctor && !dtor && !definition->isDependentContext() &&
                        definition->getTemplatedKind() == clang::FunctionDecl::TK_NonTemplate;
                    if (internalDefinition) consider(clang::GlobalDecl(definition));
                    else if (definition == function && definition->isInlined() &&
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
                    // EN: Item 9.7. A `static const` integral member initialized in its class
                    //     (`static const int kSize = 40;`) has no definition unless some translation unit writes
                    //     one; NSC defines it as weak_odr with the constant Clang evaluates.
                    // ES: Item 9.7. Un miembro `static const` entero inicializado en su clase no tiene definicion si
                    //     ninguna unidad la escribe; NSC lo define como weak_odr con la constante que evalua Clang.
                    else if (variable->isStaticDataMember() && !variable->isInline() &&
                             !variable->getType()->isDependentType() &&
                             variable->getType().isConstQualified() &&
                             variable->getType()->isIntegralOrEnumerationType() && variable->getInit() &&
                             variable->isExternallyVisible() && !variable->hasAttr<clang::DLLImportAttr>() &&
                             !variable->getDefinition())
                    {
                        if (const auto* evaluated = variable->evaluateValue(); evaluated && evaluated->isInt())
                        {
                            const auto before = selected.size();
                            consider(clang::GlobalDecl(variable));
                            if (selected.size() != before)
                                inClassConstants.emplace(selected.back().second, evaluated->getInt());
                        }
                    }
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
        // Item 9.7: the definitions of in-class `static const` members. / Las definiciones de miembros en clase.
        for (const auto& [name, constant] : inClassConstants)
            if (auto* variable = module->getGlobalVariable(name, true); variable && variable->isDeclaration())
            {
                auto* type = llvm::dyn_cast<llvm::IntegerType>(variable->getValueType());
                if (!type) continue;
                variable->setInitializer(llvm::ConstantInt::get(type, constant.extOrTrunc(type->getBitWidth())));
                variable->setConstant(true);
                variable->setLinkage(llvm::GlobalValue::WeakODRLinkage);
                variable->setComdat(module->getOrInsertComdat(name));
            }
        for (const auto& [declaration, name] : selected)
        {
            auto* value = module->getNamedValue(name);
            auto* global = value ? llvm::dyn_cast<llvm::GlobalObject>(value) : nullptr;
            if (!global || global->isDeclaration() || global->hasAvailableExternallyLinkage()) continue;
            if (global->hasLinkOnceODRLinkage()) global->setLinkage(llvm::GlobalValue::WeakODRLinkage);
            // Item 15.6: an internal definition becomes a weak_odr one the program can link. / Una definicion interna
            //     pasa a weak_odr para que el programa la enlace.
            if (global->hasLocalLinkage())
            {
                global->setLinkage(llvm::GlobalValue::WeakODRLinkage);
                global->setVisibility(llvm::GlobalValue::DefaultVisibility);
                global->setComdat(module->getOrInsertComdat(name));
            }
            emitted.push_back(name);
        }
        if (emitted.empty() || llvm::verifyModule(*module, &llvm::errs())) { emitted.clear(); return std::nullopt; }
        std::string ir;
        llvm::raw_string_ostream stream(ir);
        module->print(stream, nullptr);
        return ir;
    }
    std::optional<std::string> emitCTranslationUnitIR() const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_ || ast_->getASTContext().getLangOpts().CPlusPlus) return std::nullopt;
        auto& context = ast_->getASTContext();
        llvm::LLVMContext llvmContext;
        auto generator = clang::CreateLLVMCodeGen(ast_->getDiagnostics(), "nsc.c.implementation",
            ast_->getVirtualFileSystemPtr(), ast_->getHeaderSearchOpts(), ast_->getPreprocessorOpts(),
            ast_->getCodeGenOpts(), llvmContext);
        if (!generator) return std::nullopt;
        generator->Initialize(context);
        const auto& sources = ast_->getSourceManager();
        for (auto* declaration : context.getTranslationUnitDecl()->decls())
        {
            // Only the library's own definitions: system headers and NSC's synthetic helpers stay out.
            // Solo las definiciones de la biblioteca: los encabezados de sistema y los auxiliares de NSC quedan fuera.
            if (sources.isInSystemHeader(declaration->getLocation())) continue;
            if (const auto* named = llvm::dyn_cast<clang::NamedDecl>(declaration);
                named && named->getIdentifier() && named->getName().starts_with("__nsc_")) continue;
            if (!generator->HandleTopLevelDecl(clang::DeclGroupRef(declaration))) return std::nullopt;
        }
        generator->HandleTranslationUnit(context);
        auto module = generator->ReleaseModule();
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
                    (candidate->getStorageClass() != clang::SC_Static &&
                     !(candidate->getDefinition() && candidate->getDefinition()->isInlineSpecified()))) continue;
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
                // Item 6.2: a C99 inline definition of the header too. / Tambien una definicion inline C99.
                if ((!function->isExternallyVisible() || function->isInlineSpecified()) &&
                    function->doesThisDeclarationHaveABody() &&
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
            // Item 6.2: a C99 inline body comes in a COMDAT named after the original symbol. / Un cuerpo inline C99
            //     viene en un COMDAT con el nombre original.
            function->setComdat(nullptr);
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
        if (context.getLangOpts().CPlusPlus) addAlternateGlobalNames(context, result);
        const auto& macros = macroConstantsLocked();
        result.insert(result.end(), macros.constants.begin(), macros.constants.end());
        // T19-003: the value of a C macro is imported under its name. / El valor de una macro C se importa con su
        //     nombre.
        for (auto& value : result)
            if (const auto found = macroOfStorage_.find(value.name); found != macroOfStorage_.end())
            {
                value.cMacroStorage = value.name;
                value.name = found->second;
                value.qualifiedName = found->second;
            }
        // EN: T19-041. A macro that is not a constant is a C global of internal linkage with the type the accessor
        //     points to, read-only when the expansion is not an lvalue.
        // ES: T19-041. Una macro no constante es una global C de enlace interno con el tipo al que apunta la
        //     accesora, de solo lectura si la expansion no es un lvalue.
        for (const auto* declaration : context.getTranslationUnitDecl()->decls())
        {
            const auto* function = llvm::dyn_cast<clang::FunctionDecl>(declaration);
            if (!function || !function->getIdentifier()) continue;
            const auto found = macroOfAccessor_.find(function->getName().str());
            if (found == macroOfAccessor_.end()) continue;
            CppGlobalDeclaration value;
            value.name = found->second.first;
            value.qualifiedName = value.name;
            value.type = describeType(function->getReturnType()->getPointeeType(), context);
            if (!found->second.second) value.type.isConst = true;
            value.externalSymbol = found->first;
            // T19-041: not `macro`, which marks a constant without storage. / No `macro`, que marca una constante
            //     sin almacenamiento.
            value.cMacroAccessor = found->first;
            value.clangUsr = declarationUsr(function);
            result.push_back(std::move(value));
        }
        // EN: T19-003/C2-08. A macro expansion session reports only the constants of its macros: values with storage
        //     or an accessor already travel as C globals in the producer's catalogue.
        // ES: T19-003/C2-08. Una sesion de expansion de macros informa solo las constantes de sus macros: los valores
        //     con almacenamiento o accesora ya viajan como globales C en el catalogo del productor.
        if (options_.macroExpansionOnly)
            std::erase_if(result, [](const CppGlobalDeclaration& value)
                { return !value.macro || !value.cMacroStorage.empty() || !value.cMacroAccessor.empty(); });
        return result;
    }
    std::optional<std::pair<std::uint64_t, bool>> evaluateIntegerConstantExpression(
        std::string_view expression) const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_) return std::nullopt;
        // C2-04: a static initializer in C must be a constant expression, so Clang rejects anything else. / Un
        //     inicializador static en C debe ser una expresion constante, asi que Clang rechaza lo demas.
        const std::string code = source_ + "\nstatic const __auto_type __nsc_integer_constant_probe = (" +
            std::string(expression) + ");\n";
        llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> filesystem(llvm::vfs::createPhysicalFileSystem().release());
        if (filesystem->setCurrentWorkingDirectory(utf8(options_.workingDirectory))) return std::nullopt;
        std::vector<CppSessionDiagnostic> diagnostics;
        detail::DiagnosticCollector collector(diagnostics);
        auto arguments = detail::buildClangInvocationArguments(options_);
        const bool cpp = ast_->getASTContext().getLangOpts().CPlusPlus;
        std::string adjusted = code;
        if (cpp)
            adjusted = source_ + "\nconstexpr auto __nsc_integer_constant_probe = (" + std::string(expression) + ");\n";
        auto unit = detail::runClangSessionAction(adjusted, arguments, inputPath_, utf8(options_.driverReferencePath),
                                                  &collector, std::move(filesystem), CppCancellationToken{});
        if (!unit || unit->getDiagnostics().hasErrorOccurred()) return std::nullopt;
        auto& context = unit->getASTContext();
        for (const auto* declaration : context.getTranslationUnitDecl()->decls())
        {
            const auto* variable = llvm::dyn_cast<clang::VarDecl>(declaration);
            if (!variable || !variable->getIdentifier() ||
                variable->getName() != "__nsc_integer_constant_probe" || !variable->getType()->isIntegerType())
                continue;
            const auto value = evaluateScalarConstant(*variable, context);
            if (!value || value->kind != CppConstantValue::Kind::Integer) return std::nullopt;
            return std::make_pair(value->integerBits, variable->getType()->isSignedIntegerType());
        }
        return std::nullopt;
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
        // T19-003: a macro expansion session reports only its macros. / Solo sus macros.
        if (options_.macroExpansionOnly) return std::vector<CppNamespaceDeclaration>{};
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
    // T19-003: `-E -dD` of the analyzed source. / `-E -dD` de la fuente analizada.
    std::optional<std::string> preprocessedSourceWithMacros() const override
    {
        std::lock_guard lock(mutex_);
        if (!ast_) return std::nullopt;
        llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> filesystem(llvm::vfs::createPhysicalFileSystem().release());
        if (filesystem->setCurrentWorkingDirectory(utf8(options_.workingDirectory))) return std::nullopt;
        return detail::preprocessClangSessionSource(originalSource_, detail::buildClangInvocationArguments(options_),
            inputPath_, utf8(options_.driverReferencePath), std::move(filesystem));
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
        originalSource_ = source;
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
        // T19-001: the carriers of the `_Complex` types of the unit. / Los portadores de los `_Complex` de la unidad.
        synthesizeComplexCarriers(ast_->getASTContext());
        // EN: T19-003. The macros are evaluated before any snapshot or template request, because the values and
        //     types of some are declared in the translation unit, which a reparse then replaces.
        // ES: T19-003. Las macros se evaluan antes de cualquier captura o pedido de plantilla, porque los valores y
        //     tipos de algunas se declaran en la unidad, que un nuevo analisis reemplaza entonces.
        macroConstantsLocked();
        // T19-039: the C operators of the `_Complex` carriers. / Los operadores C de los portadores `_Complex`.
        declareCComplexOperators();
        state_ = CppSessionState::Ready;
        return {};
    }

private:
    // EN: T19-005. The type an alias template specialization names, as NativeSharp writes it. A record is completed
    //     as its first use in C++ would; a class template specialization among them is imported like one that
    //     NativeSharp names directly. A type with no NativeSharp spelling is rejected with the reason.
    // ES: T19-005. El tipo que nombra una especializacion de plantilla alias, como lo escribe NativeSharp. Un record se
    //     completa como lo haria su primer uso en C++; una especializacion de plantilla de clase entre ellos se importa
    //     como una que NativeSharp nombra directamente. Un tipo sin ortografia NativeSharp se rechaza con el motivo.
    CppTemplateInstantiation& describeAliasTarget(clang::ASTContext& context, clang::Sema& sema, clang::QualType type,
        clang::SourceLocation location, CppTemplateInstantiation& result) const
    {
        CppTemplateAliasTarget target;
        clang::QualType current = type.getCanonicalType();
        while (current->isPointerType() && target.pointerDepth < 16)
        {
            ++target.pointerDepth;
            current = current->getPointeeType().getCanonicalType();
        }
        const auto reject = [&](std::string why) -> CppTemplateInstantiation& {
            result.diagnostics.push_back("the alias template names '" +
                type.getCanonicalType().getAsString(context.getPrintingPolicy()) + "', " + std::move(why));
            return result;
        };
        if (current.hasQualifiers()) return reject("a qualified type that NativeSharp cannot name");
        if (const auto* builtin = current->getAs<clang::BuiltinType>())
        {
            switch (builtin->getKind())
            {
            case clang::BuiltinType::Bool: target.builtin = "bool"; break;
            case clang::BuiltinType::SChar: target.builtin = "signed char"; break;
            case clang::BuiltinType::UChar: target.builtin = "unsigned char"; break;
            case clang::BuiltinType::Short: target.builtin = "short"; break;
            case clang::BuiltinType::UShort: target.builtin = "unsigned short"; break;
            case clang::BuiltinType::Int: target.builtin = "int"; break;
            case clang::BuiltinType::UInt: target.builtin = "unsigned int"; break;
            case clang::BuiltinType::LongLong: target.builtin = "long long"; break;
            case clang::BuiltinType::ULongLong: target.builtin = "unsigned long long"; break;
            case clang::BuiltinType::Float: target.builtin = "float"; break;
            case clang::BuiltinType::Double: target.builtin = "double"; break;
            case clang::BuiltinType::Void:
                if (target.pointerDepth) { target.builtin = "void"; break; }
                [[fallthrough]];
            default: return reject("a builtin type that has no NativeSharp keyword");
            }
        }
        else if (const auto* tag = current->getAsTagDecl())
        {
            if (const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(tag); record && !target.pointerDepth &&
                sema.RequireCompleteType(location, current, clang::diag::err_incomplete_type))
                return reject("an incomplete record");
            std::vector<std::string> path;
            if (const auto* specialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(tag))
            {
                const auto name = cppSpecializationName(*specialization);
                if (!name) return reject("a specialization that cannot be imported as a NativeSharp type");
                path.push_back(*name);
                result.clangUsr = declarationUsr(specialization);
                instantiateNamedSpecializations(context, sema, current, location);
            }
            else
            {
                if (tag->getName().empty()) return reject("an unnamed type");
                path.push_back(tag->getNameAsString());
                result.clangUsr = declarationUsr(tag);
            }
            for (const clang::DeclContext* parent = tag->getDeclContext(); parent && !parent->isTranslationUnit();
                 parent = parent->getParent())
            {
                if (const auto* ns = llvm::dyn_cast<clang::NamespaceDecl>(parent))
                {
                    if (ns->isAnonymousNamespace()) return reject("a type of an anonymous namespace");
                    path.push_back(ns->getNameAsString());
                }
                else if (const auto* enclosing = llvm::dyn_cast<clang::RecordDecl>(parent))
                {
                    const auto name = cppEnclosingRecordName(*enclosing).value_or(enclosing->getNameAsString());
                    if (name.empty()) return reject("a type of an unnamed record");
                    path.push_back(name);
                }
                else if (!llvm::isa<clang::LinkageSpecDecl>(parent) && !llvm::isa<clang::ExportDecl>(parent))
                    return reject("a local type");
            }
            std::reverse(path.begin(), path.end());
            target.path = std::move(path);
        }
        else return reject("a type that NativeSharp cannot name through a C++ alias");
        result.instantiated = true;
        result.name = target.builtin.empty() ? target.path.back() : target.builtin;
        result.aliasTarget = std::move(target);
        result.nativeLayouts = nativeStructLayouts_;
        return result;
    }

    // EN: T19-005. Every name of a namespace-scope template: its own (templateNames) and those that a
    //     using-declaration (`using T::Box;`), a using-directive (`using namespace T;`) or a namespace alias
    //     (`namespace A = T;`) gives it, as C++ name lookup finds it through them.
    // ES: T19-005. Todos los nombres de una plantilla de un namespace: los suyos (templateNames) y los que le dan una
    //     declaracion using, una directiva using o un alias de namespace, como la encuentra la busqueda de C++.
    std::vector<std::string> allTemplateNames(const clang::TemplateDecl& declaration) const
    {
        auto names = templateNames(declaration);
        if (!templateAlternates_)
        {
            templateAlternates_.emplace();
            std::vector<std::pair<std::string, std::string>> prefixes;   // here prefix, nominated prefix
            std::vector<std::pair<const clang::Decl*, std::string>> shadows;
            std::function<void(const clang::DeclContext&)> visit = [&](const clang::DeclContext& scope) {
                for (const auto* item : scope.decls())
                {
                    if (const auto* nested = llvm::dyn_cast<clang::NamespaceDecl>(item))
                    {
                        if (!nested->isAnonymousNamespace()) visit(*nested);
                        continue;
                    }
                    if (llvm::isa<clang::LinkageSpecDecl>(item) || llvm::isa<clang::ExportDecl>(item))
                    {
                        visit(*llvm::cast<clang::DeclContext>(item));
                        continue;
                    }
                    const auto here = fullNamespacePath(item->getDeclContext());
                    if (!here) continue;
                    const auto join = [&](const std::string& name) {
                        return here->empty() ? name : *here + "::" + name;
                    };
                    if (const auto* shadow = llvm::dyn_cast<clang::UsingShadowDecl>(item))
                    {
                        if (const auto* target = llvm::dyn_cast<clang::TemplateDecl>(shadow->getTargetDecl()))
                            shadows.emplace_back(target->getCanonicalDecl(), join(target->getNameAsString()));
                    }
                    else if (const auto* directive = llvm::dyn_cast<clang::UsingDirectiveDecl>(item))
                    {
                        const auto there = directive->getNominatedNamespace()
                            ? fullNamespacePath(directive->getNominatedNamespace()) : std::nullopt;
                        if (there && !there->empty() && *there != *here)
                            prefixes.emplace_back(here->empty() ? "" : *here + "::", *there + "::");
                    }
                    else if (const auto* alias = llvm::dyn_cast<clang::NamespaceAliasDecl>(item))
                    {
                        const auto there = alias->getNamespace() ? fullNamespacePath(alias->getNamespace())
                                                                 : std::nullopt;
                        if (there && !there->empty()) prefixes.emplace_back(join(alias->getNameAsString()) + "::",
                                                                            *there + "::");
                    }
                }
            };
            visit(*ast_->getASTContext().getTranslationUnitDecl());
            for (const auto& [target, name] : shadows) templateAlternates_->emplace(target, name);
            if (!prefixes.empty())
                visitNamespaceTemplates([&](const clang::TemplateDecl& candidate, bool) {
                    for (const auto& own : templateNames(candidate))
                        for (const auto& [here, there] : prefixes)
                            if (own.starts_with(there))
                                templateAlternates_->emplace(candidate.getCanonicalDecl(),
                                                             here + own.substr(there.size()));
                });
        }
        const auto [first, last] = templateAlternates_->equal_range(declaration.getCanonicalDecl());
        for (auto alternate = first; alternate != last; ++alternate)
            if (std::find(names.begin(), names.end(), alternate->second) == names.end())
                names.push_back(alternate->second);
        return names;
    }

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
        // C2-02: argument-list macros and the spelling of each element. / Macros lista de argumentos y la escritura
        //     de cada elemento.
        std::vector<std::pair<std::string, std::vector<std::string>>> argumentLists;
        // C2-03: macros whose whole expansion is a brace initializer list. / Macros cuya expansion entera es una
        //     lista de inicializacion entre llaves.
        std::vector<std::string> braceInitializers;
        for (const auto& [identifier, state] : preprocessor.macros())
        {
            const auto* info = identifier ? preprocessor.getMacroInfo(identifier) : nullptr;
            if (!info || info->isBuiltinMacro() || info->getNumTokens() == 0) continue;
            const auto location = info->getDefinitionLoc();
            if (location.isInvalid() || sources.isInSystemHeader(location) || sources.isWrittenInBuiltinFile(location) ||
                sources.isWrittenInCommandLineFile(location) || sources.isWrittenInScratchSpace(location)) continue;
            const std::string name = identifier->getName().str();
            if (name.starts_with("__nsc_")) continue;
            if (info->isFunctionLike())
                macroConstants_->names.push_back({name, true, false, macroTokenParameters(*info)});
            // EN: C2-09. An object-like macro whose whole expansion names a function of the header
            //     (`#define compress LZ4_compress_default`) is an alias of it: a call through the macro is expanded
            //     like a function-like macro call, so C resolves the function.
            // ES: C2-09. Una macro de objeto cuya expansion entera nombra una funcion del encabezado es un alias de
            //     ella: una llamada mediante la macro se expande como una llamada a macro de funcion, asi C resuelve la
            //     funcion.
            else if (macroAliasesFunction(*info))
                macroConstants_->names.push_back({name, true, false, {}});
            else
            {
                // EN: Item 5.1. A macro that expands to a list of arguments (`"$clr.diffuse", 0, 0`) is not one
                //     value: imported as a value it was the last element of a comma expression. It stays out.
                // ES: Item 5.1. Una macro que se expande a una lista de argumentos no es un valor: importada como valor
                //     era el ultimo elemento de una expresion con comas. Queda fuera.
                int depth = 0;
                bool list = false;
                bool locationSensitive = false;
                // EN: Item 16.8. A macro that names __FILE__, __LINE__ or __func__ would take the location of NSC's
                //     synthetic unit, not the use site; it is not imported as a value.
                // ES: Item 16.8. Una macro que nombra __FILE__, __LINE__ o __func__ tomaria la ubicacion de la unidad
                //     sintetica de NSC, no la del sitio de uso; no se importa como valor.
                for (const auto& token : info->tokens())
                    if (const auto* identifier = token.getIdentifierInfo())
                        if (const auto spelled = identifier->getName(); spelled == "__FILE__" || spelled == "__LINE__" ||
                            spelled == "__func__" || spelled == "__FUNCTION__" || spelled == "__builtin_FILE" ||
                            spelled == "__builtin_LINE" || spelled == "__builtin_FUNCTION")
                            locationSensitive = true;
                std::vector<std::string> elements(1);
                for (const auto& token : info->tokens())
                {
                    if (token.isOneOf(clang::tok::l_paren, clang::tok::l_square, clang::tok::l_brace)) ++depth;
                    else if (token.isOneOf(clang::tok::r_paren, clang::tok::r_square, clang::tok::r_brace)) --depth;
                    if (token.is(clang::tok::comma) && depth == 0)
                    {
                        list = true;
                        elements.emplace_back();
                        continue;
                    }
                    if (!elements.back().empty()) elements.back() += ' ';
                    elements.back() += preprocessor.getSpelling(token);
                }
                // EN: C2-02. The elements of an argument-list macro are evaluated one by one, each as its own macro,
                //     so a call argument can stand for the whole list without taking only the last value.
                // ES: C2-02. Los elementos de una macro lista se evaluan uno por uno, cada uno como su propia macro,
                //     para que un argumento de llamada equivalga a la lista entera sin tomar solo el ultimo valor.
                if (list && !locationSensitive &&
                    std::none_of(elements.begin(), elements.end(), [](const std::string& item) { return item.empty(); }))
                    argumentLists.emplace_back(name, std::move(elements));
                const auto& tokens = info->tokens();
                const bool braced = !tokens.empty() && tokens.front().is(clang::tok::l_brace) &&
                    tokens.back().is(clang::tok::r_brace) && [&] {
                        int level = 0;
                        for (std::size_t index = 0; index < tokens.size(); ++index)
                        {
                            if (tokens[index].isOneOf(clang::tok::l_paren, clang::tok::l_square, clang::tok::l_brace))
                                ++level;
                            else if (tokens[index].isOneOf(clang::tok::r_paren, clang::tok::r_square,
                                                           clang::tok::r_brace))
                                --level;
                            if (level == 0 && index + 1 != tokens.size()) return false;
                        }
                        return true;
                    }();
                if (braced && !locationSensitive) braceInitializers.push_back(name);
                else if (!list && !locationSensitive) objectLike.push_back(name);
            }
        }
        // EN: C2-03. A brace initializer list has no type of its own: Clang evaluates it for each record type of the
        //     header as the compound literal `(T)M` (C) or `T M` (C++), and the pairs it accepts are imported as
        //     `<M>__nsc_init_<T>`. A NativeSharp declaration `T v = M;` uses the pair of its type.
        // ES: C2-03. Una lista entre llaves no tiene tipo propio: Clang la evalua para cada tipo record del
        //     encabezado como el literal compuesto `(T)M` (C) o `T M` (C++), y los pares que acepta se importan como
        //     `<M>__nsc_init_<T>`. Una declaracion NativeSharp `T v = M;` usa el par de su tipo.
        // EN: C3-03. An enumerator of an enum over a 128-bit integer whose value needs more than 64 bits cannot be an
        //     imported constant; in C, `#define E E` is a macro that expands to the enumerator itself, so its value
        //     reaches NativeSharp through the macro value accessor (T19-041) as a read-only 128-bit value.
        // ES: C3-03. Un enumerador de un enum sobre un entero de 128 bits cuyo valor necesita mas de 64 bits no puede
        //     ser una constante importada; en C, `#define E E` es una macro que se expande al propio enumerador, asi
        //     su valor llega a NativeSharp por la accesora de valores de macro (T19-041) como un valor de 128 bits.
        if (!ast_->getASTContext().getLangOpts().CPlusPlus)
        {
            std::string wideEnumerators;
            for (const auto* declaration : ast_->getASTContext().getTranslationUnitDecl()->decls())
                if (const auto* enumeration = llvm::dyn_cast<clang::EnumDecl>(declaration);
                    enumeration && enumeration->isComplete() && !sources.isInSystemHeader(enumeration->getLocation()) &&
                    ast_->getASTContext().getTypeSize(enumeration->getIntegerType()) == 128)
                    for (const auto* enumerator : enumeration->enumerators())
                    {
                        if (!preprocessor.isMacroDefined(enumerator->getName()))
                        {
                            const std::string name = enumerator->getName().str();
                            wideEnumerators += "#define " + name + " " + name + "\n";
                            objectLike.push_back(name);
                        }
                    }
            if (!wideEnumerators.empty()) source_ += "\n" + wideEnumerators;
        }
        std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> braceCandidates;
        if (!braceInitializers.empty())
        {
            const bool cppUnit = ast_->getASTContext().getLangOpts().CPlusPlus;
            std::vector<std::pair<std::string, std::string>> types;
            for (const auto* declaration : ast_->getASTContext().getTranslationUnitDecl()->decls())
            {
                if (declaration->isImplicit() || sources.isInSystemHeader(declaration->getLocation())) continue;
                if (const auto* alias = llvm::dyn_cast<clang::TypedefNameDecl>(declaration))
                {
                    if (alias->getUnderlyingType().getCanonicalType()->isRecordType())
                        types.emplace_back(alias->getName().str(), alias->getName().str());
                }
                else if (const auto* record = llvm::dyn_cast<clang::RecordDecl>(declaration))
                    if (record->isCompleteDefinition() && record->getIdentifier())
                        types.emplace_back(record->getName().str(),
                                           cppUnit ? record->getName().str()
                                                   : (record->isUnion() ? "union " : "struct ") + record->getName().str());
            }
            // One candidate per NativeSharp spelling; a typedef wins over its tag (`typedef struct P {...} P;`).
            //     / Un candidato por escritura NativeSharp; un typedef gana sobre su etiqueta.
            std::map<std::string, std::string> unique;
            for (const auto& [spelled, cType] : types)
                if (const auto [found, inserted] = unique.emplace(spelled, cType); !inserted && cType == spelled)
                    found->second = cType;
            types.assign(unique.begin(), unique.end());
            source_ += '\n';
            for (const auto& name : braceInitializers)
            {
                std::vector<std::pair<std::string, std::string>> elements;
                for (const auto& [spelled, cType] : types)
                {
                    const std::string element = name + "__nsc_init_" + spelled;
                    braceElements_[element] = {cType, name};
                    objectLike.push_back(element);
                    elements.emplace_back(spelled, element);
                }
                braceCandidates.emplace_back(name, std::move(elements));
            }
            // EN: C2-03. A pair Clang rejects with an error that does not invalidate the declaration (an integer to a
            //     pointer field, -Wint-conversion) would make the whole value unit fail: one probe with a line per
            //     pair drops every pair whose line reports an error.
            // ES: C2-03. Un par que Clang rechaza con un error que no invalida la declaracion haria fallar toda la
            //     unidad de valores: un sondeo con una linea por par descarta cada par cuya linea informa un error.
            // A `#line` names the probe lines, so a preprocessed source with its own line markers maps them too.
            //     / Un `#line` nombra las lineas del sondeo, asi tambien las mapea una fuente preprocesada.
            std::string probe = source_ + "\n#line 1 \"__nsc_brace_probe\"\n";
            const unsigned firstLine = 1U;
            std::vector<std::string> ordered;
            for (const auto& [element, value] : braceElements_)
            {
                probe += "static const " + value.first + " __nsc_brace_probe_" + element + " = " + value.second + ";\n";
                ordered.push_back(element);
            }
            llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> probeFilesystem(
                llvm::vfs::createPhysicalFileSystem().release());
            std::vector<CppSessionDiagnostic> probeDiagnostics;
            detail::DiagnosticCollector probeCollector(probeDiagnostics);
            auto probeArguments = detail::buildClangInvocationArguments(options_);
            probeArguments.push_back("-ferror-limit=0");
            std::set<std::string> rejected(ordered.begin(), ordered.end());
            if (!probeFilesystem->setCurrentWorkingDirectory(utf8(options_.workingDirectory)))
                if (detail::runClangSessionAction(probe, probeArguments, inputPath_,
                        utf8(options_.driverReferencePath), &probeCollector, std::move(probeFilesystem),
                        CppCancellationToken{}))
                {
                    rejected.clear();
                    for (const auto& diagnostic : probeDiagnostics)
                        if (diagnostic.severity == CppSessionDiagnosticSeverity::Error && diagnostic.location &&
                            diagnostic.location->file.filename() == "__nsc_brace_probe" &&
                            diagnostic.location->line >= firstLine &&
                            diagnostic.location->line < firstLine + ordered.size())
                            rejected.insert(ordered[diagnostic.location->line - firstLine]);
                }
            for (const auto& element : rejected)
            {
                braceElements_.erase(element);
                std::erase(objectLike, element);
            }
        }
        if (!argumentLists.empty())
        {
            source_ += '\n';
            for (const auto& [name, elements] : argumentLists)
                for (std::size_t index = 0; index < elements.size(); ++index)
                {
                    const std::string element = name + "__nsc_arg" + std::to_string(index);
                    source_ += "#define " + element + " " + elements[index] + '\n';
                    objectLike.push_back(element);
                }
        }
        std::sort(objectLike.begin(), objectLike.end());
        // EN: T19-003. A macro expansion session reports only the function-like macros; the values come from the
        //     producer's catalogue.
        // ES: T19-003. Una sesion de expansion de macros informa solo las macros de funcion; los valores vienen del
        //     catalogo del productor.
        // EN: C2-08. A macro expansion session (the consumer of a precompiled library, over the producer's macro
        //     source with the producer's options) evaluates the object-like macros too, so constants, values and
        //     argument lists reach the published templates; it reports only those (globalsSnapshot).
        // ES: C2-08. Una sesion de expansion de macros (el consumidor de una biblioteca precompilada, sobre la fuente
        //     de macros del productor con sus opciones) evalua tambien las macros de objeto, asi las constantes,
        //     valores y listas llegan a las plantillas publicadas; solo informa esas (globalsSnapshot).
        if (objectLike.empty()) return *macroConstants_;
        const bool cpp = ast_->getASTContext().getLangOpts().CPlusPlus;
        std::string code = source_ + "\n";
        for (const auto& name : objectLike)
        {
            // C2-03: a brace initializer takes the type it initializes. / Una lista entre llaves toma el tipo que
            //     inicializa.
            if (const auto brace = braceElements_.find(name); brace != braceElements_.end())
            {
                code += "static const " + brace->second.first + " __nsc_macro_" + name + " = " +
                    brace->second.second + ";\n";
                continue;
            }
            code += (cpp ? "constexpr auto __nsc_macro_" : "static const __auto_type __nsc_macro_") + name + " = (" +
                name + ");\n";
        }
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
        // T19-003: in C, the values that are not scalar constants and the types. / En C, los valores que no son
        //     constantes escalares y los tipos.
        if (unit) declareCMacroValuesAndTypes(*unit, objectLike, constants);
        for (const auto& name : objectLike)
            macroConstants_->names.push_back({name, false, constants.contains(name) ||
                macroDeclaredValueOrType(name)});
        for (const auto& [name, elements] : braceCandidates)
        {
            CppMacroName macro{name, false, false};
            for (const auto& [spelled, element] : elements)
                if (constants.contains(element) || macroDeclaredValueOrType(element))
                    macro.braceInitializerTypes.push_back(spelled);
            macro.constant = !macro.braceInitializerTypes.empty();
            macroConstants_->names.push_back(std::move(macro));
        }
        for (const auto& [name, elements] : argumentLists)
        {
            bool imported = true;
            for (std::size_t index = 0; index < elements.size(); ++index)
            {
                const std::string element = name + "__nsc_arg" + std::to_string(index);
                imported = imported && (constants.contains(element) || macroDeclaredValueOrType(element));
            }
            CppMacroName macro{name, false, imported};
            macro.argumentListCount = elements.size();
            macroConstants_->names.push_back(std::move(macro));
        }
        return *macroConstants_;
    }

    // EN: T19-039. In a C unit with `_Complex T` values, the arithmetic and comparison of C on each carrier
    //     `struct __complex_T` is written as static inline C functions that convert the carrier to `T _Complex`,
    //     apply the C operator and convert the result back, so Clang decides every result (including the calls to
    //     __muldc3 and __divdc3 that C requires). The carrier is declared in the same source with the fields that
    //     synthesizeComplexCarriers would give it, so the reparsed unit keeps the same C type. NativeSharp imports
    //     `__nsc_complex_op_<Operator>__...` as the extension operator `op_<Operator>` (CppSemanticFunction) for
    //     complex and complex, complex and real T, and real T and complex, and the unary minus and plus. If Clang
    //     rejects the code, the session stays as it was.
    // ES: T19-039. En una unidad C con valores `_Complex T`, la aritmetica y la comparacion de C sobre cada portador
    //     `struct __complex_T` se escriben como funciones C static inline que convierten el portador a `T _Complex`,
    //     aplican el operador C y convierten el resultado de vuelta, asi que Clang decide cada resultado (tambien las
    //     llamadas a __muldc3 y __divdc3 que exige C). El portador se declara en la misma fuente con los campos que le
    //     daria synthesizeComplexCarriers, asi que la unidad analizada otra vez conserva el mismo tipo C. NativeSharp
    //     importa `__nsc_complex_op_<Operador>__...` como el operador de extension `op_<Operador>`
    //     (CppSemanticFunction) para complejo y complejo, complejo y real T, y real T y complejo, y los unarios menos
    //     y mas. Si Clang rechaza el codigo, la sesion queda como estaba.
    void declareCComplexOperators()
    {
        // EN: Also in the macro session of a precompiled project, which reports these functions as it reports
        //     macro wrappers (cMacroWrapper).
        // ES: Tambien en la sesion de macros de un proyecto precompilado, que informa estas funciones como informa
        //     los envoltorios de macros (cMacroWrapper).
        if (!ast_ || ast_->getASTContext().getLangOpts().CPlusPlus) return;
        auto& context = ast_->getASTContext();
        std::string code;
        std::vector<std::pair<std::string, std::string>> carriers;
        for (const auto* declaration : context.getTranslationUnitDecl()->decls())
        {
            const auto* record = llvm::dyn_cast<clang::RecordDecl>(declaration);
            if (!record || !record->getIdentifier() || !record->getName().starts_with("__complex_") ||
                !record->isCompleteDefinition() || record->field_empty()) continue;
            const clang::QualType element = record->field_begin()->getType();
            if (complexCarrierRecord(context, element) != record || complexCarrierName(element) != record->getName())
                continue;
            const std::string name = record->getName().str();
            const std::string real = element.getUnqualifiedType().getAsString();
            const std::string carrier = "struct " + name;
            const std::string in = "__nsc_cx_in_" + name, out = "__nsc_cx_out_" + name;
            code += carrier + " { " + real + " real; " + real + " imag; };\n";
            code += "static inline " + real + " _Complex " + in + "(" + carrier + " v) { " + real +
                " _Complex r; __real__ r = v.real; __imag__ r = v.imag; return r; }\n";
            code += "static inline " + carrier + " " + out + "(" + real + " _Complex v) { " + carrier +
                " r; r.real = __real__ v; r.imag = __imag__ v; return r; }\n";
            const auto function = [&](const std::string& result, const std::string& op, const std::string& shape,
                                      const std::string& parameters, const std::string& body) {
                code += "static inline " + result + " __nsc_complex_op_" + op + "__" + name + "__" + shape + "(" +
                    parameters + ") { return " + body + "; }\n";
            };
            for (const auto& [op, token] : {std::pair{"Addition", "+"}, {"Subtraction", "-"}, {"Multiply", "*"},
                                            {"Division", "/"}})
            {
                function(carrier, op, "cc", carrier + " a, " + carrier + " b",
                         out + "(" + in + "(a) " + token + " " + in + "(b))");
                function(carrier, op, "cr", carrier + " a, " + real + " b", out + "(" + in + "(a) " + token + " b)");
                function(carrier, op, "rc", real + " a, " + carrier + " b", out + "(a " + token + " " + in + "(b))");
            }
            for (const auto& [op, token] : {std::pair{"Equality", "=="}, {"Inequality", "!="}})
            {
                function("_Bool", op, "cc", carrier + " a, " + carrier + " b", in + "(a) " + token + " " + in + "(b)");
                function("_Bool", op, "cr", carrier + " a, " + real + " b", in + "(a) " + token + " b");
                function("_Bool", op, "rc", real + " a, " + carrier + " b",
                         std::string("a ") + token + " " + in + "(b)");
            }
            // EN: T19-039. `a op= b` as C evaluates it: the target once, through its address.
            // ES: T19-039. `a op= b` como lo evalua C: el destino una vez, por su direccion.
            for (const char* op : {"Addition", "Subtraction", "Multiply", "Division"})
            {
                const std::string base = std::string("__nsc_complex_op_") + op + "__" + name;
                code += "static inline " + carrier + " __nsc_complex_as_" + op + "__" + name + "__cc(" + carrier +
                    "* a, " + carrier + " b) { *a = " + base + "__cc(*a, b); return *a; }\n";
                code += "static inline " + carrier + " __nsc_complex_as_" + op + "__" + name + "__cr(" + carrier +
                    "* a, " + real + " b) { *a = " + base + "__cr(*a, b); return *a; }\n";
            }
            function(carrier, "UnaryNegation", "c", carrier + " a", out + "(-" + in + "(a))");
            function(carrier, "UnaryPlus", "c", carrier + " a", out + "(+" + in + "(a))");
            // EN: T19-039. The C conversions to this carrier: from each standard real type and from each other
            //     carrier (`(T _Complex)v`). NativeSharp admits implicitly only those whose element conversion it
            //     admits implicitly between real types (CComplexConversion.h).
            // ES: T19-039. Las conversiones C a este portador: desde cada tipo real estandar y desde cada otro portador
            //     (`(T _Complex)v`). NativeSharp admite implicitamente solo aquellas cuya conversion de elemento admite
            //     implicitamente entre tipos reales (CComplexConversion.h).
            for (const char* source : {"char", "signed char", "unsigned char", "short", "unsigned short", "int",
                                       "unsigned int", "long", "unsigned long", "long long", "unsigned long long",
                                       "float", "double", "long double", "_Bool"})
            {
                std::string suffix = source;
                std::replace(suffix.begin(), suffix.end(), ' ', '_');
                code += "static inline " + carrier + " __nsc_complex_cv__" + name + "__" + suffix + "(" + source +
                    " v) { return " + out + "((" + real + " _Complex)v); }\n";
            }
            carriers.push_back({name, real});
        }
        // T19-039: between two carriers. / Entre dos portadores.
        for (const auto& [target, targetReal] : carriers)
            for (const auto& [source, sourceReal] : carriers)
                if (target != source)
                    code += "static inline struct " + target + " __nsc_complex_cv__" + target + "__" + source +
                        "(struct " + source + " v) { return __nsc_cx_out_" + target + "((" + targetReal +
                        " _Complex)__nsc_cx_in_" + source + "(v)); }\n";
        if (code.empty()) return;
        code = source_ + "\n" + code;
        llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> filesystem(llvm::vfs::createPhysicalFileSystem().release());
        if (filesystem->setCurrentWorkingDirectory(utf8(options_.workingDirectory))) return;
        macroWrapperDiagnostics_.clear();
        auto unit = detail::runClangSessionAction(code, detail::buildClangInvocationArguments(options_), inputPath_,
            utf8(options_.driverReferencePath), &macroWrapperConsumer_, std::move(filesystem), CppCancellationToken{});
        if (!unit || !unit->hasSema() || unit->getDiagnostics().hasErrorOccurred()) return;
        unit->getDiagnostics().setClient(&consumer_, false);
        ast_ = std::move(unit);
        source_ = code;
        synthesizeComplexCarriers(ast_->getASTContext());
    }

    // T19-003: whether a C macro has a declared value or type. / Si una macro C tiene un valor o tipo declarado.
    bool macroDeclaredValueOrType(std::string_view name) const
    {
        for (const auto& [storage, macro] : macroOfStorage_) if (macro == name) return true;
        for (const auto& [type, macro] : macroOfType_) if (macro == name) return true;
        for (const auto& [accessor, macro] : macroOfAccessor_) if (macro.first == name) return true;
        return false;
    }

    // EN: T19-003. A C or C++ object-like macro whose expansion is a constant initializer of pointer, record or
    //     complex type (a string decays to `char*`) gets a constant `static const __auto_type __nsc_macro_value_X =
    //     (X);` (in C++ `inline const auto`), and one whose expansion names a type a typedef
    //     `typedef X __nsc_macro_type_X;`.
    //     Clang checks each one in a separate analysis; the accepted ones are appended to the source and the
    //     reparsed translation unit replaces the session's. They are imported under the macro's name
    //     (cMacroStorage, and the alias name). Any other macro stays unimported, with its reason.
    // ES: T19-003. Una macro de objeto C o C++ cuya expansion es un inicializador constante de tipo puntero, record
    //     o complejo (una cadena decae a `char*`) recibe una constante (en C++ `inline const auto`), y una cuya
    //     expansion nombra un tipo,
    //     un typedef. Clang comprueba cada una en un analisis aparte; las aceptadas se anaden a la fuente y la
    //     unidad analizada otra vez reemplaza la de la sesion. Se importan con el nombre de la macro (cMacroStorage,
    //     y el nombre del alias). Cualquier otra macro sigue sin importarse, con su motivo.
    void declareCMacroValuesAndTypes(clang::ASTUnit& probe, const std::vector<std::string>& objectLike,
                                     const std::set<std::string>& constants) const
    {
        std::vector<std::string> values;
        auto& probeContext = probe.getASTContext();
        for (const auto* declaration : probeContext.getTranslationUnitDecl()->decls())
        {
            const auto* variable = llvm::dyn_cast<clang::VarDecl>(declaration);
            if (!variable || !variable->getIdentifier() || !variable->getName().starts_with("__nsc_macro_")) continue;
            const std::string name = variable->getName().drop_front(12).str();
            const auto* initializer = variable->getInit();
            const auto type = variable->getType();
            if (constants.contains(name) || variable->isInvalidDecl() || !initializer ||
                initializer->containsErrors() || type->isArrayType() || type->isFunctionType() ||
                !(type->isPointerType() || type->isRecordType() || type->isAnyComplexType()) ||
                !initializer->isConstantInitializer(probeContext, false)) continue;
            values.push_back(name);
        }
        std::vector<std::string> types;
        {
            std::string code = source_ + "\n";
            for (const auto& name : objectLike)
                if (!constants.contains(name) && std::find(values.begin(), values.end(), name) == values.end())
                    code += "typedef " + name + " __nsc_macro_type_" + name + ";\n";
            if (!code.empty())
            {
                llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> filesystem(
                    llvm::vfs::createPhysicalFileSystem().release());
                std::vector<CppSessionDiagnostic> typeDiagnostics;
                detail::DiagnosticCollector collector(typeDiagnostics);
                auto arguments = detail::buildClangInvocationArguments(options_);
                arguments.push_back("-ferror-limit=0");
                std::unique_ptr<clang::ASTUnit> unit;
                if (!filesystem->setCurrentWorkingDirectory(utf8(options_.workingDirectory)))
                    unit = detail::runClangSessionAction(code, arguments, inputPath_,
                        utf8(options_.driverReferencePath), &collector, std::move(filesystem), CppCancellationToken{});
                // EN: T19-003. Clang keeps a typedef valid after some errors on its line, such as the implicit `int`
                //     of `typedef __declspec(dllimport) T;`; one with an error on its appended line is not a type.
                // ES: T19-003. Clang conserva valido un typedef tras algunos errores en su linea, como el `int`
                //     implicito de `typedef __declspec(dllimport) T;`; uno con un error en su linea anadida no es un
                //     tipo.
                const auto sourceLines = static_cast<unsigned>(std::count(source_.begin(), source_.end(), '\n')) + 1;
                std::set<unsigned> errorLines;
                if (unit)
                {
                    const auto& typeSources = unit->getSourceManager();
                    const auto mainFile = typeSources.getFileEntryRefForID(typeSources.getMainFileID());
                    const auto mainName = mainFile ? std::filesystem::path(mainFile->getName().str()).filename()
                                                   : std::filesystem::path();
                    for (const auto& diagnostic : typeDiagnostics)
                        if ((diagnostic.severity == CppSessionDiagnosticSeverity::Error ||
                             diagnostic.severity == CppSessionDiagnosticSeverity::Fatal) && diagnostic.location &&
                            diagnostic.location->line > sourceLines && diagnostic.location->file.filename() == mainName)
                            errorLines.insert(diagnostic.location->line);
                }
                if (unit)
                    for (const auto* declaration : unit->getASTContext().getTranslationUnitDecl()->decls())
                    {
                        const auto* alias = llvm::dyn_cast<clang::TypedefDecl>(declaration);
                        if (!alias || !alias->getIdentifier() || !alias->getName().starts_with("__nsc_macro_type_") ||
                            alias->isInvalidDecl() || alias->getUnderlyingType()->containsErrors()) continue;
                        if (errorLines.contains(unit->getSourceManager().getPresumedLineNumber(alias->getLocation())))
                            continue;
                        types.push_back(alias->getName().drop_front(17).str());
                    }
            }
        }
        // EN: T19-041. In C, any other macro whose expansion is an lvalue gets an accessor that returns `&(M)`, and
        //     one whose expansion is a value an accessor that keeps it in a static variable and returns that
        //     address (NativeSharp reads it at once; it does not link the CRT's TLS): each access evaluates the
        //     expansion, as in C. Each form is checked in a separate analysis; a function designator stays
        //     unimported.
        // ES: T19-041. En C, cualquier otra macro cuya expansion es un lvalue recibe una accesora que devuelve
        //     `&(M)`, y una cuya expansion es un valor una accesora que lo guarda en una variable static y devuelve
        //     esa direccion (NativeSharp la lee enseguida; no enlaza el TLS del CRT): cada acceso evalua la
        //     expansion, como en C. Cada forma se comprueba en un analisis aparte; un designador de funcion sigue
        //     sin importarse.
        std::vector<std::pair<std::string, bool>> accessors;
        if (!probeContext.getLangOpts().CPlusPlus)
        {
            std::vector<std::string> pending;
            for (const auto& name : objectLike)
                if (!constants.contains(name) && std::find(values.begin(), values.end(), name) == values.end() &&
                    std::find(types.begin(), types.end(), name) == types.end())
                    pending.push_back(name);
            for (const bool lvalue : {true, false})
            {
                if (pending.empty()) break;
                std::string code = source_ + "\n";
                for (const auto& name : pending) code += macroAccessorSource(name, lvalue);
                const auto accepted = acceptedMacroAccessors(code, lvalue ? "__nsc_macro_ref_" : "__nsc_macro_get_");
                std::vector<std::string> rest;
                for (const auto& name : pending)
                    if (accepted.contains(name)) accessors.emplace_back(name, lvalue);
                    else rest.push_back(name);
                pending = std::move(rest);
            }
        }
        if (values.empty() && types.empty() && accessors.empty()) return;
        // EN: T19-003. In C++ the value is an inline variable of the global namespace, defined wherever it is used.
        // ES: T19-003. En C++ el valor es una variable inline del namespace global, definida donde se usa.
        const bool cpp = probeContext.getLangOpts().CPlusPlus;
        std::string code = source_ + "\n";
        for (const auto& name : values)
        {
            if (const auto brace = braceElements_.find(name); brace != braceElements_.end())
            {
                code += std::string(cpp ? "inline const " : "static const ") + brace->second.first +
                    " __nsc_macro_value_" + name + " = " + brace->second.second + ";\n";
                continue;
            }
            code += std::string(cpp ? "inline const auto" : "static const __auto_type") + " __nsc_macro_value_" +
                name + " = (" + name + ");\n";
        }
        for (const auto& name : types) code += "typedef " + name + " __nsc_macro_type_" + name + ";\n";
        for (const auto& [name, lvalue] : accessors) code += macroAccessorSource(name, lvalue);
        llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> filesystem(llvm::vfs::createPhysicalFileSystem().release());
        if (filesystem->setCurrentWorkingDirectory(utf8(options_.workingDirectory))) return;
        macroWrapperDiagnostics_.clear();
        auto unit = detail::runClangSessionAction(code, detail::buildClangInvocationArguments(options_), inputPath_,
            utf8(options_.driverReferencePath), &macroWrapperConsumer_, std::move(filesystem), CppCancellationToken{});
        if (!unit || !unit->hasSema() || unit->getDiagnostics().hasErrorOccurred()) return;
        unit->getDiagnostics().setClient(&consumer_, false);
        ast_ = std::move(unit);
        source_ = code;
        // EN: T19-003. In C++ the new translation unit instantiates again the specializations the header names;
        //     this runs while the session opens, before NativeSharp requests any template.
        // ES: T19-003. En C++ la nueva unidad instancia otra vez las especializaciones que nombra el encabezado;
        //     esto ocurre al abrir la sesion, antes de que NativeSharp pida una plantilla.
        if (cpp) instantiateNamedSpecializations(ast_->getASTContext(), ast_->getSema());
        synthesizeComplexCarriers(ast_->getASTContext());
        for (const auto& name : values) macroOfStorage_.emplace("__nsc_macro_value_" + name, name);
        for (const auto& name : types) macroOfType_.emplace("__nsc_macro_type_" + name, name);
        for (const auto& [name, lvalue] : accessors)
            macroOfAccessor_.emplace((lvalue ? "__nsc_macro_ref_" : "__nsc_macro_get_") + name,
                                     std::make_pair(name, lvalue));
    }

    // EN: T19-041. The C accessor of a macro that is not a constant, on one line.
    // ES: T19-041. La accesora C de una macro que no es constante, en una linea.
    static std::string macroAccessorSource(const std::string& name, bool lvalue)
    {
        if (lvalue)
            return "static inline __typeof__(" + name + ")* __nsc_macro_ref_" + name + "(void) { return &(" + name +
                "); }\n";
        const std::string type = "__typeof__((0, (" + name + ")))";
        // EN: C2-07. The value is written to storage the caller provides (its own stack slot), so concurrent reads
        //     from several threads never share a static variable; each read evaluates the expansion, as in C.
        // ES: C2-07. El valor se escribe en almacenamiento que aporta quien llama (su propio espacio de pila), asi
        //     las lecturas concurrentes de varios hilos nunca comparten una variable static; cada lectura evalua la
        //     expansion, como en C.
        return "static inline " + type + "* __nsc_macro_get_" + name + "(" + type + "* __nsc_out) { *__nsc_out = (" +
            name + "); return __nsc_out; }\n";
    }

    // EN: T19-041. The macros whose accessor (named `prefix` and the macro) Clang accepts in `code` without an error
    //     on its line, and that do not return the address of a function.
    // ES: T19-041. Las macros cuya accesora (con nombre `prefix` y la macro) Clang acepta en `code` sin un error en
    //     su linea, y que no devuelven la direccion de una funcion.
    std::set<std::string> acceptedMacroAccessors(const std::string& code, std::string_view prefix) const
    {
        std::set<std::string> accepted;
        llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> filesystem(llvm::vfs::createPhysicalFileSystem().release());
        if (filesystem->setCurrentWorkingDirectory(utf8(options_.workingDirectory))) return accepted;
        std::vector<CppSessionDiagnostic> probeDiagnostics;
        detail::DiagnosticCollector collector(probeDiagnostics);
        auto arguments = detail::buildClangInvocationArguments(options_);
        arguments.push_back("-ferror-limit=0");
        auto unit = detail::runClangSessionAction(code, arguments, inputPath_, utf8(options_.driverReferencePath),
                                                  &collector, std::move(filesystem), CppCancellationToken{});
        if (!unit) return accepted;
        const auto sourceLines = static_cast<unsigned>(std::count(source_.begin(), source_.end(), '\n')) + 1;
        const auto& sources = unit->getSourceManager();
        const auto mainFile = sources.getFileEntryRefForID(sources.getMainFileID());
        const auto mainName = mainFile ? std::filesystem::path(mainFile->getName().str()).filename()
                                       : std::filesystem::path();
        std::set<unsigned> errorLines;
        for (const auto& diagnostic : probeDiagnostics)
            if ((diagnostic.severity == CppSessionDiagnosticSeverity::Error ||
                 diagnostic.severity == CppSessionDiagnosticSeverity::Fatal) && diagnostic.location &&
                diagnostic.location->line > sourceLines && diagnostic.location->file.filename() == mainName)
                errorLines.insert(diagnostic.location->line);
        for (const auto* declaration : unit->getASTContext().getTranslationUnitDecl()->decls())
        {
            const auto* function = llvm::dyn_cast<clang::FunctionDecl>(declaration);
            if (!function || !function->getIdentifier() || !function->getName().starts_with(prefix) ||
                function->isInvalidDecl() || !function->hasBody() || function->getReturnType()->containsErrors() ||
                !function->getReturnType()->isPointerType() ||
                function->getReturnType()->getPointeeType()->isFunctionType() ||
                function->getReturnType()->getPointeeType()->isVoidType() ||
                errorLines.contains(sources.getPresumedLineNumber(function->getLocation())))
                continue;
            accepted.insert(function->getName().drop_front(prefix.size()).str());
        }
        return accepted;
    }

    // EN: T19-003. A call from NativeSharp to a C function-like macro of the header (not of the system, the
    //     compiler or the command line) is expanded by Clang inside a static inline wrapper whose parameters
    //     have the types of the call's arguments: `static inline __typeof__(M(a, b)) w(A a, B b) { return M(a, b); }`,
    //     or, when the expansion is not an expression, `static inline void w(A a, B b) { M(a, b); }`. An lvalue
    //     argument (`ref x`) is passed by address and expanded as `(*p)`, so the macro can assign it. Each argument
    //     is evaluated once, as in a call. The source is reparsed with the wrapper and the new translation unit
    //     replaces the session's; the wrapper is then imported under the macro's name (cMacroWrapper). A wrapper
    //     Clang rejects leaves the session as it was and returns Clang's diagnostics. The caller holds the mutex.
    // ES: T19-003. Una llamada desde NativeSharp a una macro de funcion C del encabezado (no del sistema, el
    //     compilador o la linea de ordenes) la expande Clang dentro de un envoltorio static inline cuyos parametros
    //     tienen los tipos de los argumentos de la llamada, o, si la expansion no es una expresion, uno que
    //     devuelve void. Un argumento lvalue (`ref x`) se pasa por direccion y se expande como `(*p)`, asi la macro
    //     puede asignarlo. Cada argumento se evalua una vez, como en una llamada. La fuente se analiza otra vez con
    //     el envoltorio y la nueva unidad reemplaza la de la sesion; el envoltorio se importa con el nombre de la
    //     macro (cMacroWrapper). Un envoltorio que Clang rechaza deja la sesion como estaba y devuelve los
    //     diagnosticos de Clang. Quien llama tiene el mutex.
    // C2-09: whether an object-like macro expands to exactly the name of a function the session declares. / Si una
    //     macro de objeto se expande exactamente al nombre de una funcion que declara la sesion.
    bool macroAliasesFunction(const clang::MacroInfo& info) const
    {
        if (info.isFunctionLike() || info.getNumTokens() != 1) return false;
        const auto* identifier = info.tokens().front().getIdentifierInfo();
        if (!identifier || !info.tokens().front().is(clang::tok::identifier)) return false;
        auto& context = ast_->getASTContext();
        const auto found = context.getTranslationUnitDecl()->lookup(clang::DeclarationName(identifier));
        return std::any_of(found.begin(), found.end(),
                           [](const clang::NamedDecl* declaration) { return llvm::isa<clang::FunctionDecl>(declaration); });
    }

    std::optional<CppTemplateInstantiation> expandCFunctionMacroCall(
        std::string_view name, std::span<const CppTemplateArgument> argumentTypes,
        const CppMacroCallSite* site = nullptr) const
    {
        auto& preprocessor = ast_->getPreprocessor();
        const auto& sources = ast_->getSourceManager();
        auto* identifier = preprocessor.getIdentifierInfo(name);
        const auto* info = identifier ? preprocessor.getMacroInfo(identifier) : nullptr;
        // C2-09: also an object-like alias of a function. / Tambien un alias de objeto de una funcion.
        const bool functionAlias = info && !info->isFunctionLike() && macroAliasesFunction(*info);
        if (!info || (!info->isFunctionLike() && !functionAlias) || info->isBuiltinMacro()) return std::nullopt;
        const auto definition = info->getDefinitionLoc();
        if (definition.isInvalid() || sources.isInSystemHeader(definition) ||
            sources.isWrittenInBuiltinFile(definition) || sources.isWrittenInCommandLineFile(definition) ||
            sources.isWrittenInScratchSpace(definition) || name.starts_with("__nsc_")) return std::nullopt;
        CppTemplateInstantiation result;
        // EN: T19-003. A macro that pastes a parameter into another token (`a##b`) would paste the wrapper's
        //     parameter, not the argument the call wrote: it is rejected (T19-040).
        // ES: T19-003. Una macro que pega un parametro a otro token (`a##b`) pegaria el parametro del envoltorio, no
        //     el argumento de la llamada: se rechaza (T19-040).
        // EN: T19-040. A parameter used only with `#` or `##` receives the argument as the call wrote it; one also
        //     used as a value receives the value, and its `#x` the text of the call. The variable arguments are
        //     still passed only as values.
        // ES: T19-040. Un parametro usado solo con `#` o `##` recibe el argumento tal como lo escribio la llamada;
        //     uno usado tambien como valor recibe el valor, y su `#x` el texto de la llamada. Los argumentos
        //     variables siguen pasandose solo como valores.
        const auto uses = macroParameterUses(*info);
        // EN: Item 16.8, C2-06. __FILE__ and __LINE__ describe the invocation: with the NativeSharp call site the
        //     wrapper of that call is generated under `#line <line> "<file>"`, so they expand to the call's file and
        //     line, as in C. __func__ names the enclosing C function, and a NativeSharp call has none: rejected.
        // ES: Item 16.8, C2-06. __FILE__ y __LINE__ describen la invocacion: con el sitio de la llamada NativeSharp
        //     el envoltorio de esa llamada se genera bajo `#line <linea> "<archivo>"`, asi se expanden al archivo y
        //     la linea de la llamada, como en C. __func__ nombra la funcion C que la contiene, y una llamada
        //     NativeSharp no tiene: se rechaza.
        bool siteSensitive = false;
        for (const auto& token : info->tokens())
            if (const auto* identifier = token.getIdentifierInfo())
            {
                const auto spelled = identifier->getName();
                if (spelled == "__func__" || spelled == "__FUNCTION__" || spelled == "__builtin_FUNCTION")
                {
                    result.diagnostics.push_back("the macro uses " + spelled.str() + ", which names the enclosing C "
                        "function, and a NativeSharp call is not inside one");
                    return result;
                }
                if (spelled == "__FILE__" || spelled == "__LINE__" || spelled == "__builtin_FILE" ||
                    spelled == "__builtin_LINE")
                {
                    if (!site || site->line == 0 || site->file.empty())
                    {
                        result.diagnostics.push_back("the macro uses " + spelled.str() + ", which would describe the "
                            "wrapper NativeSharp generates instead of the call site");
                        return result;
                    }
                    siteSensitive = true;
                }
            }
        // C2-06: only variable arguments used both as values and with `#`/`##` remain out. / Solo quedan fuera los
        //     argumentos variables usados como valor y con `#`/`##`.
        if (info->isVariadic() && info->getNumParams() > 0 && uses.value.back() &&
            (uses.stringified.back() || uses.pasted.back()))
        {
            result.diagnostics.push_back("the macro turns its variable arguments into a string (#) or pastes them "
                                         "(##), and NativeSharp passes variable arguments only as values");
            return result;
        }
        // C2-09: an alias takes the call's arguments, as the function does. / Un alias toma los argumentos de la
        //     llamada, como la funcion.
        const std::size_t parameters = functionAlias ? argumentTypes.size() : info->getNumParams();
        const bool variadic = info->isVariadic();
        if (variadic ? argumentTypes.size() + 1 < parameters : argumentTypes.size() != parameters)
        {
            result.diagnostics.push_back("the macro takes " + std::to_string(variadic ? parameters - 1 : parameters) +
                                         (variadic ? " or more" : "") + " arguments");
            return result;
        }
        auto& context = ast_->getASTContext();
        const bool cpp = context.getLangOpts().CPlusPlus;
        std::string key(name);
        std::string declared;
        std::string unevaluated;
        std::string passed;
        // T19-040: the text of each value argument, for a `#` the expansion reaches. / El texto de cada argumento
        //     valor, para un `#` que alcanza la expansion.
        std::vector<std::pair<std::size_t, std::string>> valueSpellings;
        const bool stringifiesValues = macroStringifiesValues_.contains(std::string(name));
        std::vector<bool> references;
        bool callSpecific = false;
        for (std::size_t index = 0; index < argumentTypes.size(); ++index)
        {
            const auto& argument = argumentTypes[index];
            const std::size_t parameterIndex = variadic && index + 1 >= parameters ? parameters - 1 : index;
            // T19-042: a type argument is always passed as written. / Un argumento de tipo siempre se pasa como se
            //     escribio.
            // C2-06: variable arguments that are only stringified or pasted are tokens too. / Los argumentos
            //     variables que solo se convierten en cadena o se pegan tambien son tokens.
            const bool variadicPart = variadic && parameterIndex + 1 == parameters;
            const bool tokenArgument = argument.token || (parameterIndex < uses.value.size() &&
                !uses.value[parameterIndex] && (!variadicPart || uses.stringified.back() || uses.pasted.back()));
            if (tokenArgument)
            {
                if (!argument.token || argument.sourceSpelling.empty())
                {
                    result.diagnostics.push_back("argument " + std::to_string(index + 1) +
                                                 " is pasted or turned into a string and has no spelling");
                    return result;
                }
                callSpecific = true;
                key += (index ? ",#" : "(#") + argument.sourceSpelling;
                unevaluated += (unevaluated.empty() ? "" : ", ") + argument.sourceSpelling;
                passed += (passed.empty() ? "" : ", ") + argument.sourceSpelling;
                continue;
            }
            std::string spelling;
            if (argument.kind == CppTemplateArgument::Kind::Type && argument.recordUsr)
            {
                const auto* tag = findTagByUsr(*context.getTranslationUnitDecl(), *argument.recordUsr);
                // T19-003: in C++ the qualified name. / En C++ el nombre calificado.
                if (tag && cpp && !tag->getName().empty())
                {
                    clang::PrintingPolicy policy(context.getLangOpts());
                    policy.SuppressTagKeyword = true;
                    policy.FullyQualifiedName = true;
                    spelling = "::" + clang::QualType(context.getCanonicalTagType(tag)).getAsString(policy);
                }
                else if (tag && !tag->getName().empty())
                    spelling = std::string(tag->getKindName()) + " " + tag->getNameAsString();
                else if (tag && tag->getTypedefNameForAnonDecl())
                    spelling = cpp ? "::" + tag->getTypedefNameForAnonDecl()->getQualifiedNameAsString()
                                   : tag->getTypedefNameForAnonDecl()->getNameAsString();
            }
            else if (argument.kind == CppTemplateArgument::Kind::Type)
            {
                const auto type = builtinTemplateArgumentType(context, argument.builtin);
                if (!type.isNull() && !type->isVoidType()) spelling = type.getAsString();
                else if (!type.isNull() && argument.pointerDepth > 0) spelling = "void";
                // EN: Item 5.5. A C `char` is imported as `sbyte`; in a C macro an `sbyte` value goes back as
                //     `char`, so `_Generic` selects the `char` branch a C caller would select.
                // ES: Item 5.5. Un `char` C se importa como `sbyte`; en una macro C un valor `sbyte` vuelve como
                //     `char`, asi `_Generic` elige la rama `char` que elegiria un llamador C.
                if (!cpp && spelling == "signed char") spelling = "char";
            }
            if (spelling.empty())
            {
                result.diagnostics.push_back("argument " + std::to_string(index + 1) + " has no C type");
                return result;
            }
            spelling += std::string(argument.pointerDepth, '*');
            // EN: Item 5.3 (D1). The wrapper evaluates each value argument once. When the macro evaluates the
            //     parameter several times or conditionally and the argument has side effects (a call, an assignment,
            //     an increment), the call is rejected instead of changing its result.
            // ES: Item 5.3 (D1). El envoltorio evalua cada argumento valor una vez. Si la macro evalua el parametro
            //     varias veces o de forma condicional y el argumento tiene efectos (una llamada, una asignacion, un
            //     incremento), la llamada se rechaza en lugar de cambiar su resultado.
            // Item 5.10: a written parameter needs the caller's place. / Un parametro escrito necesita el lugar.
            if (parameterIndex < uses.written.size() && uses.written[parameterIndex] && !argument.lvalue)
            {
                result.diagnostics.push_back("the macro writes or takes the address of argument " +
                    std::to_string(index + 1) + ", so it needs the caller's variable: pass it with ref ('ref " +
                    argument.sourceSpelling + "') instead of a copy");
                return result;
            }
            // EN: C2-05. Only an argument without side effects gives the same result evaluated once: the frontend
            //     classifies it from its syntax, and a name of volatile storage of the header is a read per evaluation.
            // ES: C2-05. Solo un argumento sin efectos da el mismo resultado evaluado una vez: el frontend lo
            //     clasifica por su sintaxis, y un nombre de almacenamiento volatile del encabezado es una lectura por
            //     evaluacion.
            if (parameterIndex < uses.valueCount.size() && !argument.lvalue &&
                (uses.valueCount[parameterIndex] > 1 || (uses.conditional && uses.valueCount[parameterIndex] > 0)))
            {
                const auto& text = argument.sourceSpelling;
                bool volatileName = false;
                if (!text.empty() && std::all_of(text.begin(), text.end(), [](char c)
                        { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }))
                    for (const auto* declaration : ast_->getASTContext().getTranslationUnitDecl()->decls())
                        if (const auto* variable = llvm::dyn_cast<clang::VarDecl>(declaration))
                            if (variable->getIdentifier() && variable->getName() == text &&
                                (variable->getType().isVolatileQualified() || variable->getType()->isAtomicType()))
                                volatileName = true;
                if (!argument.sideEffectFree || volatileName)
                {
                    result.diagnostics.push_back("the macro evaluates argument " + std::to_string(index + 1) + " " +
                        (uses.valueCount[parameterIndex] > 1 ? "more than once" : "conditionally") +
                        ", and '" + text + "' may have side effects that NativeSharp would evaluate only once; "
                        "store the value in a local and pass the local");
                    return result;
                }
            }
            const std::string parameter = "__nsc_a" + std::to_string(index);
            key += (index ? "," : "(") + std::string(argument.lvalue ? "&" : "") + spelling;
            // T19-003: in C++ an lvalue argument is a reference. / En C++ un argumento lvalue es una referencia.
            declared += (declared.empty() ? "" : ", ") + spelling + (argument.lvalue ? (cpp ? "& " : "* ") : " ") +
                parameter;
            unevaluated += (unevaluated.empty() ? "" : ", ") + std::string("(*(") + spelling + "*)0)";
            const std::string value = argument.lvalue && !cpp ? "(*" + parameter + ")" : parameter;
            references.push_back(argument.lvalue);
            valueSpellings.emplace_back(index, argument.sourceSpelling);
            // T19-040: a macro that stringifies its values has one wrapper per text. / Una macro que convierte sus
            //     valores en cadena tiene un envoltorio por texto.
            if (stringifiesValues)
            {
                callSpecific = true;
                key += "#" + argument.sourceSpelling;
            }
            passed += (passed.empty() ? "" : ", ") + value;
        }
        key += ")";
        // C2-06: a site-sensitive macro has one wrapper per call site. / Una macro sensible al sitio tiene un
        //     envoltorio por sitio de llamada.
        if (siteSensitive)
        {
            key += "@" + site->file + ":" + std::to_string(site->line);
            callSpecific = true;
        }
        if (macroWrapperByCall_.contains(key))
        {
            result.instantiated = true;
            result.name = std::string(name);
            if (callSpecific) result.cMacroWrapper = macroWrapperByCall_.at(key);
            return result;
        }
        const std::string wrapper = "__nsc_macro_" + std::string(name) + "_" +
                                    std::to_string(macroWrapperByCall_.size());
        const std::string parameterList = declared.empty() ? "void" : declared;
        const std::string call = std::string(name) + "(" + passed + ")";
        // EN: T19-003. In C++ the wrapper is an inline function of the global namespace whose result is deduced
        //     (`auto`, a value, or void).
        // ES: T19-003. En C++ el envoltorio es una funcion inline del namespace global cuyo resultado se deduce
        //     (`auto`, un valor, o void).
        const std::vector<std::string> cppForms = {
            "\ninline auto " + wrapper + "(" + parameterList + ")\n{ return " + call + "; }\n",
            "\ninline void " + wrapper + "(" + parameterList + ")\n{ " + call + "; }\n"};
        // EN: T19-040. After the exact type, the type of the expression after C's lvalue conversion
        //     (`(0, M(...))`), so a macro that gives a string literal (`#x`) returns `char*`, as the array decays in C.
        // ES: T19-040. Tras el tipo exacto, el tipo de la expresion tras la conversion de lvalue de C (`(0, M(...))`),
        //     asi que una macro que da un literal de cadena (`#x`) devuelve `char*`, como decae el arreglo en C.
        const std::vector<std::string> cForms = {
            "\nstatic inline __typeof__(" + std::string(name) + "(" + unevaluated + ")) " + wrapper + "(" +
                parameterList + ")\n{ return " + call + "; }\n",
            "\nstatic inline __typeof__((0, " + std::string(name) + "(" + unevaluated + "))) " + wrapper + "(" +
                parameterList + ")\n{ return " + call + "; }\n",
            "\nstatic inline void " + wrapper + "(" + parameterList + ")\n{ " + call + "; }\n"};
        // EN: C2-06. A result type that only exists at block scope (a compound literal built from the arguments,
        //     `sizeof((int[]){__VA_ARGS__})`) cannot be written as `__typeof__` of the file-scope return type: a probe
        //     function deduces it at block scope and the wrapper names it explicitly, before the `void` form.
        // ES: C2-06. Un tipo de resultado que solo existe en ambito de bloque (un literal compuesto hecho con los
        //     argumentos) no se puede escribir como `__typeof__` del tipo de retorno de archivo: una funcion de sondeo
        //     lo deduce en ambito de bloque y el envoltorio lo nombra explicitamente, antes de la forma `void`.
        std::vector<std::string> forms = cpp ? cppForms : cForms;
        if (!cpp)
        {
            const std::string probeName = "__nsc_type_probe_" + wrapper;
            std::string probe = source_ + "\nstatic inline void " + probeName + "(" + parameterList +
                ")\n{ __typeof__(" + call + ") __nsc_result = " + call + "; (void)__nsc_result; }\n";
            llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> probeFilesystem(
                llvm::vfs::createPhysicalFileSystem().release());
            if (!probeFilesystem->setCurrentWorkingDirectory(utf8(options_.workingDirectory)))
            {
                std::vector<CppSessionDiagnostic> probeDiagnostics;
                detail::DiagnosticCollector probeCollector(probeDiagnostics);
                auto probeUnit = detail::runClangSessionAction(probe, detail::buildClangInvocationArguments(options_),
                    inputPath_, utf8(options_.driverReferencePath), &probeCollector, std::move(probeFilesystem),
                    CppCancellationToken{});
                if (probeUnit && !probeUnit->getDiagnostics().hasErrorOccurred())
                    for (const auto* declaration : probeUnit->getASTContext().getTranslationUnitDecl()->decls())
                        if (const auto* function = llvm::dyn_cast<clang::FunctionDecl>(declaration);
                            function && function->getIdentifier() && function->getName() == probeName &&
                            function->getBody())
                            for (const auto* statement : function->getBody()->children())
                                if (const auto* declarations = llvm::dyn_cast<clang::DeclStmt>(statement))
                                    for (const auto* item : declarations->decls())
                                        if (const auto* variable = llvm::dyn_cast<clang::VarDecl>(item);
                                            variable && variable->getName() == "__nsc_result")
                                        {
                                            const auto type = variable->getType().getCanonicalType()
                                                .getUnqualifiedType();
                                            if (!type->isVoidType() && !type->isArrayType() &&
                                                !type->isFunctionType() && (type->isScalarType() ||
                                                    (type->isRecordType() && !type->getAsRecordDecl()
                                                        ->getDeclName().isEmpty())))
                                            {
                                                clang::PrintingPolicy policy(
                                                    probeUnit->getASTContext().getLangOpts());
                                                forms.insert(forms.end() - 1, "\nstatic inline " +
                                                    type.getAsString(policy) + " " + wrapper + "(" + parameterList +
                                                    ")\n{ return " + call + "; }\n");
                                            }
                                        }
            }
        }
        std::vector<std::string> rejected;
        for (const auto& baseForm : forms)
        {
            // C2-06: the wrapper of a site-sensitive call sits, on one line, at the call's file and line. / El
            //     envoltorio de una llamada sensible al sitio queda, en una linea, en el archivo y linea de la llamada.
            std::string form = baseForm;
            if (siteSensitive)
            {
                std::string file;
                for (const char c : site->file)
                {
                    if (c == '\\' || c == '"') file.push_back('\\');
                    file.push_back(c);
                }
                std::string oneLine = form.substr(1);
                std::replace(oneLine.begin(), oneLine.end(), '\n', ' ');
                form = "\n#line " + std::to_string(site->line) + " \"" + file + "\"\n" + oneLine + "\n";
            }
            std::string code = source_ + form;
            llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> filesystem(
                llvm::vfs::createPhysicalFileSystem().release());
            if (filesystem->setCurrentWorkingDirectory(utf8(options_.workingDirectory))) return std::nullopt;
            macroWrapperDiagnostics_.clear();
            auto unit = detail::runClangSessionAction(code, detail::buildClangInvocationArguments(options_),
                inputPath_, utf8(options_.driverReferencePath), &macroWrapperConsumer_, std::move(filesystem),
                CppCancellationToken{});
            bool failed = !unit || !unit->hasSema() || unit->getDiagnostics().hasErrorOccurred() ||
                std::any_of(macroWrapperDiagnostics_.begin(), macroWrapperDiagnostics_.end(), [](const auto& item) {
                    return item.severity == CppSessionDiagnosticSeverity::Error ||
                        item.severity == CppSessionDiagnosticSeverity::Fatal;
                });
            // EN: T19-003. A macro that turns an argument into a string (`#x`, also through another macro) would
            //     spell the wrapper's parameter, not the argument the call wrote: it is rejected (T19-040).
            // ES: T19-003. Una macro que convierte un argumento en cadena (`#x`, tambien a traves de otra macro)
            //     escribiria el parametro del envoltorio, no el argumento de la llamada: se rechaza (T19-040).
            bool stringified = false;
            const std::function<void(const clang::Stmt*)> visit = [&](const clang::Stmt* statement) {
                if (!statement || stringified) return;
                if (const auto* literal = llvm::dyn_cast<clang::StringLiteral>(statement);
                    literal && literal->getCharByteWidth() == 1 && literal->getString().contains("__nsc_a"))
                    stringified = true;
                for (const auto* child : statement->children()) visit(child);
            };
            const auto visitWrapper = [&] {
                for (const auto* declaration : unit->getASTContext().getTranslationUnitDecl()->decls())
                    if (const auto* function = llvm::dyn_cast<clang::FunctionDecl>(declaration);
                        function && function->getIdentifier() && function->getName() == wrapper)
                        visit(function->getBody());
            };
            if (!failed) visitWrapper();
            // EN: T19-040. Instead, the wrapper Clang accepted is preprocessed, each `"__nsc_aK"` (or `"(*__nsc_aK)"`
            //     for a `ref` argument) becomes the text the call wrote, as C writes `#x` of that argument, and the
            //     result is analyzed again. This covers `#x` directly and through another macro.
            // ES: T19-040. En su lugar, el envoltorio que Clang acepto se preprocesa, cada `"__nsc_aK"` (o
            //     `"(*__nsc_aK)"` para un argumento `ref`) pasa a ser el texto que escribio la llamada, como C escribe
            //     `#x` de ese argumento, y el resultado se analiza otra vez. Cubre `#x` directo y a traves de otra
            //     macro.
            if (stringified)
            {
                std::vector<std::pair<std::string, std::string>> spellings;
                for (const auto& [index, text] : valueSpellings)
                {
                    if (text.empty())
                    {
                        result.diagnostics.push_back("argument " + std::to_string(index + 1) +
                                                     " is turned into a string and has no spelling");
                        return result;
                    }
                    const auto literal = cStringLiteral(text);
                    spellings.emplace_back("\"__nsc_a" + std::to_string(index) + "\"", literal);
                    spellings.emplace_back("\"(*__nsc_a" + std::to_string(index) + ")\"", literal);
                }
                unit = expandStringifiedWrapper(code, wrapper, spellings);
                failed = !unit || !unit->hasSema() || unit->getDiagnostics().hasErrorOccurred();
                // EN: T19-040. A string that still spells a wrapper parameter stringified more than the argument
                //     (`#x` of `x + 1`): it keeps the T19-003 rejection.
                // ES: T19-040. Una cadena que aun escribe un parametro del envoltorio convirtio en cadena mas que el
                //     argumento (`#x` de `x + 1`): conserva el rechazo de T19-003.
                stringified = false;
                if (!failed) visitWrapper();
                if (stringified)
                {
                    result.diagnostics.push_back("the macro turns an argument into a string (#), and a NativeSharp "
                                                 "argument has no C/C++ spelling");
                    return result;
                }
                if (!failed && !stringifiesValues)
                {
                    macroStringifiesValues_.insert(std::string(name));
                    callSpecific = true;
                    for (const auto& [index, text] : valueSpellings) key += "#" + text;
                }
            }
            if (failed)
            {
                if (rejected.empty())
                    for (const auto& diagnostic : macroWrapperDiagnostics_)
                        if (diagnostic.severity == CppSessionDiagnosticSeverity::Error ||
                            diagnostic.severity == CppSessionDiagnosticSeverity::Fatal)
                        {
                            rejected.push_back(describeCppDiagnostic(diagnostic));
                            for (const auto& note : diagnostic.notes) rejected.push_back(describeCppDiagnostic(note));
                        }
                continue;
            }
            // The adopted unit reports to the session's consumer again. / La unidad adoptada vuelve a informar al
            //     consumidor de la sesion.
            unit->getDiagnostics().setClient(&consumer_, false);
            ast_ = std::move(unit);
            source_ = code;
            synthesizeComplexCarriers(ast_->getASTContext());
            // EN: T19-003. In C++ the new translation unit starts without what Sema instantiated in the old one:
            //     the named specializations of the header and the template uses NativeSharp requested.
            // ES: T19-003. En C++ la nueva unidad empieza sin lo que Sema instancio en la anterior: las
            //     especializaciones que nombra el encabezado y los usos de plantillas que pidio NativeSharp.
            if (cpp)
            {
                functionSpecializationNames_.clear();
                nativeStructMirrors_.clear();
                nativeStructLayouts_.clear();
                instantiateNamedSpecializations(ast_->getASTContext(), ast_->getSema());
                const auto uses = templateUses_;
                for (const auto& use : uses) instantiateTemplateLocked(use.name, use.function, use.arguments);
            }
            macroWrapperByCall_.emplace(key, wrapper);
            macroOfWrapper_.emplace(wrapper, std::string(name));
            // T19-040: only the parameters of the wrapper, without the token arguments. / Solo los parametros
            //     del envoltorio, sin los argumentos de tokens.
            macroWrapperReferences_.emplace(wrapper, std::move(references));
            result.instantiated = true;
            result.name = std::string(name);
            if (callSpecific) result.cMacroWrapper = wrapper;
            return result;
        }
        result.diagnostics.push_back("Clang cannot expand the macro with these argument types");
        result.diagnostics.insert(result.diagnostics.end(), rejected.begin(), rejected.end());
        return result;
    }

    // T19-040: the macros whose expansion stringifies a value argument. / Las macros cuya expansion convierte en
    //     cadena un argumento valor.
    mutable std::set<std::string> macroStringifiesValues_;

    // EN: T19-040. Preprocesses `code`, whose last function is `wrapper`, and analyzes `source_` with that
    //     function as Clang expanded it, each placeholder string replaced by the text of the call. Updates `code`
    //     to the new source; null when Clang rejects it.
    // ES: T19-040. Preprocesa `code`, cuya ultima funcion es `wrapper`, y analiza `source_` con esa funcion tal como
    //     la expandio Clang, cada cadena provisional sustituida por el texto de la llamada. Actualiza `code` con la
    //     fuente nueva; nulo si Clang la rechaza.
    std::unique_ptr<clang::ASTUnit> expandStringifiedWrapper(
        std::string& code, const std::string& wrapper,
        const std::vector<std::pair<std::string, std::string>>& spellings) const
    {
        llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> filesystem(llvm::vfs::createPhysicalFileSystem().release());
        if (filesystem->setCurrentWorkingDirectory(utf8(options_.workingDirectory))) return nullptr;
        macroWrapperDiagnostics_.clear();
        const auto preprocessed = detail::preprocessClangSessionSource(code,
            detail::buildClangInvocationArguments(options_), inputPath_, utf8(options_.driverReferencePath),
            std::move(filesystem), &macroWrapperDiagnostics_);
        if (!preprocessed) return nullptr;
        const auto at = preprocessed->rfind(wrapper);
        if (at == std::string::npos) return nullptr;
        const auto start = preprocessed->rfind('\n', at);
        std::string function;
        std::size_t line = start == std::string::npos ? 0 : start + 1;
        while (line < preprocessed->size())
        {
            const auto end = preprocessed->find('\n', line);
            const auto text = preprocessed->substr(line, end == std::string::npos ? std::string::npos : end - line);
            if (!text.starts_with("#")) function += text + "\n";
            if (end == std::string::npos) break;
            line = end + 1;
        }
        for (const auto& [placeholder, literal] : spellings)
            for (auto found = function.find(placeholder); found != std::string::npos;
                 found = function.find(placeholder, found + literal.size()))
                function.replace(found, placeholder.size(), literal);
        code = source_ + "\n" + function;
        llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> again(llvm::vfs::createPhysicalFileSystem().release());
        if (again->setCurrentWorkingDirectory(utf8(options_.workingDirectory))) return nullptr;
        macroWrapperDiagnostics_.clear();
        return detail::runClangSessionAction(code, detail::buildClangInvocationArguments(options_), inputPath_,
            utf8(options_.driverReferencePath), &macroWrapperConsumer_, std::move(again), CppCancellationToken{});
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
                // T19-005: an alias template is named as a class template. / Se nombra como una de clase.
                else if (const auto* aliasTemplate = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(declaration);
                         aliasTemplate && !aliasTemplate->isInvalidDecl() && aliasTemplate->getTemplatedDecl())
                    visitor(*aliasTemplate, false);
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
    // T19-003: a translation unit adopted in a const reparse reports to it again. / Una unidad adoptada en un
    //     nuevo analisis const vuelve a informarle.
    mutable detail::DiagnosticCollector consumer_;
    // EN: T19-003. The diagnostics of the reparses that add macro wrappers; a translation unit adopted from one
    //     reports to consumer_ again. Declared before ast_, which borrows the consumer while it is parsed.
    // ES: T19-003. Los diagnosticos de los nuevos analisis que agregan envoltorios de macros; una unidad adoptada
    //     de uno vuelve a informar a consumer_. Se declara antes de ast_, que toma prestado el consumidor.
    mutable std::vector<CppSessionDiagnostic> macroWrapperDiagnostics_;
    mutable detail::DiagnosticCollector macroWrapperConsumer_{macroWrapperDiagnostics_};
    // Destroy before consumer_: Clang's diagnostic engine borrows the consumer.
    // T19-003: replaced by the reparse that adds a macro wrapper. / Lo reemplaza el analisis con un envoltorio.
    mutable std::unique_ptr<clang::ASTUnit> ast_;
    // T12-004: the analyzed source, reparsed with the member adapters appended.
    // T19-003: also with the macro wrappers. / Tambien con los envoltorios de macros.
    mutable std::string source_;
    // C2-03: brace-initializer elements: element name -> (C type, macro). / Elementos de listas entre llaves.
    mutable std::map<std::string, std::pair<std::string, std::string>> braceElements_;
    // T19-003: the input as read, without wrappers. / La entrada como se leyo, sin envoltorios.
    std::string originalSource_;
    // EN: T19-003. The wrapper of each expanded C function-like macro call, by macro name and argument types, and
    //     the macro each wrapper expands.
    // ES: T19-003. El envoltorio de cada llamada expandida a una macro de funcion C, por nombre de macro y tipos
    //     de los argumentos, y la macro que expande cada envoltorio.
    mutable std::map<std::string, std::string> macroWrapperByCall_;
    mutable std::map<std::string, std::string> macroOfWrapper_;
    // T19-003: which parameters of each wrapper stand for lvalue arguments. / Que parametros son argumentos lvalue.
    mutable std::map<std::string, std::vector<bool>> macroWrapperReferences_;
    // EN: T19-003. The internal constant that holds the value of each C object-like macro with a pointer, string or
    //     record value, and the typedef of each one that names a type, with the macro each one stands for.
    // ES: T19-003. La constante interna que guarda el valor de cada macro de objeto C con valor puntero, cadena o
    //     record, y el typedef de cada una que nombra un tipo, con la macro que representa cada uno.
    mutable std::map<std::string, std::string> macroOfStorage_;
    // T19-041: accessor -> (macro, lvalue). / Accesora -> (macro, lvalue).
    mutable std::map<std::string, std::pair<std::string, bool>> macroOfAccessor_;
    mutable std::map<std::string, std::string> macroOfType_;
    // EN: T19-003. The template instantiations NativeSharp requested, repeated after a reparse that adds a C++
    //     macro wrapper, because the new translation unit starts without them.
    // ES: T19-003. Las instanciaciones de plantillas que pidio NativeSharp, repetidas tras un nuevo analisis que
    //     agrega un envoltorio de macro C++, porque la nueva unidad empieza sin ellas.
    struct TemplateUse
    {
        std::string name;
        bool function = false;
        std::vector<CppTemplateArgument> arguments;
    };
    mutable std::vector<TemplateUse> templateUses_;
    std::string inputPath_;
    std::optional<basic::Sha256Digest> preprocessingFingerprint_;
    // T13-005: the function template specializations NativeSharp requested. / Las que pidio NativeSharp.
    mutable CppFunctionSpecializationNames functionSpecializationNames_;
    // T13-007: the mirrors of NativeSharp structs, by qualified C++ name. / Los espejos, por nombre C++.
    mutable std::map<std::string, clang::CXXRecordDecl*> nativeStructMirrors_;
    mutable std::vector<CppNativeStructLayout> nativeStructLayouts_;
    // T14-002: the macros evaluated once per session. / Las macros, evaluadas una vez por sesion.
    mutable std::optional<MacroConstants> macroConstants_;
    // T19-005: the names using-declarations, directives and namespace aliases give to templates.
    // T19-005: los nombres que dan a las plantillas las declaraciones y directivas using y los alias de namespace.
    mutable std::optional<std::multimap<const clang::Decl*, std::string>> templateAlternates_;
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
