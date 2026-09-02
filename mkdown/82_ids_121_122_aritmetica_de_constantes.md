# 82 — IDs 121 y 122: la aritmética de constantes enteras

**Fecha:** 2026-09-01
**Bloque:** 1a del doc 81 (Correctness del frontend). El primero del camino crítico.
**Estado:** **CERRADO.** Medido en el host y verificado en Windows: `nsc.cc1.correctness` verde
con la fixture `c17` dentro (2026-09-02).
**Decisión tomada:** ID 122 por la **salida (a)** — convertir en el evaluador. Razón en §2.

---

## 1. Lo que se midió antes de tocar nada

El handover decía dónde estaba cada defecto. Se comprobó en el árbol de `A:\nsc-devel` antes de
escribir una línea, porque un plan escrito sobre una descripción en vez de sobre una medición ya
salió falso una vez en este proyecto:

| Afirmación del handover | Comprobado |
|---|---|
| La struct documenta `value` como magnitud | `ProjectTypeConversion.h:29-38`, comentario del ID 119 |
| El evaluador le mete `bits` | `ProjectIntegerEvaluator.cpp:268-269`, exactamente esas dos líneas |
| El sitio de inserción del 121 | `ProjectTypeConversion.cpp`: regla del ID 120 termina en 321, bucle de preferencia empieza en 323 |
| «10 ocurrencias, el defecto son 2» | `grep -rn ProjectIntegerConstant src/ include/` → 10 líneas, de las cuales **sólo tres construyen un valor**: method body 3833 y 8216 (los dos respetan la magnitud) y evaluador 268-269. Las otras siete son declaraciones de parámetro |
| `c17_constant_promotion` no existía | `tests/cc1/` llegaba hasta `c16_enum_layout` |

**Medición nueva, y cambia una suposición del handover:** los shifts **no** pasan por
`commonNumericType` en ninguna de las dos rutas. El evaluador los desvía en
`ProjectIntegerEvaluator.cpp:231` y el modelo semántico en `ProjectMethodBodyModel.cpp:8423`, los dos
con su comentario de CC1-103 delante. La regresión de CC1-103 por tanto **no puede** romperse desde
este cambio; se mide igual, pero por completitud y no por riesgo.

---

## 2. ID 122 — la decisión, y por qué (a)

`ProjectIntegerConstant::value` está documentado como **magnitud**, con el signo en `isNegative`,
porque `constantFits` sólo compara contra cotas superiores. `ProjectIntegerValue::bits` es lo
contrario: complemento a dos con el signo ya extendido a 64 por `normalize`. El evaluador pasaba lo
segundo como si fuera lo primero.

**(a) — convertir en el evaluador. Elegida.** No es el parche barato frente a la solución buena: la
struct ya *documenta* que `value` es una magnitud, y el evaluador simplemente incumplía ese
contrato. (a) hace que el código cumpla lo que ya dice de sí mismo, y deja intactos los otros dos
sitios que sí lo cumplían.

**(b) — cambiar la struct a bits + `typeKey`.** Habría tocado los tres constructores y `constantFits`
entero, que es la función donde viven los IDs 119 y CC1-100. Cambiar la representación bajo esos dos
cierres, en el mismo bloque en que se abre el 121, mete dos causas posibles en cada rojo. Queda
disponible si algún día se quiere una sola representación en todo el frontend; hoy no compra nada
que (a) no dé.

**Lo que se escribió** — `src/Frontend/ProjectIntegerEvaluator.cpp`, helper nuevo en el namespace
anónimo:

```cpp
ProjectIntegerConstant asIntegerConstant(const ProjectIntegerValue& value) noexcept
{
    const bool typeIsUnsigned = !ProjectIntegerEvaluator::isSignedInteger(value.typeKey);
    if (typeIsUnsigned || value.asSigned() >= 0)
    {
        return ProjectIntegerConstant{value.bits, typeIsUnsigned, false};
    }
    return ProjectIntegerConstant{std::uint64_t{0} - value.bits, false, true};
}
```

