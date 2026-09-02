# Handover: qué te manda `nsc`, y cómo trabajar en paralelo con esto

Escrito desde el árbol de **nsc** (`A:\nsc-devel`) para quien desarrolla **NSLink**
(`A:\nslink-devel`). Copia de este mismo archivo queda en `docs\` de tu árbol; el contenido vivo
—los fallos de la última corrida— está en el buzón:

```
A:\nslink-devel\_inbox_nsc\
```

**Revisión 2 (2026-09-02, tarde).** La revisión 1 describía una corrida del pipeline con `lld-link`
más una segunda vuelta de **sólo las 84 fixtures** con NSLink. Eso ya no es así, y el cambio te
afecta: ahora se ejecuta **el pipeline entero dos veces**. Si tenías notas de la revisión 1, la
sección 1 y la 3 son las que cambian.

---

## 1. Qué pasa ahora en cada corrida de nsc

`all.bat` ejecuta **la batería completa dos veces**, con el mismo compilador y un enlazador cada
vez:

```
PASADA 1   lld-link     construye nsc + ctest (243) + 84 trabajos + triage + manifiesto -> _logs\
PASADA 2   nslink.exe   TODO ESO OTRA VEZ, sin reconstruir nsc                  -> _logs_nslink\
```

**`nsc.exe` se construye una sola vez, con clang-cl.** NSLink no construye el compilador:
construye **lo que el compilador emite**. Lo que se ejecuta dos veces es todo lo que *invoca* a
`nsc.exe` — las 243 pruebas de `ctest` (muchas enlazan de verdad), los 84 trabajos (42 proyectos ×
`-O0`/`-O2`), el triage de bugs y el manifiesto. La pasada 2 va en modo `norebuild` justamente para
que **los dos reportes midan el mismo binario de nsc**: si se reconstruyera en medio, una
diferencia entre pasadas podría venir del compilador y no de ti.

No son «las fixtures otra vez». O sea: **si algo que emites rompe una prueba unitaria o el triage,
ahora se ve**; antes no se veía, porque esas etapas sólo se medían con lld-link.

Los dos directorios tienen la **misma forma** —los mismos nombres de log, el mismo `REPORTE.txt`—
para que se lean igual y se comparen línea a línea. **No comparten ningún número.**

La sustitución no recompila nada: `LldLinker::discover()` en nsc lee `NSC_LLD_LINK` antes de buscar
`lld-link` en el PATH, y `all.bat` la fija —y la **limpia**— en cada pasada.

Después, `nslink_pipeline.py --compare` compara **caso por caso** las dos pasadas ya hechas y
reemplaza tu buzón. Ese paso no compila nada.

---

## 2. Qué hay en el buzón, y qué significa cada cosa

```
_inbox_nsc\
    README.md            generado en cada corrida: conteos, cómo reproducir
    REPORTE.txt          el detalle, con las secciones separadas
    fallos\<caso>.log    la salida COMPLETA de nsc para ese caso
    fallos\<caso>.obj    el COFF que NSLink no supo enlazar
    corpus-oferta\       seis objetos representativos, aunque no falle nada
```

**El mensaje de NSLink está dentro del `.log`.** No hace falta buscarlo en otro sitio: `nsc`
redirige tu stdout y stderr a un fichero temporal, lo lee, y lo pega dentro de su propio error
(`"... failed with process status N:\n<tu diagnóstico>"`), que sale por el stderr de `nsc`. El
pipeline captura stderr entero.

**El reporte separa tres cosas que no son lo mismo**, y la distinción es la parte útil:

| Sección | Qué es |
|---|---|
| **REGRESIONES** | Pasaba con lld-link y falla con NSLink. **Esto es tuyo.** |
| Fallaba ya con lld-link | No es culpa de NSLink. Va aparte a propósito: mezclarlo escondería lo de arriba. |
| Falla y no está en la referencia | La corrida de referencia no tenía ese caso. Normalmente significa que las tablas de casos se separaron. |

---

## 3. Reproducir un fallo: ahora sí tienes la invocación completa

**Esto responde a `nsc_actions/001_objeto_de_runtime.md`.** Pediste el objeto de runtime y la lista
de bibliotecas de importación de la **misma** invocación. Están, y también el fichero de respuesta,
que en la revisión 1 te dije que no se podía entregar.

Se resolvió en el compilador, con una variable de entorno nueva: **`NSC_KEEP_LINK_TEMPS`**. Si está
puesta, `nsc` usa su valor como raíz del directorio temporal del enlace y **no lo borra** al
terminar; `LldLinker` tampoco borra el `.rsp` ni el `.log`. Apagada por defecto, porque en la
corrida oficial eso sería basura. La **pasada 2 la pone siempre**, así que:

```
_logs_nslink\linktemps\<caso>_<O0|O2>\
    program.obj
    runtime.obj                 el que nsc fabrica al vuelo
    *.lib                       las bibliotecas de importacion de ESA invocacion
    <salida>.link.rsp           el fichero de respuesta, tal cual se le paso al enlazador
    <salida>.link.log
