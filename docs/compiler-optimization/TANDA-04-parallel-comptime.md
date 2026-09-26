# Tanda 4: preprocesamiento `comptime` paralelo

## Objetivo

Usar los trabajos solicitados con `-j` durante el preprocesamiento de archivos fuente, que antes se ejecutaba de forma serial después de la lectura paralela.

## Cambios realizados

- Cada archivo ejecuta `ComptimeEngine` como trabajo del `CompilationWorkerPool` ya existente.
- Se mantiene un `ComptimeCache` compartido, cuya sincronización interna permite el acceso concurrente.
- La incorporación de buffers al `SourceManager` permanece serial y en orden para conservar identificadores y diagnósticos deterministas.
- Se añadió la fase observable `syntax-comptime` al progreso de compilación.

## Archivos modificados

- `src/Frontend/ProjectCompilation.cpp`

## Archivos nuevos

- Este informe. No se creó un subsistema adicional porque el pool de compilación existente cubre el trabajo.

## Mejora esperada

Los proyectos con muchos archivos ya no pagan el preprocesamiento de cada fuente uno después del otro. La fase escala hasta el límite configurado por `-j`, sin cambiar el orden visible de los resultados.

## Verificación de la tanda

- Construcción de `nsc` en configuración Debug/O0: correcta.
- Prueba dirigida `nsc.project-compilation`: correcta.
- Pruebas dirigidas `nsc.item385.comptime-core` y `nsc.item387.comptime-cache`: correctas.
- No se ejecutó baseline completo.
