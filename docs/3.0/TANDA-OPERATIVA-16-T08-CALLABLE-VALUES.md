# Tanda operativa 16 — T08 callable values y ABI observable de closures

Fecha: 2026-09-29  
Rama: `development`  
Alcance: T08-F02 y porción productiva T08-F05  
Estado del bloque T08: `IMPLEMENTING`

## Objetivo

Cerrar un vertical productivo y honesto sobre lo que el compilador ya puede ejecutar: lambda sin
captura como callback C, closure administrada capturante y método de instancia ligado. Además, hacer
observable el costo real de la closure en la frontera versionada MIR; antes se escondía bajo un
simple contador de capturas.

No se tomó como implementación el helper declarativo `CallableSemantics`: sus contratos de
`copy/move/ref`, `FnOnce` y `must_tail` siguen sin conexión con parser y backend.

## Diagnóstico previo

- Stage 43 ya construía callables sintéticos para lambdas y environments para las capturantes.
- Los valores capturados se alojaban en celdas compartidas y el environment guardaba punteros a
  esas celdas.
- Stage 55 ya bajaba una lambda `extern C` sin captura como puntero de función crudo y rechazaba una
  capturante, pues el ABI C no tiene canal para environment.
- Los method groups de instancia ya producían delegates con receptor.
- HIR/MIR conservaban nombres y tipos de captura, pero descartaban su procedencia.
- El dump MIR sólo emitía `captures=N`; no declaraba layout, asignación o storage.

## Cambios de implementación

### Procedencia estable

`methodLambdaCaptureSourceKindName` asigna nombres estables a las seis procedencias existentes:

- `original-local`;
- `original-parameter`;
- `original-this`;
- `parent-local`;
- `parent-parameter`;
- `parent-capture`.

El dumper de cuerpos publica esa procedencia junto con
`storage=shared-cell-reference`. Esto describe el backend actual; no promete captura por valor.

### HIR, MIR e identidad de caché

Cada función lambda conserva `captureSourceKinds` junto a nombres y tipos. MIR sube de 1.18 a 1.19
porque el contrato observable cambia. La identidad del caché serializa la nueva lista, evitando que
un objeto previo se considere equivalente si difiere la procedencia ABI de sus capturas.

El dump MIR ahora muestra:

- `closure-layout=none` y `closure-allocation=none` para lambdas sin captura;
- `closure-layout=cell-pointer-slots` y `closure-allocation=managed-heap` para capturantes;
- una línea por slot con nombre, tipo, fuente y `storage=shared-cell-reference`.

También se eliminó una copia triplicada y accidental de los campos de creator-capture al convertir
HIR a MIR. Era inocua, pero oscurecía esa frontera.

### Fixture productiva c66

`c66_ns3_callable_values` ejecuta en el mismo PE:

1. una lambda no capturante asignada a `extern C delegate`;
2. una closure administrada que captura un local entero;
3. un método de instancia ligado a su objeto.

La suma de los tres caminos retorna 42. La fixture se registró en el runner de correctitud para el
futuro baseline, pero en esta tanda sólo se ejecutó de forma individual.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
- `include/nativesharp/Frontend/ProjectOopHirModel.h`
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `src/Frontend/ProjectMethodBodyModelDumper.cpp`
- `src/Frontend/ProjectOopHirModel.cpp`
- `src/Frontend/ProjectOopMirModel.cpp`
- `src/Frontend/ProjectOopMirModelDumper.cpp`
- `src/LLVM/ProjectOopMirCacheIdentity.cpp`
- `tests/unit/Cc1CorrectnessTests.cpp`
- `docs/3.0/TANDA-08-FUNCIONES-LAMBDAS-Y-CLOSURES.md`

## Archivos nuevos y por qué

- `tests/cc1/c66_ns3_callable_values/c66_ns3_callable_values.nsproj`: proyecto edición 3 mínimo.
- `tests/cc1/c66_ns3_callable_values/Main.ns`: witness ejecutable de los tres ABI callable.
- `docs/3.0/TANDA-OPERATIVA-16-T08-CALLABLE-VALUES.md`: expediente de alcance, evidencia y deuda.

## Mejora concreta

Una herramienta, usuario o futura integración del debugger puede responder sin inferencias:

- si una lambda tiene environment;
- si ese environment asigna heap;
- cuántos slots contiene;
- qué símbolo originó cada celda;
- si el callable C permanece libre de estado;
- y si el artefacto cacheado corresponde al mismo contrato de captura.

## Validación dirigida ejecutada

1. `nsc` Debug/O0 construido en `A:\nsc-devel\build-clang`: correcto.
2. c66 O0 con lld-link: emisión correcta y proceso exit 42.
3. c66 O0 con `A:\nslink-devel\build-t56-sync\bin\nslink.exe`: emisión correcta y exit 42.
4. Dump MIR: schema 1.19; lambda C con layout/allocation `none`; closure con
   `cell-pointer-slots`, `managed-heap`, `original-local` y `shared-cell-reference`.
5. Dump method-body: captura local con fuente y storage explícitos.
6. Negativo dirigido: una lambda `extern C` capturante termina con exit 1 y el diagnóstico causal
   `extern C lambda must not capture state; a C function pointer has no environment channel`.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación, deploy ni operación remota. El
compilador utilizado fue exclusivamente `A:\nsc-devel\build-clang\bin\nsc.exe`.

## Deuda explícita

- identidad completa de tipos función: calling convention, efectos, safety, nullability y lifetime;
- sintaxis y lowering real de capturas `copy`, `move` y `ref`;
- clasificación productiva `Fn`, `FnMut` y `FnOnce`;
- escape analysis que permita referencias cuando la lifetime lo demuestre;
- lambdas genéricas y monomorfización;
- metadata cross-module de callables/closures;
- `must_tail` con verificación ABI/cleanup;
- visualización y evaluación de captures en debugger;
- alternativa stack/noheap cuando escape analysis pruebe que no escapa.

Por estas deudas, T08 permanece `IMPLEMENTING`; la tanda 16 sólo cierra el vertical explícito
T08-F02/F05 descrito arriba.
