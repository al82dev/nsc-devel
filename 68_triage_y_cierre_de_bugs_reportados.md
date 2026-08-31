# 68 — Triage y cierre de los bugs reportados contra el compilador

**Fecha:** 2026-08-30
**Estado de partida:** Stage 56 / C8 cerrado, `ctest` 237/237, matriz de ejecución real corrida en
Windows 32/32 (ver `claude/44_ejecucion_real_windows_c1_c8.md`).
**Resultado:** los **ocho** reportes medidos, **siete arreglados**, uno ya estaba muerto.
`ctest` **237/237**, checklist **32/32**, triage **16/16**, cero regresiones.
**Alcance:** sólo el compilador. Bindings, port de `Saru.Framework` y port del tutorial de OpenGL
son de otras instancias; aquí sólo se reciben sus reportes.

---

## 1. Por qué se midió antes de arreglar

Las instancias consumidoras habían levantado una docena de reportes que vivían como docs del
proyecto y **no estaban en `BUGS_ENCONTRADOS/` de este repo ni arreglados**. Varios se reportaron
contra compiladores anteriores a C1, C6, C7 y C8.

Medir primero no fue ceremonia. De los ocho que entraron al arnés, **uno ya estaba muerto** —lo
había cerrado C1 sin que nadie lo verificara— y **otro resultó ser un bug distinto del reportado**.
Arreglar la lista a ciegas habría sido trabajo tirado en un caso y trabajo mal dirigido en el otro.

## 2. El arnés: `tests/bug_triage/`

Ocho reproductores mínimos, uno por bug, `.nsproj` + `Main.ns` autocontenidos, corridos por
`run_bug_triage.bat` contra el `nsc` instalado, a `-O0` y `-O2`.

**Cada caso devuelve 42 cuando el compilador está sano.** Un fallo de compilación o enlace
significa que el bug es ruidoso; un código de salida distinto significa que es silencioso, que es
peor.

La decisión que hace utilizable al arnés: **cada fixture corre primero sus controles** —las
variantes vecinas que el reporte afirma que funcionan— con códigos de salida distintos, y sólo
después el caso del bug. Sin eso, un reproductor mal escrito se lee exactamente igual que un bug
del compilador. Hizo falta de inmediato: la primera versión del fixture de b3 no compilaba, y no
por el bug.

Cada `Main.ns` lleva arriba el reporte original completo, el IR cuando lo había, y por qué el
control de al lado sí compila.

## 3. Resultado

| # | Bug | Estado | Dónde estaba |
|---|---|---|---|
| b1 | `T local = this;` en método de struct guarda el puntero | ARREGLADO | HIR |
| b2 | `x == null` dentro de `operator ==` recursa | ARREGLADO | frontend, binding de operadores |
| b3 | ctor no recibe struct >8 bytes por valor | ARREGLADO | lowerer LLVM |
| b4 | `this` como argumento por valor | ARREGLADO | HIR — **la misma línea que b1** |
| b5 | `new V3(...)` a elemento de arreglo `owned` de campo | ARREGLADO | MIR |
| b6 | `extern C delegate` no cruza `ProjectReference` | ARREGLADO | **parser** — no era lo reportado |
| b7 | mismo símbolo C en dos clases (`símbolo.1`) | ya estaba muerto | lo cerró C1 |
| b8 | `1u << 28` pliega a `double` | ARREGLADO | frontend, tipo de la expresión |

Dos reportes de la lista original **no entraron al arnés a propósito**, porque no son del
compilador: `sqrt se llama a sí misma` era un `.c` del archive de stb, y `doble codificación UTF-8`
eran helpers de bindings. Los dos ya los arregló esa instancia.

---

## 4. Los arreglos

### 4.1 b3 — el constructor nunca pasó por el ABI de agregados

