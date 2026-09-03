# 113 -- Una división, tres respuestas distintas

Fecha: 2026-09-03
Fase: M1.2, cierre
Documento hermano: `110_contrato_de_overflow.md` §2.5, que escribió el contrato y predijo el fallo.

La fixture `c30_contratos` salió roja en su primera corrida, en los dos niveles de optimización y
con **códigos distintos**. Los dos rojos eran el mismo bug, y buscarlo destapó un tercero que
ninguna prueba miraba.

---

## 1. Lo que dijo la corrida

```
FAIL cc1_c30_contratos -O0 -- exit 3221225621, se esperaba 42
FAIL cc1_c30_contratos -O2 -- exit 19, se esperaba 42
```

`3221225621` es `0xC0000095`: **`STATUS_INTEGER_OVERFLOW`**. No es un código de salida del
programa, es Windows matándolo. `19` sí es del programa: es el paso 5 de la fixture, la línea

```csharp
if (minimoDiv / divisor != -2147483647 - 1) { return 19; }
```

con `minimoDiv == int.MinValue` y `divisor == -1`.

Es decir: **la misma línea, en el mismo programa, muere de una forma en `-O0` y devuelve un valor
equivocado en `-O2`.** Eso es exactamente lo que el criterio §14 del plan prohíbe y lo que el doc
110 §2.5 había escrito como condición de fallo: *"si aborta, falta la corrección de `idiv`"*.

---

## 2. Por qué dos síntomas distintos

**En `-O0`.** El compilador emitía un `sdiv` desnudo. La instrucción `idiv` de x86-64 lanza una
excepción de hardware cuando el cociente no cabe en el registro destino, y `MinValue / -1` es el
único caso donde eso pasa. El programa muere antes de devolver nada.

**En `-O2` no llega a ejecutarse ningún `idiv`.** En LLVM, `sdiv` con `MinValue` y `-1` es
**poison** — el equivalente en el IR del comportamiento indefinido. El optimizador tiene permiso
para plegarlo a lo que le convenga, y lo usa.

**La consecuencia que importa:** *"el hardware ya envuelve"* nunca habría sido una respuesta. El IR
es indefinido antes de que el hardware vea nada. La corrección tiene que estar **en el IR**.

---

## 3. La tercera respuesta, que no salió en ninguna corrida

Buscando dónde emitir la corrección aparecieron **dos** bajadores a LLVM, y no hacían lo mismo:

| Sitio | Qué hacía con `MinValue / -1` |
|---|---|
| `src/LLVM/ProjectOopLlvmLowerer.cpp` (proyectos, por donde pasan las fixtures) | `sdiv` desnudo |
| `src/LLVM/LlvmLowerer.cpp` (fichero único: `--program`, `--check`, `--dump-llvm`) | **abortaba** con `LlvmTrapReason::SignedDivisionOverflow` |
| `src/Frontend/ProjectIntegerEvaluator.cpp` (plegado de constantes) | devolvía `nullopt` |

Tres. Y el tercero es el peor de los tres, porque **medí qué pasaba después del `nullopt`**:

```llvm
store i32 poison, ptr %local445.addr, align 4
```

El backend guardaba **`poison`** en la constante. Sin diagnóstico, sin aviso: el programa
compilaba y quedaba indefinido desde esa línea. Yo había escrito en el informe anterior que el
plegado "rechazaba el programa". **Era falso**, y sólo se ve mirando el IR emitido — que es la
diferencia entre leer el código y medirlo.

Es la enfermedad del doc 100 en su forma más cara: una regla del lenguaje escrita en tres sitios,
con tres respuestas, y ninguna prueba que las comparara.

---

## 4. La decisión: envolver, no abortar

El bajador viejo abortaba **a propósito**, con una razón de trampa propia. El contrato dice otra
cosa, y el contrato gana. El motivo, en una línea: **`MinValue / -1` es un desbordamiento con
signo igual que `MaxValue + 1`**, y el doc 110 §2.1 ya decidió que ésos envuelven y están
definidos. Hacer que uno aborte y el otro envuelva es un caso especial que hay que recordar.

