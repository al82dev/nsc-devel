# T03-009: identidad de configuración de sesión

Se calcula una identidad SHA-256 reproducible de las opciones de la sesión para
distinguir configuraciones. La serialización conserva el orden de inclusiones y
macros y el contenido de entradas virtuales.

La identidad no incluye los contenidos de archivos físicos ni sus dependencias
transitivas; no es suficiente por sí sola como clave de caché.

La validación agrupada de la tanda 03 incluyó variaciones de campos, entrada y
orden. El código se probó junto con los demás cambios aún pendientes de commit;
este commit por sí solo no representa una construcción completa de la tanda.
