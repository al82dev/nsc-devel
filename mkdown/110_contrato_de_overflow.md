# 110 -- Contrato de overflow de NativeSharp

Fecha: 2026-09-03
Fase: M1.2
Estado: **contrato escrito, sin sintaxis nueva.** Este documento decide la semántica; no añade
`checked`/`unchecked` ni ninguna palabra clave.

**Por qué primero el contrato.** Doc 98 lo aprendió en la fase B del Bloque 4: *"el manejo de
errores es una decisión, no un detalle. Tomarla tarde significa reescribir quince firmas."* Con el
overflow es peor: una palabra clave escrita antes de la semántica congela una semántica que nadie
eligió.

---

## 1. Lo que ya está decidido y medido, y no se reabre

Estas piezas existen, tienen fixture y no son parte de la decisión pendiente. Se listan porque
cualquier regla nueva tiene que ser compatible con ellas.

**La normalización de conversiones explícitas.** `ProjectIntegerEvaluator::normalize` es la
operación central: `(sbyte)200` da `-56`, `(byte)300` da `44`. Es truncamiento al ancho del tipo
destino con reinterpretación del bit de signo — el comportamiento de C, y el único compatible con
un ABI donde un `byte` es un byte. Fixtures: c17, c19, `nsc.cc1.integer-evaluator`.

**Los anchos.** `include/nativesharp/Frontend/BuiltinIntegerTypes.h`, una sola tabla desde el
2026-09-03: `char` 8 sin signo, `sbyte` 8 con signo, `nint`/`nuint` 64.

**El plegado de constantes usa exactamente esas reglas.** `ProjectIntegerEvaluator` es el único
evaluador y `ProjectTypeConversion` comparte su tabla. Un `const int x = 200 + 100;` y un
`200 + 100` en tiempo de ejecución no pueden diferir por la vía del ancho, porque miden con la
misma tabla.

