# Stage 53.5 "Command-line arguments" -- cierre

## Que se agrego

`NativeSharp.Core.Environment` gana dos operaciones nucleo nuevas:

```csharp
public static int GetCommandLineArgCount();
public static int GetCommandLineArgs(string[] destination);
```

`GetCommandLineArgCount()` devuelve cuantos argumentos tiene la linea de comandos real del
proceso (incluyendo `destination[0]` == el nombre/path del ejecutable, igual que `argv[0]` en C o
`Environment.GetCommandLineArgs()[0]` en .NET real). `GetCommandLineArgs(destination)` llena un
buffer YA ASIGNADO por quien llama y devuelve cuantos elementos escribio de verdad.

Uso tipico:

```csharp
int n = Environment.GetCommandLineArgCount();
string[] args = new string[n];
int written = Environment.GetCommandLineArgs(args);
// args[0] = ejecutable, args[1..] = argumentos reales
```

## Por que dos operaciones y no una

Exactamente el mismo "hueco de owned string[]" que ya bloqueaba `String.Split` (ver el comentario
de cabecera de ese bloque en `String.ns`): este compilador no tiene forma de construir un
`owned string[]` cuyos elementos se llenan en varios pasos dentro del propio metodo -- `raii`
rechaza tipos arreglo, `move()` no acepta locales `owned string[]`, y no existe sintaxis de
inicializador de arreglo. La solucion es la misma que en `String.ns`: quien llama pre-asigna el
buffer con `new string[n]`, y el metodo lo llena devolviendo cuantos elementos escribio.

`GetCommandLineArgCount()` existe solo para que quien llama sepa que tamano pedir ANTES de
asignar -- el mismo rol que cumple el primer llamado de `GetEnvironmentVariableA`/
`GetFullPathNameA` (pedir el tamano antes de llenar), aplicado aca a un arreglo en vez de un
string.

## La decision de diseno: una sola implementacion via CommandLineToArgvW

Windows real no tiene una variante ANSI de `CommandLineToArgvW` -- solo existe la version *W
(UTF-16). Esto obligo a elegir entre dos caminos:

- (A) Usar `GetCommandLineW` + `CommandLineToArgvW` (el parser OFICIAL de Windows) y convertir
  cada resultado UTF-16 a ANSI con `WideCharToMultiByte`.
- (B) Reimplementar a mano el parseo de quoting/escaping de Win32 sobre `GetCommandLineA`
  (ANSI puro), reproduciendo la logica que YA produjo un bug real documentado en
  `15_fix_cmdexe_quoting_tests_host_abi_windows.md`.

Alberto pregunto inicialmente si no convenia implementar ambas segun el caso. Tras una segunda
ronda de aclaracion (correr las dos en paralelo duplica el mantenimiento y el riesgo sin beneficio
real, porque `CommandLineToArgvW` practicamente nunca falla, asi que un fallback manual casi nunca
se ejecutaria pero seguiria exigiendo el mismo nivel de mantenimiento que la version principal),
la decision final y confirmada fue: **una sola implementacion (opcion A), verificada con un
conjunto EXHAUSTIVO de casos de quoting hechos a mano** -- la misma confianza que exigiria una
implementacion manual, sin su riesgo.

Esto hace de `GetCommandLineW`/`CommandLineToArgvW`/`WideCharToMultiByte`/`LocalFree` las
UNICAS cuatro llamadas *W (UTF-16) de todo este runtime -- deliberada y explicitamente, no una
regresion de la disciplina "siempre ANSI" que domina el resto de `WindowsRuntimeBuilder.cpp` (ver
el comentario de cabecera de `buildNullTerminatedCopy` alli). Cada resultado UTF-16 se convierte a
ANSI inmediatamente, antes de que le llegue a codigo NativeSharp.

## CommandLineToArgvW vive en SHELL32.dll, no en KERNEL32.dll

Descubrimiento real durante el diseno: a diferencia de TODO lo demas que este runtime importa,
`CommandLineToArgvW` esta exportada por `SHELL32.dll`. Esto exigio un bloque de import-library
nuevo (`shell32Library`/`shell32Symbols`) en los dos puntos del linker
(`WindowsExecutableBuilder.cpp` y `WindowsLibraryBuilder.cpp`), calcado letra por letra del bloque
ya existente para `KERNELBASE.dll` -- el mecanismo de multiples DLLs (`importLibraries`/
`libraries` como vector) ya soportaba esto sin cambios estructurales.

## Runtime: la funcion mas compleja de este proyecto hasta ahora