```

Un subdirectorio por trabajo, porque corren en paralelo. Reproducir un fallo es entrar ahí y:

```bat
nslink @<salida>.link.rsp
```

Sin `nsc`, sin el pipeline, sin adivinar nada. El `fallos\<caso>.obj` del buzón sigue estando para
el caso simple:

```bat
nslink --link fallos\<caso>.obj -o prueba.exe --entry __nativesharp_start
```

Los argumentos fijos, copiados de `linkConsoleExecutable`, por si te sirve leerlos sin abrir el
`.rsp`:

```
/nodefaultlib
/entry:__nativesharp_start
/subsystem:console
/machine:x64
/dynamicbase
/nxcompat
/highentropyva
/out:<salida.exe>
<programa.obj>
<runtime.obj>            (temporal, lo genera nsc en cada enlace)
<bibliotecas de importacion...>
```

Se invoca como `nslink @<fichero-de-respuesta>`, con stdin en NUL y stdout+stderr redirigidos.

`linktemps\` está dentro de `_logs_nslink\`, en el árbol de nsc, no en el tuyo: son decenas de
megabytes por corrida y se reemplazan enteros. Si necesitas conservar uno, cópialo fuera.

---

## 4. `corpus-oferta\` — lo que puedes hacer aunque no falle nada

Tu `corpus/README.md` dice lo que le falta al corpus, y es exactamente lo que nsc produce:

> **LLVM / clang-cl**: no disponible en el entorno donde se construyó esta etapa. Su ausencia
> importa más de lo que parece: LLVM emite COMDAT, `.drectve` y símbolos débiles, ninguno de los
> cuales aparece en los objetos de NSASM de este directorio.

Tienes tres objetos en `corpus/llvm/`. El buzón te deja seis más en cada corrida, elegidos porque
cada uno ejercita una parte distinta del enlazador:

| Objeto | Qué ejercita |
|---|---|
| `c1_14_callback_exception_boundary_O0` | Excepción cruzando un callback de C: `.pdata`/`.xdata` → `PeExceptionTable` |
| `c1_13_two_libraries_same_symbol_O0` | Dos bibliotecas con el mismo símbolo: resolución de símbolos pura |
| `cc1_c16_enum_layout_O0` | Tamaños y offsets que el enlazador respeta tal cual |
| `cc1_c18_stackalloc_hardening_O0` | `__chkstk` y el prólogo de marco |
| `cc1_c23_views_and_lifetimes_O2` | El más grande del árbol, a `-O2`: genéricos monomorfizados, COMDAT, secciones plegables |
| `project_stage56_c1_O0` | Interop completo: importaciones, exportaciones, ABI de C |

**Si alguno te sirve como caso permanente, promoverlo a `corpus/llvm/` es tu decisión, no
nuestra.** Por eso van en un directorio aparte y no se escriben en tu corpus.

---

## 5. Lo que nsc NO va a hacer

* **No tocamos NSLink.** Ni su código, ni sus pruebas, ni su corpus. Lo único que escribimos en tu
  árbol es `_inbox_nsc\` —que no forma parte de tu construcción y se puede borrar entero— y la
  línea `Estado:` de las peticiones de `nsc_actions\` que nos dirigís, como pide tu README.
* **Un fallo de NSLink no pone en rojo la corrida de nsc.** El código de salida de `all.bat` es el
  de la **pasada 1**, y sólo el de la pasada 1. Poner tu fallo en nuestro veredicto diría que nsc
  está roto cuando lo que falla es otro componente.
* **No decidimos qué entra en tu corpus** ni abrimos incidencias en tu árbol. Dejamos material; el
  triage es tuyo.

---

## 6. Si algo del buzón está mal, o falta

Lo que llega ahí lo genera `A:\nsc-devel\nslink_pipeline.py`. Dos cosas que conviene saber:

* **`fallos\` se reemplaza entero en cada corrida.** Un buzón donde se acumulan los fallos de tres
  corridas es peor que no tener buzón: nadie sabe cuál sigue vivo. Si necesitas conservar uno,
  cópialo fuera.
* **El README del buzón dice qué NO se copió** cuando algo se queda fuera por el tope de tamaño
  (64 MB). No se llena tu árbol en silencio.

Si hace falta otro artefacto para diagnosticar —el `.exe` parcial, un dump, otra cosa— se puede
añadir: es cuestión de decirlo, por `nsc_actions\` como hiciste con el runtime.

---

## 7. Sobre el 84/84 anterior: no lo cites como verde del árbol de hoy

Dos motivos independientes, y cada uno basta:

1. **`nslink.exe` era el del 27 de agosto**, anterior al Bloque 2 y al Bloque 3 de nsc. Lo notaste
   tú, comparando los `mtime` de los tres ficheros de esa tarde
   (`markdowns/05_2026-09-02_contrato_con_nsc.md`, §B). Tenías razón y no lo habíamos visto.
2. **La corrida de referencia pudo ser también NSLink.** `NSC_LLD_LINK` se heredaba de la ventana:
   si venía puesta, la pasada «oficial» enlazaba con NSLink sin decirlo, y entonces «0 regresiones»
   no significa nada porque se comparó una cosa consigo misma. Se vio como una contradicción —cinco
   pruebas de biblioteca de importación en rojo **y** «0 regresiones» al mismo tiempo—. `all.bat`
   ahora limpia la variable en cada pasada y avisa si venía puesta.

La corrida nueva —la que estás esperando en tu §F— es la primera que mide el compilador de hoy
contra el `nslink.exe` de hoy, y con las dos pasadas separadas de verdad.
