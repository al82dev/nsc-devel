# Caso 99: claves string equivalentes en Dictionary

## Alcance y procedencia

Revisión examinada: `a6f420e2187cb058192fb3201880047c00fba8f9`, rama
`development-fixes-limitaciones-laptop-01`, basada en
`71cd86d35220f654140e5619e9f9db0020f0b158`. Se trabajó en
`A:/nsc-fixes-laptop-01`, con build, SDK, temporales y evidencia exclusivos
`A:/nsc-fixes-laptop-01-build`, `A:/nsc-fixes-laptop-01-sdk`,
`A:/nsc-fixes-laptop-01-temp` y `A:/nsc-fixes-laptop-01-evidencia`.

La reproducción histórica original referida por `docs/fixes/dictionary-string-update.md`
y su copia referida por `docs/fixes/asan-sdk-binario-aislado.md` no se encuentran
en sus rutas documentadas en esta laptop. La regresión versionada
`tests/integration/dictionary_string_update/Main.ns` se conservó intacta; no
se afirma que sea idéntica al ejemplo histórico. La evidencia histórica atribuye
el fallo a una doble liberación corregida por `5127e4d07ef4ca9d82c6059cf9669a8f4eeb3789`,
pero ese diagnóstico histórico no sustituye la ejecución de esta revisión.
Una búsqueda en la historia local con `git log --all --oneline --name-only
-SUnexpected -- tests/integration docs/fixes` encontró la referencia en
`docs/fixes/asan-sdk-binario-aislado.md` y una prueba de punteros compartidos
distinta; no encontró el ejemplo original del diccionario en esos ámbitos.
Una segunda búsqueda con `git log --all --oneline --name-only
-SUnexpected.success --pickaxe-regex`, sin restringir rutas, terminó con código
0 y solo encontró la referencia documental de `asan-sdk-binario-aislado.md`.
El usuario autorizó continuar con la regresión conservada cuando el ejemplo
original no estuviera disponible; la equivalencia textual sigue sin verificarse.

## Entorno

El usuario autorizó la construcción inicial en el directorio vacío y la
invocación por ruta del compilador generado, sin cambiar PATH. Se configuró
CMake con Ninja, Release, clang-cl y LLVM 23.1.0 existentes; se construyó solo
el objetivo `nsc`. El ejecutable informó el build
`3.0.0+build.20261112T160438Z.245818df7aaf81e1`. La configuración inicial de
CTest señalaba la instalación de C: por defecto; no se usó para las pruebas y
se reconfiguró `NSC_TEST_COMPILER` hacia el ejecutable de este build. No se
desplegó ni se usó C:/native-sharp como compilador o SDK. Los procesos usaron
TEMP, TMP, APPDATA y LOCALAPPDATA dentro de la carpeta temporal exclusiva y
las pruebas deshabilitaron la caché de artefactos.

Core se generó con `--emit-project-oop-lib -O0 --no-artifact-cache` a partir
del proyecto de este repositorio. Collections se generó de la misma manera
desde una copia aislada de sus 25 archivos `.ns`, con un proyecto de prueba
que referencia Core mediante `LibraryReference`; `fc /b` no encontró
diferencias entre cada fuente y su copia. Ambos pares `.lib`/`.nsmeta` pasaron
`--verify-library-metadata` y `--verify-library-artifacts` con este compilador.
Los consumidores binarios referenciaron explícitamente ambos pares, sin
`ProjectReference`.

SHA-256 de los insumos observados después de las pruebas:

| Archivo | SHA-256 |
|---|---|
| `nsc.exe` | `5dae549fc1b1c30ec5efad859f1db1a80b85678a43d49ea6ad2bfaf7969e9e4e` |
| `NativeSharp.Runtime.Win32.lib` | `3235a612c2dc62bb05c244885bc135ec76bf847f4f9a700badd9017c1f913a53` |
| `NativeSharpCore.lib` | `565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6` |
| `NativeSharpCore.nsmeta` | `ceb5f240c3409e20491f562f2ddf727cc26c7725b7ec87278abdea58c5503c67` |
| `NativeSharpCoreCollectionsGeneric.lib` | `1d7dc04164ea3a7595aee5752a0f80df81f00455504012c0d7296ddbf754aad6` |
| `NativeSharpCoreCollectionsGeneric.nsmeta` | `9ee13ea0c3539161a93ff640926ad736a9074e185ca083d8161e011213bc80b1` |
| regresión conservada `Main.ns` | `df5ff702684cae0511e9ef4bbcef934500ab429f645084e93e594fd874d3ca29` |
| prueba nueva `Main.ns` | `e349a3d2e1add346d90948d2bf8361b2f61e19bfdcd7427f2aa9f440604bbbd5` |

