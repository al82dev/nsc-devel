# Tanda operativa 15 — T07 panic/never y frontera EH

Fecha: 2026-09-29  
Rama: `development`  
Plataforma: Windows x64  
Configuración: Debug/O0  
Baseline: no ejecutado

## Objetivo honesto

El título maestro de la tanda enumera el bloque T07 completo: `Result`, `?`, typed `throws`, EH,
`never`, loops con valor y perfiles de panic. La auditoría confirmó que la mayor parte seguía en
`ErrorSemantics.cpp` como contratos C++ sin sintaxis ni lowering productivo. Esta tanda no cambia
ese helper por una etiqueta de completitud falsa: integra una vertical pequeña pero completa desde
fuente NativeSharp hasta PE x64 y deja cada deuda restante explícita.

La vertical elegida es el perfil abort de `panic()` y su valor divergente `never`, reutilizando el
trap runtime que ya protege fallos del compilador. Se comprueba además que convive con la EH tipada
Win64 existente mediante un witness autocontenido, sin importar todo NativeSharp.Core.

## Semántica entregada

### Sintaxis y edición

- `panic()` es contextual: `panic` sigue siendo un identificador salvo como cabeza de llamada no
  calificada.
- La forma sólo está disponible con `<Language>3</Language>`; edición 2 emite NS3002.
- El perfil abort inicial acepta exactamente cero argumentos y cero argumentos genéricos.
- Un miembro calificado llamado `panic` no se confunde con el intrínseco.

### `never` productivo

- El binding de `panic()` tiene tipo interno `builtin:never`.
- Una expression statement de ese tipo termina el estado de flujo y el código posterior se analiza
  sólo como sonda inalcanzable, sin contaminar joins de ownership.
- HIR conserva un nodo `panic type=builtin:never` sin operandos.
- MIR schema 1.18 conserva el mismo nodo; no se materializa un valor falso.

Este alcance todavía no ofrece `never` como tipo escribible ni su coerción general en ternarios,
match, retornos, overloads o genéricos. La implementación no afirma lo contrario.

### Perfil abort Win64

- LLVM emite `__nativesharp_trap(14)` seguido de `unreachable`.
- El runtime convierte la razón estable en exit code `128 + 14 = 142`.
- La llamada es `nounwind`, `noreturn`, no recibe payload y no asigna memoria.
- El perfil abort no ejecuta cleanups léxicos. `NS3-T07-F06` añadió después el perfil
  `unwind-native`, payload y handler custom; véase `items/NS3-T07-F06-assertions-panic.md`.
- lld-link y NSLink generan ejecutables con la misma conducta observable.

### EH tipada conservada

c65 declara su propio `NativeSharp.Core.Exception`, deriva `Problem`, lanza una instancia y la
captura por tipo. Eso fuerza personality/funclets, selección de catch y retorno desde catch en la
misma fixture donde existe `panic`, sin cargar las 41 fuentes de Core.

Se intentó primero la fixture histórica c8. La compilación se detuvo antes de llegar a c8 por una
deuda previa e independiente en `lib/NativeSharp.Core/ArenaAllocator.ns`: un `Span<byte>` intenta
escapar desde el local `memoria`. No se relajó el borrow checker ni se atribuyó ese fallo a T07;
el witness EH autocontenido evita ocultar el problema y mantiene la prueba estrictamente dirigida.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
  - añade el core operation estable `Panic` al final del enum.
- `src/Parser/Parser.cpp`
  - marca la llamada contextual y aplica el gate de edición 3.
- `src/Frontend/ProjectMethodBodyModel.cpp`
  - valida aridad, produce `builtin:never` y termina el flujo alcanzable.
- `include/nativesharp/Frontend/ProjectOopHirModel.h`
  - añade el nodo HIR divergente al final del enum.
