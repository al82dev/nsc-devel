# 103 -- La tabla de imports tenia tres copias mas, y estaban en las pruebas

Fecha: 2026-09-02
Ambito: solo el compilador (`A:\nsc-devel`).
Ficheros tocados: `tests/unit/Stage47HostCAbiTests.cpp`,
`tests/unit/Stage48_3CHostIntegrationTests.cpp`, `tests/unit/Stage48_4CppHostIntegrationTests.cpp`.

## Lo que midio la pasada 1 (lld-link)

```
Passed: 89   Failed: 1
ctest:  99% tests passed, 3 tests failed out of 244
cases:  88 OK, 0 FAIL en 53.4 s con -j 16
BUILD:  Limpio: ni un error ni un warning. Se compila con /WX, asi que esto es real.
TRIAGE: Vivos: 0   Arreglados: 16
```

**El Bloque 4 quedo medido y esta bien.** Los dos fixtures nuevos pasan en las cuatro
configuraciones:

```
[ OK ] cc1_c24_binary_file_io   -O0 -- exit 42
[ OK ] cc1_c24_binary_file_io   -O2 -- exit 42
[ OK ] cc1_c25_binary_roundtrip -O0 -- exit 42
[ OK ] cc1_c25_binary_roundtrip -O2 -- exit 42
```

Eso es la primera vez que `GetFileSizeEx`, `ReadFile`, `WriteFile` y `GetCurrentDirectoryA` se
llaman de verdad y no solo existen como IR. `nsc.cc1.correctness` --que es donde viven c24 y
c25 dentro de ctest-- tambien paso, 164.89 s.

Los unicos rojos son tres, y son estos:

```
61 - nsc.stage47.host-c-abi      FAILED: 4 of 78 checks
64 - nsc.stage48_3.c-host        FAILED: 2 of 44 checks
65 - nsc.stage48_4.cpp-host      FAILED: 2 of 46 checks
```

## QUE fallaba

Las ocho comprobaciones rojas son, en las tres pruebas, la misma:

```
FAIL: C host links directly against a NativeSharp static library
FAIL: C11 host links directly against the NativeSharp static library and runtime imports
FAIL: C++20 host links directly against NativeSharp static library without C++ runtime dependencies
```

...mas la verificacion del PE resultante, que falla porque el ejecutable no llego a existir.

El discriminante esta dentro de la propia prueba: en `stage48_3`, la variante **shared** paso
(`shared C host is a structurally valid PE32+ executable ... Passed`) y la **static** fallo. La
diferencia entre las dos no es el codigo medido, es quien enlaza el runtime: en la variante DLL
el runtime va dentro de la DLL, y en la estatica el objeto de runtime entra dentro del
ejecutable del host, que entonces necesita **toda** la superficie de imports Win32 del runtime.

Y esa superficie, en estas tres pruebas, salia de una copia.

## POR QUE, y esta es la parte que duele

Los tres ficheros tenian cada uno su propio helper `createRuntimeImports` con las cuatro tablas
de simbolos **pegadas a mano**, incluida:

```cpp
const std::array<std::string_view, 62> kernel32Symbols = { ... };
```

Tres copias, byte a byte identicas entre si y con la del compilador, congeladas en el estado de
antes del Bloque 4. Sin `GetCurrentDirectoryA`, sin `CreateProcessA`, sin `GetExitCodeProcess`.

Lo que hace esto vergonzoso es lo que dice la cabecera del compilador sobre si misma. En
`include/nativesharp/Linker/WindowsRuntimeImports.h`, textualmente:

> THE single source of truth for the Windows runtime's import surface, and the single place that
> materializes import libraries for a link.

y en su comentario de HISTORY:

