# NSC 3.0.5 — R03C1: assignment Pratt iterativo

## Estado

`VERIFICADO EN SU ALCANCE; GATE-F05 PREEXISTENTE QUEDA ABIERTO FUERA DE R03C1`.

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la recursión derecha de assignment y la SCC con lambdas de cuerpo expresión. |
| Naturaleza | Refactor algorítmico, sin cambio funcional. |
| Producto | `nsc`, rama `development`. |
| Símbolos autorizados | `parseAssignmentExpression`, preparación/finalización de lambda de cuerpo expresión y continuaciones privadas indispensables. |
| Contratos protegidos | Precedencia, asociación derecha, doce operadores, `= ref`, topología AST, rangos, cursor, diagnósticos y recovery. |
| Fuera de alcance | Conditional, null-coalescing, range, binary, unary, postfix, primary, lambda con cuerpo bloque, `loop`, `comprehend`, semántica, HIR, MIR, LLVM, ABI y NSMETA. |
| Pruebas inmutables | TP-PAR-427/428, TP-PAR-444 y TP-PAR-445…456. |
| Prohibido | Añadir sintaxis, validar targets en Parser, introducir límite de profundidad, baseline, suite completa, deploy, instalación o cambios de metadata. |
| Criterio de cierre | TP-PAR-551 y regresiones focales PASS, infraestructura completa, funciones tocadas de hasta 60 líneas, diff aislado y tracking actualizado sólo después de la evidencia. |

## Causa demostrada

La gramática vigente ejecutaba esta arista por cada operador:

```text
parseAssignmentExpression
  -> parseConditionalExpression(left)
  -> consume assignment operator
  -> parseAssignmentExpression(right)
```

La asociación era correcta, pero cada enlace retenía un frame nativo. Además, la detección de lambda
precede a conditional y `parseLambdaExpression` volvía a `parseAssignmentExpression` para su cuerpo de
expresión. Por tanto, `x => y => ...` formaba una segunda SCC de la misma frontera.

La complejidad temporal ya era lineal; el defecto era espacio nativo `O(profundidad)`. No se atribuyó
a R03C1 la reentrada que atraviesa conditional, postfix, primary o un bloque: esas rutas pertenecen a
R03C2–C5 y al futuro puente expresión-bloque.

## Diseño implementado

`parseAssignmentExpression` conserva una pila heap de continuaciones:

```text
AssignmentContinuation {
    kind = Assignment | ExpressionLambda
    node
    operation
    reference
}
```

La lectura avanza de izquierda a derecha una sola vez. Cada operador guarda su operando izquierdo,
token y bit `reference`; cada lambda de cuerpo expresión guarda el nodo ya materializado después de
leer prefijos, captures, parámetros, constraints, return type y `=>`. Al encontrar el operando terminal,
las continuaciones se reducen en orden inverso. Ese orden reproduce exactamente el árbol derecho.

Una lambda con `{` conserva la ruta previa de `parseBlock`; R03C1 no intenta convertir el ciclo que
atraviesa un bloque. `parseConditionalExpression` continúa siendo el productor del operando y no fue
modificado, por lo que R03C2–C5 permanecen independientes.

## Invariantes preservados

- Los doce tokens assignment continúan almacenándose sin normalización.
- Sólo `=` puede consumir el contextual `ref` y fijar `AstNode::isReference()`.
- El Parser no valida si el lado izquierdo es asignable.
- Cada nodo mantiene `first=left`, `second=right` y `range=join(left,right)`.
- Lambda conserva hijos de parámetros, contratos auxiliares, flag de cuerpo, `first=body` y rango
  desde el inicio del head hasta el final del body.
- Un RHS ausente sigue delegado a la recuperación de expresiones inferiores.
- El costo objetivo es tiempo `O(N)`, almacenamiento heap `O(N)` y stack nativo `O(1)` para las dos
  cadenas autorizadas.

## Contrafactual TP-PAR-551

El caso nuevo ejecuta tres probes aislados:

