#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include "nativesharp/Frontend/BuiltinIntegerTypes.h"
#include "nativesharp/Language/CallableSemantics.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <span>
#include <string>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
using Pair = std::pair<std::string_view, std::string_view>;

constexpr std::array<std::string_view, 15> kNumericTypes = {
    "builtin:sbyte",
    "builtin:byte", "builtin:char", "builtin:short", "builtin:ushort",
    "builtin:int", "builtin:uint", "builtin:long", "builtin:ulong",
    "builtin:nint", "builtin:nuint", "builtin:i128", "builtin:u128",
    "builtin:float", "builtin:double"};

constexpr std::array<std::string_view, 13> kIntegralTypes = {
    "builtin:sbyte",
    "builtin:byte", "builtin:char", "builtin:short", "builtin:ushort",
    "builtin:int", "builtin:uint", "builtin:long", "builtin:ulong",
    "builtin:nint", "builtin:nuint", "builtin:i128", "builtin:u128"};

// ID 121. Ancho y signo de los tipos integrales, para la regla de promocion entera de mas abajo.
//
// LA TABLA VIVE EN include/nativesharp/Frontend/BuiltinIntegerTypes.h. Estaba escrita aqui y otra
// vez en ProjectIntegerEvaluator.cpp; el comentario que las acompanaba decia que la duplicacion
// era deliberada porque este archivo no puede depender del evaluador --lo cual es CIERTO-- y que
// dos pruebas las fijaban la una contra la otra --lo cual NO lo era: ninguna comparaba las dos
// tablas--. Una cabecera de datos `constexpr` no tiene direccion de dependencia, asi que respeta
// el motivo real y elimina la copia. El detalle, en el comentario de esa cabecera.
using IntegralInfo = builtins::IntegerTypeInfo;
using builtins::findIntegerType;

