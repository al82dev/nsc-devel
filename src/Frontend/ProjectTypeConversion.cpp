#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <array>
#include <limits>
#include <span>
#include <string>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
using Pair = std::pair<std::string_view, std::string_view>;

constexpr std::array<std::string_view, 13> kNumericTypes = {
    "builtin:sbyte",
    "builtin:byte", "builtin:char", "builtin:short", "builtin:ushort",
    "builtin:int", "builtin:uint", "builtin:long", "builtin:ulong",
    "builtin:nint", "builtin:nuint", "builtin:float", "builtin:double"};

constexpr std::array<std::string_view, 11> kIntegralTypes = {
    "builtin:sbyte",
    "builtin:byte", "builtin:char", "builtin:short", "builtin:ushort",
    "builtin:int", "builtin:uint", "builtin:long", "builtin:ulong",
    "builtin:nint", "builtin:nuint"};

// ID 121. Ancho y signo de los tipos integrales, para la regla de promocion entera de mas abajo.
//
// DICE LO MISMO que `ProjectIntegerEvaluator::kIntegerTypes`, y esta escrita dos veces A
// PROPOSITO: es el evaluador de constantes quien llama aqui, no al reves, asi que este archivo
// tiene que poder contestar sin depender de el. Las dos tablas se fijan la una contra la otra en
// `nsc.cc1.integer-evaluator` y en la fixture c17; si algun dia discrepan, discrepan ahi.
//
// `builtin:char` mide OCHO bits -- CC1-100, la misma decision que toma `constantFits` abajo.
// `nint`/`nuint` son de 64 bits en este objetivo (Windows x64).
struct IntegralInfo final
{
    std::string_view key;
    unsigned width;
    bool isSigned;
};

constexpr std::array<IntegralInfo, 11> kIntegralInfo = {{
    {"builtin:sbyte", 8, true},
    {"builtin:byte", 8, false},
    {"builtin:char", 8, false},
    {"builtin:short", 16, true},
    {"builtin:ushort", 16, false},
    {"builtin:int", 32, true},
    {"builtin:uint", 32, false},
    {"builtin:long", 64, true},
    {"builtin:ulong", 64, false},
    {"builtin:nint", 64, true},
    {"builtin:nuint", 64, false}}};

const IntegralInfo* findIntegral(std::string_view typeKey) noexcept
{
    for (const IntegralInfo& info : kIntegralInfo)
    {
        if (info.key == typeKey) return &info;
    }
    return nullptr;
}

constexpr std::array<Pair, 59> kNumericWidenings = {{
    // ID 89. `sbyte` ensancha SOLO a destinos con signo: los sin signo no pueden representar
    // sus valores negativos, y meterlos aqui haria que `uint u = miSByte;` compilara y
    // envolviera en silencio. Es la misma razon por la que `byte` no ensancha a `sbyte`: 200
    // no cabe en un entero de 8 bits con signo.
    {"builtin:sbyte", "builtin:short"}, {"builtin:sbyte", "builtin:int"},
    {"builtin:sbyte", "builtin:long"}, {"builtin:sbyte", "builtin:nint"},
    {"builtin:sbyte", "builtin:float"}, {"builtin:sbyte", "builtin:double"},
    {"builtin:byte", "builtin:short"}, {"builtin:byte", "builtin:ushort"},
    {"builtin:byte", "builtin:int"}, {"builtin:byte", "builtin:uint"},
    {"builtin:byte", "builtin:long"}, {"builtin:byte", "builtin:ulong"},
    {"builtin:byte", "builtin:nint"}, {"builtin:byte", "builtin:nuint"},
    {"builtin:byte", "builtin:float"}, {"builtin:byte", "builtin:double"},
    {"builtin:char", "builtin:ushort"}, {"builtin:char", "builtin:int"},
    {"builtin:char", "builtin:uint"}, {"builtin:char", "builtin:long"},
    {"builtin:char", "builtin:ulong"}, {"builtin:char", "builtin:nint"},
    {"builtin:char", "builtin:nuint"}, {"builtin:char", "builtin:float"},
    {"builtin:char", "builtin:double"},
    {"builtin:short", "builtin:int"}, {"builtin:short", "builtin:long"},
    {"builtin:short", "builtin:nint"}, {"builtin:short", "builtin:float"},
    {"builtin:short", "builtin:double"},
    {"builtin:ushort", "builtin:int"}, {"builtin:ushort", "builtin:uint"},
    {"builtin:ushort", "builtin:long"}, {"builtin:ushort", "builtin:ulong"},
    {"builtin:ushort", "builtin:nint"}, {"builtin:ushort", "builtin:nuint"},
    {"builtin:ushort", "builtin:float"}, {"builtin:ushort", "builtin:double"},
    {"builtin:int", "builtin:long"}, {"builtin:int", "builtin:nint"},
    {"builtin:int", "builtin:float"}, {"builtin:int", "builtin:double"},
    {"builtin:uint", "builtin:long"}, {"builtin:uint", "builtin:ulong"},
    {"builtin:uint", "builtin:nuint"}, {"builtin:uint", "builtin:float"},
    {"builtin:uint", "builtin:double"},
    {"builtin:long", "builtin:float"}, {"builtin:long", "builtin:double"},
    {"builtin:ulong", "builtin:float"}, {"builtin:ulong", "builtin:double"},
    {"builtin:nint", "builtin:long"}, {"builtin:nint", "builtin:float"},
    {"builtin:nint", "builtin:double"},
    {"builtin:nuint", "builtin:ulong"}, {"builtin:nuint", "builtin:float"},
    {"builtin:nuint", "builtin:double"},
    {"builtin:float", "builtin:double"}
}};