Stage 40.2 dejó escrito que el mapeo del ABI indirecto —parámetro agregado → `ptr` a una copia del
caller, return agregado → `sret`— se aplica en **nueve sitios**. El constructor no está en esa
lista: contado en el código, `abiParameterType` aparecía en 4 sitios y `wrapIndirectArgument` en 5,
ninguno un constructor.

Y sin embargo la **firma** del constructor sí llevaba el mapeo, porque `declareFunctions` es
genérico y no distingue constructores. De ahí el error: la firma declara `ptr`, el call site pasa
`[16 x i8]`, el verifier rechaza el módulo entero.

No es un agujero de diseño, es de antigüedad. El mapeo se cableó en los sitios que existían en
Stage 40.2; el constructor de struct llegó con C6 por un camino nuevo que nunca pasó por ahí.

**El cambio:** `wrapIndirectArgument` sobre cada argumento de origen, en los dos caminos de
construcción, dentro del loop de evaluación —que es donde lo hacen los otros lowerers, y respeta
que en `lowerConstruction` los argumentos se evalúan deliberadamente antes de la asignación.

### 4.2 b5 — el vector de planes no está indexado por ordinal de construcción

`buildAllocationPlans()` recorre todas las construcciones pero **agrega** un plan sólo para las que
asignan: se salta las no resueltas y —desde C6— **toda construcción de struct**, que no tiene
layout de clase y no necesita plan.

La búsqueda indexaba el vector por el ordinal de la construcción. Eso sólo es correcto mientras
toda construcción produzca un plan. Un salto corre todos los planes posteriores un lugar, y de ahí
la búsqueda o se sale del rango —el error reportado, sin archivo ni línea— o **devuelve en silencio
el plan de otra construcción**, y habría asignado una clase con el tamaño y la alineación
equivocados.

Lo reportado era la mitad ruidosa. La mitad silenciosa es la razón por la que esto se arregla en la
raíz y no ensanchando el chequeo de rango.

Explica además las tres variantes del reporte sin hipótesis extra: la variante con fábrica estática
no crea construcción, así que no hay salto; las otras dos sí, y rompen sólo cuando el salto queda
antes de una construcción de clase.

### 4.3 b8 — un shift no es un operador aritmético simétrico

El tipo del resultado de un shift es el del operando **izquierdo**, promovido; el derecho sólo tiene
que ser integral. Pasarlo por `commonNumericType` pedía un tipo al que convirtieran los dos lados:
para `1u << 28` no hay ninguno integral, así que la búsqueda caía a la tabla de preferencias y
aterrizaba en `builtin:double`.

De ahí que `uint v = 1u << 28;` fallara con
`cannot initialize local 'v' of type 'builtin:uint' from 'builtin:double'`, un diagnóstico que no
da ninguna pista de dónde salió el double. El caso hermano —`ushort >> 8`— es el mismo error, y con
la regla correcta da `int`.

La resolución de operadores del usuario corre antes de esta rama, así que un `operator <<` de una
clase sigue ganando.

### 4.4 b2 — el literal `null` ya no liga operadores del usuario

El guardia con el que tiene que abrir todo `operator ==` era él mismo la recursión: el tipo estático
de `left` es `Node`, así que `left == null` resolvía al operador del usuario y el IR emitía una
llamada a `op_Equality` con `null` de segundo argumento. Cinco llamadas recursivas en un operador de
seis líneas: compilaba limpio y moría de stack overflow.

**Es una desviación deliberada de C#, y conviene decirlo claro** porque el reporte original la
justificaba diciendo que C# hace esto, y no es cierto: en C# esa línea también recursa, y las
salidas son `(object)left == null` o `left is null`. NativeSharp no tiene `is null`, y hasta C8
tampoco tenía `object`, así que la trampa no tenía salida ninguna.

El costo es que un operador del usuario ya no puede observar una comparación contra el literal
`null`. El beneficio es que el guardia obvio deja de ser un cuelgue. **Revertible en tres líneas si
se prefiere la semántica exacta de C#.**

