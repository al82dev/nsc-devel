# 53.2b — NativeSharp Environment (variables): GetEnvironmentVariable, SetEnvironmentVariable

## 0. Historial de esta entrega

Continuación directa de Stage 53.2 ronda 1 (`53_2_nativesharp_environment.md`), donde
`GetEnvironmentVariable`/`SetEnvironmentVariable` quedaron explícitamente fuera del alcance por
un motivo técnico concreto: ninguna operación núcleo existente hasta ese momento devolvía un
`NativeSharp.Core.String` cuya longitud se calculara en **tiempo de ejecución** — todas
precomputaban la longitud en tiempo de compilación. Se documentó como diseño investigado, no
implementado, para una ronda futura.

Alberto dio la señal de encarar esa ronda con un simple **"53.2b"**. Esta entrega es esa ronda:
implementa el mecanismo de asignación dinámica de strings desde el runtime que la ronda anterior
identificó como necesario, y en el proceso **encontró y corrigió un bug real** en la
infraestructura de linking compartida — el tipo de hallazgo que justifica, en retrospectiva, por
qué separar esta pieza en su propia ronda fue la decisión correcta.

## 1. Objetivo de esta ronda

1. `Environment.GetEnvironmentVariable(string name) -> string`: leer el valor de una variable de
   entorno del proceso, con longitud determinada recién en tiempo de ejecución.
2. `Environment.SetEnvironmentVariable(string name, string value) -> void`: escribir una
   variable de entorno del proceso.

## 2. Punto de partida verificado

`ctest` con el árbol tal cual quedó al cerrar Stage 53.2 ronda 1: **212/212**.

## 3. El mecanismo nuevo: asignar un `String` de NativeSharp desde el runtime

Esta es la pieza de diseño central de la ronda, ya anticipada (pero no implementada) en
`53_2_nativesharp_environment.md` sección 3. `GetEnvironmentVariableA` (la variante ANSI de
Win32 — NativeSharp usa `char` de 1 byte, no UTF-16, ver `integralBitWidth()` en
`ProjectMethodBodyModel.cpp`, así que ANSI es la elección honesta, no UTF-16) sigue un patrón de
dos llamadas:

1. Llamar con `lpBuffer = NULL, nSize = 0` — Win32 responde con el tamaño de buffer necesario
   (incluyendo el terminador nulo), o `0` si la variable no existe.
2. Alocar un buffer de exactamente ese tamaño y llamar de nuevo, esta vez con un buffer real.

El wrapper de runtime (`buildEnvironmentGetVariable`, `WindowsRuntimeBuilder.cpp`) hace las dos
llamadas, y en el medio **aloca un `NativeSharp.Core.String` real** usando el mismo mecanismo
crudo (`GetProcessHeap` + `HeapAlloc`) que ya usa `buildAllocate` para cualquier otra
asignación del runtime — no hay ningún camino "mágico" nuevo, es el mismo heap que usa todo lo
demás. El tamaño alocado es `8 + tamañoRequerido` (8 bytes para la cabecera `{ i64 length; ... }`
más el tamaño que Win32 pidió, que incluye un byte de más para su propio terminador nulo — ese
byte de más queda como relleno inofensivo dentro del array de datos del `String`, nunca leído
por nada porque el campo `length` no lo cuenta).

## 4. El problema de la falta de terminador nulo, y la solución compartida

Ni `GetEnvironmentVariableA` ni `SetEnvironmentVariableA` aceptan el layout nativo de
NativeSharp (`{ i64 length; i8 data[length] }`, **sin terminador nulo**) — ambas son API de C
clásicas que esperan `LPCSTR` (una cadena terminada en `\0`). Como esta necesidad se repite para
los tres argumentos de string que estas dos operaciones reciben en conjunto (`name` de Get,
`name` y `value` de Set), se factorizó en una única rutina compartida,
`buildNullTerminatedCopy`, que:

1. Lee la longitud del `String` de NativeSharp (los primeros 8 bytes).
2. Reserva un buffer en el **stack** (no en el heap — este buffer solo necesita vivir durante la
   llamada a Win32 que ocurre inmediatamente después, así que no hace falta pagar el costo ni la
   complejidad de liberarlo) de tamaño `longitud + 1`, usando `LLVMBuildArrayAlloca` (a
   diferencia de un `alloca` común, esta variante SÍ acepta un tamaño calculado en tiempo de
   ejecución, no solo una constante de compilación).
3. Copia los bytes uno por uno con un bucle explícito en LLVM IR.
4. Escribe un byte `0` al final.

El bucle de copia sigue el mismo patrón (`alloca` de un índice + bloque de comparación + bloque
de cuerpo + `br` de vuelta) que ya usaba `buildPuts` (mismo archivo) para **contar** la longitud
de un string C — la única diferencia es que este bucle nuevo, en vez de solo comparar y avanzar,
también copia un byte en cada vuelta.

## 5. El bug real de esta ronda: `memcpy` no definido, solo a partir de -O2

Con el bucle de copia funcionando correctamente a `-O0` (verificado manualmente, ver la sección
6), la suite completa (`ctest`) mostró **11 tests fallando** que no tenían absolutamente nada
que ver con `Environment` — `nsc.stage45.linker-pe-eh`, `nsc.stage47.host-c-abi`,
`nsc.stage48_1` a `nsc.stage48_6`, `nsc.driver`, `nsc.emit-exe-o2`. El mensaje de error, idéntico
en todos:

```
lld-link: error: undefined symbol: memcpy
>>> referenced by .../runtime.obj:(__nativesharp_environment_get_variable)
>>> referenced by .../runtime.obj:(__nativesharp_environment_set_variable)
```

### Causa raíz

LLVM tiene un pase de optimización llamado **LoopIdiomRecognize**, que corre a `-O1`/`-O2` (no a
`-O0`), y que reconoce patrones de bucle comunes y los reemplaza por su forma canónica más
eficiente — copiar bytes uno por uno en un bucle es exactamente el patrón que reconoce como "esto
es un `memcpy`", y lo reescribe como una llamada al intrínseco `@llvm.memcpy`. Eso es, en
general, una optimización deseable — pero esa llamada intrínseca, al bajar a código máquina real,
se convierte en una llamada al símbolo `memcpy` de la biblioteca C.

El compilador YA tenía infraestructura para esto: `WindowsExecutableBuilder.cpp` escanea el
texto LLVM del **módulo de programa** (el código del usuario, compilado por
`ProjectOopLlvmLowerer`) buscando la cadena `"@llvm.memcpy"` — si la encuentra, agrega
`"memcpy"` a la lista de símbolos que hay que importar de `ucrtbase.dll` al momento de linkear.
Este mecanismo ya funcionaba correctamente para `String.Concat` (que SÍ usa el intrínseco
`llvm.memcpy` real, no un bucle a mano, desde Stage 51).

El bug: ese escaneo **solo miraba el texto del módulo de programa, nunca el del módulo de
runtime** (`WindowsRuntimeBuilder.cpp` construye un módulo LLVM completamente separado, que se
compila y linkea de forma independiente — ver `53_1_nativesharp_time.md` sección 4 para la
explicación de por qué existen dos módulos). Mi bucle de copia a mano, escrito dentro del módulo
de runtime, nunca antes había existido ahí — nada en el runtime, antes de esta ronda, tenía un
bucle que copiara bytes. Cuando LoopIdiomRecognize lo convirtió en una llamada a `@llvm.memcpy`
**dentro del módulo de runtime**, el escaneo (que solo miraba el módulo de programa) nunca lo
vio, y el símbolo `memcpy` nunca se agregó a la lista de importación de `ucrtbase.dll`.