1. 32,768 assignments derechos y recorrido iterativo del AST resultante.
2. 32,768 lambdas anidadas de cuerpo expresión y verificación iterativa de parámetros/cuerpos.
3. Los doce operadores, dos enlaces `= ref` y recovery por RHS ausente.

La infraestructura queda registrada en:

- `tests/compiler-suite/host/modules/ParserAssignmentIterationCounterfactualModule.cpp`;
- `tests/compiler-suite/CMakeLists.txt`;
- `tests/compiler-suite/host/include/nsc_suite/TestRegistry.h`;
- `tests/compiler-suite/host/src/main.cpp`;
- `docs/compiler-testing-reengineering/inventory/test-case-design.csv`;
- `docs/compiler-testing-reengineering/inventory/suite-case-allocation.csv`;
- `tests/compiler-suite/manifest/cases.jsonl`.

El manifiesto es la superficie canónica que Builder y `CanonicalCatalog` descubren; no existe un
registro manual adicional específico para TP-PAR-551.

## Evidencia de validación

La validación fue focal: no se ejecutaron baseline, deploy ni batería completa.

- build de `nsc`: PASS; log `A:\temp\r03c1-build-nsc.log`;
- build de `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS; el primer intento detectó únicamente que el literal
  C++ del contrafactual debía escribirse como dos literales adyacentes para evitar `-Wtrigraphs`;
  logs `A:\temp\r03c1-build-suite.log` y `A:\temp\r03c1-build-suite-retry.log`;
- TP-PAR-551 aislado: PASS, tres probes y profundidades de 32,768; log
  `A:\temp\r03c1-tp-par-551.log`;
- Host.Parser completo: PASS 551/551; log `A:\temp\r03c1-host-parser.log`;
- regresiones TP-PAR-427/428/444…456: PASS 15/15; log
  `A:\temp\r03c1-parser-regressions.log`;
- TL-LANG-475 y TL-LANG-349: PASS; logs
  `A:\temp\r03c1-language-assignment-focals.log` y `A:\temp\r03c1-tl349.log`;
- `validate_source_registration.py`: PASS;
- `CanonicalCatalog.load()`: PASS, 6,006 casos, 30 componentes, 682 contratos y TP-PAR-551
  presente;
- símbolos R03C1: una definición y una declaración de cada símbolo autorizado;
- tamaños físicos: `parseAssignmentExpression` 59 líneas,
  `parseLambdaExpressionHead` 23, `finishLambdaExpression` 10 y
  `parseLambdaExpression` 4;
- `git diff --check`: PASS; únicamente avisos de conversión LF/CRLF del checkout.

El PATH de cada proceso se reconstruyó desde Machine+User y `where.exe nslink.exe` resolvió por
nombre exactamente `A:\nslink-devel\bin\release\nslink.exe`.

## Gate preexistente descubierto

TL-LANG-340, TL-LANG-342 y TL-LANG-346 compilan, pero el producto devuelve 102: el RHS de una
asignación o asignación compuesta null-conditional se evalúa también cuando el receptor es nulo.
No es una regresión R03C1:

- TL-LANG-340 reproduce exit 102 con el `nsc` anterior, build hash
  `2581a9b789ffb45e`, fixture idéntico y `--no-artifact-cache`; logs
  `A:\temp\r03c1-tl340-old-build.log` y `A:\temp\r03c1-tl340-old-run.log`;
- TL-LANG-342 reproduce `CURRENT_EXIT=102 PREVIOUS_EXIT=102`; log
  `A:\temp\r03c1-tl342-compare.log`;
- TL-LANG-346 reproduce `CURRENT_EXIT=102 PREVIOUS_EXIT=102`; log
  `A:\temp\r03c1-tl346-compare.log`.

La causa pertenece al lowering/codegen de null-conditional assignment, explícitamente protegido por
el SCOPE LOCK de R03C1. Se registra como hoja absoluta GATE-F05 y no se mezcla con este commit.
