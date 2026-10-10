# Tanda de Validación 38: selección explícita de herramientas

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

Al iniciar cualquier operación de construcción o configuración, la GUI llamaba
`with_discovery` sobre los valores visibles. Una ruta explícita que no existiera
podía ser sustituida por un NSC, NSLink o LLVM de fallback antes de validarse.
La operación dejaba entonces de corresponder a la selección del usuario.

## Corrección mínima

`_start_infrastructure` valida directamente la configuración visible. La
autodetección permanece disponible exclusivamente en `_redetect_config` y en la
carga inicial ya existente; no se alteró su heurística.

## Evidencia

- Reproducción previa: la prueba AST encontró `with_discovery` en el arranque.
- Arranque de operación después del cambio: sin autodetección.
- Botón de autodetección: conserva la llamada explícita.
- Pruebas dirigidas de rutas y selección: `5/5` PASS.
- Suite Python completa del controlador: `112/112` PASS.

No se abrió la GUI ni se ejecutó una operación real. No se configuró, compiló
o ejecutó producto; no hubo baseline, despliegue ni escritura en `C:`.
