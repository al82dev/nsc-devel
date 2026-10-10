# Tanda 02.06 — materialización de infraestructura pendiente de Host.AST

Fecha: 2026-10-09

## Propósito y alcance

Materializar los seis casos canónicos de `Host.AST` que ya estaban diseñados,
inventariados y clasificados como dependientes de infraestructura especial. El
cambio sólo agrega su catálogo y enlace al ejecutor consolidado; no implementa
la infraestructura externa, no modifica producto y no convierte ningún caso en
`PASS`, `SKIP`, `XFAIL` o defecto de producto.

## Casos materializados

| Caso | Contrato | Capa | Perfil | Infraestructura requerida |
|---|---|---|---|---|
| `TA-AST-014` | `CA-AST-002` | `unit` | `NIGHTLY` | inyección determinista de fallo de asignación o registro |
| `TA-AST-016` | `CA-AST-002` | `unit` | `NIGHTLY` | subprocess aislado para death test |
| `TA-AST-017` | `CA-AST-002` | `unit` | `NIGHTLY` | target instrumentado con ThreadSanitizer |
| `TA-AST-215` | `CA-AST-019` | `component` | `NIGHTLY` | subprocess aislado para death test |
| `TA-AST-223` | `CA-AST-020` | `robustness` | `NIGHTLY` | subprocess con watchdog determinista |
| `TA-AST-224` | `CA-AST-020` | `robustness` | `NIGHTLY` | subprocess con watchdog determinista |

`AstInfrastructureTestSupport.h` conserva para cada caso el identificador, el
contrato, la capa, el perfil, el disparador, el oráculo y el requisito externo.
`AstInfrastructurePendingModule.cpp` materializa exactamente esas seis filas.

## Integración y frontera de ejecución

El nuevo módulo está declarado en `TestRegistry.h`, se registra desde
`host/src/main.cpp` y forma parte del target consolidado mediante
`tests/compiler-suite/CMakeLists.txt`. Los casos permanecen deliberadamente
fuera de `manifest/cases.jsonl`: el gate ordinario de `Host.AST` sigue
ejecutando los `224` casos que cuentan con infraestructura real y ya fueron
validados.

La materialización no equivale a verificación. El resultado actual es:

- `230/230` casos materializados e integrados;
- `224/230` casos ejecutables y previamente validados;
- `6/230` casos `InfrastructurePending` no ejecutados;
- cero cambios al producto y a la batería histórica.

## Validación

No se ejecutaron compilación, pruebas ni gates por la separación vigente de
Fase 2. La validación de estos seis casos sólo podrá comenzar cuando exista la
infraestructura descrita en la tabla y deberá conservar los oráculos canónicos,
sin sustituirlos por proxies.