Los precedentes se reparten: **Java** envuelve (`Integer.MIN_VALUE / -1` da `Integer.MIN_VALUE`),
**C#** lanza `OverflowException`, **C** tiene UB. Envolver es el único que cumple "O0 y O2 se
comportan igual" sin pagar un salto por división.

`LlvmTrapReason::SignedDivisionOverflow` **se queda en el enum**: quitarlo renumeraría las demás
razones, y esos códigos salen en los diagnósticos.

---

## 5. Cómo se corrige, sin rama

`src/LLVM/SignedDivisionCorrection.h`, incluida por los dos bajadores. Si el caso malo se da, se
divide por 1 en vez de por -1:

```
MinValue / 1  ==  MinValue     el resultado envuelto, exacto
MinValue % 1  ==  0            el resto correcto, exacto
```

Dos comparaciones, un `and` y un `select`. Sin salto, sin bloque nuevo, y `sdiv` deja de ser poison
porque el caso que lo hacía poison ya no le llega. En el IR:

```llvm
%div.is_min      = icmp eq i32 %local.load83, -2147483648
%div.is_minus_one = icmp eq i32 %local.load84, -1
%div.overflow    = and i1 %div.is_min, %div.is_minus_one
%div.divisor     = select i1 %div.overflow, i32 1, i32 %local.load84
%sdiv            = sdiv i32 %local.load83, %div.divisor
```

Cuando el divisor es una constante distinta de -1, las dos comparaciones se pliegan solas y el
`select` desaparece: **1594 de los sitios corregidos no cuestan nada** después de optimizar.

---

## 6. Y de paso, la división por cero

El bajador de proyectos tampoco guardaba el cero: lo dejaba al hardware. Eso **no cumple el
contrato**. §2.5 dice "trampa en `-O0` y en `-O2`", y en `-O2` `sdiv x, 0` también es poison: el
optimizador pliega la función entera. La trampa tiene que estar en el IR, así que ahora la emite
`emitGuard` con `LlvmTrapReason::DivisionByZero`.

**Medición de lo que eso arregla, con un programa de cuatro líneas:**

```csharp
const int malo = 1 / 0;
```

| | Antes | Ahora |
|---|---|---|
| IR emitido | `store i32 poison` | `br i1 false, label %div.zero.ok, label %div.zero.trap` |
| Programa | compila, indefinido desde esa línea | compila, muere con `DivisionByZero` |

Sigue **sin haber diagnóstico en compilación**, y eso queda **anotado, no hecho**. Ahora la
condición de reapertura es concreta y no una intención: un `br i1 false` hacia un bloque de trampa
es un fallo demostrable en tiempo de compilación, así que el compilador *puede* diagnosticarlo.

---

## 7. La tercera y la cuarta copia de la tabla de enteros

Al tocar `applyBinaryOperation` aparecieron `isSignedInteger` e `integerWidth`, locales de
`ProjectOopLlvmLowerer.cpp`, repitiendo los anchos y los signos que el mismo día se habían
unificado en `BuiltinIntegerTypes.h`. **Ésta era la copia más peligrosa de las cuatro**, porque es
la que decide `sdiv` contra `udiv`, `ashr` contra `lshr` y `sext` contra `zext`. Un desacuerdo aquí
no da un error: da código generado que hace otra cosa.

Las dos ahora leen de la tabla única. Y antes de cambiarlas se **demostró la equivalencia**: un
programa que evalúa la versión vieja y la nueva sobre los 20 tipos que el compilador puede pasarles
—los once enteros, `bool`, los dos de coma flotante, `string`, `void`, `object`, una clase de
usuario, un puntero y la cadena vacía— y compara.

```
IDENTICAS en los 20 tipos
```

---

## 8. Cómo se verificó que no se rompió nada más

El corpus entero (180 `.nsproj`) volcado a IR con el compilador **de antes** y con el **de después**,
y comparado — no línea a línea, que es inútil cuando se insertan bloques y todo se renumera, sino
**contando cada forma de instrucción** en los dos volcados y restando.

De 3,5 millones de líneas de IR, **48 formas de instrucción cambian de cuenta. Las 48 son de la
corrección de la división.** Filtrando por las formas que la corrección introduce o consume:

