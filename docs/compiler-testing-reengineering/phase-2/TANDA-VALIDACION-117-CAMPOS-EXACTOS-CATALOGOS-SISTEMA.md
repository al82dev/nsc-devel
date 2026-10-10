# Tanda Validacion 117: campos exactos en catalogos de sistema

Fecha: 2026-10-10

## Scope lock

Esta tanda corrige exclusivamente los campos JSON consumidos por los gates de
BuildSystem y Packaging. No modifica catalogos, manifiesto, inventarios,
casos, producto ni bateria historica. No compila ni ejecuta `nsc`, no ejecuta
baseline y no toca la unidad `C:`.

## Defecto confirmado

Ambos gates convertian con `str(...)` rangos, arreglos de casos,
disposiciones, contratos y claves de contradiccion antes de derivar sus
particiones. En Packaging, cada elemento de `case_ids` tambien se convertia.
Esto perdia el tipo JSON original y producia errores secundarios de linaje o
cobertura en lugar de rechazar el catalogo mal formado en su frontera.

## Causa raiz

Los validadores de sistema compartian lectores JSON estrictos para sintaxis y
claves duplicadas, pero no una extraccion textual estricta. Cada gate resolvia
la extraccion localmente mediante conversion.

## Correccion

`required_string_field` exige ahora una cadena JSON no vacia sin conversion.
La regla se aplica a:

- `case_range` y `disposition` del catalogo BuildSystem;
- `contract_id` y `disposition` de clasificaciones BuildSystem;
- `case_ids` y `disposition` del catalogo Packaging;
- `contract_id` y `disposition` de clasificaciones Packaging;
- `scope` y `field` de contradicciones Packaging.

Las identidades ya validadas por el manifiesto canonico o por CSV se consumen
directamente. Packaging conserva por contrato el orden tipado de `case_ids` y
lo reutiliza para derivar sus conteos.

## Complejidad y costos

Las comprobaciones textuales son `O(1)` por campo; los arreglos conservan
`O(K)` para `K` identidades. Los gates mantienen complejidad lineal respecto a
sus catalogos. No agrega recursion, procesos ni artefactos.

## Evidencia reproducible

Pruebas dirigidas:

```text
python -m unittest tests.compiler-suite.controller.tests.test_component_gate_support tests.compiler-suite.controller.tests.test_system_catalog_boundaries
Ran 17 tests in 0.885s
OK
```

Los tres metodos nuevos de sistema ejecutan los cinco catalogos vivos y once
mutaciones sobre rangos, arreglos, disposiciones, contratos y contradicciones.
El soporte comun prueba ademas cinco valores coercibles contra el nuevo
validador textual.

Suite completa:

```text
python -m unittest discover -s tests/compiler-suite/controller/tests -p 'test_*.py'
Ran 260 tests in 19.068s
OK
```

Catalogos vivos:

| Catalogo | Filas |
|---|---:|
| BuildSystemContractCatalog.jsonl | 76 |
| BuildSystemCaseClassifications.jsonl | 422 |
| PackagingContractCatalog.jsonl | 3 |
| PackagingCaseClassifications.jsonl | 10 |
| PackagingPlanContradictions.jsonl | 1 |
| **Total** | **512** |

Una busqueda dirigida confirma cero conversiones restantes para los campos
auditados en ambos gates.

## Limite de la afirmacion

Esta evidencia prueba tipos, particiones y cierre de los cinco catalogos de
sistema. No acredita ejecucion funcional del producto. La meta permanece
activa hasta que la matriz ejecutable completa acordada termine con cero
errores inesperados.
