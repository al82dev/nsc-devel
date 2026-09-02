# Bug: `1e-300` se imprimía como `9.2233720368547758E-299` — 2^63 disfrazado de número

**Encontrado en:** SHR-023, midiendo `String.ToString(double)` contra `printf` de glibc.
**Afectaba a:** `lib/NativeSharp.Core/String.ns`, `FormatFinite` — y, con el mismo mecanismo,
`ToStringFixed`.
**Estado:** arreglado. Clavado en `tests/cc1/c22_console_and_formatting`.

---

## Síntoma

```
ToString(1e-300)   ->   "9.2233720368547758E-299"
```

Sin error, sin excepción, sin bandera. Un número devuelto con la misma cara de seguridad que
cualquier otro, treinta órdenes de magnitud fuera de sitio.

Y el número que salía no era aleatorio: **9223372036854775808 es exactamente 2^63.**

---

## Causa

`FormatFinite` lleva el valor a un entero de `n` cifras multiplicándolo por una potencia de diez:

```csharp
int exponent = DecimalExponent(magnitude);      // -300
int shift = exponent - digitCount + 1;          // -300 - 11 + 1 = -310
scaled = magnitude * ScalePowerOfTen(-shift);   // 1e-300 * 10^310
ulong rounded = (ulong)(scaled + 0.5);
```

**`10^310` no cabe en un `double`.** El máximo finito es 1.7976931348623157e308.

Entonces la cadena entera se desmorona en silencio, paso a paso:

| paso | valor |
|---|---|
| `ScalePowerOfTen(310)` | `+infinito` |
| `1e-300 * infinito` | `+infinito` |
| `(ulong)(infinito + 0.5)` | **2^63** — el casteo satura |
| `FormatUnsignedDigits(2^63)` | `"9223372036854775808"` |
| recortado y con el punto puesto | `"9.2233720368547758E-299"` |

Ninguno de los cinco pasos "falla". Cada uno hace exactamente lo que su tipo permite. El infinito
es un valor legítimo de `double`, y `(ulong)` de un `double` fuera de rango tiene un resultado
definido por la plataforma que no avisa a nadie.

**El bug vive en la frontera entre dos tipos, no dentro de ninguno de los dos.**

---

## Por qué importa más de lo que parece

Es la misma clase de defecto que el **ID 127** —*normalizar a medias y creerse el número*— y que el
**ID 126** —`byte + byte` desbordando en silencio—. Los tres comparten la forma:

> una operación aritmética se sale del rango de su tipo, el lenguaje **no** lo dice, y el valor
> corrupto sigue viajando como si fuera bueno.

Y hay un agravante propio de este: **el rango afectado es justo el que nadie prueba.** Un valor
normal —3.14, 1000, 0.001— nunca llega a pedir una potencia mayor que 10^22. Sólo los exponentes
extremos disparan el camino roto, y son exactamente los que no salen en un ejemplo escrito a mano.
Una fixture de formateo escrita "con casos razonables" habría pasado verde para siempre.

---

## El arreglo

Escalar **en dos pasos** cuando el factor no cabe en uno:

```csharp
else if (-shift > 300)
{
    // EN DOS PASOS, y esto no es una micro-optimizacion: 10^314 NO CABE en un
    // `double`. Con un solo factor el producto seria infinito, el casteo a `ulong`
    // daria 2^63 y `1e-300` se imprimiria como `9.22e-299` -- el numero mas pequeno
    // del rango saliendo como otro completamente distinto, sin un solo aviso. El
    // primer paso deja `scaled` cerca de 1 y el segundo cabe de sobra.
    scaled = magnitude * PowerOfTenBinary(300);
    scaled = scaled * PowerOfTenBinary(-shift - 300);
}
```

El primer producto deja `scaled` cerca de 1; el segundo cabe con margen. Dos redondeos en vez de
uno, a cambio de que el resultado exista.

`ToStringFixed` tenía el mismo agujero por otro camino: el guardia miraba **la magnitud**
(`magnitude >= 1e18`) cuando lo que tiene que caber es **el producto**. `29128246773.459644` con
nueve decimales pasa el guardia por magnitud y desborda igual, devolviendo otra vez
`9223372036.854775808`. El guardia ahora mira el valor escalado, y el tope está medido en 1e13
(doc 92 §6).

---

## Cómo se detectó

**No leyendo el código.** El comentario que había en `FormatFinite` explicaba correctamente la
pérdida de precisión de las potencias de diez y no mencionaba el desbordamiento, porque quien lo
escribió —yo— no lo había visto.

Se detectó portando las funciones nuevas literalmente a C++ y comparándolas contra
`printf("%.*g")` de glibc sobre 240.000 valores por ancho, cubriendo el rango de exponentes entero
de 1e-300 a 1e300. La discrepancia salió en la primera corrida, en la primera línea:

```
DIFIERE  1e-300  sig=15   mio=9.2233720368547758E-299   ref=1E-300
```

Es la regla del proyecto funcionando: **ninguna verificación cuenta si el modelo contra el que
verifico lo construí yo.** El modelo aquí es glibc, y el barrido cubre el rango completo del tipo
en vez de los valores que a uno se le ocurren.

---

## La regresión

`tests/cc1/c22_console_and_formatting/Main.ns`, dentro de `FloatFormatting()`:

```csharp
// EL DESBORDAMIENTO QUE ENCONTRO EL MODELO. Sin la escala en dos pasos esto imprime
// "9.2233720368547758E-299", que es 2^63 disfrazado de numero.
raii string tiny = String.ToString(1.0E-300);
if (!String.Equals(tiny, "1E-300")) { return 0; }

raii string huge = String.ToString(1.0E+300);
if (!String.Equals(huge, "1E+300")) { return 0; }
```

Comparación de cadena **exacta**, no con tolerancia: un valor treinta órdenes de magnitud fuera de
sitio pasaría cualquier tolerancia razonable, y una tolerancia que lo atrapara no dejaría pasar
nada.
