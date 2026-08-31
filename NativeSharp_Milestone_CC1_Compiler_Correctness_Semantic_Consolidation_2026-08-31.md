
# NativeSharp Compiler — Milestone de Maduración del Core

## Correctness, Semantic Consolidation, Modern Pipeline Completion, EH Hardening and Reproducibility

> **Fecha de esta consolidación:** 2026-08-31  
> **Proyecto:** NativeSharp Compiler (`nsc`)  
> **Línea de release:** NativeSharp 0.1  
> **Snapshot principal de esta revisión:** `nsc-devel-1(1).zip`  
> **Estado del árbol:** posterior a la incorporación de `?:`, `do...while` y `switch/case` en el pipeline moderno de proyectos/OOP.  
> **Propósito de este documento:** preservar la auditoría histórica completa y convertir todos los hallazgos posteriores en un milestone de ingeniería cerrado, secuencial, verificable y ejecutable punto por punto.  
> **Regla de este milestone:** no agregar nuevas features de lenguaje hasta cerrar los gates de corrección definidos aquí.

---

# PARTE I — AUDITORÍA HISTÓRICA PRESERVADA PALABRA POR PALABRA

La siguiente sección es la auditoría histórica previamente guardada. Se conserva íntegra como fotografía del estado anterior del compilador y como justificación de las decisiones del milestone.

---

# NativeSharp Compiler — Historical Technical Audit — 2026-08-31 — Post-Stage 57 — Extreme Architecture, ABI Win64, EH, Runtime, LLVM Backend, Semantic Correctness and Consolidation Review

> **Historical compiler snapshot review**
>
> **Date:** 2026-08-31  
> **Project:** NativeSharp Compiler (`nsc`)  
> **Release line:** NativeSharp 0.1  
> **Code state reviewed:** Post-Stage 57 development tree  
> **Scope:** Frontend, parser, semantic analysis, ownership/RAII, HIR/MIR, control-flow, LLVM lowering, Win64 ABI, native EH, runtime, interop, testing, reproducibility, architectural debt, backend independence and recommended consolidation work.
>
> This document preserves the complete technical conclusions produced during the extreme audit of the then-current NativeSharp compiler source tree.

---

Sí. Lo revisé como si tuviera que decidir si el compilador ya puede sostener los siguientes años de NativeSharp, no sólo si “pasa sus tests”.

Mi conclusión principal es esta: **NativeSharp ya es un compilador real y un toolchain nativo serio**, bastante más allá de un compilador educativo o un prototipo de LLVM. Pero el crecimiento rapidísimo de las últimas etapas ya empezó a revelar una segunda fase del proyecto: **ahora el problema principal ya no es agregar features; es consolidar la arquitectura que ha crecido alrededor de ellas**.

Y encontré varios puntos concretos que conviene corregir antes de seguir metiendo lenguaje a gran velocidad.

## Qué revisé

El árbol que me diste tiene aproximadamente **83,373 líneas de C++/headers del compilador**, más **46,173 líneas de tests unitarios C++**. Hay **309 archivos `.ns`**, de los cuales la biblioteca estándar representa unas 8,776 líneas y los ejemplos casi 10,000. Es una base de código considerable para un compilador de este estado.

Los archivos que concentran gran parte de la complejidad son especialmente reveladores:

| Componente | Tamaño aproximado |
|---|---:|
| `ProjectMethodBodyModel.cpp` | 10,002 líneas |
| `ProjectOopLlvmLowerer.cpp` | 8,178 |
| `WindowsRuntimeBuilder.cpp` | 4,957 |
| `Parser.cpp` | 4,953 |
| `ProjectOopHirModel.cpp` | 3,880 |
| `SemanticAnalyzer.cpp` | 3,279 |
| `Driver.cpp` | 1,971 |
| `ProjectOopMirModel.cpp` | 1,762 |

Por sí solo esto no significa mala arquitectura, pero sí muestra dónde se está acumulando la densidad semántica del compilador.

También revisé lexer/parser, semántica, modelos de proyecto, ownership/RAII, HIR/MIR, LLVM, ABI Win64, EH, runtime, imports, linking, tests, scripts de validación, manifest, library source y documentación incluida.

## La evidencia de validación es realmente buena

El ZIP contiene una ejecución de Windows del **31 de agosto de 2026**. La evidencia incluida reporta:

**239/239 CTest pasados**, con `NSC_WARNINGS_AS_ERRORS=ON`, además de un checklist separado que construye y ejecuta binarios PE reales en O0/O2.

Ese checklist termina con:

**37 passed / 0 failed.**

Incluye los contratos nuevos de interop, structs, unions, function pointers, `ref/out/in`, definite assignment, type root, callback EH boundary, `?:`, `do...while`, ejemplos Stage 56 y la matriz de regresión de bugs: **16/16 bugs reportados cerrados**.

Esto me parece una de las fortalezas más importantes del proyecto. Después de lo que pasó con EH, se nota una evolución clara desde “el IR tiene esta forma” hacia “el ejecutable PE realmente se comporta así”.

Una precisión: **yo no pude volver a ejecutar aquí esos PE de Windows**, porque este sandbox no tiene el entorno Win64/LLVM 20+ necesario ni Wine instalado. Los 239/239 y 37/37 son evidencia incluida en tu paquete. Lo que sí hice independientemente fue inspeccionar el código y ejecutar las verificaciones que puedo ejecutar sobre el árbol.

Y ahí aparecieron cosas interesantes.

# El hallazgo arquitectónico más importante

Actualmente NativeSharp tiene, en la práctica, **dos compiladores dentro del compilador**.

El pipeline histórico/single-file sigue una arquitectura muy bonita:

`Lexer → Parser → SemanticAnalyzer → HIR → MIR → CFG → Dominance → SSA → Optimization IR → LLVM`

Eso vive alrededor de `FrontendCompilation`.

Es genuinamente un pipeline de compilador clásico, con CFG propio, dominance propio, SSA propio y passes propios.

Pero **ése ya no es el pipeline que implementa el NativeSharp moderno**.

El pipeline que compila OOP, generics, ownership, EH, strings, delegates, lambdas, structs modernos, runtime, etc. es:

`ProjectCompilation`  
→ generics  
→ name lookup  
→ member model  
→ inheritance  
→ interfaces  
→ method-body semantics  
→ virtual dispatch  
→ destruction  
→ construction  
→ layouts  
→ `ProjectOopHirModel`  
→ `ProjectOopMirModel`  
→ **`ProjectOopLlvmLowerer` directamente**  
→ LLVM optimizer  
→ LLVM target machine  
→ COFF.

No pasa por el CFG propio.

No pasa por dominance propio.

No pasa por SSA propio.

No pasa por el `PassManager` propio.

Esto es muchísimo más importante de lo que parece.

El archivo `ProjectOopMirModel.h` lo deja bastante claro. Su “MIR” todavía contiene cosas como:

`If`, `While`, `DoWhile`, `For`, `TryRegion`, `CatchRegion`, `FinallyRegion`, `UsingRegion`, `DelegateCreate`, `EventInvoke`, `CoreFileReadAllText`, `CoreThreadCreate`, `CoreSemaphoreWait`, `CoreProcessGetWorkingSetSize`, `AggregateEquals`, `AllocateAndConstruct`, etc.

Es decir: **es un IR semántico de alto nivel y todavía estructurado como árbol**, no el MIR CFG/SSA de bajo nivel que uno querría entregar indistintamente a LLVM, NSASM o cualquier otro backend.

Por eso encontré una afirmación de la documentación que considero actualmente demasiado optimista:

> “MIR es IR propio y LLVM es un backend en el último escalón.”

Conceptualmente sí.

**Prácticamente, todavía no.**

Gran parte del conocimiento de cómo ejecutar NativeSharp moderno está codificado específicamente en `ProjectOopLlvmLowerer.cpp`.

Y hay una segunda pieza: `WindowsRuntimeBuilder.cpp`, con casi **5,000 líneas**, también **construye el runtime directamente mediante la API LLVM**.

Por tanto, reemplazar LLVM hoy no consistiría simplemente en escribir:

`ProjectOopMir → NSASM`.

También habría que reproducir una cantidad importante de semántica escondida en el lowerer LLVM y rehacer el runtime sintetizado mediante LLVM.

Esto no significa que el proyecto esté mal diseñado. Significa que **la independencia del backend aún está una etapa arquitectónica más lejos de lo que parece en el roadmap**.

Para mí éste es el cambio estratégico número uno que debe hacerse antes de que NSASM se convierta en un backend completo.

---

# Encontré un bug semántico real en `?:`

La implementación LLVM de `?:` me gusta.

`ProjectOopLlvmLowerer::lowerConditional()` genera:

`cond → trueBlock / falseBlock → phi`

y no usa `select`.

Eso garantiza lazy evaluation. Es correcto. El fixture Stage57 además lo ejecuta y comprueba precisamente eso.

El problema está **antes**, en `ProjectMethodBodyModel.cpp`, aproximadamente desde la línea 3708.

El analizador hace esencialmente:

```text id="hdl65w"
analyze condition
analyze true expression
analyze false expression
```

secuencialmente.

Pero el propio compilador ya posee una infraestructura correcta de análisis de flujo:

`captureOwnershipFlow()`  
`restoreOwnershipFlow()`  
`joinOwnershipFlow()`.

Y `if` sí la usa correctamente:

```text id="fmelyr"
entry
 ├─ true state
 └─ false state
      ↓
     join
```

`?:` no.

Eso produce un caso concreto de **definite assignment incorrecto**.

Imagina:

```csharp id="4p7ugn"
static int F(bool condition)
{
    int x;
    int unused = condition ? (x = 1) : 0;
    return x;
}
```

Semánticamente debe rechazarse porque si `condition == false`, `x` nunca fue asignada.

Pero el análisis actual visita primero:

```text id="hrc6ga"
x = 1
```

y mete `x` en `assignedLocals_`.

Luego analiza `0`, pero no restaura el estado previo.

Al salir del ternario, `x` permanece marcada como definitivamente asignada.

Esto es particularmente insidioso porque el backend C7 inicializa el storage de locales a cero. Por tanto, en lugar de explotar, `F(false)` podría terminar devolviendo `0`.

Es decir: **el backend puede ocultar el bug del análisis semántico**.

El compilador incluso tiene un comentario excelente dentro de `joinOwnershipFlow()` diciendo que definite assignment es un conjunto MUST y que el join debe ser una **intersección**.

La infraestructura está bien.

Simplemente Stage57 no la utilizó para `?:`.

Para mí esto es **P1 inmediato**.

---

# `?:` también perdió semántica de ownership

Hay otro problema relacionado.

`ExpressionResult` guarda:

`ownership`  
`ownershipTransfer`  
`ownershipSourceLocalOrdinal`  
`ownershipSourceParameterOrdinal`  
`borrowedLocalOrdinal`

entre otras cosas.

Pero al combinar las dos ramas de `?:`, Stage57 básicamente sólo copia:

```text id="iers70"
result.typeKey
result.valid
```

No construye semántica de ownership para el resultado.

Así que una expresión conceptualmente razonable como:

```csharp id="hj67z7"
raii string result = condition ? MakeStringA() : MakeStringB();
```

donde ambos métodos devuelvan `owned string`, no puede conservar correctamente el hecho de que **la expresión completa es un owned-returning expression**.

El chequeo de inicialización RAII exige precisamente esa información y puede rechazarla.

No es un memory-corruption bug. Es un hueco semántico de la primera implementación del ternario.

Conviene arreglarlo al mismo tiempo que el flujo.

---

# `do...while` ejecuta correctamente, pero su análisis semántico no modela `do...while`

El backend está bien.

`lowerDoWhile()` hace:

```text id="s2m6li"
entry → body → condition → body/end
```

y `continue` apunta a la condición.

