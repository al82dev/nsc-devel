# Tanda Validacion 118: identidades exactas en catalogos self-host restantes

Fecha: 2026-10-10

## Scope lock

Esta tanda corrige exclusivamente siete conversiones de campos JSON en los
gates de `SelfHost.Project`, `SelfHost.Bootstrap.Infrastructure`,
`SelfHost.Contracts` y `SelfHost.Documentation`. No modifica catalogos,
manifiestos, inventarios, casos, reglas de disposicion, producto ni bateria
historica. No compila ni ejecuta `nsc`, no ejecuta baseline y no toca la unidad
`C:`.

## Defecto confirmado

Los cuatro gates convertian con `str(...)` las identidades de contradiccion,
las identidades y rutas del catalogo Contracts o los elementos de arreglos de
casos. Un numero o booleano podia perder su tipo JSON antes de que el gate
validara identidad, linaje, cobertura o contradiccion.

## Causa raiz

Los documentos ya se leian con un parser que rechaza claves duplicadas, pero
estos consumidores locales no reutilizaban los validadores textuales exactos
incorporados en validaciones anteriores. La conversion ocurria antes del
cotejo causal.

## Correccion

Los gates reutilizan ahora `required_string_field` y
`required_string_array` para exigir:

- `id` textual no vacio en las contradicciones Project, Bootstrap
  Infrastructure y Contracts;
- `contract_id` y `source_path` textuales no vacios en el catalogo Contracts;
- `case_ids` como arreglo no vacio de cadenas unicas en Contracts;
- `affected_case_ids` como arreglo no vacio de cadenas unicas en
  Documentation.

Las reglas existentes de componente, estado, disposicion, continuidad,
linaje, cobertura y existencia de productor permanecen sin cambios.

## Complejidad y costos

Cada campo escalar conserva costo `O(1)`. Cada arreglo mantiene costo `O(K)`
para `K` identidades y espacio `O(K)` para detectar duplicados. No se agrega
recursion, procesos ni artefactos.

## Evidencia reproducible

Prueba dirigida de las cuatro fronteras:

```text
python -m unittest tests.compiler-suite.controller.tests.test_selfhost_remaining_catalog_boundaries
Ran 6 tests in 6.383s
OK
```

La prueba ejecuta los cuatro gates reales contra sus documentos vivos y
rechaza diecinueve mutaciones de tipo, vacio o duplicado.

Pruebas de los validadores compartidos y fronteras relacionadas:

```text
python -m unittest tests.compiler-suite.controller.tests.test_component_gate_support tests.compiler-suite.controller.tests.test_selfhost_inventory_boundaries tests.compiler-suite.controller.tests.test_selfhost_remaining_catalog_boundaries
Ran 24 tests in 7.565s
OK
```

Suite Python completa:

```text
python -m unittest discover -s tests/compiler-suite/controller/tests -p 'test_*.py'
Ran 266 tests in 25.723s
OK
```

Documentos vivos auditados dinamicamente:

| Documento | Filas |
|---|---:|
| SelfHostProjectPlanContradictions.jsonl | 1 |
| SelfHostBootstrapInfrastructurePlanContradictions.jsonl | 1 |
| SelfHostContractsCatalog.jsonl | 48 |
| SelfHostContractsPlanContradictions.jsonl | 1 |
| SelfHostDocumentationPlanContradictions.jsonl | 1 |
| **Total** | **52** |

Una busqueda dirigida confirma cero conversiones restantes para los campos de
esta tanda en los cuatro gates. La auditoria global encontro conversiones en
otros flujos y las conserva como siguiente frontera causal; no se mezclaron en
este commit.

## Limite de la afirmacion

Esta evidencia prueba el esquema exacto de los cinco documentos vivos y la
conducta estatica de sus cuatro gates. No acredita ejecucion funcional del
producto. La meta permanece activa hasta que la matriz ejecutable completa
acordada termine con cero errores inesperados.
