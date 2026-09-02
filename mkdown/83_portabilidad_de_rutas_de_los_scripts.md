# 83 — Portabilidad de rutas: el árbol se mudó de `D:` a `A:`

**Fecha:** 2026-09-02 · **Ampliado** tras la primera corrida en `A:`
**Qué es:** qué se rompió al mover el árbol de `D:\nsc-devel` a `A:\nsc-devel`, medido archivo por
archivo, y qué se cambió.
**Alcance:** compilador. Sólo scripts; ninguna línea de `src/`, `include/` o `lib/`.

---

## 1. Lo que se midió antes de tocar nada

Se buscaron rutas absolutas en todos los scripts del árbol
(`grep -rniE "[a-z]:[\\/]"` sobre los `.bat`, los `.cmd` de `tools/` y `verify_manifest.py`):

| Archivo | Qué se encontró | ¿Rompe en `A:`? |
|---|---|---|
| `run_windows_checklist.bat` | `set "ROOT=%~dp0"` | **No.** Ya era portable |
| `run_bug_triage.bat` | `set "ROOT=%~dp0"` | **No.** Ya era portable |
| `verify_manifest.py` | `ROOT = Path(__file__).resolve().parent` | **No.** Ya era portable |
| `tools/*.cmd` | sólo `C:\native-sharp` y un `D:\path\to\...` dentro de un `echo` de ayuda | **No.** El `D:` es texto de ejemplo, no una ruta que se use |
| `build-all.bat` | `cmake -S . -B build-clang` | **Sí, a medias.** Relativo al directorio ACTUAL, no al del script |
| `all.bat` | los tres pasos por nombre suelto | **Sí, a medias.** Mismo problema |

**Conclusión de la medición: ningún script tenía `D:` escrito dentro.** La ruta vieja no estaba en el
texto de los scripts. Estaba en otro sitio.

---

## 2. Dónde estaba de verdad el `D:` — y es lo único que rompía

`build-clang\`, el árbol de build, viajó de disco con el resto del repo y lleva la ruta vieja
escrita dentro:

```
build-clang/CMakeCache.txt:2    # For build in directory: d:/nsc-devel/build-clang
build-clang/CMakeCache.txt:88   CMAKE_FIND_PACKAGE_REDIRECTS_DIR:STATIC=D:/nsc-devel/build-clang/...
build-clang/CMakeCache.txt:236  NativeSharp_BINARY_DIR:STATIC=D:/nsc-devel/build-clang
build-clang/CMakeCache.txt:242  NativeSharp_SOURCE_DIR:STATIC=D:/nsc-devel
build-clang/CMakeCache.txt:252  CMAKE_CACHEFILE_DIR:INTERNAL=d:/nsc-devel/build-clang
build-clang/CMakeCache.txt:320  CMAKE_HOME_DIRECTORY:INTERNAL=D:/nsc-devel
```

Y no sólo ahí: `build.ninja` (560 KB), `CTestTestfile.cmake` (77 KB), `.ninja_deps`, cada regla de
compilación y la definición `NSC_SOURCE_DIR="D:/nsc-devel"` con la que se compilan las suites de
tests — que es la que les dice dónde están las fixtures.

**Efecto:** CMake se niega a reutilizar ese directorio desde otra ruta, y Ninja tampoco puede
regenerarlo. Falla el modo completo y fallan igual los modos incrementales, porque Ninja re-ejecuta
CMake antes de compilar.

**Por qué NO se arregla con un reemplazo de texto.** Se podría cambiar `D:/nsc-devel` por
`A:/nsc-devel` en los tres archivos de texto, pero `.ninja_deps` es binario, los `.obj` llevan rutas
de depuración dentro, y `install_manifest.txt` apunta a lo que se instaló desde el árbol viejo.
Quedaría un árbol de build que compila y en el que nadie puede demostrar qué se recompiló y qué no.
En este proyecto eso es lo peor que puede pasar: **la suite es la única red de seguridad**, y una
suite verde sobre un árbol de build dudoso no demuestra nada.

**Se borra y se reconfigura.** Reconfigurar cuesta una compilación completa. No poder confiar en el
resultado cuesta más.

---

## 3. Qué se cambió

### 3.1 La guarda de árbol ajeno

Va en `build-all.bat`, en `run_windows_checklist.bat` y en `run_ids_121_122.bat`. Lee
`CMAKE_HOME_DIRECTORY` del `CMakeCache.txt`, lo compara con `%~dp0` y, si no coinciden, borra
`build-clang\` y lo dice:

```bat
if not exist "%BUILDDIR%\CMakeCache.txt" goto :cache_ok
set "CACHEROOT="
for /f "tokens=2 delims==" %%A in ('findstr /B /C:"CMAKE_HOME_DIRECTORY:INTERNAL=" "%BUILDDIR%\CMakeCache.txt"') do set "CACHEROOT=%%A"
set "ROOTFWD=%ROOT:\=/%"
if /I "%CACHEROOT%"=="%ROOTFWD%" goto :cache_ok
rmdir /s /q "%BUILDDIR%"
:cache_ok
```

**En `run_windows_checklist.bat` la guarda además fuerza `MODE=full`.** Sin eso, `quick`, `build` y
`norebuild` se saltan la reconfiguración y correrían la matriz entera contra un árbol de build que
acaba de desaparecer. Es la misma clase de fallo que el comentario de `:full_build` de ese archivo
documenta: un paso que devuelve éxito sobre algo que no es lo que el script cree.

La guarda es idempotente y cuesta un `findstr`: en cuanto el árbol se reconfigura una vez, no vuelve
a dispararse.

### 3.1b El segundo fallo, que la primera corrida destapó — `CMAKE_MT-NOTFOUND`

La guarda hizo su trabajo: borró el `build-clang\` de `D:` y forzó la reconstrucción. Y **la
reconfiguración falló en tres segundos**, por algo que no tiene nada que ver con el disco:

```
-- Check for working CXX compiler: C:/clang-llvm/bin/clang-cl.exe - broken
MT: command "CMAKE_MT-NOTFOUND /nologo /manifest ... " failed (exit code 0x0)
    no such file or directory