El motivo por el que esto rompió tests que no tienen NADA que ver con `Environment` es
estructural, no una coincidencia: **el módulo de runtime se construye completo, siempre, para
todo ejecutable** — `buildInternal()` construye las tres funciones de `Environment` (y todo lo
demás) sin importar si el código del usuario las llama o no. Así que el bug afectaba a
**cualquier ejecutable compilado a `-O1`/`-O2` o superior**, tuviera o no relación con variables
de entorno, en el momento en que `Environment.ns` se agregó a la biblioteca estándar (que TODO
proyecto que referencia `NativeSharp.Core` compila transitivamente).

### El fix

Se corrigió el escaneo para que mire el texto de **ambos** módulos, concatenados, en los dos
lugares donde este patrón existía (`WindowsExecutableBuilder.cpp`, para ejecutables; y
`WindowsLibraryBuilder.cpp`, para DLLs — el mismo bug, con la misma causa raíz, estaba latente
también ahí, aunque ningún test lo había disparado todavía porque `buildDynamic()` no había sido
ejercitado con una función de runtime que produjera un bucle idiom-recognizable hasta ahora):

```cpp
const std::string programText = programModule.text() + runtimeModule->text();
```

Es el fix mínimo y honesto: no asume de antemano en qué módulo puede aparecer un futuro bucle
idiom-recognizable, simplemente mira los dos módulos que efectivamente se linkean juntos en el
ejecutable o DLL final.

## 6. Verificación manual del fix, antes y después

Antes del fix, reproducido directamente (no solo visto en `ctest`):

```
$ nsc --emit-exe .../llvm_optimization_features.ns -O2
nsc: error: Windows executable generation failed: lld-link failed with process status 256:
lld-link: error: undefined symbol: memcpy
>>> referenced by /tmp/.../runtime.obj:(__nativesharp_environment_get_variable)
>>> referenced by /tmp/.../runtime.obj:(__nativesharp_environment_set_variable)
```

Después del fix, un programa de prueba ejercitando el round-trip completo de
`SetEnvironmentVariable`/`GetEnvironmentVariable`, compilado y corrido bajo Wine en **ambas**
configuraciones:

```
$ nsc --emit-project-oop-exe Probe.nsproj -O0 -o Main_O0.exe   # compila OK
$ wine64 Main_O0.exe; echo $?
77
$ nsc --emit-project-oop-exe Probe.nsproj -O2 -o Main_O2.exe   # compila OK (antes: undefined symbol)
$ wine64 Main_O2.exe; echo $?
77
```

(`77` es el código de salida que el programa de prueba pasa a `Environment.Exit(77)` después de
confirmar el round-trip — ver sección 7 para el detalle de qué confirma exactamente.) Este
resultado es la razón por la que el test dedicado de esta ronda (a diferencia de todos los
anteriores en Stage 53) prueba **ambos** niveles de optimización, no solo `-O0` — ver sección 8.

## 7. Decisiones de diseño de la superficie

- **`GetEnvironmentVariable` devuelve `""` (no null) si la variable no existe.** Simplificación
  deliberada: distinguir "no existe" de "existe y está vacía" requeriría un tipo string nullable
  o el patrón `TryGetEnvironmentVariable(name, out value)` que
  `HANDOVER_STAGE52_TO_STAGE53.md` menciona como recomendación para APIs de estilo Try — ninguno
  de los dos existe todavía en este compilador por un motivo real de peso, no se quiso introducir
  de apuro solo para esta operación. Documentado en el comentario de cabecera de
  `Environment.ns`.
- **No hay forma de BORRAR una variable en esta ronda.** En Win32 real, borrar una variable es
  `SetEnvironmentVariableA(name, NULL)` — un puntero nulo, no un string vacío. Como
  `SetEnvironmentVariable(name, "")` en NativeSharp SÍ construye un string vacío real (no un
  puntero nulo), llamar a esa función con un valor vacío establece la variable a vacío, no la
  borra — mezclar ambos casos hubiera sido una simplificación incorrecta, no solo incompleta, así
  que se documentó como alcance explícitamente fuera en vez de implementarse mal.
