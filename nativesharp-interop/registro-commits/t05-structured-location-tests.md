# Tanda 05: pruebas de ubicación estructurada

Las pruebas comprueban la ruta, la línea y la columna de Clang como campos
separados, y verifican que `DiagnosticEngine::append` los conserve. Se requiere
repetir la validación agrupada después de estos cambios.
