# NS304-T08-F03 — Métodos, propiedades y constantes estáticos de extensión

- **Estado**: PARCIAL (parser de función estática sin receptor) / residual (lookup Sema `T.Miembro()`)
- **Rama**: development

## Resultado

- **Parser**: `extension T { static int Twice() { ... } }` ya no rechaza el `static`; la función se
  marca `static` + `ns3-extension-static` y NO recibe el receptor implícito `__nsc_extension_this`.
- Prueba `TP-PAR-538` (parser) → **PASS**.

## Residual

- **Lookup Sema**: la función estática de extensión se registra en el namespace de la extensión pero
  no es invocable (ni `Twice()` directo —fuera de scope— ni `T.Twice()`); falta resolver la
  invocación `T.Miembro()`/directa. Propiedades y constantes estáticas: mismas rutas pendientes.

