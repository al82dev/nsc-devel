# Tanda operativa 37 — CodeView: valores, `out`, handlers y globals

**Fecha:** 1 de octubre de 2026

**Rama:** `development`

**Plataforma:** Windows x64

**Configuración validada:** Debug/O0

**Baseline:** no ejecutado; Alberto lo ejecutará en T53

## Objetivo

Reproducir en el compilador generado de NSC 3.0 las limitaciones importadas
`NSC2-DBG-003`, `NSC2-DBG-004`, `NSC2-DBG-005`, `NSC2-DBG-007` y
`NSC2-DBG-008`, y corregir únicamente las que siguieran presentes:

- conservar argumentos ordinarios observables a O0, incluido un literal y un valor del caller;
- mostrar un parámetro `out` como el valor fuente referenciado y no como un pointer crudo;
- publicar estado de namespace `_nsc_state_` con nombre fuente, tipo y ancho;
- clasificar el binding tipado de `catch` como argumento del handler con lifetime del handler;
- publicar campos estáticos en el catálogo global del PDB.

## Reproducción anterior al cambio

El fixture T37 compila una llamada `Capture(26, callerValue, out outValue)`, donde `callerValue`
vale `2`; captura `SpecificError specific`; lee el estado de namespace `GlobalCount`; y consume los
campos estáticos `Widget.Created` y `Widget.Destroyed`.

La inspección del objeto O0 anterior mostró:

| Caso | Resultado anterior |
|---|---|
| `literal` | `S_LOCAL`, `int`, `flags = param`, home de frame completo |
| `fromCaller` | `S_LOCAL`, `int`, `flags = param`, home de frame completo |
| `produced` (`out int`) | `S_LOCAL`, **`int*`**, `flags = param` |
| `specific` | `S_LOCAL`, tipo correcto, pero **sin** `flags = param`; rango demasiado amplio |
| `GlobalCount` | no existía `S_GDATA32` ni otra entrada tipada de global |
| `Widget.Created` / `Destroyed` | no existían en el catálogo global |

Por tanto, `DBG-003` ya funcionaba en NSC 3.0 y debía congelarse como caso positivo; las otras
cuatro limitaciones sí se reprodujeron.

## Diseño e implementación

### Modos de paso conservados hasta MIR

HIR y MIR ahora transportan `ParameterPassKind` por parámetro. Antes el frontend retenía que el
usuario escribió `ref`, `out` o `in`, pero el backend de depuración recibía solamente el tipo físico
pointer. La nueva propiedad se propaga para métodos locales, funciones libres, imports nativos,
imports C y lambdas; también forma parte de la identidad de caché y de los dumps HIR/MIR.

El ABI físico no cambia: `out int` continúa pasando una dirección Win64. Sólo cambia su tipo de
depuración, que ahora es un `DW_TAG_reference_type`. CodeView lo publica como `int&`, de modo que
el inspector presenta el entero apuntado y mantiene la dirección como detalle técnico.

### Parámetros tipados de `catch`

Cada cláusula tipada crea un scope CodeView propio. El objeto atrapado se guarda primero en su home
y luego se emite como `DILocalVariable` de categoría parameter dentro de ese scope. En el PDB final:

- `specific` tiene `flags = param`;
- conserva `DebugValuesT37.SpecificError*`;
- vive únicamente en el `S_BLOCK32` del handler;
- su rango es `[0001:0160,+16)` en este fixture, igual al bloque físico del handler.

No se inventó una función nativa separada: el handler sigue siendo el bloque SEH real dentro del
método, pero el binding fuente queda clasificado correctamente como entrada del handler.

### Globals tipados

Los campos estáticos MIR conservan ahora rango fuente y nombre calificado de depuración. El lowerer
crea una `DIGlobalVariableExpression`, la asocia al storage LLVM mediante `!dbg` y usa:

- nombre fuente calificado;
- linkage físico existente;
- `DIType` fuente;
- archivo y línea de la declaración.

