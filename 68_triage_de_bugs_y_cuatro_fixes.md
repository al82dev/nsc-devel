# 68 — Triage de los bugs reportados, y cuatro arreglados (b2, b3, b5, b8)

**Fecha:** 2026-08-30
**Estado de partida:** Stage 56 / C8 cerrado, `ctest` 237/237, y la matriz de ejecución real
corrida en Windows (32/32) — ver `claude/44_ejecucion_real_windows_c1_c8.md`.
**Alcance de esta ronda:** sólo el compilador. Bindings, port de `Saru.Framework` y port del
tutorial de OpenGL son de otras instancias; aquí sólo se reciben sus reportes.

---

## 1. Por qué esta ronda existe

Las instancias consumidoras levantaron una docena de reportes contra `nsc`. Viven como docs del
proyecto y **no estaban en `BUGS_ENCONTRADOS/` de este repo ni arreglados**. Varios se
reportaron contra compiladores anteriores a C1, C6, C7 y C8.

Eso hace que la primera pregunta no sea cómo arreglarlos sino **cuáles siguen vivos**. No es
retórica: C7 mató un bug de C6 sin buscarlo, y C1 mató otro de los reportados sin que nadie lo
verificara. Arreglar a ciegas una lista donde una parte ya está muerta es trabajo tirado, y peor,
da una falsa sensación de deuda.

## 2. El arnés: `tests/bug_triage/`

Ocho reproductores mínimos, uno por bug, cada uno un `.nsproj` + `Main.ns` autocontenido. Se
corren con `run_bug_triage.bat` contra el `nsc` instalado, a `-O0` y `-O2`.

**Cada caso devuelve 42 cuando el compilador está sano.** Cualquier otra cosa es el bug: un fallo
de compilación o enlace significa que es ruidoso; un código de salida distinto significa que es
silencioso, que es peor.

La decisión de diseño que hace utilizable al arnés: **cada fixture corre primero sus controles** —
las variantes vecinas que el reporte afirma que funcionan — con códigos de salida distintos, y
sólo después el caso del bug. Sin eso, un reproductor mal escrito se lee exactamente igual que un
bug del compilador. Pasó de verdad: la primera versión del fixture de b3 no compilaba, y no por
el bug.

Cada `Main.ns` lleva arriba el reporte original completo — qué se rompió, dónde se encontró, el
IR cuando lo había, y por qué el control de al lado sí compila.

## 3. Resultado: 7 vivos, 1 muerto

| # | Bug | Estado | Evidencia |
|---|---|---|---|
| b1 | `T local = this;` en método de struct guarda el puntero | **VIVO — silencioso** | exit 3: los dos controles pasan, falla la lectura a través del local |
| b2 | `x == null` dentro de `operator ==` recursa | **VIVO → ARREGLADO** | exit `-1073741571` = `0xC00000FD`, STATUS_STACK_OVERFLOW |
| b3 | ctor no recibe struct >8 bytes por valor | **VIVO → ARREGLADO** | verifier: el call site pasa `[16 x i8]`, la firma dice `ptr` |
| b4 | `this` como argumento por valor | **VIVO** | *Call parameter type does not match function signature* |
| b5 | `new V3(...)` a elemento de arreglo `owned` de campo | **VIVO → ARREGLADO** | `Stage 34 cannot find Stage 29 allocation plan`, sin archivo ni línea |
| b6 | `extern C delegate` no cruza `ProjectReference` | **VIVO** | `unknown type name 'Adder' in module 'BugTriageDelegateApp'` |
| b7 | mismo símbolo C en dos clases (`símbolo.1`) | **MUERTO** | exit 42 — lo cerró C1 al redefinir la identidad de import como el triple `(library, symbol, signature)` |
| b8 | `1u << 28` pliega a `double` | **VIVO → ARREGLADO** | `cannot initialize local 'folded' of type 'builtin:uint' from 'builtin:double'` |

`b8` muere en el caso I, así que el caso hermano N — shift sobre `ushort` — sigue sin medirse: se
medirá solo cuando el I compile.

