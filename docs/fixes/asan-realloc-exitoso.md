# Control dirigido de realloc exitoso con el adaptador ASan

La evidencia previa de fallo de reserva comprobaba que un `realloc` fallido
conservase el bloque original; la lectura larga de `Console.ReadLine` no
registraba si realmente llamaba a `realloc`. Este control ejercita directamente
el camino exitoso del adaptador generado para esta copia.

`tests/integration/asan_heap_realloc_preserve.py` recibe el compilador, Clang,
DLL ASan y directorio temporal como rutas explícitas. Por cada optimización
O0/O2 compila un pequeño programa NativeSharp con `--sanitize-address` y
`NSC_KEEP_LINK_TEMPS`, recoge el único objeto `sanitizer-heap.obj` producido
por ese enlace y lo enlaza con `asan_heap_out_of_bounds/realloc_preserve.ll`.
El fixture pide memoria en el heap del proceso con flags cero, escribe un patrón
de 64 bytes, amplía el bloque a 8192 bytes y verifica el patrón completo;
después reduce a 32 bytes, comprueba el prefijo y libera exactamente una vez.
Tanto el compilador como los procesos hijos se inician desde esta copia; los
archivos de prueba y salida se alojan en el temporal explícito de A. No se
modifica el runtime ni `CMakeLists.txt`.

El comando desde `A:/nsc-limitaciones-asan` fue
`call ..\nsc-asan-realloc-evidencia\run-realloc.cmd`; el script fija los
temporales, runtime e import libraries aislados y `ASAN_OPTIONS=detect_leaks=0`.
Las dos compilaciones del productor y los dos enlaces del fixture devolvieron 0;
los ejecutables O0 y O2 devolvieron 42. Cada enlace de Clang emitió el aviso
de reemplazo del triple del módulo. Comandos, cwd, salidas y hashes figuran en
`A:/nsc-asan-realloc-evidencia/realloc.log`.

Los SHA-256 antes y después del compilador
`49b8952fb6e0676b5ab35ca09b19df888e64a332ec3fe88cbcc04d81a868a47d`,
el runtime NativeSharp aislado
`3235a612c2dc62bb05c244885bc135ec76bf847f4f9a700badd9017c1f913a53`,
Clang, la DLL externa y el fixture permanecieron iguales. Los objetos
adaptadores generados en O0 y O2 tuvieron, respectivamente, los hashes
`132ce63bc42c8b7a344be9b3b29feecb4d23090c0a5e8d0fa36816a193531cf5` y
`c05ec2ee1aa3b380754eb03280775a7fcea3f5937b8ddef370fba107e27203f5`.
También se registraron y compararon las cuatro import libraries. Antes de
añadir esa comprobación de hashes, la primera ejecución había pasado; su log
se conservó en `A:/nsc-asan-realloc-evidencia/realloc-prior.log`.

La prueba demuestra ese recorrido de reserva y redimensionamiento correcto;
no demuestra que `realloc` haya movido el bloque, que todos los tamaños o
heaps sean admitidos, que el IR de apoyo esté instrumentado ni la ausencia de
fugas. Las rutas de error, referencias incompatibles y el caso original 97 se
probaron por separado en `asan-integrated-runtime-cwd.md`. La prueba se ejecuta
directamente y su eventual registro en CMake queda para el integrador.
