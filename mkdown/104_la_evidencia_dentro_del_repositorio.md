# 104 -- La evidencia de los rojos, dentro del repositorio

Fecha: 2026-09-02
Ambito: solo el compilador (`A:\nsc-devel`).
Ficheros tocados: `tests/support/TestContext.h`, las seis pruebas que conservan evidencia,
`run_windows_checklist.bat`, `nslink_solo.bat`, `all.bat`, `.gitignore`.

## QUE pasaba

El doc 103 cerro tres pruebas rojas con un diagnostico correcto, pero lo cerro **por
eliminacion**. La evidencia que las habria explicado en dos segundos existia --`resultKeeping()`
hizo su trabajo y la conservo-- y estaba aqui:

```
EVIDENCIA CONSERVADA EN: C:\Users\al82dev\AppData\Local\Temp\nativesharp_stage48_3_c_host_integration
```

`%TEMP%` cae fuera de las carpetas del repositorio, asi que ese `link_host.log` con los
`undefined symbol` no se pudo leer desde fuera de la maquina. El diagnostico salio igual --el
discriminante estaba en la propia prueba: la variante DLL pasaba y la estatica no-- pero costo
una corrida entera de comprobacion para confirmar algo que el log decia en una linea.

Esto es una mejora de 2026-09-02: hasta ese dia las pruebas **borraban** su evidencia pasaran o
no, asi que no habia nada que leer en ningun sitio. Arreglar donde vive es el paso que faltaba.

## COMO queda

### Un sitio nuevo, que se elige con una variable

`tests/support/TestContext.h` gana una funcion:

```cpp
inline std::filesystem::path evidenceRoot(std::string_view name);
```

Lee `NSC_TEST_EVIDENCE_DIR`. Si esta puesta, la raiz de la prueba cuelga de ahi; si no, se
comporta **exactamente como antes** (`temp_directory_path() / name`), asi que ejecutar un binario
de prueba a mano no cambia. Usa `_dupenv_s` bajo `_WIN32` y `std::getenv` fuera, que es el mismo
patron que ya usa `src/Linker/LldLinker.cpp` y evita el C4996 con `/WX`.

La raiz **no** se crea ahi: cada prueba ya la borra y la crea al arrancar, y eso es justamente lo
que las mantiene aisladas cuando ctest corre con `-j 16`.

### Quien pone la variable

`run_windows_checklist.bat`, justo antes de ctest:

```bat
if defined NSC_LOG_DIR (set "NSC_TEST_EVIDENCE_DIR=%ROOT%\_evidencia\%NSC_LOG_DIR%") else (set "NSC_TEST_EVIDENCE_DIR=%ROOT%\_evidencia\_logs")
```

Cada pasada de `all.bat` deja la suya separada:

```
_evidencia\_logs_lldlink\nativesharp_stage48_3_c_host_integration\StaticHost\link_host.log
_evidencia\_logs_nslink\...
_evidencia\_logs_nslink_solo\...      (nslink_solo.bat, con su propio nombre)
```

`all.bat` borra `_evidencia` al arrancar, por el mismo motivo por el que borra los logs: la
evidencia de una corrida no debe confundirse con la de la anterior. Y `.gitignore` gana
`/_evidencia`.

### Por que FUERA de `%LOGDIR%` y no dentro

Seria mas bonito poner la evidencia dentro del directorio de logs de su pasada. Seria tambien un
error ya cometido: `nslink_pipeline.py` copia `%LOGDIR%` entero al buzon de NSLink, y esa copia
ya se trago 5319 ficheros una vez, cuando unos temporales de enlace acabaron dentro de un
directorio de logs sin estar en `LOGS_SKIP`. Colgando de `_evidencia\` --hermano de los logs, no
hijo-- ese problema no puede volver a existir y no hay ninguna lista que mantener sincronizada.

Esa es la misma leccion de los docs 102 y 103, aplicada antes de tropezar en vez de despues.

## Alcance: seis pruebas, no noventa y seis

Hay 96 ficheros de prueba que llaman a `temp_directory_path()`. Solo **seis** conservan la
evidencia cuando fallan --las que llaman a `resultKeeping()`-- y son las unicas cuya evidencia
puede llegar a existir despues de la corrida:

```
Stage46NativeLibrariesTests.cpp
Stage47HostCAbiTests.cpp
Stage48_1CHeaderGeneratorTests.cpp
Stage48_3CHostIntegrationTests.cpp
Stage48_4CppHostIntegrationTests.cpp
Stage48_5CSharpPInvokeHostTests.cpp
```

Las otras 90 borran su directorio pase lo que pase, asi que moverlo no cambiaria nada
observable: seria un diff de 90 ficheros por cero beneficio. Cuando una de ellas gane
`resultKeeping()`, cambia su linea y ya esta -- la funcion ya esta escrita y disponible para
todas.

## Lo que se descarto, y por que

**Redirigir `TEMP` y `TMP` para toda la corrida de ctest.** Es una linea en el `.bat` y cero
ficheros de prueba tocados, porque en Windows `temp_directory_path()` lee esas variables. Se
descarto: eso no redirige la evidencia, redirige **todo** lo temporal de todo lo que ctest
lanza -- clang, lld-link, `dotnet` y MSBuild en stage48_5, NuGet. Se cambia el entorno de
herramientas de terceros para arreglar un problema nuestro, y cuando algo se comporte distinto
bajo ctest que fuera, esta linea sera la ultima que alguien mire. La variable propia afecta a lo
que decidimos y a nada mas.

## Verificacion

Los seis ficheros modificados mas la cabecera, con las cabeceras reales del arbol:

```
clang++-18 -std=c++20 -Wall -Wextra -fsyntax-only -I include <cada uno>
```

Los seis limpios, ni error ni aviso. (`Stage48_1` necesita el `BuildId.h` que genera CMake y
`Stage48_5` el `-DNSC_SOURCE_DIR` que pasa CMake; se les dio un equivalente para el chequeo.)

Los tres `.bat` siguen siendo CRLF, comprobado con `file`.

Lo que esto **no** verifica: que la evidencia acabe donde digo. Eso se ve en la proxima corrida
--`all.bat` imprime ahora la ruta antes de ctest-- y sobre todo la primera vez que algo se ponga
rojo.