bool contains(std::span<const std::string_view> values, std::string_view value) noexcept
{
    for (const std::string_view candidate : values)
    {
        if (candidate == value)
        {
            return true;
        }
    }
    return false;
}

bool isIntegralPromotion(std::string_view source, std::string_view target) noexcept
{
    return target == "builtin:int" &&
        (source == "builtin:sbyte" || source == "builtin:byte" || source == "builtin:char" ||
         source == "builtin:short" || source == "builtin:ushort");
}

bool isNumericWidening(std::string_view source, std::string_view target) noexcept
{
    for (const Pair& pair : kNumericWidenings)
    {
        if (pair.first == source && pair.second == target)
        {
            return true;
        }
    }
    return false;
}

bool constantFits(
    std::uint64_t value, bool sourceUnsigned, bool sourceNegative, std::string_view target) noexcept
{
    // ID 119. Un literal NEGATIVO. Antes no llegaba nunca aqui, porque el analisis de `-` no
    // propagaba la constancia del operando: `short s = -3;` no compilaba y `short s = 3;` si.
    //
    // Cabe solo en un destino CON SIGNO, y cabe hasta la MAGNITUD DEL MINIMO, que es uno mas que
    // el maximo -- `short` llega a -32768 aunque su maximo sea 32767. `byte`, `char`, `ushort`,
    // `uint`, `ulong` y `nuint` no tienen signo, asi que ningun negativo cabe en ellos, que es lo
    // que hace que `byte b = -1;` siga rechazandose.
    if (sourceNegative)
    {
        // ID 89. `sbyte` llega hasta -128, uno mas que su maximo 127, igual que `short` llega a
        // -32768 con maximo 32767. La regla es la misma para los cuatro anchos con signo.
        if (target == "builtin:sbyte")
        {
            return value <=
                static_cast<std::uint64_t>(std::numeric_limits<std::int8_t>::max()) + 1U;
        }
        if (target == "builtin:short")
        {
            return value <= static_cast<std::uint64_t>(std::numeric_limits<std::int16_t>::max()) + 1U;
        }
        if (target == "builtin:int")
        {
            return value <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) + 1U;
        }
        if (target == "builtin:long" || target == "builtin:nint")
        {
            return value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1ULL;
        }
        return false;
    }

    // ID 89. Un byte CON signo llega a 127, no a 255. Si esta rama faltara, `sbyte s = 200;`
    // caeria al `return false` del final y el mensaje diria que no hay conversion en vez de que
    // el valor no cabe; y si dijera 255, `sbyte s = 200;` compilaria y valdria -56 en silencio.
    if (target == "builtin:sbyte")
    {
        return value <= static_cast<std::uint64_t>(std::numeric_limits<std::int8_t>::max());
    }
    if (target == "builtin:byte")
    {
        return value <= std::numeric_limits<std::uint8_t>::max();
    }
    // CC1-100 "char es unsigned de 8 bits". `builtin:char` estaba agrupado aqui con
    // `builtin:ushort` y aceptaba hasta 65535, mientras TODAS las demas capas del compilador lo
    // tratan como un byte: `llvmType` lo mapea a i8, la ABI lo pasa como i8, el parser lee un
    // literal de caracter de un byte y el modelo de layout le da tamano 1.
    //
    // La diferencia no era cosmetica. Un `switch` sobre un `char` aceptaba `case 65:` y `case 321:`
    // como etiquetas distintas -- 321 <= 65535, asi que la conversion constante decia que si -- y
    // el backend materializaba las dos como `i8 65`, entregandole a LLVM dos valores de caso
    // identicos en un mismo switch, que es comportamiento indefinido. El chequeo de duplicados
    // existe precisamente para impedir eso, y esta linea era el agujero debajo.
    //
    // El `char` de NativeSharp mide un byte hoy. Cuando pase a UTF-16, esta linea se mueve CON el
    // layout, no antes.
    if (target == "builtin:char")
    {
        return value <= std::numeric_limits<std::uint8_t>::max();
    }
    if (target == "builtin:ushort")
    {
        return value <= std::numeric_limits<std::uint16_t>::max();
    }
    if (target == "builtin:short")
    {
        return value <= static_cast<std::uint64_t>(std::numeric_limits<std::int16_t>::max());
    }
    if (target == "builtin:int")
    {
        return value <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
    }
    if (target == "builtin:uint")
    {
        return value <= std::numeric_limits<std::uint32_t>::max();
    }
    if (target == "builtin:long" || target == "builtin:nint")
    {
        return sourceUnsigned
            ? value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
            : value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    }
    if (target == "builtin:ulong" || target == "builtin:nuint")
    {
        return true;
    }
    return false;
}

