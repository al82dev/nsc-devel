# 88 — ID 126 no es un defecto suelto: es el ID 120 visto por el otro lado

**Fecha:** 2026-09-02
**Qué es:** la medición del ID 126 (`byte + byte` da `byte` y desborda en silencio) hasta el punto
en que la decisión se puede tomar con datos en vez de con una etiqueta de severidad.
**Qué NO es:** una implementación — *cuando se escribió*. **Ya está implementado y medido:** ver el
doc 89 §7. Resultado: `242/242`, `Passed: 80  Failed: 0`, y las veinte fixtures ejecutando.

> ### Resolución — 2026-09-02
>
> Se tomó la opción **C** de §7, con una corrección de alcance que salió al medir: no eran dos
> reglas sino **tres**, y la forma correcta no era parchearlas sino **invertir el orden** — para un
> par de integrales contesta `integerPromotion` y nadie más.
>
> El **mecanismo** es el de C++ (promover primero, reglas ordenadas, dos enteros nunca dan
> `double`); las **respuestas** son las de C# en el único sitio donde C++ muerde (`int + uint` →
> `long`, `long + ulong` → error). El mapa entero y el cierre están en el doc 89.

---

## 1. De dónde sale, exactamente

No sale de la tabla de preferencia, ni del ID 120, ni de `integerPromotion`. Sale de la **tercera
línea** de `commonNumericType` (`ProjectTypeConversion.cpp:378`):

```cpp
if (leftTypeKey == rightTypeKey)
{
    return std::string(leftTypeKey);
}
```

Un cortocircuito de tipos iguales, **antes** de que `integerPromotion` llegue a correr. Con los dos
operandos del mismo tipo estrecho, la función contesta ese tipo y ya. `integerPromotion` —que sabe
perfectamente que el resultado tendría que ser `int`— no se ejecuta nunca.

**Y no es sólo `byte`.** Ese cortocircuito dispara para los cinco tipos más estrechos que `int`:

| Par | Da | En C# daría |
|---|---|---|
| `byte + byte` | `byte` (8 bits, sin signo) | `int` |
| `sbyte + sbyte` | `sbyte` (8, con signo) | `int` |
| `char + char` | `char` (**8, sin signo** — ver `kIntegralInfo`) | `int` |
| `short + short` | `short` (16, con signo) | `int` |
| `ushort + ushort` | `ushort` (16, sin signo) | `int` |

Y aguas abajo el HIR lo obedece: en el caso `Binary`, `numericOperandType = resultType`, así que el
backend emite un `add i8` y **envuelve**. `byte b1 = 200, b2 = 100; byte c = b1 + b2;` compila,
enlaza y `c` vale **44**, sin un solo diagnóstico.

Es literalmente la frase con la que se abrió el doc 86 para el ID 127.

---

## 2. La contradicción interna, que es el argumento fuerte

El ID 81 escribió esta regla para la asignación compuesta, y la escribió **divergiendo de C# a
propósito** (`ProjectMethodBodyModel.cpp`, y el comentario está en el árbol):

> *«C# aqui mete un cast implicito de vuelta (`byte b; int n; b += n;` compila y TRUNCA); este
> lenguaje lo rechaza y pide el cast escrito. […] un estrechamiento que el codigo no escribio es
> justo la clase de perdida silenciosa que este compilador viene rechazando en todas partes.»*

Bien. Entonces:

```csharp
byte b; int n;
b += n;          // RECHAZADO -- "un estrechamiento que el codigo no escribio"
byte c = b + b;  // ACEPTADO  -- y trunca sin decir nada
```

**Mismo peligro, reglas opuestas, mismo lenguaje.** El primero se rechaza citando una política; el
segundo la viola en silencio. No es que una de las dos esté mal en abstracto — es que no pueden
estar las dos.

---

## 3. El radio de impacto, medido

**En toda `lib/`: cero líneas.**

