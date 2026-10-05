# NS3 baseline: overloads, flujo no retornable y enums físicos

## Problema

El analizador self-hosted tipaba una llamada usando el primer miembro con el nombre solicitado,
mientras el backend seleccionaba nuevamente el mejor overload por los tipos de los argumentos.
En `Interlocked.Load(ref long, MemoryOrder) > 0`, semántica elegía el retorno `int` del primer
`Load`, pero el backend emitía correctamente una carga `i64` del overload `long`. El literal seguía
siendo `i32` y LLVM rechazaba `icmp i64, i32`.

La reproducción exacta permitió descubrir antes de otra corrida dos defectos posteriores de la
misma ruta self-hosted:

- los bucles constantes `while (true)`, `do ... while (true)` y `for (;;)` siempre se consideraban
  capaces de caer al final, incluso sin un `break` dirigido al bucle actual;
- los enums ajenos al conjunto hardcodeado del propio compilador se representaban como punteros.
  `ReferenceRelease` terminaba produciendo constantes incompatibles con el retorno físico.

## Cambio

- La selección de overload ocurre ahora en análisis semántico después de analizar los argumentos.
- El hecho del callee y el hecho de la llamada publican el símbolo y retorno seleccionados.
- Las conversiones contextuales se calculan contra esa firma exacta.
- El backend valida que ambos operandos tengan el mismo tipo LLVM antes de emitir operaciones
  binarias, comparaciones o asignaciones compuestas; una divergencia futura falla en el punto de
  lowering en lugar de producir un módulo inválido.
- El lowering de bucles registra `break` en la pila del bucle correspondiente. Un bucle constante
  sin salida se publica como terminador y sella su bloque posterior con `unreachable`; un `break`
  de un bucle anidado no contamina al exterior.
- Los enums declarados se resuelven por el modelo semántico y usan su ancho físico real en firmas,
  parámetros, locales, constantes, comparaciones, conversiones y retornos. La lista histórica de
  nombres deja de decidir el ABI de un enum de biblioteca.
- La matriz dirigida cubre retornos `int`, `long`, `nint`, `nuint`, enums arbitrarios y las tres
  formas de bucle no retornable, además de casos con `break` y anidamiento.

## Archivos

- `selfhost/nsc/Sema/SemanticAnalyzer.ns`
- `selfhost/nsc/CodeGen/LlvmBackend.ns`
- `lib/NativeSharp.LLVM.C/LLVMApi.ns`
- `selfhost/tests/frontend/overload-width-comparisons.ns`
- `selfhost/tests/frontend/nonfallthrough-loops.ns`
- `selfhost/contracts/semantic.contract.json`

## Mejora

Semántica y codegen dejan de resolver overloads incompatibles. La corrección cubre el fallo de
`ReferenceCounts.IsValid` y también llamadas sobrecargadas usadas en aritmética, retornos,
asignaciones y condiciones con anchos enteros distintos.

`TryRetainStrong`, `TryRetainWeak`, `ReleaseStrong` y `ReleaseWeak` conservan su implementación
lock-free natural: no requieren retornos ficticios ni casts para satisfacer al backend.

## Validación

Sólo se ejecutaron comprobaciones puntuales:

- sintaxis del proyecto self-hosted: 98 unidades;
- invariantes semánticos de overload: 12/12;
- invariantes semánticos enum: 2/2;
- fixtures de overloads y flujo: O0, O1, O2 y O3;
- `RuntimeLibraries.nsproj`, el proyecto exacto de SH48: O0, O1, O2 y O3.

No se ejecutó CTest, minibaseline ni baseline. La certificación completa corresponde a la corrida
externa de Alberto.
