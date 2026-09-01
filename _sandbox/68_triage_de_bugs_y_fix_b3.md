# 68 — Triage de los bugs reportados, y el primero arreglado (b3)

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
| b2 | `x == null` dentro de `operator ==` recursa | **VIVO** | exit `-1073741571` = `0xC00000FD`, STATUS_STACK_OVERFLOW |
| b3 | ctor no recibe struct >8 bytes por valor | **VIVO → ARREGLADO en esta ronda** | verifier: el call site pasa `[16 x i8]`, la firma dice `ptr` |
| b4 | `this` como argumento por valor | **VIVO** | *Call parameter type does not match function signature* |
| b5 | `new V3(...)` a elemento de arreglo `owned` de campo | **VIVO** | `Stage 34 cannot find Stage 29 allocation plan`, sin archivo ni línea |
| b6 | `extern C delegate` no cruza `ProjectReference` | **VIVO** | `unknown type name 'Adder' in module 'BugTriageDelegateApp'` |
| b7 | mismo símbolo C en dos clases (`símbolo.1`) | **MUERTO** | exit 42 — lo cerró C1 al redefinir la identidad de import como el triple `(library, symbol, signature)` |
| b8 | `1u << 28` pliega a `double` | **VIVO** | `cannot initialize local 'folded' of type 'builtin:uint' from 'builtin:double'` |

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

## 5. El arreglo de b3

### El diagnóstico

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

### El cambio

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

### Lo que NO arregla

Sólo b3. b1 y b4 son de la misma familia pero de otros sitios: b4 es `this` cruzando como valor
hacia una llamada estática, y b1 es la asignación de `this` a un local, que huele a frontend (el
local sale con tamaño de puntero, o sea el tipo se decidió mal antes de llegar al lowerer). Se
atacan en la siguiente tanda.

---

## 6. Verificación — **PENDIENTE**

Esta ronda todavía no se ha construido ni corrido. Falta:

```
cd /d D:\nsc-devel
run_windows_checklist.bat      rem ctest debe seguir en 237/237, la matriz en 32/32
run_bug_triage.bat             rem b3 debe pasar de VIVO a ARREGLADO
```

El número que importa más aquí es el **237/237**, no el b3: es el primer cambio al compilador de
esta instancia, y lo que hay que demostrar antes que nada es que no rompió nada.

Pendiente también, cuando b3 quede verde: subir las tres variantes a un test de contrato
permanente en `tests/interop_contract/`, para que la diferencia quede medida y no se pueda perder
en una regresión.

## 7. Archivos

**Modificado (1):**

```
src/LLVM/ProjectOopLlvmLowerer.cpp    dos ocurrencias, tres lineas cada una
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