// ID 121. LA PROMOCION ENTERA BINARIA. Devuelve el tipo en el que C# evalua `a OP b` cuando los
// dos son integrales, o nullopt si alguno no lo es.
//
// EL DEFECTO QUE CIERRA, medido: `commonNumericType("builtin:int", "builtin:long", const, const)`
// devolvia **builtin:double**. Las dos conversiones unidireccionales existen (int->long por
// ensanchamiento, long->int por constante que cabe), asi que ninguna de las dos reglas de arriba
// dispara; la del ID 120 exige origen `builtin:int` y destino MAS ESTRECHO que int, y `long` no lo
// es; y el bucle de preferencia esta ordenado de mas ancho a mas estrecho, asi que su primer
// candidato es `double` y los dos operandos llegan a el. Aguas abajo el evaluador entero ve que el
// tipo comun no es integral y se rinde: `const long x = 1 + 1L;` NO SE PLIEGA.
//
// Las reglas, en este orden (C# 12.4.7 restringido a los tipos que existen aqui):
//
//   alguno es ulong/nuint                     -> ulong
//   alguno es de 64 bits                      -> long
//   alguno es uint y el otro tiene signo      -> long
//   alguno es uint                            -> uint
//   en otro caso                              -> int
//
// El caso `ulong` contra un operando CON SIGNO es error en C#. Aqui no se decide en esta funcion:
// se devuelve `ulong` y el LLAMADOR comprueba que los dos operandos convierten de verdad a el. Con
// un `long` no constante esa comprobacion falla -- no hay ensanchamiento long->ulong -- y la
// expresion sigue exactamente el camino que seguia antes. Con `3L + 5UL` la conversion por
// constante SI existe y el tipo comun es `ulong`, que es lo util y lo que la fixture c17 fija.
std::optional<std::string> integerPromotion(std::string_view left, std::string_view right) noexcept
{
    const IntegralInfo* a = findIntegral(left);
    const IntegralInfo* b = findIntegral(right);
    if (a == nullptr || b == nullptr) return std::nullopt;

    if ((a->width == 64 && !a->isSigned) || (b->width == 64 && !b->isSigned))
    {
        return std::string("builtin:ulong");
    }
    if (a->width == 64 || b->width == 64)
    {
        return std::string("builtin:long");
    }

    const bool leftIsUint = a->width == 32 && !a->isSigned;
    const bool rightIsUint = b->width == 32 && !b->isSigned;
    if ((leftIsUint && b->isSigned) || (rightIsUint && a->isSigned))
    {
        return std::string("builtin:long");
    }
    if (leftIsUint || rightIsUint)
    {
        return std::string("builtin:uint");
    }
    return std::string("builtin:int");
}

