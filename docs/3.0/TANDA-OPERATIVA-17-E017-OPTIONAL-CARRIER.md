# Tanda operativa 17 — Carrier público `Optional<T>` para coalescencia

Fecha: 2026-09-29

Rama: `development`

Alcance: fundación Runtime previa a E017–E020 y cierre de `Optional<T> ?? fallback`

Estado de E017–E020: `IMPLEMENTING`

## Objetivo

Conectar el `Optional<T>` real de `NativeSharp.Core` con el operador `??` sin convertir su
representación privada en ABI del lenguaje. El compilador debe consumir exclusivamente el
protocolo público `HasValue()`/`UncheckedValue()`, evaluar el carrier una sola vez y conservar la
evaluación perezosa del fallback incluso en O0.

Esta tanda no declara terminados `Result<T,E>`, tuple utilities, callables owning ni allocators.
Entrega la primera unión productiva entre la ergonomía T01 y las fundaciones Runtime E017–E020.

## Diagnóstico previo

- `NativeSharp.Core.Optional<T>` ya existía como struct genérico con almacenamiento privado.
- El frontend mencionaba un carrier Optional soportado en su diagnóstico, pero `??` sólo aceptaba
  punteros, referencias, delegates, arreglos nativos y `string`.
- No existía binding semántico que fijara los miembros públicos usados por la operación.
- HIR/MIR sólo tenían `null-coalescing`, cuya condición física es comparar un puntero con null.
- Bajar Optional mediante offsets de `value`/`present` habría acoplado el compilador a una
  representación privada y roto evolución de biblioteca, tipos payload grandes y DLLs.

## Contrato implementado

Un carrier sólo se reconoce cuando se cumplen todas estas condiciones:

1. es una especialización struct de la plantilla exacta, no anidada,
   `NativeSharp.Core.Optional<T>`;
2. declara un método de instancia público, único y sin parámetros `HasValue() -> bool`;
3. declara un método de instancia público, único y sin parámetros `UncheckedValue() -> T`;
4. `T` se normaliza a su type key canónico antes de comparar la firma materializada.

Un struct con el mismo nombre en otro namespace no activa el protocolo. Los campos privados no se
consultan ni en semántica, HIR, MIR o LLVM.

## Cambios de implementación

### Binding y demanda genérica

`MethodOptionalCarrierBinding` conserva carrier, payload y los ordinales exactos de ambos métodos.
El resolvedor se ejecuta durante el análisis de `??`; el RHS se convierte a `T`, no al tipo
`Optional<T>`. Los dos callables de una especialización genérica se incorporan al worklist para
que no desaparezcan por materialización diferida.

El dump de method bodies publica cada binding como `access=public-protocol`, con sus ordinales. Es
evidencia de que el compilador no deduce offsets privados.

### HIR, MIR y caché

HIR distingue `optional-coalescing` de `null-coalescing` y conserva los dos miembros del protocolo.
MIR sube de schema 1.19 a 1.20 y transporta ambos nombres físicos:

- `callee`: `HasValue`;
- `secondary-callee`: `UncheckedValue`.

La segunda identidad entra en el hash de caché MIR; un objeto cacheado con otro destino de payload
ya no puede considerarse equivalente.

### LLVM y orden de evaluación

El carrier se evalúa una vez y se deposita en un slot de entrada. `HasValue` recibe la dirección
del mismo struct. Sólo el bloque presente llama `UncheckedValue`; sólo el bloque ausente evalúa el
fallback. Ambos caminos convergen mediante un phi del payload.

Los retornos payload que usan sret Win64 se atienden después del receptor, igual que una llamada
normal de método de instancia. No se introduce heap oculto ni se conoce el layout de Optional.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
- `include/nativesharp/Frontend/ProjectOopHirModel.h`
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `src/Frontend/ProjectMethodBodyModelDumper.cpp`
- `src/Frontend/ProjectOopHirModel.cpp`
- `src/Frontend/ProjectOopMirModel.cpp`
- `src/Frontend/ProjectOopMirModelDumper.cpp`
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
- `src/LLVM/ProjectOopMirCacheIdentity.cpp`
- `tests/unit/Cc1CorrectnessTests.cpp`
- `docs/3.0/TANDA-01-EXPRESIONES-Y-ERGONOMIA.md`
- `docs/3.0/TANDA-OPERATIVA-06-T01-NULL-RANGE-INTERPOLATION-WITH.md`

## Archivos nuevos y por qué

- `tests/cc1/c67_ns3_optional_coalescing/c67_ns3_optional_coalescing.nsproj`: proyecto edición 3
  mínimo que queda registrado para el baseline futuro.
- `tests/cc1/c67_ns3_optional_coalescing/Main.ns`: witness que comprueba resultado, evaluación única
  del carrier, fallback perezoso y payload Win64 directo (`int`) e indirecto (`LargeValue`, 24 B).
- `docs/3.0/TANDA-OPERATIVA-17-E017-OPTIONAL-CARRIER.md`: expediente de alcance, cambios,
  validación y deuda de esta tanda.

## Mejora concreta

`Optional<int> ?? int` ya cruza parser, binding, HIR, MIR, LLVM, PE y ambos enlazadores. El usuario
puede expresar ausencia en un value type sin punteros y sin que el compilador dependa de campos u
offsets internos de Core. La ABI observable identifica explícitamente los dos métodos usados.

## Validación dirigida ejecutada

1. Target `nsc` Debug/O0 construido en `A:\nsc-devel\build-clang`: correcto.
2. Dump de method bodies c67: cuatro bindings públicos para payload directo e indirecto.
3. Dump MIR c67: schema 1.20, cuatro nodos `optional-coalescing`, `callee`,
   `secondary-callee` y `LargeValue abi=win64-indirect` presentes.
4. c67 O0 con lld-link: emisión correcta y proceso exit 42.
5. c67 O0 con `A:\nslink-devel\build-t56-sync\bin\nslink.exe`: emisión correcta y exit 42.
6. Negativa temporal: `Other.Optional<T>` con los mismos métodos termina con exit 1 y el
   diagnóstico de carrier no soportado.
7. `git diff --check`: correcto; sólo avisos de normalización LF/CRLF ya gobernada por Git.

Los ejecutables, el proyecto negativo y los `error*.log` generados fueron eliminados. No se
ejecutó baseline, CTest general, O1/O2/O3, instalación, deploy ni operación remota. El compilador
utilizado fue exclusivamente `A:\nsc-devel\build-clang\bin\nsc.exe`.

## Deuda explícita

- value `?.` debe producir `Optional<T>` y soportar properties, métodos y cadenas condicionales;
- `Result<T,E>`/`?` espera storage y drop genéricos sound; E017 no se marca terminado;
- E018 conserva tuple utilities, `Exchange`, enum utilities y scope guards pendientes;
- E019 conserva `FunctionRef`, `InlineFunction` y `OwnedFunction` move-only pendientes;
- E020 conserva allocator handles, factories y `OutPtr`/`InOutPtr` pendientes;
- payloads owning/move-only requieren integrar por completo drop flags y transferencias;
- NSMETA cross-module y tooling de Optional quedan asignados a las tandas 18, 29 y 30.

Por estas deudas, E017–E020 permanecen `IMPLEMENTING`. La tanda operativa 17 queda cerrada porque
su vertical declarado —`Optional<T> ?? fallback` mediante protocolo público— llega hasta PE, falla
cerrado ante impostores y tiene evidencia con ambos linkers.
