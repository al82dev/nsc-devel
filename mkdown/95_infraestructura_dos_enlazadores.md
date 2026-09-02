# 95 — Infraestructura de dos enlazadores: el pipeline ENTERO, dos veces

**Fecha:** 2026-09-02
**Qué añade:** que **una** corrida ejecute **la batería completa dos veces** —el mismo compilador,
un enlazador cada vez— y deje **dos reportes autocontenidos que no comparten un solo número**, más
el material de los fallos del enlazador en el árbol de quien lo desarrolla.
**Estado:** entregada y **ejecutada**. La primera corrida limpia está en la sección 8: `nsc` verde
con los dos enlazadores, y cinco pruebas de `ctest` que sólo fallan con NSLink.

---

## 1. Por qué existe, y la corrección que la hizo necesaria

Yo dije, midiendo mal, que `nslink.exe` no existía. Había buscado **sólo en `A:\nsc-devel`**, que
es el repo del compilador. NSLink es otro proyecto y vive en el prefijo compartido:

```
C:\native-sharp\bin\nslink.exe        538 KB
C:\native-sharp\lib\NativeLinkCore.lib   2.17 MB
C:\native-sharp\include\nslink\**
C:\native-sharp\sdk\win64\*.nsimports
A:\nslink-devel\                       su árbol de desarrollo
```

Y no es un experimento: por sus cabeceras tiene `CoffObjectReader`, `GlobalSymbolTable`,
`RelocationEngine`, `SectionLayout`, `SectionGarbageCollector`, `IdenticalCodeFolder`,
`ComdatResolver`, `DirectiveParser`, `PeImageWriter`, `PeImportBuilder`, `PeExportBuilder`,
`BaseRelocationBuilder` y **`PeExceptionTable`** — o sea `.pdata`/`.xdata`, que es lo que SEH
necesita.

**La lección de método, que vale más que el dato:** una medición es tan ancha como el directorio
donde se hizo. «Lo medí en el árbol» era cierto y la conclusión que saqué de ello era falsa.

---

## 2. Cómo se sustituye el enlazador, sin recompilar el compilador

`LldLinker::discover()` lee `NSC_LLD_LINK` **antes** de buscar `lld-link` en el PATH — su propio
mensaje de error lo dice: *«set NSC_LLD_LINK to its executable path»*. Apuntarlo a `nslink.exe`
basta.

Del otro lado, `include/nslink/driver/CompatibleDriver.h` dice que NSLink acepta exactamente las
cuatro formas de línea de comandos que `nsc` emite —ejecutable, DLL, biblioteca estática,
biblioteca de importación— y que **un switch fuera de ese subconjunto es un error con nombre y no
algo que se ignora**: *«ignorar `/OPT:REF` produciría un binario distinto del que se pidió sin que
nadie lo sepa»*. Ese archivo nombra la tarea **NSL-32**.

Las dos mitades estaban escritas. Lo que faltaba era ejercitarlas contra programas reales, y eso
es lo que esta infraestructura hace.

---

## 3. La forma: DOS PASADAS DEL MISMO PIPELINE

La primera versión de esto —escrita ayer, sustituida hoy— corría el pipeline **una** vez y luego
repetía **sólo las 84 fixtures** con NSLink. Estaba mal, y el motivo es exactamente el que la hizo
inservible: la construcción, las 243 pruebas de `ctest`, el triage de bugs y el manifiesto se
medían **únicamente con lld-link**. De NSLink no se sabía si la biblioteca de importación que
genera es válida, si el `.pdata` que emite sobrevive a una prueba unitaria, ni si el triage pasa —
sólo si 84 ejecutables corrían. Un reporte que cubre una parte del pipeline y otro que cubre el
pipeline entero **no se pueden comparar**, que era el único propósito de tenerlos.

Lo que hay ahora:

