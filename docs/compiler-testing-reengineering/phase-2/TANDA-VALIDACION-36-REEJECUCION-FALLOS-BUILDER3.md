# Tanda de Validación 36: reejecución determinista de fallos

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defectos demostrados por auditoría

La GUI convertía las claves terminales a un `set` y reconstruía el retry desde
ese conjunto. El orden quedaba sujeto a iteración de hash y una clave terminal
ajena al plan se omitía silenciosamente.

## Corrección mínima

`retry_variants` realiza una proyección pura:

1. valida unicidad del plan visible;
2. valida unicidad de resultados;
3. rechaza claves terminales fuera del plan;
4. recorre el plan original y conserva sólo sus fallos terminales.

`_suite_finished` usa esa proyección sin cambiar los estados ni la operación de
reejecución.

## Evidencia

- Orden de entrada de resultados invertido: retry conserva orden del plan.
- Estados incluidos: `5` terminales.
- Estados excluidos: `PASS`, `KNOWN_FAILURE`.
- Clave terminal ajena: rechazo explícito.
- Resultado duplicado: rechazo explícito.
- Variante visible duplicada: rechazo explícito.
- Pruebas dirigidas: `4/4` PASS.
- Suite Python completa del controlador: `107/107` PASS.

No se abrió la GUI ni se ejecutó producto. No hubo baseline, despliegue ni
escritura en `C:`.
