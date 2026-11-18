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

## Collections binario y ejemplos 99 y 100

Se copiaron las 25 fuentes de `NativeSharp.Core.Collections.Generic` del
checkout y los dos programas originales a
`A:/nsc-asan-sdk-evidencia/collections-case`. El manifiesto
`collections-case/manifest.json` registra los hashes de cada original y su
copia: todos coincidieron al copiarse. Los archivos originales no se
modificaron. Como el proyecto original de Collections usa una referencia de
proyecto a Core, se creó un proyecto de prueba distinto que conserva las
fuentes y referencia explícitamente la biblioteca y metadata Core aisladas.
Los proyectos de los dos consumidores también son nuevos y referencian ambos
binarios; sus `Program.ns` originales se conservaron sin cambios. El script
`prepare-collections.py` describe la creación; los proyectos y fuentes
generados permanecen en disco para repetir la comprobación.

`build-collections.cmd` emitió O0 con caché desactivada y código 0. Las salidas
fueron `collections-case/sdk/NativeSharpCoreCollectionsGeneric.lib` y su
metadata, con estos hashes:

```text
NativeSharpCoreCollectionsGeneric.lib      1d7dc04164ea3a7595aee5752a0f80df81f00455504012c0d7296ddbf754aad6
NativeSharpCoreCollectionsGeneric.nsmeta   76ac20d3d01504fce6b80cd10c2b5403f7857f1034bfc5ae8bec3e2ceaa401f1
```

`verify-collections.cmd` comprobó esa metadata con el compilador propio y
terminó con código 0; la salida completa está en `verify-collections.log`.

`test-collections.cmd` ejecutó los ejemplos originales con sus proyectos
adaptados, sin caché, en O0/O2 y con ASan activado/desactivado. El caso 99
compiló y ejecutó con salida 0 en las cuatro combinaciones; su texto original
dice `Unexpected success` porque era una reproducción histórica del fallo.
No demuestra por sí solo otras operaciones del diccionario. El caso 100 se
rechazó en las cuatro compilaciones antes de producir ejecutable: no se
infieren los argumentos de tipo de `SortWith` en la llamada original. Los
hashes del compilador, bibliotecas, metadata, runtime e import libraries no
cambiaron. Comandos y salidas íntegros:
`A:/nsc-asan-sdk-evidencia/test-collections-results.json`; resumen y wrapper:
`test-collections.log` y `test-collections.cmd`.

Para aislar el rechazo del caso 100, `sortwith-controls.cmd` preparó proyectos
separados, manteniendo Core binario y alternando únicamente Collections fuente
o binario. El programa original compiló y ejecutó con salida 0 en O0/O2 con
Collections fuente; con el binario falló el análisis de inferencia. Una copia
distinta que agrega explícitamente `<int>` a `SortWith` compiló y ejecutó con
salida 0 en O0/O2 tanto fuente como binario; esa copia es un control, no una
modificación ni una corrección del ejemplo original. El cuerpo del fixture
existente `tests/integration/sort_with_alias.py` pasó fuente con salida 42 en
O0/O2, pero se rechazó al consumir Collections binario por el mismo diagnóstico.
Resultados, proyectos, comandos y salida están en `sortwith-controls.json`,
`sortwith-controls.log` y los directorios `collections-case/control-*`.

`--dump-api-surface` mostró que la metadata sí contiene la plantilla
`SortWith<T>`, sus parámetros `Span<T>` y `Comparison<T>` y el cuerpo delegado
a `Sort<T>`; el volcado completo es `collections-api.log` y
`extract-sortwith-api.py` extrae su registro. La evidencia demuestra una
diferencia de inferencia al consumir el método genérico precompilado, pero no
identifica todavía el símbolo responsable ni demuestra el alcance de otros
genéricos. El caso 100 permanece **bloqueado por investigación del frontend
compartido**, asignado a coordinación con el integrador; no se modificó el
producto ni se presenta como corregido.

Un control adicional en copias separadas asigna `Spans.Of<int>(values)` a un
local antes de llamar a `SortWith` sin argumentos genéricos. Al declarar el
local `Span<int>`, la comprobación semántica pasa con Collections fuente y
falla con Collections binario; al declararlo
`NativeSharpCore::NativeSharp.Core.Span<int>`, pasa binario y falla fuente.
La forma `NativeSharp.Core.Span<int>` falla en ambos. Los seis resultados,
diagnósticos y comandos se conservan en `sortwith-local-explicit-of-controls.json`;
los proyectos están en `collections-case/control-local-explicit-of-*`.
Sin el argumento explícito de `Spans.Of`, algunas variantes fallaron antes en
esa inferencia y no aíslan la llamada a `SortWith` (`sortwith-local-controls.json`).
Esta comparación apunta a una diferencia de identidad o grafía de tipos, pero
no demuestra todavía la función exacta que la causa.

Core y Collections fueron emitidos sin ASan. Los consumidores activan ASan
solo en los cruces indicados y el entorno fija `detect_leaks=0`; por ello
estas pruebas no acreditan instrumentación interna de las bibliotecas ni
ausencia general de fugas.
