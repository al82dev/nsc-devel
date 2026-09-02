# 95 — Infraestructura de dos enlazadores: el pipeline ENTERO, dos veces

**Fecha:** 2026-09-02
**Qué añade:** que **una** corrida ejecute **la batería completa dos veces** —el mismo compilador,
un enlazador cada vez— y deje **dos reportes autocontenidos que no comparten un solo número**, más
el material de los fallos del enlazador en el árbol de quien lo desarrolla.
**Estado:** infraestructura entregada. **No ejecutada**: correrla es Windows, y desde aquí no hay
shell en esa máquina. Los números los pone `all.bat`.

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

**`all.bat` borra `build-clang\` al arrancar.** Es LA corrida, no una iteración: lo que mide tiene
que salir de fuentes, no de `.obj` que sobrevivieron a saber de qué ronda. Y hace cierto por
construcción lo de abajo — la pasada 1 configura y construye desde cero, la 2 no reconstruye nada,
así que las dos miden literalmente el mismo `nsc.exe`. Para iterar están `quick` y `norebuild`, que
conservan el árbol porque para eso existen.

**El compilador se construye UNA sola vez.** `nsc.exe` lo produce CMake con clang-cl y `lld-link`,
y eso no depende de `NSC_LLD_LINK`: NSLink nunca construye a nsc, **construye lo que nsc emite**.
Lo que se ejecuta dos veces es todo lo que *invoca* a `nsc.exe` —ctest, las 84 fixtures, el triage—
que es donde el enlazador entra en juego. Por eso la pasada 2 llama al checklist en modo
`norebuild`: no es un ahorro, es la garantía de que **los dos reportes miden el mismo binario**. Si
se reconstruyera en medio, una diferencia entre las dos pasadas podría venir del compilador y no
del enlazador, y la comparación no diría nada.

```
all.bat
  PASADA 1 -- lld-link                                  EL VEREDICTO OFICIAL
    NSC_LLD_LINK vacia
    run_windows_checklist.bat      BUILD + install + ctest 243 + 84 casos + triage
    verify_manifest.py             -> _logs\06_manifest.log
    report.py                      -> _logs\REPORTE.txt        RC_LLD
    move _logs -> _logs_lld                       (el checklist borra _logs al arrancar)

  PASADA 2 -- NSLink                                    LA MISMA BATERIA
    NSC_LLD_LINK=<nslink.exe>
    NSC_KEEP_LINK_TEMPS=%CD%\_logs\linktemps
    run_windows_checklist.bat norebuild
                                   sin build: lib\ + ctest 243 + 84 casos + triage
    verify_manifest.py             -> _logs\06_manifest.log
    report.py                      -> _logs\REPORTE.txt        RC_NSLINK
    move _logs -> _logs_nslink

  move _logs_lld -> _logs                       (la oficial vuelve a llamarse _logs)
  nslink_pipeline.py --compare                  compara las dos, NO compila nada
  veredicto combinado en consola
  exit /b %RC_LLD%
```

Los dos directorios tienen **la misma forma**, archivo por archivo:

```
SUMMARY.txt  01_build.log  02_ctest.log  03_fixtures.log  04_examples.log
05_triage_run.log  06_manifest.log  10_triage.log  TRIAGE.txt
cases\  bin\  REPORTE.txt
```

Así se leen igual, se comparan línea a línea, y **ningún número de uno puede colarse en el otro**.

### 3.1 Decisiones, y ninguna es de estilo

**Por qué se mueve el directorio en vez de escribir en dos sitios.** `run_windows_checklist.bat`
hace `rmdir /s /q _logs` en su línea 58, al arrancar. Es correcto —un reporte con logs de dos
corridas mezcladas es peor que no tener reporte— pero significa que la segunda pasada **borra la
primera**. Renombrar el directorio entero antes de lanzar la segunda es instantáneo en el mismo
volumen y no obliga a tocar el checklist, que es de la corrida oficial y no debe cambiar por esto.
También es la razón por la que la versión anterior llamaba al checklist una sola vez: entonces no
había forma de correrlo dos veces sin perder la primera.

**El modo de la pasada 2 se deriva del de la 1, no se copia.** `full` (o sin argumento) → la 2 va
en `norebuild`, que es la misma cobertura menos el paso que no depende del enlazador: sincroniza
`lib\`, corre ctest, los 84 trabajos y el triage. `quick` se propaga tal cual —su build es
incremental sobre el árbol que la pasada 1 acaba de dejar al día, o sea segundos y ningún `.obj`
distinto; no existe un modo «quick sin build» y **inventarlo aquí daría dos pasadas con cobertura
distinta, que es lo único que no se vale**. `build` no ejecuta nada, así que no hay pasada 2.

**Entre pasada y pasada se comprueba la caché de CMake, y si no está sana NO HAY PASADA 2.**
`run_windows_checklist.bat` mira `build-clang\CMakeCache.txt` al arrancar y, si le falta
`CMAKE_CXX_COMPILER_WORKS:INTERNAL=1` —la señal de que la configuración llegó al final—, **borra el
árbol de build y fuerza `full`**. Para él es lo correcto: correr la matriz contra un árbol de build
que acaba de desaparecer sería peor. Para esto es la ruina, porque en la pasada 2 significa
reconstruir el compilador, y entonces los dos reportes miden dos `nsc.exe` distintos.

Pasó en la corrida del 2026-09-02: la pasada 2 imprimió *«build-clang\ quedo de una configuracion
que no termino. Se borra. Modo forzado a `full`»*, reconstruyó entero, y el `ctest` rojo que salió
después no se puede atribuir a nadie — puede ser de NSLink o de la reconstrucción. **Un reporte que
falta es un dato; un reporte que compara dos compiladores distintos es una mentira**, así que
`all.bat` mira la caché él mismo después de la pasada 1 y, si está a medias, omite la pasada 2
diciendo por qué y remitiendo a `_logs\01_build.log`.

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
command`, **saltó a la limpieza del principio —que borra `_logs_lld` y `_logs_nslink`—** y volvió a
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

