# 89 — Sí, pasa con los otros tipos. El mapa completo de `commonNumericType`

**Fecha:** 2026-09-02
**Pregunta que contesta:** «¿esto va a pasar con los otros tipos de datos?»
**Respuesta corta:** **sí, y el ID 126 es la séptima parte del problema.**
**Cómo se midió:** reimplementando `commonNumericType`, `classifyImplicit`, `isIntegralPromotion`,
`isNumericWidening` y `integerPromotion` línea por línea desde
`src/Frontend/ProjectTypeConversion.cpp`, y evaluando **los 91 pares** de los trece tipos numéricos
contra la regla de C# §12.4.7. No es una lectura ni una estimación: es la tabla entera.

**Estado: CERRADO Y MEDIDO** — `242/242`, `Passed: 80  Failed: 0`, veinte fixtures ejecutando.
Las secciones 1-6 son la medición que llevó a la decisión; la **§7** es lo que se implementó y lo
que la corrida encontró por el camino.

---

## 1. El resultado, de una

**31 de los 91 pares dan un tipo distinto al de C#.** El ID 126 —el cortocircuito de tipos
iguales— explica **7**. Los otros 24 salen de dos reglas que el ID 126 ni menciona.

| Regla que decide | Pares que difieren |
|---|---:|
| Cortocircuito de tipos iguales (línea 378) | **7** |
| «Conversión unidireccional gana» (líneas 383-394) | **14** |
| Bucle de preferencia (final de la función) | **10** |
| | **31** |

Y no es sólo `+`. `commonNumericType` la llaman **siete sitios**: la aritmética binaria, la
comparación, el operador condicional `?:` (`ProjectMethodBodyModel.cpp:4298`), la asignación
compuesta, el evaluador de constantes enteras y el HIR al elegir el tipo de operando de un `Binary`.
**Un solo mapa gobierna todo eso.**

---

## 2. Las tres clases, por severidad

### Clase A — TRUNCACIÓN SILENCIOSA · 9 pares · **es el defecto real**

El tipo común es más estrecho que `int`, así que el backend emite un `add i8` o `add i16` y
**envuelve sin diagnóstico**.

| Par | NSC | C# | Cómo se rompe |
|---|---|---|---|
| `sbyte + sbyte` | `sbyte` | `int` | cortocircuito |
| `byte + byte` | `byte` | `int` | cortocircuito |
| `char + char` | `char` | `int` | cortocircuito |
| `short + short` | `short` | `int` | cortocircuito |
| `ushort + ushort` | `ushort` | `int` | cortocircuito |
| `sbyte + short` | `short` | `int` | unidireccional |
| `byte + short` | `short` | `int` | unidireccional |
| `byte + ushort` | `ushort` | `int` | unidireccional |
| `char + ushort` | `ushort` | `int` | unidireccional |

**El ID 126 nombra cinco de estos nueve.** Los cuatro mixtos no vienen del cortocircuito: vienen de
la regla «si sólo una de las dos conversiones existe, gana ese tipo». `byte → short` ensancha,
`short → byte` no, así que el par contesta `short`. La regla es razonable en general y aquí está
respondiendo antes que `integerPromotion`.

Consecuencia concreta: `byte b = 200; short s = 32700; short r = b + s;` da **-32636**.

### Clase B — DOS ENTEROS DANDO `double` · 10 pares · **P1, y no está en el inventario**

| Par | NSC | C# |
|---|---|---|
| `int + ulong` | **`double`** | *error de compilación* |
| `long + ulong` | **`double`** | *error de compilación* |
| `sbyte + ulong`, `short + ulong`, `ulong + nint` | **`double`** | *error* |
| … y los cinco equivalentes con `nuint` | **`double`** | *error* |

Es **exactamente la forma del defecto que cerró el ID 121** (`int + long` daba `double`), viva
todavía para cualquier cosa con signo contra `ulong`/`nuint`. Y el propio comentario del ID 121 lo
rozó sin nombrarlo:

> *«Con un `long` no constante esa comprobacion falla […] y la expresion sigue exactamente el camino
> que seguia antes.»*

Ese camino es el bucle de preferencia, y su primera parada es `double`. Dos cosas se rompen ahí:

1. **El plegado de constantes se rinde.** El evaluador entero comprueba que el tipo común sea
   integral; con `double` devuelve nada. Es la frase literal del doc 86.
2. **`double` pierde enteros por encima de 2⁵³.** `ulong grande = 1UL << 60; long uno = 1;`
   → `grande + uno` se computa en punto flotante y devuelve un número que **no es** la suma.

C# **rechaza** estos programas. NSC los acepta y los computa mal. Eso es peor que una divergencia:
es aceptar lo que el modelo de referencia prohíbe, y contestar mal.

