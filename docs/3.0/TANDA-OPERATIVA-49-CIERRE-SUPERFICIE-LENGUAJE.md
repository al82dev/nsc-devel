> **Vigencia:** NativeSharp es un lenguaje único y acumulativo; el producto es 1.0. Las referencias históricas a ediciones o versiones de lenguaje y NSMeta que aparezcan a continuación no definen compatibilidad ni deben reintroducirse.

> **Aviso vigente sobre NSMeta:** El versionado de NSMeta ya no aplica. Las menciones de versiones, compatibilidad por versión y requisitos de versión de este documento son históricas y no constituyen un contrato vigente. No debe reintroducirse el versionado de NSMeta ni rechazarse su lectura o compilación por una versión; la aceptación se determina por el contenido y los contratos aplicables.

# Tanda operativa 49 — cierre de superficie del lenguaje NativeSharp

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

La campaña distinguió ideas documentadas de características productivas mediante un manifiesto de
superficie. Las decisiones y la evidencia siguientes corresponden a esa campaña histórica; no son
un inventario del estado actual de cada característica.

En el cierre original, la única característica nueva incorporada fue `raw string literals`. F06
incorpora posteriormente métodos de extensión con parser, lookup, HIR, LLVM y NSMETA productivos.
Los `record struct` y constructores primarios ya habían cerrado su vertical en la tanda 08
(`8509f71f9`) y aparecen formalmente como parte del contrato de aquella campaña.

## Decisiones históricas de la campaña

| Superficie | Decisión histórica | Razón registrada entonces |
|---|---|---|
| `record struct` | Implementado | Parser, layout, metadata y codegen productivos desde T08. |
| Constructores primarios | Implementados | Se bajan a fields y constructor ordinarios antes de name binding. |
| Raw strings | Implementados | Delimitador de tres o más comillas, UTF-8 válido, contenido multiline exacto y sin escapes. |
| `async`/`await` | Diferido | Faltaban state machines productivas, cleanup, pinning, cancelación y metadata de depuración. |
| Streams async | Diferidos | Faltaban ABI de iterador async, backpressure, cancelación y cleanup determinista. |
| `yield`/generadores | Diferidos | Faltaban frame productivo, ownership a través de suspensión y cleanup determinista. |
| Implementaciones default de interfaz | Diferidas | Faltaban reglas de dispatch, coherence y transporte de cuerpos en metadata. |
| List patterns | Diferidos | Faltaban usefulness/exhaustiveness y su integración con borrow/move. |
| Constructores estáticos | Diferidos | Faltaban inicialización once thread-safe, reentrancia y modelo de fallo. |
| `decimal` | Diferido | Faltaban ABI de valor y contratos de aritmética, overflow, conversión y formato. |
| Métodos de extensión | Implementados | Lookup/import deterministas, prioridad de miembros reales, lowering directo y NSMeta. |
| Operadores mediante traits | Implementados | Lang-items estables, witnesses estáticos, reglas builtin preservadas y short-circuit intrínseco. |
| `long double` | Rechazado | El ABI MSVC de Windows x64 lo iguala a `double`; se exigía escribir `double`. |
| Declaraciones en condiciones | Diferidas | Faltaban alcance, definite assignment y cleanup en aristas true/false. |
| Locales `static` | Diferidos | Faltaban once thread-safe, reentrancia, destrucción y lifetime de módulo. |

Estas decisiones conservaron explícitamente el trabajo pendiente de aquella campaña. La situación
vigente de cada característica requiere consultar su implementación y sus pruebas actuales.

## Cambios de implementación

### Manifiesto de superficie

El manifiesto se identifica como `NSLANGSURFACE` y se consulta con:

```text
nsc --print-language-surface
```

Cada línea contiene nombre estable, disposición y razón. El target se fija a
`x86_64-pc-windows-msvc`, consistente con la plataforma de la campaña.

### Diagnóstico `NS3049`

Se añadió `NS3049DeferredLanguageSurface`. En esa campaña el parser lo producía en los contextos
siguientes; la lista no demuestra el estado vigente de cada forma:

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
- La forma se reconoce sin seleccionar una versión del lenguaje.
- El token continúa siendo `StringLiteral`: todos los consumidores posteriores reciben el valor
  decodificado existente y no aparece un segundo tipo de string ni un ABI paralelo.

## Archivos modificados

- `CMakeLists.txt`: registra el nuevo productor de manifiesto y el único target de prueba T49.
- `include/nativesharp/Diagnostics/DiagnosticId.h`: incorpora el código estable `NS3049`.
- `include/nativesharp/Lexer/Lexer.h`: declara la ruta raw del lexer.
- `src/Lexer/Lexer.cpp`: reconoce delimitadores raw, UTF-8 y multiline exacto.
- `src/Parser/Parser.cpp`: reconoce raw strings y emite los rechazos causales.
- `src/Driver/Driver.cpp`: publica `--print-language-surface` y lo documenta en help.

## Archivos nuevos y por qué existen

- `include/nativesharp/Language/LanguageSurface.h`: vocabulario tipado y API única del contrato.
- `src/Language/LanguageSurface.cpp`: tabla normativa, manifiesto y texto causal compartido por
  driver y parser; evita dos listas divergentes.
- `tests/unit/NativeSharpT49LanguageSurfaceTests.cpp`: 27 comprobaciones focalizadas del contrato,
  diagnósticos, edición, decodificación y codegen O0.
- Este documento: conserva decisiones, archivos, evidencia, límites y mejora atribuible al commit.

## Validación dirigida

No se ejecutó CTest completo ni baseline.

1. `nsc` se construyó correctamente en `A:\nsc-devel\build-clang`, Debug/O0.
2. Se construyó sólo `nsc_ns3_t49_language_surface_tests` en el árbol Debug con tests habilitados.
3. Se ejecutó directamente sólo ese binario: **PASS: 27 checks**.
4. El proyecto raw llegó a objeto COFF no vacío usando `--emit-project-oop-obj -O0`.
5. El ejecutable nuevo de `nsc` imprimió el manifiesto completo mediante
   `--print-language-surface`.

La primera versión del witness intentó enlazar un PE y descubrió que el árbol de tests conservaba
una runtime antigua sin `__nativesharp_console_readline`. No se reconstruyó una dependencia ajena ni
se amplió la prueba: se verificó la semántica exacta en lexer y la vertical hasta LLVM/COFF, que es
el límite causal de este cambio de lenguaje.

## Mejora obtenida

- El usuario recibe una respuesta estable y accionable en vez de errores genéricos de parser.
- Tooling, documentación y gates pueden consultar una fuente mecánica de verdad.
- La campaña distinguió async, generators y decimal pendientes de las extensiones que ya tenían
  una vertical productiva; esa clasificación es histórica.
- Raw strings permiten transportar texto, JSON, rutas y fragmentos multilínea sin duplicar cada
  backslash, manteniendo un solo tipo y ABI de string.
- El freeze puede comparar exactamente la superficie `NSLANGSURFACE`.

## Fuera de alcance

- En la entrega original no se implementaron state machines, runtime decimal, extensión de lookup
  ni type initialization; F06 cerró posteriormente sólo la extensión de lookup.
- No se cambió el ABI Win64, NSMETA ni el runtime instalado.
- No se ejecutaron optimizaciones O1/O2/O3.
- No se tocó ni instaló nada en `C:\native-sharp`.
- No se ejecutó baseline.
