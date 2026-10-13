# NS304-T08-F03 — Métodos, propiedades y constantes estáticos de extensión

- **Estado**: PENDIENTE (gap documentado)
- **Rama**: development

## Estado

- Los miembros de INSTANCIA de un bloque `extension T { ... }` (métodos + propiedades) ya bajan
  correctamente (T08-F01/F02, `TL-LANG-323/324/330`).
- Los miembros **estáticos** (`extension T { static int Twice() { ... } }`) no están soportados: en
  `parseGlobalDeclaration` el `static` sobre una función de namespace se rechaza
  ("static namespace declarations/file-local linkage are not supported"), por lo que un miembro
  estático dentro de un bloque extension no llega a `parseExtensionDeclaration` con el modificador.

## Residual / camino

- Permitir `static` dentro de `parsingExtensionBody_` (función de namespace estática sin receptor),
  marcar `ns3-extension-static`, y resolver la invocación `T.Miembro()` en Sema (lookup de función
  de namespace estática por tipo extendido).
