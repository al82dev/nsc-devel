# Tanda 19, cambio 2: sobrecargas C++ aritméticas ordenadas como Clang

## Defecto

En la [auditoría inicial](../TANDA-19-AUDITORIA-INICIAL.md):

- `g(int)` frente a `g(long)` con `g(5)` (o con un `short`) es ambigua en NativeSharp. Clang elige `g(int)`.
- `g(long)` frente a `g(long long)` con `g(5)` elige `g(long)`. Clang la rechaza por ambigua.

Causa en el código: `selectNamespaceFunctionCandidate` (`Frontend/Bodies/CallAnalyzer.cpp`) ordena por la
conversión NativeSharp. `long` de C++ mide 32 bits en Windows y se proyecta como `int` NativeSharp, así que dos
parámetros C++ distintos tienen el mismo tipo NativeSharp. Su tipo C++ está en
`NamespaceFunctionSymbol::cppParameterTypes` y la resolución no lo consultaba.

## Diseño

Cuando el orden NativeSharp deja empatadas dos o más funciones C++, y en cada posición donde sus parámetros
difieren el argumento y los parámetros son aritméticos, el empate se resuelve con los rangos de [over.ics.scs]:

| Rango | Caso |
|---|---|
| coincidencia exacta | el mismo tipo |
| promoción | un tipo más estrecho que `int` a `int`, y `float` a `double` |
| conversión | el resto |

El tipo C++ del argumento sale del tipo NativeSharp: `int`, `uint`, `long` → `long long`, `short`, etc. Una
candidata es mejor si no es peor en ningún argumento y es mejor en uno. Si queda más de una, la llamada es
ambigua y sus notas son las empatadas.

Una llamada que el orden NativeSharp ya resuelve conserva su elección. Así se mantiene la decisión de T10-009:
`widen(4)` con `widen(long long)` y `widen(double)`, o `h(5)` con `h(long)` y `h(long long)`, elige como
NativeSharp aunque C++ la encuentre ambigua.

## Cambio

- `CallAnalyzer.cpp`:
  - `cppArithmeticArgumentType`, `cppArithmeticConversionRank` y `rankCppArithmeticOverloads`;
  - su uso en `selectNamespaceFunctionCandidate`.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia.

## Evidencia

Fixture nuevo `pruebas-tanda-19-sobrecargas`, con y sin `--dll`. Ocho llamadas:

- `g(5)`, `g(short)`;
- `u(uint)`;
- `w(long)`, con `long long`;
- `d(float)` y `d(double)`;
- `p(short)` y `p(float)`.

Resultado 21212111, igual que Clang, O0–O3. Rechazo genuino, ambiguo también en Clang: `m(int, long)` frente a
`m(long, int)` con `m(1, 1)`.

| Variante | Evidencia |
|---|---|
| Estática | `A:/temp/nsc-t19-sobrecargas-jrbefr53` |
| DLL | `A:/temp/nsc-t19-sobrecargas-9_tbdm7v` |

`pruebas-tanda-17-diagnosticos` esperaba la ambigüedad de `g(int)`/`g(long)`, que ya no existe. Ahora:

- la contraparte llama a `A.g(1)` y devuelve 13, igual que Clang;
- la ambigüedad se comprueba con `k(int, long)`/`k(long, int)` y `k(1, 1)`, junto a un control de Clang que la
  rechaza.

Evidencia: `A:/temp/nsc-t17-diagnosticos-_j5xs42d` (estática) y `A:/temp/nsc-t17-diagnosticos-u4ooal10` (DLL).

## Corrección

La primera versión del cambio (`6df3344b8`) ordenaba con los rangos C++ todas las candidatas viables, también
las que NativeSharp ya resolvía. La regresión agrupada la encontró: `pruebas-tanda-10-integral` fallaba con
`widen(4)` y `narrow(3)` ambiguas, contra la decisión de T10-009. La corrección limita los rangos C++ a los
empates. Evidencia: `A:/temp/nsc-t19-sobrecargas-u01xxgg5`, `A:/temp/nsc-t17-diagnosticos-w3gdjq1s` y
`A:/temp/nsc-t10-integral-jbqr3kk5`.

## Límites

- Donde NativeSharp resuelve y C++ no (T10-009), se conserva la elección de NativeSharp.

- Solo funciones de namespace; las llamadas a métodos de clases C++ conservan el orden NativeSharp.
- Posiciones con punteros, referencias o records solo cuentan si todas las candidatas tienen el mismo tipo allí.
- Una candidata que C++ aceptaría con una conversión que NativeSharp no admite (por ejemplo, un estrechamiento)
  no participa.
