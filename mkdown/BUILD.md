# Cómo construir NativeSharp (`nsc`)

Este documento no existía en el repo -- lo que sigue son los pasos reales, verificados contra
este mismo árbol en este sandbox (Linux/GCC + LLVM 20 vía paquete del sistema), más las
instrucciones equivalentes para Windows (tu máquina), que es donde correrás `nsc` para emitir y
ejecutar los `.exe` reales sin pasar por Wine.

## Requisitos

- **CMake 3.25 o más nuevo** (`cmake_minimum_required` en `CMakeLists.txt`).
- **Un compilador C++20**: MSVC (Visual Studio 2022, cualquier build reciente) en Windows; GCC 13+
  o Clang en Linux/macOS.
- **LLVM 19 o más nuevo**. Dos caminos posibles (`CMakeLists.txt`, ~líneas 211-243):
  `find_package(LLVM CONFIG)` (necesita headers de desarrollo + `LLVMConfig.cmake` -- vcpkg o un
  instalador con "LLVM development headers", ver la sección Windows más abajo), o un `.lib`/`.so`
  monolítico solo (`-DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE` + `-DNSC_LLVM_MONOLITHIC_LIBRARY=...`
  -- SIN headers de LLVM, ver "Tu setup real" en la sección Windows: este proyecto nunca incluye
  un header real de LLVM, mantiene su propia API C a mano en `src/LLVM/LlvmCApi.h`).
- Los tests (`NSC_BUILD_TESTS`, ON por defecto) no necesitan nada adicional -- están enlazados
  directo contra las librerías del propio compilador, sin un framework externo.

## Windows (tu máquina) -- recomendado: vcpkg