const IntegralInfo* findIntegral(std::string_view typeKey) noexcept
{
    return findIntegerType(typeKey);
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
    const IntegralInfo* sourceInteger = findIntegral(source);
    const IntegralInfo* targetInteger = findIntegral(target);
    if (sourceInteger != nullptr && targetInteger != nullptr)
    {
        if (sourceInteger->isSigned == targetInteger->isSigned)
            return sourceInteger->width < targetInteger->width;
        if (!sourceInteger->isSigned && targetInteger->isSigned)
            return sourceInteger->width < targetInteger->width;
    }
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
        if (target == "builtin:i128") return true;
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
    if (target == "builtin:i128" || target == "builtin:u128") return true;
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

    // ID 126. DOS OPERANDOS DEL MISMO TIPO cuyo rango NO es menor que `int`.
    //
    // En C++ la promocion integral toca solo los rangos MENORES que int: `long + long` es `long`
    // y `size_t + size_t` es `size_t`. Aqui igual. Los cinco estrechos NO salen por aqui -- caen a
    // las reglas de abajo y acaban en `int`, que es justamente el defecto que el ID 126 cierra.
    if (left == right && !isIntegralPromotion(left, "builtin:int"))
    {
        return std::string(left);
    }
    if (a->width == 128 || b->width == 128)
    {
        return std::string((!a->isSigned || !b->isSigned)
            ? "builtin:u128" : "builtin:i128");
    }

    // `nint` y `nuint` CONSERVAN SU NOMBRE, y no es cosmetica.
    //
    // Colapsarlos a long/ulong rompe codigo que hoy compila: `nuint size = sizeof(T);
    // nuint total = size * 2;` daria `ulong`, y la vuelta `ulong -> nuint` NO ensancha, asi que la
    // asignacion dejaria de compilar. C++ contesta lo mismo que esto -- `size_t + int` es `size_t`,
    // no `unsigned long` -- porque el tipo del ancho del puntero tiene rango propio.
    //
    // Cuando estan los DOS de la misma signitud (`ulong` contra `nuint`), gana el de ancho
    // GARANTIZADO: `ulong` mide 64 bits en todo target y `nuint` no, asi que elegirlo no puede
    // perder rango en ninguna plataforma y elegir `nuint` si podria.
    if ((a->width == 64 && !a->isSigned) || (b->width == 64 && !b->isSigned))
    {
        if (left == "builtin:ulong" || right == "builtin:ulong") return std::string("builtin:ulong");
        return std::string("builtin:nuint");
    }
    if (a->width == 64 || b->width == 64)
    {
        if (left == "builtin:long" || right == "builtin:long") return std::string("builtin:long");
        return std::string("builtin:nint");
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

    // NativeSharp 3.0 T07-F04. `never` is the bottom type: a divergent expression can
    // inhabit every result context precisely because it never produces a runtime value.
    // Keep this ahead of structural conversions so a never expression does not pretend to
    // manufacture tuple elements, reference objects, or numeric bits.
    if (sourceTypeKey == "builtin:never")
    {
        return makeConversion(ProjectConversionKind::Never, 0, sourceTypeKey, targetTypeKey);
    }

    const std::optional<std::vector<std::string>> sourceTuple =
        tupleElementTypeKeys(sourceTypeKey);
    const std::optional<std::vector<std::string>> targetTuple =
        tupleElementTypeKeys(targetTypeKey);
    if (sourceTuple.has_value() || targetTuple.has_value())
    {
        if (!sourceTuple.has_value() || !targetTuple.has_value() ||
            sourceTuple->size() != targetTuple->size())
            return makeConversion(ProjectConversionKind::None, 1000, sourceTypeKey, targetTypeKey);
        int rank = 0;
        for (std::size_t index = 0; index < sourceTuple->size(); ++index)
        {
            const ProjectImplicitConversion element = classifyImplicit(
                (*sourceTuple)[index], (*targetTuple)[index]);
            if (!element.exists())
                return makeConversion(ProjectConversionKind::None, 1000, sourceTypeKey, targetTypeKey);
            rank = std::max(rank, element.rank);
        }
        return makeConversion(ProjectConversionKind::Tuple, rank + 1, sourceTypeKey, targetTypeKey);
    }

    const std::optional<language::FunctionType> sourceFunction =
        language::parseFunctionTypeIdentity(sourceTypeKey);
    const std::optional<language::FunctionType> targetFunction =
        language::parseFunctionTypeIdentity(targetTypeKey);
    const std::optional<language::AnonymousLambdaType> sourceLambda =
        language::parseAnonymousLambdaTypeIdentity(sourceTypeKey);
    if (sourceLambda.has_value() && targetFunction.has_value())
    {
        // A nominal non-capturing lambda has no runtime payload. Its one implicit materialization
        // is the address of its generated callable, and only an exactly compatible raw function
        // pointer can receive that address. A managed fn& would require allocating a delegate cell,
        // which would violate the zero-size/no-hidden-allocation contract of F02.
        if (targetFunction->carrier != language::FunctionCarrier::Pointer ||
            !language::functionTypesAssignable(sourceLambda->signature, *targetFunction))
            return makeConversion(ProjectConversionKind::None, 1000, sourceTypeKey, targetTypeKey);
        return makeConversion(ProjectConversionKind::Function, 1, sourceTypeKey, targetTypeKey);
    }
    if (sourceFunction.has_value() || targetFunction.has_value())
    {
        if (!sourceFunction.has_value() || !targetFunction.has_value() ||
            !language::functionTypesAssignable(*sourceFunction, *targetFunction))
            return makeConversion(ProjectConversionKind::None, 1000, sourceTypeKey, targetTypeKey);
        return makeConversion(ProjectConversionKind::Function, 1, sourceTypeKey, targetTypeKey);
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

    if (sourceTypeKey == "builtin:null" && isFunctionPointer(targetTypeKey))
    {
        const std::optional<language::FunctionType> target =
            language::parseFunctionTypeIdentity(targetTypeKey);
        return target.has_value() && target->nullable
            ? makeConversion(ProjectConversionKind::NullPointer, 4, sourceTypeKey, targetTypeKey)
            : makeConversion(ProjectConversionKind::None, 1000, sourceTypeKey, targetTypeKey);
    }

    if (sourceTypeKey == "builtin:null" && isFunctionReference(targetTypeKey))
    {
        const std::optional<language::FunctionType> target =
            language::parseFunctionTypeIdentity(targetTypeKey);
        return target.has_value() && target->nullable
            ? makeConversion(ProjectConversionKind::NullReference, 4, sourceTypeKey, targetTypeKey)
            : makeConversion(ProjectConversionKind::None, 1000, sourceTypeKey, targetTypeKey);
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

    // =====================================================================================
    // IDs 126 y 120 -- UN PAR DE INTEGRALES LO CONTESTA `integerPromotion`, Y NADIE MAS.
    //
    // POR QUE ESTA RAMA EXISTE, y por que va la PRIMERA. El resto de esta funcion BUSCA un tipo
    // comun: prueba conversiones en las dos direcciones y, si no concluye, recorre una lista
    // ordenada de mas ancho a mas estrecho. Buscar es lo correcto para las mezclas con punto
    // flotante y es lo que produjo TRES clases de respuesta equivocada para los enteros (medidas
    // par a par en el doc 89, 31 de los 91 pares):
    //
    //   1. El cortocircuito de tipos iguales contestaba el tipo estrecho: `byte + byte` daba
    //      `byte`, y el backend emitia un `add i8` que ENVUELVE sin diagnostico. 200+100 = 44.
    //   2. La regla "si solo una conversion existe, gana ese tipo" contestaba antes de tiempo:
    //      `byte + short` daba `short` porque byte->short ensancha y la vuelta no. Misma
    //      truncacion, a 16 bits.
    //   3. El bucle de preferencia empieza en `double`, y TODO entero convierte a `double`. Asi
    //      que `int + ulong` y `long + nuint` -- pares que C# PROHIBE -- se computaban en punto
    //      flotante: el plegado de constantes se rendia (el tipo comun no es integral) y por
    //      encima de 2^53 el resultado ni siquiera era la suma.
    //
    // LA FORMA ES LA DE C++ (§7.3.6 + §7.4): promover cada operando y aplicar reglas ORDENADAS.
    // No se busca. Y de ahi sale la propiedad que faltaba: DOS ENTEROS NUNCA DAN `double`, porque
    // el punto flotante ya no esta en el camino.
    //
    // LAS RESPUESTAS SON LAS DE C# alli donde C++ muerde, que es en un solo sitio: mezclar con
    // signo y sin signo del mismo rango. C++ colapsa al sin signo -- `int + uint` es `uint`, y por
    // eso `-1 < 1u` es false. `integerPromotion` contesta `long`, que es lo que una persona espera.
    //
    // Y CUANDO LA GUARDA FALLA, ES ERROR. No se cae al bucle. `long + ulong` sin constantes no
    // tiene tipo comun -- no hay ensanchamiento long->ulong -- y eso es exactamente lo que hay que
    // decir, en vez de contestar `double` y computar mal. Con una constante que cabe la conversion
    // SI existe y el par sigue funcionando, que es lo que fija la fixture c17 con `3L + 5UL`.
    //
    // Esta rama se traga tambien la regla del ID 120 (`adaptsFromDefaultLiteral`), que vivia mas
    // abajo y hacia que una constante literal adoptara el tipo estrecho del otro operando:
    // `short s; short t = s + 3;` compilaba. Era una divergencia deliberada de C# y se retira aqui
    // -- en C# tampoco compila, y mantenerla dejaba `b + b` como error y `b + 1` truncando en
    // silencio, que es la unica combinacion peor que no arreglar nada.
    // =====================================================================================
    if (isIntegral(leftTypeKey) && isIntegral(rightTypeKey))
    {
        // LA CONSTANTE `int` SE ADAPTA AL OTRO OPERANDO -- PERO SOLO HACIA ARRIBA.
        //
        // Es la «implicit constant expression conversion» de C#: una expresion constante de tipo
        // `int` --el tipo por defecto de un literal entero-- convierte implicitamente a cualquier
        // tipo entero en cuyo rango quepa. Aplicada al tipo comun, la constante toma el tipo del
        // otro operando. C++ llega al mismo sitio por otro camino: `unsigned int * int` colapsa al
        // sin signo.
        //
        // LA RESTRICCION «HACIA ARRIBA» ES TODA LA DIFERENCIA con la regla del ID 120 que este
        // mismo cambio retira, y las dos direcciones no son la misma cosa:
        //
        //   * hacia un tipo ESTRECHO (rango < int) la adaptacion es lo que hacia que
        //     `byte b; b + 1` se computara a 8 bits y ENVOLVIERA. Ahi manda la promocion, y por eso
        //     el ID 120 se fue.
        //   * hacia `uint`/`ulong`/`nuint`/`long`/`nint` no hay nada que truncar: el otro operando
        //     ya es igual o mas ancho que `int`. Y sin esta rama se rompe codigo real --
        //     `uint hash; hash = hash * 16777619;` en String.ns:980, y `ulong % 10` en el formateo
        //     de enteros. Lo encontro la corrida, no una lectura: el barrido previo de `lib/` busco
        //     tipos ESTRECHOS y `uint` no es uno.
        //
        // Sin ella, `integerPromotion(uint, int)` contesta `long` --correcto para dos operandos de
        // verdad, porque ni `uint` ni `int` contienen al otro-- y `long` no vuelve a `uint`.
        const auto constantAdaptsUpward = [](
            std::string_view constantTypeKey,
            const std::optional<ProjectIntegerConstant>& constant,
            std::string_view otherTypeKey) noexcept
        {
            return constantTypeKey == "builtin:int" && constant.has_value() &&
                !isIntegralPromotion(otherTypeKey, "builtin:int") &&
                classifyImplicit(constantTypeKey, otherTypeKey, constant).exists();
        };
        if (constantAdaptsUpward(leftTypeKey, leftIntegerConstant, rightTypeKey))
        {
            return std::string(rightTypeKey);
        }
        if (constantAdaptsUpward(rightTypeKey, rightIntegerConstant, leftTypeKey))
        {
            return std::string(leftTypeKey);
        }

        const std::optional<std::string> promoted = integerPromotion(leftTypeKey, rightTypeKey);
        if (!promoted.has_value())
        {
            return std::nullopt;
        }
        const auto bothReach = [&](std::string_view candidate) noexcept
        {
            return classifyImplicit(leftTypeKey, candidate, leftIntegerConstant).exists() &&
                classifyImplicit(rightTypeKey, candidate, rightIntegerConstant).exists();
        };
        if (bothReach(*promoted))
        {
            return promoted;
        }
        // EL TIPO NATIVO SE PREFIERE, PERO NO SE IMPONE. `integerPromotion` contesta `nint`/`nuint`
        // cuando uno de los operandos lo es, porque colapsarlos rompe `nuint total = size * 2`.
        // Pero no todo operando LLEGA a un tipo nativo: `uint -> nint` no ensancha, y con razon --
        // `nint` puede medir 32 bits en otro target y un `uint` no cabria. Ahi el tipo comun
        // honesto es el de ancho fijo, al que los dos SI ensanchan (`uint -> long`, `nint -> long`),
        // y no un error.
        //
        // MEDIDO: sin este paso, `uint + nint` --que hoy compila y da `long`-- dejaba de compilar.
        // Fue la unica regresion que la tabla de 91 pares encontro en la primera version de este
        // cambio, y la encontro la tabla, no una lectura.
        if (*promoted == "builtin:nint" && bothReach("builtin:long"))
        {
            return std::string("builtin:long");
        }
        if (*promoted == "builtin:nuint" && bothReach("builtin:ulong"))
        {
            return std::string("builtin:ulong");
        }
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

    // Lo que queda de aqui abajo gobierna SOLO los pares donde interviene punto flotante (o un
    // tipo numerico que no es integral): los pares de integrales ya contestaron arriba y no
    // llegan. La regla del ID 120 --`adaptsFromDefaultLiteral`, que hacia que una constante
    // literal adoptara el tipo estrecho del otro operando-- vivia aqui y se retiro con el ID 126:
    // era una divergencia deliberada de C# y ahora seria inalcanzable, porque su unico caso de uso
    // era un par de integrales.
    //
    // La llamada a `integerPromotion` que estaba en este punto tambien se fue, por lo mismo: era
    // el ID 121, y ahora es lo PRIMERO que se pregunta en vez de lo penultimo.

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
    return (!typeKey.empty() && typeKey.back() == '*') || isFunctionPointer(typeKey);
}

bool ProjectTypeConversion::isFunctionPointer(std::string_view typeKey) noexcept
{
    const std::optional<language::FunctionType> type =
        language::parseFunctionTypeIdentity(typeKey);
    return type.has_value() && type->carrier == language::FunctionCarrier::Pointer;
}

bool ProjectTypeConversion::isFunctionReference(std::string_view typeKey) noexcept
{
    const std::optional<language::FunctionType> type =
        language::parseFunctionTypeIdentity(typeKey);
    return type.has_value() && type->carrier == language::FunctionCarrier::Reference;
}

bool ProjectTypeConversion::isNativeString(std::string_view typeKey) noexcept
{
    return typeKey == "builtin:string";
}

bool ProjectTypeConversion::isNativeArray(std::string_view typeKey) noexcept
{
    return (typeKey.size() >= 2 && typeKey.ends_with("[]")) || isNativeRangeArray(typeKey);
}

bool ProjectTypeConversion::isNativeRangeArray(std::string_view typeKey) noexcept
{
    if (typeKey.size() < 4 || typeKey.back() != ']') return false;
    const std::size_t open = typeKey.rfind('[');
    if (open == std::string_view::npos || open == 0 || open + 2 >= typeKey.size()) return false;
    for (std::size_t index = open + 1; index + 1 < typeKey.size(); ++index)
        if (typeKey[index] != ',') return false;
    return true;
}

std::size_t ProjectTypeConversion::nativeArrayRank(std::string_view typeKey) noexcept
{
    if (typeKey.size() >= 2 && typeKey.ends_with("[]")) return 1;
    if (!isNativeRangeArray(typeKey)) return 0;
    const std::size_t open = typeKey.rfind('[');
    return typeKey.size() - open - 1;
}

bool ProjectTypeConversion::isNativeCoreReference(std::string_view typeKey) noexcept
{
    return isNativeString(typeKey) || isNativeArray(typeKey);
}

std::optional<std::string> ProjectTypeConversion::nativeArrayElementType(std::string_view typeKey)
{
    if (!isNativeArray(typeKey)) return std::nullopt;
    return std::string(typeKey.substr(0, typeKey.rfind('[')));
}

bool ProjectTypeConversion::isTuple(std::string_view typeKey) noexcept
{
    return typeKey.starts_with("tuple:(") && typeKey.ends_with(')');
}

std::string ProjectTypeConversion::tupleTypeKey(std::span<const std::string> elementTypeKeys)
{
    std::string result = "tuple:(";
    for (std::size_t index = 0; index < elementTypeKeys.size(); ++index)
    {
        if (index != 0) result.push_back(',');
        result += std::to_string(elementTypeKeys[index].size());
        result.push_back(':');
        result += elementTypeKeys[index];
    }
    result.push_back(')');
    return result;
}

std::optional<std::vector<std::string>> ProjectTypeConversion::tupleElementTypeKeys(
    std::string_view typeKey)
{
    if (!isTuple(typeKey)) return std::nullopt;
    std::vector<std::string> result;
    std::size_t cursor = 7; // immediately after "tuple:("
    const std::size_t close = typeKey.size() - 1;
    while (cursor < close)
    {
        std::size_t length = 0;
        const char* first = typeKey.data() + cursor;
        const char* last = typeKey.data() + close;
        const auto parsed = std::from_chars(first, last, length);
        if (parsed.ec != std::errc{} || parsed.ptr == first || parsed.ptr == last ||
            *parsed.ptr != ':') return std::nullopt;
        cursor = static_cast<std::size_t>(parsed.ptr - typeKey.data()) + 1;
        if (length == 0 || cursor + length > close) return std::nullopt;
        result.emplace_back(typeKey.substr(cursor, length));
        cursor += length;
        if (cursor == close) break;
        if (typeKey[cursor] != ',') return std::nullopt;
        ++cursor;
    }
    // Arity is part of the structural identity, including the unit tuple `()` and the
    // single-element tuple `(T,)`.  The trailing comma belongs only to source spelling; the
    // length-prefixed key remains unambiguous without storing punctuation.
    return result;
}

const char* projectConversionKindName(ProjectConversionKind kind) noexcept
{
    switch (kind)
    {
        case ProjectConversionKind::None: return "none";
        case ProjectConversionKind::Identity: return "identity";
        case ProjectConversionKind::Never: return "never";
        case ProjectConversionKind::IntegralPromotion: return "integral-promotion";
        case ProjectConversionKind::ConstantInteger: return "constant-integer";
        case ProjectConversionKind::Numeric: return "numeric";
        case ProjectConversionKind::Tuple: return "tuple";
        case ProjectConversionKind::Function: return "function";
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
