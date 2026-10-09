# Evidencia de método — Tanda 04, Host.Parser

## Universo leído

Se leyeron completos y en orden físico:

| ID | Archivo | Líneas | Bytes | SHA-256 confirmado |
|---|---|---:|---:|---|
| PF-000303 | `include/nativesharp/Parser/Parser.h` | 396 | 17,752 | sí |
| PF-000818 | `src/Parser/Parser.cpp` | 14,066 | 597,108 | sí |

`Parser.cpp` se recorrió en bloques contiguos y se releyeron de forma explícita los intervalos que una salida
truncada podía haber ocultado: 1–400, 1,980–2,040, 3,161–3,210, 3,929–3,985, 6,381–6,410,
11,181–11,225, 11,886–11,955, 12,716–12,785 y 13,500–13,700. No quedó un hueco entre 1 y 14,066.

## Extracción y contraste

1. Se extrajeron 147 definiciones `Parser::` y se comprobó que todas tienen declaración en el header.
2. Se inventariaron por separado clase, tipos nested, estado, 12 helpers anónimos y el guard RAII
   `KnownGenericTypeScope`; total: 174 símbolos.
3. Se extrajeron 20 `DiagnosticId` distintos y 99 `AstNodeKind` distintos usados por la implementación.
4. Se recorrieron los métodos desde cursor/tipos hasta recuperación y, en una segunda dirección, desde cada
   AST/diagnóstico observable hacia su productor.
5. Se separaron 42 contratos estables. Cada contrato tiene símbolos productores y al menos un caso diseñado.
6. Se diseñaron 536 casos lógicos: 525 de frecuencia PR y 11 NIGHTLY. Son filas parametrizables, no 536
   ejecutables.
7. Se contrastaron IDs, referencias símbolo→contrato, contrato→símbolo, caso→contrato, secuencia de IDs,
   paridad Markdown↔CSV y CSV↔JSON.

## Fronteras revisadas

- lifetime y ownership de tokens, arena y diagnósticos;
- cursor, EOF, rangos, progreso y las tres rutas de recuperación;
- lookahead de tipos, nombres conocidos y cierres genéricos `>`, `>>`, `>>=`;
- toda la gramática de tipos, genéricos y constraints;
- imports, namespaces, declaraciones y modifiers;
- síntesis de agregados, variants, opaque, properties/indexers y foreach;
- statements, inline assembly, patrones, match, cleanup y comprehensions;
- todos los niveles de precedencia y las familias unary/postfix/primary;
- reparse de holes interpolados;
- copia profunda, propagación FP, contadores y restauración de contextos;
- rutas recursivas y ausencia de un límite de profundidad central en Parser.

## Límites del trabajo

No se cambió ni ejecutó código, pruebas, builders o infraestructura. No hubo build, baseline, coverage,
instalación, deploy, acceso de escritura a C: ni remotos. Los seis cambios preexistentes permanecieron fuera de
este trabajo.