CMake Error at .../CMakeTestCXXCompiler.cmake:71 (message):
  The C++ compiler "C:/clang-llvm/bin/clang-cl.exe" is not able to compile a simple test program.
```

**El mensaje apunta al sitio equivocado.** El compilador no está roto: el paso `[1/2]` compiló el
`.obj` sin una queja. Lo que falta es `mt.exe`, la herramienta de manifiestos que CMake usa al
enlazar con el driver estilo MSVC. `CMAKE_MT` quedó en `NOTFOUND`, el enlace falló, y CMake lo
reportó como «el compilador no funciona».

**Por qué antes sí y ahora no**, medido en el `CMakeCache.txt` viejo antes de borrarlo:

```
CMAKE_MT:FILEPATH=C:/clang-llvm/bin/llvm-mt.exe
CMAKE_MAKE_PROGRAM:FILEPATH=C:/apps/ninja/ninja.exe
```

En la máquina de `D:` **CMake encontró `llvm-mt.exe` solo**, porque `C:\clang-llvm\bin` estaba en el
`PATH`. En este desktop no lo está — se ve en la misma corrida, que usó `A:/apps/cmake` y
`A:/apps/ninja`, otras rutas portables. `CMAKE_RC_COMPILER` sí se resolvió (`llvm-rc.exe`) porque
CMake lo deriva del directorio del compilador; `CMAKE_MT` sólo lo busca en el `PATH`.

**Lo que nunca se vio:** ese valor vivía en la caché desde la primera configuración de aquella
máquina. Mientras el árbol de build sobrevivió, nadie volvió a depender del `PATH`. Reconfigurar
desde cero es lo que lo destapó — y va a volver a pasar cada vez que se configure en una máquina
nueva.

**La corrección:** las rutas del toolchain se pasan **todas explícitas** y salen de una sola
variable:

```bat
set "LLVMROOT=C:\clang-llvm"
set "LLVM=%LLVMROOT:\=/%"
...
-DCMAKE_C_COMPILER="%LLVM%/bin/clang-cl.exe"
-DCMAKE_CXX_COMPILER="%LLVM%/bin/clang-cl.exe"
-DCMAKE_LINKER="%LLVM%/bin/lld-link.exe"
-DCMAKE_MT="%LLVM%/bin/llvm-mt.exe"
-DCMAKE_RC_COMPILER="%LLVM%/bin/llvm-rc.exe"
-DNSC_LLVM_MONOLITHIC_LIBRARY="%LLVM%/lib/LLVM-C.lib"
```

`-DCMAKE_MT=C:/clang-llvm/bin/llvm-mt.exe` es **exactamente el valor que tenía la caché que
funcionaba**: una medición, no una conjetura. Y con `CMAKE_RC_COMPILER` explícito también, la
configuración deja de depender del `PATH` para cualquier pieza del toolchain.

Los tres scripts comprueban antes de configurar que existen `clang-cl.exe`, `llvm-mt.exe`,
`lld-link.exe` y `LLVM-C.lib`, y si falta alguna la nombran. Un fallo por herramienta ausente no
tiene por qué presentarse como un compilador roto.

### 3.1c La guarda, ampliada: una caché a medias también se borra

Una configuración que falla **deja igualmente un `CMakeCache.txt`**, con sus valores fallidos dentro
(`CMAKE_MT-NOTFOUND`, en este caso). Ese cache apunta ya a la ruta correcta, así que la guarda de
§3.1 lo daría por bueno y reconfiguraría encima arrastrando lo que falló.

`CMAKE_CXX_COMPILER_WORKS:INTERNAL=1` sólo aparece cuando la configuración llegó al final, así que
su ausencia es la señal. La guarda comprueba las dos cosas —ruta ajena y configuración a medias— y
borra en ambos casos.

### 3.1d Un fallo mío en la guarda: estaba escrita dos veces

La segunda corrida imprimió esto:

```
FINDSTR: Cannot open A:\nsc-devel\build-clang\CMakeCache.txt
  build-clang\ se genero desde "" y este arbol esta en "A:/nsc-devel".
  Se borra y se reconfigura desde cero.