ProjectImplicitConversion makeConversion(
    ProjectConversionKind kind,
    int rank,
    std::string_view source,
    std::string_view target)
{
    return ProjectImplicitConversion{kind, rank, std::string(source), std::string(target), std::nullopt};
}
}

bool ProjectImplicitConversion::exists() const noexcept
{
    return kind != ProjectConversionKind::None;
}

ProjectImplicitConversion ProjectTypeConversion::classifyImplicit(
    std::string_view sourceTypeKey,
    std::string_view targetTypeKey,
    std::optional<ProjectIntegerConstant> integerConstant,
    bool targetIsClassReference)
{
    if (sourceTypeKey.empty() || targetTypeKey.empty() ||
        sourceTypeKey == "<error>" || targetTypeKey == "<error>")
    {
        return makeConversion(ProjectConversionKind::None, 1000, sourceTypeKey, targetTypeKey);
    }

    if (sourceTypeKey == targetTypeKey)
    {
        return makeConversion(ProjectConversionKind::Identity, 0, sourceTypeKey, targetTypeKey);
    }

    if (isIntegralPromotion(sourceTypeKey, targetTypeKey))
    {
        return makeConversion(ProjectConversionKind::IntegralPromotion, 1, sourceTypeKey, targetTypeKey);
    }

    if (isNumericWidening(sourceTypeKey, targetTypeKey))
    {
        return makeConversion(ProjectConversionKind::Numeric, 3, sourceTypeKey, targetTypeKey);
    }

    if (integerConstant.has_value() && isIntegral(targetTypeKey) &&
        constantFits(
            integerConstant->value,
            integerConstant->isUnsigned,
            integerConstant->isNegative,
            targetTypeKey))
    {
        return makeConversion(ProjectConversionKind::ConstantInteger, 2, sourceTypeKey, targetTypeKey);
    }

    if (sourceTypeKey == "builtin:null" && isPointer(targetTypeKey))
    {
        return makeConversion(ProjectConversionKind::NullPointer, 4, sourceTypeKey, targetTypeKey);
    }

    if (sourceTypeKey == "builtin:null" &&
        (targetIsClassReference || isNativeCoreReference(targetTypeKey)))
    {
        return makeConversion(ProjectConversionKind::NullReference, 4, sourceTypeKey, targetTypeKey);
    }

    if (isPointer(sourceTypeKey) && targetTypeKey == "builtin:void*")
    {
        return makeConversion(ProjectConversionKind::PointerToVoid, 4, sourceTypeKey, targetTypeKey);
    }

    if (sourceTypeKey == "string-literal" && targetTypeKey == "builtin:char*")
    {
        return makeConversion(
            ProjectConversionKind::StringLiteralToCharPointer,
            4,
            sourceTypeKey,
            targetTypeKey);
    }

    if (sourceTypeKey == "string-literal" && targetTypeKey == "builtin:string")
    {
        return makeConversion(
            ProjectConversionKind::StringLiteralToNativeString,
            4,
            sourceTypeKey,
            targetTypeKey);
    }

    return makeConversion(ProjectConversionKind::None, 1000, sourceTypeKey, targetTypeKey);
}