- `src/Frontend/ProjectOopHirModel.cpp`
  - baja el binding sin convertirlo en una llamada de usuario.
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
  - añade el nodo MIR y sube schema 1.17 → 1.18.
- `src/Frontend/ProjectOopMirModel.cpp`
  - conserva la identidad panic al cruzar HIR → MIR y en dumps.
- `include/nativesharp/LLVM/LlvmTrapReason.h`
  - reserva la razón 14 y documenta el exit code 142.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
  - emite el trap noreturn y el terminador unreachable.
- `tests/unit/Cc1CorrectnessTests.cpp`
  - registra c65 en el runner futuro sin ejecutar la suite completa en esta tanda.
- `docs/3.0/TANDA-07-ERRORES-Y-CONTROL-DE-FLUJO.md`
  - reemplaza el límite histórico por el estado físico real y la deuda restante.

## Archivos nuevos y por qué

- `tests/cc1/c65_ns3_panic_never/c65_ns3_panic_never.nsproj`
  - proyecto mínimo edición 3 que no depende de bibliotecas externas.
- `tests/cc1/c65_ns3_panic_never/Main.ns`
  - witness normal de panic no tomado, dead-code binding, catch tipado y salida 42.
- `docs/3.0/TANDA-OPERATIVA-15-T07-PANIC-NEVER-EH.md`
  - expediente reproducible del alcance, decisiones, evidencia y deuda de esta tanda.

## Mejora concreta

Antes, T07 sólo podía responder preguntas mediante helpers C++; ningún programa NativeSharp podía
escribir panic ni observar never en HIR/MIR/PE. Ahora existe una ruta única y auditable que:

1. rechaza edición/aridad incorrectas temprano;
2. representa divergencia sin fabricar un valor;
3. corta el bloque LLVM con un terminador real;
4. tiene un código de proceso estable para automatización/debugger;
5. produce la misma conducta con ambos linkers Windows x64;
6. convive con los funclets de excepción tipada existentes.

## Validación dirigida ejecutada

1. `nsc` Debug/O0 construido en `A:\nsc-devel\build-clang`: correcto.
2. c65 O0 con lld-link: emisión correcta y proceso exit 42.
3. c65 O0 con `A:\nslink-devel\build-t56-sync\bin\nslink.exe`: emisión correcta y exit 42.
4. Dump MIR dirigido: `schema=1.18` y `panic type=builtin:never`.
5. Probe temporal que ejecuta `panic()`:
   - lld-link: exit 142;
   - NSLink: exit 142.
6. Negativo edición 2: exit 1 con NS3002.
7. Negativo `panic(1)`: exit 1 con diagnóstico de aridad del perfil abort.
8. c65 incluye throw/catch tipado autocontenido y retorna 42 con ambos linkers.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación, deploy ni operación remota. Todos los
artefactos y ejecutables usados estuvieron en A: y los temporales se eliminaron antes del commit.

## Deuda explícita

- ~~`Result<T,E>` productivo y operador postfix `?` con early return, conversión y cleanup.~~
  Cerrado por `NS3-T07-F01`; véase `items/NS3-T07-F01-result-propagation.md`.
- ~~`throws E` en firmas, effect checking de callers y metadata cross-module.~~ Cerrado por
  `NS3-T07-F02`; los conjuntos de varios errores y exhaustividad avanzada siguen en F03.
- `never` escribible y coerción bottom en expresiones/overloads/generics/match.
- `loop { break value; }`, labels y unificación de tipos/lifetimes.
- ~~panic `unwind-native`, handler custom, payload acotado y source location.~~ Cerrado por
  `NS3-T07-F06`; el debugger break pertenece al contrato posterior de NSDBG.
- Matrices exhaustivas de cleanup en propagation/throw/panic y fronteras DLL/FFI.

Por estas deudas, la tanda operativa 15 se cierra como vertical T07-F03/F04/F06, mientras el bloque
maestro T07 permanece `IMPLEMENTING`.
