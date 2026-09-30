# Tanda operativa 22 — T10/E026: contrato de concurrencia

Fecha: 2026-09-29

Rama: `development`

Alcance: arquitectura verificable T10 y prerequisito de E026

Estado de T10/E026: `IMPLEMENTING`

## Objetivo

Convertir las reglas aisladas de coroutines/concurrencia en un contrato versionado generado por el
compilador. El artefacto debe fijar costos y prohibiciones antes de introducir syntax/lowering
`async/await`, parallel algorithms o integración `Send/Sync`.

Los comandos nuevos son:

```text
nsc --print-concurrency-contract
nsc --write-concurrency-contract <file.contract>
nsc --verify-concurrency-contract <file.contract>
```

Cada build de `nsc` genera además:

```text
build-clang/generated/NativeSharpConcurrency.contract
```

## Diagnóstico previo

- `CoroutineSemantics` modelaba futures, cancelación, Send/Sync y órdenes atómicos, pero sólo era
  ejercitado por un test C++ aislado.
- El parser no contiene `async`, `await` ni `yield`; no existe state-machine lowering productivo.
- El runtime 2.0 posee atomics/threads/JobSystem históricos, pero este trabajo está limitado al
  compilador y no recompila ni modifica bibliotecas.
- Sin un contrato único, frontend, Runtime, JobSystem y future tooling podían escoger scheduler,
  heap, cancelación u orden de cleanup incompatibles.

## Contrato `NSCONCURRENCY 1.0`

- plataforma: Windows x64;
- scheduler: sólo `JobSystem` explícito proporcionado por el caller;
- parallel policy: caller entrega JobSystem, threshold y cancelación;
- futures lazy;
- frame storage: caller stack, caller arena o heap explícito;
- cero heap implícito;
- poll single-consumer y sin resume después de estado terminal;
- cancelación cooperativa, nunca thread abort;
- cleanup: defers activos en reversa y después locals activos en reversa;
- task scope: un borrow exige join antes de abandonar el scope;
- `Send/Sync` estructural y fail-closed; raw pointer no deriva traits;
- órdenes atómicos fijados por operación;
- `volatile` no proporciona sincronización;
- async generator exige backpressure explícito.

El manifiesto es textual, determinista y tiene fingerprint estable. Verificar compara el contrato
completo, no sólo nombre/versión.

## Archivos modificados

- `include/nativesharp/Language/CoroutineSemantics.h`: versión y API de manifest/write/verify.
- `src/Language/CoroutineSemantics.cpp`: manifiesto, fingerprint y codec de archivo.
- `src/Driver/Driver.cpp`: integra los tres comandos CLI y `--help`.
- `CMakeLists.txt`: genera el contrato después de enlazar `nsc`, lo declara byproduct y lo incorpora
  al payload de instalación futuro.
- `tests/unit/NativeSharp3T10CoroutineTests.cpp`: amplía T10 de 10 a 13 comprobaciones.
- `docs/3.0/TANDA-10-COROUTINES-Y-CONCURRENCIA.md`: registra la integración y conserva la deuda.

## Archivos nuevos y por qué

- `docs/3.0/TANDA-OPERATIVA-22-T10-CONCURRENCY-CONTRACT.md`: expediente de alcance, evidencia,
  archivos y deuda.

No se crea una segunda implementación de JobSystem, atomics o futures. El contrato pertenece al
compilador y será el punto de acuerdo de esas implementaciones cuando se conecten.

## Validación dirigida ejecutada

1. `nsc` Debug/O0 construido correctamente.
2. El post-build generó `NativeSharpConcurrency.contract` mediante el `nsc` recién enlazado.
3. Target único `nsc_ns3_t10_coroutine_tests`: `PASS: 13 checks`.
4. `--print-concurrency-contract` publicó el manifiesto 1.0 completo.
5. `--verify-concurrency-contract` aceptó el artefacto generado.
6. El test cubrió future/cancel/cleanup/backpressure/Send/Sync/memory order y round-trip.
7. `NSC_BUILD_TESTS` quedó restaurado a `OFF` y `nsc` se reconstruyó en ese estado.

No se ejecutó baseline, CTest general, O1/O2/O3, JobSystem, bibliotecas, instalación, deploy ni
remotas.

## Mejora concreta

Un build NativeSharp 3.0 transporta una política de concurrencia verificable. Ningún componente
puede introducir silenciosamente un scheduler global, heap oculto, thread abort, copy de borrows,
`volatile` como fence o memory orders distintos sin cambiar un contrato versionado observable.

## Deuda explícita

- syntax/effects `async`, `await`, `yield` y tipos future productivos;
- state-machine HIR/MIR/LLVM, layout de frame, pinning y cleanup normal/excepcional/cancelado;
- borrow checking a través de await y task scopes integrado al frontend;
- traits `Send/Sync`, impls negativas y metadata cross-module;
- E026 parallel algorithms con JobSystem/threshold/cancelación explícitos;
- async generators, backpressure real y logical debug stacks;
- atomics/JobSystem requieren recertificación 3.0 dirigida cuando sus consumidores estén listos;
- gates cross-DLL, ambos linkers, stress, races y benchmarks.

Por estas deudas T10 y E026 permanecen `IMPLEMENTING`. La tanda operativa 22 cierra la política
ejecutable del compilador, no declara implementado `async/await`.