```
formas que NO son de la correccion de division: 0
```

Lo que cambia, y cuánto:

| Forma | Delta | Qué es |
|---|---|---|
| `call void @__nativesharp_trap(i32 1)` | +2613 | guardas de división por cero que no existían |
| `icmp eq i32 X, -2147483648` | +1018 | divisiones con signo corregidas (+3 en 64 bits) |
| `sdiv i32 X, 2` → `sdiv i32 X, X` | -535 / +695 | el divisor pasa a ser el resultado del `select` |
| `br i1 true, label X, label X` | +1594 | guardas con divisor constante, que se pliegan solas |

**Y el resto:**

- `./sweep.sh llvm` → `OK=175 FAIL=5`, los cinco de siempre (los negativos intencionados).
- `nsc.cc1.integer-evaluator` → **39 de 39**, con dos comprobaciones nuevas de 64 bits.
- `c30` paso 5b nuevo: la constante plegada contra el valor calculado, más el resto. Es el guardián
  de que las tres vías no vuelvan a separarse.

---

## 9. Una prueba que cambió de sentido, y por qué eso no es hacer trampa

`Cc1IntegerEvaluatorTests.cpp` tenía dos `expect` exigiendo `nullopt` para `MinValue / -1`. Ahora
exigen `MinValue` y `0`. **Eso no es aflojar una prueba para que pase**: la prueba fijaba un
comportamiento que el contrato decidió cambiar, el compilador se arregló en tres sitios, y la
prueba dejó de exigir lo contrario de lo que el lenguaje dice. Lo que la prueba protegía de verdad
—que el compilador no ejecute UB de C++ dentro de sí mismo— **sigue protegido**: el evaluador
calcula este caso a mano en vez de dejar que `signedA / signedB` lo haga.

La diferencia entre las dos cosas se ve en el diff: se tocó el compilador en tres ficheros y la
prueba en uno. Si sólo se hubiera tocado la prueba, sería lo otro.

---

## 9.5 La corrida siguiente: el rojo se movió al paso 7, y ese era **mío**

```
FAIL cc1_c30_contratos -O0 -- exit 31, se esperaba 42
FAIL cc1_c30_contratos -O2 -- exit 31, se esperaba 42
```

**El mismo código en las dos configuraciones**, así que no era una divergencia de optimización: era
una expectativa falsa. El paso 5 pasó — la corrección de la división funciona en Windows con LLVM
23, que es lo que el contenedor no podía medir.

El paso 7 comprobaba que estas dos expresiones fueran **distintas**:

```
izquierda = (grande + pequeno) - grande     esperaba 0
derecha   = grande + (pequeno - grande)     esperaba 1
```

Con `grande = 1e16` y `pequeno = 1`, **las dos valen cero en IEEE 754 correcto**. A esa magnitud el
ULP vale 2, así que `1e16 + 1` y `1e16 - 1` caen los dos exactamente a mitad de camino entre dos
dobles, y el redondeo al par más cercano manda los dos a `1e16`. El compilador daba la respuesta
correcta y la prueba exigía una imposible.

**Y no era la única.** El paso siguiente comprobaba `(1.0/3.0)*3.0 != 1.0`, y en doble precisión
**sí es exactamente 1.0**. Arreglar sólo la que salió roja habría movido el rojo a la 34 en la
corrida siguiente — la misma lección que el `c28` del Bloque 5, repetida por la misma causa:
**supuse la aritmética en vez de calcularla.**

Las tres comprobaciones nuevas se verificaron numéricamente **antes** de escribirlas, y cada una
apunta a una bandera concreta de `fast-math`:

| | Qué mide | Valores |
|---|---|---|
| 7a | `reassoc` | `(g+1)+1` pierde los dos unos → `g`; `g+(1+1)` → `g+2` |
| 7b | `arcp` | `5/3` = `0x1.aaaaaaaaaaaabp+0`, `5*(1/3)` = `0x1.aaaaaaaaaaaaap+0` — un ULP |
| 7c | `nnan` | `NaN == NaN`, `NaN < 1`, `NaN >= 1`: las tres falsas, y las dos últimas serían una la negación de la otra con cualquier otro valor |

