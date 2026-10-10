# Tanda Validacion 114: esquema exacto del catalogo bootstrap

Fecha: 2026-10-10

## Scope lock

Esta tanda corrige exclusivamente el cargador de
`SelfHostBootstrapInfrastructureCatalog.jsonl`. No modifica el catalogo, sus
73 contratos, sus 438 casos, inventarios, producto ni bateria historica. No
compila ni ejecuta `nsc`, no ejecuta baseline y no toca la unidad `C:`.

## Defecto confirmado

El lector JSON rechazaba claves duplicadas, pero despues convertia con
`str(...)` los siete campos textuales y cada elemento de `case_ids`. Por ello,
valores como enteros o booleanos podian cambiar de tipo antes de validar
identidad, linaje, rutas y metadatos. Tambien se aceptaban campos desconocidos
y la ausencia de campos solo producia errores indirectos.

## Causa raiz

El parseo estructural y la extraccion del contrato estaban separados por una
capa de coercion. La unicidad de claves no equivalia a un esquema tipado.

## Correccion

El cargador exige ahora, antes del linaje:

- exactamente `contract_id`, `file_id`, `source_path`, `category`, `language`,
  `purpose`, `oracle` y `case_ids`;
- siete cadenas no vacias, sin conversion implicita;
- `case_ids` como arreglo no vacio de cadenas no vacias y unicas.

Los valores de `category`, `language` y los textos descriptivos no se congelan
en enums. El denominador sigue derivandose del inventario y no de conteos
codificados en la prueba.

## Complejidad y costos

Cada fila recorre ocho campos y sus `K` identidades una sola vez. La carga
completa mantiene `O(N + C)` en tiempo y memoria, donde `N` es el numero de
contratos y `C` el numero total de casos. No agrega recursion, procesos ni
artefactos.

## Evidencia reproducible

Prueba causal:

```text
python -m unittest tests.compiler-suite.controller.tests.test_bootstrap_infrastructure_support
Ran 5 tests in 0.208s
OK
```

Los cinco metodos incluyen 28 mutaciones de esquema: campos ausentes o
desconocidos, 21 coerciones de campos textuales y cinco formas invalidas de
`case_ids`. Tambien conservan la prueba de claves duplicadas y cargan el
catalogo vivo.

Suite completa de controladores:

```text
python -m unittest discover -s tests/compiler-suite/controller/tests -p 'test_*.py'
Ran 252 tests in 16.890s
OK
```

Catalogo vivo:

```text
CONTRACTS 73
CASES 438
MISSING_PRODUCERS 0
```

Una busqueda dirigida sobre el cargador confirma cero usos restantes de
`str(value[...])`.

## Limite de la afirmacion

Esta evidencia prueba el esquema del catalogo, su cierre con el inventario y
la existencia de sus productores. No acredita la ejecucion funcional de esos
productores. La meta permanece activa hasta que la matriz ejecutable completa
acordada termine con cero errores inesperados.
