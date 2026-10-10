# Tanda de Validación 43: artefactos host de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA SIN CAMBIO FUNCIONAL

## Frontera auditada

Se cotejaron las rutas que seleccionan el compilador y el host consolidado para
Debug y Release. Cada modo parte de un árbol distinto y `host_executable` sólo
recorre disposiciones CMake dentro del árbol solicitado.

Cuando no existe el host, la función conserva la ruta canónica faltante dentro
del modo pedido; `_run_host` la convierte posteriormente en error de preflight y
no busca un artefacto de otro modo.

## Evidencia

- Raíces Debug/Release de la suite: disjuntas.
- Artefactos sintéticos simultáneos: cada modo seleccionó el suyo.
- Host ausente: ruta esperada dentro de Debug, sin fallback.
- Productos NSC Debug/Release: disjuntos.
- Pruebas dirigidas: `4/4` PASS.
- Suite Python completa del controlador: `129/129` PASS.

Los archivos sintéticos se crearon sólo bajo `_nscbuilder3`; no se ejecutó
ningún binario, no se abrió la GUI y no se configuró, compiló o ejecutó producto.
No hubo baseline, despliegue ni escritura en `C:`.
