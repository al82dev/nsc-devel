# 102 -- El build roto por una cuenta a mano

Fecha: 2026-09-02
Ambito: solo el compilador (`A:\nsc-devel`).
Fichero tocado: `src/Linker/WindowsRuntimeImports.cpp`.

## El veredicto de la corrida

`all.bat` termino, y las dos pasadas dicen exactamente lo mismo:

```
1. VEREDICTO
Passed: 0   Failed: 0
BUILD:  ROTO -- nada de lo de abajo mide el arbol nuevo.

FALLOS (1):
  [FAIL] compiler build/install -- ver 01_build.log
```

```
FAILED: [code=1] CMakeFiles/nsc_linker.dir/src/Linker/WindowsRuntimeImports.cpp.obj
A:\nsc-devel\src\Linker\WindowsRuntimeImports.cpp(160,9): error: excess elements in struct initializer
1 error generated.
ninja: build stopped: cannot make progress due to previous errors.
```

Un solo error, en un solo fichero, en las dos pasadas. Ni ctest ni los trabajos llegaron a
correr (`02_ctest.log no existe`), asi que de esta corrida **no hay ninguna medicion del Bloque
4**: no se ha probado nada, solo se ha fallado antes de probar.

El resto del arbol si compilo: 360 de 419 objetos antes de que ninja parara, y entre ellos estan
los cinco ficheros grandes del Bloque 4 --`WindowsRuntimeBuilder.cpp` [137/419],
`ProjectOopMirModel.cpp` [141], `ProjectOopHirModel.cpp` [149], `ProjectMethodBodyModel.cpp`
[159], `ProjectOopLlvmLowerer.cpp` [163]-- y `Cc1CorrectnessTests.cpp` [227]. Ninguno tiene ni
un aviso. El fallo es de una linea, y no es de las que escribi con cuidado.

## QUE estaba mal

La tabla de imports de KERNEL32 se declaraba con el tamano escrito a mano:

```cpp
const std::array<std::string_view, 62> kernel32Symbols = {
    "ExitProcess",
    ...
    "MoveFileA"};
```

El Bloque 4 metio tres entradas nuevas en esa lista --`GetCurrentDirectoryA` (fase A),
`CreateProcessA` y `GetExitCodeProcess` (fase C)-- y la `62` se quedo como estaba. Hay 65
elementos en el inicializador y sitio para 62. Eso es el error entero.

## POR QUE no lo vi antes, que es la parte que importa

Dos motivos, y los dos son estructurales, no un despiste.

**Primero: el error apunta a una linea que no toque.** `clang` senala `(160,9)`, que es
`"Sleep",` -- el elemento numero 63, el primero que ya no cabe. Los tres que meti estan en las
lineas 87, 90 y 91. El diagnostico apunta al primer sobrante, no al causante, asi que leer la
linea del error no lleva a la linea del bug: lleva a una entrada que lleva meses ahi y que no
tiene nada de malo. Es un error que se lee al reves.

**Segundo: el banco de pruebas del contenedor no compila `src/Linker`.** El harness con el que
verifico aqui (los 74 `.cpp` del frontend contra `libLLVM-18`) llega hasta el IR y para. Todo
`src/Linker` y todo `src/Runtime` quedan fuera. Cuando dije "169 OK / 5 FAIL, y los 5 son
negativos intencionados", esa medicion era cierta **y no decia absolutamente nada sobre este
fichero**, porque este fichero no entra en ella. Es el mismo agujero por el que se colo el
`undefined symbol: GetCurrentDirectoryA` del 2026-09-01 (86 de 86 trabajos con "build failed",
documentado en el comentario de la propia tabla): las dos veces el fallo estuvo en la lista de
imports, y las dos veces el contenedor dio verde.

La consecuencia practica, escrita para que no se me olvide: **un cambio en `src/Linker` o en
`src/Runtime` no esta verificado hasta que corre `all.bat` en Windows.** El contenedor no puede
opinar sobre ellos.

## COMO queda arreglado

No subiendo el `62` a `65`. Eso arregla hoy y deja la trampa puesta para la siguiente alta:

```cpp
const auto kernel32Symbols = std::to_array<std::string_view>({
    "ExitProcess",
    ...
    "MoveFileA"});
```

`std::to_array` deduce el tamano del inicializador, asi que la cuenta la lleva el compilador y
esta clase de error **ya no se puede escribir**. Aplicado a las tres tablas del fichero
(`kernel32Symbols`, `kernelbaseSymbols`, `shell32Symbols`) por la misma razon, aunque las dos
ultimas tengan un elemento: la de un elemento de hoy es la de cuatro de dentro de dos meses.

`std::to_array` es C++20 y el proyecto compila en C++20 (`target_compile_features(nsc_linker
PUBLIC cxx_std_20)`), asi que no hace falta tocar nada mas. El consumidor de las tres tablas es
la lambda generica `addSymbols(std::string_view dll, auto&& symbolRange)`, que solo hace un
`for` sobre el rango: no hay ningun sitio que dependiera del tipo exacto ni del tamano.

El comentario que explica todo esto queda dentro del fichero, encima de la tabla, junto al que
ya explicaba por que esta lista es el decimo sitio que hay que tocar.

## Verificacion

`src/Linker` no compila en el contenedor, asi que extraje las tres declaraciones **tal cual
quedan en el fichero** a una unidad de traduccion suelta y la compile con
`clang++-18 -std=c++20 -Wall -Wextra`. Compila sin avisos, y el programa comprueba tres cosas:

```
kernel32=65 kernelbase=1 shell32=1 total=67
OK
```

- los tamanos que deduce el compilador son 65 / 1 / 1 (la vieja `62` estaba tres corta);
- no hay ningun simbolo duplicado en las tres tablas juntas;
- estan presentes los seis que el Bloque 4 necesita y que son la razon de este cambio:
  `GetCurrentDirectoryA`, `CreateProcessA`, `GetExitCodeProcess`, `GetFileSizeEx`, `ReadFile`,
  `WriteFile` (mas `CreateFileA`, `CloseHandle` y `WaitForSingleObject`, que ya estaban y que
  las operaciones nuevas reutilizan).

Eso verifica la construccion. Lo que **no** verifica --y no puede-- es que el enlace real
resuelva los simbolos: eso lo mide `all.bat` en Windows y no antes.

## Lo que sigue sin estar medido

Igual que antes de esta corrida: **ninguna de las seis operaciones nuevas del Bloque 4 ha
llamado todavia a su funcion de Win32.** `GetFileSizeEx`, `ReadFile`, `WriteFile`,
`GetCurrentDirectoryA` y `CreateProcessA` siguen existiendo solo como IR. Los dos fixtures
nuevos --`c24_binary_file_io` y `c25_binary_roundtrip`-- no se han ejecutado ni una vez.

La corrida que viene es la primera que puede decir algo sobre eso. Los numeros esperados no
cambian respecto a lo que dije antes:

```
ctest      244   (c24 y c25 van dentro de nsc.cc1.correctness, no son pruebas nuevas)
trabajos    88   (44 casos x 2, eran 84)
manifiesto  regenerado por verify_manifest --write
```
