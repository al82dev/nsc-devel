# Tanda operativa 29 — Ergonomía procedural y control de flujo

Fecha: 2026-09-30

Rama: `development`

Estado: implementación terminada y validación dirigida O0 correcta; baseline reservado para la
tanda 53 y ejecución externa de Alberto

## Objetivo

Cerrar cinco limitaciones concretas observadas durante la conversión de código real sin crear una
segunda semántica para funciones libres ni relajar las reglas de ownership:

1. permitir `new C(...)` dentro de una función libre de namespace;
2. preservar correctamente parámetros `out` a través de llamadas recursivas;
3. aceptar listas ordenadas de expresiones en inicialización e incremento de `for`;
4. convertir interpolaciones con holes runtime en operaciones normales de `String`;
5. aceptar una sentencia embebida —incluida la sentencia vacía— tras `if` y `else`.

`while`, `do`, `for`, `foreach`, `using`, `switch`, `match`, `try`/`catch`/`finally` y `asm` no
cambian su requisito de bloque.

## Cambios funcionales

### Construcción desde funciones libres

`ProjectConstructionModel` recorre ahora cuerpos de funciones libres publicados por
`ProjectNamespaceFunctionModel`. La identidad del llamador usa el mismo ordinal sintético que el
modelo de cuerpos y HIR: primero los miembros de clase y después las funciones de namespace.

La accesibilidad no finge un tipo contenedor inexistente. Desde una función libre:

- un constructor `public` es visible;
- un constructor `internal` es visible únicamente dentro del mismo proyecto;
- `private` y `protected` no son visibles;
- el constructor implícito sigue la visibilidad del tipo y el límite de proyecto.

El resultado es un `ObjectConstructionBinding` ordinario. Resolución de overload, ownership,
layout, HIR, MIR y LLVM continúan por las rutas que ya usan los métodos.

### `out` recursivo

HIR resolvía parámetros by-reference consultando sólo `ClassMemberSymbol`. Una función libre usa
un ordinal sintético, por lo que su parámetro `out` se degradaba erróneamente a valor: la llamada
recursiva recibía la dirección correcta, pero la escritura posterior no seguía el puntero del
llamador.

El lowerer consulta ahora `MethodParameterSymbol` desde miembro o función de namespace. Los
parámetros `ref`, `out` e `in` libres producen el mismo `ParameterValue` de tipo puntero y el mismo
`PointerLoad` que sus equivalentes dentro de una clase. La prueba ejecuta Euclides extendido con
varias profundidades, destinos intercambiados y ambos `out` aliasados al mismo local.

### Listas de expresiones en `for`

El parser acepta:

```text
for (a = 1, b = 2; condición; a = a + 1, b = b + 2) { ... }
```

Una lista se representa internamente como un `BlockStatement` sintético de
`ExpressionStatement`; no se introduce el operador coma en expresiones generales. Body model,
HIR y LLVM recorren esos elementos de izquierda a derecha. Las declaraciones de la forma
tradicional del `for` conservan su camino anterior.

### Interpolación runtime

Las interpolaciones totalmente constantes conservan `InterpolatedStringExpression` y el folding
existente. Cuando algún hole depende de runtime, el parser:

- conserva offset y longitud del texto del hole para diagnósticos;
- vuelve a lexear y parsear el hole una sola vez como expresión NativeSharp real;
- construye concatenación asociada a la izquierda para fijar orden de evaluación;
- usa llamadas ordinarias a `NativeSharp.Core.String`, de modo que overloads, ownership,
  temporales, cleanup, excepciones y la comprobación `noheap` pasan por los modelos normales;
- conserva escapes dentro del hole para que el lexer de la expresión los procese, en vez de que
  el lexer exterior los destruya antes de tiempo.

Formatos implementados:

| Formato | Lowering runtime |
|---|---|
| vacío | `String.ToString(value)` |
| `D`/`Dn` | `ToString` y `ZFill`; conserva el signo |
| `X`/`Xn`, `x`/`xn` | `ToHexString(value, n, upperCase)`; conserva ancho del tipo para negativos |
| `F`/`Fn` | `ToStringFixed`; `F` usa dos decimales |
| `G`/`g` | `ToString` |

Anchura/precisión se limita a 64. `G` con precisión se rechaza por ahora: los overloads actuales
de dos argumentos significan radix para enteros y cifras significativas para floats; elegir uno
por sintaxis produciría resultados silenciosamente incorrectos. Los formatos desconocidos
reciben un diagnóstico causal.

`ZFill` se añade a la lista cerrada de operaciones de `NativeSharp.Core.String` que pueden tomar
prestado un owner temporal fresco durante la expresión completa. No se relaja esta regla para
métodos de usuario.

