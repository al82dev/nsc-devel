# 86 — ID 127: la normalización que hacía media operación

**Fecha:** 2026-09-02
**Severidad:** **P1.** Un programa correcto compilaba, enlazaba y devolvía el número equivocado sin
un solo diagnóstico.
**Encontrado por:** la fixture `c17_constant_promotion`, con código de salida 3, en la primera
corrida que **la ejecutó de verdad**.
**Estado:** **CERRADO.** Confirmado por ejecución el 2026-09-02 — ver §7.

---

## 1. El síntoma

```
[FAIL] cc1_c17_constant_promotion -O0 -- exit 3, se esperaba 42
[FAIL] cc1_c17_constant_promotion -O2 -- exit 3, se esperaba 42
```

Código 3 es la tercera comprobación de la fixture:

```csharp
public const long NegativeWide = (-1) + 1L;
...
if (Promoted.NegativeWide != 0L) { return 3; }
```

Las dos anteriores pasaron: `1 + 1L` valía 2 y `3L + 5UL` valía 8. **Los positivos plegaban bien y
los negativos no.**

Y `ctest` seguía en verde, porque bajo ctest —en Windows— `checkFixture` compila y enlaza pero no
ejecuta: `wineCommand()` devuelve vacío bajo `_WIN32`. La fixture llevaba dos rondas «pasando» sin
que su valor se hubiera mirado nunca.

---

## 2. Cómo se acotó, sin adivinar

**Primero: descartar el sitio obvio.** Se revisaron los veintitantos consumidores de
`ExpressionResult::integerConstant` en el modelo semántico. **Todos** lo pasan a `classifyImplicit`;
ninguno materializa un valor. El número no salía de ahí.

**Segundo: seguir el valor hacia atrás desde el binario.** El backend emite el campo en
`declareStaticFields` (`ProjectOopLlvmLowerer.cpp`):

```cpp
initializer = LLVMConstInt(type, field.constantIntegerValue, isSignedInteger(...) ? 1 : 0);
```

`field.constantIntegerValue` lo pone `ProjectOopMirModel.cpp`:

```cpp
const std::optional<std::uint64_t> value =
    integerConstantValue(initializer.value.get(), staticFields_);
```

**Y ahí estaba el hallazgo: `integerConstantValue` es un TERCER evaluador de constantes enteras.**
Un plegado sobre el árbol HIR, con su propia tabla de anchos (`integerBitWidth`), su propia tabla de
signo (`isSignedIntegralType`) y su propia normalización (`normalizeInteger`). No es el evaluador
canónico ni el del modelo semántico: es una tercera copia.

**Tercero: la copia estaba a medias.**

```cpp
std::uint64_t normalizeInteger(std::uint64_t value, std::string_view typeKey) noexcept
{
    return value & integerMask(integerBitWidth(typeKey));
}
```

Trunca al ancho del tipo **y no extiende el signo**. La operación canónica,
`ProjectIntegerEvaluator::normalize` —cuyo propio comentario la llama «LA OPERACIÓN CENTRAL»— hace
las dos cosas: trunca, y si el tipo tiene signo rellena hacia arriba desde el bit más alto del ancho
real.

---

## 3. La traza

`integerConstantValue` normaliza **cada operando al tipo del RESULTADO**. Para `(-1) + 1L`, cuyo
resultado es `long`:

| Paso | Operación | Valor |
|---|---|---|
| 1 | `-1` es `Unary Minus` sobre `IntegerConstant 1` de tipo `builtin:int` | `normalizeInteger(0 - 1, "builtin:int")` |
| 2 | `0xFFFFFFFFFFFFFFFF & 0xFFFFFFFF` | **`0x00000000FFFFFFFF`** — el signo se pierde aquí |
| 3 | Ensanchado a `long`: `normalizeInteger(4294967295, "builtin:long")` | 4294967295, sin cambio |
| 4 | `+ 1` | **4294967296** |

El campo quedaba con 4294967296 en vez de 0. La constante **se plegaba**, el programa **compilaba y
enlazaba**, y devolvía el número equivocado.

> **Nota de honestidad sobre la medición.** La primera versión de las dos aserciones del IR no
> adjuntaba el módulo al mensaje de fallo, así que el log dijo *que* fallaban pero no *con qué
> valor*. Sin ese número no se puede distinguir «el signo se perdió al ensanchar» (4294967296) de
> «el signo se ignoró» (2). La traza de arriba está derivada del código, no leída de un log, y **la
> corrida que confirme la corrección es la que la valida**. Las aserciones llevan ya el IR en el
> mensaje para que esto no vuelva a costar una corrida.

---

## 4. Por qué nunca se había visto

**El defecto era inalcanzable.** Sólo aparece cuando un valor negativo de un tipo estrecho se
**ensancha** a uno más ancho. Y hasta el ID 121, `int + long` con constantes **no compilaba**: el
tipo común era `double`, el evaluador entero comprobaba `isIntegerType` y se rendía.

