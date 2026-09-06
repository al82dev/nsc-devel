# 124 — Gate 1: cerrar NSC-LIMIT-005

**Fecha:** 2026-09-05
**Plan:** doc 123, «Gate 1 — cerrar NSC-LIMIT-005»
**Estado:** instrumentación validada con LLVM 23 en Windows; pendiente de la corrida completa de
la matriz contractual

---

## 1. Qué defecto cierra este gate

`tests/cc1/c38_enum_ancho` devolvía **42 a -O0 y 39 a -O2**. La causa se acotó en `b9`: `ToString()`
de un `enum : sbyte` con valor **negativo** devolvía la reserva —el nombre del tipo— después de
optimizar. El módulo verificaba, su texto era correcto, y el mismo texto pasado a `opt.exe` elegía
bien. Lo que decidía distinto era `LLVMRunPasses`.

La mitigación —normalizar la decisión a `i64`— dejó el programa correcto y **la pregunta sin
responder**: no había forma de decir con qué LLVM, con qué pipeline ni por dónde iba la corrida
cuando el valor dejó de ser el bueno. `LLVMRunPasses` es una sola llamada opaca: entra un módulo y
sale cambiado.

Este gate no vuelve a arreglar el enum. Hace dos cosas: **poder ver dentro de esa llamada**, y
**fijar el arreglo para que no se pueda deshacer en silencio**.

---

## 2. Lo que se observa, y lo que deliberadamente no se afirma

`LlvmOptimizationObservation` recoge, por corrida del optimizador:

| dato | por qué está |
|---|---|
| versión de LLVM **cargada** | va primero: sin ella, dos corridas que difieren no se pueden ni comparar |
| pipeline (`default<O2>`) y nivel del TargetMachine | son dos cosas distintas y pueden no coincidir |
| triple y data layout **del módulo** | leídos tras `configureModuleForTarget`; el IR de `--dump-project-oop-llvm` **no** es el que ve la API C, así que reproducir la llamada en `opt.exe` con otro triple no reproduce nada |
| verificación antes **y** después | un módulo que entra inválido y uno que sale inválido son fallos distintos |
| último pase y última unidad observados | lo único que la API C ofrece para saber por dónde iba |
| repetición máxima del par (pase, unidad) | señal de posible falta de convergencia |
| segundos | señal de tiempo excesivo |

**Lo que NO hace, y conviene decirlo antes que lo que sí:** no nombra un culpable. El log de LLVM
prueba **orden de ejecución**, no causalidad. Que un pase sea el último visto antes de un fallo no
lo convierte en la causa: puede haber roto algo un pase anterior y éste haber sido el primero en
tropezar. Por eso los campos se llaman «último observado» y el texto que sale al usuario también.
Hay una comprobación que falla si la palabra «culpable» aparece en el reporte.

Atribuir un fallo a un pase con certeza exigiría `PassInstrumentationCallbacks`, que es **C++**.
Incluir una cabecera C++ de LLVM es una decisión de arquitectura (doc 123 §7) y no se toma como
efecto secundario de un criterio de cierre. Todo esto sale de la API **C**:
`LLVMPassBuilderOptionsSetDebugLogging` más captura en el límite del proceso.

En POSIX puede desviarse el descriptor 2 dentro del proceso. En Windows **no**: la distribución
real enlaza `LLVM-C.dll`, que lleva su propio CRT, y `_dup2` en `nsc` sólo cambia el descriptor de
`nsc`. El intento original pasó con LLVM 18 en el contenedor y falló inmediatamente con LLVM 23
en Windows. La corrección es un supervisor: crea el hijo con `stderr` dirigido a un fichero antes
de que la DLL inicialice su stream; el hijo delimita cada ventana de `LLVMRunPasses`, y el padre
separa el registro de los diagnósticos ordinarios y conserva el último pase observado.

Ejemplo real, `c38` a -O2:

