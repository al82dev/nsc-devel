# Tanda NSCBuilder: ventana de errores condensados

## Problema

La pestaña de errores conserva una fila por caso o etapa, pero obliga a abrir logs extensos para
encontrar el primer diagnóstico. Fallos derivados del cierre Gate 0 aparecen al mismo nivel que la
causa y varias fixtures afectadas por un único defecto repiten filas casi idénticas.

## Cambios

- Al terminar una corrida fallida, NSCBuilder abre una ventana con causas únicas.
- Cada log se reduce a su primer diagnóstico accionable de Clang/MSVC, NativeSharp o CMake.
- Fallos con la misma causa, canal y optimización se agrupan y enumeran los casos afectados.
- Los cierres `Cerrar validación...` y `Certificar baseline...` se omiten del resumen cuando existen
  causas anteriores; permanecen sin cambios en metadata, pestañas y logs completos.
- Doble clic abre el log original y el botón `Abrir resultados` conserva acceso a toda la evidencia.

## Archivos

- Modificado: `nscbuilder.py`.
- Modificado: `tests/unit/test_nscbuilder_pipeline.py`.
- Nuevo: este documento de trazabilidad.

## Mejora

Una corrida como la auditada muestra cinco causas en lugar de ocho filas y cientos de diagnósticos
en cascada, sin descartar evidencia ni alterar el resultado de certificación.

## Validación permitida

Sólo se ejecutan las pruebas Python dirigidas del pipeline y una comprobación de condensación sobre
los logs conservados de la corrida auditada. No se ejecuta baseline, CTest general ni matrices.
