# 92 — SHR-023: formateo. Once cifras, y ese número está medido

**Fecha:** 2026-09-02
**Qué cierra:** SHR-023 — el tercer y último ítem del Bloque 2.
**Estado:** implementado, parseado con el parser real y **medido contra glibc en el contenedor**.
Pendiente de la corrida de Windows.

---

## 1. El hueco, y el comentario que lo defendía

`String.ns` tenía `ToString` de `int`, `long`, `ulong`, `bool` y `char`. De float/double, nada — y
había un comentario explicando por qué:

> *«float/double NO tienen overload aquí a propósito: formatear punto flotante a decimal
> correctamente (clase Grisu/Ryu) es un problema aparte y no existe infraestructura dtoa en el
> compilador todavía — mejor dejarlo sin resolver de forma visible (no compila) que fingir una
> implementación ingenua e incorrecta.»*

**La primera mitad del argumento es cierta. La conclusión no lo era.**

Convertía un problema de **precisión** en un hueco de **capacidad**. Sin `ToString(double)` un
compilador no puede imprimir una constante en un diagnóstico, y eso no se arregla esperando a tener
Ryu. La respuesta correcta no es "no compila" ni "una implementación ingenua": es **una
implementación con una garantía escrita y medida**, y con el sitio exacto donde la garantía se
degrada dicho por delante.

Faltaba también hexadecimal y bases arbitrarias, que no tienen nada que ver con la dificultad de
la coma flotante y llevaban ahí ausentes por arrastre.

---

## 2. Dos preguntas distintas, dos nombres distintos

Es la decisión de diseño del ítem.

```
ToString(valor, base)  ->  SIGNO + MAGNITUD.   ToString(-255, 16)  ==  "-ff"
ToHexString(valor)     ->  PATRÓN DE BITS.     ToHexString(-255)   ==  "ffffff01"
```

C# las mezcla: `Convert.ToString(-255, 16)` da el patrón de bits y `(-255).ToString()` da el signo,
**con el mismo verbo para las dos**. Es una fuente de sorpresas silenciosas: quien escribe
`ToString(x, 16)` esperando leerlo como un número se encuentra con complemento a dos, y quien
quiere volcar una palabra de máquina no sabe si lo está consiguiendo.

Son dos preguntas diferentes — *«escribe este NÚMERO en base 16»* y *«escribe estos BITS»* — y aquí
tienen dos nombres. Es el instinto de C++ (que la representación sea explícita) con ergonomía de C#
(no hay que pasar un flag ni recordar una tabla: el nombre lo dice).

El doble casteo de `ToHexString(int)` es parte de eso y va comentado en el sitio:

```csharp
return FormatUnsignedRadix((ulong)(uint)value, 16, false);
```

Con `(ulong)` a secas, `-1` se leería `ffffffffffffffff` en vez de `ffffffff`. `(uint)` primero
recorta a la palabra de 32 bits, que es la que el llamador está volcando.

---

## 3. El número once, y de dónde sale

Aquí está el trabajo real del ítem.

El instinto dice **quince** — el `DBL_DIG` de IEEE-754. El instinto se equivoca, porque `DBL_DIG`
describe lo que un `double` **guarda**, no lo que este algoritmo puede **recuperar**.

Se portó la implementación a C++ y se midió contra `printf("%.*g")` de glibc, sobre **240.000
valores por ancho**, cubriendo el rango de exponentes entero (1e-300 a 1e300). Tasa de desacuerdo:

| cifras | 1–10 | 11 | 12 | 13 | 14 | **15** | 16 | 17 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| desacuerdo | 0.000% | **0.000%** | 0.020% | 0.161% | 1.764% | **16.584%** | 77.352% | 96.756% |

Y restringido al rango "normal" (|exponente| ≤ 22, donde las potencias de diez son exactas):

| cifras | 11 | 12 | 13 | 14 | 15 |
|---:|---:|---:|---:|---:|---:|
| desacuerdo | 0.000% | 0.003% | 0.046% | 0.476% | 4.604% |

**El acantilado está entre once y doce, y no es casualidad.** El error relativo de escalar por
`10^k` en aritmética de `double` es del orden de 1e-15, que es medio dígito en la posición doce.