El fixture real comprueba cuerpo-primero, continue, break y nesting. Muy bien.

Pero `ProjectMethodBodyModel` implementó esto:

```cpp id="yfmadm"
case DoWhileStatement:
case WhileStatement:
{
    analyze condition;
    ...
    analyze body;
}
```

Incluso hay un comentario diciendo que reutilizar el análisis de `while` sólo es “conservador”.

No completamente.

Por ejemplo:

```csharp id="gt2zx9"
int x;

do
{
    x = 1;
}
while (false);

return x;
```

debe ser válido.

`x` está inequívocamente asignado: el cuerpo corre una vez como mínimo.

Pero el estado posterior actual empieza con:

```cpp id="1qyjxj"
OwnershipFlowSnapshot postLoop = loopEntry;
```

y une ese estado de **cero iteraciones** con el estado de backedge.

Ese camino de cero iteraciones **no existe** en un `do...while`.

Consecuencia: puede rechazar código válido de definite assignment.

Más claro todavía:

```csharp id="tqcgpt"
int x;

do
{
    x = 1;
}
while (x < 10);
```

El análisis actual intenta analizar `x < 10` **antes de analizar el cuerpo**, por lo que puede diagnosticar `x` como no asignada en una condición donde, por definición, el cuerpo ya la asignó.

No provoca una mala ejecución porque el backend sí está bien. Provoca falsos diagnósticos y semántica del lenguaje incorrecta.

Lo pondría como otro **P1 Stage57**.

---

# Encontré además una fragilidad concreta en el pre-parser de tipos

Antes de parsear cada unidad, `ProjectCompilation.cpp` hace un escaneo manual de tokens mediante:

`collectTypeSeedsInScope()`  
`collectUsingAliases()`

para descubrir tipos de otros archivos/proyectos y permitir que el parser sepa cuándo algo como:

```csharp id="dc8mmx"
Predicate<int>
```

es un tipo genérico y no una comparación con `<`.

Ya tuvieron un bug exactamente aquí en Stage52, y el código contiene el comentario que explica cómo se arregló.

El problema conceptual es que esto se ha convertido en **un segundo mini-parser**.

Y ya encontré otro edge case en él.

El comentario dice que un delegate puede tener un return type genérico:

```csharp id="v7nhkl"
delegate Result<int> Callback(int x);
```

y que debe saltarse `<...>` del return type.

Pero la implementación comprueba primero:

```cpp id="gh8yj8"
Identifier && next == '<'
```

como posible **nombre del delegate**.

Por lo tanto, ante:

```csharp id="k07qlh"
delegate Result<int> Callback(int x);
```

ve:

```text id="x474ep"
Result <
```

y puede registrar **`Result` como nombre del delegate**, antes de llegar a `Callback`.

El comentario intenta hacer exactamente lo contrario de lo que el orden del código efectivamente hace.

Esto afectaría especialmente visibilidad cross-file/cross-project, que es justamente la razón de existir del scanner.

No veo un test con generic-returning delegate que cubra esa forma.

Es otro excelente argumento para dejar de duplicar parte de la gramática mediante token scanning heurístico.

---

# También existe divergencia de lenguaje entre los dos pipelines

Stage57 añadió realmente:

`KwDo`  
`Question`  
`DoWhileStatement`  
`ConditionalExpression`

al lexer, AST y parser.

El pipeline OOP moderno los implementa.

Pero busqué ambos nodos en:

`src/Sema`  
`src/HIR/HirBuilder.cpp`  
`src/MIR/MirBuilder.cpp`  
`src/LLVM/LlvmLowerer.cpp`

y **no existen**.

Por tanto:

```text id="briu7n"
nsc --emit-project-oop-...
```

y el pipeline moderno saben un lenguaje que:

```text id="vtsa4n"
FrontendCompilation
```

todavía no sabe.

Hay incluso un test viejo en `ParserTests.cpp` que dice:

> “ternary conditional syntax is not part of NativeSharp 0.1”

y todavía espera que falle.

El test pasa, pero ahora **pasa por la razón equivocada**: el parser ya entiende `?:`; es una etapa posterior del pipeline histórico la que no puede procesarlo.

Eso es una señal clásica de deuda de arquitectura.

No mantendría dos dialectos inadvertidos de NativeSharp mucho tiempo más.

---

# ABI e interop: aquí estoy bastante impresionado

Revisé específicamente el arreglo de structs que habíamos discutido.

No fue un parche superficial.

La implementación refleja de verdad el ABI Win64:

Para agregados ordinarios:

```text id="6f5bhd"
1 / 2 / 4 / 8 bytes → direct integer carrier
otros tamaños       → indirect
```

y existe tratamiento de:

copia del argumento indirecto, alineación, `sret`, atributos LLVM, métodos de instancia con `this`, posición correcta de `sret`, callsites y declaraciones.

También encontré la matriz que contrasta layouts reales contra Clang para:

1, 2, 3, 4, 8, 12, 16, 24, 64 y 80 bytes.

Eso es exactamente el tipo de prueba que quiero ver en una ABI.

La parte de interop reciente también está mucho más sólida que antes. La identidad de imports no se reduce ya a sólo nombre de símbolo: aparece el manejo de DLL + symbol + signature, con runtime resolution en la colisión entre DLLs.

El borde C/NativeSharp también protege contra excepciones NativeSharp escapándose por callbacks C. La matriz espera explícitamente exit code 141 cuando ocurre ese trap.

**Yo sí considero la capa de ABI suficientemente madura para empezar `nsbind` y bindings en paralelo.**

Eso no significa que pueda consumir arbitrariamente cualquier header C/C++ monstruoso. Todavía faltan cosas como bitfields, variadics y expresiones de enum generales, y C++ completo obviamente es otro problema. Pero para APIs C bien seleccionadas y headers normalizados, la cimentación ya existe.

---

# EH sigue siendo una de las partes más serias del proyecto

Por el código y las pruebas actuales, ya no considero EH una “feature experimental puesta encima de LLVM”.

Hay funclets Win64, cleanup regions, typed catches, RAII, owned cleanup, rethrow, personality, `.pdata/.xdata`, structured exits y varias regresiones específicas de bugs reales.

Especialmente valioso es que existen tests de ejecución surgidos de errores anteriores, no sólo tests diseñados antes de encontrar bugs.

Ésa es exactamente la forma en que un compilador va endureciéndose.

No encontré en esta revisión una nueva falla comparable a aquella de `finally` O2 o al multicatch anterior.

Eso no prueba ausencia absoluta de bugs, pero la zona está actualmente mucho mejor defendida que Stage45 inicial.

---

# El runtime ya es considerable, pero Unicode será una deuda real

NativeSharp `string` está documentado explícitamente como:

**byte-oriented / ASCII**.

Y el runtime Windows confirma esa decisión.

Filesystem/environment/path usan mayoritariamente:

`CreateFileA` / equivalentes ANSI,  
`GetFullPathNameA`,  
`FindFirstFileA`,  
`CopyFileA`,  
`MoveFileA`, etc.

Command line necesita inevitablemente:

`GetCommandLineW`  
`CommandLineToArgvW`

pero luego convierte los argumentos con:

```text id="pv4ri5"
WideCharToMultiByte(CP_ACP = 0)
```

es decir, **a la code page ANSI del sistema**, no a UTF-8.

Esto está perfectamente documentado en el código; no es un bug accidental.

Pero creo que a partir del punto donde NativeSharp empiece a manejar proyectos, asset pipelines, shaders, nombres de archivos, tooling y contenido internacional, se convertirá en una limitación importante.

Yo no cambiaría `char` a UTF-16.

Me parece mucho más coherente que eventualmente:

**NativeSharp string = bytes UTF-8**,  
Win32 boundary = `W` APIs,  
UTF-8 ↔ UTF-16 sólo en la frontera.

No hace falta hacerlo antes de nsbind, pero sí lo pondría antes de considerar el runtime “maduro”.

---

# La independencia de LLVM necesita una etapa intermedia

Ésta sería mi recomendación arquitectónica más fuerte.

No intentaría hacer directamente:

```text id="grqrzs"
ProjectOopMir
      ↓
   NSASM
```

porque terminarías reproduciendo las 8,000 líneas de decisiones que hoy están en `ProjectOopLlvmLowerer`.

Yo introduciría algo equivalente a:

```text id="a8ssq6"
Modern semantic HIR
        ↓
Backend-neutral MIR
        ↓
CFG + explicit EH regions
        ↓
ABI lowering
        ↓
Low-level NativeSharp IR / LIR
       ↙  ↘
    LLVM   NSASM
```

Ahí deben quedar explícitos:

basic blocks, branches, values/temporaries, aggregate carriers, hidden `sret`, calling conventions, object layouts, virtual dispatch calls, EH edges, cleanup edges, platform runtime calls y eventually relocatable symbols.

Entonces LLVM sería realmente sólo un backend.

Y el NSASM sería otro.

Hoy **LLVM todavía sirve también de “segunda mitad del compilador”**, no solamente de code generator.

No lo veo como un error histórico. De hecho, era probablemente la forma correcta de construir NativeSharp tan rápido. Pero ahora ya llegaste al tamaño donde conviene separar esa responsabilidad.

---

# La modularidad externa es buena; la interna necesita una refactorización

CMake divide correctamente el producto en:

`nsc_compilation`  
`nsc_target`  
`nsc_frontend`  
`nsc_llvm`  
`nsc_runtime`  
`nsc_linker`  
`nsc_driver`.

Eso es bueno.

El problema está un nivel por debajo.

Un `ProjectMethodBodyModel.cpp` de **10 mil líneas** haciendo name binding local, overload resolution, definite assignment, ownership, borrowing, core-runtime recognition, call binding, operators, lambdas, control flow, etc., ya está llegando al límite de lo razonable.

Lo mismo con:

`ProjectOopLlvmLowerer.cpp` ≈ 8,200  
`WindowsRuntimeBuilder.cpp` ≈ 5,000.

No recomiendo partirlos sólo para “tener archivos más pequeños”.

Recomiendo separarlos según responsabilidades.

Por ejemplo, `ProjectMethodBodyModel` debería eventualmente delegar a componentes específicos para:

flow analysis, ownership analysis, overload resolution, expression typing y core binding.

Especialmente **flow analysis**. El bug de `?:` y el de `do...while` son evidencia directa de que seguir implementando control-flow manualmente dentro del visitor de expresiones está empezando a ser peligroso.

Una vez que `switch`, short-circuit más complejo, pattern-like constructs o más ownership entren, el riesgo sube.

---

# El paquete que recibí no es todavía un snapshot canónico

Ejecuté yo mismo:

```text id="yqgiqs"
python3 verify_manifest.py
```

Resultado:

**1109 checked, 15 failures.**

Los 15 son hashes distintos y corresponden precisamente a archivos centrales recientemente modificados:

`CMakeLists.txt`, Lexer, TokenKind, Parser, AST kinds, `ProjectMethodBodyModel`, NameLookup, OOP HIR/MIR y LLVM lowerer.

El manifest es de alrededor de las 18:11 del 30 de agosto y esos fuentes son posteriores, alrededor de las 23:50.

Mi interpretación más probable no es corrupción: **se implementó Stage57 y no se regeneró el manifest**.

Pero un snapshot que se quiere considerar reproducible debe dar:

```text id="o2xtt7"
1109/1109 PASS
```

También quedaron artefactos históricos contradictorios en la raíz. Por ejemplo un archivo llamado simplemente:

`8`

todavía dice:

```text id="kl9mkr"
[VIVO] b3_ctor_aggregate -O2 ...
```

mientras que el triage actual dice 16/16 corregidos.

Y permanecen los informes intermedios de B3/B2/B5/B8 junto al cierre final.

Esto no afecta al compilador.

Sí afecta mucho a la pregunta:

