# Integración del ajuste de ruta de ASan

Se aplicó el commit de la laptop `88511eac2284642013b294820ca1afb451a80632`:
la ruta del runtime devuelta por Clang se construye desde los mismos bytes
UTF-8 mediante `std::u8string`, sin usar `std::filesystem::u8path` deprecado.
La causa de la compilación fallida y su evidencia en la laptop están en
`linker-ruta-utf8-build-laptop.md`. El build de esta copia tiene
`NSC_WARNINGS_AS_ERRORS=OFF`, de modo que no se presenta el fallo con warnings
como reproducido aquí.

`A:/nsc-fixes-evidencia/integration-asan-path-build.cmd` compiló
incrementalmente `nsc` con código 0. El test
`integration-asan-path-test.cmd` empleó el compilador aislado, Clang externo
`C:/clang-llvm/bin/clang.exe` y su runtime como entradas. En O0/O2 aceptó una
copia idéntica del runtime y ejecutó 42; rechazó una copia alterada con código 2
y el diagnóstico de runtime incompatible. Comandos y salidas están en el log
homónimo. El resultado verifica esa ruta y validación de contenido, no una
ejecución con ruta no ASCII devuelta por Clang ni todos los errores de memoria.

Tras el enlace pasaron las cinco pruebas unitarias dirigidas de `#run`, los dos
controles del cargador de proyectos y la coherencia del prototipo de Arc. El
build contiene cambios locales sin registrar de otras áreas; no acredita un
árbol limpio. No se modificaron Clang, el runtime externo ni la instalación
funcional; no se ejecutó la batería completa.
