# Consumo binario aislado de Core con AddressSanitizer

El objetivo de esta comprobación es verificar el consumo de Core como biblioteca
binaria desde un programa con y sin instrumentación, no certificar todos los
componentes del SDK ni la ausencia general de errores de memoria.

## Procedencia

El checkout `A:/nsc-limitaciones-asan` estaba en la rama
`development-fixes-limitaciones-asan`, HEAD
`41f0a0cbb8683c89f21a504e5aa5d9e8676bdc8c`; la comparación de `src` y `lib`
contra ese HEAD estaba vacía. Había modificaciones concurrentes en fixtures y
documentación, descritas en `A:/nsc-asan-sdk-evidencia/status.txt`.

La configuración se creó desde la raíz del checkout, con temporales, caché y
salidas en `A:/nsc-asan-sdk-evidencia`. CMake identificó Clang y LLVM 23.1.0.
Se construyó el objetivo `nsc`; después se ejecutó el mismo objetivo de forma
incremental, con salida 0. La configuración, comandos y logs son
`configure.cmd`, `configure.log`, `build-nsc.cmd`, `build-nsc.log`,
`rebuild-nsc.cmd` y `rebuild-nsc.log` en esa carpeta. El objetivo de prueba
`nsc_project_oop_address_sanitizer_tests` se construyó por separado; ese build
no ejecutó la prueba.

El ejecutable de esta copia y la biblioteca de runtime generada tienen estos
SHA-256:

```text
nsc.exe                         49b8952fb6e0676b5ab35ca09b19df888e64a332ec3fe88cbcc04d81a868a47d
NativeSharp.Runtime.Win32.lib   3235a612c2dc62bb05c244885bc135ec76bf847f4f9a700badd9017c1f913a53
```

`build-core.cmd` usó ese nsc para emitir `lib/NativeSharp.Core/NativeSharp.Core.nsproj`
como biblioteca O0, sin caché de artefactos. El log identifica las 71 fuentes
del checkout y terminó con código 0. Se generaron, en el directorio SDK propio:

```text
NativeSharpCore.lib      565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6
NativeSharpCore.nsmeta   8705270aba9a632ff7b627ea9a4c37ed9d105c266e915a9552cd983ff2c6fe7f
```

Los hashes de las cuatro import libraries, Clang y el runtime ASan externo
figuran junto con los anteriores en el log de la prueba, antes y después de la
ejecución. El enlazador `nslink.exe` se invocó por nombre y se usaron las
rutas específicas autorizadas para los procesos del build aislado y Clang.
El mismo compilador comprobó la metadata con `--verify-library-metadata`;
el resultado fue válido y código 0. Comando y salida completos están en
`A:/nsc-asan-sdk-evidencia/verify-core.cmd` y `verify-core.log`.

## Control dirigido

`test-readline.cmd` ejecutó `tests/integration/asan_readline_growth_observe.py`.
El consumidor declara explícitamente la biblioteca y metadata recién
producidas; `NS_LIB_ROOT`, el runtime NativeSharp y sus import libraries
apuntan a la carpeta aislada. El programa comprueba una línea de 8192 bytes.
Los cuatro cruces de ASan activado/desactivado y O0/O2 compilaron con salida 0
y ejecutaron con salida 42. Los hashes de las entradas permanecieron iguales.
Comandos, configuración y salidas completas están en
`A:/nsc-asan-sdk-evidencia/test-readline.log`.

La opción `--sanitize-address` se admite actualmente para ejecutables de
proyecto, no para `--emit-project-oop-lib`; el Core de esta prueba **no está
instrumentado**. `ASAN_OPTIONS=detect_leaks=0` excluye comprobación general de
fugas. Este control no demuestra instrumentación de accesos dentro de Core,
que una realocación haya movido su bloque, ni detección de errores de memoria
en un control negativo; esos aspectos necesitan evidencia independiente.
