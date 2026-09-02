# 51 — NativeSharp Complete String

## 0. Historial de esta entrega (por qué este documento tiene dos capas)

Esta es la versión de cierre. La primera entrega de Stage 51 (61 overloads públicos / 47 familias) fue revisada por Alberto contra el ZIP real y encontró tres cosas ciertas: (1) el nombre "Complete String" quedaba grande frente al alcance real entregado, (2) el gap de enums en el pipeline OOP (documentado en la sección 4 original) era, en sus palabras, "un bloqueo de Stage 51" y no algo para esquivar permanentemente, y (3) la superficie declarada como imposible (`Split`/`RSplit`/`Partition`/`RPartition`) no se había verificado con suficiente insistencia contra el compilador real antes de descartarla. Los tres puntos eran correctos. Esta revisión los resuelve: el gap de enums está corregido de punta a punta (sección 4), la superficie de Split se re-investigó, se encontró un patrón real que sí funciona, y se implementó (sección 6.1), y el conteo final es 90 overloads públicos / 69 familias. Se conserva el análisis original de la sección 4 y 5 tal cual se escribió — es la bitácora real de cómo se llegó al fix, no un resumen retroactivo.

## 1. Objetivo de Stage 51

Stage 51 amplía `NativeSharp.Core.String` de 9 métodos (Stage 50: `Concat`, `Equals`, `Substring`, `FromCharArray`, `ToString(string)`, `Copy`, `IsNullOrEmpty`, `Contains`, `IndexOf`) a una superficie que combina la nomenclatura C# (`System.String`, PascalCase) con idiomas útiles de Python (`Trim`/`Strip`, `PadLeft`/`LJust`, `ToLower`/`CaseFold`, predicados `IsAlpha`/`IsDigit`/...), manteniendo la representación física nativa establecida desde Stage 41: string es longitud explícita + bytes, sin CLR, sin GC, sin runtime administrado.

No se introduce ningún mecanismo intrínseco nuevo en el compilador para la superficie de `String.ns` misma. Los únicos cuatro métodos con lowering nativo directo siguen siendo `Concat`, `Equals(string,string)`, `Substring`, `FromCharArray`. Todo lo demás — 85 métodos públicos más — se construye por composición sobre esos cuatro primitivos. Lo que SÍ cambió en el compilador esta vuelta es el pipeline OOP en sí (soporte de enums, sección 4) — no como parche dentro de `String.ns`, sino como capacidad real del lenguaje que `String.ns` es el primer consumidor en usar.

## 2. Punto de partida verificado

Antes de escribir una sola línea se releyó el snapshot que Alberto entregó (`NativeSharp_Stage51_Current_Code_and_Handover.zip`) contra el Stage 50 FULL certificado, con `diff -rq` real, no manifest. El delta reportado en el handover coincidió exactamente con el código: 5 `.cpp` + 5 `.h` modificados (el fix de promoción entera `ulong / 10`, ver sección 5) y `lib/NativeSharp.Core/String.ns` como archivo nuevo con exactamente los 9 métodos declarados, cuerpos intrínsecos como placeholder. `tests/` y `CMakeLists.txt` eran idénticos a Stage 50 — cero suite Stage 51 registrada todavía. Ese fue el punto de partida real, no lo narrado de memoria.

## 3. Disciplina de ownership: verificada contra el compilador, no asumida

Antes de escribir la superficie completa se probaron construcciones aisladas contra `nsc` real (`--check-project-method-bodies`, `--verify-project-oop-llvm`) para no adivinar reglas de un sistema de ownership que ya es deliberadamente estricto (Stage 50). Los hallazgos, todos reproducibles:

