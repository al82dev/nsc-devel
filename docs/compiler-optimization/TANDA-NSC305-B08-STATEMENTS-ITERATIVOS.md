# NSC 3.0.5 — B08: statements iterativos

Fecha: 2026-10-17
Rama: `development`
Estado: B08 y B08F terminados; límite de parser separado como R04

## Objetivo

Separar la familia de statements del monolito de method bodies y sustituir recorridos recursivos
por estado explícito en heap, sin modificar sintaxis, semántica, diagnostics, ownership, ABI,
layout, formatos ni NSMETA.

## Extracción física

El commit `2a7899e32` movió 4,578 líneas de statements, scopes de control, exits y cleanup a
`src/Frontend/Bodies/StatementAnalyzer.cpp`. El cambio fue mecánico: el target focal conservó
`PASS: 76 checks` antes de iniciar la transformación algorítmica.

La unidad contiene `analyzeBlock`, `analyzeStatement`, variables, `using`, `try`, `throw`,
`return`, `for`, `foreach`, `switch`, cleanup regions y structured exits. Se registró de manera
explícita en `nsc_frontend`.

## Recorridos convertidos

### Bloques léxicos

`analyzeBlock` ya no se llama a sí mismo para bloques hijos directos. Usa un vector de
`BlockFrame` que conserva:

- nodo del bloque;
- scope activo;
- índice del siguiente statement;
- profundidad de cleanup al entrar;
- frontera de captura de `defer`;
- snapshot que debe restaurarse al terminar una sonda de código inalcanzable.

La entrada y salida de cada frame preserva el orden anterior: creación de scope, captura de
`defer`, análisis en orden fuente, diagnóstico de scoped-child borrow, continuaciones normales de
cleanup, restauración de profundidad, cobertura de `match` y restauración del flujo inalcanzable.

### Patrones de deconstruction en statements

La lambda autorrecursiva `bindPattern` fue sustituida por `PatternWork`. Los hijos se apilan en
orden inverso para conservar exactamente el recorrido preorder de izquierda a derecha, incluidos
ordinals de locals, shadowing events y orden de diagnostics.

## Prueba profunda

El runner focal existente incorpora un fixture con 1,024 bloques léxicos anidados. La compilación
de method bodies termina correctamente y publica 1,025 scopes: el root del método más los 1,024
scopes hijos. No se creó otro runner ni se amplió el alcance a una batería completa.

## Frontera residual: B08F activado

Esta subfase no elimina todavía el anidamiento del host causado por continuaciones de control.
`if`, `while`/`do`, `for`, `foreach`, `switch`, `try` y `using` aún ejecutan bodies de forma
síncrona mediante `analyzeBlock`. Aunque cada body recorre sus bloques léxicos con frames, una
cadena arbitrariamente profunda de constructs de control todavía puede crecer la pila C++.

El diseño formal exige una máquina iterativa de statement frames con merge explícito por scope.
Por ello B08 no se declara cerrado globalmente: B08F queda activado para convertir esas
continuaciones sin perder snapshots, loop contexts, cleanup regions, catch scopes, exits ni el
orden observable. La prueba de 1,024 bloques demuestra solamente la ruta léxica ya convertida.

## Invariantes

- mismo orden fuente;
- mismos scopes y ordinals;
- mismos snapshots y joins de ownership;
- mismas sondas de código inalcanzable;
- mismas continuaciones y profundidad de cleanup;
- mismos bindings, diagnostics, texto, severidad y rango;
- ninguna modificación de sintaxis, ABI, layout, ownership público, formatos o NSMETA.

## Validación

1. Extracción mecánica:

   `cmake --build A:\nsc-devel\build-test-infrastructure --target nsc_project_method_body_model_tests -j 16`

   Resultado previo al algoritmo: `PASS: 76 checks`.

2. Build y gate focal después de frames/worklist:

   `A:\nsc-devel\build-test-infrastructure\tests\nsc_project_method_body_model_tests.exe`

   Resultado: `PASS: 78 checks`.

3. Integración:

   `cmake --build A:\nsc-devel\build-clang --target nsc -j 16`

   Resultado: `nsc.exe` construido; contrato static host-runtime passed.

No se ejecutaron baseline, deploy, instalación, batería completa ni consumidores externos. CMake
mostró una ruta histórica de CTest en C:, pero no se ejecutó ni se escribió en ella. Fuentes,
temporales, builds y evidencia de esta tanda permanecieron en A:.

## Resultado

La familia de statements queda separada físicamente; bloques léxicos y patrones deconstruction ya
usan memoria explícita y cuentan con regresión profunda. La continuación B08F es obligatoria antes
de afirmar que el dispatcher completo de statements es iterativo.

## B08F — checkpoint de continuaciones estructurales

El worklist de `analyzeBlock` ya incorpora frames explícitos para:

- cadenas `if`/`else if` y sus joins de ownership;
- `while` y `do while`, incluidos estados de `break`, `continue` y back-edge;
- `for`, su scope propio, update, cleanup y back-edge;
- `foreach`, separado internamente en preparación y finalización para mantener activo el préstamo
  de iteración durante el candidato sintetizado;
- `using`, separado en preparación/finalización para conservar exactamente la vida del cleanup;
- `switch`, con preparación única de etiquetas y continuación explícita por sección.

Las regresiones profundas añadidas cubren 512 `else if`, 256 `if`, 256 `while`, 128 `for`, 192
`foreach`, 256 `using` y 256 `switch`. El gate focal alcanza `PASS: 85 checks`; `nsc.exe` vuelve a
construirse y conserva el contrato de host runtime estático.

La prueba exploratoria de 256 `foreach` no llegó a `method-bodies`: el último evento completado fue
`syntax-lex` y el proceso terminó con `0xC00000FD` durante `syntax-parse`. El caso semántico queda en
192 para probar la ruta que sí alcanza el binder, y el límite de parser se conserva como pendiente
explícito; no se atribuye falsamente al binder ni se considera resuelto.

El commit de checkpoint no cerró B08F porque `try`/`catch`/`finally` todavía requería su frame de
continuación.

## Cierre de B08F

`TryFrame` completa la máquina de estados con etapas explícitas para cuerpo `try`, cada cuerpo
`catch` y `finally`. El estado conservado incluye cleanup de `finally`, rango de estados de
`throw`, entrada y salida del `try`, entrada común de catches, scopes y filtros de cada catch,
profundidad/error type activo, joins y restauración final del ownership flow.

Se añadieron dos regresiones independientes: 256 `try/finally` anidados y 256 cuerpos `catch`
anidados. Ambas alcanzan `method-bodies` y pasan. El gate focal final de B08F es `PASS: 87 checks`;
`nsc.exe` se construyó otra vez y el contrato de host runtime estático pasó.

La reauditoría de llamadas confirmó que la ruta normal de `analyzeBlock` intercepta antes del
dispatcher todos los statements estructurales: bloques, `if`, bucles, `for`, `foreach`, `using`,
`switch` y `try`. La única llamada externa a `analyzeStatement` procede de bindings de arms de
`match`, cuyo contrato contiene exclusivamente declaraciones variables/deconstruction. Los cases
históricos del dispatcher permanecen como fallback defensivo, pero no forman una cadena recursiva
en el pipeline normal ni son usados por los fixtures profundos.

B08F queda cerrado. R04 permanece abierto porque es un desbordamiento independiente de
`syntax-parse`, demostrado antes de que exista el modelo de method bodies.
