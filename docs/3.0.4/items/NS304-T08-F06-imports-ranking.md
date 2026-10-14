# NS304-T08-F06 — Imports, precedencia, ambigüedad y lookup incremental

- **Estado**: IMPLEMENTADO (precedencia nominal-antes-extensión + ranking por constraints/visibilidad + índice por identidad)
- **Rama**: development

## Estado

- **Nominal antes de extensión**: `bindExtension` es el último paso del member lookup — el miembro
  nominal gana. (`ProjectMethodBodyModel.cpp`)
- **Visibilidad** (TL-LANG-405): `public extension Box` resuelve cross-module; `internal extension
  HiddenBox` se rechaza con `no compatible member`.
- **Ranking por constraints** (TL-LANG-412): `extension Box<T> where T : Special` despacha sobre la
  especialización cerrada `Box<I>` (`b.Twice()` → exit 6) vía inferencia del receptor + especialización
  de `Twice<T>`.
- **Índice por identidad**: `extensionBindingBySyntax_` ahora indexa por identidad de fuente
  (`fileId + begin offset`), no por puntero de nodo AST; el binding incremental sobrevive re-parses
  que reasignan la asignación del nodo. (`extensionSourceIdentity`)

## Pruebas

- `TL-LANG-323` (extension method), `TL-LANG-399` (operador intermodular), `TL-LANG-405`
  (visibilidad), `TL-LANG-412` (ranking por constraints). Guards sin regresión.

## Commits

- `2168021e` — índice de binding de extensión por identidad de fuente.