Dos reportes de la lista original **no entraron al arnés a propósito**, porque no son del
compilador: `sqrt se llama a sí misma` era un `.c` del archive de stb (`__builtin_sqrt` bajando a
una llamada a libm por `errno`), y `doble codificación UTF-8` eran helpers de bindings que
codificaban un `string` que ya era UTF-8. Los dos ya los arregló esa instancia.

## 4. Tres cosas que salieron del triage y no estaban en ningún reporte

**Las propiedades sólo-`set` no existen.** `error: set-only properties are not part of the Stage
38 subset`. Salió porque la primera versión del fixture de b3 usaba un setter de propiedad como
tercer control. Hueco de lenguaje pequeño, no listado en ninguna parte.

**Dos de los errores no dicen dónde.** `b4` y `b5` fallan con un mensaje interno de "Stage 34" sin
archivo ni línea. Es exactamente lo que convirtió un bug de una línea en nueve pasos de bisección
al portar `objloader`. Arreglar la atribución de esos diagnósticos abarata cada bug posterior, así
que conviene hacerlo antes y no después.

**b1, b3 y b4 son una sola familia**, y ahora están medidas las dos direcciones del desacuerdo:

- **b3** — el call site carga `[16 x i8]` y lo pasa por valor; la firma del constructor dice `ptr`.
- **b4** — `this` es un `ptr` y se pasa donde el callee espera el agregado.
- **b1** — `this` se trata como un valor de tipo puntero en vez de como el struct al que apunta, y
  el local recibe almacenamiento de tamaño puntero.

Es el mismo desacuerdo entre sitio de llamada y firma, en formas de llamada que **no** son un
método ordinario.

---

## 5. Los cuatro arreglos

### 5.1 b3 — el constructor nunca pasó por el ABI de agregados

**Diagnóstico.**

Stage 40.2 (`claude/36_win64_aggregate_abi.md`) dejó escrito que el mapeo del ABI indirecto —
parámetro agregado → `ptr` a una copia del caller, return agregado → `sret` a la cabeza — se
aplica en **nueve sitios** que construyen firmas o llamadas: `declareFunctions`, el prólogo de
definición, los dos caminos de return estructurado, los cuatro lowerers de llamada
(directa/estática/virtual/interfaz), el wrapper `export extern C` y los dos emisores de delegate.

**El constructor no está en esa lista.** Contado en el código: `abiParameterType` aparecía en 4
sitios y `wrapIndirectArgument` en 5, ninguno de ellos un constructor.

Y sin embargo la **firma** del constructor sí lleva el mapeo, porque `declareFunctions` es
genérico y no distingue constructores de métodos. De ahí el error exacto: la firma declara `ptr`,
el call site pasa `[16 x i8]`, y el verifier de LLVM rechaza el módulo entero.

No es un agujero de diseño, es de antigüedad: el mapeo se cableó en los sitios que existían en
Stage 40.2, y el constructor de struct llegó con C6 por un camino nuevo (`lowerValueConstruction`)
que nunca pasó por ahí. El de clase (`lowerConstruction`) es más viejo y tenía el mismo hueco por
la misma razón — nadie le pasó un agregado grande hasta que el port de `Saru.Framework` intentó
`BoundingFrustum(Matrix)`.

**El cambio.**

En el loop que evalúa los argumentos de origen, en los **dos** caminos de construcción:

```cpp
LLVMValueRef value = lowerExpression(node->children[index].get(), error);
if (value == nullptr) return nullptr;
value = wrapIndirectArgument(value, node->children[index]->typeKey, error);   // nuevo
if (value == nullptr) return nullptr;
evaluatedArguments.push_back(value);
```

Es la misma línea, con el mismo helper, que ya usaban los cuatro lowerers de llamada.
`wrapIndirectArgument` reserva un alloca en el **entry block** (no en el call site: emitido ahí se
re-alocaría en cada vuelta de un bucle y haría crecer el stack sin límite), copia el agregado, y
pasa el puntero. Ése es el contrato de Win64 medido contra clang: la copia es **por llamada**, y
el callee puede escribirla.

**Por qué dentro del loop de evaluación y no junto al `push_back` del objeto:** es donde lo hacen
los otros lowerers, y en `lowerConstruction` los argumentos se evalúan deliberadamente **antes**
de la asignación del objeto, para que sus efectos secundarios no desaparezcan si la asignación
devuelve null. Poniendo el wrap ahí, ese orden no cambia.

