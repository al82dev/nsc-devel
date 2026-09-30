# Tanda operativa 20 — E046: contrato de smart pointers

Fecha: 2026-09-29

Rama: `development`

Alcance: E046 / NS3-R046

Estado de E046: `TARGETED-GREEN`

Estado de E047–E053: `DESIGNED/BLOCKED-BY-DEPENDENCIES`

## Objetivo

Congelar en el compilador una sola arquitectura versionada para ownership inteligente antes de
implementar tipos de biblioteca o runtime. El contrato debe impedir que `Unique`, `Rc`, `Arc` y sus
weak adquieran semánticas distintas en frontend, Runtime, NSMETA o tooling.

Los comandos nuevos son:

```text
nsc --print-smart-pointer-contract
nsc --write-smart-pointer-contract <file.contract>
nsc --verify-smart-pointer-contract <file.contract>
```

## Diagnóstico previo

- No existía código productivo para `Unique<T>`, `Rc<T>`, `WeakRc<T>`, `Arc<T>` o `WeakArc<T>`.
- Los nombres y reglas estaban repetidos en roadmaps, pero no había un artefacto versionado que
  futuros componentes pudieran verificar.
- Drop genérico, `Send/Sync`, atomics de conteo, metadata ownership completa y
  allocator/destructor/unload cross-module siguen incompletos.
- Implementar reference counting antes de esas dependencias habría creado una ABI y unas reglas de
  lifetime imposibles de certificar.

## Contrato congelado

### Familias y costo visible

- `Unique<T>`: owner heap nullable, move-only, sin `Clone`, transferencia entre threads sólo si el
  payload satisface el contrato futuro de envío;
- `Rc<T>`/`WeakRc<T>`: conteos no atómicos y uso exclusivamente local;
- `Arc<T>`/`WeakArc<T>`: conteos atómicos y cruce de threads sólo con payload `Send/Sync`;
- ninguna familia permite copia implícita; compartir requiere `Clone` explícito;
- `WeakRc` y `WeakArc` no mantienen vivo el payload.

### Invariantes

- `Upgrade` después de strong count cero falla y nunca resucita el objeto;
- strong/weak overflow se rechaza antes de wraparound;
- los ciclos se rompen explícitamente con weak; no existe GC implícito;
- el control block conserva strong/weak, allocator, destroy thunk, deallocate thunk y procedencia
  del módulo;
- un producer module no puede descargarse mientras sobreviva un control block;
- la factory shared reserva payload/control block en una asignación documentada;
- `Unique` expone `MakeUnique/Get/Borrow/Release/Reset`; las familias shared exponen las operaciones
  mínimas específicas registradas en el manifiesto.

## Implementación

`SmartPointerSemantics` contiene la tabla canónica de cinco familias y un validador común de
operaciones. El validador rechaza copia implícita, operaciones de familia incorrecta, overflow,
resurrección weak, `Rc` cross-thread, payload no thread-safe y lifetime de módulo insuficiente.

El manifiesto `NSSMARTPTR 1.0` es textual, determinista y tiene fingerprint estable. La verificación
compara el contrato completo, no sólo su encabezado, por lo que un cambio de política exige una
revisión consciente de versión.

## Archivos modificados

- `CMakeLists.txt`: integra la semántica en `nsc_compilation` y registra el target dirigido E046.
- `src/Driver/Driver.cpp`: añade print/write/verify y documenta los comandos en `--help`.

## Archivos nuevos y por qué

- `include/nativesharp/Language/SmartPointerSemantics.h`: versión, familias, thread models,
  operaciones e interfaz del manifiesto.
- `src/Language/SmartPointerSemantics.cpp`: invariantes, formato determinista, fingerprint y codec
  de archivo.
- `tests/unit/NativeSharp3E046SmartPointerTests.cpp`: 12 comprobaciones dirigidas de la arquitectura.
- `docs/3.0/TANDA-OPERATIVA-20-E046-SMART-POINTER-CONTRACT.md`: expediente de alcance, evidencia y
  deuda.

## Validación dirigida ejecutada

1. `nsc` Debug/O0 construido en `A:\nsc-devel\build-clang`: correcto.
2. Target único `nsc_ns3_e046_smart_pointer_tests`: `PASS: 12 checks`.
3. `--print-smart-pointer-contract`: manifestó las cinco familias e invariantes.
4. `--write-smart-pointer-contract` + `--verify-smart-pointer-contract`: round-trip correcto.
5. La prueba dirigida cubrió no-copy, clone explícito, downgrade/upgrade, no-resurrection, overflow,
   `Rc` local, `Arc` condicionado por `Send/Sync` y producer module pinned.
6. El contrato temporal se eliminó.
7. `NSC_BUILD_TESTS` quedó restaurado a `OFF` y `nsc` se reconstruyó en ese estado.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación, deploy ni remotas. No se modificó
ninguna biblioteca ni el depurador.

## Mejora concreta

La implementación futura ya no puede escoger accidentalmente copia implícita, ocultar atomics,
permitir resurrección weak, envolver contadores o perder allocator/destructor al cruzar DLL. Existe
una política ejecutable, versionada y verificable que todos los componentes podrán consumir.

## Deuda explícita

- E047: `Unique<T>` de biblioteca, factories, storage, drop genérico y safe borrow;
- E048: control block físico, single allocation, allocator/destroy/deallocate thunks;
- E049: `Rc<T>/WeakRc<T>` productivos y restricciones thread integradas al checker;
- E050: `Arc<T>/WeakArc<T>`, memory ordering y traits `Send/Sync` productivos;
- E051: casts, aliasing, arrays, deleters y adapters C/Win32;
- E052: ciclo/leak diagnostics, count inspection y tooling;
- E053: NSMETA, EXE/DLL, unload, ambos linkers, O0–O3, stress y benchmarks;
- dependencias duras T06-F08, T10-F08, T12-F07, T13-F08 y E020 todavía no están todas verdes.

La tanda operativa 20 cierra E046 porque la arquitectura es ahora una política ejecutable del
compilador. No afirma que los tipos smart pointer existan todavía.