**El mínimo con signo.** La magnitud se calcula sobre el gemelo sin signo (`0 - bits`): negar
`INT64_MIN` con signo es comportamiento indefinido, y un compilador no puede permitir que su texto
de entrada lo lleve ahí. Para `INT64_MIN` la magnitud sale `2^63`, que es exactamente lo que
`constantFits` acepta en su rama de negativos (`value <= INT64_MAX + 1`). Es la misma técnica que ya
usaba el unario `-` de ese mismo archivo (línea 202).

**`isNegative` de un cero es falso**, igual que en el sitio del modelo semántico
(`negated.isNegative = negated.value != 0`). Las dos convenciones coinciden ahora.

---

## 3. ID 121 — la regla, y sobre todo su sitio

`src/Frontend/ProjectTypeConversion.cpp`. Dos piezas:

**Una tabla local de ancho y signo** (`kIntegralInfo`), junto a `kIntegralTypes` en el namespace
anónimo. Dice lo mismo que `ProjectIntegerEvaluator::kIntegerTypes` **a propósito**: es el evaluador
quien llama aquí, no al revés, así que este archivo tiene que poder contestar sin depender de él. Lo
dice su comentario, y las dos tablas se fijan la una contra la otra en `nsc.cc1.integer-evaluator` y
en la fixture c17.

**La regla**, en este orden:

```
alguno es ulong/nuint                  -> ulong
alguno es de 64 bits                   -> long
alguno es uint y el otro tiene signo    -> long
alguno es uint                         -> uint
en otro caso                           -> int
```

**El sitio: DESPUÉS de la regla del ID 120 y ANTES del bucle de preferencia.** Es la mitad de la
corrección. Delante del 120, `short s; s + 3` pasa a evaluarse en `int` y `short t = s + 3;` vuelve
a no compilar — exactamente el defecto que el 120 cerró. Detrás del bucle no sirve de nada: el bucle
está ordenado de más ancho a más estrecho y su primera parada es `double`.

**La guarda, que no es ceremonia.** `integerPromotion` da el tipo que C# usaría; el llamador
comprueba con dos `classifyImplicit` que los dos operandos **llegan** a ese tipo con las conversiones
que este compilador tiene de verdad. Si no llegan, se cae al bucle de preferencia y la expresión se
comporta como antes. Eso es lo que hace que la regla **sólo añada respuestas donde no había ninguna
correcta**, y es lo que resuelve el caso `ulong`:

* `3L + 5UL` → las dos conversiones por constante existen → **ulong**. Útil, y lo que pide el
  handover.
* `long x, ulong y` no constantes → no hay ensanchamiento `long -> ulong` → la guarda falla → el
  comportamiento es idéntico al de antes de este cambio. En C# esto es un error; aquí no se inventa
  una respuesta nueva en el mismo bloque en que se arregla otra cosa.

---

## 4. Cómo se midió, aquí y ahora

**Compilando `ProjectTypeConversion.cpp` sola y llamándola**, que es como se midió el ID 120 y como
se descubrió que su primera versión rompía `(-1) + 1L`. Diecinueve casos, `-Wall -Wextra`, cero
fallas:

| Caso | Resultado | Qué fija |
|---|---|---|
| `int(1) + long(1L)` | `long` | el 121, directo. Antes: `double` |
| `long(3L) + ulong(5UL)` | `ulong` | la parte sin signo |
| `long + ulong` sin constantes | `double` | **la guarda**: sin constantes no se inventa nada |
| `int + uint` sin constantes | `long` | ninguno pierde rango |
| `uint + byte` | `uint` | |
| `uint + short` | `long` | |
| `ushort + short` | `int` | dos estrechos de signo distinto |
| `short + int(3)` | `short` | **regresión del ID 120** |
| `byte + int(1)` | `byte` | **regresión del ID 120** |
| `char + int(65)` | `char` | **regresión del ID 120** |
| `int(-1) + long(1L)` | `long` | un literal ancho no se estrecha |
| `short + int(-3)` como magnitud | `short` | **el 122, por su efecto** |
| `short + int(-3)` como bits crudos | `int` | **lo que hacía antes.** Las dos respuestas difieren, o el 122 no estaba cerrado |
| `int + double` | `double` | los pares con flotante siguen siendo del bucle |
| `float + long` | `float` | |
| `nint + int` | `nint` | ya lo daba la regla unidireccional; no se toca |
| `nuint + uint` | `nuint` | ídem |