**Pedir quince cifras a este algoritmo es imprimir cuatro cifras de ruido con la misma cara de
seguridad que las once primeras** — que es exactamente el defecto que este proyecto llama
*«normalizar a medias y creerse el número»* (ID 127).

A once cifras, los **únicos** desacuerdos que quedaron en las 240.000 muestras fueron **empates
exactos**: `0.25` con una cifra, y dos valores terminados en `...5` exacto, donde glibc redondea al
par y esto redondea alejándose del cero. Esa diferencia está documentada en el código y **es una
decisión**, no un error.

Redondear correctamente a quince o diecisiete cifras necesita aritmética de precisión extendida —
es el **teorema de Steele-White**, no una debilidad de este código — y eso llega cuando llegue
`BigInteger`. Hasta entonces, **once cifras ciertas valen más que quince de las que cuatro
mienten.**

El parámetro sigue ahí: `ToString(x, 17)` para quien quiera ver la representación, con la tabla de
arriba delante para saber lo que está comprando.

`ToString(float)` usa **siete**: un `float` guarda entre seis y nueve cifras decimales, y pedirle
once a un valor que ya se ensanchó a `double` imprime **el error del ensanchamiento**, no el
número. `3.14f` a once cifras es `3.1400001049`, cierto sobre los bits y falso sobre lo que el
programador escribió. Con siete da `3.14`.

---

## 4. El defecto que la medición encontró, y que leer el código no habría encontrado

El modelo falló en `1e-300`. Devolvía `9.2233720368547758E-299`.

Ese número es **2^63 disfrazado**.

`1e-300` a once cifras exige escalar por `10^310`. **`10^310` no cabe en un `double`.** Con un solo
factor el producto es infinito, el casteo a `ulong` da 2^63, y el número más pequeño del rango se
imprime como otro completamente distinto — sin un solo aviso.

```csharp
else if (-shift > 300)
{
    // EN DOS PASOS ... El primer paso deja `scaled` cerca de 1 y el segundo cabe de sobra.
    scaled = magnitude * PowerOfTenBinary(300);
    scaled = scaled * PowerOfTenBinary(-shift - 300);
}
```

**Esto no se encuentra leyendo el código.** Se encuentra midiendo contra un modelo que no escribió
uno mismo — que es la regla del proyecto: *«ninguna verificación cuenta si el modelo contra el que
verifico lo construí yo.»* El modelo aquí es glibc.

La fixture lo clava: `String.ToString(1.0E-300)` tiene que dar `"1E-300"`.

El mismo desbordamiento aparecía en `ToStringFixed` y se arregló con el mismo criterio (§6).

---

## 5. Por qué la exponenciación es binaria

```csharp
private static double PowerOfTenBinary(int exponent)
```

Nueve multiplicaciones para `10^300` en vez de trescientas. **No es sólo velocidad: es precisión.**

Cada multiplicación en coma flotante redondea. Una cadena de trescientas acumula del orden de
trescientos ULP y deja basura ya en la decimotercera cifra. Nueve dejan del orden de nueve ULP, por
debajo de la garantía de once cifras.

Y hay una propiedad que la hace **exacta** en el rango normal: `10^0` … `10^22` son las únicas
potencias de diez representables sin error en un `double`, y los productos intermedios de la
exponenciación binaria hasta `10^22` **también** caen dentro (10^16 = 2^16·5^16, y 5^22 ≈ 2.4e15 <
2^53). Así que para |k| ≤ 22 no hay ni un redondeo.

`DecimalExponent` es búsqueda binaria sobre el exponente por la misma razón: un bucle `m = m / 10`
hasta que `m < 10` hace hasta trescientas divisiones **inexactas sobre el propio valor que luego se
va a imprimir**. La búsqueda binaria hace nueve comparaciones y **no toca** `magnitude`.

Las dos líneas de corrección al final de `DecimalExponent` no son paranoia: `ScalePowerOfTen`
redondea para exponentes grandes, así que la potencia con la que se compara puede caer al otro lado
de la frontera. Corrigen como mucho una posición, y sin ellas el error aparece exactamente en los
valores extremos — que es donde nadie mira.

---

## 6. Punto fijo: trece cifras, también medido

