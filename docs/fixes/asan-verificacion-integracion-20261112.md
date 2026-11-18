# Verificación dirigida de ASan en integración

Tras integrar selectivamente los commits del frente ASan, se ejecutó
`tests/integration/address_sanitizer_driver.py` desde esta copia, con
`A:/nsc-fixes-evidencia/build/bin/nsc.exe`. El lanzador y la salida están en
`A:/nsc-fixes-evidencia/asan-integrated-driver-check-20261112.py` y `.log`.
El compilador tenía SHA-256
`5cc75d23c719b89d5caad0eb62d3011bbb4fe4d2b7e5e11d44fe89ee5878e0cc` antes y
después; el registro también compara Clang, DLL ASan, runtime, importaciones
y runner. Cwd y directorio temporal efectivo estuvieron en A y no se modificó
el PATH. Se usó como referencia `C:/clang-llvm/bin/clang.exe` y su DLL ASan.

El runner terminó con código 0, sin omisiones, para O0/O2: programas seguros,
desbordamiento de pila detectado, publicación y reconstrucción de la DLL,
control sin instrumentación y rechazo de opciones o runtimes incompatibles.
Una primera comprobación sin las variables de Clang y DLL había devuelto 77
(omitida), no éxito; se conserva en `asan-integrated-prereq-check-20261112.log`.

La ejecución fijó `ASAN_OPTIONS=detect_leaks=0`: no demuestra ausencia de fugas.
El fixture tampoco acredita instrumentación del Core binario. La inferencia de
`SortWith` con Collections binario permanece abierta y está documentada por
separado en `asan-sortwith-binario-handoff.md`.
