# SEM-06.03 — Equivalencia determinista por número de jobs

## Propósito

Demostrar en el camino productivo que `-j1`, `-j2` y `-j12` cambian únicamente la concurrencia,
nunca el resultado semántico.

## Fixture

La prueba dirigida construye un proyecto con 24 callables independientes y una plantilla genérica
demandada desde esos cuerpos. El caso fuerza al menos dos oleadas: cuerpos iniciales y
especialización descubierta.

Para los tres presupuestos compara:

- éxito y error de ejecución;
- secuencia exacta de diagnósticos estructurados;
- volcado completo de method bodies;
- cardinalidad de callables analizados, shards creados y resultados fusionados;
- existencia de más de una oleada.

La prueba no congela hashes, textos de herramientas, número global de tests ni tiempos de pared.

## Evidencia dirigida

Se construye y ejecuta exclusivamente `nsc_project_method_body_model_tests`, junto con los tests
puntuales del scheduler/reductor y escape afectados. Este fixture llama directamente a la fase
semántica, que no recibe un nivel de optimización; la equivalencia explícita de Driver en O0 se
verifica por separado. No se ejecuta baseline, minibaseline, CTest general ni O1/O2/O3.
