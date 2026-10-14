# NS304-T11-F06 — Traits sin dyn escapable

- **Estado**: IMPLEMENTADO (witness estático de borrowed struct; conversión a `dyn` rechazada sin box)
- **Rama**: development

## Resultado (witness estático — 2026-10-14)

- `ProjectNameLookup` (predeclare de trait impl): el target de un `impl` puede ser un **borrowed
  struct** (`isBorrowedStructTarget`), no solo una clase no estática.
- Verificado: `impl Getter for View` sobre `borrowed struct View` + `v.Get()` despacha estático por
  el witness → exit 5. Registrado `TL-LANG-415` (**PASS**). La conversión a `dyn Getter` de la vista se
  rechaza por la conversión ordinaria (*"cannot initialize local … from …"*), sin box.

## Residual (calidad de diagnóstico, no surface)

- **Diagnóstico específico de escape/box**: `dyn Getter` de un ref-like hoy se rechaza con el mensaje
  genérico de conversión; falta el diagnóstico puntual de "escape/box" que el ítem pide. Es calidad
  de diagnóstico, no una surface nueva.

## Auditoría (línea base)

Contrato (tracking): *Implementar traits sin dyn escapable*.
Plan T11-F06 (DeepSeekDocs): *permitir implementación estática/witness de traits por ref-like;
rechazar conversión a `dyn` cuyo storage pueda escapar o requerir box; probar dispatch genérico sin
heap*.
