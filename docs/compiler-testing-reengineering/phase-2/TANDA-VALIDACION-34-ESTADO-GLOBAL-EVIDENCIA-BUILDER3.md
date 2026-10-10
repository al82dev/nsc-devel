# Tanda de Validación 34: estado global cerrado de la evidencia

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defectos demostrados

`EvidenceStore._overall_status` devolvía `PASSED` en dos entradas que no
demuestran éxito:

1. un conjunto sin resultados;
2. un conjunto que contenía un estado ajeno al protocolo.

La segunda condición podía ocultar una incompatibilidad entre un controlador y
el reporte final.

## Corrección mínima

El cálculo exige ahora al menos un resultado y valida las claves del contador
contra el dominio cerrado `PASS`, `KNOWN_FAILURE`, `FAIL`, `UNEXPECTED_PASS`,
`BLOCKED`, `INFRASTRUCTURE_ERROR` y `CANCELLED`. La publicación cancelada ejecuta
la misma validación antes de aplicar su estado global `CANCELLED`.

## Evidencia

- Estados válidos del protocolo: `7`.
- Vacío antes del cambio: `PASSED` incorrecto.
- Estado desconocido antes del cambio: `PASSED` incorrecto.
- Ambos casos después del cambio: rechazo explícito.
- Variantes no iniciadas al cancelar: todas materializadas como `CANCELLED`.
- Pruebas dirigidas: `4/4` PASS.
- Suite Python completa del controlador: `99/99` PASS.

No se configuró, compiló ni ejecutó producto; no hubo baseline, despliegue ni
escritura en `C:`.
