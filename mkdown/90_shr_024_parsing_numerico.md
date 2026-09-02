# 90 — SHR-024: parsing numérico. Arranca el Bloque 2

**Fecha:** 2026-09-02
**Qué cierra:** SHR-024 — el primer ítem del Bloque 2 y el que lo bloquea entero.
**Estado:** implementado y entregado. **Sin medir** — falta `all.bat`. El veredicto lo pone la
corrida.

---

## 1. El hueco, medido antes de escribir

`String.ns` tiene `ToString(int)`, `ToString(long)`, `ToString(ulong)`, `ToString(bool)` y
`ToString(char)` — **toda la dirección de salida**. De entrada:

```
grep -rn "Parse" lib/   →   cero
```

No es que estuviera incompleto: **no existía**. Y un lexer no existe sin él, así que esto era el
tapón del Bloque 2 entero.

---

## 2. La decisión de forma, que decide las otras veinticinco

La API obvia es `TryParse(string, out int)`, a la C#. **Es la equivocada para el único consumidor
que importa.**

Un lexer nunca tiene un `string` con el número. Tiene el archivo entero y un desplazamiento. Con
una API que toma la cadena completa hay que:

* **cortar una subcadena por cada literal numérico** — una asignación y una liberación por token,
  decenas de miles por archivo;
* y **saber dónde termina el número antes de parsearlo**, que es justo lo que el parser está
  averiguando.

Por eso el núcleo toma `ref int index`: **entra apuntando al primer carácter y sale apuntando al
primero que no consumió.** El lexer avanza sobre su propio buffer y no aloca nada.

```csharp
public static bool TryParseUInt64(string text, ref int index, int radix, out ulong value)
public static bool TryParseInt64 (string text, ref int index, int radix, out long  value)
public static bool TryParseDouble(string text, ref int index,            out double value)
public static bool TryParseIntegerLiteral(string text, ref int index,
                                          out ulong magnitude,
                                          out bool hasUnsignedSuffix, out bool hasLongSuffix)
```

Las sobrecargas de conveniencia —`TryParseInt32(string, out int)` y compañía, que exigen consumir
la cadena entera— son **cáscara de tres líneas** encima del mismo núcleo.

**Y esto sobrevive al Bloque 3.** Cuando lleguen `StringView`/`Span<T>`, una vista es
(puntero, longitud): recibirá su propia sobrecarga sobre este núcleo y **no habrá que reescribir
nada**. Escribirlo contra `string` completo habría garantizado la reescritura.

**Por qué `string` y no `char*`.** No hay forma expuesta de sacar un `char*` de un `string`, así que
un núcleo sobre punteros no podría servir a las sobrecargas de cadena sin duplicar el cuerpo. Con
`string` + índice el lexer ya no aloca —que era el problema entero— y no hace falta capacidad nueva.

---

## 3. El desbordamiento se detecta ANTES de multiplicar

```csharp
if (accumulated > (limit - digitValue) / radixValue) { return false; }
accumulated = accumulated * radixValue + digitValue;
```

Mirar el resultado **después** de multiplicar no sirve: el producto ya envolvió y el valor con el
que se compararía es basura. Es la misma clase de error que el ID 127 — normalizar a medias y
creerse el número.

La división entera trunca hacia abajo, y eso es exactamente lo que hace la comparación exacta: si
`accumulated` es como mucho el cociente, el producto cabe; si lo supera aunque sea en uno, no cabe.
No hay margen de seguridad ni holgura: es la frontera real.

**La asimetría del mínimo.** `-9223372036854775808` es válido y `+9223372036854775808` no lo es: el
mínimo con signo es uno más en magnitud que el máximo. Por eso la magnitud se acumula y se compara
**en `ulong`** y sólo se convierte a `long` al final — convertir antes desbordaría en el propio
chequeo.

**Los de 32 bits parsean en 64 y comprueban el rango después.** Un acumulador de 32 bits sería una
segunda copia de la detección de desbordamiento, y dos copias de una regla de rango es exactamente
cómo se separan. CC1-105 ya limpió una duplicación así.

---

## 4. El literal de un lexer, que no es sólo un número

`TryParseIntegerLiteral` lee `0x1F`, `0b1010`, `0o17`, `42`, `42U`, `42L`, `42UL`, `42ul` —
prefijos y sufijos en cualquier orden y cualquier caja.

Dos decisiones que importan:

**Devuelve la magnitud y las banderas por separado, sin decidir el tipo.** Elegir entre
`int`/`uint`/`long`/`ulong` es del analizador semántico, que conoce el contexto. Un lexer que
decidiera el tipo aquí obligaría a decidirlo otra vez allí.

**El signo NO forma parte del literal.** En `x-1` el `-` es un operador binario, y si es unario o
binario lo decide el parser por posición. Un lexer que se comiera el `-` dentro del número rompe esa
expresión. `TryParseIntegerLiteral("-1", ...)` devuelve **false**, a propósito.

---

## 5. Lo que este archivo NO promete, dicho antes de que alguien lo asuma

* **No hay redondeo IEEE-754 exacto** en coma flotante. La mantisa se acumula como entero —sumar
  `dígito / 10^k` en punto flotante pierde precisión en cada paso y además depende del *orden* de
  los dígitos, que es una forma silenciosa de dar dos resultados para el mismo número— y luego se
  escala por una potencia de diez calculada en `double`. Eso deja hasta un par de ULP en los casos
  difíciles. El redondeo correcto necesita aritmética de precisión extendida (Eisel-Lemire, o
  enteros grandes) y **es otro ítem**. Para leer los literales de un compilador basta; para un
  round-trip bit a bit no, y por eso se escribe aquí en vez de descubrirse después.
