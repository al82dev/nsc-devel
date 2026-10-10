# Tanda Validacion 115: cierre exhaustivo de consumidores raw

Fecha: 2026-10-10

## Rectificacion

Validacion 113 contabilizo siete consumidores raw del manifiesto central. Una
auditoria exhaustiva de todas las llamadas `read_jsonl(...)` encontro tres
fronteras adicionales:

- `run_host_source_file_ownership_gate.py`;
- `run_host_nsc_gate.py`;
- `validate_pending_manifest` en `selfhost_inventory.py`.

El denominador correcto es diez archivos consumidores, no siete. Esta tanda
rectifica el cableado y las metricas; no oculta ni reescribe la evidencia
anterior.

## Scope lock

La correccion se limita a sustituir esas tres lecturas por
`component_manifest_rows` y ampliar el guard de cableado. No modifica el
manifiesto, inventarios, casos, oraculos, producto ni bateria historica. No
compila ni ejecuta `nsc`, no ejecuta baseline y no toca la unidad `C:`.

## Causa raiz

La auditoria inicial busco las formas de llamada presentes en los siete gates
de BuildSystem, Packaging y self-host. Dos gates host usaban nombres locales
distintos (`manifest_path`) y el tercer consumidor estaba encapsulado en una
funcion generica; por eso no entraron al primer denominador.

## Correccion

Las tres fronteras validan ahora el documento completo con el parser canonico
antes de proyectar filas raw:

- `Host.Source` conserva sus 54 casos ejecutables despues del esquema global;
- `Host.nsc` demuestra cero filas ejecutables despues del esquema global;
- cada componente pendiente demuestra ausencia en el manifiesto sin abrir el
  JSONL por una ruta paralela.

El guard de cableado cubre ahora los diez archivos consumidores.

## Complejidad y costos

Cada invocacion conserva el costo `O(N)` de `component_manifest_rows` para un
manifiesto de `N` filas. No agrega recursion, procesos ni artefactos. La carga
completa es deliberada: una fila invalida en cualquier componente invalida el
documento comun antes de ejecutar un consumidor.

## Evidencia reproducible

Pruebas dirigidas:

```text
python -m unittest tests.compiler-suite.controller.tests.test_manifest_tag_boundaries tests.compiler-suite.controller.tests.test_component_gate_support tests.compiler-suite.controller.tests.test_live_manifest_closure
Ran 17 tests in 6.610s
OK
```

Suite completa:

```text
python -m unittest discover -s tests/compiler-suite/controller/tests -p 'test_*.py'
Ran 252 tests in 16.964s
OK
```

Filas vivas proyectadas:

| Componente | Filas |
|---|---:|
| BuildSystem | 36 |
| Host.nsc | 0 |
| Host.Source | 54 |
| Packaging | 12 |
| SelfHost.Basic | 27 |
| SelfHost.Diagnostics | 28 |
| SelfHost.Lexer | 53 |
| SelfHost.Source | 48 |
| SelfHost.Types | 36 |
| **Total** | **294** |

La busqueda literal de las tres formas directas conocidas encuentra una sola
llamada: la lectura interna de `component_manifest_rows`, despues del parser
canonico.

## Limite de la afirmacion

Esta evidencia prueba el cierre de las diez fronteras raw y rectifica el
denominador anterior. No acredita ejecucion funcional del producto. La meta
permanece activa hasta que la matriz ejecutable completa acordada termine con
cero errores inesperados.