> “¿Cuál es exactamente el estado canónico de este ZIP?”

Conviene que un release/snapshot de desarrollo tenga una sola verdad.

---

# El versionado interno también quedó atrás

El código actual contiene Stage57.

Pero:

`Version.h` todavía dice:

```text id="ekmtap"
kCompilerVersion = "0.1.0-dev55";
kCompilerStage   = "55";
```

y CMake todavía tiene:

```text id="kr6htg"
DESCRIPTION "NativeSharp stage 50 ownership types"
```

Además existe un test que exige que `--version` diga Stage55.

No estoy diciendo que debas cambiar **NativeSharp 0.1**; esa decisión de release es independiente.

Lo que sí recomiendo es que el **build identity** refleje el árbol exacto.

Por ejemplo, incluso manteniendo NativeSharp 0.1, debería poder distinguirse un binario Stage55 de este compilador que ya contiene Stage57.

Para depurar bugs futuros eso será crítico.

---

# `switch` todavía falta de verdad

Los documentos 69/70 dicen que faltaban:

`?:`  
`do...while`  
`switch`.

Los dos primeros ya existen.

`switch/case` no.

Busqué los tokens y no existen `KwSwitch`/`KwCase`.

Así que el roadmap está actualmente una ronda atrás.

Esto es interesante porque significa que **el código fuente está más avanzado que la documentación que vino dentro del ZIP**.

---

# Hay algunas limitaciones legítimas todavía visibles

No son bugs, pero dejan claro que NativeSharp aún no es “C# nativo completo”.

El código todavía declara expresamente como diferidos, entre otros casos, generic methods dentro de generic types, partial generic types, nested lambdas, algunos delegate targets virtuales, struct receiver capture para delegates, compound assignment directamente sobre properties/indexers y `->` para pointer-member access.

También los enum initializers aún no tienen constant-expression evaluation general.

Todo eso es aceptable en 0.1.

Lo importante es distinguir **feature no implementada** de **feature implementada pero semánticamente incorrecta**. Stage57 tiene un poco de ambas cosas ahora mismo.

---

# Mi evaluación del compilador, después de esta revisión

Ya no lo clasificaría como “proyecto de compilador que genera LLVM”.

Eso se quedó corto.

Lo clasificaría como:

**un toolchain Win64 nativo en fase alpha avanzada, con frontend propio, sistema de tipos propio, ownership/RAII, EH nativo, ABI C/Win64 considerablemente probado, runtime propio, biblioteca estándar creciente, generics monomorfizados, OOP, delegates/closures/events, generación COFF/PE y un sistema de validación bastante serio.**

Eso es un compilador de verdad.

Y también diría algo quizás más importante: **el mayor riesgo del proyecto ya dejó de ser “¿podremos implementar X?”**.

Los últimos días muestran que casi cualquier feature se puede implementar.

El riesgo ahora es:

> “¿podemos continuar añadiendo X, Y, Z sin que cinco implementaciones distintas de control-flow, ownership, ABI y parsing empiecen a divergir?”

Ahí está actualmente el cuello de botella intelectual.

## Lo que yo haría inmediatamente

Antes de `switch` o de otro bloque grande de lenguaje, haría una pequeña ronda de consolidación, sin convertirla en meses de refactoring:

1. Corregir `?:` con snapshot de estado antes de las ramas y `joinOwnershipFlow()` al final; añadir tests de definite assignment, ownership, borrow y owned-returning expressions. Separar `DoWhileStatement` del análisis de `WhileStatement` y modelar correctamente body-first y guaranteed-one-iteration. Añadir como regresión exactamente `int x; do { x=1; } while(false); return x;`.

2. Añadir un regression test cross-project para `delegate Result<int> Callback(...)` y corregir/eliminar el heuristic type-seed scanner. A largo plazo, sustituirlo por una verdadera declaration/header pass.

3. Decidir explícitamente qué pasa con el pipeline histórico `FrontendCompilation`: o se mantiene y debe compartir toda la semántica moderna, o pasa a ser infraestructura histórica/testing y el `ProjectCompilation` se convierte formalmente en el único frontend del lenguaje. **No mantendría dos dialectos.**

4. Diseñar el IR que realmente separará NativeSharp de LLVM. No hace falta implementarlo entero todavía, pero sí fijar desde ahora la frontera correcta antes de NSASM/NSLink. También mover gradualmente lógica del runtime fuera de `WindowsRuntimeBuilder` LLVM-specific.

5. Hacer una pasada de higiene: regenerar manifest, eliminar artefactos de triage obsoletos, sincronizar roadmap, mantener NativeSharp 0.1 pero corregir el identificador interno Stage/build, y añadir una validación periódica desde un `build` completamente vacío además del excelente checklist incremental actual.

Después de eso, **sí seguiría con `switch`, nsbind, bindings, windowing, input y graphics en paralelo**.

No considero que los problemas que encontré invaliden el progreso anterior. De hecho, son problemas propios de un proyecto que **ya cruzó la frontera de “compilador pequeño” a “sistema de compilador”**.

Mi conclusión más importante de toda esta revisión es quizá ésta:

**NativeSharp ya tiene suficiente lenguaje, runtime, ABI y tooling como para que seguir añadiendo features deje de ser la prioridad absoluta. Ahora merece una arquitectura de segunda generación que preserve todo lo que construiste y permita que NSASM, NSLink, nsbind y los bindings crezcan sin quedar amarrados a un `ProjectOopLlvmLowerer.cpp` de 15,000 líneas dentro de unos meses.**

Y sí: después de revisar este árbol, tengo una opinión **más alta** del compilador que antes en cuanto a lo que ya funciona —especialmente ABI, EH, testing e interop— pero también una visión mucho más clara de dónde está la deuda técnica real. El bug del ternario y el análisis de `do...while` son los dos que arreglaría **antes de declarar Stage57 cerrado**.


---

# PARTE II — ACTUALIZACIÓN DE LA AUDITORÍA SOBRE `nsc-devel-1(1).zip`

## 1. Qué cambió desde la fotografía histórica

El snapshot nuevo sí es posterior al auditado originalmente. La adición funcional más visible es `switch/case`, incorporado al pipeline moderno de proyectos/OOP.

La ruta observada es:

- lexer;
- AST;
- parser;
- `ProjectMethodBodyModel`;
- `ProjectOopHirModel`;
- `ProjectOopMirModel`;
- `ProjectOopLlvmLowerer`;
- LLVM `switch`;
- fixture de lenguaje `l3_switch`;
- tests positivos y negativos Stage57.

La evidencia Windows incluida en el paquete es además posterior a la del snapshot anterior y reporta:

- 239/239 CTest PASS;
- 39/39 checks/fixtures PASS;
- `l1_conditional` O0/O2 PASS;
- `l2_dowhile` O0/O2 PASS;
- `l3_switch` O0/O2 PASS;
- triage histórico 16/16 PASS.

Esto confirma que el código nuevo no es una modificación documental: el `switch` normal se construyó y se ejecutó en Windows.

La advertencia metodológica se mantiene: esta auditoría estática no sustituye ejecutar los PE Win64 en este entorno. Los resultados O0/O2 anteriores son evidencia incluida dentro del propio snapshot. Lo que sí se verificó directamente aquí fue el contenido actual de los fuentes, las ausencias/presencias de rutas de lowering, el manifest y la coherencia entre subsistemas.

---

# 2. Estado cuantitativo del núcleo moderno

En este snapshot los componentes de mayor densidad continúan creciendo:

| Archivo | Líneas actuales aproximadas |
|---|---:|
| `src/Frontend/ProjectMethodBodyModel.cpp` | 10,391 |
| `src/LLVM/ProjectOopLlvmLowerer.cpp` | 8,299 |
| `src/Runtime/WindowsRuntimeBuilder.cpp` | 4,957 |
| `src/Parser/Parser.cpp` | 5,053 |
| `src/Frontend/ProjectOopHirModel.cpp` | 3,967 |
| `src/Frontend/ProjectOopMirModel.cpp` | 1,771 |

El dato relevante no es solamente que sean archivos grandes. El problema es la densidad de responsabilidades.

`ProjectMethodBodyModel.cpp` contiene hoy, entre otras cosas:

- binding de nombres locales;
- resolución de llamadas;
- overload resolution;
- typing;
- definite assignment;
- ownership;
- borrow;
- moves;
- control-flow;
- `if`;
- loops;
- `?:`;
- `switch`;
- operadores;
- lambdas;
- delegates;
- reconocimiento de operaciones del runtime;
- folding constante específico de `switch`.

Esta concentración ya está produciendo múltiples implementaciones parciales de una misma semántica, y los bugs encontrados son evidencia concreta de ello.

---

# 3. `switch/case`: estructura general correcta, folding constante peligroso

La estructura general de `switch` es buena.

Se tomaron decisiones correctas:

- no hay fall-through implícito;
- cada sección se analiza desde un snapshot común de entrada;
- `break` pertenece al switch más interno;
- `continue` sigue buscando el loop envolvente;
- HIR y MIR representan explícitamente el switch;
- LLVM usa una única construcción de switch;
- existen tests negativos para tipos de governing expression, labels no constantes, duplicados, múltiples `default` y `continue` sin loop;
- existe ejecución O0/O2.

El problema está en el nuevo `ProjectMethodBodyModel::foldIntegerConstantExpression`.

Ese helper es un segundo evaluador de constantes, separado del evaluador más completo que ya existe en el compilador.

El comentario del propio código afirma deliberadamente que no es un evaluador general. Eso es válido como intención, pero el subconjunto que sí implementa debe seguir respetando exactamente la semántica tipada del lenguaje. Actualmente no lo hace.

---

# 4. P1 — los casts en labels de `switch` no normalizan el valor

La implementación actual de folding atraviesa un cast y devuelve el valor del operando sin aplicar el ancho, signedness ni truncamiento del tipo destino.

Eso es incorrecto para conversiones integrales narrowing.

La consecuencia importante no es simplemente que se muestre un valor distinto en una optimización: el frontend utiliza esos valores para decidir si dos labels son duplicados, mientras LLVM termina materializando la constante con el ancho real del governing type.

Por tanto puede ocurrir esta secuencia:

1. el frontend ve dos enteros distintos;
2. el frontend acepta ambos labels;
3. el backend los convierte al mismo bit pattern del tipo real;
4. LLVM recibe dos case values equivalentes.

Ese caso debe ser rechazado antes de llegar a LLVM.

Éste es un bloqueador real para considerar `switch` semánticamente cerrado.

---

# 5. P1 — signed/unsigned se pierde en `/`, `%` y `>>`

El folding nuevo guarda los resultados en una representación `int64_t`.

Para suma, resta, producto y bitwise intenta operar sobre el gemelo unsigned para evitar UB por overflow.

Pero división, módulo y shift right vuelven a operar con signed `int64_t`.

Eso significa que una expresión cuyo tipo semántico sea `ulong` puede evaluarse como signed.

Los casos críticos son:

- unsigned division con bit 63 activo;
- unsigned modulo con bit 63 activo;
- unsigned logical right shift;
- signed arithmetic right shift;
- expresiones con widths menores a 64 bits.

El compilador ya contiene una implementación mejor de estas reglas dentro de su folding MIR histórico/moderno. Duplicarlas en `ProjectMethodBodyModel` es innecesario y peligroso.

---

# 6. P1 — falta la protección de `minimum signed / -1`

El evaluador de constantes existente en otras partes del compilador protege el caso de división signed mínima entre -1.

El folding nuevo de `switch` no contiene esa protección.

Una expresión constante válida sintácticamente puede por tanto llevar el propio frontend a una operación de C++ que no debe ejecutarse sin guard.

Un compilador no debe permitir que el texto de entrada alcance undefined behavior dentro del compilador.

---

# 7. P1 — el ancho de shift se trata siempre como 64 bits