### 4.5 b1 y b4 — eran la misma línea

Todas las formas de almacenamiento del lenguaje tienen su par explícito elegido por
`addressContext`: `LocalValue`/`LocalAddress`, `ParameterValue`/`ParameterAddress`,
`FieldValue`/`FieldAddress`. **`this` era la única que no** — devolvía siempre el puntero crudo del
receptor.

Para una **clase** eso es correcto en los dos contextos: `this` *es* la referencia. Para un
**struct** el receptor viaja por dirección desde Stage 40.2, así que en contexto de valor hay que
cargar a través de él. De ahí los dos bugs:

- `Get(this)` pasaba el puntero donde la firma quería el agregado → verifier. **Ruidoso.**
- `S8 self = this;` guardaba los **bits del puntero** en los bytes del local. Para un struct de 8
  bytes el carrier es `i64`, así que el `store ptr` cabía y nadie se quejó, y cada lectura de campo
  posterior decodificaba esos bits como los campos del struct. **Silencioso** — y era, amargamente,
  el workaround que la gente escribió para el ruidoso.

**El cambio:** emitir `PointerLoad(ThisValue)` en contexto de valor cuando el receptor es struct.
`PointerLoad` ya existía y es el mismo camino que usa un parámetro `ref` unas líneas más abajo.

Verificado que no rompe el acceso a campos: `lowerReceiver` baja los receptores struct con
`addressContext = true`, así que `this.campo` y `this.Metodo()` siguen tomando la dirección, y los
otros dos emisores de `ThisValue` —inicializador de constructor y helper de inicializadores de
instancia— también son consumidores de dirección.

### 4.6 b6 — no era lo reportado, y el primer diagnóstico mío también estuvo mal

El reporte decía que un `extern C delegate` no cruza un `ProjectReference` mientras que clases y
structs sí. La primera lectura del código pareció explicarlo: **sólo los tipos `public` cruzan un
borde de módulo** —misma regla que C#, y correcta— y el delegate reportado no tenía modificador de
accesibilidad, mientras que las clases y structs a su lado sí. O sea: la asimetría estaba en las
declaraciones.

**Esa conclusión era incorrecta, y la midió el propio arnés.** Poner `public` en el delegate tampoco
compila:

```
error: top-level accessibility supports only public/internal class, interface, struct,
       enum, and delegate declarations
```

En el dispatch de declaraciones de nivel superior, la rama de `extern` estaba **después** del error
de accesibilidad en la misma cadena `else if`. Un `extern C delegate` no podía llevar modificador de
accesibilidad en absoluto, así que sólo podía ser internal, y por lo tanto no podía cruzar. El bug
es real y es **una rama faltante en el parser** — no el sistema de tipos, no el borde de módulo.

**El cambio:** la rama de `extern` se mueve antes del error de accesibilidad, y cuando lo que sigue
es un delegate se manda por el camino común que estampa la accesibilidad. Un bloque `extern C { }`
no es un tipo, así que ahí un modificador sigue siendo error, con su propia razón.

**Consecuencia para la instancia de bindings:** el árbol se reestructuró a un proyecto por
biblioteca para rodear esto. Esa reestructuración ya no es necesaria.

### 4.7 El diagnóstico que escondió b6

Aparte del arreglo, `ProjectNameLookup` reportaba `unknown type name 'X'` para un tipo que existe en
un proyecto referenciado y simplemente no es público. Eso manda a buscar un typo que no está, y es
la mitad de por qué b6 se leyó mal durante una ronda entera. Ahora dice qué tipo es y que no es
público.

---

## 5. Tres hallazgos que no estaban en ningún reporte

**Las propiedades sólo-`set` no existen** — `set-only properties are not part of the Stage 38
subset`. Salió de la primera versión del fixture de b3. Hueco de lenguaje pequeño, sin listar en
ninguna parte.

