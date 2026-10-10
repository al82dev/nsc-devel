# Tanda Validacion 116: identidades exactas self-host

Fecha: 2026-10-10

## Scope lock

Esta tanda corrige exclusivamente las coerciones textuales de
`validate_manifest_cases` y `classification_sets` en el inventario compartido
self-host. No modifica manifiestos, clasificaciones, inventarios, casos,
producto ni bateria historica. No compila ni ejecuta `nsc`, no ejecuta baseline
y no toca la unidad `C:`.

## Defecto confirmado

Ambas fronteras convertian mediante `str(...)` valores JSON antes de comparar
identidades, contrato, capa, perfil o disposicion. Un entero o booleano no
conservaba su tipo original y el diagnostico posterior describia un cambio de
linaje o una disposicion desconocida en lugar de un documento mal formado.

## Causa raiz

La implementacion usaba conversion como atajo de extraccion. Esto duplicaba
parcialmente el esquema y debilitaba la causalidad de los errores, aunque los
consumidores posteriores pudieran rechazar varias de las cadenas resultantes.

## Correccion

`required_string_field` exige ahora una cadena JSON no vacia y devuelve el
valor sin conversion. La frontera se aplica a:

- `case_id`, `contract_id`, `layer` y `profile` del manifiesto raw;
- `case_id`, `contract_id` y `disposition` de clasificaciones;
- `component` de clasificaciones cuando el campo esta presente.

Los campos descriptivos adicionales permanecen extensibles porque no forman
parte de la identidad consumida por estas funciones.

## Complejidad y costos

Cada campo se valida en tiempo y memoria `O(1)`; cada catalogo conserva su
recorrido lineal `O(N)`. No agrega recursion, procesos ni artefactos.

## Evidencia reproducible

Pruebas dirigidas:

```text
python -m unittest tests.compiler-suite.controller.tests.test_selfhost_inventory_boundaries tests.compiler-suite.controller.tests.test_manifest_tag_boundaries
Ran 7 tests in 7.091s
OK
```

Los cuatro metodos nuevos contienen 24 mutaciones: doce sobre identidades del
manifiesto, nueve sobre identidad y disposicion de clasificaciones, y tres
sobre el componente opcional.

Suite completa:

```text
python -m unittest discover -s tests/compiler-suite/controller/tests -p 'test_*.py'
Ran 256 tests in 18.011s
OK
```

Catalogos vivos:

| Componente | Filas clasificadas |
|---|---:|
| Host.Source | 4 |
| SelfHost.Basic | 1 |
| SelfHost.Source | 14 |
| SelfHost.Types | 13 |
| **Total** | **32** |

Una busqueda dirigida confirma cero conversiones `str(row.get(...))` para los
cinco campos auditados dentro de `selfhost_inventory.py`.

## Limite de la afirmacion

Esta evidencia prueba tipos e identidades de las fronteras compartidas y la
compatibilidad de los cuatro catalogos vivos. No acredita ejecucion funcional
del producto. La meta permanece activa hasta que la matriz ejecutable completa
acordada termine con cero errores inesperados.
