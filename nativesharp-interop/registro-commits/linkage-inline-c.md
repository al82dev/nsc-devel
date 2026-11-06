# Linkage de static inline en Clang

Clang asigna `NoLanguageLinkage` a funciones con enlace interno, incluso al analizar C. La detección de `static inline` usa el modo real de la sesión y no interpreta ese valor como si el código no fuera C.