* `.gitignore` — `/_logs_lld` y `/_logs_nslink`, junto a `/_logs`.
* `verify_manifest.py` — `_logs_nslink` en `EXCLUDED_DIRECTORIES`. **Hay que nombrarlo aparte**:
  la comparación es por nombre exacto de componente de ruta, no por prefijo, y `_logs_nslink` no
  es `_logs`. Sin esa línea entrarían al manifiesto los `.obj`, `.exe` y los temporales de enlace
  de la segunda pasada, y cambiarían en cada corrida.
* `_logs_lld` existe sólo **durante** la corrida: al final vuelve a ser `_logs`. Si queda uno de
  ayer, `all.bat` lo borra al arrancar — restaurarlo al final de hoy sería el peor resultado
  posible de todos los posibles.

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
| `all.bat` | **Reescrito.** Las dos pasadas completas, el snapshot de directorios, la limpieza de variables, el veredicto combinado. **Sin bloques `if (...)`**: dentro de un bloque cmd.exe exige escapar `(`, `)` y `>`, y una omisión convierte un `echo` en una redirección silenciosa. Con etiquetas y `goto` no hay nada que escapar |
| `nslink_pipeline.py` | Modo `--compare`: lee las dos pasadas ya hechas, compara y llena el buzón, **sin compilar** |
| `run_cases.py` | `with_link_temp_subdirectory()` — un subdirectorio de temporales por trabajo. No hace nada si `NSC_KEEP_LINK_TEMPS` no está puesta |
| `src/Linker/LldLinker.cpp` | `keepLinkTemporaries()`; los cuatro sitios de limpieza del `.rsp` y el `.log` quedan bajo esa guarda |
| `src/Linker/WindowsExecutableBuilder.cpp` | `linkTemporaryRootOverride()`; `TemporaryDirectory` no se destruye cuando se pidió conservarla |
| `.gitignore` | `/_logs_lld` y `/_logs_nslink` |
| `verify_manifest.py` | `_logs_nslink` en `EXCLUDED_DIRECTORIES`, con el porqué del nombre exacto |

**No se tocó NSLink.** Ni su código, ni sus pruebas, ni su corpus. Lo único que se escribe en su
árbol es el buzón, y la línea `Estado:` de las peticiones que nos dirigen en `nsc_actions\`.

---

## 8. Lo que esto NO dice todavía

**Nadie ha corrido esta versión.** La infraestructura está escrita y probada en seco —las etiquetas
de `all.bat` resuelven todas, el fichero es CRLF y ASCII, los scripts compilan y su comportamiento
se probó con árboles falsos— pero la corrida de verdad es Windows y desde el contenedor no hay
forma de lanzarla.

Hasta que `all.bat` corra **en una ventana limpia**, no se sabe si NSLink enlaza lo que el
compilador emite hoy. Y el 84/84 anterior no lo dice, por dos motivos independientes que se
descubrieron después:

* La referencia contra la que se comparó **pudo ser también NSLink**, por la variable heredada.
* El `nslink.exe` instalado entonces era del **27 de agosto**, anterior al Bloque 2 y al Bloque 3.
  Lo notó el propio worker de NSLink comparando los `mtime` de los tres ficheros de esa tarde
  (`markdowns/05_2026-09-02_contrato_con_nsc.md`), no nosotros.

Un verde que no se sabe de qué binario es no es un verde.

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