El porqué quedó escrito en el propio código —el error del verifier y el alcance medido: 8 bytes
pasa, 12/16/20/64 fallan— para que no se vuelva a perder.

**Lo que NO arregla.** Sólo b3. b1 y b4 son de la misma familia pero de otros sitios: b4 es
`this` cruzando como valor hacia una llamada estática, y b1 es la asignación de `this` a un local,
que sale con tamaño de puntero — o sea el tipo se decidió mal antes de llegar al lowerer.

---

### 5.2 b5 — el vector de planes no está indexado por ordinal de construcción

**Y es peor de lo que se reportó.** `buildAllocationPlans()` recorre todas las construcciones pero
**agrega** un plan sólo para las que realmente asignan: se salta una construcción sin resolver y
—desde C6— **toda construcción de struct**, que no tiene layout de clase y no necesita plan porque
no hay nada que asignar.

La búsqueda, en cambio, indexaba el vector por el ordinal de la construcción:

```cpp
return &objectLayoutModel_->allocationPlans()[constructionOrdinal];
```

Eso sólo es correcto mientras **toda** construcción produzca un plan. Una construcción saltada
corre todos los planes posteriores un lugar hacia abajo, y de ahí la búsqueda o se sale del rango
—el `Stage 34 cannot find Stage 29 allocation plan` reportado, sin archivo ni línea— o, en los
casos que siguen dentro del rango, **devuelve en silencio el plan de otra construcción** y habría
asignado una clase con el tamaño y la alineación equivocados.

Lo que se reportó es la mitad ruidosa. La mitad silenciosa es la razón por la que esto se arregla
en la raíz y no ensanchando el chequeo de rango. Cada plan ya guarda el ordinal al que pertenece,
así que la búsqueda ahora compara contra él.

Esto también explica las tres variantes del reporte sin necesidad de más hipótesis: la variante B
(`V3.Create(...)`) no crea ninguna construcción, así que no hay salto y los índices siguen
alineados; la A y la C sí lo crean, y sólo rompen cuando el salto queda **antes** de una
construcción de clase.

---

### 5.3 b8 — un shift no es un operador aritmético simétrico

El tipo del resultado de un shift es el del operando **izquierdo**, promovido. El derecho sólo
tiene que ser integral y no aporta nada al tipo. Pasarlo por `commonNumericType` pedía en cambio
un tipo al que convirtieran **los dos** lados: para `1u << 28` —uint a la izquierda, un literal
int a la derecha— no hay tipo integral al que lleguen ambos, así que la búsqueda caía a la tabla
de preferencias y aterrizaba en la primera entrada que ambos sí alcanzan, `builtin:double`.

De ahí que una línea tan ordinaria como `uint v = 1u << 28;` fallara con
`cannot initialize local 'v' of type 'builtin:uint' from 'builtin:double'`, un diagnóstico que no
da ninguna pista de dónde salió el double.

El caso hermano N —shift sobre un entero más angosto que int— es el mismo error: `ushort >> 8`
también terminaba en double, porque ushort e int se alcanzan mutuamente y el desempate volvía a
caer en la tabla. Con la regla correcta el resultado es `int`, el tipo promovido de la izquierda,
y los dos casos se arreglan de una vez.

La resolución de operadores definidos por el usuario corre **antes** de esta rama, así que un
`operator <<` de una clase sigue ganando.

---

### 5.4 b2 — el literal `null` ya no liga operadores del usuario

El guardia con el que tiene que abrir todo `operator ==` era él mismo la recursión: el tipo
estático de `left` es `Node`, así que `left == null` resolvía al operador del usuario como
cualquier otra comparación, y el IR emitía una llamada a `op_Equality` con `null` de segundo
argumento — cinco llamadas recursivas en un operador de seis líneas. Compilaba limpio y moría de
stack overflow.

**Esto es una desviación deliberada de C#, y conviene decirlo claro** porque el reporte original
lo justificaba diciendo que C# hace esto, y no es cierto: en C# esa línea también recursa, y las
salidas son `(object)left == null` o `left is null`. NativeSharp no tiene `is null`, y hasta C8
tampoco tenía `object` al que castear, así que la trampa no tenía salida ninguna.