En el IR emitido: los dos `fadd` en el orden escrito sin fusionar, el `fdiv 1.0, y` seguido del
`fmul` sin plegar, los tres `fcmp` con predicado **ordenado** (`oeq`, `olt`, `oge`) — y **cero
banderas rápidas**.

---

## 9.6 Los dos rojos de `ctest`, y por qué eran los correctos

Con las fixtures en **100/100**, `ctest` dio **242 de 244**. Los dos que faltaban eran los que había
previsto, y los dos del **bajador de fichero único**:

```
nsc.llvm-lowering      FAIL: all six stage-17 runtime trap reasons are materialized in control flow
nsc.llvm-optimization  FAIL: aggressive LLVM optimization preserves the signed-division-overflow trap  (x2, O2 y O3)
```

Las dos fijaban la razón de trampa **2, `SignedDivisionOverflow`**, que este cambio dejó de emitir a
propósito. **No es que el arreglo rompiera algo: es que las pruebas decían lo contrario del
contrato.**

**Y la distinción importa, porque bajarle el número a una prueba es exactamente lo que no se hace
para que pase.** Lo que cada una protegía sigue protegido:

- **`nsc.llvm-lowering`** pedía seis razones y ahora pide cinco (1, 3, 4, 5, 6) — pero **gana dos
  comprobaciones nuevas**: que la razón 2 **no** aparezca, y que en su lugar esté la corrección
  (`icmp eq i32 %a, -2147483648`, `icmp eq i32 %b, -1`, `select i1 %div.overflow, i32 1, i32 %b`).
  La corrección es código generado y hay que verificarlo igual que la trampa a la que sustituye.
  **Medido**: 39 de 39 tras el cambio, compilando y ejecutando la prueba en el contenedor.

- **`nsc.llvm-optimization`** pasa de exigir la razón 2 a exigir su ausencia. Lo que esa prueba
  protege de verdad —que el optimizador agresivo no borre lo que el compilador emitió— lo siguen
  cubriendo las razones 1, 3 y 5 del mismo bucle, intactas.

**Lo que deliberadamente NO se comprueba ahí:** que el `select` sobreviva a `-O2`/`-O3`. Buscarlo en
el texto optimizado fijaría una **forma** del IR que `InstCombine` puede reescribir legítimamente
sin cambiar el resultado, y una prueba así se pone roja por un cambio de LLVM que no rompe nada. La
propiedad de verdad —que el resultado siga siendo `MinValue` después de optimizar— la mide `c30`
**ejecutando en `-O2`**, que es justo donde apareció el fallo original.

**Un límite honesto de esta ronda:** `nsc.llvm-optimization` **no se pudo ejecutar en el
contenedor**. Su optimizador usa `LLVMRunPassesOnFunction`, que es API de LLVM 23 y no existe en
`libLLVM-18`. La edición es de una línea invertida y compila limpio con `-Wall -Wextra`, pero quien
la mide es la corrida en Windows.

Para llegar hasta aquí hubo que **construir en el contenedor lo que el arnés no construía**:
`FrontendCompilation.cpp`, `LlvmLowerer.cpp` y los subsistemas MIR, CFG, HIR, SSA, Dominance,
Passes y Target. Eso convirtió un rojo que sólo se veía en Windows en uno reproducible aquí — y es
la diferencia entre arreglar y adivinar.

---

## 10. Lo que queda

| Qué | Estado |
|---|---|
| `MinValue / -1` en las tres vías | **cerrado**, con fixture que las compara |
| `x / 0` como trampa determinista en O0 y O2 | **cerrado** |
| `x / 0` con **diagnóstico** en compilación | **anotado, no hecho.** Reapertura: el `br i1 false` lo hace demostrable |
| Los ocho pasos de `c30` ejecutando de verdad | **pendiente de la próxima corrida en Windows** |
| `LlvmLowerer.cpp` (fichero único) ejercitado | **no**: el contenedor sólo compila el camino de proyectos. Compila limpio con `-Wall -Wextra`, y nada más se puede decir desde aquí |