The system cannot find the file specified.
```

No era un estado raro del árbol: **la guarda estaba pegada dos veces** en `build-all.bat` y en
`run_ids_121_122.bat`. Al sustituir la versión corta por la larga, el corte del bloque viejo se hizo
buscando `:cache_ok` y encontró primero la subcadena dentro de un `goto :cache_ok`, así que la mitad
final del bloque viejo sobrevivió debajo del nuevo. La primera pasada borró el directorio; la
segunda corrió `findstr` sobre un `CMakeCache.txt` que ya no existía, se quedó con `CACHEROOT`
vacío, lo tomó por ajeno y llamó a `rmdir` sobre una carpeta ausente.

Salió barato porque `CACHEROOT` vacío nunca iguala a `ROOTFWD`. Pero el fragmento duplicado dejaba
**dos etiquetas `:cache_ok`**, y cmd.exe salta siempre a la primera: si la comparación hubiera dado
igual, el `goto` habría vuelto hacia atrás y el script entraría en bucle. Eliminado el duplicado, y
de paso `findstr` y `rmdir` van con `2>nul` para no ensuciar la salida cuando el archivo no está.

### 3.1e El toolchain cambió de versión: clang 22.1.6 → 23.1.0

Las dos corridas lo dicen en su primera línea: la que falló por `CMAKE_MT` identificó
**Clang 22.1.6**, y la siguiente **Clang 23.1.0**. La ruta `C:\clang-llvm` es la misma; lo que hay
dentro no.

Eso importa más que la mudanza de disco, porque el proyecto compila con `/W4 /WX`: **un compilador
nuevo trae diagnósticos nuevos, y con warnings-as-errors cada uno es un build roto.** El primero
apareció en el archivo 74 de 415:

```
A:\nsc-devel\src\CFG\CfgVerifier.cpp(25,6): error: unused function template 'containsPointer'
    [-Werror,-Wunused-template]
