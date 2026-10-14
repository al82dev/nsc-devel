# NS304-T07-F07 — Send/Sync, thread y executor

- **Estado**: IMPLEMENTADO (derivación estructural + enforcement multithread)
- **Rama**: development

## Surface final (surface #7)

- `Send`/`Sync` son **marker traits vacíos** (`lib/NativeSharp.Core/Send.ns`, `Sync.ns`), sin
  gramática `Send T`/`Sync T`. La conformidad se deriva **estructuralmente**.
- `IThreadExecutor : IExecutor` (trait vacío) distingue un executor que puede mover el `Future` a
  otro thread; un `IExecutor` puro es single-thread local.

## Derivación estructural (`ProjectGenericConstraintModel`)

- `where T : Send`/`where T : Sync` no exige declaración nominal; evalúa los campos con
  `evaluateThreadSafety`: `RawPointer` (clave `*`) no deriva; `ref:` → `SharedReference`;
  `refmut:` → `MutableReference` (Send del referido, **no** Sync); `Value` deriva.
- `TL-LANG-464` (positivo, exit 7) / `TL-LANG-465` (raw pointer negativo, rechazo).

## Enforcement multithread (`TaskScope<TExecutor>.Spawn`)

- Resuelve el argumento `TExecutor` de la especialización `TaskScope<TExecutor>`; si implementa
  `IThreadExecutor`, verifica cada `ref` capture del `Future<void>`:
  - no-Send → `future submitted to a thread executor is not Send: capture 'x' has type 'T'`;
  - Send pero no-Sync (captura compartida) → `shared capture 'x' requires its referred type 'T'
    to be Sync`.
- `TL-LANG-466` (local executor no-Send compila, exit 7); `TL-LANG-467` (multithread negativo);
  `TL-LANG-468` (multithread positivo, exit 42); `TL-LANG-469` (shared no-Sync negativo).

## Pruebas

- `TL-LANG-464`..`TL-LANG-469` (A–F del plan).

## Commits

- `02b27eb0` — derivación estructural Send/Sync.
- `3cd6f9f6` — enforcement Send en `Spawn` (IThreadExecutor).
- `cabec748` — captura compartida exige Sync.