```
LLVM 18.1.3; pipeline 'default<O2>'; nivel del TargetMachine O2;
triple del modulo 'x86_64-pc-windows-msvc'; data layout 'e-m:w-...';
el modulo verificaba antes y verifica despues;
ultima funcion/unidad observada '__nativesharp_rtti_is_interface (22 instructions)';
ultimo pass observado 'AnnotationRemarksPass' de 43603 observados
```

---

## 3. Observar no puede cambiar el resultado — medido, no argumentado

El registro sólo imprime y el supervisor sólo cambia dónde nace `stderr` en el hijo. Las dos cosas
son inofensivas *en teoría*, y la teoría es exactamente lo que no vale aquí.

**12/12 IR byte a byte idénticos** con y sin observación, sobre `c38`, `c44`, `c45` y `c46` × O0,
O1 y O2. Además hay una prueba que optimiza el mismo módulo dos veces y compara los dos textos en
los tres niveles.

**Coste:** ~4-6 % del tiempo de optimización a O1/O2, **0 %** a O0. Sobre el tiempo de compilación
completo del proyecto de prueba: 9.95 s → 10.25 s a O2, ~3 %.

---

## 4. Por qué el lector del registro vive fuera del optimizador

`observeLlvmPassLog` es una función libre en `LlvmOptimizationObservation.cpp`, no un detalle
privado de `LlvmOptimizer.cpp`. El motivo es que **una detección que nadie ha visto dispararse no
es una detección**: provocar en LLVM una falta de convergencia real es caro y frágil —haría falta
un módulo patológico que además siguiera siéndolo en la siguiente versión de LLVM—, así que se
prueba con registros escritos a mano.

Lo que esas pruebas fijan:

- el umbral **por los dos lados**: 64 repeticiones exactas no son sospechosas, 65 sí. Sin el caso
  de igualdad, un `>=` colado por accidente pasaría desapercibido;
- **el mismo pase sobre unidades distintas no es una repetición**. Éste es el caso que decide si la
  detección sirve para algo: LLVM corre cada pase de función sobre todas las funciones del módulo,
  así que si la clave fuera sólo el nombre del pase, cualquier compilación normal de un módulo con
  más de 64 funciones se marcaría como sospechosa y el aviso no significaría nada;
- «último» es **literalmente el último**, no el más frecuente;
- un registro que no se pudo capturar **se dice**, no se deja en blanco: una línea vacía donde
  debería ir un pase se lee como «no hubo pases», que es una afirmación distinta y falsa;
- las dos formas que tiene la línea de venir: sin unidad, y con el `\r` de un fichero abierto en
  modo texto en Windows.

### Control de mutación

Las pruebas se comprobaron rompiendo el lector a propósito. Cada mutación cae en la comprobación
que le corresponde y sólo en ella:

| mutación | comprobación que se pone roja |
|---|---|
| `>` → `>=` en el umbral | «exactly the suspicion threshold is not yet suspicious» |
| clave del contador = sólo el nombre del pase | «the same pass over different units is not a repetition» |
| se quita el recorte del `\r` | «a CRLF line does not leave a stray carriage return» |

---

## 5. El arreglo del enum estrecho, fijado

`lowerEnumName` promueve el receptor con **`zext`** a i64 y compara contra el patrón de bits
enmascarado al ancho del tipo subyacente. Con `sext` —el cambio de una letra que cualquiera haría
«limpiando» un tipo con signo— `-3` se convierte en `i64 -3` mientras el caso sigue siendo
`i64 253`, no casa ninguno, y se elige la reserva.

La prueba nueva en `ProjectOopLoweringTests.cpp` comprueba **tres capas**, y hacen falta las tres:

1. **la forma**: `zext i8 … to i64`, nunca `sext`; `switch i64`, nunca la cadena de `select`;
2. **el patrón de bits**: el caso es `i64 253`, no `i64 -3`;
3. **el resultado después de O2**: con receptor constante, el pipeline pliega la función a un `ret`
   del global correcto. Que sea el de `"MenosTres"` y no el de la reserva es la prueba de que la
   decisión sobrevive al optimizador.