El folding de `switch` acepta counts hasta 63 sin consultar el ancho semántico del operando.

Pero NativeSharp tiene varios anchos integrales.

La validación debe ocurrir respecto al ancho real del tipo de la expresión, no respecto a la representación interna usada por el compiler implementation.

Éste es otro argumento para eliminar el evaluador especializado y usar una sola infraestructura tipada.

---

# 8. P1 — `char` es 8-bit en todo el backend, pero 16-bit en constant conversion

La auditoría confirma una inconsistencia real en `ProjectTypeConversion.cpp`.

En layout, ABI, LLVM, parser y representación de carácter, `builtin:char` es un entero de 8 bits.

Sin embargo, `constantFits` agrupa `builtin:char` con `builtin:ushort` y acepta valores hasta 65535.

Eso es incompatible con el resto del lenguaje.

La corrección semántica debe ser inequívoca:

- NativeSharp `char` actual es de 8 bits;
- la constant implicit conversion hacia `char` sólo puede aceptar valores representables por 8 bits;
- `char` no debe heredar reglas de C# `System.Char`/UTF-16 mientras su layout real siga siendo de un byte.

La llegada de `switch` vuelve este bug especialmente peligroso porque dos labels que parecen distintos para el semantic model pueden truncarse al mismo `i8`.

---

# 9. P1 — `?:` sigue analizando las dos ramas secuencialmente

El hallazgo de la auditoría anterior continúa intacto.

El semantic model analiza:

- condición;
- rama verdadera;
- rama falsa;

sobre un único estado mutable.

No captura el estado posterior a la condición, no restaura ese mismo estado antes de cada rama y no hace un join de ambas ramas.

El LLVM lowering sí usa control-flow lazy con bloques separados y PHI.

Eso significa que semántica y ejecución describen dos grafos de control diferentes.

Consecuencias:

- definite assignment demasiado optimista;
- moves condicionales tratados como incondicionales;
- ownership/borrow state contaminado por una rama que puede no ejecutarse;
- posibilidad de que el backend inicialice storage y oculte el error en runtime.

Debe corregirse antes de cerrar el operador.

---

# 10. P1 — `?:` no construye ownership del resultado

Cuando las dos ramas tienen tipo compatible, el resultado del operador recibe esencialmente tipo y validez.

No se construye de forma completa la semántica de:

- ownership;
- ownership transfer;
- source local;
- source parameter;
- borrowed local.

Por tanto dos ramas que ambas producen un valor owned no necesariamente producen una expresión condicional que sea a su vez un owned transfer válido.

Esto debe resolverse junto con el join de flujo, no como parche separado.

---

# 11. P1 — `&&` y `||` tienen el mismo defecto de flujo

Este hallazgo es anterior a Stage57, pero quedó expuesto al auditar el ternario.

`ProjectMethodBodyModel::analyzeBinary()` analiza primero el operando izquierdo y después el derecho siempre.

Más tarde decide qué operador era.

El LLVM lowering, en cambio, genera short-circuit real para `&&` y `||`.

Consecuencia:

- una asignación que sólo vive en RHS puede marcar un local como definitivamente asignado aunque RHS no haya corrido;
- un move/borrow/cleanup state producido en RHS puede contaminar el estado posterior aunque esa rama no se ejecute;
- la semántica no coincide con el CFG ejecutado.

Este problema y el de `?:` son la misma clase de bug.

No deben arreglarse con dos hacks locales independientes.

---

# 12. P1 — `do...while` sigue compartiendo el análisis de `while`

El semantic model agrupa `DoWhileStatement` y `WhileStatement` y analiza la condición antes del cuerpo.

El comentario actual sostiene que eso es meramente conservador porque un do-while ejecuta al menos una vez.

Para definite assignment esa afirmación no es correcta.

Un do-while posee dos propiedades esenciales que deben reflejarse:

- el cuerpo se ejecuta antes de la primera condición;
- el camino de cero iteraciones no existe.

Esto afecta:

- reads en la condición de variables asignadas dentro del body;
- variables definitivamente asignadas después del loop;
- moves;
- ownership;
- continue edges;
- break exits.

El backend sí ejecuta correctamente body-first. El semantic analyzer debe modelar lo mismo.

---

# 13. Confirmación — `stackalloc` existe en parser y semántica moderna, pero no en OOP HIR

`AstNodeKind::StackallocExpression` existe y llega a `ProjectMethodBodyModel`.

No existe un case correspondiente en `ProjectOopHirModel::lowerExpression`.

Por tanto una expresión aceptada y tipada por el semantic model moderno cae finalmente en el `default` de `lowerExpression`, que reporta una expression kind no soportada por Stage34 OOP HIR.

El pipeline clásico sí posee:

- HIR `StackAllocation`;
- MIR `StackAllocate`;
- LLVM lowering mediante array alloca;
- validaciones de count y tamaño.

La feature no está ausente de NativeSharp como proyecto histórico: está ausente de la migración al pipeline moderno.

---

# 14. Hallazgo adicional — la semántica moderna de `stackalloc` tampoco está completa

No basta con agregar un node al OOP HIR.

En el pipeline clásico `stackalloc` valida explícitamente:

- elemento completo;
- elemento sized;
- elemento no `void`;
- elemento no array;
- count integral no bool;
- constant negative count;
- overflow del cálculo byte-size.

En `ProjectMethodBodyModel`, `StackallocExpression` comparte una rama con `NewExpression`.

La validación detallada del count se ejecuta específicamente para el `new[]` dinámico.

El `stackalloc` no replica todas esas comprobaciones antes de declararse válido.

Por tanto el milestone debe migrar **la semántica completa**, no sólo el lowering.

---

# 15. Confirmación — `sizeof(type)` y `sizeof(expression)` no existen en OOP HIR

Ambos nodos existen en parser y en `ProjectMethodBodyModel`.

Ninguno existe como case en `ProjectOopHirModel`.

El pipeline clásico los baja directamente a una constante `nuint`, precisamente porque `sizeof` es compile-time y el operando de la forma expression es unevaluated.

La migración moderna está incompleta.

---

# 16. Hallazgo adicional crítico — el `sizeof(expression)` moderno analiza el operando como una expresión normal

Éste es más profundo que la ausencia del HIR.

La rama moderna llama a `analyzeExpression` sobre el operando de `sizeof(expression)`.

Eso puede modificar:

- definite assignment;
- ownership state;
- move state;
- borrow state;
- bindings de operaciones con side effects.

Sin embargo el pipeline clásico posee tratamiento explícito de unevaluated operand y tests que comprueban que un incremento dentro de `sizeof` no sobreviva como ejecución en HIR.

Además hay otra divergencia:

- pipeline clásico: resultado `nuint`;
- semantic model moderno actual: resultado `ulong`.

Así que la tarea correcta para `sizeof` es triple:

1. corregir su semántica unevaluated;
2. unificar el tipo del resultado con `nuint`;
3. sólo entonces añadir OOP HIR lowering.

---

# 17. Confirmación — raw scalar `new T` es aceptado por la semántica moderna y rechazado por OOP HIR

`ProjectMethodBodyModel` acepta el `new` raw no-array de un tipo no-clase y lo tipa como pointer al elemento.

`ProjectOopHirModel`, en cambio, sólo acepta el `NewExpression` que tiene binding de `ArrayAllocation`.

Para cualquier otro raw `new T` reporta que raw new sigue fuera del pipeline.

Éste es otro feature de Parte 1 que no terminó de migrarse al compilador moderno.

---

# 18. Confirmación — pointer arithmetic está implementado en el pipeline clásico, no en el moderno

El MIR clásico tiene opcodes específicos para:

- pointer add;
- pointer subtract;
- pointer difference.

El verifier clásico comprueba reglas de tipos y LLVM tiene lowering específico.

`ProjectMethodBodyModel::analyzeBinary()` moderno no posee una rama de pointer arithmetic.

La ruta aritmética general exige `commonNumericType`, y los pointers no son tipos numéricos.

Por tanto el moderno ya sabe manejar:

- address-of;
- dereference;
- pointer indexing;
- pointer ABI;
- pointer parameters;
- raw element address;

pero no sabe manejar todavía:

- pointer + integer;
- integer + pointer;
- pointer - integer;
- pointer - matching pointer;
- pointer increment/decrement, en la medida en que dependan de las reglas numéricas actuales.

Esto es otra divergencia explícita entre los dos compiladores.

---

# 19. El pre-parser de type seeds sigue siendo un segundo parser frágil

`ProjectCompilation.cpp` contiene `collectTypeSeedsInScope` y `collectUsingAliases`.

Su propósito es sembrar nombres de tipos antes del parse completo para resolver ambigüedades como generic type syntax.

Ya fue necesario parchearlo en Stage52 para delegates cross-project.

La lógica de delegates dice que el return type puede ser genérico y que el scanner debe saltar sus `<...>`.

Sin embargo primero considera candidato a nombre de delegate cualquier identifier seguido de `<` o `(`.

Una declaración cuyo return type sea genérico puede hacer que el scanner confunda el nombre del return type con el nombre del delegate.

El problema conceptual es mayor que ese edge case:

> la gramática de declaraciones se está implementando una segunda vez mediante scanning heurístico de tokens.

Cada nueva forma de type syntax incrementará el riesgo.

La solución de milestone no debe ser otro parche al scanner; debe reemplazar esta duplicación por una declaration/header pass real.

---

# 20. Los dos pipelines continúan divergiendo

El pipeline moderno de proyecto entiende ya:

- `?:`;
- `do...while`;
- `switch/case`;
- OOP;
- generics;
- ownership;
- EH moderno;
- runtime moderno.

El pipeline clásico conserva otras capacidades:

- `sizeof`;
- `stackalloc`;
- raw new;
- pointer arithmetic;
- CFG propio;
- dominance;
- SSA;
- passes propios.

El resultado actual no es una arquitectura de capas donde ambos caminos son equivalentes.

Son dos conjuntos de capacidades parcialmente superpuestos.

Esto ya produce tests verdes por razones históricas equivocadas. Un test clásico puede afirmar que una forma sintáctica “no pertenece a NativeSharp 0.1” cuando el parser moderno ya la acepta y lo que falla en realidad es una etapa clásica posterior.

Este milestone debe terminar con **un único frontend user-facing canónico**.

---

# 21. `switch` + `try/catch`: la fixture propuesta falta y debe agregarse

La fixture `l3_switch` no contiene `try` ni `catch`.

No existe actualmente una prueba de ejecución que cubra de manera directa:

- loop exterior;
- try/catch;
- switch dentro del catch;
- break local al switch;
- continue hacia el loop exterior.

Esa composición debe medirse en O0 y O2.

Sin embargo, el análisis del EH actual muestra una sutileza importante.

---

# 22. Los bodies reales de `catch` ya no viven dentro de catch funclets

Después de los arreglos B-006/B-007 y multicatch, el lowering actual hace que el `catchpad` retorne rápidamente y que el body real de la cláusula se ejecute como parent block ordinario.

Por tanto un switch situado sintácticamente dentro de un catch no necesariamente se baja con `funcletStack_` a profundidad mayor que el loop exterior.

Esto significa que la fixture `loop → catch → switch` sigue siendo indispensable, pero no es la mejor prueba para demostrar que `funcletDepth` y `continueFuncletDepth` realmente difieren.

Para eso el escenario más quirúrgico es un switch dentro de un cleanup funclet, especialmente un `finally` ejecutado durante exceptional unwind.

---

# 23. Hallazgo EH nuevo — structured exit desde catch puede saltarse `__nativesharp_exception_clear`

La salida normal de un catch termina pasando por un bloque `eh.catch.handled`.

Ese bloque llama a `__nativesharp_exception_clear`, que limpia la current exception guardada en TLS.

