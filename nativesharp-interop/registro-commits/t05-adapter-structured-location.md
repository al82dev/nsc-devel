# Tanda 05: Clang a ubicación estructurada

El adaptador conserva archivo, línea y columna de Clang como datos separados en
el diagnóstico NativeSharp, además de mantener el texto visible en consola
cuando no existe un rango nativo. No interpreta la columna de Clang como UTF-16.
Pendiente de nueva validación agrupada.