## Resultados dirigidos

| Prueba | SDK fuente O0/O2 | SDK binario O0/O2 |
|---|---|---|
| Regresión conservada: inserción, actualización con otra copia igual y segunda clave | 0 / 0 | 0 / 0 |
| Valores con destructor, reemplazo, `Clear`, salida de alcance y 4096 actualizaciones | 0 / 0 | 0 / 0 |

Cada cifra es el código de salida del programa. La prueba nueva comprueba que
el valor reemplazado se destruye una vez, que los restantes se destruyen en
`Clear` o al terminar el alcance y que no se vuelven a destruir en el caso
comprobado. La prueba registrada `nsc.sdk.dictionary-string-lifetime` pasó
1/1 con CTest; ejecuta solo esa prueba y sus dos niveles de optimización.
Las salidas están bajo las carpetas `case99-source-existing`,
`case99-binary-existing`, `case99-source-lifetime-final`,
`case99-binary-lifetime-final` y `case99-binary-script` de la raíz de evidencia.

Para repetir la prueba fuente desde el build configurado con el compilador
propio: `ctest --test-dir A:/nsc-fixes-laptop-01-build -R dictionary-string-lifetime --output-on-failure`.
Para la variante binaria, `cmake/VerifyDictionaryStringBinary.cmake` requiere
`SOURCE_FILE`, `CORE_LIBRARY`, `CORE_METADATA`, `COLLECTIONS_LIBRARY`,
`COLLECTIONS_METADATA`, `NSC_EXECUTABLE`, `RUNTIME_LIBRARY`,
`NSLINK_EXECUTABLE` y `OUTPUT_DIR`; copia la fuente a un consumidor aislado y
ejecuta el verificador existente a O0 y O2. Deben pasarse los artefactos del
SDK propio y un directorio de salida exclusivo en A:.

Comandos principales de esta ejecución, desde CMD y con las variables de
aislamiento indicadas arriba (`NS_LIB_ROOT` apuntó al SDK propio,
`NSC_WINDOWS_RUNTIME_LIBRARY` al runtime del build y `NSC_LINKER=nslink.exe`):

```bat
cmake -S A:/nsc-fixes-laptop-01 -B A:/nsc-fixes-laptop-01-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl.exe -DCMAKE_CXX_COMPILER=clang-cl.exe -DCMAKE_LINKER=lld-link.exe -DCMAKE_RC_COMPILER=llvm-rc.exe -DCMAKE_MT=llvm-mt.exe -DLLVM_DIR=C:/clang-llvm/lib/cmake/llvm
cmake --build A:/nsc-fixes-laptop-01-build --target nsc --parallel 4
A:\nsc-fixes-laptop-01-build\bin\nsc.exe --emit-project-oop-lib A:\nsc-fixes-laptop-01\lib\NativeSharp.Core\NativeSharp.Core.nsproj -O0 -o A:\nsc-fixes-laptop-01-sdk\NativeSharpCore.lib --no-artifact-cache --no-error-log
A:\nsc-fixes-laptop-01-build\bin\nsc.exe --emit-project-oop-lib A:\nsc-fixes-laptop-01-temp\case99-collections\Case99BinaryCollections.nsproj -O0 -o A:\nsc-fixes-laptop-01-sdk\NativeSharpCoreCollectionsGeneric.lib --no-artifact-cache --no-error-log
cmake -S A:/nsc-fixes-laptop-01 -B A:/nsc-fixes-laptop-01-build -DNSC_TEST_COMPILER=A:/nsc-fixes-laptop-01-build/bin/nsc.exe
ctest --test-dir A:/nsc-fixes-laptop-01-build -R dictionary-string-lifetime --output-on-failure
cmake -DNSC_EXECUTABLE=A:/nsc-fixes-laptop-01-build/bin/nsc.exe -DRUNTIME_LIBRARY=A:/nsc-fixes-laptop-01-build/generated/NativeSharp.Runtime.Win32.lib -DNSLINK_EXECUTABLE=nslink.exe -DSOURCE_FILE=A:/nsc-fixes-laptop-01/tests/integration/dictionary_string_lifetime/Main.ns -DCORE_LIBRARY=A:/nsc-fixes-laptop-01-sdk/NativeSharpCore.lib -DCORE_METADATA=A:/nsc-fixes-laptop-01-sdk/NativeSharpCore.nsmeta -DCOLLECTIONS_LIBRARY=A:/nsc-fixes-laptop-01-sdk/NativeSharpCoreCollectionsGeneric.lib -DCOLLECTIONS_METADATA=A:/nsc-fixes-laptop-01-sdk/NativeSharpCoreCollectionsGeneric.nsmeta -DOUTPUT_DIR=A:/nsc-fixes-laptop-01-evidencia/case99-binary-script -P A:/nsc-fixes-laptop-01/cmake/VerifyDictionaryStringBinary.cmake
```