Esto se aplica tanto a campos de clase como a estados de namespace definidos por el productor. No
se duplica metadata en declarations importadas.

El PDB final contiene:

| Entrada | Record | Tipo |
|---|---|---|
| `DebugValuesT37.GlobalCount` | `S_GDATA32` | `int` |
| `DebugValuesT37.Widget.Created` | `S_GDATA32` | `int` |
| `DebugValuesT37.Widget.Destroyed` | `S_GDATA32` | `int` |

## Valores del caller y ejecución

La desensamblación O0 conserva explícitamente:

- `callerValue = 2` en su home de stack;
- el literal `26` como inmediato `0x1a` en el registro del primer argumento;
- la dirección de `outValue` en el registro del tercer argumento;
- homes CodeView completos para `literal`, `fromCaller` y `produced` en el callee.

El PE se enlaza directamente contra
`A:\nsc-devel\build-clang\generated\NativeSharp.Runtime.Win32.lib`, se ejecuta y termina con código
`42`. La prueba no usa ni instala el compilador o runtime de `C:\native-sharp`.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectOopHirModel.h`
  - añade el modo de paso por parámetro al contrato HIR.
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
  - conserva modo de paso, rango fuente y nombre global de depuración.
- `src/Frontend/ProjectOopHirModel.cpp`
  - propaga `value/ref/out/in` por todas las clases de callable.
- `src/Frontend/ProjectOopMirModel.cpp`
  - transfiere el modo a MIR y materializa la identidad fuente de globals.
- `src/Frontend/ProjectOopHirModelDumper.cpp`
- `src/Frontend/ProjectOopMirModelDumper.cpp`
  - hacen observable el contrato nuevo.
- `src/LLVM/ProjectOopMirCacheIdentity.cpp`
  - incluye modos de paso e identidad de globals; versión de clave `v3`.
- `src/LLVM/LlvmCApi.h`
  - expone las APIs LLVM C necesarias para referencias y globals de debug.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
  - emite referencias CodeView, argumentos de handler y globals tipados.
- `CMakeLists.txt`
  - registra únicamente el verificador dirigido T37 cuando las pruebas están habilitadas.

## Archivos nuevos y razón

- `tests/integration/debug_values_t37/DebugValuesT37.nsproj`
- `tests/integration/debug_values_t37/Main.ns`
  - reproducción autocontenida de los cinco IDs y test ejecutable con resultado 42.
- `cmake/VerifyNativeSharpDebugValuesT37.cmake`
  - compila un objeto, inspecciona CodeView, confirma los inmediatos O0, enlaza con el runtime
    generado en A:, ejecuta el PE e inspecciona símbolos y catálogo global del PDB.
- `docs/3.0/TANDA-OPERATIVA-37-CODEVIEW-VALORES-GLOBALS.md`
  - este registro de reproducción, diseño, archivos, mejora y evidencia.

## Validación dirigida

- Construcción: `cmake --build A:\nsc-devel\build-clang --target nsc -j 12` — correcta.
- Verificador: `VerifyNativeSharpDebugValuesT37.cmake` — correcto:
  `NativeSharp T37 O0 arguments, out values, catch parameters and globals verified`.
- Ejecución física del fixture: exit `42`.
- PDB: parámetros ordinarios con homes; `out int` como `int&`; `specific` como parameter de bloque;
  tres `S_GDATA32` calificados y tipados.

No se habilitó CTest completo, no se ejecutó baseline, no se probaron O1/O2/O3, no se usaron
remotas y no se modificó ni instaló nada en C:.

## Mejora obtenida

El depurador ya recibe semántica fuente en los puntos donde el ABI físico por sí solo era ambiguo.
Un `out int` deja de parecer una dirección sin significado, un binding de `catch` deja de mezclarse
con las locales ordinarias y el catálogo de globals puede resolver estados y campos estáticos por
nombre calificado con su tipo y ancho. Los argumentos ordinarios O0, que ya eran correctos en 3.0,
quedan protegidos contra regresiones junto con sus valores físicos de caller.
