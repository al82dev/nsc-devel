# NS304-T01-F06 — locales `static` y lifetime de módulo

## Contrato

Una declaración local `static` conserva scope y resolución léxicos, pero su
almacenamiento pertenece al módulo. La inicialización ocurre al alcanzar la
declaración por primera vez y exactamente un hilo reclama el intento. Los
estados `uninitialized`, `initializing`, `initialized` y `failed` comparten el
contrato terminal de inicialización estática; una reentrada del hilo dueño no
espera sobre sí misma y los demás hilos esperan hasta un estado terminal.

Un local propietario o un valor con drop no se destruye al salir del bloque.
Su thunk de descarga comprueba que la inicialización terminó, reclama el estado
una sola vez y destruye el almacenamiento desde `llvm.global_dtors`. Un local
`ref` y un VLA se rechazan porque no pueden prometer lifetime de módulo.

## Implementación

- El parser conserva `static` en cada declarador, incluidos los prefijos
  locales `raii`, `const`, `volatile`, `var`, `pin` y `ref`.
- `MethodLocalSymbol`, HIR y MIR transportan `staticLocal`; dump y cache identity
  lo incluyen explícitamente.
- Sema mantiene lookup/definite assignment, prohíbe referencias y VLA, excluye
  promoción a pila y no crea cleanup léxico para owners/value-drop estáticos.
- LLVM crea storage, state y owner privados por ordinal local. El statement
  emite fast path, CAS, espera, reentrancia y publicación terminal `2`/`3`.
- Los locales con destrucción generan `__nsc_static_local_destroy_N` y una
  entrada `llvm.global_dtors`; un local nunca inicializado no se destruye.

## Evidencia puntual

- `TL-LANG-299`: `PASS`, 90358 microsegundos. Dos locales estáticos llegan a
  MIR sin cleanup léxico y los negativos `ref`/VLA conservan causa exacta.
- Guards: `TL-LANG-297` `PASS` en 22235 microsegundos, `TL-LANG-298` `PASS` en
  7 microsegundos, `TL-LANG-160` `PASS` en 3885 microsegundos y `TL-LANG-294`
  `PASS` en 7 microsegundos.
- `nsc_stage37_static_initialization_tests`: `PASS`, 72 checks. Verifica LLVM,
  CAS/espera/reentrancia/fallo, `llvm.global_dtors`, verifier, O2 y COFF.
- El primer build focal reejecutó CMake automáticamente por un `GLOB mismatch`
  de documentos 3.0.4 añadidos desde la configuración anterior. No se cambió
  CMake ni toolchain. El rebuild posterior usó el árbol coherente y completó
  tres pasos sin regeneración.

## Gate F06

```text
first reach = claim state 0 -> 1 -> initialize -> publish 2
other thread = wait while 1, then observe 2 or 3
same thread reentry = do not self-deadlock
scope exit = no destruction
module unload = destroy exactly once iff state == 2
```
