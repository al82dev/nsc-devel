# 93 — Cierre del Bloque 2. Y las dos cosas que se hicieron sin doc

**Fecha:** 2026-09-02
**Qué cierra:** el Bloque 2 entero (SHR-024, SHR-022, SHR-023), más el **ID 128** y la
**paralelización** de la corrida, que se entregaron sin documentar.
**Estado:** entregado y compilado. Los números los pone `all.bat`.

---

## 1. El criterio del bloque, ejecutado

El doc 81 lo dejó escrito antes de empezar:

> **Terminado cuando:** un programa NativeSharp lee `0x1F`, `42UL` y `3.14f` de una cadena y
> escribe un diagnóstico formateado por stderr.

Eso es, literalmente, `BlockTwoAcceptance()` en `tests/cc1/c22_console_and_formatting/Main.ns`:

```csharp
string source = "0x1F 42UL 3.14";
int index = 0;
// ... TryParseIntegerLiteral, TryParseIntegerLiteral, TryParseDouble, todo sobre `index`
Console.ErrorWriteLine(message);
```

Y el mensaje se comprueba carácter a carácter **antes** de escribirlo:

```
c22.ns(1,1): nota: leidos 0x001F, 42 y 3.14 en 14 caracteres
```

Ninguna subcadena cortada, ninguna asignación por token, hexadecimal con ancho, decimal, coma
flotante, y **error estándar**. Las tres superficies del bloque en una frase.

**Que el criterio estuviera escrito antes es lo que hace que cerrar el bloque signifique algo.** Un
criterio redactado al final se escribe para que lo cumpla lo que ya se hizo.

---

## 2. ID 128 — el parser, y cómo se descubrió que mi diagnóstico era falso

**Esto es lo importante del ítem, más que el arreglo.**

Escribiendo `Number.ns` (SHR-024) apareció:

```
Number.ns:523: generic type argument list requires a type
```

en esta línea:

```csharp
if (wide < (long)NumericLimits.Int32Min() || wide > (long)NumericLimits.Int32Max())
```

**Lo primero que hice fue esquivarlo en el `.ns` y anotarlo como «defecto del parser».** Fue el
error del día y está anotado como tal: modificar el código de prueba para que pase es exactamente
lo contrario de arreglar el compilador.

Al construir un banco de pruebas del parser en el contenedor —lexer + parser + AST + diagnósticos,
compilados aparte— **el banco desmintió mi propio diagnóstico**:

| caso | ¿parsea? |
|---|:--:|
| `x < (int)y` | sí |
| `d < (double)0` | sí |
| `a < (b)` | sí |
| `a < (G())` | sí |
| `wide < (long)G() \|\| wide > (long)G()` | **NO** |

No era «`<` seguido de un casteo». Era que `looksLikeGenericCallTypeArguments` escaneaba hacia
delante buscando el `>` que cerraría una lista de argumentos genéricos, y **encontraba el `>` de la
segunda comparación**. Con una sola comparación en la expresión no hay segundo `>` y todo funciona;
por eso los cuatro casos simples pasaban.

El arreglo: cambiar un **conjunto de parada de tres tokens** por una **lista blanca** de lo que sí
puede aparecer dentro de una lista de argumentos de tipo, y rendirse en cuanto aparece cualquier
otra cosa.

```cpp
bool Parser::canAppearInTypeArgumentList(TokenKind kind) noexcept
{
    if (lexer::isBuiltinTypeKeyword(kind) || lexer::isOwnershipQualifierKeyword(kind)) return true;
    switch (kind)
    {
        case TokenKind::Identifier:
        case TokenKind::Dot:
        case TokenKind::Comma:
        case TokenKind::Star:
        case TokenKind::LeftBracket:
        case TokenKind::RightBracket:
        case TokenKind::IntegerLiteral:   // el contador de un arreglo fijo: Foo<float[4]>(x)
        case TokenKind::Less:
        case TokenKind::Greater:
        case TokenKind::GreaterGreater:
            return true;
        default:
            return false;
    }
}
```

**Lista blanca y no lista negra, a propósito.** Una lista negra deja pasar todo lo que nadie pensó
en prohibir, y un escaneo que se va demasiado lejos es precisamente el defecto. Una lista blanca
falla hacia «esto no es genérico», que es la respuesta segura: se pierde un caso genérico exótico y
se gana que ninguna expresión ordinaria se malinterprete.

Y **se revirtió el esquive**: la línea 523 de `Number.ns` volvió a su forma natural, que ahora es
la regresión del ID 128. El arreglo se mide con el código que lo destapó, no con un caso de
laboratorio.

**Lo que este ítem cambió en el método, y vale más que el ítem:** desde aquí, todo lo que se
entrega se **compila en el contenedor primero** — `clang++ -std=c++20 -fsyntax-only` sobre cada
`.cpp` tocado, y el parser real sobre cada `.ns`. La secuencia «entrego, truena, explico por qué es
contradictorio» no termina nunca.

---

## 3. La paralelización, que era el cuello de botella real

`ctest` corría los 242 tests **en serie**. Medido: la suma de los tiempos individuales ≈ el tiempo
de pared. Y los 80 casos del checklist (fixtures + ejemplos, a `-O0` y `-O2`) también, uno detrás
de otro en `.bat`.

Con 128 GB de RAM, NVMe y dieciséis núcleos, eso no es una limitación de la máquina.

**Lo que se hizo:**

* `ctest ... -j %CTEST_JOBS%`, con `CTEST_JOBS` por defecto a `%NUMBER_OF_PROCESSORS%`. Comprobado
  antes que es seguro: cada binario de test crea su propia raíz bajo
  `temp_directory_path() / "<nombre-único>"` y la borra al empezar — **28 raíces distintas**, cero
  solapamiento.
