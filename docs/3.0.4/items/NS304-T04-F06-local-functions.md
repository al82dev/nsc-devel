# NS304-T04-F06 — Funciones locales genéricas, recursión y autoreferencia segura

- **Estado**: EN IMPLEMENTACIÓN (auditado)
- **Rama**: development
- **Alcance (SCOPE LOCK)**: parser (declaración de función local dentro de bloque),
  `ProjectMethodBodyModel`/`ProjectNameLookup` (registro y análisis de la función local), y la
  prueba `TL-LANG-319`. NO tocar CMake, toolchain, Builder, foreach, rango T01-F04.

## Auditoría (línea base)

- NO existe `parseLocalFunction` ni declaración de función dentro de un bloque/método (el parser
  de statement no despacha a una declaración de función local).
- La infraestructura de callables/lambdas es extensa (olas paralelas, `MethodLambdaBinding`), pero
  una FUNCIÓN LOCAL es un callable nominal nuevo, no un lambda anónimo.
- La recursión de lambda ya se marca (`recursive generic-lambda body type` en :22915) pero es
  parcial; la autoreferencia de una función local nombrada requiere registrar su nombre antes de
  analizar su cuerpo.

## Gap F06

1. Declaración de función local `int f(int x) { ... }` (y `static`/genérica `<T>`) dentro de un
   método.
2. Recursión: la función local debe poder llamarse a sí misma (registro anticipado del nombre).
3. Autoreferencia segura (sin captura accidental del entorno al autoreferenciarse).

## Plan

1. Parser: despachar la declaración de función local desde `parseStatement` dentro de bloques.
2. Sema: registrar la función local como callable nominal (nombre en scope antes de analizar el
   cuerpo) y analizarla en la ola de callables.
3. Prueba `TL-LANG-319`.

## Prueba puntual

`TL-LANG-319`: `int Fact(int n) => n <= 1 ? 1 : n * Fact(n - 1); return Fact(5);` — exit 120.