En las 5 500 líneas de biblioteca hay **un** `byte` y **un** `short` fuera de firmas
(`Hashing.Byte`, `Hashing.Short`, `NumericLimits`), y **diecisiete** `char`. Ninguno hace
aritmética estrecha. La biblioteca ya la escribe a la manera de C#:

```csharp
// lib/NativeSharp.Core/String.ns:91
return (char)((int)value + 32);
```

Sube a `int`, opera, baja con un cast escrito. Y `NumericLimits.ns` lo documenta como si la regla
ya fuera la de C#:

> *«aca la resta se computa en `int` […] y NO se angosta implicitamente a `short` […] asi que hace
> falta el cast explicito.»* → `return (short)(-32767 - 1);`

**Nadie se apoya en el desbordamiento.** El estilo del propio código ya promueve a mano.

**La asignación compuesta no se toca.** `c14` hace `byte small = 200; small += 55;` y espera 255.
Eso pasa por la regla del tipo del destino (ID 81, y ahora ID 82 para accesores), **no** por
`commonNumericType` — que ahí sólo se usa como comprobación de «operandos compatibles». Cambiar el
tipo común no altera ni una de esas líneas.

---

## 4. La trampa: cerrar el 126 solo deja el lenguaje PEOR

Si se quita el cortocircuito y nada más:

```csharp
byte b;
byte x = b + b;   // ahora ERROR: el resultado es int
byte y = b + 1;   // sigue compilando y sigue envolviendo
```

Porque `b + 1` no pasa por el cortocircuito: pasa por la **regla del ID 120**
(`adaptsFromDefaultLiteral`), que hace que una constante literal `int` que cabe adopte el tipo
estrecho del otro operando. Esa regla seguiría dando `byte`.

Un lenguaje donde `b + b` es un error y `b + 1` trunca en silencio es **peor** que uno donde las
dos truncan: la inconsistencia no tiene ninguna explicación que un usuario pueda deducir.

**Por eso el ID 126 y el ID 120 son UNA decisión, no dos.**

---

## 5. Y el ID 120 tampoco es lo que su nombre sugiere

El comentario del ID 120 dice que el defecto era que `short s; short t = s + 3;` no compilaba. Vale
la pena decir esto en voz alta: **en C# tampoco compila.** `s + 3` es `int` y asignarlo a `short`
pide un cast.

El defecto REAL que el 120 arregló era otro y sí era grave: `commonNumericType(short, int-const 3)`
devolvía **`double`**. Dos integrales dando punto flotante, y el evaluador de constantes rindiéndose
detrás. Eso había que arreglarlo sí o sí.

Pero había **dos** arreglos posibles, y sólo uno se consideró:

* devolver `short` → ergonómico, **diverge de C#**, y es el origen del ID 126;
* devolver `int` → es C# exacto, arregla el `double` igual de bien, y obliga a `(short)(s + 3)`.

Se eligió el primero. No está escrito en ningún sitio que se comparara con el segundo.

---

## 6. Un argumento que no es teórico: el Bloque 2 es un lexer

Lo siguiente en la ruta es **SHR-024, parsing numérico**. El cuerpo de esa función es esto:

```csharp
int digit = c - '0';
if (digit < 0 || digit > 9) { ... }
```

`char` aquí mide **8 bits sin signo**. Con la regla actual `c - '0'` es de tipo `char`, así que para
cualquier `c` menor que `'0'` el resultado **no es negativo: es 247, o 240, o lo que toque**. La
comprobación `digit < 0` no se cumple nunca y el parser acepta basura.

El defecto no está en el compilador de hoy porque hoy nadie escribe ese código. **Estará el día que
se escriba el lexer, y será exactamente esta línea.**

---

## 7. Las tres opciones, con su precio