* Las secciones 3 y 4 del checklist reemplazadas por **`run_cases.py`**, un pool de hilos que
  compila y ejecuta cada caso, cada uno **escupiendo su propio log** en `_logs\cases\`, más un
  `results.json` y un `counts.txt` que el `.bat` lee para el código de salida.
* `report.py` consolida todo en **un** `_logs\REPORTE.txt`, que es lo único que hay que leer.

**La carrera que había que ver antes de paralelizar, y que casi no se ve:** `build_project.cmd`
llama a `prepare_libraries.cmd` **en cada invocación**, y eso hace `rmdir /s /q lib\NativeSharp.Core`
seguido de un `xcopy`. En serie es redundante; **en paralelo es una carrera que corrompe `lib/`
mientras otro trabajo la está leyendo**. Por eso cada trabajo pone:

```python
env["NS_LIB_READY"] = "1"   # sin esto, rmdir de lib\ en paralelo = carrera
```

El checklist prepara la biblioteca **una vez**, antes de lanzar el pool.

**Y por qué Python y no `.bat`.** Un pool de trabajos con timeout por trabajo, un log por trabajo,
resultados ordenados por la tabla y no por el orden de terminación, y un JSON con los tiempos, en
`.bat` es un ejercicio de masoquismo con `for /f` y ficheros temporales. La lista de casos vive
ahora en `run_cases.py` y **añadir una fixture es una línea** — que es exactamente lo que hizo falta
para c22.

**Resultado medido en la corrida anterior:** ctest de **250.9 s a 72.8 s**; los casos, 80 en
**22.0 s con `-j 16`** frente a 300.0 s de suma (×13.6); la corrida completa de ~7 min a **3 min
28 s**.

**El siguiente cuello de botella, ya medido:** la construcción (~90 s de los 3:28), y dentro de
ctest, `nsc.cc1.correctness`, que es **un solo binario** recorriendo ~20 fixtures en serie. `-j` no
puede partirlo; habría que partir el test.

---

## 4. Lo que el Bloque 2 enseñó

**El número que promete una API tiene que medirse.** `ToString(double)` iba a prometer quince
cifras porque `DBL_DIG` es quince. La medición contra `printf` de glibc —240.000 valores por
ancho— dio 16.6% de desacuerdo a quince y 0.000% a once. `DBL_DIG` describe lo que un `double`
*guarda*, no lo que un algoritmo concreto *recupera*. Doc 92 §3.

**El defecto grave lo encontró el modelo, no la lectura del código.** `1e-300` se imprimía como
`9.2233720368547758E-299`, que es 2^63 disfrazado de número. Está en la clase de defectos que este
proyecto ya conoce —el ID 127, «normalizar a medias y creerse el número»— y no se ve leyendo.
Doc 92 §4.

**Un ítem del plan puede contener un ítem de otro bloque.** «Formatting de float/double, hex,
vistas y enums» son tres capas: biblioteca, Bloque 3 (vistas: los tipos no existen todavía) y
**compilador** (enums: los nombres no sobreviven a la compilación). Cerrar el bloque exigió
separarlas y anotar el resto con su propio ID, no hacerlas todas ni fingir que estaban.

**Y una del bloque anterior que se confirmó aquí.** El ID 126 se cerró horas antes de que existiera
su primer consumidor. `DigitValue` en `Number.ns` es `code - 48`; escrito de la forma natural sería
`c - '0'`, y hasta el ID 126 `char - char` daba `char` — ocho bits sin signo — así que para
cualquier carácter menor que `'0'` el resultado no era negativo sino 247 y `digit < 0` no se
cumplía nunca. **El parser habría aceptado basura, en silencio.**

---

## 5. Qué se tocó, en total, en este cierre

**SHR-022** (doc 91): `ProjectMethodBodyModel.h/.cpp`, `ProjectOopHirModel.cpp`,
`ProjectOopMirModel.cpp`, `ProjectOopLlvmLowerer.cpp`, `WindowsRuntimeBuilder.cpp`, `LlvmCApi.h`
(+1 símbolo), `Console.ns`.

**SHR-023** (doc 92): `String.ns` (+23 métodos).

**Compartido:** `tests/cc1/c22_console_and_formatting/` (nueva), `Cc1CorrectnessTests.cpp`,
`run_cases.py`, `mkdown/81_inventario_completo_por_bloques.md`.

---

## 6. Lo que sigue

**Bloque 3 — vistas y tiempo de vida.** `StringView` (SHR-010), `Optional<T>` (SHR-017),
`Span<T>`/`ReadOnlySpan<T>` (SHR-011/012) y **escape checking** (SHR-013), que es una regla del
compilador, no un tipo de biblioteca.

El escape checking va **con** los tres tipos y no después, y la razón está medida en el doc 81: una
vista que sobrevive a su dueño es un uso-después-de-liberar, y añadir la comprobación más tarde
obliga a reescribir a todos los consumidores.

`Number.ns` y `String.ns` ya están escritos para recibirlo: el núcleo de parsing trabaja sobre
`(texto, índice)` y una vista es (puntero, longitud) — recibirá su propia sobrecarga sobre el mismo
núcleo, **sin reescribir nada**. El formateo recibirá su `ToString(StringView)` igual.

**Abierto y anotado, no olvidado:** ID 129 (enums por nombre, es del compilador). Y siguen siendo
decisiones tuyas, no de ingeniería: ID 87 (`volatile`), SHR-063 (condicionales de compilación),
ID 125 (UTF-8), ID 124 (probablemente falso).