std::optional<std::string> ProjectTypeConversion::commonNumericType(
    std::string_view leftTypeKey,
    std::string_view rightTypeKey,
    std::optional<ProjectIntegerConstant> leftIntegerConstant,
    std::optional<ProjectIntegerConstant> rightIntegerConstant)
{
    if (!isNumeric(leftTypeKey) || !isNumeric(rightTypeKey) ||
        leftTypeKey == "builtin:bool" || rightTypeKey == "builtin:bool")
    {
        return std::nullopt;
    }
    if (leftTypeKey == rightTypeKey)
    {
        return std::string(leftTypeKey);
    }

    const ProjectImplicitConversion leftToRight = classifyImplicit(
        leftTypeKey, rightTypeKey, leftIntegerConstant);
    const ProjectImplicitConversion rightToLeft = classifyImplicit(
        rightTypeKey, leftTypeKey, rightIntegerConstant);
    if (leftToRight.exists() && !rightToLeft.exists())
    {
        return std::string(rightTypeKey);
    }
    if (rightToLeft.exists() && !leftToRight.exists())
    {
        return std::string(leftTypeKey);
    }

    // ID 120. EL OPERANDO CONSTANTE SE ADAPTA AL OTRO, NO AL REVES.
    //
    // MEDIDO, no supuesto, compilando este archivo solo y llamando a esta funcion:
    // `commonNumericType("builtin:short", "builtin:int", -, const 3)` devolvia **builtin:double**.
    // La razon: con una constante que cabe, las conversiones existen EN LAS DOS DIRECCIONES, asi
    // que ninguna de las dos reglas de arriba dispara y se caia al bucle de preferencia, que esta
    // ordenado de mas ancho a mas estrecho y por tanto acierta `double` siempre. Consecuencia:
    // `short s; short t = s + 3;` no compilaba, y `byte b; b = b + 1;` tampoco -- la suma daba
    // `double` y despues no habia conversion de vuelta.
    //
    // La regla correcta es la de C#: una expresion constante que cabe en el tipo del otro operando
    // TOMA ese tipo. Se comprueba antes del bucle de preferencia, que a partir de aqui solo
    // gobierna los casos sin constante de por medio.
    // ESTRECHADA A PROPOSITO, y esto es lo que la hace segura. La primera version decia "si un
    // lado convierte por constante, gana el otro", y MEDIDO sobre el evaluador de constantes eso
    // rompia `(-1) + 1L`: el `1L` es explicitamente `long`, la conversion long->int por constante
    // existe, y la regla ancha lo habria evaluado en `int`. Un literal que la persona escribio
    // ANCHO no debe estrecharse en silencio.
    //
    // La regla de C# es mas concreta: la conversion implicita de expresion constante va de un
    // literal `int` -- el tipo por defecto de un literal entero -- a un tipo MAS ESTRECHO que int.
    // Eso es exactamente lo que hace falta para `short s; s + 3` y `byte b; b + 1`, que es lo que
    // estaba roto, y no toca nada mas.
    const auto adaptsFromDefaultLiteral =
        [](const ProjectImplicitConversion& conversion, std::string_view target) noexcept
    {
        // ID 89: `sbyte` es un destino MAS ESTRECHO QUE INT y por tanto pertenece a esta lista.
        // Sin el, `sbyte s = 10; sbyte t = s + 3;` no compilaria mientras que la misma linea con
        // `short` si -- medido con el probe de este stage, que devolvia `int` para el par
        // (sbyte, const 3). Un tipo nuevo tiene que entrar en TODAS las reglas que nombran a la
        // familia de los estrechos, no solo en las tablas de ancho y signo.
        return conversion.kind == ProjectConversionKind::ConstantInteger &&
            conversion.sourceTypeKey == "builtin:int" &&
            (target == "builtin:sbyte" || target == "builtin:byte" || target == "builtin:char" ||
             target == "builtin:short" || target == "builtin:ushort");
    };
    if (adaptsFromDefaultLiteral(rightToLeft, leftTypeKey))
    {
        return std::string(leftTypeKey);
    }
    if (adaptsFromDefaultLiteral(leftToRight, rightTypeKey))
    {
        return std::string(rightTypeKey);
    }

    // ID 121. VA AQUI, y el sitio es la mitad de la correccion.
    //
    // DESPUES de la regla del ID 120: si fuera antes, `short s; s + 3` pasaria a evaluarse en `int`
    // y `short t = s + 3;` volveria a no compilar -- exactamente el defecto que el 120 cerro, con
    // su razon escrita justo arriba. Esa divergencia respecto a C# es deliberada y esta regla no
    // la revierte.
    //
    // ANTES del bucle de preferencia: el bucle esta ordenado de mas ancho a mas estrecho y su
    // primera parada es `double`, asi que cualquier par de integrales que llegue ahi sale como
    // punto flotante. Esta regla contesta primero para los pares integrales y le deja al bucle lo
    // que de verdad es suyo: los pares donde interviene `float` o `double`.
    //
    // LA GUARDA NO ES CEREMONIA. `integerPromotion` da el tipo que C# usaria; las dos llamadas a
    // `classifyImplicit` comprueban que los dos operandos LLEGAN a el de verdad con las
    // conversiones que este compilador tiene. Si no llegan -- `long` no constante contra `ulong`,
    // por ejemplo -- se cae al bucle de preferencia y la expresion se comporta como se comportaba
    // antes de este cambio. La regla solo AÑADE respuestas donde no habia ninguna correcta.
    if (const std::optional<std::string> promoted = integerPromotion(leftTypeKey, rightTypeKey);
        promoted.has_value() &&
        classifyImplicit(leftTypeKey, *promoted, leftIntegerConstant).exists() &&
        classifyImplicit(rightTypeKey, *promoted, rightIntegerConstant).exists())
    {
        return promoted;
    }

    // ID 89. `sbyte` va AL FINAL, y es la posicion mas conservadora que existe: el bucle
    // devuelve el PRIMER candidato al que llegan los dos operandos, asi que anadir una entrada al
    // final no puede cambiar ninguna respuesta que la tabla ya daba. Solo contesta donde antes se
    // devolvia nullopt.
    constexpr std::array<std::string_view, 13> preference = {
        "builtin:double", "builtin:float", "builtin:ulong", "builtin:long",
        "builtin:nuint", "builtin:nint", "builtin:uint", "builtin:int",
        "builtin:ushort", "builtin:short", "builtin:char", "builtin:byte",
        "builtin:sbyte"};
    for (const std::string_view candidate : preference)
    {
        if (classifyImplicit(leftTypeKey, candidate, leftIntegerConstant).exists() &&
            classifyImplicit(rightTypeKey, candidate, rightIntegerConstant).exists())
        {
            return std::string(candidate);
        }
    }
    return std::nullopt;
}

