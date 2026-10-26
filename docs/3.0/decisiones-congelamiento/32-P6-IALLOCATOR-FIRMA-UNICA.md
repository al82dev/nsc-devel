# Congelamiento 3.0 P6 — `IAllocator` con una sola firma y diagnóstico para la anterior

Fecha: 2026-10-26. Revisión de partida: `2a65e3c59`.

## Decisión aplicada

P6, opción 1, en palabras de Alberto: se conserva únicamente la firma nueva de `IAllocator`. Se mantienen los 43
cambios indispensables, se agrega el diagnóstico claro para implementaciones antiguas y se anota el reemplazo en
los tres documentos 3.0.4.

## Problema

C-08 congeló `IAllocator` en `Allocate(long, int)`, `Resize(byte*, long, long, int)` y
`Deallocate(byte*, long, int)`. Una clase con la firma de 3.0.4 (`Allocate(int)`, `Resize(byte*, int, int)`,
`Deallocate(byte*)`) recibía tres errores genéricos sobre la clase, sin decir que la firma cambió ni cuál es la
nueva (sonda `A:\temp\nsc-dec\d18\a1OldAllocator`):

```
concrete class '…OldArena' does not implement interface method 'Allocate(builtin:long,builtin:int)' from …IAllocator
```

## SCOPE LOCK

- **Cambia.** `ProjectInterfaceModel.cpp`: `reportReplacedAllocatorSignature`. Cuando falta la implementación de
  un miembro de `NativeSharp.Core.IAllocator` y la clase tiene un miembro del mismo nombre con la lista de
  parámetros reemplazada, el error sale en ese miembro: «'Allocate(builtin:int)' implements the IAllocator
  signature replaced in 3.0 (C-08); implement 'byte* Allocate(long bytes, int alignment)'». Sin miembro viejo,
  queda el mensaje genérico.
- **Documentos.** Nota de reemplazo al final de `NS304-T13-F02-allocator-selection.md`,
  `NS304-T13-F04-arena-pool.md` y `NS304-T17-F06-allocators-generators-containers.md`. El texto histórico no se
  toca.
- **Idéntico.** La firma de `IAllocator`; `ArenaAllocator` (conserva `Allocate(int)` como override de
  `Allocator` e implementa la firma nueva); la migración de los 43 sitios de
  `tests/compiler-suite/host/modules/LanguageSurfaceModule.cpp`, que se queda como cambio indispensable (DEC-16).
- **Prohibido.** Aceptar la firma vieja con un adaptador (opción 2 de P6, descartada).

## Validación

nsc Release recompilado. Sondas `d18`:

| Sonda | Resultado |
|---|---|
| a1OldAllocator (las tres firmas viejas) | tres errores, uno en cada miembro, cada uno con su firma nueva |
| a3OneOld (solo `Deallocate(byte*)` viejo) | un error en `Deallocate` que nombra su firma nueva |
| a2NewAllocator (firma nueva) | 3 |

Prueba nueva: `Freeze30ContractTests::testReplacedAllocatorSignature`.

Regresión focal con `nsc` Release y `nsc_component_tests` Debug y Release recompilados (exit 0):

- casos de asignadores `TL-LANG-477`, `-484`, `-485`, `-486`, `-487` y `-517`: 12 de 12 PASS (Debug y Release);
- refcases 96, copycases 30;
- corpus de `await` (run `p6` contra `d04`): sin diferencias de compilación ni de salida.

## No verificado

- `Freeze30ContractTests` no está configurado en los árboles locales; su caso nuevo es la sonda de arriba.
- Batería completa: queda para el cierre final.