| | Qué | Precio |
|---|---|---|
| **A** | **Dejarlo.** La aritmética estrecha se queda estrecha, consistentemente, y envuelve. Divergencia documentada de C# | La clase de defecto silencioso que este proyecto lleva seis documentos persiguiendo, con la contradicción del §2 sin resolver |
| **B** | **Cerrar sólo el 126.** Quitar el cortocircuito | **No hacerlo.** Deja `b + b` como error y `b + 1` truncando. Inconsistencia sin explicación posible (§4) |
| **C** | **Cerrar los dos.** Quitar el cortocircuito **y** la regla del ID 120, y dejar que `integerPromotion` conteste para todo par integral | Deja de compilar `short t = s + 3;` y `sbyte t = s + 3;`. Hay que escribir el cast. En `lib/`: **cero** cambios. En fixtures: `c19::ConstantAdaptsToTheNarrowOperand` **se invierte** (pasa a ser una prueba negativa) y unas pocas líneas piden cast |

---

## 8. Mi recomendación, y por qué

**C.**

Cuatro razones, en orden de peso:

1. **Resuelve la contradicción del §2 en la dirección que el proyecto ya eligió en todas partes.**
   La política escrita es «un estrechamiento que el código no escribió es pérdida silenciosa y se
   rechaza». Aplicarla aquí no añade una regla nueva: quita una excepción.

2. **Quita una divergencia de C# en vez de añadir otra.** El ID 120 introdujo una; C la retira. La
   lista de divergencias deliberadas de este lenguaje es corta y cara de mantener — cada una hay que
   explicarla en el libro, en los diagnósticos y en la cabeza del que porta código.

3. **El código ya está escrito así.** `(char)((int)value + 32)` en `String.ns`, `(short)(-32767 - 1)`
   en `NumericLimits.ns`. La biblioteca lleva desde el principio escribiendo el cast. Lo único que
   cambia es que el compilador dejará de aceptar la otra forma.

4. **Llega antes que el consumidor.** El Bloque 2 es el primer código que de verdad hará aritmética
   de `char`. Cambiar la regla ahora cuesta unas líneas de fixture; cambiarla después cuesta releer
   el lexer entero buscando dónde `c - '0'` mintió.

**El argumento en contra, dicho entero y sin rebajarlo:** este lenguaje existe para hablar con C, y
en C `unsigned char + unsigned char` promueve a `int` igual que en C#… pero el programador de
sistemas *espera* poder escribir `b1 + b2` sobre bytes sin ceremonia. C te obliga al cast igual, así
que la ceremonia no es una invención nuestra — pero es ceremonia, y la opción A la evita. Si la
prioridad es ergonomía sobre el modelo de C#, A es defendible; lo que **no** es defendible es B, ni
dejar la contradicción del §2 en pie sin decidir.

---

## 9. Cómo se mediría, si se hace

No es un cambio que se pueda dar por bueno leyendo el diff.

1. **Antes de tocar nada**, escribir las llamadas directas a `commonNumericType` que fijan la
   respuesta esperada para los cinco pares estrechos, más `(estrecho, constante)`. Es como se midió
   el ID 120 y como se descubrió que su primera versión rompía `(-1) + 1L`.
2. Quitar el cortocircuito de tipos iguales **sólo para pares integrales estrechos** — `int + int`,
   `long + long` y los flotantes tienen que seguir contestando en una línea, y `type:X + type:X`
   para tipos definidos por el usuario ni siquiera llega hasta aquí.
3. Quitar `adaptsFromDefaultLiteral` y sus dos llamadas.
4. Correr `all.bat`. Lo que se rompa dice exactamente cuánto código dependía de la regla vieja — y
   ese número **es** la medida del coste, no una estimación.
5. Invertir `c19::ConstantAdaptsToTheNarrowOperand` y añadir a `c14` la mitad que hoy no existe:
   que `byte c = b1 + b2;` **no compile**, y que `(byte)(b1 + b2)` sí y valga 44.

---

## 10. Lo que queda decidido aquí, pase lo que pase

Independientemente de la opción que se elija, **el ID 126 está mal catalogado**. No es «un defecto
P1 suelto»: es la consecuencia visible de una decisión de diseño que ya se tomó en el ID 120, y su
ficha en el inventario debería decir eso — junto con el ID 120, y como una sola entrada.

Un inventario que lista los dos por separado invita a arreglar uno solo, que es la única de las tres
opciones que empeora el lenguaje.