bool ProjectTypeConversion::isNumeric(std::string_view typeKey) noexcept
{
    return contains(kNumericTypes, typeKey);
}

bool ProjectTypeConversion::isIntegral(std::string_view typeKey) noexcept
{
    return contains(kIntegralTypes, typeKey);
}

bool ProjectTypeConversion::isPointer(std::string_view typeKey) noexcept
{
    return !typeKey.empty() && typeKey.back() == '*';
}

bool ProjectTypeConversion::isNativeString(std::string_view typeKey) noexcept
{
    return typeKey == "builtin:string";
}

bool ProjectTypeConversion::isNativeArray(std::string_view typeKey) noexcept
{
    return typeKey.size() >= 2 && typeKey.ends_with("[]");
}

bool ProjectTypeConversion::isNativeCoreReference(std::string_view typeKey) noexcept
{
    return isNativeString(typeKey) || isNativeArray(typeKey);
}

std::optional<std::string> ProjectTypeConversion::nativeArrayElementType(std::string_view typeKey)
{
    if (!isNativeArray(typeKey)) return std::nullopt;
    return std::string(typeKey.substr(0, typeKey.size() - 2));
}

const char* projectConversionKindName(ProjectConversionKind kind) noexcept
{
    switch (kind)
    {
        case ProjectConversionKind::None: return "none";
        case ProjectConversionKind::Identity: return "identity";
        case ProjectConversionKind::IntegralPromotion: return "integral-promotion";
        case ProjectConversionKind::ConstantInteger: return "constant-integer";
        case ProjectConversionKind::Numeric: return "numeric";
        case ProjectConversionKind::NullPointer: return "null-pointer";
        case ProjectConversionKind::NullReference: return "null-reference";
        case ProjectConversionKind::ReferenceUpcast: return "reference-upcast";
        case ProjectConversionKind::ReferenceInterface: return "reference-interface";
        case ProjectConversionKind::PointerToVoid: return "pointer-to-void";
        case ProjectConversionKind::StringLiteralToCharPointer: return "string-literal-to-char-pointer";
        case ProjectConversionKind::StringLiteralToNativeString: return "string-literal-to-native-string";
        case ProjectConversionKind::UserDefinedImplicit: return "user-defined-implicit";
        case ProjectConversionKind::UserDefinedExplicit: return "user-defined-explicit";
    }
    return "none";
}
}
