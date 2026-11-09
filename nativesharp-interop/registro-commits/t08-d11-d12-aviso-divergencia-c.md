# T08-D11 y T08-D12: aviso cuando una operación entera con un operando C da otro resultado que en C

Fecha: 2026-11-09. Decisión del usuario (punto 1): conservar las reglas de NativeSharp en las expresiones escritas en
NativeSharp e implementar la alternativa 4 de la [investigación](t08-d11-d12-semantica-c-investigacion.md): un
diagnóstico con una corrección explícita cuando corresponda.

**Esto no es interoperabilidad transparente.** Una expresión C copiada tal cual a NativeSharp sigue dando el valor de
NativeSharp. El compilador avisa en los casos que reconoce y propone la conversión que da el valor de C; el programa
cambia solo si el programador aplica la corrección. Los tipos enteros C diferenciados (alternativa 3) quedan como
investigación futura y no se implementan.

## Qué se avisa

El aviso es `warning` (`CppInteropDiagnostic`) y no impide la construcción. La semántica no cambia según el origen
del valor: tipos, valores y sobrecargas son los de NativeSharp, con o sin aviso.

| Caso | Condición | Mensaje (resumen) | Corrección |
|---|---|---|---|
| T08-D11 | `-` o `~` unario sobre un operando C de tipo `byte`, `sbyte`, `short` o `ushort` | NativeSharp conserva el tipo estrecho; C promueve a `int` | `~(int)x`, `-(int)x` |
| T08-D12 | comparación o aritmética entre `int` y `uint` que NativeSharp amplía a `long`, con algún operando C | NativeSharp amplía a `long`; C convierte el `int` a `unsigned int` | `(uint)` delante del operando `int` (el mensaje dice si es el izquierdo o el derecho) |

**Operando C.** Es un valor leído directamente de C, con la definición de «origen directo» de la investigación:

- una global C, también por un `typedef` C (`u8_t`);
- un enumerador C;
- un campo de un record C;
- el resultado de una llamada a una función C;
- un elemento indexado o un dato desreferenciado a partir de uno de los anteriores (`g_bytes[1]`, `*g_pub`).

En T08-D12 basta con que uno de los dos operandos sea C. El otro puede ser un literal (`g_i < 1u`, `-1 < g_u`) o un
local NativeSharp (`int a = -1; a < g_u`).

**No se avisa:**

- con operandos nativos (`byte b = 255; ~b`);
- con la corrección ya escrita (`~(int)g_ub`, `(uint)g_i < g_u`);
- con un valor de tipo enum C (`g_e < g_u`), que ya sigue a C por tipo (T08-009);
- cuando NativeSharp adapta la constante a `uint` y no amplía a `long` (`g_u > 0`, `g_u + 1`).

## Cambios

**Marca de origen** (`ExpressionResult::cppCOrigin`). Solo se usa para diagnosticar; no interviene en tipos ni en la
generación de código.

| Analizador | Inicia o copia la marca |
|---|---|
| `IdentifierAnalyzer.cpp` | global o enumerador C (`isCppExternal`); `*p` copia la de `p` |
| `MemberAccessAnalyzer.cpp` | campo de un record C |
| `IndexAndRangeExpressions.cpp` | elemento: copia la de la base |
| `CallAnalyzer.cpp` | llamada a una función C (identidad canónica C sin cuerpo) |

**Diagnóstico** (`CIntegerDivergence.h`, nuevo, y `OperatorExpressionAnalyzer.cpp`). `analyzeUnary`,
`analyzeBinaryComparison` y `analyzeArithmeticBinary` emiten el aviso en las condiciones de la tabla. El texto
nombra el tipo, la regla de C y la corrección.

**Presentación** (`DriverArtifactGeneration.cpp`). Hasta ahora el driver solo mostraba los diagnósticos cuando la
compilación fallaba. `renderCppInteropWarnings` muestra, tras una compilación correcta, los avisos
`CppInteropDiagnostic` en los tres caminos: ejecutable, biblioteca estática y DLL. Los demás avisos se siguen
tratando como antes.

## Limitaciones

1. **Copia a un local.** `var t = g_ub; ~t` no avisa: la marca no pasa por las variables locales. Es el límite de la
   definición de origen directo (investigación, sección 2), y la prueba lo fija como caso sin aviso.
2. **Asignación compuesta e incremento.** `x += …`, `++` y `--` no forman parte del diagnóstico y no se
   comprobaron.
3. **Resultados intermedios.** El resultado de una operación no es operando C: en `(g_i * g_u) >> 1` se avisa la
   multiplicación, pero no el desplazamiento.
4. **Solo los dos casos de la tabla.** No se diagnostican otras diferencias entre los enteros de C y de NativeSharp,
   como los desplazamientos, `long` frente a `unsigned long` o el desbordamiento con signo.
5. **Caché.** El aviso sale cuando el proyecto se compila. Con `--artifact-cache`, un acierto del paquete de
   compilación restaura el producto sin compilar y no lo vuelve a mostrar.
6. **Plantillas precompiladas.** Un cuerpo genérico se analiza al instanciarlo. El aviso sale al construir el
   consumidor, en las líneas de la fuente de plantilla transportada (`<biblioteca>.nsmeta.public-templates.ns`), no
   al construir la biblioteca. Un método no genérico de la biblioteca avisa al construir la biblioteca.

## Prueba nueva: `pruebas-tanda-08-enteros-c/validar-divergencia-c.py`

Treinta casos, uno por línea de un método `Run`. Cada caso comprueba su valor NativeSharp y `Run` devuelve el número
del primero que difiere.

- **19 casos con aviso:** global, campo, llamada, typedef, elemento y desreferencia para T08-D11; globales, campos,
  llamadas, literal, local y enumerador para T08-D12, en comparación, `/`, `*` y `%`.
- **11 casos sin aviso:** nativos, correcciones (que dan los valores de C: -256, 128, 0, 2147483647, 4294967295),
  valor enum C, constantes adaptadas a `uint` y la copia a un local (limitación 1).

**Comprobado:**

- **Directo:** el conjunto de líneas con aviso coincide exactamente con los 19 casos; la construcción no falla.
  Ejecución en O0–O3 con los valores de NativeSharp.
- **Biblioteca precompilada no genérica:** avisa al construirse, en las mismas líneas.
- **Plantilla precompilada consumida sin encabezado:** el consumidor avisa en las 19 líneas de la fuente
  transportada y se ejecuta en O0 y O2 con los valores de NativeSharp.

`sonda-semantica-c.py` (la instantánea de 38 valores de la investigación) pasa sin cambios: el diagnóstico no
cambia ningún valor.

## Regresión

`claude\scripts\regresion-d11.cmd`: la sonda, el validador nuevo, `regresion-enum.cmd` (enum C, typedef, anchos,
globales atómicas) y `regresion-d13.cmd` (records, campos, globales, punteros, typedef y alias `volatile`, todos con
expresiones sobre operandos C).

Resultado: todo correcto salvo `flexible-c/diagnosticar-var-volatile.cmd`, que termina con 1 por diseño (ver
[T08-D13](t08-d13-alias-volatile-modo-origen.md)).
