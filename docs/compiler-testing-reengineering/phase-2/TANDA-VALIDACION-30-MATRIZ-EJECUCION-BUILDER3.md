# Tanda de Validación 30: matriz lógica de ejecución de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON EVIDENCIA ESTÁTICA

## Riesgo cubierto

Una selección visible podía omitir casos, incluir perfiles más costosos de los
pedidos, duplicar una combinación o convertir una disposición pendiente en una
ejecución implícita.

## Prueba permanente

`test_nscbuilder3_matrix.py` deriva el resultado esperado desde cada fila viva
y lo coteja con las APIs productivas de selección y expansión. Cubre:

1. particiones completas por ola, componente y contrato;
2. selección puntual representativa al inicio, centro y final del catálogo;
3. perfiles acumulativos `PR`, `NIGHTLY`, `WEEKLY` y `Todos`;
4. producto exacto de `Debug`/`Release` y `O0`/`O1`/`O2`/`O3`;
5. casos sin dimensión de optimización, representados una sola vez como `NA`;
6. unicidad de cada clave concreta;
7. exclusión total de disposiciones no ejecutables.

## Evidencia

- Casos canónicos: `9,587`.
- Casos ejecutables: `5,695`.
- Disposiciones no ejecutables: `3,892`.
- Variantes concretas derivadas: `12,542`.
- Colisiones de clave: `0`.
- Pruebas dirigidas: `4/4` PASS.
- Suite Python completa del controlador: `86/86` PASS.

No se modificó Builder 3 porque el cotejo no demostró discrepancias. No se
configuró, compiló ni ejecutó producto; no hubo baseline, despliegue ni escritura
en `C:`.