### Clase C — `nint`/`nuint` en vez de `long`/`ulong` · 12 pares · **NO era un defecto**

`int + nint` → `nint`. `uint + nuint` → `nuint`. C# diría `long` y `ulong`.

> **CORRECCIÓN (2026-09-02).** Esta sección se escribió comparando **sólo contra C#**, y ese era el
> patrón equivocado. La decisión que se tomó es *mecanismo de C++*, y **C++ contesta exactamente lo
> que este árbol ya contestaba**: `size_t + int` es `size_t`, no `unsigned long`, porque el tipo del
> ancho del puntero tiene rango propio. La Clase C **desaparece: no había nada que arreglar aquí.**
>
> Y menos mal que se miró: la primera versión del cambio los colapsaba a `long`/`ulong` «para
> coincidir con C#» y eso **rompía código que hoy compila** — `nuint size = sizeof(T);
> nuint total = size * 2;` daba `ulong`, y la vuelta `ulong → nuint` no ensancha. Lo cazó la tabla
> de 91 pares antes de la primera corrida.

Lo que sí queda anotado como deuda, y es de fondo: **`kIntegralInfo` declara `nint`/`nuint` de 64
bits a mano**, cuando su ancho debería decidirlo el target. Hoy es inocuo porque el único target es
x64. No es de este ítem.

---

## 3. La contradicción, y esta vez está en el mismo archivo

`shiftResultType` (`ProjectMethodBodyModel.cpp:75`):

```cpp
if (leftTypeKey == "builtin:sbyte" || leftTypeKey == "builtin:byte" ||
    leftTypeKey == "builtin:char"  || leftTypeKey == "builtin:short" ||
    leftTypeKey == "builtin:ushort")
{
    return "builtin:int";
}
```

**Exactamente los cinco tipos.** Exactamente la promoción a `int`. Ya implementada, ya en el árbol,
para el desplazamiento.

Y `isIntegralPromotion` (`ProjectTypeConversion.cpp:116`) declara **la misma lista de cinco** con
destino `int` — la promoción está nombrada en el clasificador de conversiones. La conoce el shift,
la conoce el clasificador… y la función que decide `+` la esquiva.

```csharp
byte b = 200;
int  x = b << 1;   // 400  -- la promocion SI ocurre
byte y = b + b;    // 144  -- la promocion NO ocurre
```

Mismos cinco tipos, mismo archivo, reglas opuestas. **Tres sitios nombran la promoción y el que
decide la suma no llega a ella.**

Súmese la del doc 88 §2: el ID 81 rechaza `byte b; int n; b += n;` citando que «un estrechamiento
que el código no escribió es pérdida silenciosa», mientras `byte c = b + b;` trunca sin decir nada.

---

## 4. La corrección: una inversión de orden, no tres parches

El doc 88 recomendaba quitar el cortocircuito y la regla del ID 120. Correcto, y **demasiado
pequeño**: quitar el cortocircuito arregla 5 de los 9 de la Clase A, la regla unidireccional los 4
restantes, y el bucle de preferencia es una tercera corrección que ni el 126 ni el 120 mencionan.

Parchear tres reglas habría dejado un cuarto agujero el día que apareciera un cuarto camino. Lo que
se hizo es una sola cosa:

> Para un par de **integrales**, `integerPromotion` contesta **primero, y es el único que contesta**.
> Ni cortocircuito de tipos iguales, ni «gana la conversión unidireccional», ni caída al bucle de
> preferencia. **Y si la guarda falla, es error** — nunca `double`.

El cortocircuito, la regla unidireccional y el bucle siguen existiendo **para lo suyo**: flotantes y
mezclas con `float`/`double`, donde sí son correctos.

Esto explica además por qué el ID 121 no lo arregló ya: puso `integerPromotion` en el sitio correcto
**para el par que tenía delante** —después del 120, antes del bucle— y su guarda se rinde
calladamente justo en los pares de la Clase B. **El ID 121 no falló: midió un par y arregló ese par.**

---

## 5. Lo que la implementación tuvo que añadir, y no estaba en la medición

Dos cosas. Ninguna se veía leyendo; las dos las encontró medir.

**a) El tipo nativo se prefiere, pero no se impone.** `integerPromotion` contesta `nint`/`nuint`
cuando uno de los operandos lo es. Pero `uint → nint` **no ensancha**, y con razón: `nint` puede
medir 32 bits en otro target y un `uint` no cabría. Sin un paso más, `uint + nint` —que hoy compila y
da `long`— dejaba de compilar. Ahora, si un operando no llega al tipo nativo, se cae al de ancho
fijo, al que los dos sí ensanchan. **Lo cazó la tabla de 91 pares, antes de la primera corrida.**

**b) La constante `int` se adapta al otro operando, pero SOLO HACIA ARRIBA.** Esto sí llegó a la
corrida, y reventó la biblioteca entera:

```
lib\NativeSharp.Core\String.ns:980: cannot implicitly convert assignment RHS
                                     from 'builtin:long' to 'builtin:uint'
    hash = hash * 16777619;
