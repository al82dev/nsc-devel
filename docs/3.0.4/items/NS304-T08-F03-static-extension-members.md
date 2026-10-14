# NS304-T08-F03 — Métodos, propiedades y constantes estáticos de extensión

- **Estado**: PARCIAL (parser de función estática sin receptor) / residual (lookup Sema `T.Miembro()`)
- **Rama**: development

## Resultado

- **Parser**: `extension T { static int Twice() { ... } }` ya no rechaza el `static`; la función se
  marca `static` + `ns3-extension-static` y NO recibe el receptor implícito `__nsc_extension_this`.
- Prueba `TP-PAR-538` (parser) → **PASS**.

## Residual

- **Lookup Sema**: la invocación `T.Miembro()` (type-qualified) no resuelve: `extension Box { static
  int Twice() } ... Box.Twice()` → `receiver/type 'Box' has no compatible member named 'Twice'`.
  Verificado ronda 101. La invocación directa `Twice()` (mismo scope) y por `using Suite` SÍ pasan
  (exit 7) tras el fix de namespace global (T08-F01). Falta registrar la estática de extensión en el
  lookup del TIPO EXTENDIDO. Propiedades y constantes estáticas: mismas rutas pendientes.