El costo de la desviación es que un operador del usuario ya no puede observar una comparación
contra el literal `null`, que es algo que nadie quiere. El beneficio es que el guardia obvio y
correcto deja de ser un cuelgue. Una comparación contra `null` es identidad de referencia, y eso
es a lo que baja ahora.

**Si prefieres la semántica exacta de C# aquí, esto es lo primero que hay que revertir** — es un
cambio de tres líneas y está aislado.

---

## 6. Verificación — **PENDIENTE**

Esta ronda todavía no se ha construido ni corrido. Falta:

```
cd /d D:\nsc-devel
run_windows_checklist.bat      rem ctest debe seguir en 237/237, la matriz en 32/32
run_bug_triage.bat             rem b2, b3, b5 y b8 deben pasar a ARREGLADO
```

El número que importa más aquí es el **237/237**, no los cuatro bugs. Son los primeros cambios al
compilador de esta instancia, dos de ellos en el sistema de tipos del frontend, que es justo donde
una regresión se vería. Si algo se rompe, el orden de sospecha es: b8 primero (toca el tipo de
toda expresión de shift), b2 después (cambia a qué liga una comparación), y b5/b3 al final (son
locales y no cambian ninguna semántica).

Pendiente cuando queden verdes: subir estos casos a tests de contrato permanentes en
`tests/interop_contract/`, para que la diferencia quede medida y no se pueda perder en una
regresión. `tests/bug_triage/` es un arnés de medición, no una suite de regresión: no lo corre
`ctest`.

## 7. Archivos

**Modificados (3):**

```
src/LLVM/ProjectOopLlvmLowerer.cpp        b3  -- wrapIndirectArgument en los dos caminos de construccion
src/Frontend/ProjectOopMirModel.cpp       b5  -- allocationPlan busca por ordinal, no indexa
src/Frontend/ProjectMethodBodyModel.cpp   b8  -- rama propia para shift en analyzeBinary + helper shiftResultType
                                          b2  -- el literal null no liga operadores del usuario
```

**Nuevos (21):**

```
run_bug_triage.bat
tests/bug_triage/README.md
tests/bug_triage/b1_this_copy/{Main.ns, b1_this_copy.nsproj}
tests/bug_triage/b2_operator_null/{Main.ns, b2_operator_null.nsproj}
tests/bug_triage/b3_ctor_aggregate/{Main.ns, b3_ctor_aggregate.nsproj}
tests/bug_triage/b4_this_by_value/{Main.ns, b4_this_by_value.nsproj}
tests/bug_triage/b5_ctor_owned_field_array/{Main.ns, b5_ctor_owned_field_array.nsproj}
tests/bug_triage/b6_delegate_projectref/lib/{Callback.ns, BugTriageDelegateLib.nsproj}
tests/bug_triage/b6_delegate_projectref/app/{Main.ns, b6_delegate_projectref.nsproj}
tests/bug_triage/b7_duplicate_extern_c/{Main.ns, b7_duplicate_extern_c.nsproj}
tests/bug_triage/b8_shift_folding/{Main.ns, b8_shift_folding.nsproj}
run_windows_checklist.bat
68_triage_de_bugs_y_fix_b3.md
FIX_B3_CHANGED_FILES.txt
```

## 8. Nota de método sobre el arnés

Dos errores propios, los dos en los `.bat`, anotados aquí y en la cabecera de cada script porque
se van a repetir si alguien vuelve a escribir uno desde un entorno Linux:

1. **Un `.bat` con saltos de línea LF se comporta mal en `cmd.exe`.** `call :label` se resuelve por
   offset de bytes asumiendo CRLF; con LF el parser se desalinea, **salta llamadas enteras sin
   dejar rastro** y ejecuta labels fuera de orden. Costó un caso perdido en silencio.
2. **Texto libre en los argumentos de un `call` dentro de un bloque `for (...)` rompe el parseo**
   si contiene `( ) < > ; =`, aunque esté entre comillas. La versión final de `run_bug_triage.bat`
   no usa bloques `for` ni pasa descripciones: cada caso son dos `call` planos.