La capa 1 sola describiría la implementación de hoy; la capa 3 sola podría pasar por accidente si
el plegado cambiara. Juntas dicen que la elección es correcta **y por qué** lo es.

Medido con dos mutaciones sobre `ProjectOopLlvmLowerer.cpp` — `zext`→`sext`, y escribir el caso con
el número fuente en vez de con sus bits —: **4 comprobaciones rojas cada una**, en las capas que
tocan.

---

## 6. La fixture diferencial O0/O2 — `c47_diferencial_o0_o2`

`c38`, `c39` y `b9` comprueban que el resultado es **el correcto**. Ésta comprueba que **las dos
vías coinciden**:

- la **constante** —`ConSigno.MenosTres.ToString()`—, que el optimizador puede plegar;
- la **opaca** —`PasaConSigno(ConSigno.MenosTres).ToString()`—, que no puede plegar porque el valor
  cruza una función cuyo resultado depende de `Environment.GetCommandLineArgCount()`.

El defecto de -O2 no era un valor equivocado en todas partes: era que el **optimizador decidía
distinto** que el código sin optimizar, y sólo para una constante negativa estrecha. Un programa
que comprueba el valor correcto lo detecta si falla la vía que le tocó escribir; uno que compara
las dos vías lo detecta caiga la que caiga, y además dice cuál de las dos se movió.

**Y no depende de comparar dos corridas.** El runner ya ejecuta cada fixture a -O0 y a -O2, así que
una discrepancia acabaría apareciendo como dos códigos de salida distintos. Aquí la discrepancia se
detecta **dentro de la misma corrida**, en cualquiera de los dos niveles.

Barrido: los cuatro anchos estrechos (`sbyte`, `byte`, `short`, `ushort`) con los valores que
delatan el signo (-128, -3, 200, -32768, 60000), sobre la conversión a `int` y sobre `ToString()`.
Las cadenas se comparan byte a byte con `MismosBytes`, no con el operador de igualdad, por el mismo
motivo que en `c39`: el operador es una de las cosas que podrían estar rotas.

**Medido que muerde.** Con el compilador sano, las ocho llamadas a `MismosBytes` que quedan en el
IR a -O2 reciben **el mismo global** en los dos argumentos. Con `zext` cambiado a `sext`, tres de
ellas reciben globales **distintos** —y son exactamente las tres de los enums **con signo**; las de
los enums sin signo no se mueven—. El programa devolvería 20, 22 o 26 en vez de 42. Eso es la
caracterización de `b9` reproducida por la fixture: estrecho + con signo + negativo.

---

## 7. La observación va siempre, no bajo bandera

Las **nueve** llamadas de producción al optimizador —3 en `Driver.cpp`, 2 en
`WindowsExecutableBuilder.cpp`, 4 en `WindowsLibraryBuilder.cpp`— pasan una observación, y el
mensaje de cualquier fallo lleva el reporte añadido.

En Windows `nsc.exe` es el supervisor y vuelve a ejecutar su misma línea de comandos con un
argumento interno. El hijo hace la compilación real y emite el registro delimitado; el padre
reenvía stdout, conserva solamente los diagnósticos que estaban fuera de esas ventanas y, si el
hijo falla, añade el último pase y la última unidad observados. Una corrida sana no muestra el
registro de LLVM.

No hay bandera para encenderlo porque una bandera no serviría: un fallo del optimizador que no dice
contra qué LLVM, con qué pipeline ni por dónde iba **obliga a reproducirlo** para diagnosticarlo, y
eso es exactamente lo que no se pudo hacer con `c38` —cuando el síntoma es un código de salida
distinto a -O2, la corrida que falló ya terminó—. El coste está medido (§3) y es el precio de que
el **primer** fallo sea legible.

Si ese ~3 % llegara a molestar, la salida no es una bandera de todo o nada: la mitad valiosa del
reporte (versión, pipeline, triple, data layout, verificación a los dos lados, tiempo) **no cuesta
nada**; lo que cuesta es el registro de pases. Separarlos sería el ajuste, y es una decisión de
producto, no de este gate.