**Dos árboles de build y dos directorios de logs, desde el primer byte.** Es lo que hace simple
todo lo demás, y se llegó a ello por descarte. Con un solo `build-clang\` había que elegir entre
reconstruirlo en la pasada 2 —y entonces los dos reportes medían compiladores distintos sin
decirlo— o no reconstruirlo, y entonces la pasada 2 dependía de que la 1 hubiera dejado el árbol
sano. Con dos árboles no hay que elegir: cada pasada construye el suyo desde cero y las dos son
completas por sí solas.

Que las dos compilen las mismas fuentes por separado **no es desperdicio**: si algún día los dos
`nsc.exe` no se comportan igual, eso es un fallo de reproducibilidad y esta corrida es donde tiene
que salir.

```
all.bat
  Limpieza: se borran los dos arboles y los dos directorios de logs.

  PASADA 1 -- lld-link                                  EL VEREDICTO OFICIAL
    NSC_LLD_LINK vacia
    NSC_BUILD_DIR=build-clang-lldlink   NSC_LOG_DIR=_logs_lldlink
    run_windows_checklist.bat      build + ctest + 84 casos + triage
    verify_manifest.py             -> _logs_lldlink\06_manifest.log
    report.py                      -> _logs_lldlink\REPORTE.txt      RC_LLD

  PASADA 2 -- NSLink                                    LA MISMA BATERIA
    NSC_LLD_LINK=<nslink.exe>
    NSC_KEEP_LINK_TEMPS=%CD%\_logs_nslink\linktemps
    NSC_BUILD_DIR=build-clang-nslink    NSC_LOG_DIR=_logs_nslink
    run_windows_checklist.bat      build + ctest + 84 casos + triage
    verify_manifest.py             -> _logs_nslink\06_manifest.log
    report.py                      -> _logs_nslink\REPORTE.txt       RC_NSLINK

  nslink_pipeline.py --compare   compara las dos, NO compila nada
  veredicto combinado ; exit /b %RC_LLD%
```

Las variables son dos, y sin ellas cada script se comporta exactamente como antes, así que se les
puede seguir llamando a mano:

| Variable | La leen |
|---|---|
| `NSC_BUILD_DIR` | `run_windows_checklist.bat` |
| `NSC_LOG_DIR` | `run_windows_checklist.bat`, `run_bug_triage.bat`, `run_cases.py`, `report.py` |

Los dos directorios tienen **la misma forma**, archivo por archivo:

```
SUMMARY.txt  01_build.log  02_ctest.log  03_fixtures.log  04_examples.log
05_triage_run.log  06_manifest.log  10_triage.log  TRIAGE.txt
cases\  bin\  REPORTE.txt
```

Así se leen igual, se comparan línea a línea, y **ningún número de uno puede colarse en el otro**.

### 3.1 Decisiones, y ninguna es de estilo

**Por qué no se comparte el directorio de logs.** `run_windows_checklist.bat` borra el suyo al
arrancar. Es correcto —un reporte con logs de dos corridas mezcladas es peor que no tener reporte—
pero significaba que la segunda pasada borraba la primera. La versión anterior lo resolvía moviendo
`_logs\` a otro nombre y restaurándolo al final, y ese baile costó una corrida entera: al
descarrilar, la ejecución volvió a la limpieza del principio y borró las dos pasadas. Con
`NSC_LOG_DIR` cada una nace en su sitio y no hay nada que mover.

**El marcador de «configuración completa» era falso, y costó una corrida.**
`run_windows_checklist.bat` decidía si `build-clang\` servía mirando si la caché tenía
`CMAKE_CXX_COMPILER_WORKS:INTERNAL=1`. Ese marcador **no existe nunca en este árbol**: CMake omite
la prueba del compilador y el log lo dice —*«Check for working CXX compiler: … - skipped»*—. O sea
que la comprobación daba positivo en **todas** las corridas, borraba un árbol sano y convertía cada
build incremental en una reconstrucción completa. La señal correcta es que exista `build.ninja`, que
lo escribe el `Generating done`, el último paso de la configuración.

Una comprobación cuyo positivo no se puede distinguir de su caso normal no es una comprobación.

**`NSC_LLD_LINK` se fija en CADA pasada, nunca se hereda.** Si alguien llamó a `nslink-env.bat` en
esa ventana, sin limpiarla la pasada «oficial» enlazaría con NSLink **sin decirlo**. No es
hipotético: pasó en la primera corrida de esta infraestructura, y salió a la vista como una
contradicción —cinco pruebas de biblioteca de importación en rojo y «0 regresiones» al mismo
tiempo, dos resultados que no pueden ser verdad a la vez—. `all.bat` la limpia y avisa si venía
puesta.

**El manifiesto se corre en las dos pasadas** aunque su resultado no dependa del enlazador. Es un
no-op la segunda vez, y a cambio los dos reportes quedan completos y con la misma forma. Un reporte
al que le falta una sección se lee distinto que uno que la tiene en verde, y comparar dos cosas que
no tienen la misma forma es como no compararlas.

**El código de salida de `all.bat` es el de la PASADA 1, y sólo el de la pasada 1.** NSLink es otro
componente, con otro dueño y su propio árbol. Un fallo suyo poniendo en rojo la corrida de nsc
diría que nsc está roto cuando lo que falla es el enlazador de otro. Se ve en el veredicto
combinado, se le entrega en su buzón, y no se confunde con esto.

**La pasada 2 se hace SIEMPRE, incluso si la primera está en rojo.** Saltarla ahí sería justo al
revés de lo útil: los dos reportes juntos son precisamente lo que dice de quién es el fallo. Sólo
se omite si no existe el ejecutable, y el veredicto lo dice en vez de callarlo. Se apunta a otro
con `set NSLINK_EXE=<ruta>`.

**`NSC_KEEP_LINK_TEMPS` sólo en la pasada 2.** `nsc` deja el directorio temporal del enlace en vez
de borrarlo: ahí viven `program.obj`, `runtime.obj`, las bibliotecas de importación y el fichero de
respuesta de **cada** invocación — que es lo que NSLink pidió por escrito (`nsc_actions/001`). Va
**dentro** de `_logs\` para que el `move` se lo lleve de un golpe. En la pasada oficial sería basura
sin nadie que la lea.

### 3.2 No se reescribe `all.bat` mientras corre

Está dicho en la cabecera del propio archivo porque costó una corrida entera. cmd.exe lee un `.bat`
**por offset de byte** mientras lo ejecuta: al volver de un `call` sigue leyendo donde se quedó, en
el archivo tal como está **en ese momento**. Si alguien lo reescribió en medio, ese offset ya apunta
a otro sitio.

El 2026-09-02 lo reescribí con la corrida en marcha. Al terminar el checklist de la pasada 2, cmd
retomó ~1800 bytes desplazado, imprimió `'######...' is not recognized as an internal or external
command`, **saltó a la limpieza del principio —que borra los directorios de logs—** y volvió a
llamar al checklist. Las dos pasadas de esa corrida se perdieron enteras.

La regla es simple y no admite excepciones: mientras `all.bat` esté corriendo, no se toca ningún
archivo del repo.

### 3.3 El veredicto combinado

```
=====================================================================================
VEREDICTO COMBINADO -- EL MISMO PIPELINE, LOS DOS ENLAZADORES
=====================================================================================
  PASADA 1  lld-link  [OFICIAL] : VERDE           -> _logs\REPORTE.txt
  PASADA 2  NSLink              : CON FALLOS      -> _logs_nslink\REPORTE.txt
  Buzon de NSLink               : generado, con regresiones
                                  A:\nslink-devel\_inbox_nsc\

  Los dos reportes tienen la MISMA forma y NO comparten un solo numero.
  Lo de NSLink NO cuenta como fallo de nsc: otro componente, otro dueno, otro arbol.