**El desplazamiento ya valida el contador contra el ancho del operando**, no contra 64
(`nsc.cc1.integer-evaluator`: *"int << 32 is refused -- the count is validated against the operand
width, not 64"*).

---

## 2. El contrato

### 2.1 Aritmética con signo: **wrap definido en complemento a dos**

`int`, `long`, `short`, `sbyte`, `nint`: el desbordamiento **envuelve**, en complemento a dos, y es
**comportamiento definido**. `int.MaxValue + 1` es `int.MinValue`, en O0 y en O2, y el compilador no
puede aprovechar el desbordamiento para deducir nada.

**Por qué no comportamiento indefinido, como C y C++.** El UB de C existe para permitir
optimizaciones sobre bucles y para máquinas que no eran complemento a dos. Ninguna de las dos
razones aplica aquí: el objetivo es x86-64 y ARM64, los dos complemento a dos, y este proyecto tiene
como criterio explícito (§14 del plan) que *O0 y O2 respeten la misma semántica observable*. El UB
de enteros es exactamente el mecanismo por el que eso deja de ser verdad: un programa que desborda
se comporta distinto según lo que el optimizador dedujo. Es el fallo más difícil de diagnosticar que
existe en C, y no se gana nada adoptándolo.

**Consecuencia en el IR:** las instrucciones aritméticas se emiten **sin** `nsw`. Hoy hay que
comprobar si se emiten con él; si se emiten, es un bug de este contrato y hay que quitarlo.

### 2.2 Aritmética sin signo: **wrap**, como siempre

`uint`, `ulong`, `ushort`, `byte`, `char`, `nuint`: módulo 2^ancho. Ya era así en C y en C#, y es lo
que la fixture c25 mide con `18446744073709551615ul`.

**Consecuencia en el IR:** sin `nuw`.

### 2.3 La promoción decide el ancho **antes** de que el desbordamiento signifique nada

`byte a = 200; byte b = 100; int c = a + b;` da `300`, no `44`. La suma se hace en `int` porque la
promoción entera la hizo `int`, y sólo una conversión explícita a `byte` truncaría. Esto ya está
medido (`nsc.cc1.integer-evaluator`: *"200 + 100 is 300 as int -- operand width does not narrow the
result"*) y el contrato lo confirma: **el desbordamiento es una propiedad del tipo del resultado, no
de los operandos.**

### 2.4 Conversiones explícitas: **truncan, siempre, sin comprobar**

`(byte)300` es `44`. Nunca lanza, nunca satura, nunca depende de la configuración. Un `cast` en
NativeSharp es una afirmación del programador sobre el ancho, igual que en C.

**Y no hay conversiones implícitas que estrechen.** El modelo semántico ya las rechaza; el contrato
lo fija: si el destino no puede representar todos los valores del origen, hace falta un cast
escrito. Eso es lo que hace que 2.4 sea seguro — nadie trunca por accidente.

### 2.5 División: `MinValue / -1` y `x / 0`

- `int.MinValue / -1` **envuelve** a `int.MinValue`, coherente con 2.1. En x86-64 la instrucción
  `idiv` genera una excepción de hardware en ese caso, así que **hay que emitir la corrección** — es
  el único sitio donde el contrato obliga a generar código extra.
- `x / 0` con enteros es un **trap**, en O0 y en O2. No es un valor: es un fallo del programa. No se
  convierte en excepción de NativeSharp porque no hay ninguna que tenga sentido capturar y seguir.

**MEDIDO Y ARREGLADO EL 2026-09-03 — ver doc 113.** Los dos casos estaban mal, y de tres formas
distintas a la vez: el bajador de proyectos emitía un `sdiv` desnudo (excepción de hardware en O0,
poison en O2), el bajador de fichero único abortaba a propósito, y el plegado de constantes
guardaba **`poison`** en la constante sin decir nada. Ahora la corrección vive en
`src/LLVM/SignedDivisionCorrection.h`, la usan los dos bajadores, el plegado devuelve el mínimo, y
la división por cero lleva una trampa explícita en el IR en vez de depender del hardware. Fixture:
`c30` pasos 5, 5b y 22.

Lo único que sigue abierto: **no hay diagnóstico en compilación** para un `x / 0` con constantes.

### 2.6 Coma flotante: **IEEE 754 sin fast-math**

`float` y `double` siguen IEEE 754. No se activa `fast-math` ni ninguna relajación: ni
reasociación, ni `x/y` → `x*(1/y)`, ni contracción a FMA sin que el programador la pida. El
resultado de una expresión no depende del nivel de optimización.

**Por qué es no negociable aquí.** `String.ToString(double)` tiene una garantía **medida** de once
cifras significativas contra glibc sobre 240.000 valores (doc 92, SHR-023). Fast-math la invalida en
silencio. Y §9 del plan pide "política determinista de punto flotante" — esto es esa política.

**Consecuencia en el IR:** ninguna instrucción de coma flotante lleva banderas rápidas. Hay que
comprobarlo.

---

## 3. Qué NO decide este contrato

**No hay `checked`/`unchecked`, y puede que nunca haya.** El plan (§5, M1.2) lo dice: *"sólo si dos
políticas dentro de un mismo programa tienen un consumidor real se agrega sintaxis para
seleccionarlas"*. Hoy hay una sola política —wrap definido— y no hace falta seleccionar nada.

**Lo que sí puede hacer falta antes es un modo de DIAGNÓSTICO**, y es una cosa distinta de una
palabra clave: una propiedad de proyecto (M5.2) que en Debug inserte una comprobación y aborte al
desbordar. No cambia la semántica del lenguaje —el programa correcto se comporta igual— sino que
convierte un wrap silencioso en un fallo ruidoso mientras se desarrolla. Eso encaja en "checks de
bounds, overflow y asserts según el contrato elegido" de M5.2 y **no necesita sintaxis**.

La distinción importa: `checked` cambia lo que el programa *significa*; un check de Debug cambia
sólo cuándo te enteras.

---

## 3.5 Un hueco del lenguaje encontrado al escribir la fixture

**`-2147483648` no se puede escribir como literal `int`.**

```
cannot initialize local 'plegadoResta' of type 'builtin:int' from 'builtin:long'
```

El menos unario se aplica a `2147483648`, que no cabe en `int` y por tanto es `long`, así que la
expresión entera es `long`. C# trata este caso como especial —el par `-` + literal se tipa junto—
y NativeSharp no. Se rodea escribiendo `-2147483647 - 1`, que es aritmética `int` sin ambigüedad, y
es lo que hace la fixture c30.

No es un bug del contrato de overflow: es una decisión de tipado de literales que nadie había
tomado. Queda **anotado, no hecho**, con la condición de reapertura escrita: cuando alguien
necesite escribir el mínimo de un entero sin el rodeo, o cuando el rodeo aparezca en más de dos
sitios de la biblioteca.

---

## 4. Lo que se midió, y el resultado

Las seis mediciones de la tabla de abajo se hicieron el 2026-09-03 con el frontend real, y **las dos
de inspección del IR salieron a favor del contrato**:

**`nsw`/`nuw`: no se emiten en ninguna parte.** Sobre el IR de la fixture c30:

```
313 add i64      207 add i32      114 sub i32
239 mul i64       47 sub i64        7 mul i32
```

Ni un solo `add nsw`, `sub nsw`, `mul nsw` ni su equivalente `nuw`. Es decir: el compilador **no**
afirma "esto no desborda", así que el optimizador no puede deducir nada del desbordamiento. §2.1 y
§2.2 se cumplen hoy, y ahora hay una fixture que lo fija.

**Banderas rápidas de coma flotante: ninguna.**

```
5 fadd double    3 fsub double    17 fmul double    10 fdiv double
1 fadd float                       1 fmul float
```

Todas desnudas — sin `fast`, `reassoc`, `contract`, `nnan` ni `ninf`. §2.6 se cumple, y la garantía
de once cifras de `String.ToString(double)` (doc 92) no está en riesgo por esta vía.

**Clasificación de lo medido, que es lo que M1-A pide:** las dos son **comportamiento permitido y
ya correcto**. No hay bug que arreglar en la aritmética emitida. Lo que faltaba no era el
comportamiento: era el contrato escrito y una prueba que lo fije.

### Lo que sigue sin medir, y sólo se puede medir ejecutando

| Qué | Estado |
|---|---|
| `nsw`/`nuw` en la aritmética emitida | **medido: no se emiten.** Correcto |
| banderas rápidas de coma flotante | **medido: ninguna.** Correcto |
| `int.MaxValue + 1` en O0 y O2 | c30 pasos 1–2, **pendiente de ejecutar** |
| `int.MinValue / -1` | **MEDIDO Y ROTO EN TRES SITIOS. Arreglado.** Doc 113 |
| plegado de constantes vs ejecución | c30 paso 6, **pendiente**. Ocho pares de la misma expresión. El paso 5b sí se midió: el plegado guardaba `poison` |
| `x / 0` entero | trampa explícita en el IR desde el 2026-09-03. Sin **diagnóstico** en compilación: anotado, no hecho |

El paso 6 es el que más me importa y el que ninguna otra prueba del árbol hacía: la misma
expresión evaluada por `ProjectIntegerEvaluator` en compilación y por el binario en ejecución, ocho
veces. Si discrepan, hay dos juegos de reglas — y la unificación de `BuiltinIntegerTypes.h` existe
para que no pueda pasar.

---

## 5. Lo que hay que medir para cerrar M1.2

Ninguna de estas seis cosas tiene fixture hoy. Son el trabajo de la ronda, y son de medición antes
que de implementación:

| Qué | Cómo se mide | Si sale mal |
|---|---|---|
| `nsw`/`nuw` en la aritmética emitida | inspección del IR | quitarlos: contradicen 2.1 y 2.2 |
| `int.MaxValue + 1` en O0 y en O2 | fixture ejecutable, comparar los dos | divergencia = bug del contrato |
| `int.MinValue / -1` | fixture ejecutable | si aborta, falta la corrección de `idiv` |
| `x / 0` entero | fixture ejecutable | fijar el comportamiento como trap |
| banderas rápidas en coma flotante | inspección del IR | quitarlas |
| plegado de constantes vs tiempo de ejecución | la misma expresión como `const` y como cálculo | divergencia = dos evaluadores, y ya sólo hay uno |

La última es la que más me preocupa y la más barata de escribir: `const int a = 2147483647 + 1;`
frente a `int b = leer(); int c = b + 1;` con `b == 2147483647`. Si dan distinto, el plegado y la
ejecución no comparten reglas — y el doc 100 ya enseñó lo que cuesta que dos caminos contesten
distinto a la misma pregunta.

---

## 6. Resumen en una tabla

| Operación | Con signo | Sin signo |
|---|---|---|
| `+ - *` al desbordar | wrap, definido | wrap, definido |
| conversión explícita que estrecha | trunca | trunca |
| conversión implícita que estrecha | no existe | no existe |
| `/` por cero | trap | trap |
| `MinValue / -1` | wrap a `MinValue` | n/a |
| `<<` con contador ≥ ancho | rechazado en compilación | rechazado en compilación |
| coma flotante | IEEE 754, sin fast-math | — |
| O0 vs O2 | idéntico | idéntico |