Pero un `continue`, `break` o posiblemente otro structured exit desde el body de catch puede ser bajado directamente al target estructurado cuando no hay cleanup ordinals activos y la profundidad de funclet coincide.

En esa ruta el control no pasa por `eh.catch.handled`.

Por tanto existe una divergencia real de invariant:

- fallthrough normal de catch limpia TLS;
- una salida estructurada legal desde el mismo catch puede no hacerlo.

Aunque todavía se necesite fixture runtime para medir la consecuencia observable exacta en todos los casos, estáticamente la diferencia existe y debe eliminarse.

La limpieza de current exception no debe depender de caer accidentalmente por un bloque LLVM concreto.

Debe representarse como obligación semántica de salida del catch.

---

# 24. `LoopTargets` con dos profundidades sigue siendo razonable, pero necesita una prueba mejor

La estructura backend guarda:

- break target;
- continue target;
- break funclet depth;
- continue funclet depth.

Eso es razonable porque un switch puede poseer su propio break target y heredar el continue target de un loop exterior.

Pero la prueba que realmente debe demostrar la necesidad de depths distintos es:

- loop exterior;
- try/finally;
- exceptional path entrando al cleanup funclet;
- switch dentro del finally;
- break que sólo abandona el switch y debe permanecer dentro del cleanup funclet.

Ese caso debe ejecutarse O0/O2 y comprobar que:

- el break no abandona el finally;
- el resto del cleanup obligatorio se ejecuta;
- la excepción continúa por la ruta esperada;
- no se altera el loop exterior de manera incorrecta.

---

# 25. Reproducibilidad del snapshot: manifest todavía inválido

Se ejecutó directamente `verify_manifest.py` sobre este snapshot.

Resultado:

- 1109 entradas comprobadas;
- 17 fallos de hash;
- exit status de fallo.

En el snapshot anterior eran 15.

Los archivos con mismatch incluyen componentes modificados por Stage57 y switch: CMake, AST, lexer, parser, ProjectMethodBodyModel, OOP HIR/MIR y LLVM lowerer.

La explicación más probable sigue siendo que el código cambió y el manifest no se regeneró.

No parece evidencia de corrupción.

Pero formalmente el paquete no es canónico según su propio mecanismo de integridad.

---

# 26. Build identity continúa atrasado

El árbol actual conserva:

- NativeSharp language version 0.1;
- compiler version `0.1.0-dev55`;
- compiler stage `55`;
- CMake description referida a Stage50 ownership types.

No se propone cambiar la release NativeSharp 0.1.

La corrección necesaria es que el **build identity interno** represente el snapshot real.

De lo contrario dos compiladores distintos pueden reportar exactamente la misma identidad, lo que perjudica reproducción de bugs, metadata de librerías generadas y compatibilidad.

---

# 27. La conclusión consolidada

NativeSharp ya no tiene como riesgo principal “no poder implementar una feature”.

El riesgo ahora es que distintas capas estén definiendo partes diferentes del lenguaje:

- un folding de constantes en switch;
- otro folding en MIR;
- una regla de char en layout;
- otra en constant conversion;
- un CFG real en LLVM;
- otro modelo secuencial en semantic analysis;
- un parser real;
- un scanner heurístico de type seeds;
- un frontend clásico;
- un frontend moderno.

Los bugs encontrados no son casualidades desconectadas.

Son síntomas de duplicación semántica.

Por eso el milestone siguiente debe ser deliberadamente aburrido en el mejor sentido: menos sintaxis nueva y más invariantes únicas, compartidas y medibles.

---

# PARTE III — REGISTRO CONSOLIDADO DE DEFECTOS Y HUECOS

| ID | Prioridad | Área | Hallazgo | Estado requerido al cerrar milestone |
|---|---|---|---|---|
| C-001 | P1 | Constants | casts de case no normalizan valor | cerrado |
| C-002 | P1 | Constants | unsigned `/`, `%`, `>>` evaluados como signed | cerrado |
| C-003 | P1 | Constants | signed-min / -1 no protegido | cerrado |
| C-004 | P1 | Constants | shift width fijo a 64 | cerrado |
| C-005 | P1 | Types | `char` 8-bit vs constant conversion 16-bit | cerrado |
| F-001 | P1 | Flow | `?:` no hace branch snapshot/join | cerrado |
| F-002 | P1 | Ownership | `?:` pierde ownership transfer | cerrado |
| F-003 | P1 | Flow | `&&` analiza RHS como incondicional | cerrado |
| F-004 | P1 | Flow | `||` analiza RHS como incondicional | cerrado |
| F-005 | P1 | Flow | `do...while` analizado como while | cerrado |
| EH-001 | P1 | EH | salida estructurada de catch puede omitir exception clear | cerrado |
| EH-002 | P1 | EH tests | falta loop/catch/switch break+continue runtime fixture | cerrado |
| EH-003 | P1 | EH tests | falta fixture que ejercite dos funclet depths reales | cerrado |
| M-001 | P2 alto | Modern pipeline | `stackalloc` no llega a OOP HIR | cerrado |
| M-002 | P2 alto | Semantics | `stackalloc` moderno no replica validación clásica completa | cerrado |
| M-003 | P2 alto | Modern pipeline | `sizeof(type)` no llega a OOP HIR | cerrado |
| M-004 | P1/P2 alto | Semantics | `sizeof(expr)` moderno no es unevaluated y usa tipo distinto | cerrado |
| M-005 | P2 alto | Modern pipeline | raw `new T` no llega a OOP HIR | cerrado |
| M-006 | P2 alto | Modern pipeline | pointer arithmetic no existe en semantic model moderno | cerrado |
| P-001 | P2 alto | Parser | type-seed scanner es gramática duplicada | reemplazado |
| P-002 | P2 alto | Parser | generic-return delegate puede sembrarse incorrectamente | cerrado |
| A-001 | P2 alto | Architecture | dos frontends user-visible divergen | unificado |
| A-002 | P2 | Architecture | lowering moderno todavía LLVM-specific demasiado pronto | preparado para siguiente milestone |
| R-001 | P2 | Reproducibility | manifest 17 hashes inválidos | 0 fallos |
| R-002 | P2 | Reproducibility | dev55/stage55/CMake Stage50 desincronizados | corregido |
| R-003 | P2 | Testing | suite no cubre interacciones detectadas | cubierta |

---

# PARTE IV — MILESTONE PROPUESTO

# Milestone CC1 — Compiler Correctness & Semantic Consolidation

## Objetivo

El objetivo de este milestone no es ampliar el lenguaje.

El objetivo es conseguir que las features que NativeSharp **ya afirma soportar** tengan:

1. una sola semántica;
2. un solo frontend canónico para el usuario;
3. control-flow semántico consistente con control-flow ejecutado;
4. evaluación constante tipada y compartida;
5. salida EH estructurada con invariantes explícitos;
6. todas las features de memoria/pointers históricas necesarias migradas al pipeline moderno;
7. build reproducible e identificable;
8. tests de interacción O0/O2 suficientes para impedir la reaparición de estos defectos.

## Regla de scope

Durante CC1:

- no añadir pattern matching;
- no añadir nuevas formas de `switch`;
- no añadir nueva sintaxis de lenguaje;
- no iniciar todavía el backend NSASM como reemplazo de LLVM;
- no iniciar la reescritura completa del runtime;
- no hacer la migración Unicode;
- no mezclar este milestone con Windowing/Input/Graphics;
- no “arreglar de paso” features no relacionadas sin crear un ID separado.

La prioridad es convertir el compilador actual en una base más coherente.

---

# PARTE V — PLAN DE IMPLEMENTACIÓN PASO A PASO

Cada tarea de esta sección está diseñada para ser cerrada individualmente.

Una tarea sólo puede marcarse completada si cumple su criterio de terminado.

No se considera “hecha” porque compile.

---

# FASE 0 — Congelar baseline y crear disciplina del milestone

## CC1-000 — Crear documento de estado inicial

**Tarea**

Guardar dentro de `docs/history` una copia de este documento y registrar el nombre exacto del snapshot que originó el milestone.

**Debe registrar**

- fecha;
- nombre del ZIP;
- language version;
- compiler version;
- compiler stage;
- CMake description actual;
- resultado actual del manifest;
- evidencia Windows incluida;
- conteo actual de tests;
- lista de defectos C/F/EH/M/P/A/R de este documento.

**Criterio de terminado**

Existe un único documento de baseline que permite saber exactamente qué árbol se estaba corrigiendo.

---

## CC1-001 — Congelar la lista de features del milestone

**Tarea**

Crear una lista explícita de features que no podrán añadirse durante CC1.

**Criterio de terminado**

Toda PR/cambio del milestone puede clasificarse como:

- corrección;
- migración al pipeline moderno;
- test;
- refactor semántico;
- reproducibilidad.

Si no entra en esas categorías, se difiere.

---

## CC1-002 — Crear una suite de tests CC1 dedicada

**Tarea**

Crear una suite o agrupación de tests cuyo nombre identifique este milestone.

Debe contener exclusivamente regresiones nuevas descubiertas por esta auditoría.

**Criterio de terminado**

Cada bug de la tabla consolidada que sea verificable por test tiene al menos un test CC1 asociado.

---

# FASE 1 — Unificar la semántica integral y el constant folding

Esta fase debe completarse antes de modificar más `switch`.

---

## CC1-100 — Fijar oficialmente `char` como unsigned 8-bit

**Archivos/subsistemas a auditar**

- `ProjectTypeConversion`;
- type layout;
- object layout;
- LLVM type mapping;
- C ABI/header generator;
- literal typing;
- enum/constant conversion;
- switch labels.

**Tarea**

Eliminar cualquier regla que permita representar en `builtin:char` un valor fuera del rango real de un byte.

**Criterio de terminado**

Todos los subsistemas coinciden en:

- tamaño 1 byte;
- ancho 8 bits;
- rango de constant conversion coherente con ese ancho.

No queda ninguna regla heredada de un `char` UTF-16.

---

## CC1-101 — Añadir regresiones de rango de `char`

**Tests obligatorios**

- límite inferior válido;
- límite superior válido;
- primer valor inmediatamente superior rechazado;
- constant conversion hacia `char`;
- `switch(char)` con labels en límites;
- duplicado que antes sólo aparecía después de truncamiento.

**Criterio de terminado**

Los tests fallan con el compilador baseline y pasan después de CC1-100.

---

## CC1-102 — Crear un único evaluador integral tipado

**Tarea**

Extraer la semántica integral ya madura que existe en el folding del compilador a un componente compartido.

No crear otro evaluador nuevo dentro de `ProjectMethodBodyModel`.

**El componente debe conocer explícitamente**

- tipo del operando;
- ancho en bits;
- signedness;
- tipo del resultado;
- normalización después de casts;
- unary plus;
- unary minus;
- bitwise not;
- suma;
- resta;
- multiplicación;
- división;
- módulo;
- bitwise and/or/xor;
- shifts;
- enum constants;
- const fields;
- integer literals;
- character literals.

**Debe rechazar de forma definida**

- división por cero;
- módulo por cero;
- signed minimum dividido entre -1;
- shift negativo;
- shift igual o mayor que el ancho del operando;
- operaciones cuyo tipo no sea integral admitido.

**Criterio de terminado**

Existe una sola implementación compartida de estas reglas para los consumidores que entren en CC1.

---

## CC1-103 — Hacer que el evaluador preserve signed/unsigned

**Tarea**

Eliminar toda decisión basada únicamente en guardar el bit pattern en `int64_t`.

Cada operación debe ejecutarse según el tipo semántico NativeSharp.

**Criterio de terminado**

Los casos unsigned con bit alto activo producen el mismo resultado que la semántica del tipo NativeSharp.

---

## CC1-104 — Hacer que los casts constantes normalicen ancho y signedness

**Tarea**

Cuando una constant expression contiene cast integral, el valor resultante debe convertirse al tipo destino antes de continuar el folding.

