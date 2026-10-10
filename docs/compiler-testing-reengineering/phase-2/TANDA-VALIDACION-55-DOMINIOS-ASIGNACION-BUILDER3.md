# Tanda de Validación 55: dominios de asignación de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

Las filas del compilador en `suite-case-allocation.csv` llegaban a la selección
sin validar sus dominios. Un perfil desconocido era descartado mediante el
fallback de rango y un modo u optimización desconocido no generaba variantes;
la omisión parecía una selección válida. También podían mezclarse centinelas
con valores ejecutables o repetirse tokens.

## Corrección mínima

`validate_allocation_domains` cierra exclusivamente los valores que Builder 3
consume: capa, ejecutor planeado, perfil, aislamiento, modos y optimizaciones.
Los valores `FROZEN_CONTRACT` y `N/A` son exclusivos y las listas rechazan
duplicados. La validación se aplica sólo después de filtrar los componentes del
compilador; el manifiesto conserva autoridad sobre las filas ejecutables.

## Evidencia

- Las `9587` asignaciones del compilador satisfacen los dominios vigentes.
- Cuatro dominios escalares desconocidos: rechazados por campo.
- Modo y optimización desconocidos: rechazados.
- Dos mezclas de centinela y dos listas duplicadas: rechazadas.
- Pruebas dirigidas: `4/4` PASS.
- Suite Python completa del controlador: `177/177` PASS.

No se modificó ningún CSV ni se exigió paridad entre asignación y manifiesto.
No se abrió la GUI y no se configuró, compiló o ejecutó producto. No hubo
baseline, despliegue ni escritura en `C:`.
