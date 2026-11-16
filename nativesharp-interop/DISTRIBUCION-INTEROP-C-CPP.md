# Distribución de NSC con interoperabilidad C y C++

Este documento cubre T18-017 a T18-020: qué contiene el paquete de NSC, de qué depende, qué más necesita para
compilar programas que usan bibliotecas C y C++, y cómo reproducir las pruebas integrales. La prueba del
paquete es `pruebas-tanda-18-paquete` ([registro](registro-commits/t18-paquete.md)).

## Paquete (T18-017)

`cmake --install build-tanda09 --prefix <prefijo>` instala 116 archivos (lista en
`share/nativesharp/toolchain/NativeSharpInstallation.files`):

| Ruta | Contenido |
|---|---|
| `bin/nsc.exe` | compilador; LLVM y Clang 23.1.0 enlazados de forma estática (`NSC_HOST_LINKAGE=STATIC`) |
| `lib/nativesharp/runtime/NativeSharp.Runtime.Win32.lib` | runtime NativeSharp |
| `sdk/win64/import-libraries/*.import.lib` | bibliotecas de importación de KERNEL32, KERNELBASE, SHELL32 y UCRTBASE |
| `sdk/win64/nativesharp-runtime.nsimports` | importaciones del runtime |
| `lib/NativeSharp.Core*`, `lib/NativeSharp.LLVM.C` | bibliotecas NativeSharp en fuente |
| `share/nativesharp/...` | contratos de toolchain, runtime y compatibilidad |

Dependencias de `nsc.exe` (`llvm-readobj --coff-imports`): solo DLL del sistema: `KERNEL32`, `ADVAPI32`,
`ntdll`, `VERSION`, `OLEAUT32`, `SHELL32` y `ole32`. No necesita el runtime de Visual C++ ni DLL de LLVM.

NSLink no forma parte de la instalación de NSC: es otro producto (`C:/native-sharp/bin/nslink.exe`, NSLink
3.0.0). Importa `KERNEL32`, `ADVAPI32`, `ntdll`, `SHELL32`, `ole32` y `bcrypt`. `lld-link.exe` y
`llvm-lib.exe` vienen de la distribución oficial de LLVM 23.1.0; `lld-link` importa solo DLL del sistema.

## Cómo encuentra NSC cada pieza

| Pieza | Orden de búsqueda | Código |
|---|---|---|
| Runtime NativeSharp | `NSC_WINDOWS_RUNTIME_LIBRARY`; `<bin>/../lib/nativesharp/runtime/`; el prefijo de instalación configurado | `WindowsRuntimeLibrary.cpp` |
| Bibliotecas de importación | `NSC_WINDOWS_RUNTIME_IMPORT_LIBRARY_DIRECTORY`; `<bin>/../sdk/win64/import-libraries/`; el prefijo configurado | `WindowsRuntimeImports.cpp` |
| Enlazador | `NSC_LINKER` (`nslink.exe` o `lld-link.exe`); `nslink.exe` junto a `nsc.exe`; `PATH` | `LldLinker.cpp` (`findNsLink`) |
| lld-link (con `--crt-startup`, bitcode ThinLTO, TLS) | `lld-link.exe` junto a `nsc.exe`; `PATH` | `LldLinker.cpp` |
| Bibliotecas estáticas (`--emit-project-oop-lib`) | `NSC_LLVM_LIB`; `llvm-lib.exe` junto al enlazador; `PATH` | `LldLinker.cpp` (`findLibrarian`) |
| Encabezados de recursos de Clang | `--cpp-clang-root=<raíz>`: `<raíz>/lib/clang/23/include` | `CompilerSession.cpp` |

Con `--cpp-clang-root`, NSC usa Clang dentro del proceso: `<raíz>/bin/clang.exe` es solo el nombre del
driver y no tiene que existir. La raíz necesita únicamente `lib/clang/23/include` (322 archivos).

## Entorno limpio (T18-018)

`pruebas-tanda-18-paquete` instala el build en un prefijo nuevo de `A:/temp`. Copia junto a `nsc.exe`:

- `nslink.exe`;
- `lld-link.exe`;
- después, `llvm-lib.exe`.

También copia solo los encabezados de recursos de Clang. Compila y ejecuta con un entorno que tiene solo
`SystemRoot`, `SystemDrive`, `TEMP`, `TMP` y `PATH=C:\Windows\System32`, sin ninguna variable `NSC_*`.

| Caso | O0 | O2 |
|---|---|---|
| Programa NativeSharp solo | 42 | 42 |
| Biblioteca C estática con un struct por valor | 42 | 42 |
| Biblioteca C++ con `std::vector` y `std::accumulate`, más `std::gcd` desde NativeSharp, `--crt-startup` | 42 | 42 |
| DLL NativeSharp cargada con `LoadLibrary` por un programa C | 42 | 42 |
| Biblioteca estática NativeSharp, con `llvm-lib.exe` junto a `nsc.exe` | 3 miembros | — |