El proyecto temporal de Collections se conserva en la ruta mostrada y usa
`Compile Include="**/*.ns"` y una `LibraryReference` explícita al par de
Core del SDK propio. Es una adaptación del proyecto de prueba, no un cambio
del proyecto original ni de las fuentes de Collections.

## Límites y decisión

No se reprodujo un defecto en la regresión conservada ni en los casos nuevos;
no se modificó `Dictionary.ns` ni ownership, punteros compartidos o metadata.
Las 4096 actualizaciones y el fin de alcance ejercen limpieza de claves, pero
no demuestran ausencia de fugas ni una liberación exactamente una vez de cada
`string`. Inicialmente, `--asan-clang=clang.exe` fue rechazado con código 2;
después el usuario autorizó la ruta completa de Clang para esta prueba.

Con `--sanitize-address`, el Clang existente en `C:/clang-llvm/bin/clang.exe`,
su runtime `C:/clang-llvm/lib/clang/23/lib/windows/clang_rt.asan_dynamic-x86_64.dll`
y `ASAN_OPTIONS=detect_leaks=0`, se compilaron y ejecutaron tanto la regresión
conservada como la prueba de vida útil: las ocho combinaciones de SDK fuente o
binario y O0 u O2 terminaron con código 0, sin diagnósticos de ASan. El control
negativo existente `tests/integration/asan_heap_out_of_bounds`, copiado a la
carpeta temporal exclusiva, detectó `heap-buffer-overflow` y terminó con código
1 en O0 y O2, como se esperaba. Las salidas y los logs de compilación están en
las carpetas `case99-source-existing`, `case99-binary-existing`,
`case99-source-lifetime` y `case99-binary-lifetime` de la raíz de evidencia;
los logs del control son `asan-control-O0.log` y `asan-control-O2.log` en
`case99-source-lifetime`. Los cuatro hashes del SDK binario permanecieron
iguales después de las ejecuciones y la copia binaria de la prueba de vida útil
coincidió byte por byte con el fixture fuente.

ASan instrumenta los ejecutables de proyecto, pero Core y Collections binarios
se produjeron sin instrumentación; por tanto la variante binaria no demuestra
la cobertura interna de esas bibliotecas. Además, una ejecución dirigida con
`ASAN_OPTIONS=detect_leaks=1` terminó con código 1 y el diagnóstico
`detect_leaks is not supported on this platform`: no se verificó la ausencia
de fugas. El control negativo acredita detección de un acceso fuera de límites,
no una prueba negativa específica de doble liberación.
Como comprobación adicional del mecanismo integrado, `nsc explain
debug-allocator` devolvió `status=missing` e indicó ausencia de asignador con
guardas y de detección en runtime de fugas, uso tras liberación y doble drop.
En esta sesión tampoco se encontraron `drmemory.exe`, `valgrind.exe` ni
`gflags.exe` en PATH; no se alteró la configuración global de Windows.

Permanecen sin verificar el ejemplo histórico exacto y la ausencia de fugas o
la liberación exacta de cada clave. No se ejecutó la batería
completa. `CMakeLists.txt` es un archivo compartido: su único cambio registra
la prueba fuente nueva y puede requerir resolución de conflictos al integrar.