`ToStringFixed(valor, decimales)` da exactamente `decimales` cifras tras el punto, sin recortar
ceros. Es lo que pide una columna alineada, y es una pregunta distinta de "cuántas cifras
significativas" — por eso no es un modo del método anterior.

Medido contra `printf("%.*f")` sobre 320.000 valores, variando el tope del valor escalado:

| tope | 1e18 | 1e17 | 1e16 | 1e15 | 1e14 | **1e13** |
|---:|---:|---:|---:|---:|---:|---:|
| desacuerdo | 6.945% | 2.355% | 1.945% | 0.051% | 0.024% | **0.0006%** |

El límite es **trece cifras en total**, parte entera más decimales. Por encima, la única
multiplicación del método ya no basta para dejar el error por debajo de medio dígito; por encima de
1e18 ni siquiera cabe en un `ulong`.

Pasado el tope el método **cae a la forma general** en vez de devolver basura, y la caída es
**visible** en la salida: aparece notación científica donde se pedía punto fijo. *Una degradación
que no se ve no es una degradación, es un error.*

---

## 7. Lo que NO está aquí, y no por olvido

**VISTAS.** `ToString(StringView)` no se puede escribir antes de que `StringView` exista.
SHR-010/011/012 son el **Bloque 3** por la descomposición del propio proyecto (doc 81). Es orden,
no hueco. Cuando lleguen, reciben su sobrecarga sobre este mismo núcleo.

**ENUMS.** Imprimir el **nombre** de un enum necesita una tabla nombre/valor **generada por el
compilador**: los nombres no sobreviven a la compilación, y ninguna cantidad de código en
`String.ns` los recupera. El mecanismo existe y tiene precedente —`StructToString` en la línea C8
"type root" sintetiza miembros y los resuelve estáticamente— y los datos existen: los miembros de
un enum ya se registran con `isEnumConstant` + `enumConstantValue` en `ProjectMemberModel.h`.

Pero **es un ítem del compilador, no de formateo**, y su sitio es al lado de `StructToString`, no
escondido dentro de un stage de biblioteca. Queda anotado como **ID 129** en el inventario. Meterlo
aquí habría sido cerrar el bloque con un ítem del bloque siguiente dentro.

Sin culturas, sin separadores de millares, sin especificadores de formato tipo `"{0:N2}"`. `Format`
sigue siendo sustitución posicional contra `string[]`; formatear un número exige convertirlo antes,
y ahora **se puede**.

---

## 8. Qué se tocó

| Archivo | Qué |
|---|---|
| `lib/NativeSharp.Core/String.ns` | +23 métodos: bases 2–16, hexadecimal con ancho y caja, `ToString(double/float)` con y sin ancho, `ToStringFixed`. Y el comentario histórico reescrito |
| `tests/cc1/c22_console_and_formatting/` | **Nueva** fixture de ejecución, salida 42 (compartida con SHR-022) |
| `tests/unit/Cc1CorrectnessTests.cpp` | Registro de c22 |
| `run_cases.py` | c22 en la tabla de casos — una línea, que es el punto de haber movido la lista ahí |

---

## 9. Cómo está medido

Tres capas, y ninguna se fía de las otras:

1. **El parser real.** `String.ns` y la fixture se pasan por el `Parser.cpp` del proyecto,
   compilado en el contenedor. No es análisis semántico, pero descarta la clase entera de errores
   de sintaxis antes de tocar la máquina.
2. **El modelo contra glibc.** Las funciones nuevas portadas literalmente a C++ y medidas contra
   `printf`, 240.000 valores por ancho para la coma flotante y 3.800.000 comprobaciones para bases
   y hexadecimal (**3.800.000 / 3.800.000 exactas**). Aquí es donde apareció el desbordamiento de
   `1e-300`.
3. **La fixture, ejecutando en Windows.** Ida y vuelta `String` ↔ `Number` para las catorce bases,
   la distinción signo/bits, el corte a notación científica por los dos lados, los no finitos, y el
   criterio del bloque completo.

La fixture compara **cadenas exactas**, no tolerancias: una vez que la garantía está medida y
escrita, comparar con holgura sería renunciar a lo que se acaba de demostrar. Es lo contrario de
c21, que compara flotantes con tolerancia **porque el archivo dice explícitamente que no tiene esa
propiedad**. La tolerancia se usa donde la promesa es débil, no donde molesta.