Negativos:

- sin los encabezados de recursos: `Clang resource include directory is not accessible`;
- sin `nslink.exe` junto a `nsc.exe`: `NSLink was not found beside nsc.exe or in PATH`;
- sin `llvm-lib.exe`, la biblioteca estática se rechaza nombrando `llvm-lib`.

El prefijo configurado del build es `C:/Program Files (x86)/NativeSharp`. El paquete se usó desde su propio
directorio, sin instalar nada en `C:`.

## Qué necesita un programa (T18-019)

| Programa | Herramientas | Encabezados | Bibliotecas |
|---|---|---|---|
| Solo NativeSharp, ejecutable o DLL | `nsc.exe`, `nslink.exe` | — | las del paquete |
| Biblioteca NativeSharp estática | además `llvm-lib.exe` | — | — |
| Biblioteca C sin el CRT | `nsc.exe`, `nslink.exe` | los de la biblioteca; recursos de Clang 23 (`--cpp-clang-root`) | la `.lib` de la biblioteca (`Library=`) |
| Biblioteca C o C++ que usa el CRT o la STL, estática | además `lld-link.exe` y `--crt-startup` | además el toolset de MSVC (`include`) y el SDK de Windows (`ucrt`, `um`, `shared`) como `IncludeDirectory Kind="System"` | `libcmt`, `libvcruntime`, `libcpmt`, `libucrt` y `kernel32` con `--link-lib` |
| Biblioteca como DLL | `nsc.exe`, `nslink.exe` | igual que la estática | la biblioteca de importación (`Library=`) y la DLL (`Runtime=`) |
| Bitcode ThinLTO o TLS de una biblioteca C | `lld-link.exe` | — | — |

Versiones probadas:

- MSVC toolset 14.52.36815;
- Windows SDK 10.0.28000.0, en `A:/vc2026-runtime`;
- solo el runtime estático (`/MT`); el runtime dinámico de MSVC (`/MD`) queda fuera.

La [guía de bibliotecas](GUIA-BIBLIOTECAS-C-CPP.md) explica cómo se declaran en el proyecto.

## Versiones (T18-020)

| Componente | Versión |
|---|---|
| Rama y commit | `development-cpp-clang`, `ad7922d8b` antes de la validación de la tanda 18 |
| NSC | 3.0.0, build hash `79b7f63eb94a6e50`, Release, `NSC_ENABLE_CPP_INTEROP=ON`, `NSC_HOST_LINKAGE=STATIC` |
| Clang, LLD, llvm-lib | 23.1.0 (`ea7d852a70e8bdfaf601d6626a760f9771b2c4b4`), `C:/clang-llvm` |
| NSLink | 3.0.0, `C:/native-sharp/bin/nslink.exe` |
| MSVC toolset, Windows SDK | 14.52.36815, 10.0.28000.0 |
| CMake, Ninja, Python | 4.4.2, 1.13.2, 3.14.7 |
| zlib | 1.3.2 (`A:/al82dev-clang-deps/zlib-1.3.2`) |
| ICU4C | 80 (`A:/al82dev-clang-deps/icu/icu4c`) |

## Reproducción

Construir NSC:

```
set PATH=C:\clang-llvm\bin;%PATH%
ninja -C A:\nsc-devel\build-tanda09 nsc
```

Cada fixture se ejecuta desde su directorio. El arnés `regresion-t09.py` fija `NSC_LINKER=nslink.exe`,
`NSC_WINDOWS_RUNTIME_LIBRARY=A:/nsc-devel/build-tanda09/generated/NativeSharp.Runtime.Win32.lib`,
`TEMP=TMP=A:/temp` y pone `C:\clang-llvm\bin` en `PATH`.

```
python validar.py --nsc A:/nsc-devel/build-tanda09/bin/nsc.exe --clang-root C:/clang-llvm [--dll]
```

| Prueba integral | Fixture | Variantes |
|---|---|---|
| zlib (C) | `pruebas-tanda-18-zlib` | estática y `--dll`, O0–O3 |
| ICU4C `icuuc` (C++) | `pruebas-tanda-18-icu` | estática y `--dll`, O0–O3 |
| STL de MSVC (plantillas) | `pruebas-tanda-18-stl` | estática, O0–O3 |
| Constructores que lanzan | `pruebas-tanda-18-constructores` | estática y `--dll` |
| Paquete en entorno limpio | `pruebas-tanda-18-paquete` | O0 y O2 |

Regresión completa:

- `lote-t18.cmd <etiqueta>`: tandas 18 a 9 y CTest de interoperabilidad, cinco procesos;
- `lote-dll.cmd <etiqueta>`: los validadores de las tandas 18 a 10 que aceptan `--dll`;
- `ctest --test-dir A:\nsc-devel\build-tanda09`: las pruebas de NSC.

Todos están en `build-claude-t08/tools`. Cada fixture imprime su directorio de evidencia en `A:/temp`.
