# `finally` con excepciones, funclets sin personality y uniones con miembros destructibles

Fichas NSC-NEW-005, NSC-NEW-024, NSC-NEW-025 y NSC-NEW-013.

## Defectos corregidos

- **NSC-NEW-005, NSC-NEW-024, NSC-NEW-025.** Un `try/catch` dentro de un `finally` síncrono, o una
  llamada que puede lanzar dentro del `finally` de un método `async`, fallaba en LLVM ("Cleanup
  funclets for the SEH personality cannot contain exceptional actions"). Una excepción NativeSharp
  ejecuta el `finally` en código padre (`eh.finally.catch`); la copia que se emite dentro del cleanup
  funclet SEH es la que alcanza una excepción ajena (SEH de código no NativeSharp), y un funclet así
  no puede contener `invoke` ni `catchswitch`. Esa copia se emite ahora en modo de respaldo
  (`foreignFinallyFallback_`): sus llamadas son `nounwind`, un `try` interior baja solo su cuerpo
  protegido y su `finally`, y un `throw` interior termina con trampa. Decisión aplicada sin
  consulta: durante el desenrollado de una excepción ajena, el `finally` se ejecuta pero su `catch`
  interior no atrapa una segunda excepción; el proceso termina, como un destructor que lanza
  durante el desenrollado en C++. Las excepciones NativeSharp no cambian.
- **NSC-NEW-013 (personality).** Un `cleanuppad` emitido antes de que la función tuviera
  personality hacía fallar el verificador ("CleanupPadInst needs to be in a function with a
  personality"). `buildInvokeInCurrentFunclet` y `emitExceptionalCleanupChain` fijan ahora la
  personality SEH antes de crear el primer pad.
- **NSC-NEW-013 (uniones).** Una `union` cruda con un miembro cuyo valor necesita destrucción
  (`Rc<T>`, `Arc<T>`, un struct con campo owned) no tiene discriminante para saber qué alternativa
  destruir: se liberaba la equivocada o ninguna. `validateRawUnion` la rechaza con alternativa
  ("use a variant"). Decisión aplicada sin consulta: es una restricción nueva; el `variant` es la
  forma con discriminante.
- **NSC-NEW-013 (offsets explícitos).** Con `at(N)` todo campo es un posible solape del campo
  prestado y el análisis de cuerpos no tiene layouts, así que sobrescribir otro campo mientras vive
  una vista se rechaza de forma conservadora ("cannot overwrite owner field while a borrowed view
  is alive"), igual que un solape real. Los casos estáticos de tupla anidada con `Weak`/`Arc`
  quedan solo para `Rc`, que es la combinación que el SDK publica.

## Pruebas

- `nsc.integration.async-finally-exception-replacement` y `nsc.integration.shared-async-finally-owner`
  (registradas en CMake; antes no tenían `add_test`), `nsc.integration.async-finally-frame-regression`.
- `nsc.sdk.shared-public-pointers`: `union_without_view` y `overwrite_union_view` esperan el rechazo
  de la unión, `overwrite_explicit_disjoint_view` el rechazo conservador.

NSC-NEW-026 (`yield break` desde un `catch` con cleanup que lanza) sigue abierto: es la salida
estructurada a través de un cleanup funclet activo, una causa distinta.
