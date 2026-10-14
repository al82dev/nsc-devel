# NS304-T13-F03 — Scope/stack

- **Estado**: IMPLEMENTADO (construcción stack + drop + escape de return/capture)
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
- **Escape**: retornar un objeto `scope` se rechaza (`scope object '...' lives in the current frame and
  cannot be returned`); capturarlo en una lambda también se rechaza.
- Verificado: `TL-LANG-484` (construcción + lectura + destructor exactamente una vez, exit 0),
  `TL-LANG-485` (retorno rechazado). Guards 432/472 PASS.

## Residual

- El almacenamiento en campo/static de una referencia a un objeto `scope` no tiene aún un diagnóstico
  dedicado (el caso de campo de mayor duración de la decisión 13). El retorno y la captura ya están
  cerrados.
