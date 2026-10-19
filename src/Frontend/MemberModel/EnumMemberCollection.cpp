#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "../Internal/EnumInitializerResolver.h"
#include "../Internal/AnonymousMemberPromotionVisitState.h"
#include "../Internal/EnumValue.h"
#include "../Internal/EnumSuccessor.h"

#include "nativesharp/Language/AggregateSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Compilation/ProjectGraph.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/PrecompiledMemberIdentityIndex.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <filesystem>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
// Stage 51.x: enum member support in the OOP pipeline. Each `EnumMemberDeclaration` becomes a
// `public static const int` member of its enum type, reusing the existing Stage 37 native
// constant-field machinery for the read side (semantic member access, HIR/MIR field-initializer
// materialization, and LLVM constant-global lowering) end to end -- no new member kind and no
// new LLVM lowering path are introduced. Values follow the same rule as the classic single-file
// enum pipeline in SemanticAnalyzer::analyzeEnum: implicit members auto-increment from 0, and an
// explicit initializer is only accepted when it is a plain (optionally negated) integer literal;
// this is narrower than a full constant-expression evaluator, but it covers every enum this
// project declares (StringComparison, StringSplitOptions) and it fails loudly instead of
// silently mis-evaluating something more exotic.
namespace
{
// EL VALOR DE UN ENUMERADOR SE LLEVA COMO (MAGNITUD SIN SIGNO, SIGNO), NO COMO UN `int64_t`.
//
// Parece un rodeo y no lo es: los dos extremos del rango que un enum puede declarar --el minimo de
// `long`, que es -2^63, y el maximo de `ulong`, que es 2^64-1-- NO CABEN EN EL MISMO TIPO DE C++.
// Estrechar a `int64_t` antes de comprobar hace que `ulong.MaxValue` se vea como -1 y se rechace
// una constante perfectamente legal, y hace que el limite superior de `ulong` sea incomprobable.
// Con el par, el rango de los diez tipos enteros se comprueba exactamente y sin casos especiales.
using internal::EnumValue;

// ¿Cabe este valor en el tipo subyacente? Se compara sobre el ancho y el signo REALES.
//
// LOS DOS LADOS IMPORTAN. Demasiado ancho aceptaria 300 en un `enum : byte` y lo truncaria al
// materializarlo -- un valor que el programa nunca escribio. Demasiado estrecho no dejaria escribir
// en un `enum : long` o `: ulong` constantes que C escribe todos los dias.
bool enumValueFits(EnumValue value, const builtins::IntegerTypeInfo* underlying)
{
    if (underlying == nullptr || underlying->width == 0 || underlying->width > 64) return false;
    const unsigned width = underlying->width;
    if (value.negative)
    {
        // `-0` es 0, y es el unico negativo que un tipo sin signo admite.
        if (!underlying->isSigned) return value.magnitude == 0;
        // El minimo de un tipo con signo de N bits es -2^(N-1): la magnitud llega justo a 2^(N-1).
        const std::uint64_t limit =
            width >= 64U ? (std::uint64_t{1} << 63) : (std::uint64_t{1} << (width - 1U));
        return value.magnitude <= limit;
    }
    if (underlying->isSigned)
    {
        const std::uint64_t limit =
            (width >= 64U ? (std::uint64_t{1} << 63) : (std::uint64_t{1} << (width - 1U))) - 1U;
        return value.magnitude <= limit;
    }
    if (width >= 64U) return true;   // 2^64-1 no se puede escribir como (1<<64)-1
    return value.magnitude <= (std::uint64_t{1} << width) - 1U;
}

// El patron de bits que se guarda. Los consumidores del valor --el evaluador de constantes, el
// HIR y el backend-- lo convierten a `uint64_t` inmediatamente y lo normalizan por el tipo, asi
// que lo que el `int64_t` de ClassMemberSymbol lleva son BITS y no una magnitud con signo. Por eso
// `ulong.MaxValue` puede viajar dentro de el.
std::uint64_t enumValueBits(EnumValue value)
{
    return value.negative ? (0U - value.magnitude) : value.magnitude;
}

// El sucesor, para el auto-incremento. Se calcula sobre el par y no sobre los bits, para que
// -1 -> 0 -> 1 sea una cuenta y no una envoltura.
//
// `hasValue` en falso significa NO HAY SUCESOR REPRESENTABLE: el anterior era 2^64-1, que es el
// mayor entero que cualquier tipo subyacente admite. Hace falta un tercer estado y no vale saturar
// devolviendo el mismo valor -- eso es lo que hacia la primera version de esta funcion, y el
// resultado era que
//
//     enum E : ulong { Max = 18446744073709551615, Siguiente }
//
// compilaba y `Siguiente` valia LO MISMO que `Max`, en silencio. Saturar es una RESPUESTA FALSA:
// el programa recibe un valor que nadie escribio y dos nombres distintos que no se pueden
// distinguir. Y el comentario que lo acompanaba era ademas incorrecto -- decia que `enumValueFits`
// lo rechazaria, cuando 2^64-1 cabe perfectamente en `ulong` y por eso pasaba.
using internal::EnumSuccessor;

EnumSuccessor enumValueNext(EnumValue value)
{
    if (value.negative && value.magnitude > 0)
    {
        const std::uint64_t reduced = value.magnitude - 1U;
        return EnumSuccessor{EnumValue{reduced, reduced != 0}, true};
    }
    if (value.magnitude == ~std::uint64_t{0}) return EnumSuccessor{value, false};
    return EnumSuccessor{EnumValue{value.magnitude + 1U, false}, true};
}

// Enum members are registered before method-body name binding runs, but their initializers need
// the same typed integer constant evaluator used by const fields and switch labels. This adapter
// deliberately resolves only names already registered in the enum being collected. That permits
// the normal left-to-right idiom (`All = Red | Green | Blue | Alpha`) without making a dynamic
// expression, a member from another type, or a forward reference look constant by accident.
using internal::EnumInitializerResolver;

EnumValue enumValueFromIntegerConstant(const ProjectIntegerValue& value)
{
    if (ProjectIntegerEvaluator::isSignedInteger(value.typeKey) && value.asSigned() < 0)
    {
        return EnumValue{std::uint64_t{0} - value.bits, true};
    }
    return EnumValue{value.bits, false};
}
}

