# Cobertura del validador de la tanda 08

El inventario de `validar*.cmd` mostró que el validador principal solo
invocaba siete fixtures y la matriz general de metadata. Quedaban fuera
validadores de punteros opacos, callbacks precompilados, typedefs, bitfields,
TLS, flotantes, prototipos, inline y sombras.

Se preparó el validador principal para invocar explícitamente las rutas del
inventario, después de sus preparaciones cuando corresponda. Acumula los
fallos y no convierte los rechazos en éxitos: los casos de enum mixto y TLS
conocidos pueden hacer que el gate falle. Además agrupa los targets de la
configuración ON en una sola construcción incremental. La configuración OFF
se conserva por separado.

El script modificado aún no se ha ejecutado como gate integral. Su presencia
no demuestra que pasen los casos, ni que el inventario de características C/C++
esté completo; las limitaciones registradas siguen abiertas.
