# NS304-T08-F03 — Métodos, propiedades y constantes estáticos de extensión

- **Estado**: CERRADO (parser + directo/import + `T.Miembro()`)
- **Rama**: development

## Resultado

- **Parser**: `extension T { static int Twice() { ... } }` ya no rechaza el `static`; la función se
  marca `static` + `ns3-extension-static` y NO recibe el receptor implícito `__nsc_extension_this`.
- Prueba `TP-PAR-538` (parser) → **PASS**.

### Cierre `T.Miembro()` — verificado 2026-10-13

- `bindExtension` maneja el `typeReceiver`: la estática (0 params) forma un `StaticMethodGroup` sin
  receiver; `analyzeCall` rutea el group estático a `analyzeNamespaceFunctionCall` sin forwarded
  receiver (el path del método de instancia no aplica).
- Verificado: `extension Box { static Twice() } ... Box.Twice()` → exit 7. `TL-LANG-389` PASS.
  Commits `e2f747a4`, `f56ff7b3`.