Esos diecisiete quedaron escritos como aserciones en `tests/unit/Cc1CorrectnessTests.cpp`, no en un
programa de usar y tirar. **Se compilaron contra el `ProjectTypeConversion.cpp` real, con
`-Wall -Wextra`, y pasan.**

**Lo que NO está medido todavía y hay que decirlo:** la fixture `c17` no se ha compilado ni
ejecutado, y la comprobación del IR (`= add` cero en un módulo cuya única aritmética es constante)
se escribió contra un stub. Las dos se miden en la corrida de Windows. La del IR es la única que
puede dar un rojo por su propia forma en vez de por un defecto: si falla, lo primero es mirar el IR
que imprime en el mensaje, por si el `add` que encuentra no es de la aritmética constante.

---

## 4b. Resultado de la primera corrida en Windows (2026-09-02)

**El compilador compila y las dos mitades del cierre están medidas.**

| Qué | Resultado |
|---|---|
| `nsc.cc1.integer-evaluator` | **verde** — el cambio del 122 no rompió la matriz del evaluador |
| Las 17 aserciones sobre `commonNumericType` | **verdes** — ninguna aparece en el log de fallos |
| El plegado, en el IR | **verde**, y es la prueba directa del 121 |

El IR emitido por el programa de prueba lleva las dos constantes materializadas:

```llvm
@__nsc_static_0_Mixed_0   = constant i64 2
@__nsc_static_0_Chained_1 = constant i64 42
```

`1 + 1L` se plegó a `2`, y `Mixed + 40L` —un `const` construido sobre otro `const` plegado— se
plegó a `42`. Antes de este cambio el tipo común era `double`, el evaluador entero se rendía, y
ninguna de esas dos líneas habría compilado.

### La aserción del IR estaba mal escrita, y el log lo demostró

La primera versión contaba `= add` en todo el módulo y exigía cero. Falló con «found 1». **El fallo
era del test, no del compilador**, y el propio mensaje lo probó porque imprime el IR: el `add` que
encontró es

```llvm
%interface_map.next = add i64 %interface_map.index, 1
```

el bucle del mapa de interfaces que el backend emite en **todo** módulo, tenga o no aritmética el
programa. Contar instrucciones sueltas por el módulo entero mide el runtime, no el plegado.

Sustituida por la medición directa: que exista una global con el nombre del campo y el valor
plegado dentro. Es lo que el log ya enseñaba, así que se verifica contra salida real y no contra un
modelo que me inventé — que es la regla de este proyecto y que esta vez incumplí.

### La fixture c17: verde

En la primera corrida `nsc.cc1.correctness` salió rojo, pero **ninguna de sus fixtures llegó a
compilarse** — ni la c17 nueva ni las dieciséis anteriores. Todas fallaron con el mismo mensaje:

```
nsc: error: referenced project does not exist:
  'A:\nsc-devel\tests\cc1\c1_constants\../../../lib/NativeSharp.Core/NativeSharp.Core.nsproj'
```

Causa de entorno, no del cambio: `lib/` se sincronizaba en un paso posterior al de ctest. Está en el
doc 83 §4, con su corrección.

Con eso arreglado y el test recompilado:

```
1/1 Test #59: nsc.cc1.correctness ..............   Passed   38.27 sec
100% tests passed out of 1
```

**`c17_constant_promotion` compila y devuelve 42 a `-O0` y a `-O2`**, y con ella las dieciséis
fixtures anteriores. Sumado a la corrida completa anterior —241 de 242 verdes, siendo ésta la única
roja— el Bloque 1a queda cerrado por los dos lados: los siete campos `const` de anchos mezclados se
pliegan, los valores plegados son los correctos en ejecución, y las tres regresiones (IDs 119, 120 y
CC1-103) siguen en pie.

---

## 5. Qué se tocó

| Archivo | Qué |
|---|---|
| `src/Frontend/ProjectIntegerEvaluator.cpp` | ID 122: helper `asIntegerConstant` + el sitio de la llamada |
| `src/Frontend/ProjectTypeConversion.cpp` | ID 121: tabla `kIntegralInfo`, `integerPromotion`, y la llamada guardada entre el 120 y el bucle |
| `tests/cc1/c17_constant_promotion/Main.ns` | fixture nueva |
| `tests/cc1/c17_constant_promotion/c17_constant_promotion.nsproj` | |
| `tests/unit/Cc1CorrectnessTests.cpp` | registro de c17 + 17 aserciones sobre `commonNumericType` + la medición en el IR |
| `run_ids_121_122.bat` | build con warnings-as-errors y las tres corridas de ctest |