```

**Medido antes de tocarlo:** `containsPointer` aparece una sola vez en todo el archivo —su propia
definición— y ninguna otra. A su lado vive `countPointer`, que sí se usa, dos veces, en
`verifyEdge`, y que hace la comprobación **fuerte**: una arista tiene que aparecer *exactamente una
vez* en las listas de sus dos extremos, no «al menos una». `containsPointer` es el predecesor débil
que quedó muerto cuando se endureció esa comprobación. Se elimina; `std::find` se va con él y
`<algorithm>` se queda por `std::count` y `std::equal`.

**Y por eso el build ahora va con `ninja -k 0`.** Pararse en el primer archivo que falla obliga a
una reconstrucción completa por cada warning, y con 415 objetivos y un compilador recién estrenado
eso es una tarde. Con `-k 0` la corrida sigue hasta el final y reporta **todos** los archivos que
fallan de una vez, que es la lista que hace falta para arreglarlos en un solo pase.

También se quitó `-DCMAKE_C_COMPILER`: el proyecto declara sólo `CXX`, así que CMake avisaba de una
variable especificada y no usada.

### 3.2 `%~dp0` en vez del directorio actual

`build-all.bat` pasa de `cmake -S . -B build-clang` a `cmake -S "%ROOT%" -B "%BUILDDIR%"`, con
`ROOT` derivado de `%~dp0`. Se puede invocar desde cualquier directorio y desde cualquier disco.
También se le añadió comprobación de error por paso: la versión anterior encadenaba configure,
build e install sin mirar el código de salida de ninguno — y `cmake --install` devuelve 0 aunque el
build haya fallado, porque instala lo que ya existía. Ese fallo exacto está documentado en el
comentario de `:full_build` de `run_windows_checklist.bat`, donde costó una corrida entera.

### 3.3 `all.bat` — dos correcciones de comportamiento

```bat
run_windows_checklist.bat
run_bug_triage.bat
python verify_manifest.py --clean --write
```

**Un `.bat` que invoca otro `.bat` sin `call` transfiere el control y no vuelve.** Tal como estaba,
`all.bat` corría el checklist y terminaba ahí: **`run_bug_triage.bat` y `verify_manifest.py` no se
ejecutaban nunca**. Los tres pasos llevan ahora `call`, y el archivo abre con `pushd "%~dp0"` para
no depender del directorio actual.

Esto **cambia lo que hace una corrida de `all.bat`**: pasa a correr de verdad los tres pasos, y por
tanto tarda más que antes. Es lo que el archivo dice que hace.

### 3.4 `run_ids_121_122.bat` — CRLF

Se había escrito con finales de línea LF. Las cabeceras de `run_windows_checklist.bat` y
`run_bug_triage.bat` avisan de esto por escrito: cmd.exe resuelve saltos por offset de bytes
asumiendo CRLF, y con LF se desalinea y salta bloques enteros en silencio. Reescrito con CRLF, como
los demás.

---

## 4. La corrida del 2026-09-02: 27 tests rojos, una sola causa, y no es de disco

`all.bat` corrió entero por primera vez: build e install verdes, **71/71** fixtures del checklist
verdes, **16/16** del triage de bugs. Rojo sólo `ctest`: **27 de 242**.

Los 27 son **una sola causa**. El mensaje, idéntico en todos:

```
nsc: error: referenced project does not exist:
  'A:\nsc-devel\tests\cc1\c1_constants\../../../lib/NativeSharp.Core/NativeSharp.Core.nsproj'
