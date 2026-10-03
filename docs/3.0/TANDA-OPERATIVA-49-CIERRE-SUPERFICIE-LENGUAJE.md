# Tanda operativa 49 — cierre de superficie del lenguaje NativeSharp 3.0

**Fecha:** 2026-10-01

**Rama:** `development`

**Plataforma:** Windows x64

**Configuración:** Debug/O0

**Baseline:** no ejecutado; reservado para la tanda 53 y operado por Alberto

> **Actualización 2026-10-03:** `NS3-T05-F06` completó la vertical productiva de métodos de
> extensión y reemplazó su disposición por `implemented`. El resto de este documento conserva la
> evidencia del cierre original de T49. Contrato vigente:
> [extension-methods.md](language/extension-methods.md).
> `NS3-T05-F07` añadió posteriormente `trait-operators=implemented`; contrato vigente:
> [operator-traits.md](language/operator-traits.md).

## Resultado

La superficie de lenguaje 3.0 deja de confundir una idea documentada con una característica
productiva. El compilador publica un manifiesto versionado que clasifica cada candidato de esta
tanda como implementado, diferido a 4.0 o rechazado para 3.0. Las formas diferidas que el parser
puede reconocer fallan con el código estable `NS3049` y explican la dependencia técnica que falta;
no se aceptan como sintaxis decorativa ni se simulan mediante helpers desconectados.

En el cierre original, la única característica nueva incorporada fue `raw string literals`. F06
incorpora posteriormente métodos de extensión con parser, lookup, HIR, LLVM y NSMETA productivos.
Los `record struct` y constructores primarios ya habían cerrado su vertical en la tanda 08
(`8509f71f9`) y aparecen formalmente como parte del contrato congelable 3.0.

## Decisiones terminales

| Superficie | Decisión 3.0 | Razón |
|---|---|---|
| `record struct` | Implementado | Parser, layout, metadata y codegen productivos desde T08. |
| Constructores primarios | Implementados | Se bajan a fields y constructor ordinarios antes de name binding. |
| Raw strings | Implementados | Delimitador de tres o más comillas, UTF-8 válido, contenido multiline exacto y sin escapes. |
| `async`/`await` | Diferido a 4.0 | Faltan state machines productivas, cleanup, pinning, cancelación y metadata de depuración. |
| Streams async | Diferidos a 4.0 | Falta ABI de iterador async, backpressure, cancelación y cleanup determinista. |
| `yield`/generadores | Diferidos a 4.0 | Faltan frame productivo, ownership a través de suspensión y cleanup determinista. |
| Implementaciones default de interfaz | Diferidas a 4.0 | Faltan reglas versionadas de dispatch, coherence y transporte de cuerpos en metadata. |
| List patterns | Diferidos a 4.0 | Faltan usefulness/exhaustiveness y su integración con borrow/move. |
| Constructores estáticos | Diferidos a 4.0 | Falta inicialización once thread-safe, reentrancia y modelo de fallo. |
| `decimal` | Diferido a 4.0 | Falta ABI de valor y contratos de aritmética, overflow, conversión y formato. |
| Métodos de extensión | Implementados | Lookup/import deterministas, prioridad de miembros reales, lowering directo y NSMETA 1.32. |
| Operadores mediante traits | Implementados | Lang-items estables, witnesses estáticos, reglas builtin preservadas y short-circuit intrínseco. |
| `long double` | Rechazado en 3.0 | El ABI MSVC de Windows x64 lo iguala a `double`; 3.0 exige escribir `double`. |
| Declaraciones en condiciones | Diferidas a 4.0 | Faltan alcance, definite assignment y cleanup en aristas true/false. |
| Locales `static` | Diferidas a 4.0 | Faltan once thread-safe, reentrancia, destrucción y lifetime de módulo. |

Estas decisiones no eliminan el trabajo futuro. Lo sacan del contrato 3.0 de manera explícita y
evitan que un consumidor interprete un helper de diseño como una promesa de sintaxis o ABI.

## Cambios de implementación

### Manifiesto versionado

Se agregó `NSLANGSURFACE 3.0`, consultable con:

```text
nsc --print-language-3-surface
```

Cada línea contiene nombre estable, disposición y razón. El target se fija a
`x86_64-pc-windows-msvc`, consistente con la plataforma comprometida para 3.0.

### Diagnóstico `NS3049`

Se añadió `NS3049DeferredLanguageSurface`. El parser lo produce en contextos inequívocos:

