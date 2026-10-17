# NSC 3.0.5 — R04B1: cadenas `else if` iterativas

## SCOPE LOCK

- **Repositorio/rama:** `A:\nsc-devel`, `development`.
- **Hipótesis única:** `parseIfStatement` se autorreferencia por cada `else if`; la cadena debe
  conservarse en frames heap-backed y materializarse de derecha a izquierda.
- **Archivos autorizados:** `include/nativesharp/Parser/Parser.h`, `src/Parser/Parser.cpp`, la
  regresión focal ya existente en `tests/unit/ProjectMethodBodyModelTests.cpp`, este expediente y el
  tracking `A:\handover-deepseek\restantes-optimizacion-deepseek.md`.
- **Contratos congelados:** AST, ranges, association del `else`, condition declarations, orden y
  texto de diagnostics, sintaxis, semántica, API, ABI, NSMETA, CLI y artefactos 3.0.4.
- **Prohibido:** cambiar la gramática, aumentar stack, reducir la regresión, tocar cuerpos de otros
  statements, ejecutar baseline, suite completa, deploy o instalación.
- **Gate:** 512 ramas `else if` siguen pasando dentro de las 87 comprobaciones focales; `nsc`
  construye; `parseIfStatement` no se llama a sí mismo.
- **Límite honesto:** esto elimina únicamente la autorrecursión de la cadena. La recursión de
  cuerpos anidados por `parseEmbeddedStatementBlock`/`parseBlock` permanece dentro de R04B.

## Diseño

1. `IfParseState` conserva inicio, declaración condicional, condición y bloque `then`.
2. Un bucle consume todas las cláusulas `else if` en orden fuente.
3. El `else` terminal se analiza una sola vez con el camino existente.
4. La cadena se reconstruye en orden inverso, incluyendo el bloque sintético de cada condition
   declaration y usando el cursor final para conservar los ranges inclusivos actuales.

## Estado

R04B1 terminado el 2026-10-17.

## Resultado

- `IfParseState` conserva cada cláusula sin retener frames C++.
- `parseIfHeaderAndBody` mantiene el orden de parseo de condición y cuerpo.
- `parseIfStatement` consume la cadena en un bucle y `lowerIfStatement` la materializa de derecha
  a izquierda, conservando dangling-else y el bloque sintético de condition declarations.
- Prueba focal: `PASS: 87 checks`, incluido el caso permanente de 512 ramas `else if`.
- Construcción target `nsc`: PASS; contrato estático de host runtime: PASS.
- `git diff --check`: PASS.
- Inspección estática: cero llamadas de `parseIfStatement` a sí mismo.

No se ejecutaron baseline, suite completa, deploy ni instalación. R04B continúa con cuerpos y
bloques estructurados anidados.