---

## 8. Ficheros

**Nuevos**

- `include/nativesharp/LLVM/LlvmOptimizationObservation.h`
- `src/LLVM/LlvmOptimizationObservation.cpp`
- `include/nativesharp/LLVM/LlvmProcessTrace.h`
- `src/LLVM/LlvmProcessTrace.cpp`
- `tests/cc1/c47_diferencial_o0_o2/` (`Main.ns`, `c47_diferencial_o0_o2.nsproj`)

**Modificados**

- `src/LLVM/LlvmCApi.h` — 4 entradas: `LLVMGetTarget`, `LLVMGetDataLayoutStr`, `LLVMGetVersion`,
  `LLVMPassBuilderOptionsSetDebugLogging`
- `include/nativesharp/LLVM/LlvmOptimizer.h` — sobrecarga de 5 argumentos; la de 4 delega con
  `nullptr` y es **exactamente** equivalente
- `src/LLVM/LlvmOptimizer.cpp` — captura local en POSIX, emisión delimitada al supervisor en
  Windows, cronómetro, verificación a los dos lados y reporte añadido a cada ruta de fallo
- `src/nsc/main.cpp` — supervisor Windows; crea el hijo con `stderr` fijado desde el nacimiento,
  elimina las ventanas de instrumentación en éxito y añade el último pase observado en fallo
- `src/Driver/Driver.cpp`, `src/Linker/WindowsExecutableBuilder.cpp`,
  `src/Linker/WindowsLibraryBuilder.cpp` — las nueve llamadas, observadas
- `tests/unit/LlvmOptimizationTests.cpp` — 106 comprobaciones; incluye un hijo real contra
  `LLVM-C.dll`, CRLF, sonda propiedad de LLVM y equivalencia IR O0/O1/O2
- `tests/unit/ProjectOopLoweringTests.cpp` — 71 comprobaciones (antes 56)
- `tests/unit/Cc1CorrectnessTests.cpp` — registro de `c47`
- `CMakeLists.txt` — observación y supervisor de proceso en `nsc_llvm`

---

## 9. Medido y no medido

**Medido en contenedor (clang 18, `libLLVM-18`)**

```
reconstrucción completa                          OK
barrido bodies                                   OK=192 FAIL=5   (los 5 negativos intencionados;
                                                                  192 = 191 + la fixture nueva)
tests/unit/LlvmOptimizationTests.cpp             PASS: 101 checks
tests/unit/ProjectOopLoweringTests.cpp           PASS:  71 checks
Driver.cpp / los dos builders, -Wall -Wextra     compilan sin avisos
IR idéntico con y sin observación                12/12 (c38, c44, c45, c46 × O0, O1, O2)
```

**Medido además en Windows con LLVM 23.1.0**

```
build-test-nslink      nsc.llvm-optimization   PASS: 106 checks
build-test-lldlinker   nsc.llvm-optimization   PASS: 106 checks
build-clang            target nsc              compila sin avisos
nsc construido         emisión de .obj a O2    OK; registro de LLVM oculto en éxito
```

La primera ejecución de Windows falló 4/103 y fue la que demostró que `_dup2` no alcanzaba la
salida de `LLVM-C.dll`. Esa prueba no se relajó: ahora comprueba la frontera de proceso real y
subió a 106 comprobaciones.

**Todavía no medido**

Las corridas **completas** de CTest, fixtures y bug triage. Esta entrega ejecutó únicamente el
target y la prueba dirigidos de la instrumentación en los dos árboles, más una emisión O2 mínima
con el compilador construido. Eso demuestra la corrección de la captura contra LLVM 23, pero no
sustituye la matriz contractual del gate.

**El cierre del gate (doc 123) exige**, y está pendiente de la corrida de Alberto:

- 291/291 CTest por configuración aplicable;
- 132/132 fixtures con lld-link y 132/132 con NSLink;
- 18/18 bug triage con lld-link y 18/18 con NSLink.

Con `c47` la cuenta de fixtures sube a 133 por enlazador.