- **No se distingue una condición de carrera entre las dos llamadas a Win32.** Entre el primer
  `GetEnvironmentVariableA` (que pide el tamaño) y el segundo (que llena el buffer), otro
  proceso — o, cuando exista Stage 54, otro thread — podría cambiar la misma variable. Esto no se
  protege en esta ronda, consistente con el alcance actual de un compilador sin threading
  todavía.
- **El valor de retorno `BOOL` de `SetEnvironmentVariableA` se descarta.** Misma justificación
  documentada que `buildOutParamI64Call` en Stage 53.1: el único fallo realista (el bloque de
  variables de entorno del proceso quedándose sin memoria) no es algo que esta ronda intente
  exponer como error visible desde NativeSharp.

## 8. Verificación real, sin inventar cifras

- El fix del bug de `memcpy` se verificó de dos formas independientes: reproduciendo el error
  directamente con `nsc --emit-exe ... -O2` antes del fix, y confirmando su ausencia después;
  y corriendo la suite completa tres veces seguidas tras el fix (**212/212**, **212/212**,
  **212/212** — antes de agregar el nuevo test de esta ronda) para descartar que fuera un
  problema intermitente en vez de una causa raíz real.
- `nsc_stage53_2b_environment_variables_tests`: **PASS, 41/41 chequeos**, incluyendo ejecución
  real bajo Wine del programa empaquetado en **ambas** configuraciones de optimización (`-O0` y
  `-O2`) — la primera vez en todo Stage 53 que un test dedicado prueba explícitamente los dos
  niveles, precisamente porque el bug de esta ronda solo se manifestaba en uno de ellos.
- El programa de verificación empaquetado (`examples/project_stage53_2b/Main.ns`) cubre: el caso
  de variable inexistente (string vacío, no crash); un round-trip básico de Set+Get verificado
  por longitud Y por contenido (`.Equals`, no solo `.Length`); sobreescribir con un valor más
  largo (confirma que la segunda llamada a `GetEnvironmentVariableA` vuelve a consultar el
  tamaño real, no reutiliza el de una lectura anterior); sobreescribir con un valor más corto
  (confirma que no queda basura del valor largo anterior); un nombre de variable largo (confirma
  que `buildNullTerminatedCopy` funciona igual de bien para el argumento `name` que para
  `value`); y `Environment.Exit(0)` como el propio mecanismo de verificación final.
- Suite completa final: **213/213** (`ctest --output-on-failure -j$(nproc)`), confirmado estable
  en corridas repetidas.

## 9. Cierre formal

Stage 53.2b queda cerrada en este estado: 2 operaciones núcleo nuevas
(`Environment.GetEnvironmentVariable`/`SetEnvironmentVariable`), un mecanismo nuevo de
asignación dinámica de `NativeSharp.Core.String` desde el runtime (reutilizable por cualquier
operación núcleo futura que necesite lo mismo — por ejemplo, un futuro `GetCommandLineArgs`), un
bug real de infraestructura de linking encontrado y corregido de raíz (no parcheado ni evitado),
un programa de ejemplo empaquetado con 8 chequeos, y un test dedicado que prueba dos
configuraciones de optimización en vez de una. Suite completa: 213/213.

Con esto, "Environment" (el segundo tercio de Stage 53) queda funcionalmente completo para el
alcance que este proyecto necesita hoy: hechos del proceso (53.2), variables de entorno (53.2b).
Queda pendiente, todavía deliberadamente fuera, `GetCommandLineArgs` (necesita además parsear el
quoting de línea de comandos de Windows — un problema ya conocido en este proyecto, doc #15).
Quedan Stage 53.3 (Filesystem/Paths) y Stage 54 (threading, con especial cuidado). Ninguna arranca
sin señal explícita de Alberto.