**No se tocó `CMakeLists.txt`:** las fixtures `cc1` no se registran ahí, se registran dentro de
`Cc1CorrectnessTests.cpp`. Se comprobó antes de escribir (`grep c16_enum_layout CMakeLists.txt` →
cero).

---

## 6. Lo que mide la fixture c17

Tiene dos mitades y las dos hacen falta:

**Que las constantes SE PLIEGAN.** Un campo `const` cuyo inicializador no sea constante de
compilación no compila. Los siete campos de `Promoted` son la prueba: si el 121 se reabre, el
archivo **deja de compilar**, no deja de dar 42.

**Que el valor plegado es el correcto**, en ejecución. Y los valores están elegidos para separar
rutas, no por gusto: `2^53 + 1` es el primer entero que un `double` no representa, así que
`int + long` en punto flotante daría un resultado distinto y la comprobación fallaría. Un número
cualquiera no separaría las dos rutas.

Más las tres regresiones: ID 120 (`short t = s + 3;` compila), ID 119 (`short s = -3;` cabe, y es lo
que el 122 podía romper) y CC1-103 (el shift toma el tipo del izquierdo).

---

## 7. Hallazgo nuevo, medido, y NO se cierra aquí — ID 126

Al medir la promoción salió un defecto que no es el 121 ni el 122 y que **es P1 por el criterio de
este proyecto**: un programa correcto miente en silencio.

**`byte + byte` da `byte`, y `byte + short` da `short`.**

No lo produce la regla nueva. Lo producen dos cosas que están antes que ella en
`commonNumericType`: la salida temprana por tipos iguales (línea 265) y las dos reglas
unidireccionales (líneas 274-281), donde `byte -> short` ensancha y `short -> byte` no.

**La consecuencia:**

```csharp
byte a = 200;
byte b = 100;
byte c = a + b;   // compila. Vale 44.
```

En C# eso no compila (el tipo de `a + b` es `int` y hace falta un cast escrito). Aquí compila y
desborda en silencio. Y en constantes es peor, porque el evaluador normaliza al tipo común:
`normalize(300, "builtin:byte")` es **44** — está fijado como tal en
`nsc.cc1.integer-evaluator`, donde ese 44 es el resultado *correcto* de la normalización, que es
otra cosa.

**Por qué no se cierra en este bloque.** Cerrarlo cambia el lenguaje: `byte c = a + b;` pasaría a no
compilar sin cast, como en C#. Eso es una decisión sobre la superficie del lenguaje, no una
corrección de un defecto interno, y puede romper código de `lib/` que hoy compila. **Va al registro
como ID 126 y se decide aparte.** Meterlo aquí pondría dos causas posibles en cada rojo del Bloque
1a, que es justo lo que la regla de «un bloque a la vez» existe para evitar.

**Ojo con el orden si se cierra:** la salida temprana por tipos iguales está *antes* que la regla del
ID 120, así que arreglar el 126 no se hace añadiendo otra regla al final — hay que tocar la salida
temprana, y eso pasa por encima de los IDs 119 y 120. Se mide antes de escribir.

---

## 8. Cómo se corre

```
run_ids_121_122.bat
```

Deja `_logs\ids_121_122\` con cinco logs. El orden es deliberado: primero el build con
warnings-as-errors (la tabla nueva vive en un namespace anónimo; si quedara sin usar, `/WX` la caza
ahí y no tres pasos después), luego `nsc.cc1.integer-evaluator` y `nsc.cc1.correctness` por separado
para que un rojo diga cuál, y al final la suite entera.

**Gate del bloque 1a:** `nsc.cc1.correctness` verde con `c17` dentro, y los 242 de la suite verdes.
Con eso cerrado sigue el ID 123 (`stackalloc` hardening) y la fixture `c18`, que es lo que falta
para el gate del Bloque 1 completo.
