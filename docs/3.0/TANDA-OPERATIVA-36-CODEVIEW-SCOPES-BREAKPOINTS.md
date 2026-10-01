# Tanda operativa 36 — CodeView: scopes, shadowing y breakpoints

**Fecha:** 30 de septiembre de 2026

**Rama:** `development`

**Plataforma:** Windows x64

**Configuración validada:** Debug/O0

**Baseline:** no ejecutado; por política queda reservado para Alberto en T53

## Objetivo

Cerrar las limitaciones importadas `NSC2-DBG-001`, `NSC2-DBG-002` y
`NSC2-DBG-006` en el backend LLVM/CodeView de NSC 3.0:

- una local no debe ser visible antes de su declaración;
- una sentencia ejecutable aprobada debe conservar una dirección de línea utilizable como
  breakpoint/step;
- dos locales homónimas declaradas por ciclos consecutivos deben tener rangos independientes y no
  mostrar simultáneamente el valor muerto de la otra.

La comprobación inspecciona el PDB producido a O0 y ejecuta el PE. No considera suficiente encontrar
un record `S_LOCAL` aislado.

## Reproducción anterior al cambio

Se creó una función libre mínima con:

1. `total`, declarada al inicio del método;
2. `late`, declarada dentro de un bloque después de una asignación ejecutable;
3. dos ciclos `for` consecutivos que declaran sendas variables `i`;
4. `Next`, declarada al final, seguida por una asignación y el retorno.

Antes del arreglo, CodeView publicaba `total`, ambos `i` y `Next` con el mismo rango exterior
`[0001:0004,+143)`. En consecuencia, `Next` era visible antes de existir y los dos `i` se
solapaban durante prácticamente toda la función. El record de la local existía, pero su alcance no
representaba el programa fuente.

La línea de asignación posterior a `Next` ya producía una entrada de línea válida. Este caso positivo
se conserva como contrato para evitar que una corrección de scopes degrade breakpoints.

## Diseño aplicado

### Scope léxico por declaración

Cada bloque de fuente conserva su `DILexicalBlock` de llaves. Antes de bajar una declaración local
directa se crea un scope hijo y se mantiene activo hasta el final de ese bloque. Las declaraciones
sucesivas forman así una cadena:

- una local anterior continúa visible;
- una local posterior comienza en su propia declaración;
- todas terminan al cerrar las llaves que las contienen.

La construcción del scope se centralizó para que bloque y `for` compartan las mismas validaciones de
información de depuración, archivo y posición fuente.

### Scope propio de `for`

El inicializador, condición, cuerpo e incremento de cada `for` se bajan dentro de un scope del ciclo.
Dos `for (int i = ...)` consecutivos producen por ello dos parents CodeView distintos y rangos no
solapados.

### Visibilidad después de inicializar

`dbg.declare` se emite después de completar el inicializador o la inicialización por defecto. Así la
local no se anuncia al depurador durante la evaluación de su propio initializer. Este orden también
se aplica a agregados inicializados directamente en su slot.

No se introdujeron intrinsics artificiales `lifetime.start/end`: durante la investigación se comprobó
que no resolvían los rangos CodeView de este caso. La solución final modela la semántica mediante
scopes fuente reales.

## Resultado observable

`llvm-pdbutil` presenta los offsets en decimal. Los rangos finales son:

| Local | Rango CodeView | Significado comprobado |
|---|---|---|
| `total` | `[0001:0004,+143)` | vive durante la función después de su declaración |
| `late` | `[0001:0023,+20)` | empieza en su store y termina al salir del bloque interior |
| primer `i` | `[0001:0043,+38)` | cubre exclusivamente el primer `for` |
| segundo `i` | `[0001:0083,+37)` | cubre exclusivamente el segundo `for` |
| `Next` | `[0001:0122,+25)` | empieza al final y no aparece en instrucciones anteriores |

La desensamblación confirmó los límites y el PE termina con código `42`. Las líneas ejecutables 7,
8, 9, 11, 13, 15, 17, 19, 20 y 21 tienen entradas de línea en el PDB; en particular, la asignación de
la línea 20 sigue siendo un sitio de breakpoint.

## Archivos modificados

- `src/LLVM/ProjectOopLlvmLowerer.cpp`
  - factoriza la creación de scopes léxicos de depuración;
  - delimita declaraciones dentro de bloques;
  - delimita el initializer y el cuerpo de cada `for`;
  - publica la ubicación de la local después de inicializarla.
- `CMakeLists.txt`
  - registra la verificación dirigida `nsc.ns3.t36-debug-scopes` cuando las pruebas están habilitadas.

## Archivos nuevos y razón

- `tests/integration/debug_scopes_t36/DebugScopesT36.nsproj`
- `tests/integration/debug_scopes_t36/Main.ns`
  - fixture mínimo y autocontenido que reproduce las tres limitaciones sin depender del runtime.
- `cmake/VerifyNativeSharpDebugScopesT36.cmake`
  - construye sólo el fixture, lo enlaza directamente, ejecuta el PE e inspecciona símbolos, rangos y
    líneas del PDB.
- `docs/3.0/TANDA-OPERATIVA-36-CODEVIEW-SCOPES-BREAKPOINTS.md`
  - evidencia de diseño, reproducción, archivos, mejora y validación de este commit.

## Validación dirigida

- `cmake --build A:\nsc-devel\build-clang --target nsc -j 12`
  - correcto; sólo se construyó `nsc` en la configuración Debug/O0 existente.
- `cmake -P cmake/VerifyNativeSharpDebugScopesT36.cmake` con el `nsc.exe` recién generado,
  `lld-link.exe` y `llvm-pdbutil.exe`
  - correcto: `NativeSharp T36 O0 lexical scopes, shadowing and breakpoint sites verified`.

No se habilitó ni ejecutó CTest completo, no se corrió el baseline y no se modificó ni instaló nada en
`C:\native-sharp`.

## Mejora obtenida

El PDB ya no confunde existencia de storage con visibilidad fuente. Esto elimina valores fantasma al
entrar en bloques, evita que el depurador elija un `i` muerto entre ciclos consecutivos y preserva los
sitios ejecutables usados por breakpoints y stepping. La tanda deja una prueba exacta y económica que
puede ejecutarse sin gastar el costo del baseline.