> before this existed, the four runtime symbol tables (KERNEL32's 62 entries, ...) were
> DUPLICATED verbatim between WindowsExecutableBuilder.cpp and WindowsLibraryBuilder.cpp, and the
> project carried a standing rule that any new runtime Win32 import had to be synchronized across
> **five locations** by hand.

Cinco sitios. La consolidacion elimino **dos** --los dos constructores-- y declaro el problema
resuelto. Los otros tres estaban en `tests/unit`, nadie volvio a mirarlos, y el numero 62 del
comentario es literalmente el tamano que se quedo clavado en las tres copias.

Asi que este es el mismo fallo del 2026-09-01 (`undefined symbol: GetCurrentDirectoryA`, 86 de
86 trabajos rotos) y del doc 102 (`excess elements in struct initializer`, build roto) por
**tercera vez en dos dias**, en el ultimo sitio donde todavia quedaba una copia. Las tres veces:
un simbolo nuevo en el runtime y una lista que no se entero.

## COMO queda arreglado

Los tres helpers dejan de tener lista y llaman a la funcion del compilador:

```cpp
const std::string_view allCompilerSupportSymbols =
    "@__C_specific_handler @llvm.memset @llvm.memcpy @llvm.memmove";
return createWindowsImportLibraries(
    linker, directory, allCompilerSupportSymbols, {}, imports, error);
```

Dos detalles que no son obvios:

**El texto de IR falso.** Los cuatro simbolos condicionales de ucrtbase los decide un escaneo del
texto del IR, porque las pasadas de LLVM pueden materializar `@llvm.memcpy`/`memset`/`memmove` o
referenciar `__C_specific_handler` en cualquiera de los dos modulos. Aqui no hay un IR concreto
que escanear --se esta preparando el enlace de un host ajeno-- asi que se le pasa un texto que
los nombra a los cuatro. Es exactamente el conjunto que la copia anterior pedia a mano
(`{"strlen", "__C_specific_handler", "memset", "memcpy", "memmove"}`).

**Pedir de mas es gratis.** Una biblioteca de importacion es un archivo PEREZOSO: el enlazador
solo saca de ella los miembros que alguien referencia. Un simbolo de mas en el `.lib` no aparece
en el PE ni cuesta un byte. Pedir de menos es justo el fallo que esto viene a arreglar. Esa
asimetria es la razon de que la respuesta correcta sea "toda la superficie", y no "calcular
cuales hacen falta".

## Lo que NO cambia, y lo digo explicitamente

**No se ha tocado ni un `expect()`.** No se ha borrado ninguna comprobacion, no se ha relajado
ninguna condicion, no se ha cambiado ningun valor esperado. Salen las mismas cuatro bibliotecas
de importacion, en el mismo orden (KERNEL32, KERNELBASE, SHELL32, ucrtbase), el `if
(imports.size() != 4) return;` sigue igual, y detras quedan literalmente las mismas lineas de
antes. Lo unico que cambia es **de donde sale la lista de simbolos**: de la tabla real del
compilador en vez de una copia congelada.

Y quiero dejar escrito por que esto no es "aflojar la prueba para que pase", porque la regla del
proyecto es que las pruebas no se tocan para que todo salga verde:

- La copia **no verificaba nada**. Una segunda lista escrita a mano no comprueba que la primera
  sea correcta; solo se queda vieja en silencio y despues acusa al cambio equivocado. En esta
  corrida acuso al Bloque 4 de romper el enlace de hosts C/C++, y el Bloque 4 no tenia nada que
  ver.
- Lo que la prueba mide --que un host C o C++ enlaza contra la biblioteca estatica de NativeSharp
  y produce un PE valido-- se mide **mejor** ahora, porque usa la superficie de imports real, la
  que un host de verdad recibiria, en vez de una reconstruccion.
- La cabecera del compilador ya declaraba que era la unica fuente de verdad. Esto no inventa una
  politica nueva: termina la que estaba a medias.

## Verificacion

`tests/unit` no se compila en el contenedor como parte del harness, pero las cabeceras si estan.
Los tres ficheros modificados, tal cual quedan, con las cabeceras reales del arbol:

```
clang++-18 -std=c++20 -Wall -Wextra -fsyntax-only -I include Stage47HostCAbiTests.cpp
clang++-18 -std=c++20 -Wall -Wextra -fsyntax-only -I include Stage48_3CHostIntegrationTests.cpp
clang++-18 -std=c++20 -Wall -Wextra -fsyntax-only -I include Stage48_4CppHostIntegrationTests.cpp
```

Los tres sin una sola linea de salida: ni error ni aviso. Comprobado ademas que
`WindowsRuntimeImports.h` en el contenedor es identico byte a byte al del arbol, para que el
chequeo sea contra la cabecera de verdad y no contra una vieja.

Lo que esto **no** demuestra es que el enlace resuelva. Eso lo mide `all.bat` en Windows, y es lo
unico que cuenta.

## Estado despues de esto

Queda **una** tabla de imports Win32 en todo el proyecto. La regla de "sincronizar cinco sitios a
mano" ya no tiene sentido porque no hay cinco sitios: hay uno, y los cuatro consumidores --los
dos constructores del compilador y los tres helpers de prueba-- lo llaman.

De la cadena de diez sitios que hay que tocar para una operacion nueva del nucleo (doc 100), el
decimo --`src/Linker/WindowsRuntimeImports.cpp`-- sigue siendo el decimo. Lo que desaparece es el
undecimo, el duodecimo y el decimotercero, que estaban ahi sin que nadie los hubiera contado.

Numeros esperados en la proxima corrida: **ctest 244/244**, **88 trabajos OK**, triage 16/16.