Los `const` negativos que sí existían eran de un solo ancho:

```csharp
const int x = -5;
```

Ahí truncar a 32 bits dos veces da el mismo patrón, y `LLVMConstInt(i32, bits, signExtend=1)` lo
interpreta bien. El defecto no tenía por dónde salir.

> **El ID 121 no introdujo este defecto: lo hizo alcanzable.** Es la forma exacta que este proyecto
> ya conoce — cerrar un hueco destapa el siguiente, porque el segundo estaba escondido detrás del
> primero. `sizeof`, `stackalloc`, `new T` y la aritmética de punteros fueron la misma cadena.

Y el propio archivo ya sabía que hacía falta extender el signo: tiene un `signedInteger()` que lo
hace correctamente. Sólo que se usaba en un sitio, el desplazamiento aritmético.

---

## 5. La corrección — y por qué no es un parche

Lo mínimo habría sido añadir la extensión de signo a `normalizeInteger`. **No es lo correcto.**

Esto era un **tercer evaluador de constantes enteras con tablas propias**, que es exactamente lo que
CC1-105 eliminó del modelo semántico *después* de que produjera los defectos C-001, C-002, C-003 y
C-004 de una sola vez. Y falló de la misma manera: no divergió de golpe, **se quedó corta en una
rama**. Parchear la copia deja la copia.

```cpp
std::uint64_t normalizeInteger(std::uint64_t value, std::string_view typeKey) noexcept
{
    return ProjectIntegerEvaluator::normalize(value, typeKey);
}
```

`ProjectIntegerEvaluator::normalize` es `static` y pública; el include no crea ciclo. A partir de
aquí las dos capas **no pueden volver a discrepar**.

**Por qué es seguro para los otros consumidores.** `normalizeInteger` se usa en once sitios. Nueve
son el propio plegado entero, donde extender el signo es lo correcto por definición. Los otros dos
—el plegado a `double`, líneas 234 y 250— pasan su resultado por `signedInteger(bits, typeKey)`, que
**vuelve a enmascarar antes de extender**, así que para ellos el cambio es inocuo. Y
`declareStaticFields` llama a `LLVMConstInt(type, value, signExtend)`, que trunca al ancho del tipo:
para un `const short` los bits altos puestos dan el mismo `i16`.

`integerMask` sigue usándose en `signedInteger`, así que no queda ninguna función sin usar —
comprobado, porque con `/WX` y clang 23 eso sería un build roto (ver doc 83 §3.1e).

---

## 6. Qué se tocó

| Archivo | Qué |
|---|---|
| `src/Frontend/ProjectOopMirModel.cpp` | `normalizeInteger` delega en `ProjectIntegerEvaluator::normalize`; include añadido; el comentario documenta el defecto entero |
| `tests/unit/Cc1CorrectnessTests.cpp` | Las dos aserciones del IR ahora adjuntan el módulo al mensaje de fallo |

Las dos aserciones ya existían de la ronda anterior; lo único nuevo es que ahora imprimen el valor.

---

## 7. El experimento, y su resultado

Era un experimento con **una sola variable**: entre la corrida roja y la siguiente, lo único que
cambió en todo el árbol fue la extensión de signo en esa función.

```
[ OK ] ctest (242 tests)              100% tests passed out of 242
[ OK ] cc1_c17_constant_promotion -O0 -- exit 42
[ OK ] cc1_c17_constant_promotion -O2 -- exit 42
Passed: 78   Failed: 0
```

Las dos aserciones del IR pasan, así que `NegativeWide` es `constant i64 0` y `NegativeSum` es
`constant i64 5`. Y la fixture, **ejecutada de verdad**, devuelve 42 a `-O0` y a `-O2`.

**Con una sola variable cambiada y el resultado invertido, el diagnóstico queda demostrado y no
inferido.** El número intermedio (4294967296) sigue sin haberse leído de un log —la primera versión
de la aserción no lo imprimía— pero la relación causa-efecto sí está medida: quitar la extensión de
signo rompe esas dos constantes, ponerla las arregla, y nada más se tocó.

---

## 8. La lección, que es de método

**El gate del Bloque 1a se dio por cerrado con media medición.** `nsc.cc1.correctness` salió verde y
se leyó como «la c17 pasa». Lo que ese verde demostraba era **que compila y enlaza**: bajo ctest, en
Windows, las fixtures no se ejecutan. La ejecución vivía en una lista escrita a mano dentro de
`run_windows_checklist.bat` que llegaba hasta `c16`.

Dos afirmaciones distintas —«compila» y «da 42»— leídas como una sola. Es la misma clase de error que
el proyecto tiene documentada: **un inventario hecho leyendo etiquetas `case` en vez del cuerpo del
`case`.** Aquí fue leer el nombre de un test verde en vez de lo que ese test mide.

**Lo que lo hizo visible:** añadir las tres fixtures a la lista de ejecución. El defecto llevaba dos
rondas ahí.
