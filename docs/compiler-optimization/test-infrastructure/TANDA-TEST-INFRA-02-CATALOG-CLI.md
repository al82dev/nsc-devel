# Tanda 2 — Catálogo y selección del orquestador único

## Objetivo

Crear la frontera inicial del orquestador consolidado sin ejecutar procesos ni modificar la infraestructura CMake/CTest existente.

## Archivos creados

- `tests/harness/run_nsc_tests.py`
- `tests/harness/nsc_test/__init__.py`
- `tests/harness/nsc_test/model.py`
- `tests/harness/nsc_test/catalog.py`
- `tests/harness/tests/test_catalog.py`

## Clases y contratos

- `TestCase`: manifiesto inmutable validado.
- `ExpectedOutcome`: expectativas declaradas por el caso.
- `CaseKind`: tipos de ejecución permitidos.
- `TestCatalog`: descubrimiento iterativo, orden estable, filtros y detección de IDs duplicados.
- `ManifestError` y `CatalogError`: separación entre errores de contrato y errores de catálogo.

## Interfaz añadida

```text
python tests/harness/run_nsc_tests.py --nsc <archivo> --list
python tests/harness/run_nsc_tests.py --nsc <archivo> --case <id>
python tests/harness/run_nsc_tests.py --nsc <archivo> --component <nombre>
python tests/harness/run_nsc_tests.py --nsc <archivo> --all
```

En esta tanda la CLI imprime `execution=not-enabled-in-tanda-2`. Valida y congela la ruta de NSC, pero no inicia el compilador.

## Decisiones

- Los manifiestos son JSON para no añadir dependencias externas.
- Sólo `case.json` y `*.case.json` son manifiestos; los demás JSON pueden ser entradas o expectativas auxiliares.
- El recorrido de directorios usa una worklist explícita y no recursión.
- Las rutas de un caso no pueden escapar del directorio de su manifiesto.
- Durante la migración sólo se admite O0.
- Un catálogo inexistente representa un catálogo vacío; un ID o componente solicitado que no exista es error.

## Validación ejecutada

- catálogo vacío;
- manifiesto válido;
- ID duplicado;
- ruta que intenta escapar;
- selección CLI sin ejecutar el archivo entregado como NSC;
- archivo JSON auxiliar ignorado por el descubrimiento de manifiestos.

```text
python -m unittest discover -s tests\harness\tests -p test_*.py -v
Ran 6 tests in 0.139s
OK
```

La prueba CLI utiliza un archivo deliberadamente no ejecutable como `nsc.exe`; el éxito demuestra que esta tanda valida y congela la ruta, pero no inicia el compilador.

## Comportamiento preservado

No se modifican pruebas, compilador, CMake, CTest, `nscbuilder.py`, `_nscbuilder` ni rutas en `C:`.

## Pendiente deliberado

La ejecución de procesos, sandbox, timeout y reportes pertenece exclusivamente a la Tanda 3.
