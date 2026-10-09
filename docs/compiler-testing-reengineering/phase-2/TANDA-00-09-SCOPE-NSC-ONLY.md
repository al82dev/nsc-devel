# Tanda 00.09 - Alcance exclusivo de NSC

Fecha: 2026-10-09

## Corrección de alcance

La Fase 2 implementa únicamente la suite del compilador NSC. No registra ni
ejecuta casos de bibliotecas `Library.*`, SDK, tooling o dependencias externas.
Esos componentes deberán tener suites propias en sus repositorios si se decide
trabajarlos posteriormente.

## Cambios realizados

- Se retiró la antigua ola 7 de bibliotecas y SDK: 9 componentes y 606 casos.
- Se retiró la antigua ola 11 de disposición externa: 4 componentes y 0 casos.
- Self-host, bootstrap y paquete de NSC se renumeraron como olas 7, 8 y 9.
- El tracking, el scope lock, el plan de olas y la asignación CSV quedaron
  alineados con el mismo límite.
- Los inventarios y auditorías de Fase 1 se conservan como evidencia histórica;
  no forman parte de la implementación ni de los totales de Fase 2.

## Resultado verificable

| Magnitud | Resultado |
|---|---:|
| Olas activas de producto | 9 |
| Componentes asignados | 44 |
| Casos previstos | 9,587 |
| Componentes `Library.*` o SDK asignados | 0 |
| Componentes externos asignados | 0 |

Esta corrección no modifica código, manifiestos de casos implementados, pruebas
históricas, builds ni artefactos del compilador.
