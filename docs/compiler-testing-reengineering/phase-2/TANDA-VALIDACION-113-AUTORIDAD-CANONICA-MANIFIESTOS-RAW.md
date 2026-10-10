# Tanda Validacion 113: autoridad canonica para manifiestos raw

Fecha: 2026-10-10

## Scope lock

Esta tanda corrige exclusivamente la frontera por la que siete gates leen las
filas raw de `tests/compiler-suite/manifest/cases.jsonl`. No cambia el parser
canonico, el manifiesto, inventarios, clasificaciones, oraculos, producto ni la
bateria historica. No compila ni ejecuta `nsc`, no ejecuta baseline y no toca
la unidad `C:`.

## Defecto confirmado

Los siete gates necesitaban conservar los objetos raw porque consumen campos
especificos de sus oraculos. Para obtenerlos, cada gate abria directamente el
JSONL y ejecutaba despues un validador local. Esos validadores locales no
reproducian el contrato completo de `nsc_suite.load_manifest`.

Una matriz sintetica demostro que los caminos raw aceptaban formas que la
autoridad canonica rechazaba: enums y escalares invalidos, objetos sustituidos
por arreglos, arreglos sustituidos por escalares, `expected_outcome` invalido y
campos desconocidos. El defecto era de frontera: un mismo manifiesto tenia dos
interpretaciones dependiendo del consumidor.

## Causa raiz

La carga raw evitaba el parser canonico antes de filtrar por componente. Los
validadores locales verificaban linaje y partes del oraculo, pero no eran ni
debian convertirse en una segunda implementacion del esquema global.

## Correccion

`component_manifest_rows` establece una sola frontera compartida:

1. carga inventarios canonicos de componentes y contratos;
2. valida el documento completo con `nsc_suite.load_manifest`;
3. conserva el orden canonico de identidades del componente;
4. relee los objetos raw y filtra el componente solicitado;
5. exige identidad y orden exactos entre la proyeccion tipada y las filas raw.

Los consumidores migrados son:

- `run_buildsystem_gate.py`;
- `run_packaging_gate.py`;
- `run_selfhost_basic_gate.py`;
- `run_selfhost_diagnostics_gate.py`;
- `run_selfhost_lexer_gate.py`;
- `run_selfhost_source_gate.py`;
- `run_selfhost_types_gate.py`.

Los validadores especificos permanecen porque prueban contratos del componente;
ya no tienen autoridad para relajar el esquema comun.

## Complejidad y costos

Por proceso de gate, la frontera realiza dos recorridos iterativos del
manifiesto de `N` filas: uno tipado y uno raw. El costo es `O(N)` en tiempo y
`O(N)` en memoria. No agrega recursion, procesos ni artefactos. La doble carga
es deliberada: conserva los objetos raw sin duplicar el parser canonico y
permite cotejar todas las identidades antes de ejecutar producto.

## Evidencia reproducible

Prueba causal:

```text
python -m unittest tests.compiler-suite.controller.tests.test_manifest_tag_boundaries tests.compiler-suite.controller.tests.test_component_gate_support
Ran 16 tests in 5.800s
OK
```

La prueba causal contiene 24 subcasos de mutacion: doce formas invalidas en
BuildSystem y las mismas doce en SelfHost.Diagnostics. Ninguna atraviesa la
frontera canonica.

Suite completa de controladores:

```text
python -m unittest discover -s tests/compiler-suite/controller/tests -p 'test_*.py'
Ran 248 tests in 16.527s
OK
```

Validacion del manifiesto vivo:

| Componente | Casos validados |
|---|---:|
| BuildSystem | 36 |
| Packaging | 12 |
| SelfHost.Basic | 27 |
| SelfHost.Diagnostics | 28 |
| SelfHost.Lexer | 53 |
| SelfHost.Source | 48 |
| SelfHost.Types | 36 |
| **Total** | **240** |

Una busqueda dirigida confirma que la unica llamada restante a
`read_jsonl(manifest)` para el manifiesto central vive dentro de
`component_manifest_rows`, despues de la validacion canonica.

## Limite de la afirmacion

Esta evidencia prueba el esquema, la identidad y el cableado de los siete
consumidores raw. No acredita compilacion, enlace ni ejecucion funcional del
producto. La meta permanece activa hasta que la matriz ejecutable completa
acordada termine con cero errores inesperados.