```text
owned string x = expr;          ERROR — un local no se declara "owned" a secas
raii string x = expr;           OK    — la única forma correcta de recibir un valor owned

raii string a = "literal";      ERROR — raii solo se inicializa con new/null/move/llamada owned
raii string a = String.Copy("literal");   OK

Concat(ownedCallResult, x)      ERROR — "ownership-transfer argument requires an owned parameter"
raii string t = ownedCallResult;
Concat(t, x)                    OK    — un raii local se lee como argumento prestado sin problema

a = String.Concat(a, b);        ERROR — "RAII locals are move-only; assignment requires target = move(source)"
raii string next = String.Concat(a, b);
a = move(next);                 OK    — reasignar exige pasar por un raii local nuevo + move()

return someRaiiLocal;           ERROR — "returning RAII owner requires return move(owner)"
return move(someRaiiLocal);     OK
return String.Concat(a, b);     OK    — retornar una llamada fresca (no nombrada) no exige move()

destination[i] = ownedCallResult();     OK — SI se puede asignar un valor owned directamente a un
                                         elemento de un arreglo prestado (base del patron de Split,
                                         ver seccion 6.1)
delete destination[i]; delete[] destination;   OK — pero hace falta destruir cada elemento owned
                                         ANTES de delete[] el arreglo -- delete[] no llama
                                         destructores de elementos (ver seccion 6.1)
```

Estas reglas gobiernan cada método nuevo del archivo. El patrón de acumulador dentro de un `while` (usado en `Replace(string,string,string)` y `Join`) quedó verificado de punta a punta — semántica, HIR, MIR y LLVM — antes de usarse en el archivo real.

## 4. Los enums ya funcionan en el pipeline OOP (corregido esta vuelta)

### 4.1 El hallazgo original

Al diseñar los overloads con `StringComparison` apareció un error de compilación reproducible con un repro mínimo de tres líneas:

```csharp
public enum Mode { A = 0, B = 1 }
public class User
{
    public static bool Check(Mode value) => value == Mode.A; // no compilaba
}
```

```text
error: unknown identifier 'Mode' in callable/initializer context
```

El rodeo obvio — castear a `int` — tampoco funcionaba: fallaba en LLVM ("unsupported Stage 34 LLVM conversion from 'type:...Mode' to 'builtin:int'"). Los enums no estaban cableados de punta a punta en el pipeline OOP, ni por acceso calificado ni por cast explícito, ni por comparación de igualdad.

### 4.2 Las cuatro causas reales, localizadas en código

No era un solo punto de falla — eran cuatro capas independientes, cada una descubierta al corregir la anterior y volver a probar:

1. **`ProjectMemberModel::build`** (`src/Frontend/ProjectMemberModel.cpp`) nunca procesaba tipos `NamedTypeDeclarationKind::Enum` en absoluto — el filtro solo aceptaba `Class`/`Interface`/`Struct`. Los miembros de un enum (`EnumMemberDeclaration`) nunca se registraban como símbolos de ningún tipo.
2. **`ProjectMethodBodyModel::resolveTypeIdentifier`** (línea ~5984) solo aceptaba `Class`/`Struct` como receptor de tipo para `Tipo.Miembro`.
3. Un segundo gate en `analyzeMember` (~línea 3919) validaba el receptor de un acceso a miembro contra `isClass`/`isStruct`/`isInterface` únicamente — un enum pasaba el primer gate (una vez corregido) pero se rechazaba aquí.
4. **`convert()`** en `ProjectOopLlvmLowerer.cpp` (el lowering de casts explícitos) no reconocía una clave de tipo `"type:...NombreEnum"` como entero de 32 bits — por eso el cast a `int` fallaba incluso después de arreglar el acceso calificado.

### 4.3 La solución: reusar Stage 37, no inventar mecanismo nuevo

