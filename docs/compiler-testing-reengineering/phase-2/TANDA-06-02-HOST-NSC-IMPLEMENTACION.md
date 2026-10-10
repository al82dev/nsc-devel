# Tanda 06-02 — Host.nsc — materialización e integración

## SCOPE LOCK

- Objetivo autorizado: materializar exclusivamente el componente canónico `Host.nsc` de Ola 6.
- Naturaleza: infraestructura de pruebas y documentación; cero cambios funcionales al compilador.
- Revisión canónica de Fase 1: `3bb175443a18b2049c20d75443dd6743e2993903`.
- Fuente de producto: `src/nsc/main.cpp`, protegida y no modificada.
- Archivos permitidos: módulos nuevos, gate nuevo, integración mínima compartida, tracking, cursor y Markdown.
- Archivos protegidos: producto, históricos, remotos, `C:`, manifiesto ordinario y toda infraestructura central
  distinta de los cuatro anclajes mínimos autorizados.
- Acciones prohibidas: build, pruebas, gates, validación, despliegue, recursión y publicación de PASS sintético.
- Criterio de este corte: 160 IDs materializados, trazados y clasificados; sólo casos realmente ejecutables
  pueden entrar al manifiesto ordinario.

## Denominador canónico

La fuente normativa es `inventory/implementation-waves.csv`, no una suma inferida. Su fila de Ola 6 fija:

| Componente | Archivos | Contratos | Casos | Ejecutores previstos |
|---|---:|---:|---:|---|
| `Host.nsc` | 1 | 20 | 160 | `nsc_component_tests;nsc_suite_controller+nsc` |

Los inventarios `test-case-design.csv` y `suite-case-allocation.csv` contienen exactamente
`TF-NSC-001..160`. Cada contrato `CF-NSC-001..020` conserva ocho variantes. La distribución canónica es:

- 80 casos `unit` y 80 casos `process`;
- 140 casos `PR` y 20 casos `NIGHTLY`;
- 160 casos diseñados, 160 materializados y cero omitidos.

## Decisión de ejecutabilidad

Los contratos unitarios `CF-NSC-001..010` pertenecen a helpers o tipos privados de `src/nsc/main.cpp`. No hay
seam pública que permita invocarlos desde `nsc_component_tests`. Copiar sus algoritmos dentro de la suite
probaría un sustituto y produciría evidencia falsa.

Los contratos de proceso `CF-NSC-011..020` requieren un ejecutor general que controle `argv`, entorno, cwd,
filesystem, streams, Driver, procesos hijos y fallos. El ejecutor `nsc_suite_controller+nsc` vigente sólo
acepta cuatro operaciones de identidad de `Host.Basic`; no puede ejecutar estos contratos sin ampliar
infraestructura central protegida.

Por ello la clasificación exacta de este corte es:

| Estado | Contratos | Casos | Filas de manifiesto |
|---|---:|---:|---:|
| `InfrastructurePending` | 20 | 160 | 0 |
| `KnownProductDefect` | 0 | 0 | 0 |
| Ejecutable ordinario | 0 | 0 | 0 |

No existe evidencia de defecto de producto que autorice `KnownProductDefect`. La ausencia de infraestructura
no se presenta como PASS, SKIP, XFAIL ni oráculo vacío.

## Trazabilidad contrato-oráculo

| Contratos | Casos | Capa | Oráculo bloqueado | Infraestructura necesaria |
|---|---|---|---|---|
| `CF-NSC-001..010` | `TF-NSC-001..080` | `unit` | valores, bytes, estado, paths y formatos exactos | seam de helpers privados, entorno y fault injection |
| `CF-NSC-011..020` | `TF-NSC-081..160` | `process` | argumentos, diagnósticos, archivos, salida y exit code | ejecutor NSC general y fixtures de proceso controlados |

Cada caso conserva su `case_id`, `contract_id`, componente, capa, perfil y variante en el registro. El cuerpo
defensivo falla si alguien lo invoca directamente, mientras `TestDisposition::InfrastructurePending` impide
que el selector ordinario lo ejecute.

## Módulos consolidados

- `NscTestSupport.h`: genera IDs canónicos y registra pendientes sin éxito sintético.
- `NscUnit001To080Module.cpp`: materializa los diez contratos unitarios.
- `NscProcess081To160Module.cpp`: materializa los diez contratos de proceso.
- `run_host_nsc_gate.py`: comprueba el denominador, la asignación 80/80, el registro completo, la ausencia de
  filas ejecutables y que ningún caso pendiente sea seleccionable como requerido.

Los algoritmos de registro son iterativos, de tiempo `O(160)` y memoria adicional acotada por los metadatos
de los casos. No existe recursión.

## Integración compartida

La autorización posterior conectó, sin sobrescribir trabajo concurrente:

1. las dos declaraciones al registro;
2. las dos llamadas correspondientes al ensamblaje del host;
3. los dos módulos a las fuentes CMake;
4. el gate de clasificación a CTest;
5. los conteos exactos en `TRACKING.md` y `CURSOR.json`.

No se debe añadir ninguna fila `Host.nsc` a `manifest/cases.jsonl` mientras el total ejecutable sea cero.

## Validación

No se ejecutaron builds, pruebas, gates, análisis ni validación, por orden expresa de Fase 2. Estado exacto:

`MATERIALIZACIÓN E INTEGRACIÓN COMPLETAS; VALIDACIÓN PENDIENTE`.
