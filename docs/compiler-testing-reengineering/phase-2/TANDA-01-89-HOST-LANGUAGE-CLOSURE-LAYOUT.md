# Tanda 01.89 — Host.Language: layout público de closure

Fecha: 2026-10-09

## Alcance

| Campo | Valor |
|---|---|
| Caso | `TL-LANG-034` |
| Contrato | `CL-LANG-003` |
| Capacidad real | `computeClosureLayout` |
| Oráculo público | `ClosureLayout::size`, `ClosureLayout::alignment` |
| Producto modificado | No |

## Matriz

- Closure vacío: tamaño 0 y alineación 1.
- Captures 1/1, 8/8 y 4/4: tamaño final 24 y alineación 8.
- Las mismas clases reordenadas: tamaño final 16 y alineación 8.

El caso comprueba padding y alineación final mediante los únicos campos públicos.
No duplica offsets internos ni exige una representación que el compilador no
expone.

## Archivos autorizados

- `tests/compiler-suite/host/modules/LanguageCallableSemanticsModule.cpp`.
- `tests/compiler-suite/manifest/cases.jsonl`.
- Este documento y el tracking de Fase 2.

## Evidencia

- Build dirigido `nsc_component_tests` Release: correcto con warnings como
  errores.
- Ejecución exclusiva `--case TL-LANG-034`: 1/1, `PASS`, 837 microsegundos.
- No se modificó ningún archivo bajo `include/` o `src/`.
- No se ejecutaron la suite completa, batería histórica, baseline, cobertura,
  instalación ni despliegue.

## Estado

`TL-LANG-034` queda implementado y verificado contra la superficie pública real.
