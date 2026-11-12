# Ejecutores dirigidos: aislamiento pendiente de validar

Base de preparación: e6388ffdf92599de0f00bb9453c96252a573dca3.
Los cambios de ejecutores no se han compilado ni ejecutado.

Los ejecutores SIMD, de fábricas y de async incluidos reciben rutas absolutas
en A y una carpeta de evidencia nueva fuera del repositorio. Los procesos
usan el repositorio como directorio de trabajo y reciben TEMP y TMP en la
carpeta aislada; no se modifica PATH. La ruta del compilador se pasa
explícitamente. Los proyectos de fábricas reciben Core y metadata explícitos;
el proyecto SIMD también los recibe tras esta propuesta.

Los tres controles async construyen una clase Exception específica dentro de
sus propias fuentes; no se les añade Core binario sin demostrar que puedan
convivir con ese fixture. La prueba de excepción extranjera requiere además
runtime e import libraries explícitos en A, y sigue invocando clang y lld-link
por nombre. La procedencia efectiva de los binarios auxiliares que esas
herramientas o el compilador utilicen sigue pendiente de verificar en ejecución.

Argumentos después del nombre de cada script:

- SIMD: `nsc Core.lib Core.nsmeta evidencia [--check-only]`.
- Fábrica con base importada y productor directo binario:
  `nsc Core.lib Core.nsmeta evidencia`.
- Fábrica con delegación:
  `nsc Core.lib Core.nsmeta evidencia [--compile|--execute]`.
- Llamada async y consumidor externo: `nsc evidencia`.
- Excepción extranjera: `nsc runtime imports evidencia`.

Las rutas deben ser absolutas en A. Los tres controles async conservan su
comportamiento previo de eliminar sus carpetas temporales internas; su salida
se debe capturar en un log en A al ejecutarlos. Añadir la biblioteca Core al
proyecto SIMD cambia su configuración y necesita validación dirigida. La
revisión de argumentos, asignación de entorno y salida de cada runner también
queda pendiente de ejecución; esta nota no constituye resultado de pruebas.