**Dos errores internos no dicen dónde.** `b4` y `b5` fallaban con un mensaje de "Stage 34" sin
archivo ni línea — es lo que convirtió un bug de una línea en nueve pasos de bisección al portar
`objloader`. Sigue pendiente, y conviene hacerlo antes del próximo lote de bugs, no después.

**b1, b3 y b4 eran una familia**, y las tres direcciones del desacuerdo quedaron medidas: el call
site pasando el agregado donde la firma dice `ptr` (b3), `this` pasando el `ptr` donde el callee
espera el agregado (b4), y `this` tratado como valor de tipo puntero (b1). Formas de llamada que no
son un método ordinario, en las que los dos extremos del boundary se construyen por caminos
distintos.

---

## 6. Verificación

| Qué | Resultado |
|---|---|
| Build + install | limpio |
| `ctest` en Windows real | **237/237, 100%** (251.89 s) |
| Checklist de ejecución (`tests/interop_contract` + ejemplos Stage 56, `-O0` y `-O2`) | **32/32, 0 fallas** |
| Triage (`tests/bug_triage`, `-O0` y `-O2`) | **16/16, 0 vivos** |

Cero regresiones, con cambios en el parser, el frontend, el HIR, el MIR y el lowerer.

**Pendiente:** subir estos ocho casos a tests de contrato permanentes en `tests/interop_contract/`.
`tests/bug_triage/` es un arnés de medición y **`ctest` no lo corre**: tal como está, nada impide
que cualquiera de estos vuelva sin que nadie se entere.

## 7. Archivos

**Modificados (6):**

```
src/Parser/Parser.cpp                       b6  rama de extern antes del error de accesibilidad
src/Frontend/ProjectOopHirModel.cpp         b1, b4  PointerLoad(ThisValue) en contexto de valor
src/Frontend/ProjectMethodBodyModel.cpp     b8  rama de shift en analyzeBinary + shiftResultType
                                            b2  el literal null no liga operadores del usuario
src/Frontend/ProjectOopMirModel.cpp         b5  allocationPlan busca por ordinal, no indexa
src/Frontend/ProjectNameLookup.cpp          diagnostico de tipo no publico entre proyectos
src/LLVM/ProjectOopLlvmLowerer.cpp          b3  wrapIndirectArgument en los dos ctors
```

**Nuevos:** `tests/bug_triage/` (8 fixtures + README), `run_bug_triage.bat`,
`run_windows_checklist.bat`, este doc y `STAGE56_BUGFIX_CHANGED_FILES.txt`.

## 8. Nota de método sobre el arnés

Tres tropiezos propios, los tres en los `.bat`, anotados en la cabecera de cada script porque se van
a repetir:

1. **Un `.bat` con saltos de línea LF se comporta mal en `cmd.exe`.** `call :label` se resuelve por
   offset de bytes asumiendo CRLF; con LF el parser se desalinea, **salta llamadas enteras sin dejar
   rastro** y ejecuta labels fuera de orden.
2. **Texto libre en los argumentos de un `call` dentro de un bloque `for (...)` rompe el parseo** si
   contiene `( ) < > ; =`, aunque esté entre comillas. La versión final no usa bloques `for` para
   eso ni pasa descripciones.
3. **Sobrescribir un `.bat` mientras corre lo descarrila**, por el mismo mecanismo de offsets: cmd
   siguió leyendo el archivo nuevo desde una posición del viejo y trató de ejecutar media palabra.
   No tocar un script en uso.

`run_windows_checklist.bat` tiene modos: sin argumento hace el ciclo completo, `quick` se salta
`ctest` y la reconfiguración de cmake (~1.5 min en vez de ~6) para iterar, `build` sólo compila e
instala, y `norebuild` corre las pruebas sin compilar. **El modo completo es el único que demuestra
que no se rompió nada**, así que es el que cierra una ronda.