```

Al retirar la regla del ID 120 se fue con ella algo que **no es la misma cosa**:

* hacia un tipo **estrecho** (`byte b; b + 1`), adaptar es lo que hacía que la operación se computara
  a 8 bits y envolviera. Eso es el ID 126 y se va.
* hacia `uint`/`ulong`/`nuint`/`long`/`nint` **no hay nada que truncar**: el otro operando ya es
  igual o más ancho que `int`. Es la *implicit constant expression conversion* de C#, y C++ llega al
  mismo sitio por otro camino (`unsigned int * int` colapsa al sin signo). **Las dos referencias dan
  `uint`; la primera versión daba `long`.**

**El error de método, que es lo que hay que llevarse.** El doc 88 §3 afirma «en `lib/` cero líneas»,
y lo afirma con un barrido que buscó `byte|sbyte|short|ushort|char`. **`uint` no es un tipo estrecho,
así que fue invisible.** La conclusión era correcta para lo que se midió; la medición no cubría el
caso que rompió — y eso es peor que equivocarse, porque suena igual de seguro. Con el patrón
correcto salen tres sitios de la misma clase: `hash * 16777619`, `remaining % 10` y
`(ulong)(-(value + 1)) + 1`.

---

## 6. Cómo quedó medido

La tabla de **91 pares está en `Cc1CorrectnessTests.cpp`**, y con ella la **simetría** (`a + b` debe
dar lo mismo que `b + a`, que con reglas ordenadas por conversiones unidireccionales no es gratis).
A partir de aquí **la tabla es la especificación**: cualquier cambio en las tablas de ancho, de signo
o de ensanchamiento que mueva una sola respuesta falla ahí, con el par escrito en el mensaje de
error.

Más cinco aserciones sobre la adaptación de la constante —las tres que rompieron, la que **no** debe
adaptarse hacia abajo, y un negativo contra `uint` que no cabe y por tanto promueve a `long` en vez
de envolver— y tres pruebas desde el lenguaje: `byte wraps = a + b;` **rechazado**, `long + ulong`
**rechazado**, y `nuint`/`nint` siguen compilando.

Dos fixtures se **invirtieron**, y las dos llevan su historia escrita al lado:

| Antes | Ahora |
|---|---|
| `c17::NarrowStaysNarrow` — medía la regla del ID 120 | `c17::NarrowPromotesToInt`, con `byte 200 + byte 100 == 300` |
| `c19::ConstantAdaptsToTheNarrowOperand` | `c19::NarrowArithmeticPromotesToInt`, con `sbyte 100 + 100 == 200` |

Y la aserción del **ID 122** se mudó de `commonNumericType` a `classifyImplicit`: al retirarse la
regla del 120 sus dos pares pasaron a dar `int` y **el test dejaba de discriminar**. La pregunta de
fondo —si `-3` cabe en un `short`— vive en el clasificador, que es donde `constantFits` decide, así
que además es el sitio correcto.

---

## 7. El resultado

```
Passed: 80   Failed: 0
ctest:  100% tests passed out of 242
BUILD:  limpio, ni un error ni un warning (con /WX)
MANIFEST: 1225/1225 PASS
bug_triage: 16/16, cero vivos
```

Veinte fixtures **ejecutadas**, incluidas las dos invertidas. `lib/` no cambió ni una línea — el
único ajuste fue en el compilador, no en la biblioteca.

**Lo que el lenguaje hace ahora:**

* los cinco tipos menores que `int` promueven a `int`. `byte 200 + byte 100` da 300, no 44;
* **dos enteros nunca dan `double`** — los diez pares que mezclan 64 con signo y 64 sin signo son
  error de compilación, igual que en C#;
* `nint`/`nuint` conservan su nombre cuando el otro operando llega a ellos, y se caen a `long`/`ulong`
  cuando no;
* una constante `int` que cabe se adapta al otro operando **hacia arriba**, y sólo hacia arriba.

**Dos rondas, dos fallos, y los dos de método:** cambiar el texto de un diagnóstico sin buscar quién
lo leía, y barrer `lib/` con un patrón que no cubría el tipo que rompió. Las dos veces la corrida lo
dijo en un solo archivo, que es exactamente para lo que se escribió `report.py`.