- declaración `async` y operador `await`;
- sentencia `yield return`/`yield break`;
- cuerpo default de método, propiedad o indexador de interfaz;
- list pattern después de `case`;
- constructor `static`;
- tipo `decimal` y secuencia `long double`;
- declaración en condición de `if`, `while` o `switch`;
- local `static`.

Las palabras contextuales se reconocen sólo en su posición de feature. Por ejemplo, una llamada
ordinaria `await(...)` no se convierte ciegamente en el operador diferido.

### Raw string literals

- El delimitador inicial contiene tres o más comillas.
- El cierre usa el mismo ancho; una corrida más corta pertenece al contenido.
- Backslashes, comillas, espacios, CR/LF y tab se conservan sin interpretación de escapes.
- UTF-8 se valida con las mismas restricciones de scalar encoding que los strings ordinarios.
- Bytes de control distintos de tab/CR/LF producen `InvalidStringLiteral`.
- La forma exige `<Language>3</Language>` mediante `NS3002`.
- El token continúa siendo `StringLiteral`: todos los consumidores posteriores reciben el valor
  decodificado existente y no aparece un segundo tipo de string ni un ABI paralelo.

## Archivos modificados

- `CMakeLists.txt`: registra el nuevo productor de manifiesto y el único target de prueba T49.
- `include/nativesharp/Diagnostics/DiagnosticId.h`: incorpora el código estable `NS3049`.
- `include/nativesharp/Lexer/Lexer.h`: declara la ruta raw del lexer.
- `src/Lexer/Lexer.cpp`: reconoce delimitadores raw, UTF-8 y multiline exacto.
- `src/Parser/Parser.cpp`: exige Language 3 para raw strings y emite los rechazos causales.
- `src/Driver/Driver.cpp`: publica `--print-language-3-surface` y lo documenta en help.

## Archivos nuevos y por qué existen

- `include/nativesharp/Language/Language3Surface.h`: vocabulario tipado y API única del contrato.
- `src/Language/Language3Surface.cpp`: tabla normativa, manifiesto y texto causal compartido por
  driver y parser; evita dos listas divergentes.
- `tests/unit/NativeSharp3T49LanguageSurfaceTests.cpp`: 27 comprobaciones focalizadas del contrato,
  diagnósticos, edición, decodificación y codegen O0.
- Este documento: conserva decisiones, archivos, evidencia, límites y mejora atribuible al commit.

## Validación dirigida

No se ejecutó CTest completo ni baseline.

1. `nsc` se construyó correctamente en `A:\nsc-devel\build-clang`, Debug/O0.
2. Se construyó sólo `nsc_ns3_t49_language_surface_tests` en el árbol Debug con tests habilitados.
3. Se ejecutó directamente sólo ese binario: **PASS: 27 checks**.
4. El proyecto raw llegó a objeto COFF no vacío usando `--emit-project-oop-obj -O0`.
5. El ejecutable nuevo de `nsc` imprimió el manifiesto completo mediante
   `--print-language-3-surface`.

La primera versión del witness intentó enlazar un PE y descubrió que el árbol de tests conservaba
una runtime antigua sin `__nativesharp_console_readline`. No se reconstruyó una dependencia ajena ni
se amplió la prueba: se verificó la semántica exacta en lexer y la vertical hasta LLVM/COFF, que es
el límite causal de este cambio de lenguaje.

## Mejora obtenida

- El usuario recibe una respuesta estable y accionable en vez de errores genéricos de parser.
- Tooling, documentación y gates pueden consultar una fuente mecánica de verdad.
- NativeSharp 3.0 deja de prometer async, generators o decimal a medias; extensions ya tiene una
  vertical productiva propia.
- Raw strings permiten transportar texto, JSON, rutas y fragmentos multilínea sin duplicar cada
  backslash, manteniendo un solo tipo y ABI de string.
- El freeze futuro puede comparar exactamente la superficie `NSLANGSURFACE 3.0`.

## Fuera de alcance

- En la entrega original no se implementaron state machines, runtime decimal, extensión de lookup
  ni type initialization; F06 cerró posteriormente sólo la extensión de lookup.
- No se cambió el ABI Win64, NSMETA ni el runtime instalado.
- No se ejecutaron optimizaciones O1/O2/O3.
- No se tocó ni instaló nada en `C:\native-sharp`.
- No se ejecutó baseline.
