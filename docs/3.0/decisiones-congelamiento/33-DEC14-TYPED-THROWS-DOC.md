> **Vigencia:** NativeSharp es un lenguaje único y acumulativo; el producto es 1.0. Las referencias históricas a ediciones o versiones de lenguaje y NSMeta que aparezcan a continuación no definen compatibilidad ni deben reintroducirse.

> **Aviso vigente sobre NSMeta:** El versionado de NSMeta ya no aplica. Las menciones de versiones, compatibilidad por versión y requisitos de versión de este documento son históricas y no constituyen un contrato vigente. No debe reintroducirse el versionado de NSMeta ni rechazarse su lectura o compilación por una versión; la aceptación se determina por el contenido y los contratos aplicables.

# Congelamiento histórico DEC-14 — `typed-throws.md` corregido contra el compilador

Fecha: 2026-10-26. Revisión de partida: `7f6214d00`.

## Decisión aplicada

DEC-14: reescribir `docs/3.0/language/typed-throws.md` para que diga lo que hace el compilador. Revisé el
alcance por edición y las versiones MIR/NSMETA que cita.

## Problema

El documento afirmaba tres cosas que el compilador no hace:

1. El texto atribuía la cláusula a una edición concreta. La cláusula y sus comprobaciones no dependen de ella:
   `Parser::parseThrowsClause` no la consulta, y las sondas históricas lo confirmaron.
2. El texto afirmaba que la cláusula sólo se habilitaba con una etiqueta de proyecto. La cláusula se aceptaba
   sin esa condición y una llamada sin manejar se rechazaba en ambas configuraciones de la campaña.
3. El texto asociaba el contrato con números concretos de MIR y NSMeta, lo que parecía limitar su presencia.
   NSMeta ya no tiene versionado: su compatibilidad se decide por el contenido.

La frase falsa viene de `71156c746` (t07-f02). C-13 (`8d46bf5cc`) y P-1 (`73b03a394`) la conservaron.

## SCOPE LOCK

- **Cambia.** Sólo `docs/3.0/language/typed-throws.md`, en español y en inglés:
  - primer párrafo: la cláusula y las comprobaciones no dependen de una edición;
  - párrafo de `throws` contextual: palabra clave sólo después de la lista de parámetros o de `get`/`set`,
    identificador en cualquier otra posición;
  - preservar la procedencia del contrato MIR y NSMeta sin exigir una versión de lenguaje o NSMeta.
- **Idéntico.** El compilador, la biblioteca y el resto del documento.
- **Prohibido.** Cambiar el compilador para que el documento viejo fuera cierto.

## Validación

Sondas en `A:\temp\nsc-dec\d19`, `nsc` de `7f6214d00`, `-O0`:

| Sonda histórica | Configuración | Forma | Resultado |
|---|---|---|---|
| T1Ed2CallUnchecked | 2 | llamada a `RemoveFirst` sin `try` | rechazada («neither caught here nor declared») |
| T2Ed3CallUnchecked | 3 | la misma | rechazada |
| T3Ed2Identifier | 2 | `int throws = 3; return throws;` | 3 |
| T3bEd3Identifier | 3 | la misma | 3 |
| T4Ed2Clause | 2 | método `throws Boom` llamado sin manejar | rechazada en la llamada; la cláusula se acepta |
| T4bEd2ClauseCaught | 2 | la misma llamada dentro de `try`/`catch (Boom)` | 5 |
| T5Ed2Throw | 2 | `throw` sin `catch` ni `throws` | rechazada |
| T6Ed3Throw | 3 | la misma | rechazada |
| T7Ed2FilteredCatch | 2 | sólo un `catch (Boom) when (true)` | rechazada (el filtro no cubre) |
| T8Ed2DestructorThrows | 2 | `~Holder() throws Boom { }` | error de sintaxis: un destructor no admite `throws` |
| T9Ed2LambdaInTry | 2 | lambda que llama a `F()` dentro de un `try` | rechazada (el `try` no cubre el cuerpo) |
| T10Ed2Accessor | 2 | `get throws Boom` leído dentro de `try` | 9 |
| T11Ed2AccessorUnhandled | 2 | el mismo accesor sin manejar | rechazada en `get_Value` |

Las demás afirmaciones del documento (override que ensancha, conversión de method group, `extern C` con
`throws`, `catch` que no cubre) ya tienen casos en `tests/unit/NativeSharp3T07ErrorTests.cpp`:
`OverrideWidening`, `DelegateMismatch`, `ExternBoundary`, `UnrelatedCatchPattern` y `NonExhaustiveCatch`.

## No verificado

- `async-generator-errors.md` (elemento 134) queda para su propia revisión.
- Batería completa: queda para el cierre final.
