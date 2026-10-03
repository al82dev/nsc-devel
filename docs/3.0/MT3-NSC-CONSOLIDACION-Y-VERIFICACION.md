# MT3-NSC — Consolidación y verificación del catálogo 3.0

## Alcance

Esta tanda consolida el inventario normativo de NSC en
`mkdown/NativeSharp-Language-3.0-Roadmap.md` y crea la primera automatización de
la mega-tanda. No implementa todavía las 120 features: registra honestamente su
estado inicial y evita que una vertical parcial se confunda con una tanda cerrada.

## Cambios

- se recuperaron `NS3-T06-F08`, `NS3-T10-F08`, `NS3-T12-F07` y
  `NS3-T13-F08`, presentes en el maestro anterior pero ausentes del roadmap;
- el catálogo NSC queda formado por 120 IDs únicos entre T00 y T17;
- se agregó una tabla ejecutiva con estado y trabajo abierto por tanda;
- P00–P11, SH00–SH54, `vsBeef` y las 156 limitaciones quedan definidos como
  gates/fuentes de evidencia, no como catálogos paralelos;
- el gate previo de NSC 2.0 fue retirado como bloqueo de la línea 3.0;
- se creó `tools/verify_native_sharp_3_plan.py` para validar el catálogo y la
  tabla compacta de tracking;
- se agregaron pruebas unitarias para faltantes, duplicados, texto extra y
  contadores inconsistentes.

## Archivos nuevos

- `tools/verify_native_sharp_3_plan.py`: verificador automático y reutilizable
  por las siguientes tandas de componente.
- `tests/unit/test_verify_native_sharp_3_plan.py`: regresiones del verificador.
- este documento: evidencia y frontera exacta del commit.

## Mejora

El plan deja de depender de conteos manuales o de interpretar varios documentos
contradictorios. Un ID faltante, duplicado o un tracking con texto adicional o
contadores falsos produce un fallo dirigido antes de cualquier baseline.

## Pruebas previstas

1. unitarias del verificador;
2. verificación contra el roadmap y tracking reales;
3. suite normal corta de NSC en O0, excluyendo baseline, self-host prolongado,
   certificación y matrices O1/O2/O3.