```

**Y ese archivo existe.** Está en `A:\nsc-devel\lib\NativeSharp.Core\NativeSharp.Core.nsproj`, y
las fixtures del checklist compilaron ese mismo `.nsproj` con esa misma referencia **cinco minutos
después, sin un solo fallo**. Lo que cambia entre los dos momentos no son las fuentes: es que
`lib/NativeSharp.Core` **no estaba ahí cuando corrió ctest**.

### Quién crea ese directorio, y cuándo

`tools/prepare_libraries.cmd` — que `tools/build_project.cmd` invoca antes de cada compilación:

```bat
if exist "%PACKAGE_LIB%\NativeSharp.Core" rmdir /s /q "%PACKAGE_LIB%\NativeSharp.Core"
...
xcopy "%CORE_SRC%" "%PACKAGE_LIB%\NativeSharp.Core\" /E /I /Y /Q
```

**`lib/NativeSharp.Core` y `lib/NativeSharp.Core.Collections.Generic` no son fuentes: son copias
sincronizadas desde `C:\native-sharp\lib`**, borradas y regeneradas en cada build de proyecto. Lo
confirma `lib/.source.txt`, cuyo contenido es literalmente «Synced from …» y cuya fecha es la de la
corrida.

### El defecto estructural

En `run_windows_checklist.bat` el orden es:

```
1. build + install
2. ctest              <-- 27 de sus tests necesitan lib/NativeSharp.Core
3. fixtures           <-- ESTE es el paso que sincroniza lib/
4. examples
```

**ctest depende de un directorio que un paso POSTERIOR es el responsable de crear.** Mientras
`lib/` sobreviva de una corrida anterior nadie lo nota. En cuanto queda ausente —una sincronización
interrumpida entre el `rmdir` y el `xcopy` basta— ctest cae con 27 rojos cuyo mensaje no menciona
bibliotecas por ningún lado, y el paso 3 los deja verdes acto seguido. Es la peor forma posible de
reportarlo: el log dice «referenced project does not exist» sobre un archivo que, cuando vas a
mirarlo, existe.

**Cómo se comprueba, y cuesta una orden.** Ahora mismo `lib/` está sincronizado, porque el paso 3 de
esa corrida lo dejó así. Correr sólo ctest:

```
ctest --test-dir build-clang --output-on-failure
```

Si los 27 pasan a verde, la causa era exactamente ésa y la corrección es sincronizar las
bibliotecas **antes** de ctest, no después. Si siguen rojos, es otra cosa y hay que medir de nuevo.

**Lo que no explica esto:** `verify_manifest.py --clean`. Se revisó por si el `call` nuevo de
`all.bat` lo hubiera puesto a borrar algo por primera vez, y no: su `clean()` sólo **archiva** en
`_sandbox/` una lista fija de seis `.md`/`.txt` de la raíz, y su propio comentario dice que la
primera versión sí borraba y que se cambió a propósito. No toca `lib/` ni ningún otro directorio.

### La corrección

Dos líneas de `run_windows_checklist.bat`, y las dos son defectos del flujo, no preferencias.

**Sincronizar `lib/` antes de ctest.** Se llama a `tools\prepare_libraries.cmd` —la herramienta que
ya existe para esto— justo después del install y antes del paso 2. Es además el momento correcto:
así `lib/` corresponde al compilador que se acaba de instalar y no al de la corrida anterior. Si la
sincronización falla, el script aborta ahí, porque sin `lib/` ni ctest ni las fixtures miden lo que
creen medir.

**Que un ctest rojo cuente en el veredicto.** El bloque de ctest escribía `[FAIL]` en el resumen y
**no tocaba los contadores**. Por eso la corrida terminó diciendo

```
Passed: 71
Failed: 0
```

con 27 de 242 tests en rojo, y salió con código 0. El paso más importante de los cinco era el único
que no entraba en el veredicto — y eso es exactamente lo que rompe la idea de «un solo comando»: el
comando contestaba que todo estaba bien cuando no lo estaba. Ahora cuenta como una unidad, con el
detalle en `02_ctest.log`, y `all.bat` termina distinto de cero si algo falló.

---

## 5. Lo que NO se tocó, y por qué

* **`C:\clang-llvm` y `C:\native-sharp`.** No viven en el repo: son el toolchain LLVM y el prefijo de
  instalación. Siguen siendo correctos en el desktop y no tienen por qué derivarse de `%~dp0`.
* **`tools/setup_toolchain.cmd`.** Su `D:\path\to\native-sharp` está dentro de un `echo` de ayuda,
  como ejemplo de qué forma tiene la variable `NS_ROOT`. No es una ruta que el script use.
* **`run_bug_triage.bat` y `verify_manifest.py`.** Ya eran portables. Se midieron y se dejaron.

---

## 6. Qué hay que correr una vez

La primera corrida de cualquiera de los tres scripts detecta el árbol de `D:` —y también el
`build-clang\` a medias que dejó la configuración fallida— y lo borra sola. Esa corrida compila el
proyecto entero desde cero; las siguientes vuelven a ser incrementales.

Para el Bloque 1a, esa primera corrida es directamente la del stage:

```
run_ids_121_122.bat
```