La forma más simple de tener LLVM+CMake correctamente enlazados en Windows es vía
[vcpkg](https://github.com/microsoft/vcpkg), que resuelve automáticamente los componentes y
genera un `LLVMConfig.cmake` utilizable.

```powershell
# 1) Si no tienes vcpkg todavía:
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat

# 2) Instala LLVM (tarda -- es un build grande; ~30-60 min típico)
.\vcpkg\vcpkg install llvm[target-x86,tools]:x64-windows

# 3) Descomprime este zip en, por ejemplo, C:\src\nativesharp, y desde ahí:
cd C:\src\nativesharp
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\ruta\a\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release -j

# 4) Corre los tests
cd build
ctest -C Release --output-on-failure
```

`nsc.exe` queda en `build\Release\nsc.exe` (o `build\nsc.exe` si usaste un generador de un solo
config como Ninja).

### Alternativa: LLVM pre-compilado + Ninja

Si ya tienes un LLVM instalado (por ejemplo el instalador oficial de llvm.org, o uno que ya usas
para otro proyecto) y prefieres no pasar por vcpkg:

```powershell
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR="C:\ruta\a\LLVM\lib\cmake\llvm"
cmake --build build -j
cd build
ctest --output-on-failure
```

`LLVM_DIR` debe apuntar a la carpeta que contiene `LLVMConfig.cmake` dentro de tu instalación de
LLVM (normalmente `<prefijo-LLVM>\lib\cmake\llvm`).

Si tu LLVM no trae headers de desarrollo (solo el runtime/las libs), el CMake cae automáticamente
al fallback de librería monolítica (`find_library(... NAMES LLVM LLVM-24 ... LLVM-19)`).

### Tu setup real: `clang-cl` + `lld-link` + LLVM-C monolítico, sin headers de LLVM

Este es el comando que de verdad usás vos, verificado que funciona (confirmado por vos mismo) --
lo documento acá tal cual, en vez de solo el ejemplo especulativo de vcpkg de arriba:

```powershell
cmake -S . -B build-clang -G Ninja -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_C_COMPILER="C:/clang-llvm/bin/clang-cl.exe" ^
    -DCMAKE_CXX_COMPILER="C:/clang-llvm/bin/clang-cl.exe" ^
    -DCMAKE_LINKER="C:/clang-llvm/bin/lld-link.exe" ^
    -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" ^
    -DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE ^
    -DNSC_LLVM_MONOLITHIC_LIBRARY="C:/clang-llvm/lib/LLVM-C.lib" ^
    -DNSC_BUILD_TESTS=ON ^
    -DNSC_WARNINGS_AS_ERRORS=ON

cmake --build build-clang --parallel

cd build-clang
ctest --output-on-failure
```

Por qué esto funciona SIN los headers de LLVM (corrigiendo lo que decía este documento antes): el
proyecto entero -- todo `src/LLVM/*.cpp` -- nunca incluye un header real de LLVM (`llvm/...h`).
En vez de eso usa `src/LLVM/LlvmCApi.h`, un subconjunto de la API C de LLVM mantenido A MANO
dentro de este mismo repo (declaraciones `extern "C"` propias, sin depender del header oficial).
Por eso alcanza con linkear contra `LLVM-C.lib` (el import de la superficie C estable de LLVM,
no la biblioteca monolítica C++ completa) -- no hace falta ninguna instalación de LLVM con
headers de desarrollo para compilar `nsc` con este camino. Detalle de las piezas:

- `-DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE`: salta directo la búsqueda de `LLVMConfig.cmake`
  (evita que CMake intente la ruta de vcpkg/headers de arriba) y cae al `else()` de
  `CMakeLists.txt` (líneas ~236-243), que solo hace `find_library`.
- `-DNSC_LLVM_MONOLITHIC_LIBRARY=...`: como esta variable ya viene definida en la línea de
  comando, el `find_library(... REQUIRED)` interno no busca nada -- usa directo la ruta que le
  diste.
- `-DCMAKE_LINKER=".../lld-link.exe"` + `-DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld"`: `clang-cl`
  como front-end de compilación (compatible ABI-wise con MSVC), `lld-link` como linker real --
  la combinación que usa este proyecto en vez del `link.exe` de MSVC.
- `-G Ninja`: generador de un solo config -- por eso `cmake --build build-clang --parallel` no
  necesita `--config Release` (ya quedó fijado en el paso de configuración vía
  `CMAKE_BUILD_TYPE`), y por la misma razón el deploy de abajo tampoco lleva `--config`.

**Deploy con este mismo setup** (ver la sección "Instalar/desplegar" más abajo para el porqué
completo): después del `cmake --build build-clang --parallel` de arriba,

```powershell
cmake --install build-clang --prefix C:\native-sharp
```

(sin `--config` -- innecesario con Ninja, que es de un solo config; `cmake --install` simplemente
reusa el build ya hecho en `build-clang`).

## Linux (como este sandbox de verificación)

```bash
# Debian/Ubuntu -- LLVM 20 vía el paquete del sistema (lo que este sandbox usa)
sudo apt install cmake llvm-20-dev clang-20 ninja-build

cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
cd build && ctest --output-on-failure
```

## macOS

```bash
brew install cmake llvm ninja
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR="$(brew --prefix llvm)/lib/cmake/llvm"
cmake --build build -j
cd build && ctest --output-on-failure
```

## Compilar solo `nsc`, sin los tests (build rápido)

Por defecto `NSC_BUILD_TESTS` está en `ON` (ver `option(NSC_BUILD_TESTS ...)` al inicio de
`CMakeLists.txt`), así que un build normal compila también los ~95 ejecutables de test junto con
el compilador. Si solo quieres `nsc` (por ejemplo para iterar rápido en Windows sin esperar el
build completo), apágalo en el `cmake` de configuración:

```bash
cmake -B build -S . -DNSC_BUILD_TESTS=OFF ...   # + tus flags de LLVM/generador de siempre
cmake --build build -j
```

Con esto CMake ni siquiera genera los targets de test -- no es que se salten al correr, es que no
existen (verificado: 160 pasos de build en vez de varios cientos, y `build/` termina con
únicamente `bin/nsc`, sin carpeta `tests/`). Si ya tienes un `build/` configurado con tests
encendidos, puedes reconfigurar el mismo directorio en vez de borrar y empezar de cero:

```bash
cmake -S . -B build -DNSC_BUILD_TESTS=OFF
cmake --build build -j
```

(Nota: si vienes de un `build/` que ya había compilado tests, los ejecutables de test viejos se
quedan en disco -- `cmake`/`ninja` no los borra al desactivar la opción, simplemente deja de
regenerarlos/actualizarlos. Para un build 100% limpio sin residuos, usa un directorio de build
nuevo.)

## Dónde queda cada archivo generado

`CMakeLists.txt` fija explícitamente tres carpetas de salida dentro de `build/` (el directorio
que le pasas a `cmake -B`), en vez de dejar que CMake las ponga donde le parezca por defecto:

- `build/bin/nsc` (o `build/bin/nsc.exe` en Windows) -- el compilador. Único ejecutable de
  "producto" del proyecto.
- `build/lib/` -- las 7 librerías estáticas internas del propio compilador (`nsc_compilation`,
  `nsc_target`, `nsc_frontend`, `nsc_llvm`, `nsc_runtime`, `nsc_linker`, `nsc_driver`) -- `.a` en
  Linux/macOS, `.lib` en Windows/MSVC. No son algo que uses directamente vos; quedan ahí en vez
  de sueltas en la raíz de `build/` por prolijidad y porque algunas cadenas de herramientas
  (linkers, IDEs) esperan encontrar todo lo enlazable junto.
- `build/tests/` -- todos los ejecutables de test (solo si `NSC_BUILD_TESTS=ON`, el default;
  con `NSC_BUILD_TESTS=OFF` esta carpeta directamente no se crea).

Verificado desde cero (build limpio, sin residuos de un `build/` anterior): con
`NSC_BUILD_TESTS=ON` la raíz de `build/` no tiene ni un `.a`/ejecutable suelto -- todo cae en su
carpeta -- y con `NSC_BUILD_TESTS=OFF` sencillamente no existe `build/tests/`.

`ctest` sigue funcionando igual sin importar esto -- CMake registra la ruta real de cada
ejecutable de test internamente, no depende de que estén en la raíz de `build/`.

## Instalar/desplegar el compilador en otra ruta (`cmake --install`)

`build/` es la carpeta de TRABAJO (ahí vive todo lo que este documento describe arriba, incluidos
los `.a`/`.lib` internos y los ejecutables de test) -- no hace falta que sea la ruta desde la que
después usás `nsc` para tus propios proyectos. Para copiar el compilador YA CONSTRUIDO más las
bibliotecas `.ns` que cualquier proyecto NativeSharp necesita (`lib/NativeSharp.Core`,
`NativeSharp.Core.Threading`, `NativeSharp.Core.Collections.Generic`, `NativeSharp.Core.IO`,
`NativeSharp.Core.Diagnostics`) a una ruta de instalación fija -- por ejemplo `C:\native-sharp`,
que es además la ruta que los `build.bat` de `examples/tutorial_NN_*/` ya buscan como fallback si
no encuentran `nsc.exe` relativo al propio ejemplo -- usá el mecanismo estándar de instalación de
CMake, DESPUÉS de un build normal:

```powershell
cmake --build build --config Release -j
cmake --install build --config Release --prefix C:\native-sharp
```

(Este ejemplo usa un generador multi-config tipo Visual Studio, de ahí el `--config Release` en
ambos pasos. Con tu setup real de `clang-cl`+`lld-link`+Ninja de la sección "Windows" de arriba
-- de un solo config -- ambos comandos van SIN `--config`: `cmake --build build-clang --parallel`
+ `cmake --install build-clang --prefix C:\native-sharp`, exactamente como se muestra ahí.)

```bash
# Linux/macOS, mismo mecanismo:
cmake --build build -j
cmake --install build --prefix /ruta/que/quieras
```

Esto deja `<prefix>\bin\nsc.exe` y `<prefix>\lib\NativeSharp.Core\...`,
`<prefix>\lib\NativeSharp.Core.Threading\...`, etc. -- el árbol `lib/` completo, no solo las
bibliotecas que tocó la última ronda. Podés instalar sobre la misma ruta las veces que quieras
(cada `cmake --install` sobreescribe el `nsc.exe` y refresca `lib/` con lo que haya en el
checkout actual) -- es la forma recomendada de mantener tu instalación en `C:\native-sharp` al
día después de aplicar cada ZIP incremental nuevo, en vez de copiar archivos a mano.

No hace falta volver a correr `cmake -B build -S .` para esto -- `cmake --install` reusa la
configuración y el build ya hechos en `build/`; solo hace falta que `cmake --build` haya corrido
al menos una vez antes (para que exista `nsc.exe` en `build/bin/` para copiar).

## Verificación de que quedó bien

```bash
ctest --output-on-failure
# esperado: 100% tests passed, 0 tests failed out of 210 (o el número que diga TEST_MATRIX_52.md
# / TEST_MATRIX_51.md al momento de esta entrega)
```

Si algo falla, la salida de `ctest --output-on-failure` incluye el nombre del test y su output
completo -- pégamelo tal cual y lo reviso.

## Uso básico de `nsc` una vez construido

```bash
# Emitir un ejecutable Windows x64 real a partir de un .nsproj:
nsc --emit-project-oop-exe ruta/al/proyecto.nsproj -O0 -o salida.exe

# Solo chequeo semántico, sin generar código (rápido, útil mientras escribes):
nsc --check-project-method-bodies ruta/al/proyecto.nsproj
```

`salida.exe` es un binario Windows x64 real (PE32+) -- en tu máquina lo corres directo, sin Wine.