**Criterio de terminado**

La representación posterior al cast ya es la representación canónica del tipo destino.

---

## CC1-105 — Migrar `switch` al evaluador tipado compartido

**Tarea**

Eliminar la lógica independiente que actualmente decide los valores de case labels.

`switch` debe pedir al evaluador compartido:

- valor tipado;
- valor normalizado;
- representabilidad respecto al governing type.

**Criterio de terminado**

`ProjectMethodBodyModel` deja de poseer una segunda semántica aritmética para labels.

---

## CC1-106 — Normalizar labels al governing type antes de buscar duplicados

**Tarea**

La tabla de duplicados del switch debe usar el valor final que tendría el case en el governing type, no el valor pre-conversión.

**Criterio de terminado**

Dos labels que producen el mismo bit pattern del governing type se diagnostican como duplicados antes de HIR/MIR/LLVM.

---

## CC1-107 — Garantizar que HIR/MIR reciben labels ya canónicos

**Tarea**

El backend no debe ser responsable de descubrir truncamientos que cambien la identidad de un case.

**Criterio de terminado**

El valor almacenado en HIR/MIR para cada label coincide con el valor que se emitirá en LLVM.

---

## CC1-108 — Crear matriz de regresión de constant folding

**Tests obligatorios**

- cast narrowing;
- cast signed a unsigned;
- cast unsigned a signed;
- unsigned right shift;
- signed right shift;
- unsigned division;
- unsigned modulo;
- shift en cada ancho integral;
- shift inválido;
- divide-by-zero;
- modulo-by-zero;
- minimum signed / -1;
- enum + constant field;
- label duplicado tras conversión;
- `char` duplicado tras normalización.

**Criterio de terminado**

Todos están cubiertos por tests unitarios y, para los casos ejecutables, fixture O0/O2.

---

## GATE 1

No avanzar a la siguiente fase hasta que:

- `char` sea coherente;
- `switch` no posea folding aritmético propio;
- la matriz CC1 de constantes esté verde;
- LLVM nunca reciba case labels duplicados por truncamiento.

---

# FASE 2 — Hacer que el análisis de flujo represente el CFG real

Ésta es la fase central del milestone.

---

## CC1-200 — Formalizar una operación común de branch snapshot/join

**Tarea**

Definir dentro del semantic model una única forma de:

1. capturar el estado de entrada;
2. analizar una rama;
3. capturar su salida;
4. restaurar la entrada;
5. analizar otra rama;
6. unir ambas salidas.

**El estado debe incluir**

- reachability;
- definite assignment;
- moved RAII locals;
- maybe-moved locals;
- ownership state;
- borrow state;
- cualquier estado que actualmente viaje en `OwnershipFlowSnapshot`.

**Criterio de terminado**

Los constructos ramificados dejan de duplicar manualmente el protocolo.

---

## CC1-201 — Especificar la regla de join para definite assignment

**Tarea**

Documentar y aplicar que definite assignment es una propiedad MUST.

Un local sólo queda definitely assigned después del join si está assigned en todos los paths alcanzables que llegan al join.

**Criterio de terminado**

Existe un test unitario directo que demuestre intersección de estados.

---

## CC1-202 — Especificar la regla de join para moves/ownership

**Tarea**

Documentar y aplicar la diferencia entre:

- definitely moved;
- maybe moved;
- not moved.

**Criterio de terminado**

Un move presente en una sola rama nunca aparece como definitely moved, pero sí impide usos que requieran demostrar que el owner sigue intacto en todos los paths.

---

## CC1-203 — Corregir el flujo de `?:`

**Tarea**

Analizar ambas ramas desde el mismo snapshot de entrada posterior a la condición y unir sus estados.

**Criterio de terminado**

Una asignación o move presente sólo en una rama no contamina incondicionalmente el estado posterior.

---

## CC1-204 — Definir ownership del valor producido por `?:`

**Tarea**

Añadir reglas explícitas para el resultado del operador.

Casos mínimos:

- ambas ramas borrowed del mismo tipo;
- ambas ramas owned transfer;
- una owned y una no-owned incompatible;
- null/reference compatible según las reglas actuales;
- source local/source parameter sólo cuando esa identidad sea válida tras el join.

**Criterio de terminado**

Una variable RAII puede inicializarse correctamente desde un ternario cuyas dos ramas produzcan un owned value compatible.

---

## CC1-205 — Añadir tests negativos de definite assignment para `?:`

**Tests obligatorios**

- asignación sólo en rama true;
- asignación sólo en rama false;
- asignación en ambas ramas;
- una rama unreachable;
- nested ternary.

**Criterio de terminado**

Sólo los casos donde todos los paths alcanzables asignan el local son aceptados.

---

## CC1-206 — Añadir tests de ownership para `?:`

**Tests obligatorios**

- owned return en ambas ramas;
- move de un owner sólo en una rama;
- move del mismo owner en ambas ramas;
- borrow en una rama y owned en otra;
- cleanup de resultados temporales.

**Criterio de terminado**

O0/O2 y semantic diagnostics coinciden.

---

## CC1-207 — Corregir el flujo de `&&`

**Tarea**

Modelar dos caminos posteriores al operando izquierdo:

- camino donde RHS no se ejecuta;
- camino donde RHS sí se ejecuta.

El estado posterior debe ser el join de ambos.

**Criterio de terminado**

Una asignación o move exclusivo de RHS no se vuelve incondicional.

---

## CC1-208 — Corregir el flujo de `||`

**Tarea**

Aplicar la misma infraestructura de short-circuit, con la polaridad correspondiente.

**Criterio de terminado**

El semantic model y LLVM describen los mismos paths posibles.

---

## CC1-209 — Añadir matriz de short-circuit flow

**Tests obligatorios**

Para `&&` y `||`:

- assignment en RHS;
- move en RHS;
- borrow en RHS;
- call con side-effect en RHS;
- nested short-circuit;
- combinación con ternary;
- combinación dentro de condición de loop.

**Criterio de terminado**

Todos los casos tienen diagnostic o ejecución acorde con short-circuit real.

---

## CC1-210 — Separar `DoWhileStatement` de `WhileStatement`

**Tarea**

Eliminar el análisis compartido que trata do-while como while.

No duplicar toda la lógica del loop: compartir helpers, pero mantener distinta topología de control.

**Criterio de terminado**

El orden semántico es body → condition.

---

## CC1-211 — Eliminar el path de cero iteraciones de do-while

**Tarea**

El estado posterior de un do-while no puede incluir por defecto el estado de entrada como path que “no ejecutó el body”.

Sólo deben contribuir:

- salidas por `break`;
- salida por condición false después de al menos una ejecución;
- otras rutas estructuradas legales.

**Criterio de terminado**

Una asignación garantizada en el body puede ser definitely assigned después de un do-while si ninguna salida previa evita esa asignación.

---

## CC1-212 — Corregir continue edges de do-while

**Tarea**

Todo `continue` dentro del do-while debe contribuir al estado de entrada de la condición, no al inicio del body.

**Criterio de terminado**

La condición se analiza con el join correcto entre fallthrough del body y continues.

---

## CC1-213 — Añadir matriz de do-while

**Tests obligatorios**

- local asignado en body y leído en condición;
- local asignado en body y leído después del loop;
- break antes de assignment;
- break después de assignment;
- continue antes de assignment;
- continue después de assignment;
- nested do-while;
- do-while con RAII local;
- do-while con move;
- do-while dentro de try/finally.

**Criterio de terminado**

Semantic analysis y runtime O0/O2 coinciden.

---

## CC1-214 — Auditar `switch` con la infraestructura común de flow

**Tarea**

Verificar que `switch` conserva:

- snapshot común por sección;
- join de secciones alcanzables;
- estado de `break`;
- comportamiento cuando no existe `default`;
- ausencia de fall-through.

**Criterio de terminado**

Los tests de switch incluyen definite assignment y ownership, no sólo valores de retorno.

---

## GATE 2

No avanzar hasta que:

- `?:`, `&&`, `||` y `do...while` modelen el mismo CFG observable que LLVM;
- no exista ningún test conocido donde el backend inicialice un local y oculte un error de definite assignment;
- moves condicionales estén cubiertos.

---

# FASE 3 — Endurecer structured exits y EH

---

## CC1-300 — Definir el invariant de salida de catch

**Invariant obligatorio**

Toda salida de una cláusula catch que haya manejado una excepción debe limpiar la current exception exactamente una vez antes de abandonar el catch semántico.

Las formas de salida a cubrir son:

- fallthrough;
- return;
- break;
- continue;
- salida hacia cleanup posterior.

**Criterio de terminado**

El invariant está documentado en MIR/backend y no depende de llegar a un bloque LLVM por casualidad.

---

## CC1-301 — Representar exception-clear como obligación explícita de cleanup

**Tarea**

Mover conceptualmente la responsabilidad de `__nativesharp_exception_clear` desde un único bloque de fallthrough a la maquinaria de structured exits/cleanup.

La representación debe permitir que return/break/continue sepan que abandonar un catch tiene esa obligación.

**Criterio de terminado**

Todos los exits legales desde catch atraviesan la misma semántica de clear.

---

## CC1-302 — Evitar double-clear

**Tarea**

Una vez centralizada la limpieza, asegurar que la ruta normal de catch no ejecuta el clear dos veces.

**Criterio de terminado**

Cada path de salida contiene exactamente una limpieza lógica.

---

## CC1-303 — Añadir fixture loop → catch → switch

**La fixture debe contener**

- loop exterior;
- excepción atrapada;
- switch dentro del catch;
- un case con break local al switch;
- otro case con continue al loop;
- default;
- mutaciones observables antes y después del switch.

**Criterio de terminado**

O0 y O2 producen exactamente los mismos contadores/estado esperado.

---

## CC1-304 — Añadir fixture direct continue desde catch

**Tarea**

Cubrir el mismo problema sin switch.

**Razón**

Distinguir bug general de EH de bug específico de switch.

**Criterio de terminado**

La excepción manejada queda limpiada y el loop continúa correctamente.

---

## CC1-305 — Añadir fixture direct break desde catch

**Tarea**

Cubrir un catch dentro de loop con salida por break.

**Criterio de terminado**

El loop se abandona y no queda current exception residual.

---

## CC1-306 — Añadir fixture return desde catch

**Tarea**

Probar return de valor y return void desde catch.

**Criterio de terminado**

La ruta de retorno ejecuta todos los cleanups y el exception clear correspondiente.

---

## CC1-307 — Añadir fixture de dos funclet depths reales

**Escenario obligatorio**

- loop exterior;
- try/finally;
- exceptional unwind;
- ejecución del finally como cleanup funclet;
- switch dentro del finally;
- break local al switch;
- statements posteriores al switch dentro del finally;
- catch exterior que observa que el finally terminó correctamente.

**Criterio de terminado**

El break del switch permanece dentro del cleanup funclet y no salta al target del loop.

---

## CC1-308 — Auditar `LoopTargets`

**Tarea**

Revisar cada lugar donde se crea un frame de loop o switch.

Para cada frame registrar correctamente:

- break target;
- continue target;
- break funclet depth;
- continue funclet depth.

**Criterio de terminado**

No existe una construcción que copie una depth por conveniencia cuando los targets reales viven en profundidades distintas.

---

## CC1-309 — Actualizar comentarios de EH al diseño actual

**Tarea**

Eliminar comentarios que sigan suponiendo que los bodies de catch viven dentro de catch funclets.

**Criterio de terminado**

La documentación inline describe el lowering posterior a B-006/B-007/multicatch.

---

## CC1-310 — Reejecutar toda la matriz histórica EH

**Debe incluir**

- finally O0/O2;
- using;
- owned params;
- RAII;
- rethrow;
- multicatch;
- B-006;
- B-007;
- todas las regresiones Stage45;
- nuevos tests CC1.

