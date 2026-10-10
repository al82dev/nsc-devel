# Tanda de Validación 33: precondiciones de rutas de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON EVIDENCIA ESTÁTICA

## Riesgo cubierto

Una fila podía tener ejecutor válido pero carecer del proyecto, operación,
ordinal o variante que su controlador consume, y descubrirlo sólo después de
iniciar la corrida offline.

## Prueba permanente

`test_nscbuilder3_route_prerequisites.py` revisa todas las rutas especializadas:

1. cada caso self-host posee un `.nsproj` existente y contenido dentro de la
   suite, argumento no vacío, sandbox reconocido y código de salida entero;
2. las filas de identidad usan sólo las cuatro operaciones implementadas por
   `CompilerIdentityExecutor`;
3. cada fila BuildSystem entra en exactamente una de sus seis ramas activas;
4. cada fila Packaging entra en exactamente una de sus dos ramas activas.

Los gates se importan realmente, pero sus operaciones externas se sustituyen
por dobles. Así se prueba el despacho y el consumo de campos sin ejecutar una
herramienta.

## Evidencia

- Casos self-host cotejados: `192`.
- Proyectos self-host ausentes o externos: `0`.
- Operaciones de identidad sin implementación: `0`.
- Filas BuildSystem cotejadas: `36`.
- Filas Packaging cotejadas: `12`.
- Filas sin rama o con más de una rama: `0`.
- Pruebas dirigidas: `4/4` PASS.
- Suite Python completa del controlador: `95/95` PASS.

No se ejecutaron CMake, NSC, NSLink, archivadores, fixtures ni procesos. No se
modificó Builder 3 porque no se demostraron discrepancias. No hubo baseline,
despliegue ni escritura en `C:`.