Cada miembro de enum se registra como un `ClassMemberSymbol` de tipo `Field`, `isStatic = true`, `isConst = true`, con su **tipo declarado siendo el propio enum** (no `int` — así `Mode.Second` tipa como `Mode` y compara correctamente contra otro `Mode`, igual que en C# real) y un nuevo campo `isEnumConstant` + `enumConstantValue` precalculado (auto-incremento desde 0, o literal entero explícito — con signo, verificado en rango `int32`; un inicializador que no sea un literal entero simple se rechaza con diagnóstico en vez de evaluarse mal). Ese registro reutiliza el 100% del mecanismo de campos `static const` que ya existía desde Stage 37 (constant-folding en la construcción de MIR, materialización como global LLVM `constant i32` sin ningún costo de inicialización en runtime) — la única pieza nueva es sintetizar directamente el nodo HIR `IntegerConstant` con el valor ya calculado, en vez de bajar una expresión de inicializador real (que no existe para el caso auto-incremento).

Los tres gates de arriba se corrigieron con el mismo criterio en cada uno: aceptar `Enum` donde antes solo se aceptaba `Class`/`Struct`, sin tocar el comportamiento existente para esos dos casos. Confirmado con un repro real hasta objeto COFF:

```csharp
public enum Mode { First = 0, Second = 1, Third }
class Checker
{
    public static bool IsSecond(Mode value) { return value == Mode.Second; }
    public static int Run()
    {
        Mode m = Mode.Second;
        if (!IsSecond(m)) { return 1; }
        if (IsSecond(Mode.First)) { return 2; }
        if ((int)Mode.Third != 2) { return 3; }
        return 0;
    }
}
```

compila limpio hasta LLVM verificado y objeto COFF real, con `Mode.First`/`Second`/`Third` como globales `constant i32 0`/`1`/`2` reales en el IR emitido. Test dedicado: `tests/unit/Stage51EnumOopSupportTests.cpp`, 24 checks — registro de miembros (auto-incremento y literal explícito), uso de punta a punta (acceso calificado, igualdad, paso por parámetro, cast explícito, hasta COFF), y guardas de regresión (nombre de miembro duplicado e inicializador no-literal siguen rechazándose).

### 4.4 Consecuencia directa: `StringComparison` real en `String.ns`

Con el gap corregido, `Equals`, `Contains`, `IndexOf`, `StartsWith`, `EndsWith` ahora tienen un overload real con `StringComparison comparison` (además del `bool ignoreCase` existente — ambos son API válida, `System.String.Equals` real también tiene los dos). El overload de enum delega en el de `bool`:

```csharp
public static bool Equals(string left, string right, StringComparison comparison)
{
    return Equals(left, right, comparison == StringComparison.OrdinalIgnoreCase);
}
```

y funciona con la fachada de instancia existente: `text.Equals(other, StringComparison.OrdinalIgnoreCase)` compila, verifica LLVM y emite un `.exe` PE32+ real (confirmado con el ejemplo empaquetado).

## 5. El fix de promoción entera (`ulong / 10`)

Ya presente en el snapshot que Alberto entregó, verificado aquí con lectura de código y con un test de regresión nuevo. El mecanismo:

`ProjectTypeConversion::commonNumericType` intenta primero una dirección asimétrica antes de caer a la tabla de preferencia (`double, float, ulong, long, ...`): si `int-literal-10 → ulong` es válido vía `ConstantInteger` (rank 2, gracias a que `ProjectIntegerConstant{value, isUnsigned}` ahora se construye en `IntegerLiteralExpression` y se propaga por más de veinte sitios de la expresión binaria/argumentos/retornos) pero `ulong → int` no lo es, devuelve `ulong` directamente — sin llegar nunca a la lista de preferencia, donde `double` habría ganado por ser la primera candidata compatible con ambos lados. En LLVM, `operandType` decide `udiv`/`urem` en vez de `fdiv`/`frem` cuando no es entero con signo.

El test `tests/unit/Stage51CompleteStringTests.cpp::testUlongDivisionStaysIntegral` fija esto: compila `ulong quotient = dividend / 10;` y verifica el `typeKey` del local (`builtin:ulong`, no `builtin:double`) y que el LLVM emitido contenga `udiv`/`urem` y no contenga `fdiv`/`sitofp`/`uitofp`. Un caso de control (`testMixedDoubleStillPromotes`) confirma que la promoción a `double` sigue siendo correcta cuando el otro operando es de verdad `double`.

## 6. Superficie completa (90 overloads públicos, 69 familias)

```text
Búsqueda       StartsWith, EndsWith, LastIndexOf, IndexOfAny, LastIndexOfAny,
               Contains, IndexOf, Equals   (cada uno con overload bool ignoreCase
               Y overload StringComparison real, ver seccion 4.4)
Edición        Insert, Remove(int)/Remove(int,int), Replace(char,char),
               Replace(string,string,string), RemovePrefix, RemoveSuffix
Split/Partition Split(char|string, string[]), RSplit, SplitLines, Partition,
               RPartition, Count               (ver seccion 6.1 -- patron nuevo)
Trim/Strip     Trim, TrimStart, TrimEnd, Strip, LStrip, RStrip
Padding        PadLeft, PadRight (+ overload sin char de relleno), LJust, RJust,
               Center, ZFill, ExpandTabs(+ overload con tabSize)
Casing         ToLower, ToUpper, ToLowerInvariant, ToUpperInvariant, CaseFold,
               SwapCase, Capitalize, Title
Comparación    Compare, CompareOrdinal, CompareTo, Clone
Predicados     IsAlpha, IsDigit, IsAlnum, IsAscii, IsUpper, IsLower, IsWhiteSpace,
               IsNullOrWhiteSpace, IsDecimal, IsNumeric, IsIdentifier, IsPrintable,
               IsTitle
Formato        Format(template, string[] args)  -- sustitucion posicional {0}{1},
               ver seccion 6.2
Terminadores   ReplaceLineEndings
Hash/arreglos  GetHashCode (FNV-1a 32-bit), ToCharArray, CopyTo, Join
ToString()     bool, char, int, long, ulong  (float/double deliberadamente fuera,
               ver seccion 7)
```

### 6.1 Split/RSplit/Partition/RPartition/SplitLines/Count: el patrón que sí funciona

La entrega anterior descartó esta familia completa citando que "no hay precedente de `owned` combinado con tipo arreglo". Es cierto — y se re-verificó explícitamente esta vuelta, no se asumió de nuevo:

```text
raii string[] x = new string[2];   ERROR — "raii local requires a NativeSharp class/interface
                                    reference type or native string" (raii NO acepta arreglos)
owned string[] x = new string[2];  OK como declaracion... pero:
return move(x);                    ERROR — "move(...) source must be a RAII local or owned
                                    parameter" (un local owned-no-raii tampoco califica para move)
return new string[] { a, b };      ERROR — no hay sintaxis de inicializador de arreglo (no parsea)
```

Confirmado: no existe hoy una forma de construir un `owned string[]` cuyos elementos se llenan en varios pasos y devolverlo. Eso descarta la firma "clásica" `string[] Split(...)`. Pero es una pregunta distinta a "¿se puede producir una lista de strings de alguna forma?" — y ahí sí hay un patrón que funciona, verificado antes de escribir el código real:

```text
destination[0] = Dup(a);           OK — asignar un valor owned (resultado de una llamada
                                    owned-returning) directamente a un elemento de un arreglo
                                    STRING[] RECIBIDO COMO PARAMETRO (prestado, no producido) SI
                                    esta permitido.
```

Esa asignación es el mecanismo completo. La superficie usa el patrón "buffer provisto por el caller": el caller reserva el arreglo (`string[] destination`) — decide su tamaño, que además pone un tope natural a cuántas piezas como máximo se producen — y el método lo llena, devolviendo cuántos elementos escribió de verdad. Es además el patrón correcto para un lenguaje sin GC: nadie más que el caller decide dónde vive la memoria del resultado.

```csharp
public static int Split(string value, char separator, string[] destination)
{
    if (value == null || destination == null || destination.Length == 0) { return 0; }
    int count = 0; int start = 0; int i = 0;
    while (i < value.Length && count < destination.Length - 1)
    {
        if (value[i] == separator)
        {
            destination[count] = Substring(value, start, i - start);
            count = count + 1; start = i + 1;
        }
        i = i + 1;
    }
    destination[count] = Substring(value, start, value.Length - start);
    count = count + 1;
    return count;
}
```

Cuando el número real de piezas excede `destination.Length`, el último elemento escrito recibe el resto sin partir ("capacity-bounded split", igual que en C/C++). `Split` parte de izquierda a derecha, así que el resto queda al final; `RSplit` necesita el resto al *inicio* — en vez de mover valores owned ya escritos entre casillas del arreglo (que tiene su propio riesgo de ownership sin probar), usa dos pasadas: la primera solo cuenta separadores, la segunda ignora los primeros `skipCount` y parte normalmente con los que quedan. `Partition`/`RPartition` siempre escriben exactamente 3 elementos (`destination.Length >= 3`) y devuelven `bool` (encontrado o no) en vez de la tupla que Python usa, porque el lenguaje no tiene tuplas. Las cinco funciones se verificaron algorítmicamente con una réplica en Python línea por línea contra los mismos casos usados en `examples/project_stage51/Main.ns` antes de darlas por buenas (no se puede ejecutar el `.exe` real en este entorno — ver sección 8).

**Consecuencia de memoria importante, documentada donde se usa:** cada elemento que estos métodos escriben es un `string` owned. `delete[] destination;` libera el arreglo pero **no** llama destructores de sus elementos (`Deallocation` con `dynamicDestruction = false`, confirmado en `ProjectOopHirModel.cpp`) — es una limitación real del modelo de arreglos, la misma raíz que impide `owned string[]`. Limpiar sin fugas exige `delete destination[i];` elemento por elemento **antes** de `delete[] destination;` — confirmado que compila y verifica LLVM, y es el patrón usado en cada check nuevo de `Main.ns`.

### 6.2 `Format`: sustitución posicional, no variadics

`Format(string template, string[] args)` NO es `printf`/`string.Format` completo — no hay especificadores de formato, ni alineación, ni escape de llaves. Es sustitución de `{0}`, `{1}`, ... contra un arreglo de argumentos ya-`string` que el caller arma. No hizo falta ningún soporte de variadics/params real: es un parámetro de arreglo prestado, igual que `Join`. `printf`-style real con especificadores por tipo queda fuera de este stage a propósito (ver sección 7).

## 7. Qué queda explícitamente fuera, y por qué

- **`Encode`.** La representación ya es byte-oriented; "Encode" en sentido .NET implica un modelo de encodings con nombre (UTF-8/Latin1/ASCII como valores distinguibles) que no está diseñado todavía. Distinto de un problema de ownership: es una superficie conceptual que no existe.
- **`ToString()` de `float`/`double`.** Formatear punto flotante a decimal correctamente (clase Grisu/Ryu) es un problema aparte; no hay infraestructura dtoa en el compilador. Se prefiere dejarlo sin resolver de forma visible (no compila si se intenta) a fingir una implementación ingenua e incorrecta.
- **`Format` con especificadores de tipo** (`{0:X}`, alineación `{0,10}`, escape `{{`). La sustitución posicional simple (sección 6.2) cubre el caso de uso principal; el resto es una extensión de parser del propio template, no un problema de ownership.
- **Soporte Unicode real** (`CaseFold` más allá de ASCII, normalización, culturas). El acceso sigue siendo byte-oriented desde Stage 41; cada método que distingue mayúsculas/minúsculas lo dice explícitamente. `IsDecimal`/`IsNumeric` son alias directos de `IsDigit` en este runtime ASCII-only -- no fingen una distinción de categoría Unicode que no existe.

## 8. Verificación real, sin inventar cifras

Contra este mismo árbol, sin tocar CMake ni cambiar de compilador respecto a como ya se venía haciendo:

```text
GCC 13.3.0             ctest: 202/203 (build limpio, -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
```

De los 5 tests que no pasaban en la entrega anterior, 4 se corrigieron esta vuelta -- con causa raíz real, no ajustando el resultado:

- **`nsc.version`** y la aserción de versión propia de **`nsc.stage50_10`**: el string esperado estaba fijo en `"dev50"`/`"stage 50"`, nunca actualizado cuando el árbol avanzó a `dev51`. Corregido a lo que la suite realmente debe verificar (para `stage50_10`, que `kLanguageVersion` sigue en `0.1` -- el número de stage/version string se mueve independiente de eso).
- **`nsc.stage49.raii-move`** (`RaiiNeedsReference`): esperaba el substring `"raii local requires a NativeSharp class or interface reference type"`; el mensaje real ya dice `"...class/interface reference type or native string"` desde que `raii string` ganó soporte -- el texto del diagnóstico mejoró correctamente en algún punto anterior a este stage, este test viejo nunca se actualizó. Corregido el substring esperado.
- **`nsc.stage35.recovery`** y **`nsc.llvm-lowering`**: buscaban el substring literal `"getelementptr inbounds %nativesharp.class_descriptor"` / `"getelementptr inbounds %Pair"`; el LLVM 20 runtime de este sandbox imprime esos GEP con un calificador `nuw` de más (`"getelementptr inbounds nuw %..."`), confirmado leyendo el IR real generado por esos mismos fixtures. Partido en dos verificaciones de substring independientes para no depender del formato exacto de un calificador de una versión de LLVM.

Los cuatro se confirmaron contra el propio `NativeSharp_Stage50_FULL.zip` certificado (reconstruido limpio en este sandbox) antes de tocarlos: dos de ellos (`stage35.recovery`, `llvm-lowering`) YA fallaban ahí también por la misma razón de `nuw` -- es decir, nunca fueron "201/201" en este entorno concreto, independientemente de este stage; el string de versión y `raii-move` sí pasaban en el FULL certificado y se rompieron en algún punto entre ese certificado y el snapshot de handover que se recibió, antes de empezar este trabajo.

Queda **1 test fallando, sin tocar a propósito**: `nsc.stage50_10.ownership-examples-documentation` espera contenido específico en `examples/project_stage50/README.md` -- ese archivo no existe en este árbol. No se fabricó contenido para pasarlo; es un hueco preexistente del snapshot, documentado, no una regresión de este trabajo.

El objeto/ejecutable real: `examples/project_stage51` (ampliado a ~170 checks con retorno de código de error específico por cada `if` que falla, cubriendo toda la superficie nueva incluyendo Split/RSplit/Partition/Count/Format/StringComparison) compila limpio semántica → HIR → MIR → LLVM (102 callables), y `nsc --emit-project-oop-exe` produce un PE32+ x86-64 real. No se pudo ejecutar ese `.exe` -- no hay Windows ni Wine en este entorno -- así que el comportamiento en tiempo de ejecución de cada método queda verificado hasta "compila y genera PE válido" más una réplica algorítmica en Python de la lógica de Split/RSplit/SplitLines/ExpandTabs/ReplaceLineEndings/Format contra los mismos casos de `Main.ns`, no hasta "corrí el binario real". Limitación del entorno de build, no de este stage.

## 9. Cierre formal

SOURCE-ONLY exacto, FULL congelado, fresh extraction, clean rebuild/test desde esa extracción, SHA-256, handover 51→52 -- todo en el paquete de esta entrega (ver `STAGE51_CHANGED_FILES.txt` y `docs/HANDOVER_STAGE51_TO_STAGE52.md`).