**Criterio de terminado**

No se acepta un arreglo de exception-clear que reabra un bug antiguo.

---

## GATE 3

EH no se considera cerrado hasta que:

- todos los exits desde catch limpien current exception;
- el fixture catch/switch/loop pase O0/O2;
- el fixture de cleanup funclet con switch pase O0/O2;
- la matriz histórica EH siga verde.

---

# FASE 4 — Completar las features de memoria/pointers de Parte 1 en el pipeline moderno

Esta fase no agrega lenguaje nuevo.

Migra features que ya existen en parser/pipeline clásico.

---

# 4A — `sizeof`

## CC1-400 — Unificar el tipo de resultado de `sizeof`

**Tarea**

El pipeline moderno debe usar el mismo tipo que el frontend clásico para `sizeof`: `nuint`.

**Criterio de terminado**

No existe divergencia `ulong` vs `nuint`.

---

## CC1-401 — Crear modo de análisis unevaluated para `sizeof(expression)`

**Tarea**

El operando debe:

- resolver nombres;
- resolver miembros;
- resolver tipos;
- validar que la expresión tenga un tipo sized válido;

pero no debe:

- ejecutar definite-assignment effects;
- registrar una asignación;
- mover ownership;
- crear borrow state persistente;
- registrar runtime side effects;
- producir cleanup;
- producir traps de ejecución por el simple hecho de estar dentro de sizeof.

**Criterio de terminado**

El estado de flujo antes y después de analizar el operando es idéntico.

---

## CC1-402 — Reutilizar el layout canónico para calcular `sizeof`

**Tarea**

No hardcodear tamaños en el OOP HIR.

Usar el mismo servicio de target layout utilizado por ABI/object layout.

**Criterio de terminado**

Structs de 1,2,3,4,8,12,16,24,64,80 bytes producen el mismo resultado que la matriz ABI.

---

## CC1-403 — Bajar `sizeof` directamente a constante HIR

**Tarea**

No crear una operación runtime si no es necesaria.

Las dos formas deben llegar a HIR como constante `nuint`.

**Criterio de terminado**

MIR/LLVM no necesitan saber que originalmente había un `sizeof`.

---

## CC1-404 — Añadir tests modernos de `sizeof`

**Tests obligatorios**

- primitive;
- pointer;
- enum;
- struct de tamaño no potencia de dos;
- `sizeof(expression)` con incremento;
- `sizeof(expression)` con assignment;
- `sizeof(expression)` con call que tendría side effect;
- invalid `void`;
- invalid function designator;
- uso en const;
- uso en switch label si las reglas del lenguaje lo permiten.

**Criterio de terminado**

Los side effects no alteran ejecución ni flow state.

---

# 4B — `stackalloc`

## CC1-410 — Igualar la validación semántica moderna con la clásica

**Tarea**

Implementar en el semantic model moderno exactamente las validaciones maduras del frontend clásico.

**Debe validar**

- tipo completo;
- tipo sized;
- no void;
- no array element type;
- count integral no bool;
- negative constant count;
- byte-size overflow.

**Criterio de terminado**

La misma entrada recibe el mismo diagnóstico en ambos frontends mientras el clásico siga existiendo.

---

## CC1-411 — Añadir nodo OOP HIR de stack allocation

**Tarea**

Representar explícitamente:

- element type;
- count;
- result pointer type.

**Criterio de terminado**

`StackallocExpression` deja de alcanzar el default “unsupported OOP expression kind”.

---

## CC1-412 — Añadir nodo OOP MIR de stack allocation

**Tarea**

Conservar element type y count para el backend.

**Criterio de terminado**

No se modela stackalloc como heap allocation ni como array object.

---

## CC1-413 — Añadir lowering LLVM moderno de stack allocation

**Tarea**

Reutilizar las reglas probadas del lowering clásico:

- validación de nonnegative count cuando corresponda;
- overflow de byte-size;
- `alloca` de array;
- alineación del element type.

**Criterio de terminado**

No se llama al heap allocator.

---

## CC1-414 — Añadir fixture O0/O2 de stackalloc moderno

**Debe medir**

- escritura;
- lectura;
- indexación;
- count runtime;
- count cero;
- struct element type;
- interacción con function calls.

**Criterio de terminado**

La memoria vive en stack y la ejecución es estable O0/O2.

---

# 4C — raw `new T`

## CC1-420 — Definir el raw scalar allocation moderno como feature soportada

**Tarea**

Migrar el comportamiento existente del frontend clásico al pipeline OOP moderno.

No confundirlo con object construction de clases.

**Criterio de terminado**

Un raw `new T` de tipo permitido deja de ser aceptado por semántica y rechazado por HIR.

---

## CC1-421 — Añadir representación HIR de raw allocation

**Debe conservar**

- element type;
- result pointer;
- cantidad implícita de un elemento.

**Criterio de terminado**

La operación es distinguible de `new[]` y de object construction.

---

## CC1-422 — Añadir MIR y LLVM lowering de raw allocation

**Tarea**

Usar el allocator nativo existente con el tamaño real de un elemento.

**Criterio de terminado**

El pointer resultante puede ser dereferenciado y liberado mediante la semántica raw existente.

---

## CC1-423 — Añadir tests raw new/delete modernos

**Tests obligatorios**

- primitive;
- struct;
- write/read;
- delete;
- nullptr paths;
- O0/O2.

**Criterio de terminado**

La feature funciona en el mismo pipeline que OOP/generics/runtime.

---

# 4D — pointer arithmetic

## CC1-430 — Migrar las reglas de tipos del MIR clásico

**Reglas mínimas a preservar**

- pointer + integral → pointer;
- integral + pointer → pointer;
- pointer - integral → pointer;
- matching pointer - pointer → `nint`;
- pointee debe ser completo y sized;
- `void*` arithmetic no permitido;
- tipos incompatibles en pointer difference rechazados;
- bool no es offset integral válido si la semántica clásica lo prohíbe.

**Criterio de terminado**

Las reglas modernas están documentadas y testeadas.

---

## CC1-431 — Añadir binding semántico explícito de pointer arithmetic

**Tarea**

No hacer pasar pointers por `commonNumericType`.

Crear una ruta propia en `analyzeBinary`.

**Criterio de terminado**

El error/resultado se decide antes de HIR con tipos exactos.

---

## CC1-432 — Añadir HIR moderno de pointer add/subtract/difference

**Tarea**

Representar las tres operaciones como operaciones semánticamente distintas.

**Criterio de terminado**

El backend no necesita inferir si una resta es pointer-minus-integer o pointer-minus-pointer.

---

## CC1-433 — Añadir MIR moderno de pointer arithmetic

**Tarea**

Preservar:

- pointee type;
- offset type;
- result type.

**Criterio de terminado**

Existe una frontera backend-neutral comparable a los opcodes del MIR clásico.

---

## CC1-434 — Añadir LLVM lowering moderno usando element stride

**Tarea**

La aritmética debe avanzar por elementos, no por bytes, salvo que el pointee sea byte-sized.

**Criterio de terminado**

Un pointer a struct de 12 bytes avanza 12 bytes por incremento unitario.

---

## CC1-435 — Implementar pointer increment/decrement sobre la misma infraestructura

**Tarea**

No crear una cuarta semántica.

Prefix/postfix pointer increment/decrement debe reutilizar pointer add/subtract con uno.

**Criterio de terminado**

Pre/post semantics y side effects son correctos.

---

## CC1-436 — Añadir matriz moderna de pointer arithmetic

**Tests obligatorios**

- primitive pointer add;
- struct pointer add;
- integer + pointer;
- pointer subtract integer;
- pointer difference;
- prefix increment;
- postfix increment;
- decrement;
- mismatched pointee rejection;
- void pointer rejection;
- boundary type checks;
- O0/O2.

**Criterio de terminado**

La matriz moderna iguala o supera la cobertura clásica.

---

## GATE 4

No cerrar esta fase hasta que:

- `sizeof`;
- `stackalloc`;
- raw `new`;
- pointer arithmetic;

sean features reales del pipeline moderno o tengan un diagnóstico deliberado anterior al HIR.

No se permite que una feature sea “aceptada semánticamente” y muera en el default del OOP HIR.

---

# FASE 5 — Eliminar el mini-parser de type seeds

---

## CC1-500 — Documentar exactamente qué información necesita el parser antes del full parse

**Debe quedar limitado a**

- namespace;
- using alias;
- class name;
- struct name;
- interface name;
- enum name;
- delegate name;
- generic arity cuando sea necesaria para disambiguación.

**Criterio de terminado**

No se agregan cuerpos, statements ni expresiones a esta pass.

---

## CC1-501 — Crear declaration/header pass real

**Tarea**

Implementar una primera pasada que use las reglas de declaración del parser, no heurísticas de “identifier seguido de `<`”.

La pass debe poder ignorar bodies de tipos y métodos después de haber capturado sus headers.

**Criterio de terminado**

El lenguaje de declaraciones no está duplicado manualmente en `ProjectCompilation.cpp`.

---

## CC1-502 — Migrar `using` aliases a la declaration pass

**Tarea**

Eliminar el scanner independiente `collectUsingAliases`.

**Criterio de terminado**

Aliases y type declarations nacen de la misma pasada de headers.

---

## CC1-503 — Migrar delegate discovery

**Tarea**

La declaration pass debe extraer el nombre correcto aun cuando:

- return type sea genérico;
- delegate sea genérico;
- namespace sea nested;
- type arguments sean builtins;
- declaration venga de ProjectReference.

**Criterio de terminado**

No existe lógica especial basada en mirar el token siguiente al identifier.

---

## CC1-504 — Eliminar `collectTypeSeedsInScope`

**Tarea**

Una vez que la nueva pass cubra sus consumidores, borrar el scanner heurístico.

**Criterio de terminado**

No quedan dos implementaciones de la gramática de declaraciones.

---

## CC1-505 — Crear matriz cross-file/cross-project

**Tests obligatorios**

- delegate simple;
- generic delegate;
- generic return type;
- nested generic return type;
- namespace;
- alias;
- ProjectReference;
- builtin type argument;
- user type argument.

**Criterio de terminado**

Todos parsean sin depender del orden de archivos.

---

## GATE 5

No se acepta “parchear el scanner” como cierre.

El milestone exige retirar la duplicación.

---

# FASE 6 — Establecer un frontend canónico user-facing

---

## CC1-600 — Declarar `ProjectCompilation` como frontend canónico de NativeSharp 0.1

**Tarea**

Documentar que toda compilación de usuario debe atravesar el pipeline moderno.

**Criterio de terminado**

La documentación ya no presenta dos caminos equivalentes.

---

## CC1-601 — Hacer que el modo single-file use el pipeline moderno

**Tarea**

Un archivo suelto debe poder envolverse internamente como proyecto sintético mínimo y atravesar el mismo semantic model/HIR/MIR que un `.nsproj`.

**Criterio de terminado**

`nsc --check archivo.ns` y la compilación del mismo archivo dentro de proyecto producen las mismas decisiones semánticas.

---

## CC1-602 — Dejar `FrontendCompilation` clásico sólo como infraestructura legacy/test

**Tarea**

No añadirle nuevas features de lenguaje después de CC1.

**Criterio de terminado**

Ningún command user-facing depende de él.

---

## CC1-603 — Reclasificar tests clásicos

**Tarea**

Separar tests que validan infraestructura histórica de tests que definen el lenguaje actual.

**Criterio de terminado**

Un test legacy no puede afirmar por sí solo que una sintaxis pertenece o no pertenece al NativeSharp moderno.

---

## CC1-604 — Añadir conformance tests single-file vs project

**Features mínimas**

- ternary;
- do-while;
- switch;
- sizeof;
- stackalloc;
- raw new;
- pointer arithmetic;
- ownership;
- EH.