=====================================================================================
```

---

## 4. El buzón: cómo el worker de NSLink trabaja en paralelo

`A:\nslink-devel\_inbox_nsc\` — un directorio de **entrada**, aparte de su corpus curado, que no
forma parte de su construcción ni de sus pruebas y se puede borrar entero.

```
_inbox_nsc\
    README.md            generado: qué es, cómo reproducir, qué NO se pudo adjuntar
    REPORTE.txt          la comparación, con las secciones separadas
    fallos\<caso>.log    la salida COMPLETA de nsc  -- el mensaje del propio NSLink va aquí dentro
    fallos\<caso>.obj    el COFF que NSLink no supo enlazar
    corpus-oferta\       seis objetos representativos, SIEMPRE, fallen o no
```

Lo genera `nslink_pipeline.py --compare`, que **no compila nada**: lee los `cases\results.json` de
las dos pasadas ya hechas, los clasifica y copia el material. Compilar una tercera vez para llenar
el buzón sería medir otra cosa que la que se reporta.

El reporte separa tres cosas que no son lo mismo, y la distinción es la parte útil:

| Sección | Qué es |
|---|---|
| **REGRESIONES** | Pasaba con lld-link y falla con NSLink. **Es suyo.** |
| Fallaba ya con lld-link | No es culpa de NSLink. Va aparte a propósito: mezclarlo escondería lo de arriba. |
| Falla y no está en la referencia | Las tablas de casos de las dos pasadas se separaron. |

**Por qué el `.obj` vale más que el log.** El `corpus/README.md` de NSLink dice lo que le falta:

> **LLVM / clang-cl**: no disponible en el entorno donde se construyó esta etapa. Su ausencia
> importa más de lo que parece: LLVM emite COMDAT, `.drectve` y símbolos débiles, ninguno de los
> cuales aparece en los objetos de NSASM de este directorio.

Tienen **tres** objetos LLVM en `corpus/llvm/`. Cada corrida les deja uno por cada caso que falle,
y con él el fallo se reproduce con `nslink` a secas — sin `nsc`, sin el pipeline y sin esta máquina.

**Y `corpus-oferta\` es lo que hace que el paralelismo funcione de verdad.** Si el buzón sólo
tuviera fallos, una corrida verde dejaría al otro worker sin nada que hacer. Estos seis van
siempre, elegidos porque cada uno ejercita una parte distinta del enlazador:

| Objeto | Qué ejercita |
|---|---|
| `c1_14_callback_exception_boundary` | Excepción cruzando un callback de C: `.pdata`/`.xdata`, o sea `PeExceptionTable` |
| `c1_13_two_libraries_same_symbol` | Dos bibliotecas con el mismo símbolo: resolución de símbolos pura |
| `cc1_c16_enum_layout` | Tamaños y offsets que el enlazador tiene que respetar tal cual |
| `cc1_c18_stackalloc_hardening` | `__chkstk` y el prólogo de marco |
| `cc1_c23_views_and_lifetimes` (**-O2**) | El caso más grande del árbol: genéricos monomorfizados, COMDAT, secciones plegables |
| `project_stage56_c1` | Interop completo: importaciones, exportaciones, ABI de C |

**Promoverlos a `corpus/llvm/` es decisión de quien mantiene NSLink, no de quien deja el buzón.**
Por eso se ofrecen en un directorio aparte y no se escriben en su corpus.

### 4.1 El fichero de respuesta: ya no es «lo que no se puede entregar»

Lo era. `nsc` lo escribía, lo usaba y **lo borraba** en el mismo paso, así que cuando el pipeline
veía el fallo ya no existía; reconstruirlo a mano habría sido dar por medido algo que no se midió.

Se resolvió en el compilador, que es donde tenía que resolverse: **`NSC_KEEP_LINK_TEMPS`**. Si está
puesta, `LldLinker` no borra el `.rsp` ni el `.log`, y `WindowsExecutableBuilder` usa su valor como
raíz del directorio temporal y no lo destruye al salir. Apagada por defecto: en la corrida oficial
sería basura. La pasada 2 la pone, y el `move` se lleva todo dentro de `_logs_nslink\linktemps\`.

`run_cases.py` le da a cada trabajo su propio subdirectorio (`with_link_temp_subdirectory`), porque
los casos corren en paralelo y compartir una sola raíz haría que se pisaran los temporales.

---

## 5. Por qué el pipeline suelto no usa `build_project.cmd`

`nslink_pipeline.py` sigue existiendo **también** en su forma original —una corrida aislada de las
84 fixtures con NSLink, sin tocar nada del pipeline oficial— porque para iterar sobre un fallo
concreto no hace falta reconstruir el compilador. En esa forma llama a `nsc.exe` directamente con
los mismos dos comandos que ese `.cmd` emite:

* **`prepare_libraries.cmd` no corre**, así que no hay `rmdir /s /q lib\NativeSharp.Core` ni la
  carrera que obliga a `NS_LIB_READY` (doc 93 §3). Se usa `lib\` tal como la dejó la corrida
  oficial.
* **Nada se escribe dentro de `_logs\`.**

En `all.bat` no se usa esa forma sino `--compare`, porque ahí la corrida con NSLink ya la hizo el
checklist entero.

Los directorios de logs están **fuera** de `_logs\` porque el checklist lo borra al arrancar. Eso
obligó a tres cosas más, y las tres son necesarias:

* `.gitignore` — los dos árboles de build y los dos directorios de logs.
* `verify_manifest.py` — `_logs_lldlink` y `_logs_nslink` en `EXCLUDED_DIRECTORIES`. **Hay que
  nombrarlos aparte**: la comparación es por nombre exacto de componente de ruta, no por prefijo, y
  `_logs_nslink` no es `_logs`. Sin esas líneas entrarían al manifiesto los `.obj`, `.exe` y los
  temporales de enlace de las dos pasadas, y cambiarían en cada corrida.

---

## 6. La tabla de casos está duplicada, y es deliberado

`nslink_pipeline.py` lleva su propia copia de la tabla `CASES` en vez de importarla de
`run_cases.py`. Importarla ataría el experimento al script de producción: un cambio en el
experimento para probar algo podría romper la corrida oficial, que es exactamente lo que un
pipeline aislado no debe poder hacer.

El precio es real y se paga con una alarma, no con disciplina: **cuando los conteos no cuadran, el
reporte lo dice** —«referencia: N trabajos / aquí: M»— y nombra la causa probable. Añadir una
fixture obliga a añadirla en los dos sitios.

---

## 7. Qué se tocó

| Archivo | Qué |
|---|---|
| `all.bat` | **Reescrito.** Los dos árboles, los dos directorios, la limpieza, las dos pasadas siempre, el veredicto combinado. **Sin bloques `if (...)`**: dentro de un bloque cmd.exe exige escapar `(`, `)` y `>`, y una omisión convierte un `echo` en una redirección silenciosa. Con etiquetas y `goto` no hay nada que escapar |
| `run_windows_checklist.bat` | `NSC_BUILD_DIR` y `NSC_LOG_DIR`; el marcador de configuración completa pasa a ser `build.ninja` |
| `run_bug_triage.bat`, `report.py` | `NSC_LOG_DIR` |
| `run_cases.py` | `NSC_LOG_DIR` y `with_link_temp_subdirectory()` — un subdirectorio de temporales por trabajo, porque los 84 corren en paralelo |
| `nslink_pipeline.py` | `run_compare` —que **no existía**, ver doc 96 §3.2—, la comparación de `ctest`, y la copia de los logs al buzón |
| `src/Linker/LldLinker.cpp` | `keepLinkTemporaries()`; los cuatro sitios de limpieza del `.rsp` y el `.log` bajo esa guarda |
| `src/Linker/WindowsExecutableBuilder.cpp` | `linkTemporaryRootOverride()`; `TemporaryDirectory` no se destruye cuando se pidió conservarla |
| `verify_manifest.py`, `.gitignore` | Los directorios nuevos fuera del manifiesto y de git |

**No se tocó NSLink.** Ni su código, ni sus pruebas, ni su corpus. Lo único que se escribe en su
árbol es el buzón —y los logs dentro de él— y la línea `Estado:` de las peticiones que nos dirigen
en `nsc_actions\`.

---

## 8. Qué midió, la primera vez que corrió de verdad

| | ctest | 84 trabajos | triage | manifiesto |
|---|---|---|---|---|
| **lld-link** | **243/243** | 84 OK | OK | OK |
| **NSLink** | **238/243** | 84 OK | OK | OK |

`nsc` verde. Los cinco fallos de NSLink son **uno**: todos cuelgan de la biblioteca de importación
—`stage46.native-libraries` la produce, y `stage47`, `stage48_1`, `stage48_3` y `stage48_4` intentan
enlazar anfitriones de C y C++ contra ella—.

**Y es exactamente lo que esta infraestructura existe para ver.** Los 84 trabajos pasan con los
**dos** enlazadores: si la segunda pasada hubiera sido «las fixtures otra vez», el reporte habría
dicho *84 OK, 0 regresiones* y NSLink habría quedado en verde con un defecto real dentro. El
detalle, en el doc 96 §2.

---

## 9. Lo que sigue

**`Process.Run` (SHR-032) sigue siendo el bloqueo duro del Bloque 4**, y que NSLink sea nuestro no
lo cambia: sea `lld-link.exe` o `nslink.exe`, `nsc` sigue haciendo `CreateProcessW`, y un
`nsc-next` escrito en NativeSharp necesita crear procesos hijos para lanzar cualquiera de los dos.

**Lo que sí cambia es a medio plazo, y es lo que NSL-32 y el milestone CC1 apuntaban:** si `nsc`
deja de *lanzar* un enlazador y enlaza **en proceso** contra `NativeLinkCore`, `Process.Run` se cae
del camino crítico del auto-hospedado. Eso ya no es «escribir un enlazador» —está escrito— sino
exponerle una entrada con ABI de C y llamarla. Es una decisión de arquitectura, no un bloque de
biblioteca, y no es mía.