bool prepareEnumMemberValue(
    const ast::AstNode* memberNode,
    std::size_t typeOrdinal,
    const NamedTypeDeclaration& type,
    std::string_view underlyingKey,
    const builtins::IntegerTypeInfo* underlying,
    const ProjectIntegerEvaluator& integerEvaluator,
    std::unordered_map<std::string, const ast::AstNode*>& seen,
    EnumSuccessor& nextValue,
    EnumValue& value,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const auto previous = seen.find(memberNode->text());
    if (previous != seen.end())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::DuplicateSymbol,
            diagnostics::DiagnosticSeverity::Error,
            memberNode->range(),
            "duplicate enum member '" + memberNode->text() + "' in type '" +
                type.identity.fullyQualifiedName() + "'");
        diagnostics.report(
            diagnostics::DiagnosticId::PreviousDeclaration,
            diagnostics::DiagnosticSeverity::Note,
            previous->second->range(),
            "previous declaration is here");
        return false;
    }
    seen.emplace(memberNode->text(), memberNode);

    // SIN SUCESOR NO HAY MIEMBRO. Un enumerador sin inicializador detras del maximo de 64 bits
    // no tiene ningun valor que tomar, y darle uno --el que sea-- seria inventarlo.
    if (!nextValue.hasValue && memberNode->first() == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidEnumInitializer,
            diagnostics::DiagnosticSeverity::Error,
            memberNode->range(),
            "enum member '" + memberNode->text() + "' has no value: the previous member is the "
            "largest value of the underlying type '" + std::string(underlyingKey) +
            "', so there is no next one to count to");
        return false;
    }

    value = nextValue.value;
    if (memberNode->first() != nullptr)
    {
        const std::optional<ProjectIntegerValue> folded =
            integerEvaluator.evaluate(memberNode->first(), typeOrdinal);
        if (!folded.has_value())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidEnumInitializer,
                diagnostics::DiagnosticSeverity::Error,
                memberNode->range(),
                "enum initializer for '" + memberNode->text() +
                    "' must be an integral constant expression");
            return false;
        }
        value = enumValueFromIntegerConstant(*folded);
    }
    // LA COMPROBACION DE RANGO ESTA FUERA DEL `if` A PROPOSITO. El valor de un miembro sin
    // inicializador sale del auto-incremento, y ese tambien se puede salir: en
    // `enum E : byte { A = 255, B }` el 255 cabe y el 256 no, y quien lo escribio no puso
    // ningun numero que mirar. Comprobar solo lo escrito dejaria pasar justo el caso que nadie
    // ve.
    if (!enumValueFits(value, underlying))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidEnumInitializer,
            diagnostics::DiagnosticSeverity::Error,
            memberNode->range(),
            "enum value for '" + memberNode->text() + "' does not fit in the underlying "
            "type '" + std::string(underlyingKey) + "' of this enum");
        return false;
    }
    nextValue = enumValueNext(value);
    return true;
}