**Criterio de terminado**

Mismo source → mismo resultado semántico en modo single-file y project.

---

## GATE 6

Al cerrar esta fase debe existir una sola respuesta a:

> “¿Qué lenguaje acepta `nsc`?”

---

# FASE 7 — Preparar la futura independencia de LLVM sin intentar terminarla aquí

CC1 no implementará NSASM como backend.

Sí debe eliminar varias formas de semantic leakage hacia LLVM.

---

## CC1-700 — Prohibir nuevas decisiones de lenguaje dentro de `ProjectOopLlvmLowerer`

**Tarea**

Toda nueva decisión sobre:

- tipos;
- ownership;
- constant folding;
- case normalization;
- pointer operation kind;
- structured-exit obligations;

debe llegar resuelta en MIR.

**Criterio de terminado**

LLVM lowerer baja decisiones, no las inventa.

---

## CC1-701 — Hacer explícitas en MIR las nuevas operaciones migradas

**Debe incluir**

- stack allocation;
- raw allocation;
- pointer add;
- pointer subtract;
- pointer difference;
- catch-exit cleanup/obligation si esa representación termina viviendo en MIR.

**Criterio de terminado**

Es posible describir la semántica de esas operaciones sin mencionar LLVM APIs.

---

## CC1-702 — Crear inventario de decisiones todavía LLVM-specific

**Tarea**

Listar, función por función, qué decisiones de `ProjectOopLlvmLowerer` todavía serían necesarias reproducir para un backend alternativo.

**Categorías**

- CFG construction;
- PHI construction;
- ABI lowering;
- EH/funclets;
- virtual dispatch;
- aggregate carriers;
- runtime calls;
- object construction;
- cleanup edges.

**Criterio de terminado**

El próximo milestone de LIR/backend-neutral empieza con inventario cerrado, no con exploración abierta.

---

## CC1-703 — No implementar todavía el nuevo LIR

**Tarea**

Cerrar CC1 con requisitos del futuro LIR definidos, pero sin mezclar ese rediseño masivo con los fixes de correctness.

**Criterio de terminado**

Existe un handover preciso para el siguiente milestone arquitectónico.

---

# FASE 8 — Reproducibilidad, identidad y cierre

---

## CC1-800 — Sincronizar build identity

**Tarea**

Mantener NativeSharp language release 0.1, pero actualizar:

- compiler development identifier;
- compiler stage identifier;
- CMake description;
- tests de `--version`;
- metadata producer version cuando corresponda.

**Criterio de terminado**

Un binario construido desde este árbol puede identificarse inequívocamente.

---

## CC1-801 — Ejecutar build desde directorio completamente vacío

**Tarea**

No validar sólo incremental.

Eliminar build output y realizar:

- configure;
- build;
- install;
- tests.

**Criterio de terminado**

El resultado no depende de artefactos previos.

---

## CC1-802 — Ejecutar suite completa warnings-as-errors

**Criterio de terminado**

100% PASS.

---

## CC1-803 — Ejecutar fixtures Win64 O0 y O2

**Debe incluir**

- todas las fixtures históricas;
- nuevas fixtures CC1;
- EH;
- constants;
- flow;
- memory/pointers.

**Criterio de terminado**

O0 y O2 coinciden en comportamiento.

---

## CC1-804 — Ejecutar verifier/COFF/PE checks

**Debe comprobar**

- LLVM verifier;
- object generation;
- PE generation;
- `.pdata/.xdata` en escenarios EH;
- ausencia de CLR header;
- exit codes esperados.

**Criterio de terminado**

No hay regresión de toolchain.

---

## CC1-805 — Regenerar manifest al final, no antes

**Tarea**

Regenerar hashes sólo cuando el árbol que se pretende cerrar ya no cambiará.

**Criterio de terminado**

`verify_manifest.py` reporta cero fallos.

---

## CC1-806 — Eliminar o archivar documentos contradictorios

**Tarea**

Mover triages intermedios obsoletos a `docs/history` y dejar un único documento de estado actual.

**Criterio de terminado**

No existe simultáneamente un documento raíz diciendo “bug vivo” y otro diciendo “cerrado” sin contexto histórico.

---

## CC1-807 — Crear reporte de cierre del milestone

**Debe incluir**

- todos los IDs CC1;
- commit/revision final;
- tests añadidos;
- bugs cerrados;
- cambios de arquitectura;
- limitaciones deliberadamente diferidas;
- manifest result;
- build identity;
- Windows execution evidence.

**Criterio de terminado**

Otra persona puede leer un solo documento y saber exactamente qué cambió y qué quedó fuera.

---

# PARTE VI — MATRIZ DE PRUEBAS OBLIGATORIA DEL MILESTONE

## A. Constant semantics

Debe existir cobertura para:

- todos los integer widths;
- signed/unsigned;
- narrowing casts;
- widening casts;
- enum constants;
- const fields;
- shifts;
- division;
- modulo;
- overflow policy;
- invalid arithmetic traps;
- `char`.

## B. Flow semantics

Debe existir cobertura para:

- if;
- ternary;
- `&&`;
- `||`;
- while;
- do-while;
- for;
- switch;
- nested combinations;
- break;
- continue;
- return;
- unreachable paths.

Para cada construcción se deben probar:

- definite assignment;
- ownership;
- moves;
- RAII;
- cleanup.

## C. EH

Debe existir cobertura para:

- catch normal fallthrough;
- return from catch;
- break from catch;
- continue from catch;
- switch inside catch;
- finally normal;
- finally exceptional;
- switch inside exceptional finally;
- multicatch;
- rethrow;
- nested try;
- RAII across catch.

## D. Parte 1 migrada al moderno

Debe existir cobertura de proyecto/OOP para:

- sizeof type;
- sizeof expression;
- stackalloc;
- raw new;
- delete;
- address-of;
- dereference;
- indexing;
- pointer add/subtract/difference;
- pointer inc/dec.

## E. Parsing/project model

Debe existir cobertura para:

- multi-file;
- namespace;
- using alias;
- delegate;
- generic delegate;
- generic return type;
- nested generic return;
- ProjectReference.

---

# PARTE VII — ORDEN DE EJECUCIÓN RECOMENDADO

El orden recomendado es estricto:

1. Fase 0 — baseline.
2. Fase 1 — constants y `char`.
3. Fase 2 — flow analysis.
4. Fase 3 — EH structured exits.
5. Fase 4 — completar Parte 1 en pipeline moderno.
6. Fase 5 — declaration/header pass.
7. Fase 6 — frontend canónico.
8. Fase 7 — preparar frontera backend-neutral.
9. Fase 8 — cierre/reproducibilidad.

Hay trabajo que puede ejecutarse en paralelo, pero sólo dentro de límites seguros.

### Paralelismo permitido después de Fase 0

- equipo/instancia A: Fase 1 constants;
- equipo/instancia B: Fase 2 flow;
- equipo/instancia C: Fase 3 EH tests e invariant;
- equipo/instancia D: Fase 4 `sizeof`/stackalloc/raw new/pointers;
- equipo/instancia E: Fase 5 declaration pass;
- equipo/instancia F: preparar tests y reproducibilidad.

### Integración obligatoria

Antes de Fase 6 todas las ramas deben integrarse y pasar una suite completa conjunta.

No debe hacerse el frontend canónico sobre ramas semánticas todavía divergentes.

---

# PARTE VIII — DEFINITION OF DONE GLOBAL

CC1 sólo está terminado si se cumplen simultáneamente todos estos puntos:

- no hay P1 de este documento abierto;
- `switch` usa folding integral compartido y tipado;
- `char` es coherente en todo el compilador;
- ternary, short-circuit y do-while tienen flow correcto;
- catch structured exits limpian exception TLS de forma explícita;
- las fixtures EH nuevas pasan O0/O2;
- `sizeof`, `stackalloc`, raw new y pointer arithmetic funcionan en el pipeline moderno;
- ninguna feature declarada soportada cae en el default de OOP HIR;
- el scanner heurístico de type seeds fue retirado;
- el modo single-file y project usan el mismo frontend canónico;
- el pipeline clásico queda formalmente legacy;
- clean build PASS;
- warnings-as-errors PASS;
- suite completa PASS;
- fixtures O0/O2 PASS;
- verifier/PE/EH checks PASS;
- manifest 0 failures;
- build identity sincronizado;
- documentación histórica archivada;
- reporte de cierre producido.

---

# PARTE IX — QUÉ NO DEBE CONSIDERARSE UN CIERRE

No aceptar como “resuelto” ninguno de los siguientes casos:

- “el test nuevo pasa en O0” sin O2;
- “LLVM verifier pasa” sin ejecución cuando el bug es de control-flow;
- “agregamos el case al HIR” sin migrar validación semántica;
- “parcheamos el generic delegate” dejando el mini-parser;
- “el backend trunca correctamente” cuando el frontend debía detectar duplicado;
- “la variable devuelve cero” cuando el semantic analyzer debía rechazar el read;
- “el catch funciona por fallthrough” sin probar return/break/continue;
- “el manifest se regeneró” antes de acabar los cambios;
- “single-file y project compilan ambos” si aceptan lenguajes diferentes;
- “LLVM es sólo backend” mientras decisiones semánticas sigan naciendo dentro de `ProjectOopLlvmLowerer`.

---

# PARTE X — RESULTADO ESPERADO DESPUÉS DE CC1

Al cerrar este milestone, NativeSharp no tendrá necesariamente más sintaxis que hoy.

Tendrá algo más importante:

- semántica integral única;
- control-flow semántico consistente;
- ownership más confiable;
- EH estructurado con invariants medibles;
- memoria/pointers de Parte 1 recuperados en el pipeline moderno;
- parser de proyecto menos heurístico;
- un solo frontend user-facing;
- mejor frontera hacia un futuro LIR;
- build reproducible;
- tests que miden interacciones reales.

Éste es el punto donde el proyecto deja de crecer principalmente “hacia afuera” y empieza a consolidarse “hacia adentro”.

Eso es exactamente lo que necesita un compilador que ya dejó de ser pequeño.

Después de CC1, el siguiente milestone arquitectónico puede atacar con mucha más seguridad:

- CFG moderno propio;
- lowering ABI explícito;
- LIR backend-neutral;
- LLVM como backend real;
- NSASM como segundo backend;
- y, más adelante, NSLink independiente.

Pero CC1 debe ocurrir primero.

---

# PARTE XI — CONCLUSIÓN DE MILESTONE

La recomendación final es no tratar este trabajo como una colección de bugs sueltos.

El milestone tiene un hilo conductor único:

> **cada concepto del lenguaje debe tener una sola semántica y todos los pipelines deben consumir esa misma verdad.**

`char` no puede ser 8-bit para LLVM y 16-bit para constant conversion.

Un ternario no puede ser branchy para LLVM y secuencial para definite assignment.

Un `&&` no puede ser short-circuit en runtime e incondicional en ownership analysis.

Un do-while no puede ser body-first en ejecución y condition-first en semántica.

Un case label no puede tener un valor en el frontend y otro después del cast en LLVM.

Un catch no puede limpiar current exception sólo si cae por el bloque “normal”.

Un `sizeof` no puede ser unevaluated en el frontend clásico y side-effecting en el moderno.

Una feature no puede existir en el parser, ser aceptada por semántica y desaparecer al entrar en OOP HIR.

Un proyecto no puede depender de un segundo mini-parser para descubrir qué identifiers son tipos.

Y `nsc` no debe tener dos respuestas diferentes a la pregunta “¿qué es NativeSharp?”.

CC1 cierra exactamente esas grietas.

Una vez cerrado, el compilador estará en una posición mucho más fuerte para continuar con nsbind, bindings, windowing, graphics, NSASM, NSLink y la independencia progresiva de LLVM sin acumular otra capa de semántica duplicada sobre la actual.
