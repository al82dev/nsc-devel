# Congelamiento 3.0, elemento 138 — TP-PAR-397 sigue el contrato de P4 (`RefAt`)

Fecha: 2026-10-27. Revisión de partida: `1a213f28b`.

## Nota de trabajo

```text
HEAD: 1a213f28b
Elemento: 138 del plan de 153 (falla de NSC Builder 3 reportada por Alberto: FAIL=2, TP-PAR-397 Debug y Release)
Contrato vigente: P4 (`80b186758`, doc 29): foreach sobre un span lee el elemento con RefAt(index)
Documento histórico que podría contradecirlo: el oráculo de TP-PAR-397, que exigía Data()
Archivos exactos que se modificarán:
  tests/compiler-suite/host/modules/ParserForeachSemanticsModule.cpp
  docs/3.0/decisiones-congelamiento/57-TP-PAR-397-FOREACH-SPAN-CON-REFAT.md
```

## Problema

P4 cambió a propósito el lugar del elemento en `foreach` sobre `Span`/`ReadOnlySpan`. Antes era
`Data()[index]` y ahora es `RefAt(index)`: así un span sobre un arreglo con mapa de vida lee cada elemento por
ese arreglo. TP-PAR-397 siguió exigiendo exactamente un `Data`, y el commit de P4 no la ajustó. Reproducido con
`nsc_component_tests --case TP-PAR-397` (Release): `FAIL`.

## Decisión de Alberto (2026-10-27)

Ajustar únicamente esa prueba:

- `RefAt == 1`;
- `Data == 0`;
- mensaje `RefAt/index`.

No cambia ningún otro caso ni la conducta del compilador. Va en su propio commit.

## SCOPE LOCK

- **Cambia.** El oráculo y el mensaje de TP-PAR-397.
- **Idéntico.** El compilador y los demás casos del módulo (TP-PAR-395, 396, 398, 399 y 400).

## Validación

`nsc_component_tests` Debug y Release de este árbol: `--case TP-PAR-397` da `PASS` en los dos.

## No verificado

- Suite completa: gate final.