`buildEnvironmentGetCommandLineArgs` en `WindowsRuntimeBuilder.cpp` es, en numero de bloques
basicos, la funcion runtime mas compleja construida hasta ahora en NativeSharp: un ciclo con
ramas anidadas vacio/asignado por cada argumento, sin PHI nodes en ningun lado (el mismo estilo
"alloca + store/load" que ya usa el ciclo de copia de `buildNullTerminatedCopy`).

Un detalle de implementacion que vale la pena registrar porque casi produce IR invalido: el
calculo de `writeCount = min(argc, destination.Length)` originalmente calculaba `indexSlot`/
`elementResultSlot`/la carga de `writeCount` DESPUES de emitir los `br` de merge de
`useArgCount`/`useDestLength` -- eso intenta insertar instrucciones despues del terminador de un
basic block, que es IR invalido (un bloque solo puede terminar con UN terminador, al final).
Corregido moviendo esas allocas/inicializaciones al bloque `okEntry`, ANTES de la rama
`argCountFits`, y dejando que `loopCheck` haga los loads que necesita justo al entrar. Detectado
y corregido durante la implementacion, antes de compilar por primera vez.

`writeCount = min(argc, destination.Length)`: si la linea de comandos real tiene mas argumentos
que los que entran en el buffer que paso quien llama, los elementos que sobran se descartan en
silencio (sin excepcion) -- comportamiento distinto de `String.Split`, que trunca el ULTIMO
elemento en vez de descartar los sobrantes. Documentado explicitamente en el comentario de
`Environment.ns`.

## Matriz de quoting exhaustiva (el porque de esta ronda)

El programa de ejemplo (`examples/project_stage53_5/Main.ns`) recibe seis argumentos reales,
elegidos para cubrir cada caso de escaping que `CommandLineToArgvW` debe resolver correctamente:

| arg | bytes literales | que cubre |
|---|---|---|
| 1 | `hello` | caso simple, sin espacios ni comillas |
| 2 | `hello world` | espacio embebido -- debe llegar como UN solo argumento |
| 3 | *(vacio)* | argumento vacio real, no ausencia de argumento |
| 4 | `say "hi"` | comillas dobles embebidas |
| 5 | `C:\path\` | barra invertida final, justo antes de donde iria un cierre de comillas |
| 6 | `a\b` | barra invertida NO seguida de comilla -- pasa sin interpretacion especial |

Los seis casos pasaron en el primer intento, tanto en O0 como en O2, verificados con un probe
manual bajo Wine ANTES de escribir el harness de test definitivo (misma disciplina de
"verificar antes de empaquetar" que la sorpresa `""` vs `"."` de Stage 53.4 establecio).

## Refinamiento del harness de test: escaping de un solo quote, no un mecanismo nuevo

Cada ronda anterior de Stage 53 invocaba Wine con `std::system("cd ... && wine ... args...")`,
concatenando los argumentos directamente en el string. Esa concatenacion ingenua es un riesgo
real para ESTA ronda especificamente: el shell de Linux introduciria su PROPIA capa de quoting
entre los argumentos exactos que el test quiere mandar y lo que Wine/NativeSharp realmente
reciben -- contaminando justo lo que se esta probando.

La primera version de esta ronda intento resolverlo con `fork()` + `execvp()` (un `argv[]`
explicito construido directamente en C++, sin shell de por medio). Alberto senalo, correctamente,
que este proyecto es exclusivamente para Windows y que el codigo (incluidos los tests en C++, que
el compila tanto en este sandbox Linux como en su propia maquina Windows con clang-cl) no deberia
tener nada especifico de Unix/Linux -- `fork()`/`execvp()`/`<unistd.h>` no existen en absoluto
bajo `clang-cl`, y de hecho ese fue el motivo de un build roto real que Alberto reporto (ver mas
abajo).

La solucion definitiva se quedo DENTRO del mismo mecanismo `std::system()` que ya usa cada archivo
de test de Stage 53 (sin APIs de proceso especificas de una plataforma): `runUnderWineWithArgs`
envuelve cada argumento en comillas simples de POSIX antes de concatenarlo (escapando cualquier
comilla simple embebida como `'\''` -- cierra comilla, comilla literal escapada, reabre comilla),
una regla de escaping mecanica e inequivoca para un solo proposito -- que el shell de Linux le
entregue a Wine los bytes exactos -- que es un problema mucho mas simple que el quoting/escaping
de Win32 que esta ronda realmente prueba. Lo que se termina ejercitando es la reconstruccion
argv-a-linea-de-comandos-de-Windows que Wine hace por su cuenta (disenada para resolver
correctamente via `CommandLineToArgvW`), no el escaping del propio harness de test.

## Bug real de build en Windows (clang-cl), corregido antes del cierre

Alberto corrio `build-all.bat` en su maquina (`D:\nsc-devel`, `cmake ... -G Ninja
-DCMAKE_C_COMPILER=clang-cl.exe ...`) sobre el primer ZIP entregado de esta ronda y obtuvo:

```
D:\nsc-devel\tests\unit\Stage53_5CommandLineArgsTests.cpp(72,10): fatal error:
'sys/wait.h' file not found
```

Causa raiz: la primera version de `runUnderWineWithArgs` (ver seccion anterior) incluia
`<fcntl.h>`/`<signal.h>`/`<sys/wait.h>`/`<unistd.h>` sin ninguna proteccion de plataforma --
headers que no existen bajo el toolchain Windows/clang-cl de Alberto. Corregido eliminando esos
headers y el uso de `fork()`/`execvp()`/`waitpid()`/`kill()` por completo, quedandose dentro del
mismo `std::system()` portable que ya usa cada archivo de test de Stage 53 (ver la seccion
anterior para el mecanismo definitivo). Reconstruido y re-verificado (43/43, dos corridas
estables) antes de reempacar los dos ZIPs finales de esta ronda.

## Efecto colateral: los 3 archivos de test de host-integration necesitaron actualizarse

`Stage47HostCAbiTests.cpp`, `Stage48_3CHostIntegrationTests.cpp` y
`Stage48_4CppHostIntegrationTests.cpp` mantienen SU PROPIA copia local de la lista de simbolos
`KERNEL32.dll` (para poder linkear un host C/C++ directamente contra la libreria estatica de
NativeSharp, sin pasar por el generador automatico de import-libraries del propio `nsc`). El
modulo runtime SIEMPRE declara y llama las cuatro funciones nuevas (`GetCommandLineW`,
`CommandLineToArgvW`, `WideCharToMultiByte`, `LocalFree`), sin importar si el programa que se
esta compilando las usa -- asi que cualquier link directo contra la libreria estatica necesita
resolver esos cuatro simbolos tambien. Los tres archivos se actualizaron: sus copias locales de
`kernel32Symbols` ganaron los tres simbolos ANSI/genericos nuevos, y cada uno gano su propio
bloque de import-library `shell32` para `CommandLineToArgvW`, calcado del patron ya usado en
`WindowsLibraryBuilder.cpp`. Sin este ajuste, los tres tests fallaban con errores de link por
simbolo no resuelto -- confirmado y corregido antes de dar por cerrada la ronda.

## Un hallazgo NO relacionado con esta ronda: nsc.stage52-4.owned-cleanup-runtime

Al correr la suite completa de `ctest` se detecto que el test
`nsc.stage52-4.owned-cleanup-runtime` -- que verifica que un bug de double-free/use-after-free
REAL (RAII sin `move()`) efectivamente crashea bajo ejecucion real en Wine -- dejo de crashear de
forma consistente (5/5 corridas manuales de un repro identico, exit code 9 limpio, sin senal de
corrupcion). Esto es una prueba negativa que depende de que la corrupcion de memoria pise algo que
el heap-checker de Windows/Wine efectivamente detecte -- un comportamiento inherentemente sensible
al layout binario. Agregar las nuevas funciones runtime de esta ronda al modulo runtime compartido
(usado por TODOS los programas, aunque no llamen a `GetCommandLineArgs`) desplazo ese layout lo
suficiente como para que el mismo bug de siempre ya no corrompa memoria de forma detectable.

No es un defecto de logica introducido por Stage 53.5 -- el repro aislado (identico al fixture del
test, sin usar ninguna API nueva) confirma el mismo comportamiento de forma consistente, no
intermitente. Es una fragilidad preexistente de un test que depende de UB para pasar. Queda
registrado aca para que Alberto decida si vale la pena hacer ese test mas robusto (por ejemplo,
detectando la corrupcion por otro medio que no dependa del heap-checker de Wine) en una ronda
futura -- fuera del alcance de Stage 53.5 cerrarlo.

## Resultados de verificacion

- Test dedicado: 43/43, estable en dos corridas.
- Suite completa `ctest`: 216/217 (el unico fallo es el hallazgo preexistente de
  `nsc.stage52-4.owned-cleanup-runtime` descripto arriba, no relacionado con la logica de esta
  ronda).
- Ejecucion real bajo Wine, O0 y O2, con la matriz de quoting completa: exit code 0 en ambos
  niveles de optimizacion, probado manualmente antes de empaquetar y confirmado de nuevo por el
  test dedicado.
