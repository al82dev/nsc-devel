# Cierre de la Ola 0 — Infraestructura mínima

## Alcance cerrado

La Ola 0 implementa únicamente la infraestructura común de la suite nueva:

- manifiesto JSONL estricto respaldado por inventarios;
- selector determinista con cierre iterativo de dependencias;
- estados causales y `BLOCKED` por productor;
- workspaces nuevos por caso, restringidos a `A:`;
- comparación estructural de oráculos mediante worklist explícita;
- eventos JSONL progresivos y cancelación;
- reportes finales completos y no sobrescribibles;
- un ejecutable C++ consolidado con registro por ID;
- un coordinador iterativo de casos;
- un adaptador de proceso que valida el protocolo del ejecutable host;
- un gate integral con tres fixtures sintéticos.

No se implementó ni ejecutó ningún caso de producto. No se ejecutaron NSC, NSLink, la batería histórica, un
baseline, cobertura, despliegue ni operaciones sobre `C:`.

## Evidencia reproducida

| Comprobación | Debug | Release |
|---|---:|---:|
| Configuración aislada | PASS | PASS |
| Construcción de `nsc_component_tests` | PASS | PASS |
| `nsc.compiler-suite.infrastructure` | PASS | PASS |
| `nsc.compiler-suite.controller-infrastructure` | PASS | PASS |
| Importación dinámica de VCRUNTIME/MSVCP/UCRT | 0 | 0 |

Las pruebas Python del controlador descubrieron y aprobaron 46 casos. El gate integral descubrió dinámicamente
los tres casos sintéticos del manifiesto y produjo tres `PASS`, ocho eventos progresivos y los cuatro artefactos
de corrida. Los conteos anteriores describen esta revisión; no están codificados como criterio rígido del runner.

## Integridad

- Los seis archivos preexistentes protegidos conservaron sus SHA-256 registrados en el `SCOPE LOCK` operativo.
- La batería histórica no fue modificada por los commits de la Ola 0.
- Los resultados sintéticos demuestran el framework, no el comportamiento de NSC.
- No existen `SKIP`, oráculos vacíos ni fallos pendientes dentro de los tres fixtures de infraestructura.

## Gate de salida

`LISTA`. La siguiente acción determinista es materializar `Host.Basic`, primer componente de la Ola 1, desde sus
16 contratos y 88 casos canónicos. La Ola 1 permanece `EN CURSO` únicamente después de iniciar esa materialización;
sus otros cuatro componentes continúan pendientes.
