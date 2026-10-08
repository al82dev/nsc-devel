# Tanda 20G — Auditoría de ejecución uno a uno

## Objetivo

Impedir que la migración sustituya las 684 pruebas históricas por conteos
agregados. Cada identificador debe demostrar por separado que tiene una ruta de
ejecución única y que esa ruta pertenece a la capa final indicada por la matriz
de destino.

## Causa demostrada

La evidencia anterior indicaba que pasaban 289 unidades consolidadas, 76
manifiestos y 28 suites especializadas. Esos totales describían el subconjunto
ejecutado, pero no respondían qué ocurrió con cada uno de los 684 IDs
históricos. La comparación exacta mostró que ese cierre era insuficiente.

## Cambios

- `tools/audit_test_execution_parity.py` carga las 684 filas de
  `test-destination-matrix.json`.
- El registro unitario se obtiene mediante `nsc_unit_tests.exe --list`; esta
  operación enumera pruebas y no ejecuta sus cuerpos.
- Los manifiestos se descubren con una lista de trabajo explícita almacenada en
  heap, sin recursión.
- Las suites especializadas se clasifican mediante el mismo catálogo y la misma
  política de alcance que usa el harness de producción.
- Cada fila recibe dos estados independientes:
  - ejecución: `RUNNABLE`, `REGISTERED_BUT_BLOCKED`,
    `AMBIGUOUS_MULTIPLE_PATHS` o `UNMAPPED`;
  - migración: `TARGET_RUNNABLE`, `TARGET_REGISTERED_BUT_BLOCKED`,
    `TRANSITIONAL_WRONG_LAYER` o `TARGET_UNMAPPED`.
- Se generan JSON, CSV y Markdown con las 684 filas y hashes de las entradas.
- El auditor falla por defecto mientras no existan 684 rutas ejecutables y 684
  destinos finales correctos. `--allow-incomplete` sólo permite materializar el
  diagnóstico; no convierte el gate en aprobado.

## Resultado del primer barrido

| Métrica | Resultado |
|---|---:|
| Inventario histórico | 684 |
| Ruta ejecutable única | 390 |
| Registrada pero bloqueada | 41 |
| Sin ruta ejecutable | 252 |
| Destino final correcto y ejecutable | 132 |
| Capa transitoria incorrecta | 258 |
| Más de una ruta | 1 |
| Gate | `FAILED` |

La suma `390 + 41 + 252 + 1 ambigua = 684` conserva el inventario completo. La
fila ambigua permanece visible en JSON y CSV; no se absorbe en otro estado.

## Complejidad y contratos

- Tiempo: `O(H + U + M + S)`, donde `H` son filas históricas, `U` unidades,
  `M` manifiestos y `S` suites especializadas.
- Espacio: `O(H + U + M + S)` para índices y resultados reproducibles.
- La identidad se compara por ID exacto; no se aceptan prefijos ni semejanza de
  nombres.
- Cero IDs omitidos: el JSON y CSV contienen exactamente 684 filas.
- El auditor no compila NSC, no ejecuta las pruebas, no toca C: y no modifica la
  infraestructura anterior.
- `nscbuilder.py` permaneció con SHA-256
  `f67632044a0f4da02bbb66799ff1524cc7d436fc36c4080dba34c09883aa5cd6`.

## Validación dirigida

```text
python -m py_compile tools/audit_test_execution_parity.py tests/harness/tests/test_audit_test_execution_parity.py
python -m unittest tests.harness.tests.test_audit_test_execution_parity
Resultado: 6/6 OK

python tools/audit_test_execution_parity.py --repo-root A:\nsc-devel --unit-executable A:\nsc-devel\build-test-v2\tests\nsc_unit_tests.exe --allow-incomplete
Resultado: historical=684 runnable=390 target=132 blocked=41 unmapped=252
```

Los dos espacios de estados particionan exactamente las 684 filas y el CSV
contiene 684 registros de datos. No existen filas descartadas por el reporte.

## Estado

`IMPLEMENTADO; GATE DE PARIDAD FALLIDO COMO SE ESPERABA`.

El siguiente trabajo no es cerrar Builder 2: es restaurar una ruta exacta para
los 252 IDs sin mapear, eliminar los 41 bloqueos, resolver la ruta ambigua y
migrar las 258 rutas transitorias a su destino final.
