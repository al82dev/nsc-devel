# Tanda 01.90 — Host.Language: división entera

Fecha: 2026-10-09

## Alcance

| Campo | Valor |
|---|---|
| Caso | `TL-LANG-201` |
| Contrato | `CL-LANG-016` |
| Operación pública | `IntegerArithmeticOperation::Divide` |
| Producto modificado | No |
| Operación inexistente excluida | Remainder |

## Matriz

- División unsigned ordinaria `255 / 3` en 8 bits.
- División por cero bajo Checked, Wrapping, Saturating y Unchecked.
- `i8::MIN / -1` bajo las cuatro políticas.
- Bits, flag de overflow, flag de comportamiento indefinido y diagnóstico
  exactos.

La prueba caracteriza únicamente `Divide`, que sí pertenece al enum y al
evaluador vigentes. No introduce ni presupone remainder.

## Presupuesto de cambio

- `tests/compiler-suite/host/modules/LanguageNumericCoreModule.cpp`.
- `tests/compiler-suite/manifest/cases.jsonl`.
- Este documento, `TRACKING.md` y `CURSOR.json`.

## Evidencia

- Build dirigido `nsc_component_tests` Release: correcto con warnings como
  errores.
- Ejecución exclusiva `--case TL-LANG-201`: 1/1, `PASS`, 281 microsegundos.
- No se modificó ningún archivo bajo `include/` o `src/`.
- No se ejecutaron la suite completa, batería histórica, baseline, cobertura,
  instalación ni despliegue.

## Estado

`TL-LANG-201` queda implementado y verificado contra `Divide`, la operación
pública vigente.
