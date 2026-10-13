# NS304-T08-F03 — Métodos, propiedades y constantes estáticos de extensión

- **Estado**: PARCIAL (parser de función estática sin receptor) / residual (lookup Sema `T.Miembro()`)
- **Rama**: development

## Resultado

- **Parser**: `extension T { static int Twice() { ... } }` ya no rechaza el `static`; la función se
  marca `static` + `ns3-extension-static` y NO recibe el receptor implícito `__nsc_extension_this`.
- Prueba `TP-PAR-538` (parser) → **PASS**.

## Residual

- **Lookup Sema**: resolver `T.Miembro()` como función de namespace estática de extensión (la
  invocación aún no está cableada). Propiedades y constantes estáticas: mismas rutas pendientes.

