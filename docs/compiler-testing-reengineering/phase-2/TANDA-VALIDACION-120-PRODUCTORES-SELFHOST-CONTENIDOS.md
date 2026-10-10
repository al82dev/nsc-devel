# Tanda Validacion 120: productores self-host contenidos en el repositorio

Fecha: 2026-10-10

## Scope lock

Esta tanda corrige exclusivamente la contencion de `source_path` en los
catalogos `SelfHost.Contracts` y `SelfHost.Bootstrap.Infrastructure`. No
modifica rutas vivas, catalogos, modulos, manifiestos, inventarios, casos,
producto ni bateria historica. No compila ni ejecuta `nsc`, no ejecuta baseline
y no toca la unidad `C:`.

## Defecto confirmado

Ambos consumidores aceptaban cualquier `source_path` cuyo destino existiera.
Una ruta absoluta o una ruta relativa con `..` podia resolver fuera del
checkout y acreditar un archivo externo como productor del contrato.

## Causa raiz

La validacion comprobaba existencia mediante `is_file()`, pero no verificaba
que la ruta original fuera relativa ni que su destino resuelto permaneciera
dentro de `repository_root`.

## Correccion

`resolve_repository_file` aplica ahora una unica frontera:

1. rechaza rutas absolutas y rutas con unidad;
2. resuelve la ruta contra el repositorio;
3. exige que el destino resuelto sea descendiente del repositorio;
4. exige que el destino sea un archivo existente.

Bootstrap Infrastructure y Contracts usan esa frontera antes de acreditar el
productor. Los `73` contratos Bootstrap Infrastructure y los `48` contratos
Contracts vivos conservan sus rutas y cierre; en total se validan `121` filas
de catalogo y `726` casos.

## Complejidad y costos

La comprobacion de cada ruta es `O(P)` para `P` segmentos y usa espacio
`O(P)` durante la normalizacion. No se agrega recursion, procesos ni
artefactos.

## Evidencia reproducible

Pruebas dirigidas:

```text
python -m unittest tests.compiler-suite.controller.tests.test_bootstrap_infrastructure_support tests.compiler-suite.controller.tests.test_selfhost_remaining_catalog_boundaries
Ran 13 tests in 7.304s
OK
```

Cuatro mutaciones, dos rutas absolutas y dos traversals, se rechazan en los dos
consumidores. Los catalogos vivos siguen satisfaciendo esquema, linaje,
cobertura y existencia.

Suite Python completa:

```text
python -m unittest discover -s tests/compiler-suite/controller/tests -p 'test_*.py'
Ran 270 tests in 28.228s
OK
```

## Limite de la afirmacion

Esta evidencia prueba que los productores de esos dos catalogos pertenecen al
checkout auditado. No acredita ejecucion funcional del producto. La meta
permanece activa hasta que la matriz ejecutable completa acordada termine con
cero errores inesperados.