ClassMemberSymbol makeEnumMemberSymbol(
    std::size_t typeOrdinal,
    const NamedTypeDeclaration& type,
    const ast::AstNode* memberNode,
    const EnumValue& value)
{
    ClassMemberSymbol symbol;
    symbol.kind = ClassMemberKind::Field;
    symbol.accessibility = ClassMemberAccessibility::Public;
    symbol.declaringTypeOrdinal = typeOrdinal;
    symbol.fileId = type.fileId;
    symbol.range = memberNode->range();
    symbol.syntax = memberNode;
    symbol.sourcePath = type.parts.empty() ? std::string{} : type.parts.front().sourcePath;
    symbol.isPrecompiled = type.isPrecompiled;
    symbol.name = memberNode->text();
    // The member's declared/visible type is the enum itself (so `Mode.Second` type-checks as
    // `Mode`, matching a `Mode`-typed parameter/local and comparing correctly against other
    // `Mode` values) -- not its int-sized underlying storage. `isEnumConstant` below is what
    // tells the MIR/LLVM layers to still treat it as an integral compile-time constant.
    symbol.declaredTypeKey = "type:" + type.identity.key();
    symbol.isStatic = true;
    symbol.isReadonly = true;
    symbol.isConst = true;
    symbol.isEnumConstant = true;
    // BITS, no magnitud con signo -- ver enumValueBits(). Es lo que deja que un
    // `enum E : ulong { Max = 18446744073709551615 }` viaje en un `int64_t`.
    symbol.enumConstantValue = static_cast<std::int64_t>(enumValueBits(value));
    return symbol;
}

bool ProjectMemberModel::collectEnumMembers(
    std::size_t typeOrdinal,
    const NamedTypeDeclaration& type,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (type.syntax == nullptr)
    {
        return !diagnostics.hasErrors();
    }

    // EL RANGO ADMITIDO SALE DEL TIPO SUBYACENTE, no siempre de `int`.
    //
    // Antes esto exigia que todo valor cupiera en un `int` de 32 bits. Con `enum : byte` eso seria
    // demasiado ancho --aceptaria 300 en un enum de un byte y lo truncaria al materializarlo-- y
    // con `enum : long` demasiado estrecho. Las dos direcciones son fallos, y la segunda ni
    // siquiera dejaria escribir constantes que C escribe todos los dias.
    //
    // La regla del tipo subyacente esta en include/nativesharp/Frontend/EnumUnderlyingType.h, que
    // es la misma que consultan el modelo de layout y el backend.
    const std::string_view underlyingKey = enumUnderlyingTypeKey(type.syntax);
    const builtins::IntegerTypeInfo* underlying = builtins::findIntegerType(underlyingKey);
    // `enum E : bool` o `enum E : MiClase` se rechaza aqui, una vez, y no en cada miembro.
    if (type.syntax->typeSyntax() != nullptr && declaredEnumUnderlyingTypeKey(type.syntax).empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidEnumInitializer,
            diagnostics::DiagnosticSeverity::Error,
            type.range,
            "the underlying type of an enum must be a built-in integer type "
            "(byte, sbyte, short, ushort, int, uint, long, ulong, nint, nuint)");
        return false;
    }

    std::unordered_map<std::string, const ast::AstNode*> seen;
    const EnumInitializerResolver initializerResolver(*this, *nameLookup_);
    const ProjectIntegerEvaluator integerEvaluator(&initializerResolver);
    EnumSuccessor nextValue;
    for (const ast::AstNode* memberNode : type.syntax->children())
    {
        if (memberNode == nullptr || memberNode->kind() != ast::AstNodeKind::EnumMemberDeclaration)
        {
            continue;
        }

        EnumValue value;
        if (!prepareEnumMemberValue(
                memberNode, typeOrdinal, type, underlyingKey, underlying,
                integerEvaluator, seen, nextValue, value, diagnostics))
            continue;
        ClassMemberSymbol symbol = makeEnumMemberSymbol(
            typeOrdinal, type, memberNode, value);

        const std::size_t memberOrdinal = members_.size();
        members_.push_back(std::move(symbol));
        membersByType_[typeOrdinal].push_back(memberOrdinal);
        membersByName_[typeOrdinal][members_[memberOrdinal].name].push_back(memberOrdinal);
        memberBySyntax_.emplace(memberNode, memberOrdinal);
    }

    return !diagnostics.hasErrors();
}
}