* **No se aceptan separadores** (`1_000`). Si el lenguaje los admite algún día, entran aquí.
* **No hay culturas ni separador decimal configurable.** Punto, ASCII, y ya.

La fixture compara los flotantes **con tolerancia**, a propósito: comparar por igualdad exacta sería
afirmar una propiedad que este archivo dice explícitamente que no tiene.

---

## 6. La conexión con el ID 126, que no es anecdótica

El cuerpo de `DigitValue` es esto:

```csharp
int code = (int)value;
if (code >= 48 && code <= 57) { return code - 48; }
```

Escrito de la forma natural sería `c - '0'`. **Hasta el ID 126, `char - char` daba `char`** — ocho
bits sin signo — así que para cualquier carácter menor que `'0'` el resultado no era negativo sino
247, y la comprobación `digit < 0` no se cumplía nunca. **El parser habría aceptado basura, en
silencio.**

El ID 126 se cerró horas antes de que existiera su primer consumidor. Esta fixture lo mide de paso.

---

## 7. Qué se tocó

| Archivo | Qué |
|---|---|
| `lib/NativeSharp.Core/Number.ns` | **Nuevo.** El núcleo con posición, los literales con prefijo y sufijo, coma flotante, y las sobrecargas de conveniencia |
| `tests/cc1/c21_number_parsing/` | **Nueva** fixture de ejecución, salida 42 |
| `tests/unit/Cc1CorrectnessTests.cpp` | Registro de c21 |
| `run_windows_checklist.bat` | c21 en la lista de **ejecución** |

`NativeSharp.Core.nsproj` tiene `<Compile Include="**/*.ns" />`, así que el archivo nuevo entra
solo. Y `lib/` **es** la fuente pese al `.source.txt`: `CMakeLists.txt:383` hace
`install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib/ DESTINATION lib)`, y `prepare_libraries.cmd`
copia de vuelta desde el prefijo. Es un viaje de ida y vuelta, y el orden del checklist —construir,
instalar, sincronizar— lo hace consistente.

---

## 8. Cómo está medido

`c21_number_parsing`, ejecutando, salida 42. Siete bloques:

1. **Ida y vuelta decimal** — con signo y sin signo, positivo y negativo, 32 y 64 bits.
2. **Los límites, por los dos lados.** Cada par es *el límite exacto entra* y *el siguiente no*.
   Comprobar sólo que el límite entra dejaría pasar una comprobación escrita demasiado ancha;
   comprobar sólo que el siguiente falla, una demasiado estrecha. Incluye la asimetría: la misma
   magnitud que falla en positivo entra en negativo.
3. **Bases** 2, 3, 8, 16 — y que un dígito fuera de la base **no** es un dígito (`"2"` en base 2 y
   `"3"` en base 3 fallan), sin lo cual la base sería decorativa. Y que una base inexistente se
   rechaza: la **0** dividiría por cero, la **1** no terminaría nunca y la **17** pediría un dígito
   que `DigitValue` no sabe leer.

   > **Corrección posterior (cierre de SHR-023, 2026-09-02).** Aquí la base **3** estaba del lado
   > de las *inválidas*, porque `TryParseUInt64Radix` aceptaba únicamente `2, 8, 10, 16`. Esa
   > enumeración no la pedía el algoritmo: el bucle es `valor * base + dígito` y funciona para
   > cualquier base cuyos dígitos `DigitValue` sepa leer, que son **2–16**. Al escribir
   > `String.ToString(v, base)` para 2–16 (SHR-023) las dos direcciones dejaron de admitir el mismo
   > conjunto, y una fixture que afirma que son inversas no puede pasar con contratos distintos.
   >
   > **Se amplió `Number`, no se estrechó la fixture.** La guarda pasó de enumerar cuatro bases a
   > `radix < 2 || radix > 16`, y esta prueba pasó a medir **más** que antes: la base 3 del lado
   > positivo *con* su dígito fuera de rango, y las tres bases que sí son inválidas de verdad. El
   > razonamiento completo está en el doc 92 §8.
4. **Literales** con prefijo y sufijo, incluido que `-1` **no** es un literal y que `0x` sin dígitos
   tampoco, pero `0` suelto sí.
5. **La posición** — tres números y dos comas en una sola cadena, leídos sin cortar ni una
   subcadena. Es la razón de ser de la forma de la API. Más: cuando falla, **el índice no se mueve**.
6. **Coma flotante** — entero, fracción, exponente positivo y negativo, signo explícito, y los
   rechazos (`.`, `-`, `e5`). Y `"1e"`, que es el número 1 seguido de una `e` que no es parte del
   número: el núcleo se para en el índice 1 y la sobrecarga que exige consumirlo todo lo rechaza.
7. **Basura** — vacío, letras, sobras delante y detrás, doble signo.

---

## 9. Lo que sigue en el Bloque 2

**SHR-022** — `Console.Error` y `Console.Write`. `Console` tiene **un** método, `WriteLine(string)`.
Sin stderr un compilador no puede emitir diagnósticos.

**SHR-023** — formateo de float/double, hexadecimal, vistas y enums. Es **ampliación, no
construcción**: `Format(string, string[])` y los `ToString` de enteros ya existen.

El bloque termina cuando un programa NativeSharp lee `0x1F`, `42UL` y `3.14f` de una cadena y
escribe un diagnóstico formateado por stderr. **La primera mitad de esa frase ya está.**
