# NS304-T13-F03 — Scope/stack

- **Estado**: IMPLEMENTADO (construcción stack + drop + escape de return/capture/field/static)
- **Rama**: development

## Resultado (2026-10-14)

- **Surface**: `scope Tipo nombre = new Tipo(argumentos);` — `scope` es modificador contextual de
  declaración local (no `scope { ... }`).
- **Parser**: `scope` se consume en `parseVariableDeclaration` y marca la declaración `local:scope`.
- **Sema**: `MethodLocalSymbol::isScope`; el objeto `scope` tiene `objectLifetimeOwnership = Owned` y
  `storageOwnership = None` (frame, nunca heap). El cleanup es `RaiiDestroy` (el destructor corre una
  vez al salir del alcance; no se llama al deallocator).
- **HIR**: una declaración `scope` fuerza `stackPromotedObject` sobre la construcción (con argumentos de
  ctor), de modo que baja a `alloca` del frame.
- **Escape**: retornar (`scope object '...' cannot be returned`), capturar en lambda
  (`cannot be captured by a lambda because it lives in the current frame`) o almacenar en campo/static
  (`cannot be stored in a field or static`) se rechazan con diagnóstico.
- Verificado: `TL-LANG-484` (construcción + lectura + destructor exactamente una vez, exit 0),
  `TL-LANG-485` (retorno rechazado), `TL-LANG-486` (campo rechazado). Guards 432/472 PASS.

## Commits

- `e6dc0f8f` — scope/stack + drop + escape (retorno/captura).
- `1591cdf3` — rechazo de captura escapable.
- `5a63fee7` — rechazo de almacenamiento en campo/static.
