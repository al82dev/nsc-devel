# Tanda Validacion 119: identidad exacta en modulos self-host materializados

Fecha: 2026-10-10

## Scope lock

Esta tanda corrige exclusivamente la coercion de la primera columna en
`validate_unique_materialized_ids`. No modifica parsers, modulos C++,
catalogos, manifiestos, inventarios, casos, producto ni bateria historica. No
compila ni ejecuta `nsc`, no ejecuta baseline y no toca la unidad `C:`.

## Defecto confirmado

El helper compartido aceptaba una fila materializada con un numero, booleano u
otro objeto como identidad y la convertia mediante `str(...)` antes de detectar
vacios o duplicados. La representacion textual podia participar en el indice
aunque el productor no hubiera emitido una identidad valida.

## Causa raiz

El helper declaraba filas de objetos genericos, pero asumía que la primera
columna siempre provenia de una captura textual. Esa precondicion no se
comprobaba en la propia frontera compartida.

## Correccion

`validate_unique_materialized_ids` exige ahora que `row[0]` sea una cadena no
vacia antes de indexarla. La deteccion existente de duplicados permanece
inalterada. Sus tres consumidores reales conservan sus denominadores:

| Componente | Identidades materializadas |
|---|---:|
| SelfHost.Bootstrap | 23 |
| SelfHost.CodeGen | 136 |
| SelfHost.Documentation | 12 |
| **Total** | **171** |

## Complejidad y costos

La comprobacion agrega costo `O(1)` por fila. El indice conserva tiempo y
espacio `O(N)` para `N` identidades. No se agrega recursion, procesos ni
artefactos.

## Evidencia reproducible

Pruebas dirigidas:

```text
python -m unittest tests.compiler-suite.controller.tests.test_selfhost_inventory_boundaries
Ran 6 tests in 2.608s
OK
```

Cuatro mutaciones (`int`, `bool`, cadena vacia y `None`) se rechazan. Los tres
gates estaticos ejecutan sus productores vivos y terminan correctamente.

Suite Python completa:

```text
python -m unittest discover -s tests/compiler-suite/controller/tests -p 'test_*.py'
Ran 268 tests in 27.118s
OK
```

Una busqueda dirigida confirma cero usos restantes de `str(row[0])` en los
controladores de integracion.

## Limite de la afirmacion

Esta evidencia prueba la frontera de identidad materializada y sus tres
consumidores estaticos. No acredita ejecucion funcional del producto. La meta
permanece activa hasta que la matriz ejecutable completa acordada termine con
cero errores inesperados.