### `if`/`else` sin llaves

El parser acepta una sentencia embebida y la envuelve en un `BlockStatement` sintético. Así los
passes posteriores conservan scopes, flow, cleanup, drops y source range con la misma estructura
que un bloque explícito. Un `else` se asocia al `if` abierto más cercano y `else if` sigue siendo
un `IfStatement` anidado.

Un `;` aislado se representa como bloque vacío. Las declaraciones locales y deconstrucciones sin
llaves se rechazan con el diagnóstico: `a declaration used as an if or else body requires an
explicit '{ ... }' block`.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectConstructionModel.h` y
  `src/Frontend/ProjectConstructionModel.cpp`: funciones libres, ordinales sintéticos y
  accesibilidad desde proyecto.
- `src/Frontend/ProjectCompilation.cpp`: entrega el modelo de funciones de namespace al modelo de
  construcción.
- `src/Frontend/ProjectOopHirModel.cpp`: parámetros by-reference libres y secuencias de `for`.
- `src/Frontend/ProjectMethodBodyModel.cpp`: análisis ordenado de listas de `for` y préstamo
  temporal cerrado para `String.ZFill`.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: lowering ordenado de secuencias sintéticas del `for`.
- `include/nativesharp/Language/ExpressionErgonomics.h` y
  `src/Language/ExpressionErgonomics.cpp`: posición exacta de holes y parsing sin perder offsets.
- `src/Lexer/Lexer.cpp`: preservación de escapes dentro de holes runtime.
- `include/nativesharp/Parser/Parser.h` y `src/Parser/Parser.cpp`: sentencia embebida, lista de
  expresiones y desazucaración de interpolación.
- `tests/unit/NativeSharp3T01ExpressionTests.cpp`, `tests/unit/ParserTests.cpp` y
  `tests/unit/ProjectConstructionModelTests.cpp`: actualización de contratos históricos.
- `CMakeLists.txt`: registro de la prueba dirigida T29.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T29ProceduralErgonomicsTests.cpp`: fixture O0 autocontenido que compila y
  ejecuta los cinco features, prueba formatos runtime, aliasing de `out`, dangling-else, cuerpo
  vacío, rechazo de declaración, rechazo de formato inválido y comportamiento `noheap`.
- `docs/3.0/TANDA-OPERATIVA-29-ERGONOMIA-PROCEDURAL.md`: expediente de implementación, decisiones,
  archivos, mejora y validación de este commit.

El fixture copia temporalmente sólo `String.ns` y `NumericLimits.ns` bajo su directorio de
evidencia y los declara como edición 3. Esto evita usar `C:\native-sharp`, evita mezclar ediciones
2/3 dentro del grafo y elimina el temporal cuando la prueba pasa.

## Validación dirigida ejecutada

1. `nsc` construido en `A:\nsc-devel\build-clang`, configuración Debug/O0: correcto.
2. `nsc_ns3_t29_procedural_ergonomics_tests`: `PASS: 6 checks`.
3. `nsc_ns3_t01_expression_tests`: `PASS: 27 checks`.
4. `nsc_parser_tests`: `PASS: 23 checks`.
5. `nsc_project_construction_model_tests`: `PASS: 64 checks`.
6. `git diff --check`: correcto antes del commit.

No se ejecutó baseline, CTest general, O1/O2/O3, libraries, instalación, deploy, componentes
externos ni operaciones remotas.

## Mejora concreta

Código procedural convertido desde C/C++ ya no necesita envolver construcciones en clases,
reescribir recursión con retornos artificiales, separar manualmente encabezados de `for`, reducir
interpolaciones a constantes o añadir llaves a cada rama simple. Las cinco formas llegan a PE O0
usando los mismos modelos de ownership, flow, cleanup y ABI que el resto del lenguaje.

## Límites deliberados

- Sólo funciones libres dentro de namespaces participan en este cierre; no se inventa un nuevo
  modelo para declaraciones globales fuera del sistema de namespaces.
- Las listas con coma existen sólo en inicialización e incremento de `for`; no se agrega un
  operador coma general.
- Runtime interpolation requiere que el proyecto 3.0 suministre la superficie correspondiente de
  `NativeSharp.Core.String`; el cierre de paquetes/ediciones del Core instalado pertenece a T44.
- `G` con precisión queda rechazado hasta disponer de una API inequívoca.
- `while`, `do`, `for`, `foreach` y `using` siguen exigiendo bloque.
- La integración de stepping/CodeView se validará en T36–T38, no se declara certificada aquí.
