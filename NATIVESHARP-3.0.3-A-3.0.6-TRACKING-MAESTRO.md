# NativeSharp 3.0.3 a 3.0.6 — tracking maestro de defectos, lenguaje, rendimiento y calidad

**Fecha de corte:** 2026-10-11  
**Estado del documento:** inventario y planificación; no certifica implementaciones  
**Plataforma normativa:** Windows x64, ABI MSVC, LLVM/Clang 23.x  
**Autoridad de método:** [LINEAMIENTOS-DE-TRABAJO-Y-PORTEO.md](../LINEAMIENTOS-DE-TRABAJO-Y-PORTEO.md)  
**Alcance:** 3.0.3 defectos conocidos, 3.0.4 cierre lingüístico, 3.0.5 rendimiento y 3.0.6 calidad interna  
**Fuera de esta edición:** modificar código, compilar NSC o ejecutar pruebas

---

## 1. Propósito y fuentes

Este documento reúne cuatro inventarios sin reinterpretarlos ni declarar cierres por existencia documental:

- **Parte A / 3.0.3:** los 52 `KnownProductDefect` activos derivados de la unión del
  [manifiesto](tests/compiler-suite/manifest/cases.jsonl) y
  [expected-outcomes.json](nsc-builder/metadata/expected-outcomes.json).
- **Parte B / 3.0.4:** los 147 IDs exactos de
  [NATIVESHARP-V3.0.4-PLAN-DE-DESARROLLO.md](../NATIVESHARP-V3.0.4-PLAN-DE-DESARROLLO.md).
- **Parte C / 3.0.5:** el contenido de rendimiento de
  [NATIVESHARP-3.0.4-REINGENIERIA-PROCESO-COMPILACION.md](../NATIVESHARP-3.0.4-REINGENIERIA-PROCESO-COMPILACION.md),
  reasignado aquí a 3.0.5 para ejecutarse después del cierre lingüístico. El archivo fuente no se renombra ni se
  modifica; su título 3.0.4 queda registrado como deuda documental.
- **Parte D / 3.0.6:** auditoría futura, sin cambios funcionales, de arquitectura, documentación, comentarios,
  estilo, algoritmos y reconstructibilidad según las secciones 9, 10 y 18 del lineamiento canónico.

Fuentes auxiliares de la Parte A:

- [test-case-design.csv](docs/compiler-testing-reengineering/inventory/test-case-design.csv);
- [suite-case-allocation.csv](docs/compiler-testing-reengineering/inventory/suite-case-allocation.csv);
- módulos de prueba bajo `tests/compiler-suite/host/modules/` y `tests/compiler-suite/selfhost/modules/`;
- catálogos `*CaseClassifications.jsonl` de BuildSystem, Packaging, SelfHost.Source y SelfHost.Types;
- commits `5dee730d`, `18ae74f2` y `dc3ee85f`, que son procedencia de la metadata actual, no commits de cierre.

### 1.1 Deudas documentales que no cuentan como progreso

1. El manifiesto contiene 47 filas con `expected_outcome=KNOWN_FAILURE`; cinco KPD adicionales sólo adquieren ese
   outcome mediante `nsc-builder/metadata/expected-outcomes.json`. La unión canónica actual es 52.
2. Tres filas con terminación anormal exacta fueron ajustadas por `18ae74f2`; ese commit corrige clasificación de
   infraestructura, no el defecto de producto.
3. La Parte B nace con 147/147 `PENDIENTE`; no existe `docs/3.0.4/items/` ni hay IDs `NS304-*` versionados en el repo.
4. La fuente de rendimiento todavía se titula 3.0.4. Este maestro la programa como 3.0.5 sin alterar la fuente.
5. El pedido mencionó funciones de hasta 120 líneas. El lineamiento no contiene esa regla: exige **120 caracteres
   por línea** y **máximo 60 líneas ejecutables por función o método** (§9, líneas 2959-2975). Rige el límite de 60.

---

## 2. Estados, transiciones y cálculo de progreso

Estados permitidos:

| Estado | Significado |
|---|---|
| `PENDIENTE` | El contrato está inventariado, pero no existe cierre verificable. |
| `EN_AUDITORIA` | Se está demostrando causa, alcance, dependencias y oráculo antes de editar. |
| `EN_IMPLEMENTACION` | Existe autorización y la causa demostrada se está corrigiendo. |
| `PRUEBA_PUNTUAL_OK` | Pasó sólo la prueba del ítem; no certifica tanda ni parte. |
| `TANDA_CERRADA` | Todos los ítems de la tanda tienen evidencia puntual y cierre documental. |
| `CERTIFICACION_FINAL_OK` | La batería completa final pasó con cero errores del alcance. |
| `BLOQUEADO` | Falta autoridad, dependencia o decisión explícita; no equivale a progreso. |
| `DEUDA_DOCUMENTAL` | Dos fuentes contradicen estado, versión, denominador o procedencia. |
| `NO_APLICA` | El criterio fue auditado y se justificó por qué no aplica. |

Transiciones válidas:

```text
PENDIENTE -> EN_AUDITORIA -> EN_IMPLEMENTACION -> PRUEBA_PUNTUAL_OK -> TANDA_CERRADA
TANDA_CERRADA de la última tanda -> CERTIFICACION_FINAL_OK
cualquier estado -> BLOQUEADO o DEUDA_DOCUMENTAL con evidencia
```

Reglas de cálculo:

- `% ítems cerrados = ítems con cierre verificable / denominador congelado * 100`.
- `EN_AUDITORIA`, `EN_IMPLEMENTACION`, `PRUEBA_PUNTUAL_OK`, `BLOQUEADO` y `DEUDA_DOCUMENTAL` cuentan como 0 % cerrado.
- El porcentaje de tanda sólo usa sus ítems; el de parte usa la suma de sus tandas.
- Una prueba puntual nunca incrementa la certificación final.
- Una contradicción histórica se registra, pero no se elige silenciosamente la versión más favorable.

### 2.1 Regla operativa vinculante para cada parte

1. Congelar primero todo el inventario y auditar todas las causas conocidas; una prueba no se usa para descubrir
   dependencias una por una (§14, líneas 3566-3601).
2. Recorrer todas las tandas en orden.
3. Dentro de cada tanda implementar ítem por ítem y ejecutar únicamente su prueba puntual. Cada corrección conserva
   cambio, Markdown, commit y evidencia propios (§14, líneas 3602-3605).
4. Cerrar la tanda completa y continuar con la siguiente sin ejecutar batería completa, minibaseline ni baseline
   intermedios (§14, líneas 3611-3616).
5. Sólo después del último ítem de la última tanda reconstruir y ejecutar una vez la batería completa aplicable
   (§14, líneas 3607-3609).
6. Si la batería final encuentra errores atribuibles al alcance, registrar todos, reabrir desde la Tanda 1,
   auditar nuevamente todas las tandas, corregir todas las causas conocidas y repetir una única batería completa al
   final del nuevo ciclo (§14, líneas 3622-3629).
7. Repetir ciclos hasta cero causas funcionales conocidas, todas las pruebas puntuales aprobadas y batería final
   completa con el resultado requerido (§14, líneas 3631-3635).
8. Cada evidencia registra `ciclo`, parte, tanda, ítem, revisión, comando puntual, resultado, artefactos y causa.
9. El tracking se actualiza después de cada auditoría, corrección, prueba puntual, cierre de tanda y batería final;
   debe coincidir con la evidencia (§3 líneas 99-104; §17 líneas 4028-4057 y 4500-4551).
10. “Ítem por ítem” no autoriza fragmentación artificial: una hipótesis causal cohesionada sigue siendo una unidad
    comprensible (§8, antipatrones, línea 2199 y siguientes).

Bitácora mínima por ciclo:

| Ciclo | Parte | Inventario congelado | Tandas reabiertas | Evidencia puntual | Batería final | Resultado |
|---:|---|---|---|---|---|---|
| 0 | A-D | Este documento | Ninguna | No ejecutada en esta auditoría | No ejecutada | Inventario inicial |

---

## 3. Resumen ejecutivo

| Parte | Versión | Tandas | Denominador | Cerrados | Progreso | Estado |
|---|---:|---:|---:|---:|---:|---|
| A | 3.0.3 | 14 | 52 defectos | 41 | 78.85 % | `EN_PROGRESO`; A08 G35 cerrado 1/2 |
| B | 3.0.4 | 18 | 147 ítems | 0 | 0 % | `PENDIENTE` |
| C | 3.0.5 | 15 | 149 elementos granulares | 0 | 0 % | `PENDIENTE`; T8 tiene evidencia parcial no terminal |
| D | 3.0.6 | 10 | 10 frentes de auditoría | 0 | 0 % | `PENDIENTE` futuro, sin cambios funcionales |

---

# Parte A — NativeSharp 3.0.3: 52 Known Product Defects

## A.1 Regla de autoridad

El universo se calcula como unión por `case_id` de:

1. 47 filas iniciales del manifiesto con `expected_outcome=KNOWN_FAILURE`;
2. 41 filas con tag `known-product-defect` o `known-failure`;
3. cinco outcomes complementarios en `nsc-builder/metadata/expected-outcomes.json`.

La unión inicial da 52 IDs únicos. Tras cerrar G35, quedan 11 KPD abiertos y 41 casos Required. `KPD`
significa
que la prueba y su oráculo existen y el producto no los satisface; no
significa que la causa de implementación esté completamente diagnosticada. Donde sólo existe contrato/oráculo, la
causa se formula como “contrato no satisfecho” y no se inventa una causa interna.

## A.2 Tandas y denominadores

| Tanda | Componente | Ítems | Cerrados | Estado |
|---|---|---:|---:|---|
| A00 | Host.Language | 7 | 7 | `CERRADA` |
| A01 | Host.Lexer | 1 | 1 | `CERRADA` |
| A02 | Host.AST | 3 | 3 | `CERRADA` |
| A03 | Host.Parser | 21 | 21 | `CERRADA` |
| A04 | Host.Sema | 5 | 5 | `CERRADA` |
| A05 | Host.CFG | 1 | 1 | `CERRADA` |
| A06 | Host.Dominance | 1 | 1 | `CERRADA` |
| A07 | Host.LanguageService | 1 | 1 | `CERRADA` |
| A08 | SelfHost.Diagnostics | 2 | 1 | `EN_PROGRESO`; G35 cerrado |
| A09 | SelfHost.Lexer | 1 | 0 | `PENDIENTE` |
| A10 | SelfHost.Source | 1 | 0 | `PENDIENTE` |
| A11 | SelfHost.Types | 2 | 0 | `PENDIENTE` |
| A12 | BuildSystem | 2 | 0 | `PENDIENTE` |
| A13 | Packaging | 4 | 0 | `PENDIENTE` |
| **Total** |  | **52** | **41** | **78.85 %** |

## A.3 Inventario exacto

En `Evidencia`, `M:n` refiere a la línea actual del manifiesto, `T:` al módulo de prueba y `C:` al catálogo lateral.
Los commits citados son procedencia de metadata vigente. En los 52 casos el cierre funcional continúa pendiente.

Cada ficha conserva tanda, identidad, contrato, estado inicial, procedencia de metadata y estado de cierre.

- **A00 · `TL-LANG-019` / `CL-LANG-002`**
  - Contrato: rechazar tamaño/alineación cero, no potencia de dos y overflow sin publicar layout parcial.
  - Estado inicial: `KNOWN_FAILURE`; M:467; T:`LanguageAggregateSemanticsModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual y consolidado A00 `PASS`; commit de implementación A00: este commit.
- **A00 · `TL-LANG-035` / `CL-LANG-003`**
  - Contrato: rechazar overflow de offset, alineación y tamaño final del layout de closure.
  - Estado inicial: `KNOWN_FAILURE`; M:483; T:`LanguageCallableSemanticsModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual y consolidado A00 `PASS`; commit de implementación A00: este commit.
- **A00 · `TL-LANG-099` / `CL-LANG-008`**
  - Contrato: rechazar un `ExpressionFeature` externo antes de indexar la tabla.
  - Estado inicial: `KNOWN_FAILURE`; M:547; T:`LanguageExpressionSemanticsModule.cpp`; `18ae74f2`.
  - Cierre: `CERRADO`; puntual y consolidado A00 `PASS`; commit de implementación A00: este commit.
- **A00 · `TL-LANG-121` / `CL-LANG-009`**
  - Contrato: dominancia exacta de especializaciones, sin cambios por spellings duplicados.
  - Estado inicial: `KNOWN_FAILURE`; M:569; T:`LanguageGenericSemanticsModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual y consolidado A00 `PASS`; commit de implementación A00: este commit.
- **A00 · `TL-LANG-230` / `CL-LANG-017`**
  - Contrato: summary de ownership malformado falla con error exacto y limpia todo output parcial.
  - Estado inicial: `KNOWN_FAILURE`; M:678; T:`LanguageOwnershipLifetimeModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual y consolidado A00 `PASS`; commit de implementación A00: este commit.
- **A00 · `TL-LANG-250` / `CL-LANG-019`**
  - Contrato: rechazar una familia externa de smart pointer antes de indexar la tabla.
  - Estado inicial: `KNOWN_FAILURE`; M:698; T:`LanguageSmartPointerOperationsModule.cpp`; `18ae74f2`.
  - Cierre: `CERRADO`; puntual y consolidado A00 `PASS`; commit de implementación A00: este commit.
- **A00 · `TL-LANG-262` / `CL-LANG-020`**
  - Contrato: rechazar nombre/alineación/payload inválido y overflow de alineación final del variant.
  - Estado inicial: `KNOWN_FAILURE`; M:710; T:`LanguageSumTypeLayoutModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual y consolidado A00 `PASS`; commit de implementación A00: este commit.
- **A01 · `TL-LEX-075` / `CL-LEX-010`**
  - Contrato: triple raw string vacío produce un único `StringLiteral` con valor vacío.
  - Estado inicial: `KNOWN_FAILURE`; M:828; T:`LexerRawStringModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual y consolidado A01 `PASS`; commit de implementación A01: este commit.
- **A02 · `TA-AST-014` / `CA-AST-002`**
  - Contrato: `bad_alloc` no deja objeto vivo fuera del arena y destruye una vez.
  - Estado inicial: `KNOWN_FAILURE`; M:877; T:`AstDestructiveModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G06 como KPD y Required `PASS`; commit de implementación G06: este commit.
- **A02 · `TA-AST-016` / `CA-AST-002`**
  - Contrato: si lanza el destructor final, limpiar las entradas restantes y terminar controladamente.
  - Estado inicial: `KNOWN_FAILURE`; M:879; T:`AstDestructiveModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G07 como KPD y Required `PASS`; commit de implementación G07: este commit.
- **A02 · `TA-AST-215` / `CA-AST-019`**
  - Contrato: `AstDumper` maneja `TypeSyntax` genérico nulo sin access violation.
  - Estado inicial: `KNOWN_FAILURE`; M:1077; T:`AstDestructiveModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G08 y consolidado A02 como Required `PASS`; commit de implementación G08: este commit.
- **A03 · `TP-PAR-063` / `CP-PAR-007`**
  - Contrato: provenance none/tracked mapea flag exacto; texto inválido diagnostica y recupera.
  - Estado inicial: `KNOWN_FAILURE`; M:1153; T:`ParserTypeModifierContinuationModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G09 como KPD y Required `PASS`; commit de implementación G09: este commit.
- **A03 · `TP-PAR-069` / `CP-PAR-007`**
  - Contrato: elemento allocation inválido/void emite `InvalidType` sin perder progreso.
  - Estado inicial: `KNOWN_FAILURE`; M:1159; T:`ParserTypeModifierContinuationModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G10 como KPD y Required `PASS`; commit de implementación G10: este commit.
- **A03 · `TP-PAR-103` / `CP-PAR-011`**
  - Contrato: ``a < b > (c)`` queda como comparación binaria, no llamada genérica.
  - Estado inicial: `KNOWN_FAILURE`; M:1193; T:`ParserGenericLookaheadContinuationModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G11 como KPD y Required `PASS`; commit de implementación G11: este commit.
- **A03 · `TP-PAR-306` / `CP-PAR-028`**
  - Contrato: declaración con múltiples declarators conserva nodos y orden fuente.
  - Estado inicial: `KNOWN_FAILURE`; M:1396; T:`ParserStatementContinuationModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G12 fortalecido y E2E Parser-Sema-HIR como KPD y Required `PASS`; commit de implementación G12: este commit.
- **A03 · `TP-PAR-309` / `CP-PAR-028`**
  - Contrato: deconstruction conserva binding `ref`, `move` y value exactos.
  - Estado inicial/evidencia: `KNOWN_FAILURE`; M:1399; T:`ParserStatementContinuationModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G13 como KPD y Required `PASS`; commit de implementación G13: este commit.
- **A03 · `TP-PAR-316` / `CP-PAR-029`**
  - Contrato: target y dialect válidos de inline assembly retienen metadata canónica.
  - Estado inicial/evidencia: `KNOWN_FAILURE`; M:1406; T:`ParserInlineAssemblyContinuationModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G14 como KPD y Required `PASS`; commit de implementación G14: este commit.
- **A03 · `TP-PAR-327` / `CP-PAR-029`**
  - Contrato: placeholders por índice/nombre resuelven al operando declarado exacto.
  - Estado inicial/evidencia: `KNOWN_FAILURE`; M:1417; T:`ParserInlineAssemblyContinuationModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G15 como KPD y Required `PASS`; commit de implementación G15: este commit.
- **A03 · `TP-PAR-334` / `CP-PAR-030`**
  - Contrato: condition declaration baja a local con scope seguido de su condición.
  - Estado inicial/evidencia: `KNOWN_FAILURE`; M:1424; T:`ParserControlFlowContinuationModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G16 como KPD y Required `PASS`; commit de implementación G16: este commit.
- **A03 · `TP-PAR-355` / `CP-PAR-031`**
  - Contrato: tuple pattern vacío/nested conserva cero children o jerarquía exacta.
  - Estado inicial/evidencia: `KNOWN_FAILURE`; M:1445; T:`ParserPatternSemanticsModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G17 como KPD y Required `PASS`; commit de implementación G17: este commit.
- **A03 · `TP-PAR-356` / `CP-PAR-031`**
  - Contrato: struct pattern shorthand/explicit alinea nombres y children.
  - Estado inicial/evidencia: `KNOWN_FAILURE`; M:1446; T:`ParserPatternSemanticsModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G18 como KPD y Required `PASS`; commit de implementación G18: este commit.
- **A03 · `TP-PAR-358` / `CP-PAR-031`**
  - Contrato: range pattern inclusivo/exclusivo conserva delimiter y endpoints.
  - Estado inicial/evidencia: `KNOWN_FAILURE`; M:1448; T:`ParserPatternSemanticsModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G19 como KPD y Required `PASS`; commit de implementación G19: este commit.
- **A03 · `TP-PAR-400` / `CP-PAR-034`**
  - Contrato: `foreach` con deconstruction baja ambos bindings de patrón exactamente.
  - Estado inicial/evidencia: `KNOWN_FAILURE`; M:1490; T:`ParserForeachSemanticsModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G20 como KPD y Required `PASS`; commit de implementación G20: este commit.
- **A03 · `TP-PAR-405` / `CP-PAR-034`**
  - Contrato: dos `foreach` secuenciales reciben temporales sintéticos distintos.
  - Estado/evidencia: `KNOWN_FAILURE`; M:1495; T:`ParserForeachContinuationModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G21 como KPD y Required `PASS`; commit de implementación G21: este commit.
- **A03 · `TP-PAR-406` / `CP-PAR-034`**
  - Contrato: `foreach` anidados no colisionan en identidad temporal.
  - Estado/evidencia: `KNOWN_FAILURE`; M:1496; T:`ParserForeachContinuationModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G21 como KPD y Required `PASS`; commit de implementación G21: este commit.
- **A03 · `TP-PAR-422` / `CP-PAR-035`**
  - Contrato: `try` sin `catch` ni `finally` diagnostica constructo incompleto.
  - Estado/evidencia: `KNOWN_FAILURE`; M:1512; T:`ParserCleanupContinuationModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G22 como KPD y Required `PASS`; commit de implementación G22: este commit.
- **A03 · `TP-PAR-431` / `CP-PAR-036`**
  - Contrato: ranges `..`, `..=` y endpoints omitidos retienen forma/flags exactos.
  - Estado/evidencia: `KNOWN_FAILURE`; M:1521; T:`ParserExpressionPrecedenceContinuationModule.cpp`.
  - Procedencia: `5dee730d`. Cierre: `CERRADO`; puntual G23 como KPD y Required `PASS`; commit G23: este commit.
- **A03 · `TP-PAR-450` / `CP-PAR-037`**
  - Contrato: lambda con retorno explícito retiene `TypeSyntax` independiente del body.
  - Estado/evidencia: `KNOWN_FAILURE`; M:1540; T:`ParserLambdaContinuationModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G24 como KPD y Required `PASS`; commit de implementación G24: este commit.
- **A03 · `TP-PAR-476` / `CP-PAR-038`**
  - Contrato: allocation de `void` emite `InvalidType`.
  - Estado inicial: `KNOWN_FAILURE`; M:1566; T:`ParserUnaryExpressionModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G10 como KPD y Required `PASS`; commit de implementación G10: este commit.
- **A03 · `TP-PAR-511` / `CP-PAR-040`**
  - Contrato: interpolación nested segura termina en EOF; profundidad 128 hoy desborda stack.
  - Estado/evidencia: `KNOWN_FAILURE`; M:1601; T:`ParserInterpolationFinalModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual aislada G25 como KPD y Required `PASS`; commit G25: este commit.
- **A03 · `TP-PAR-514` / `CP-PAR-041`**
  - Contrato: clone de `TypeSyntax` profundo, completo e independiente.
  - Estado/evidencia: `KNOWN_FAILURE`; M:1604; T:`ParserConstructionCloneFinalModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G26 como KPD y Required `PASS`; commit de implementación G26: este commit.
- **A03 · `TP-PAR-524` / `CP-PAR-041`**
  - Contrato: spelling/name por kind/aridad exacto y `op_Invalid` fuera de matriz.
  - Estado/evidencia: `KNOWN_FAILURE`; M:1614; T:`ParserConstructionCloneFinalModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; puntual G27 como KPD y Required `PASS`; commit de implementación y cierre A03: este commit.
- **A04 · `TS-SEMA-106` / `CS-SEMA-013`**
  - Contrato: fields `void` y function rechazados ambos con `InvalidType`.
  - Estado/evidencia: `KNOWN_FAILURE`; M:1730; T:`SemaStructLayoutModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; G28 pasó como KPD y Required, con un fixture independiente por frontera diagnóstica; commit G28: este commit.
- **A04 · `TS-SEMA-110` / `CS-SEMA-013`**
  - Contrato: recursión nominal directa e indirecta rechazada por incompletitud.
  - Estado/evidencia: `KNOWN_FAILURE`; M:1734; T:`SemaStructLayoutModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; G29 pasó como KPD y Required, y el guard de orden global `TS-SEMA-092` permaneció `PASS`; commit G29: este commit.
- **A04 · `TS-SEMA-246` / `CS-SEMA-024`**
  - Contrato: `new` de void/function/incomplete rechazado como `InvalidAllocationType`.
  - Estado/evidencia: `KNOWN_FAILURE`; M:1870; T:`SemaAllocationType245To266Module.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; G28 pasó como KPD y Required tras rechazar `TypeKind::Function`; commit G28: este commit.
- **A04 · `TS-SEMA-265` / `CS-SEMA-025`**
  - Contrato: C ABI struct vacío/incompleto/recursivo rechaza y termina.
  - Estado/evidencia: `KNOWN_FAILURE`; M:1889; T:`SemaAllocationType245To266Module.cpp`.
  - Terminal documentado: stack overflow `0xC00000FD`; procedencia `18ae74f2`.
  - Cierre: `CERRADO`; G30 reprodujo el terminal exacto y después pasó como KPD corregido y Required; commit G30: este commit.
- **A04 · `TS-SEMA-317` / `CS-SEMA-030`**
  - Contrato: `SymbolDumper` emite flags, enum, offset y constant sólo cuando existen.
  - Estado/evidencia: `KNOWN_FAILURE`; M:1939; T:`SemaFallthroughDump299To321Module.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; G31 pasó como KPD y Required y cerró A04 5/5; commit G31: este commit.
- **A05 · `TF-CFG-076` / `CF-CFG-010`**
  - Contrato: rechazar módulo/función/terminador inválido sin publicar estado parcial.
  - Estado/evidencia: `KNOWN_FAILURE`; M:2246; T:`CfgBuilderModule.cpp`; `5dee730d`.
  - Cierre: `CERRADO`; G32 pasó como KPD y Required, preservó el primer diagnóstico y cerró A05 1/1; commit G32: este commit.
- **A06 · `TF-DOM-142` / `CF-DOM-018`**
  - Contrato: dominance frontier produce vectores exactos, incluido self-edge en su propia frontera.
  - Estado/evidencia: `KNOWN_FAILURE`; M:2514; T:`DominanceTraversalDump129To192Module.cpp`.
  - Procedencia: `5dee730d`.
  - Cierre: `CERRADO`; G33 pasó como KPD y Required y cerró A06 1/1; commit G33: este commit.
- **A07 · `TF-LANGUAGE-SERVICE-304` / `CF-LANGUAGE-SERVICE-038`**
  - Contrato: rename de keyword builtin rechazado atómicamente, sin edits parciales.
  - Estado/evidencia: `KNOWN_FAILURE`; M:5446; T:`LanguageServiceQueries233To312Module.cpp`;
    `5dee730d`.
  - Cierre: `CERRADO`; G34 pasó como KPD y Required para los doce aliases builtin reservados y cerró A07 1/1;
    commit G34: este commit.
- **A08 · `TSH-00213` / `CSH-DIA-004`**
  - Contrato: JSON diagnóstico conserva campos tipados y escapa tab/control characters exactamente.
  - Estado/evidencia: `KNOWN_FAILURE`; M:5531; T:`SelfHostDiagnosticsRenderingModule.ns`; `5dee730d`.
  - Cierre: `CERRADO`; G35 pasó como KPD y Required con todos los controles U+0000-U+001F escapados;
    commit G35: este commit.
- **A08 · `TSH-00214` / `CSH-DIA-004`**
  - Contrato: JSON warning conserva severity/código/argumento exactos.
  - Estado/evidencia: `KNOWN_FAILURE`; M:5532; T:`SelfHostDiagnosticsRenderingModule.ns`; `5dee730d`.
  - Cierre: `PENDIENTE`.
- **A09 · `TSH-00442` / `CSH-LEX-007`**
  - Contrato: triple raw literal vacío conserva token, valor decodificado, rango y cero errores.
  - Estado/evidencia: `KNOWN_FAILURE`; M:5613; T:`SelfHostLexerLiteralsModule.ns`; `5dee730d`.
  - Cierre: `PENDIENTE`.
- **A10 · `TSH-00903` / `CSH-SRC-008`**
  - Contrato: archivo ausente no se publica como buffer vacío aceptado.
  - Estado/evidencia: `KNOWN_FAILURE` lateral; M:5666; C:`SelfHostSourceCaseClassifications.jsonl`.
  - Procedencia: `dc3ee85f`. Cierre: `PENDIENTE`.
- **A11 · `TSH-01011` / `CSH-TYP-005`**
  - Contrato: count no numérico permanece descriptor Named, no Array de count cero.
  - Estado/evidencia: `KNOWN_FAILURE` lateral; M:5557; C:`SelfHostTypesCaseClassifications.jsonl`.
  - Procedencia: `dc3ee85f`. Cierre: `PENDIENTE`.
- **A11 · `TSH-01023` / `CSH-TYP-007`**
  - Contrato: `IsInteger`/`IsNumeric` dan false para IDs inválidos sin indexar fuera de tabla.
  - Estado/evidencia: `KNOWN_FAILURE` lateral; M:5566; C:`SelfHostTypesCaseClassifications.jsonl`.
  - Procedencia: `dc3ee85f`. Cierre: `PENDIENTE`.
- **A12 · `T12B-00021` / `C12B-BLD-004`**
  - Contrato: rechazar NSLink producer vacío antes de derivar rutas self-host runtime.
  - Estado/evidencia: `KNOWN_FAILURE` lateral; M:5691; C:`BuildSystemCaseClassifications.jsonl`.
  - Procedencia: `dc3ee85f`. Cierre: `PENDIENTE`.
- **A12 · `T12B-00022` / `C12B-BLD-004`**
  - Contrato: rechazar NSLink path malformado antes de publicar rutas self-host runtime.
  - Estado/evidencia: `KNOWN_FAILURE` lateral; M:5692; C:`BuildSystemCaseClassifications.jsonl`.
  - Procedencia: `dc3ee85f`. Cierre: `PENDIENTE`.
- **A13 · `T12B-00457` / `C12B-PKG-001`**
  - Contrato: `deploy.bat` respeta build/prefix del caller y no usa rutas hardcoded.
  - Estado/evidencia: `KNOWN_FAILURE`; M:5707; C:`PackagingCaseClassifications.jsonl`; `5dee730d`.
  - Cierre: `PENDIENTE`.
- **A13 · `T12B-00458` / `C12B-PKG-001`**
  - Contrato: `deploy.bat` acepta y preserva paths con espacios o Unicode.
  - Estado/evidencia: `KNOWN_FAILURE`; M:5708; C:`PackagingCaseClassifications.jsonl`; `5dee730d`.
  - Cierre: `PENDIENTE`.
- **A13 · `T12B-00460` / `C12B-PKG-001`**
  - Contrato: destino drive-root inválido se rechaza antes de publicar.
  - Estado/evidencia: `KNOWN_FAILURE`; M:5710; C:`PackagingCaseClassifications.jsonl`; `5dee730d`.
  - Cierre: `PENDIENTE`.
- **A13 · `T12B-00461` / `C12B-PKG-001`**
  - Contrato: build y prefix iguales se rechazan antes de publicar.
  - Estado/evidencia: `KNOWN_FAILURE`; M:5711; C:`PackagingCaseClassifications.jsonl`; `5dee730d`.
  - Cierre: `PENDIENTE`.

## A.4 Criterio de transición y cierre

Un KPD sólo sale de `PENDIENTE` cuando existe causa raíz demostrada. Sólo sale de `KNOWN_FAILURE` cuando:

1. el producto satisface el oráculo sin relajar la prueba;
2. la prueba puntual pasa en sus variantes declaradas;
3. se retira coherentemente la expectativa KPD del manifiesto o metadata lateral;
4. no queda clasificación lateral contradictoria;
5. el commit y Markdown registran causa, solución y evidencia;
6. la tanda completa cierra antes de avanzar;
7. la Parte A termina únicamente con la batería completa final en cero errores del alcance.

## A.5 Diagnóstico causal previo a implementación

### A.5.1 Cobertura y reglas de lectura

La auditoría estática del ciclo 0 cubre los 52 KPD. Demuestra 51 causas contra la implementación actual y deja
una sola reconciliación pendiente: `TP-PAR-524`. Una causa demostrada no cuenta como defecto cerrado ni como
prueba aprobada. Los productores son funciones o artefactos que originan el comportamiento; los consumidores
son las capas que reciben su salida. La prueba indicada es la puntual existente, no la certificación de Parte A.

Resultado del inventario causal:

- 52/52 identidades tienen fila de manifiesto, contrato, executor y cuerpo registrable.
- 51/52 tienen causa estática demostrada en el productor vigente.
- 1/52 parece clasificación KPD obsoleta y debe reconciliarse mediante su prueba puntual: `TP-PAR-524`.
- 47/52 tienen una prueba puntual adecuada para iniciar corrección causal.
- 4/52 requieren fortalecer el oráculo o definir una frontera observable: `TL-LANG-099`, `TL-LANG-250`,
  `TP-PAR-306` y `TP-PAR-511`.
- `TP-PAR-524` tiene prueba puntual completa, pero el código actual satisface estáticamente su matriz.

### A.5.2 Familias causales, productores y consumidores

- **G01 — aritmética de layout sin overflow: `TL-LANG-019`, `TL-LANG-035`, `TL-LANG-262`.**
  - Productores: `computeWindowsX64AggregateLayout`, `computeClosureLayout` y
    `computeWindowsX64VariantLayout`.
  - Causa: `alignUp`, suma de payload y padding operan en `size_t` sin comprobar suma/redondeo previos.
  - Consumidores: layout de objetos, closures y variants; Frontend y ABI Windows x64.
  - Puntuales: tres módulos `Language*SemanticsModule.cpp`; existen y cubren overflow transaccional.
  - Estado: `CERRADO` en A00; los tres casos pasan individualmente y en el consolidado.
- **G02 — enum externo indexa tablas: `TL-LANG-099`, `TL-LANG-250`.**
  - Productores: `expressionLoweringContract` y `smartPointerFamilyContract`.
  - Causa: convierten el enum a índice y acceden a `std::array` sin validar rango.
  - Consumidores: lowering de expresiones y validación de operaciones smart pointer.
  - Puntuales: existen, pero la API devuelve referencia `noexcept` y carece de canal observable de rechazo.
  - Estado: `CERRADO` en A00; ambas APIs devuelven puntero nullable y validan el índice.
- **G03 — cardinalidad no normalizada: `TL-LANG-121`.**
  - Productor: `specializationStrictlyDominates`.
  - Causa: compara el tamaño del set candidato con el span fallback, que aún contiene duplicados.
  - Consumidores: selección de especializaciones en `ProjectGenericConstraintModel` y MethodBodyModel.
  - Puntual: `LanguageGenericSemanticsModule.cpp`; existe y cubre duplicados/asimetría.
  - Estado: `CERRADO` en A00; ambos lados se normalizan antes de comparar cardinalidad.
- **G04 — decode no transaccional: `TL-LANG-230`.**
  - Productor: `decodeEscapeSummary`.
  - Causa: publica identidad y parámetros conforme avanza; un error posterior deja salida parcial.
  - Consumidores: metadata de escape/ownership y sus lectores.
  - Puntual: `LanguageOwnershipLifetimeModule.cpp`; existe y exige output vacío tras cada error.
  - Estado: `CERRADO` en A00; parseo local, consumo completo de enteros y publicación al éxito.
- **G05 — ancho raw toma toda la corrida: `TL-LEX-075`, `TSH-00442`.**
  - Productores: `Lexer::lexRawString` host y `Lexer.LexRawString` self-host.
  - Causa: seis comillas se consumen como apertura de ancho seis; no quedan tres para el cierre vacío.
  - Consumidores: Parser host y pipeline Lexer/Parser self-host.
  - Puntuales: `LexerRawStringModule.cpp` y `SelfHostLexerLiteralsModule.ns`; ambas existen.
  - Estado: productor host `CERRADO` en A01; `TSH-00442` y el productor self-host permanecen abiertos para A09.
- **G06 — registro de arena no es exception-safe: `TA-AST-014`.**
  - Productor: `AstArena::make`.
  - Causa: ejecuta `new T` antes de `entries_.push_back`; si el vector lanza, el objeto queda huérfano.
  - Consumidores: todo productor de AST que asigna mediante `AstArena`.
  - Puntual: `AstDestructiveModule.cpp` en `nsc_component_tests_destructive`; existe.
  - Estado: `CERRADO` en A02; ownership temporal RAII conserva el objeto hasta registrar la entrada.
- **G07 — destructor lanzable corta cleanup: `TA-AST-016`.**
  - Productor: `AstArena::~AstArena` y su deleter sin frontera de excepción.
  - Causa: una excepción durante destrucción termina el proceso e impide destruir entradas restantes.
  - Consumidores: teardown de todo AST alojado en arena.
  - Puntual: proceso hijo de `AstDestructiveModule.cpp`; existe y verifica ambos marcadores.
  - Estado: `CERRADO` en A02; cada deleter queda contenido y el cleanup LIFO continúa.
- **G08 — dumper desreferencia tipo nulo: `TA-AST-215`.**
  - Productor: `AstDumper::dumpType`.
  - Causa: no valida `typeSyntax` antes de leerlo; los argumentos genéricos se recorren sin filtro.
  - Consumidores: dumps de AST, diagnósticos y herramientas de inspección.
  - Puntual: proceso hijo de `AstDestructiveModule.cpp`; existe.
  - Estado: `CERRADO` en A02; el dumper publica `<missing>` y retorna antes de desreferenciar.
- **G09 — provenance valida clase, no valor: `TP-PAR-063`.**
  - Productor: `Parser::parseType`.
  - Causa: para un modo desconocido llama `expect(Identifier)`, que acepta cualquier identificador.
  - Consumidores: todas las declaraciones y expresiones que usan `TypeSyntax` de puntero.
  - Puntual: `ParserTypeModifierContinuationModule.cpp`; existe.
  - Estado: `CERRADO` en A03; el parser exige el contextual `tracked`, diagnostica cualquier otro valor y recupera hasta `)`.
- **G10 — `void` aceptado como objeto allocation: `TP-PAR-069`, `TP-PAR-476`.**
  - Productor: `Parser::parseAllocationElementType` y el camino `new` que lo consume.
  - Causa: acepta todo builtin y no diagnostica `KwVoid` como `InvalidType`.
  - Consumidores: `new`, arrays de allocation y validación semántica posterior.
  - Puntuales: TypeModifier y UnaryExpression; existen y comparten la misma frontera causal.
  - Estado: `CERRADO` en A03; `KwVoid` conserva el nodo para recuperación, pero emite `InvalidType` en la frontera compartida.
- **G11 — lookahead genérico sobreacepta relacionales: `TP-PAR-103`.**
  - Productor: `Parser::looksLikeGenericCallTypeArguments`.
  - Causa: cualquier `<...>` cerrado seguido de `(` puede reclasificarse como llamada genérica.
  - Consumidor: `parsePostfixExpression`, antes de la precedencia relacional.
  - Puntual: `ParserGenericLookaheadContinuationModule.cpp`; existe.
  - Estado: `CERRADO` en A03; cada argumento de nivel superior debe comenzar como tipo conocido o valor const permitido.
- **G12 — declaración modela un solo declarator: `TP-PAR-306`.**
  - Productor: `Parser::parseVariableDeclaration`.
  - Causa: construye un nodo y espera `;`; no existe bucle de declarators separados por coma.
  - Consumidores: bloques, MethodBodyModel y lowering de locales.
  - Puntual: existe, pero no inspecciona el segundo nodo; debe reforzarse antes de cerrar.
  - Estado: `CERRADO` en A03; `declaration-list` preserva ambos nodos sin introducir scope y ambos alcanzan Sema/HIR.
- **G13 — deconstruction pierde modos: `TP-PAR-309`.**
  - Productor: `Parser::parseDeconstructionPattern`.
  - Causa: sólo acepta identificador o paréntesis; `ref` y `move` se consumen como error sin metadata.
  - Consumidores: declaración deconstruction, bindings y ownership semántico.
  - Puntual: `ParserStatementContinuationModule.cpp`; existe.
  - Estado: `CERRADO` en A03; los tres bindings preservan `ref`, `move` y `value` en orden.
- **G14 — encabezado inline asm no implementado: `TP-PAR-316`.**
  - Productor: `Parser::parseInlineAssemblyStatement`.
  - Causa: después de `asm` exige `{`; no parsea `target` ni `dialect` ni publica metadata canónica.
  - Consumidores: validación y lowering de inline assembly.
  - Puntual: `ParserInlineAssemblyContinuationModule.cpp`; existe.
  - Estado: `CERRADO` en A03; el lexer conserva el modo raw tras el header y el parser publica metadata canónica.
- **G15 — placeholders asm sólo numéricos: `TP-PAR-327`.**
  - Productor: `Parser::validateInlineAssemblyPlaceholders`.
  - Causa: interpreta todo placeholder no `out` como ordinal; no resuelve nombres de operandos `in`.
  - Consumidores: diagnóstico y lowering de operandos asm.
  - Puntual: `ParserInlineAssemblyContinuationModule.cpp`; existe.
  - Estado: `CERRADO` en A03; nombres se resuelven sólo contra inputs declarados y los ausentes se diagnostican.
- **G16 — condition declaration sólo se reporta diferida: `TP-PAR-334`.**
  - Productores: `looksLikeConditionDeclaration` y `parseIfStatement`.
  - Causa: el lookahead reconoce la forma, pero `parseIfStatement` sólo reporta deferred y parsea expresión.
  - Consumidores: scope del local condicional y flujo `if`.
  - Puntual: `ParserControlFlowContinuationModule.cpp`; existe.
  - Estado: `CERRADO` en A03; el local y el `IfStatement` comparten un bloque de scope en orden fuente.
- **G17 — tuple pattern vacío fuerza primer hijo: `TP-PAR-355`.**
  - Productor: `Parser::parsePatternPrimary`.
  - Causa: tras `(` llama siempre a `parsePattern`; no reconoce `()` antes de construir el primer hijo.
  - Consumidores: `match`, conditional patterns y deconstruction.
  - Puntual: `ParserPatternSemanticsModule.cpp`; existe.
  - Estado: `CERRADO` en A03; `()` produce una tupla sin hijos y la jerarquía nested se conserva.
- **G18 — struct pattern exige colon: `TP-PAR-356`.**
  - Productor: lambda `parseStructBody` dentro de `parsePatternPrimary`.
  - Causa: ejecuta siempre `expect(Colon)`; no convierte shorthand en binding implícito.
  - Consumidores: análisis y lowering de patrones estructurales.
  - Puntual: `ParserPatternSemanticsModule.cpp`; existe.
  - Estado: `CERRADO` en A03; shorthand sintetiza un binding y conserva alineación nombre/child.
- **G19 — range pattern ignora `..=`: `TP-PAR-358`.**
  - Productor: rama literal de `Parser::parsePatternPrimary`.
  - Causa: sólo consume `DotDot`; `DotDotEqual` queda sin consumir y no llega al endpoint superior.
  - Consumidores: pattern conditions y exhaustividad.
  - Puntual: `ParserPatternSemanticsModule.cpp`; existe.
  - Estado: `CERRADO` en A03; el delimitador queda en la representación y selecciona `<` o `<=` al bajar.
- **G20 — foreach exige nombre simple: `TP-PAR-400`.**
  - Productor: `Parser::parseForeachStatement`.
  - Causa: después del tipo exige un `Identifier`; no delega a un patrón de deconstruction.
  - Consumidores: candidatos array/span/enumerator y bindings del cuerpo.
  - Puntual: `ParserForeachSemanticsModule.cpp`; existe.
  - Estado: `CERRADO` en A03; el patrón se parsea una vez y sus bindings se emiten en cada candidato.
- **G21 — temporales foreach tienen identidad fija: `TP-PAR-405`, `TP-PAR-406`.**
  - Productor: `Parser::parseForeachStatement`.
  - Causa: reutiliza nombres `$__foreach_*` constantes para instancias secuenciales y anidadas.
  - Consumidores: name binding, HIR/MIR y depuración de loops.
  - Puntuales: `ParserForeachContinuationModule.cpp`; existen.
  - Estado: `CERRADO` en A03; cada instancia reserva un prefijo sintético antes de parsear su cuerpo.
- **G22 — `try` no exige handler: `TP-PAR-422`.**
  - Productor: `Parser::parseTryStatement`.
  - Causa: retorna tras cero `catch` y ausencia de `finally` sin emitir diagnóstico.
  - Consumidores: cleanup, control de flujo y lowering de excepciones.
  - Puntual: `ParserCleanupContinuationModule.cpp`; existe.
  - Estado: `CERRADO` en A03; emite `ExpectedToken` sin consumir el siguiente token.
- **G23 — range expression no admite inicio omitido: `TP-PAR-431`.**
  - Productor: `Parser::parseRangeExpression`.
  - Causa: llama primero a `parseLogicalOrExpression`; no detecta `DotDot` como inicio sin operando.
  - Consumidores: slicing, indexación y foreach de rangos.
  - Puntual: `ParserExpressionPrecedenceContinuationModule.cpp`; existe.
  - Estado: `CERRADO` en A03; el delimitador puede abrir el range y EOF termina un endpoint final omitido.
- **G24 — lambda no parsea retorno explícito: `TP-PAR-450`.**
  - Productor: `Parser::parseLambdaExpression`.
  - Causa: tras parámetros exige `FatArrow`; no consume `-> TypeSyntax` ni lo adjunta al nodo.
  - Consumidores: binding/inferencia de lambda y lowering callable.
  - Puntual: `ParserLambdaContinuationModule.cpp`; existe.
  - Estado: `CERRADO` en A03; lookahead y parser conservan `-> TypeSyntax` antes de `=>`.
- **G25 — recursión de expresiones sin presupuesto: `TP-PAR-511`.**
  - Productor: cadena recursiva de Parser para paréntesis e interpolaciones anidadas.
  - Causa: no hay depth budget ni conversión iterativa antes de agotar stack a profundidad 128.
  - Consumidores: parseo de interpolated holes y expresiones agrupadas.
  - Puntual: existe, pero sólo prueba profundidad 32 y falla artificialmente; falta reproducción fiel aislada.
  - Estado: `CERRADO` en A03; oráculo real aislado a 128 y colapso iterativo de agrupaciones puras.
- **G26 — clone de TypeSyntax incompleto: `TP-PAR-514`.**
  - Productor: `parser::cloneParsedType`.
  - Causa: omite `constExpression` y origen; copia `TypeModifierSyntax::arrayCount` por alias.
  - Consumidores: clones de nodos, constraints genéricos y receivers sintetizados.
  - Puntual: `ParserConstructionCloneFinalModule.cpp` vía `ParserProductTestShim.cpp`; existe.
  - Estado: `CERRADO` en A03; payloads de expresión, modifiers y origen se clonan sin alias.
- **G27 — increment/decrement ignoran aridad inválida: `TP-PAR-524`.**
  - Productores: `operatorSpelling`, `operatorMethodName` y tabla `kOperatorTraits`.
  - Causa: los atajos de `PlusPlus`/`MinusMinus` devolvían nombres válidos para cualquier aridad en vez de limitarse a uno.
  - Consumidores: declaraciones de operador y metadata de métodos.
  - Puntual: existe, reprodujo dos fallos de la matriz inválida y quedó Required después de corregir el productor.
  - Estado: `CERRADO` en A03; aridades distintas de uno retornan `op_Invalid`.
- **G28 — categoría Function no se rechaza: `TS-SEMA-106`, `TS-SEMA-246`.**
  - Productores: análisis de fields y `SemanticAnalyzer::validateObjectType`.
  - Causa: `validateObjectType` omitía `TypeKind::Function`; el análisis de fields tampoco rechazaba `TypeSyntax::isFunction()` y el fixture combinado de `TS-SEMA-106` quedaba detenido por el diagnóstico Parser del field `void` antes de alcanzar Sema.
  - Consumidores: layout de structs y análisis de `new`/objetos.
  - Puntuales: `SemaStructLayoutModule.cpp` y `SemaAllocationType245To266Module.cpp`; ambas pasaron como KPD y Required.
  - Estado: `CERRADO` en A04; las fronteras `void` y function se ejercitan por separado sin relajar el conteo exacto de `InvalidType`.
- **G29 — tipos nominales posteriores no están predeclarados: `TS-SEMA-110`.**
  - Productor: fases global y de fields de struct en `SemanticAnalyzer`.
  - Causa: el análisis global era de una sola pasada; `First` resolvía `Second` como `UnknownTypeName` antes de que existiera su símbolo y después quedaba completo, ocultando el ciclo. No requería DFS: requería predeclarar nominales antes de resolver fields.
  - Consumidores: layout físico, símbolos de fields y diagnósticos de incompletitud.
  - Puntual: `SemaStructLayoutModule.cpp`; pasó como KPD y Required con exactamente un `IncompleteType` por escenario.
  - Estado: `CERRADO` en A04; los símbolos predeclarados se publican después en orden fuente, verificado por `TS-SEMA-092`.
- **G30 — C ABI recurre sin visited-set: `TS-SEMA-265`.**
  - Productores: `isCAbiCompatibleStruct` e `isCAbiCompatibleStructField`.
  - Causa: un field struct llama de nuevo al mismo análisis sin conjunto active/visited.
  - Consumidores: validación de firmas `extern C`.
  - Puntual: `SemaAllocationType245To266Module.cpp`; reprodujo `0xC00000FD` y pasó como KPD corregido y Required.
  - Estado: `CERRADO` en A04; ciclos activos se rechazan y subgrafos ya validados se reutilizan antes de calcular layout.
- **G31 — SymbolDumper omite constante: `TS-SEMA-317`.**
  - Productor: `SymbolDumper::dumpSymbol`.
  - Causa: emite flags, enum y offset, pero nunca consulta `constantValue`.
  - Consumidores: dumps semánticos y evidencia diagnóstica.
  - Puntual: `SemaFallthroughDump299To321Module.cpp`; pasó como KPD y Required con salida completa exacta.
  - Estado: `CERRADO` en A04; reutiliza el formateador canónico de `SemanticDumper` y omite el campo si no existe.
- **G32 — CFG filtra función nula: `TF-CFG-076`.**
  - Productor: `CfgBuilder::build`.
  - Causa: el `if (function != nullptr && ...)` omite la entrada nula en vez de fallar.
  - Consumidores: módulo CFG, Dominance, SSA y optimizaciones posteriores.
  - Puntual: `CfgBuilderModule.cpp`; pasó como KPD y Required con diagnóstico exacto y resultado nulo.
  - Estado: `CERRADO` en A05; todas las entradas MIR se prevalidan antes de crear o publicar el módulo CFG.
- **G33 — frontier omite self-loop de un predecesor: `TF-DOM-142`.**
  - Productor: `DominanceBuilder::buildFrontier`.
  - Causa: descarta todo bloque con menos de dos predecesores; un self-edge único sí crea frontera propia.
  - Consumidores: SSA/phi placement y verificadores de dominancia.
  - Puntual: `DominanceTraversalDump129To192Module.cpp`; pasó como KPD y Required con frontier singleton exacta.
  - Estado: `CERRADO` en A06; sólo se omiten nodos sin predecesores y los lineales siguen vacíos por su idom.
- **G34 — rename no reserva aliases builtin: `TF-LANGUAGE-SERVICE-304`.**
  - Productor: `validRenameIdentifier` usado por `AnalysisSnapshot::rename`.
  - Causa: LanguageService duplicaba parcialmente la tabla del lexer y omitía `i8/u8`, `i16/u16`, `i32/u32`,
    `i64/u64`, `isize/usize` e `i128/u128`.
  - Consumidores: rename atómico y ediciones de LanguageService.
  - Puntual: contrato 38, variante 8 de `LanguageServiceQueries233To312Module.cpp`; pasó como KPD y Required.
  - Estado: `CERRADO` en A07; lexer y rename consultan una sola autoridad y el oráculo exige rechazo atómico de
    los doce aliases sin publicar ediciones parciales.
- **G35 — JSON no escapa tab: `TSH-00213`.**
  - Productor: `DiagnosticRenderer.JsonEscape` self-host.
  - Causa: escapa slash, quote, CR y LF, pero no tab ni el resto de controles JSON.
  - Consumidores: Driver JSON, controller y cualquier parser de diagnósticos.
  - Puntual: `SelfHostDiagnosticsRenderingModule.ns`; pasó como KPD y Required, incluyendo los cinco escapes
    nombrados y la ruta genérica `\\u00xx`.
  - Estado: `CERRADO` en A08; el renderer conserva Unicode y escapa en una pasada todos los controles JSON.
- **G36 — severidad JSON hardcoded: `TSH-00214`.**
  - Productor: `DiagnosticRenderer.Json` self-host.
  - Causa: serializa siempre `"severity":"error"` e ignora `Diagnostic.Severity()`.
  - Consumidores: Driver JSON, reportes y controller.
  - Puntual: `SelfHostDiagnosticsRenderingModule.ns`; existe.
- **G37 — error de lectura colapsa a vacío: `TSH-00903`.**
  - Productores: `File.ReadAllText` y `SourceBuffer.Load` self-host.
  - Causa: fallo devuelve `""` y Load construye buffer, indistinguible de archivo vacío válido.
  - Consumidores: Lexer, Parser, Bootstrap y Driver self-host.
  - Puntual: `SelfHostSourceLoadingModule.ns`; existe.
- **G38 — array count ignora TryParse: `TSH-01011`.**
  - Productor: `TypeTable.Intern` self-host.
  - Causa: ignora el bool de `Number.TryParseInt64`; count inválido queda cero y publica Array.
  - Consumidores: Sema/CodeGen self-host y metadata de tipos.
  - Puntual: `SelfHostTypesInterningModule.ns`; existe.
- **G39 — predicados indexan antes de validar ID: `TSH-01023`.**
  - Productores: `TypeTable.IsInteger` e `IsNumeric` self-host.
  - Causa: llaman `Get(id)` sin comprobar `0 <= id < Count()`.
  - Consumidores: conversiones numéricas de Sema y CodeGen self-host.
  - Puntual: `SelfHostTypesFunctionModule.ns`; existe.
- **G40 — helper runtime publica desde path inválido: `T12B-00021`, `T12B-00022`.**
  - Productor: `cmake/NativeSharpSelfHostRuntime.cmake::nsc_resolve_selfhost_runtime`.
  - Causa: path NSLink vacío o malformado se usa para derivar library/ABI sin precondición estructural.
  - Consumidores: cuatro verificadores CMake de runtime/self-host/fixed-point.
  - Puntuales: `run_buildsystem_gate.py` + `BuildSystemRuntimeProbe.cmake`; existen.
- **G41 — deploy no expone contrato de inputs: `T12B-00457`, `T12B-00458`, `T12B-00460`, `T12B-00461`.**
  - Productor: `deploy.bat`.
  - Causa: usa build/prefix hardcoded; no puede preservar paths ni validar destino/root/solape.
  - Consumidores: instalación manual y superficie Packaging.
  - Puntuales: `run_packaging_gate.py` con `cmake.cmd` espía y sandbox; existen.

### A.5.3 Grafo productor-consumidor y dependencias

```text
Language/layout/traits ---------> Frontend/ABI ------------------------------+
Lexer -> AST arena -> Parser -> Sema -> CFG -> Dominance -> SSA/optimización |
                    \-> LanguageService                                      |
SelfHost.Source -> SelfHost.Lexer/Parser -> SelfHost.Sema/Types -> CodeGen    |
Diagnostics -----------------------------------------------------> Driver     |
CMake self-host runtime ----------------------------------------> verificadores|
deploy.bat ------------------------------------------------------> instalación |
                                                                            v
                                                              pruebas puntuales
```

Dependencias de ejecución declaradas:

- `nsc_component_tests`: A00, A01 y A03-A07, Debug y Release.
- `nsc_component_tests_destructive`: A02; requiere proceso hijo donde lo declara el caso.
- Controller + NSC: A08-A13; self-host añade Basic/Source/Diagnostics/Collections según el manifiesto.
- A12 requiere CMake, LLVM 23 y NSLink; A13 requiere CMake y filesystem aislado.
- Ningún KPD downstream se considera cerrado porque un productor upstream cambie: cada ID conserva su puntual.

### A.5.4 Orden vinculante de tandas y puertas internas

El orden de trabajo permanece A00 -> A13. Dentro de cada tanda se respeta la dependencia causal siguiente:

1. **A00:** G01 -> G04; G02 sólo tras definir un rechazo observable sin acceso fuera de rango.
2. **A01:** G05 host; después se conserva el mismo contrato para el productor self-host de A09.
3. **A02:** G06 -> G07 -> G08, porque ownership/teardown precede inspección destructiva.
4. **A03:** G09 -> G11, G12 -> G20, G21 -> G25, G26 y finalmente reconciliación G27.
5. **A04:** G28 -> G29 -> G30 -> G31.
6. **A05-A07:** G32 -> G33; G34 es independiente después de Parser/Sema estables.
7. **A08-A11:** G35 -> G36; G37 -> G05 self-host; G38 -> G39.
8. **A12-A13:** G40 antes de sus consumidores CMake; G41 en sandbox, sin publicar instalación real.

Puertas previas a editar producto:

- `TP-PAR-306`: ampliar el oráculo para inspeccionar los dos declarators, no sólo consumo hasta EOF.
- `TP-PAR-511`: reemplazar el fallo artificial por proceso aislado y frontera 128 reproducible.
- `TP-PAR-524`: su ejecución previa al parche demostró la divergencia de aridad; se corrigió antes de retirar KPD.
- Los 13 casos todavía abiertos pueden entrar a implementación causal después del `SCOPE LOCK` de su tanda.

### A.5.5 Estado de la fase previa

- Estado de Parte A: `EN_PROGRESO`; A08 G35 cerrado 1/2, 41/52 defectos corregidos y 11 KPD abiertos.
- Inventario causal: 52 demostrados; no quedan casos por reconciliar en el inventario previo.
- Productores/consumidores: 52 trazados en 41 familias.
- Pruebas puntuales: A00 7/7, A01 1/1, A02 3/3, A03 21/21, A04 5/5, A05 1/1 y A06 1/1 Required `PASS`.
- Evidencia A00: productor Release LLVM 23.1.0 reconstruido sin regenerar CMake; 14 objetos y enlace de 166 objetos.
- Consolidado A00 Required: siete invocaciones `nsc_component_tests.exe --case <ID>`, 7/7 `PASS`, 684 ms.
- Evidencia A01: `Lexer.cpp` y su módulo de registro reconstruidos con LLVM 23.1.0 sin regenerar CMake;
  enlace equivalente de 166 objetos; `TL-LEX-075` puntual y Required `PASS`.
- Evidencia G06: instanciación de `AstArena::make` y registro destructivo reconstruidos con LLVM 23.1.0;
  `TA-AST-014` puntual como KPD y Required `PASS`.
- Evidencia G07: `AstArena.cpp` y registro destructivo reconstruidos con LLVM 23.1.0;
  `TA-AST-016` puntual como KPD y Required `PASS`.
- Evidencia G08: `AstDumper.cpp` y registro destructivo reconstruidos con LLVM 23.1.0;
  `TA-AST-215` puntual como KPD y Required `PASS`.
- Consolidado A02 Required: `TA-AST-014`, `TA-AST-016` y `TA-AST-215`, 3/3 `PASS`, 154 ms de pared.
- Evidencia G09: `ParserProductTestShim.cpp` y el registro TypeModifier reconstruidos con LLVM 23.1.0;
  `TP-PAR-063` puntual como KPD y Required `PASS`.
- Evidencia G10: `ParserProductTestShim.cpp` y los registros TypeModifier/UnaryExpression reconstruidos con LLVM 23.1.0;
  `TP-PAR-069` y `TP-PAR-476` puntuales como KPD y Required `PASS`.
- Evidencia G11: `ParserProductTestShim.cpp` y el registro GenericLookahead reconstruidos con LLVM 23.1.0;
  `TP-PAR-103` puntual como KPD y Required `PASS`; subbloque A03 G09-G11 cerrado 4/4.
- Evidencia G12: el oráculo fortalecido falló contra el producto previo y después verificó topología, initializers,
  orden y dos `LocalVariableDeclaration` a través de Parser-Sema-HIR; seis objetos reconstruidos con LLVM 23.1.0.
- Evidencia G13: `TP-PAR-309` pasó como KPD (13,667 us) y como Required (16,095 us) tras preservar `ref`, `move` y `value`
  en los bindings de deconstruction; `ParserProductTestShim` y el módulo puntual fueron reconstruidos con LLVM 23.1.0.
- Evidencia G14: `TP-PAR-316` pasó como KPD (4,634 us) y como Required (4,007 us) tras conservar el modo raw del lexer,
  canonicalizar target/dialect y verificar metadata, body, línea y EOF; tres objetos reconstruidos con LLVM 23.1.0.
- Evidencia G15: `TP-PAR-327` pasó como KPD (20,290 us) y como Required (18,977 us) tras resolver placeholders nominales
  exclusivamente contra inputs declarados y diagnosticar un nombre ausente; dos objetos reconstruidos con LLVM 23.1.0.
- Evidencia G16: `TP-PAR-334` pasó como KPD (16,104 us) y como Required (4,568 us) tras bajar la declaración condicional a
  un bloque con el local y el `IfStatement` en orden; dos objetos reconstruidos con LLVM 23.1.0.
- Evidencia G17: `TP-PAR-355` pasó como KPD (2,683 us) y como Required (3,481 us) tras reconocer `()` antes de intentar
  construir el primer hijo; dos objetos reconstruidos con LLVM 23.1.0.
- Evidencia G18: `TP-PAR-356` pasó como KPD (9,633 us) y como Required (3,560 us) tras sintetizar el binding shorthand y
  conservar `fieldNames[i]` alineado con `children[i]`; dos objetos reconstruidos con LLVM 23.1.0.
- Evidencia G19: tras retirar un cambio de layout innecesario que invalidaba 67 consumidores, `TP-PAR-358` pasó como
  KPD (6,572 us) y Required (3,288 us) conservando `..`/`..=` en `Pattern::name` y bajando `<`/`<=`; dos objetos LLVM 23.1.0.
- Evidencia G20: `TP-PAR-400` pasó como KPD (9,517 us) y como Required (6,422 us) tras parsear una vez el patrón y emitir
  ambos bindings en los candidatos array/span/enumerator; dos objetos reconstruidos con LLVM 23.1.0.
- Evidencia G21: `TP-PAR-405` (KPD 15,387 us; Required 18,201 us) y `TP-PAR-406` (KPD 841 us; Required 928 us) pasaron
  tras reservar un prefijo único por instancia antes de parsear el cuerpo; cuatro objetos reconstruidos con LLVM 23.1.0
  sin regenerar configuración.
- Evidencia G22: `TP-PAR-422` pasó como KPD (18,580 us) y Required (16,204 us) al exigir `catch` o `finally`, emitir
  exactamente un `ExpectedToken` y conservar EOF; dos objetos reconstruidos con LLVM 23.1.0 sin regenerar configuración.
- Evidencia G23: `TP-PAR-431` pasó como KPD (13,032 us) y Required (3,932 us) tras admitir un delimitador inicial y
  tratar EOF como final omitido; el oráculo verifica por separado forma exclusiva, inclusiva y ambos endpoints omitidos.
- Evidencia G24: `TP-PAR-450` pasó como KPD (3,943 us) y Required (4,044 us) tras reconocer `-> TypeSyntax =>` en el
  lookahead y adjuntar el tipo explícito al `LambdaExpression`; dos objetos LLVM 23.1.0 sin regenerar configuración.
- Evidencia G25: el nuevo oráculo aislado reprodujo primero `0xC00000FD` (3221225725) a profundidad 128; después
  `TP-PAR-511` pasó como KPD (223,235 us) y Required (220,726 us), preservando además tuplas y precedencia mediante
  colapso iterativo de agrupaciones puras; dos objetos LLVM 23.1.0 sin regenerar configuración.
- Evidencia G26: `TP-PAR-514` pasó como KPD (2,094 us) y Required (2,394 us) tras clonar profundamente
  `constExpression`, `arrayCount`, tipos de función y metadatos de origen en el arena destino; dos objetos LLVM 23.1.0
  sin regenerar configuración.
- Evidencia G27: la ejecución inicial de `TP-PAR-524` reprodujo dos fallos para `++` con aridad 2 y `--` con aridad 0;
  después de limitar ambos atajos a aridad 1, pasó como KPD (1,605 us) y Required (1,100 us); dos objetos LLVM 23.1.0
  sin regenerar configuración.
- Consolidado A03 Required: 21 invocaciones `nsc_component_tests.exe --case <ID>`, 21/21 `PASS`, 4,542 ms de pared.
- Evidencia G28: `TS-SEMA-106` pasó como KPD (6,910 us) y Required (18,967 us); `TS-SEMA-246` pasó como KPD
  (4,196 us) y Required (3,209 us). Se reconstruyeron sólo el shim de Sema, los dos módulos propietarios y el enlace
  exacto derivado de `build.ninja`, con LLVM 23.1.0 y sin regenerar configuración.
- Evidencia G29: `TS-SEMA-110` pasó como KPD (23,769 us) y Required (13,225 us) tras predeclarar structs sin alterar
  el orden observable; `TS-SEMA-092` confirmó ese orden (585 us). Se reconstruyeron sólo el shim de Sema, el módulo
  propietario y el enlace exacto derivado de `build.ninja`, con LLVM 23.1.0 y sin regenerar configuración.
- Evidencia G30: `TS-SEMA-265` reprodujo primero el terminal Windows `0xC00000FD`; con el guard causal pasó como KPD
  corregido (2,817 us) y Required (11,643 us). Se reconstruyeron sólo el shim de Sema, el módulo propietario y el
  enlace exacto derivado de `build.ninja`, con LLVM 23.1.0 y sin regenerar configuración.
- Evidencia G31: `TS-SEMA-317` pasó como KPD (3,552 us) y Required (7,569 us) al compartir el formateador canónico
  de constantes; `TS-SEMA-310` confirmó el formato previo (5,357 us). Se reconstruyeron sólo ambos dumpers, el módulo
  propietario y el enlace exacto derivado de `build.ninja`, con LLVM 23.1.0 y sin regenerar configuración.
- Consolidado A04 Required: cinco invocaciones `nsc_component_tests.exe --case <ID>`, 5/5 `PASS`, 1,098 ms de pared.
- Evidencia G32: `TF-CFG-076` falló como KPD previo (67,095 us), pasó como KPD corregido (12,876 us) y Required
  (3,260 us) tras prevalidar funciones antes de cualquier asignación CFG; se reconstruyeron sólo `CfgBuilder`, el
  módulo propietario y el enlace exacto derivado de `build.ninja`, con LLVM 23.1.0 y sin regenerar configuración.
- Consolidado A05 Required: una invocación `nsc_component_tests.exe --case TF-CFG-076`, 1/1 `PASS`, 244 ms de pared.
- Evidencia G33: `TF-DOM-142` falló como KPD previo (62,393 us), pasó como KPD corregido (14,261 us) y Required
  (4,780 us) al incluir un predecesor self-edge; `TF-DOM-086` preservó la frontier de loop (535 us). Se reconstruyeron
  sólo el shim de `DominanceBuilder`, el módulo propietario y el enlace exacto, sin regenerar configuración.
- Consolidado A06 Required: una invocación `nsc_component_tests.exe --case TF-DOM-142`, 1/1 `PASS`, 192 ms de pared.
- Evidencia G34: `TF-LANGUAGE-SERVICE-304` pasó como KPD corregido (33,118 us) y Required (192,101 us); el guard
  `TF-LANGUAGE-SERVICE-233` pasó (78,172 us). La reconstrucción cerró 176 dependencias pendientes del mismo target
  para eliminar una mezcla ABI previa, sin regenerar configuración ni ejecutar batería.
- Consolidado A07 Required: una invocación `nsc_component_tests.exe --case TF-LANGUAGE-SERVICE-304`, 1/1 `PASS`,
  228 ms de pared (27,896 us de caso).
- Evidencia G35: `TSH-00213` pasó como KPD corregido (42.834 ms) y Required (18.333 ms); el fixture verifica
  `\\b`, `\\f`, `\\t`, `\\r`, `\\n` y la ruta genérica `\\u000b` antes del oráculo JSON del controller.
- Próxima acción de Parte A: A08 G36 (`TSH-00214`).

---

# Parte B — NativeSharp 3.0.4: cierre lingüístico

## B.1 Autoridad, estado y dependencias

El inventario siguiente reproduce los 147 IDs únicos del plan fuente. Todos comienzan `PENDIENTE`; la existencia
de diseño, helper, metadata o prueba interna no cambia el estado. La prueba puntual de un ítem debe cubrir la
vertical que corresponda y nunca sustituye la certificación de la tanda o de 3.0.4.

```text
T00
 ├─ T01
 ├─ T02
 ├─ T03
 ├─ T04 -> T05 -> T07
 │          └-> T06
 ├─ T08 -> T09 -> T12
 ├─ T10 -> T11
 ├─ T13
 ├─ T14
 └─ T15
                 T16 -> T17
```

| Tanda | Tema | Ítems | Cerrados | Progreso |
|---|---|---:|---:|---:|
| T00 | Auditoría ejecutable y congelamiento de contratos | 8 | 0 | 0 % |
| T01 | Inicialización estática y declaraciones en condiciones | 8 | 0 | 0 % |
| T02 | Cuerpos default de traits e interfaces | 6 | 0 | 0 % |
| T03 | List, slice y property patterns | 8 | 0 | 0 % |
| T04 | Lambdas, callables y funciones locales | 8 | 0 | 0 % |
| T05 | Async/await productivo | 10 | 0 | 0 % |
| T06 | Generadores síncronos | 8 | 0 | 0 % |
| T07 | Async streams y concurrencia estructurada | 8 | 0 | 0 % |
| T08 | Extension members y operadores | 7 | 0 | 0 % |
| T09 | Construcción, propiedades y eventos | 10 | 0 | 0 % |
| T10 | Strings, null-awareness y nombres estáticos | 8 | 0 | 0 % |
| T11 | Spans, referencias y genéricos modernos | 8 | 0 | 0 % |
| T12 | Partial members y composición de fuente | 6 | 0 | 0 % |
| T13 | Allocators, scope e in-place construction | 9 | 0 | 0 % |
| T14 | Comptime, derive y generación higiénica | 8 | 0 | 0 % |
| T15 | Tipo decimal nativo | 8 | 0 | 0 % |
| T16 | Integración ABI, metadata, debug y self-host | 8 | 0 | 0 % |
| T17 | Conformidad C# 14/Beef y cierre | 11 | 0 | 0 % |
| **Total** |  | **147** | **0** | **0 %** |

## B.2 Inventario exacto T00-T17

### T00 — auditoría ejecutable y congelamiento de contratos

- [ ] `NS304-T00-F01` — Inventariar cada feature declarada `TERMINADA` y localizar su vertical real.
- [ ] `NS304-T00-F02` — Clasificar evidencia por parser, semántica, MIR, LLVM, NSMETA, runtime y ejecución.
- [ ] `NS304-T00-F03` — Construir matriz C# 8-14: equivalente, alternativa, divergencia o gap obligatorio.
- [ ] `NS304-T00-F04` — Construir matriz Beef: memoria, extensions, generics, comptime, interop y patterns.
- [ ] `NS304-T00-F05` — Añadir disposición `PlannedFor304` sin presentar features pendientes como productivas.
- [ ] `NS304-T00-F06` — Congelar versiones previstas de AST, HIR, MIR, NSMETA, ABI runtime y debug metadata.
- [ ] `NS304-T00-F07` — Publicar manifest de dependencias compiler/runtime/STL por feature.
- [ ] `NS304-T00-F08` — Crear harness v3.0.4 sin conteos fijos, hashes de herramientas ni textos frágiles.

### T01 — inicialización estática y declaraciones en condiciones

- [ ] `NS304-T01-F01` — Definir estados `uninitialized`, `initializing`, `initialized` y `failed`.
- [ ] `NS304-T01-F02` — Implementar gramática y semántica de constructores estáticos por tipo.
- [ ] `NS304-T01-F03` — Implementar inicializadores de módulo con orden y dependencias explícitos.
- [ ] `NS304-T01-F04` — Implementar once thread-safe, espera, reentrancia y detección de ciclos.
- [ ] `NS304-T01-F05` — Hacer terminal el fallo de inicialización, sin retry; panic sigue el perfil del proceso.
- [ ] `NS304-T01-F06` — Implementar locales `static`, guard, lifetime y destrucción al descargar módulo.
- [ ] `NS304-T01-F07` — Implementar declaraciones en `if`, `while` y `switch` con scope y cleanup por arista.
- [ ] `NS304-T01-F08` — Versionar metadata, emitir debug info y probar EXE, DLL, concurrencia y recursión.

### T02 — cuerpos default de traits e interfaces

- [ ] `NS304-T02-F01` — Aceptar cuerpos default en métodos, propiedades e indexadores.
- [ ] `NS304-T02-F02` — Fijar prioridad: implementación explícita, miembro nominal, default más específico.
- [ ] `NS304-T02-F03` — Resolver diamantes, overrides, traits sellados, coherence y conflictos intermodulares.
- [ ] `NS304-T02-F04` — Integrar genéricos, associated types, constraints, `some Trait` y `dyn Trait`.
- [ ] `NS304-T02-F05` — Transportar cuerpos y witnesses versionados por NSMETA sin requerir fuentes.
- [ ] `NS304-T02-F06` — Cerrar vtables, ABI DLL, evolución binaria, MIR, LLVM y debug de thunks.

### T03 — list, slice y property patterns

- [ ] `NS304-T03-F01` — Implementar list patterns exactos, prefijo, sufijo y elemento descartado.
- [ ] `NS304-T03-F02` — Implementar rest/slice capture sin copia implícita y con lifetime del origen.
- [ ] `NS304-T03-F03` — Soportar arrays, inline arrays, slices, spans, strings UTF-8 y protocolo usuario.
- [ ] `NS304-T03-F04` — Completar property patterns anidados, posicionales, relacionales y lógicos.
- [ ] `NS304-T03-F05` — Extender usefulness/exhaustiveness para longitud, OR-pattern y dominios abiertos.
- [ ] `NS304-T03-F06` — Definir bindings `copy`, `move`, `ref` y `ref mut` dentro de cada pattern.
- [ ] `NS304-T03-F07` — Bajar a length checks, branches y borrows sin colección ni boxing temporal.
- [ ] `NS304-T03-F08` — Añadir diagnósticos de casos faltantes, redundancia, escape y move inválido.

### T04 — lambdas, callables y funciones locales

- [ ] `NS304-T04-F01` — Inferir tipo callable natural para lambda y method group cuando sea único.
- [ ] `NS304-T04-F02` — Permitir retorno explícito, atributos, calling convention y efectos en lambdas.
- [ ] `NS304-T04-F03` — Permitir `ref`, `ref readonly`, `in`, `out` y `scoped` sin repetir tipos inferibles.
- [ ] `NS304-T04-F04` — Implementar parámetros opcionales y defaults consteval en lambdas.
- [ ] `NS304-T04-F05` — Completar lambdas y funciones locales estáticas y prohibición de captura.
- [ ] `NS304-T04-F06` — Implementar funciones locales genéricas, recursión y autoreferencia segura.
- [ ] `NS304-T04-F07` — Congelar ranking de method groups, overload priority y conversiones a `fn*`/`fn&`.
- [ ] `NS304-T04-F08` — Cerrar layout, drop, escape, PDB y ejecución de closures combinadas con errors.

### T05 — async/await productivo

- [ ] `NS304-T05-F01` — Retirar rechazo `NS3049` para `async`/`await` y completar AST/recovery.
- [ ] `NS304-T05-F02` — Integrar `Future` lazy, `Poll`, `Waker`, resultado, error y cancelación.
- [ ] `NS304-T05-F03` — Implementar transformación productiva a state machine HIR/MIR.
- [ ] `NS304-T05-F04` — Calcular frame layout, discriminante, liveness y storage de locals suspendidos.
- [ ] `NS304-T05-F05` — Verificar borrows, moves, pinning, self-references y escapes entre suspensiones.
- [ ] `NS304-T05-F06` — Ejecutar drop/defer/finally una vez en completion, error o cancelación.
- [ ] `NS304-T05-F07` — Integrar `throws E`, `Result`, `?`, panic y unwind nativo con async.
- [ ] `NS304-T05-F08` — Permitir storage en stack, caller, arena o allocator explícito; nunca heap obligatorio.
- [ ] `NS304-T05-F09` — Emitir ABI/NSMETA de future opaco, poll/resume/drop y consumo intermodular.
- [ ] `NS304-T05-F10` — Emitir LLVM y debug metadata de state, locals, sequence points y logical frames.

### T06 — generadores síncronos

- [ ] `NS304-T06-F01` — Retirar rechazo `NS3049` para `yield return` y `yield break`.
- [ ] `NS304-T06-F02` — Implementar AST, semántica y restricciones de contexto de generator.
- [ ] `NS304-T06-F03` — Transformar cuerpo en frame y state machine MIR verificable.
- [ ] `NS304-T06-F04` — Calcular locals vivos, captures, ownership y drop flags por suspensión.
- [ ] `NS304-T06-F05` — Soportar items por value, move, ref y ref mut con lifetime asociado.
- [ ] `NS304-T06-F06` — Integrar pinning y construcción in-place para frames autoreferenciales.
- [ ] `NS304-T06-F07` — Implementar early stop, destrucción, errors, panic y cleanup determinista.
- [ ] `NS304-T06-F08` — Congelar iterator ABI/NSMETA, LLVM, PDB y consumo por `foreach`.

### T07 — async streams y concurrencia estructurada

- [ ] `NS304-T07-F01` — Definir `AsyncIterator`/`AsyncGenerator` y demanda explícita del consumidor.
- [ ] `NS304-T07-F02` — Implementar máquina única para `await` + `yield` sin frames duplicados.
- [ ] `NS304-T07-F03` — Definir backpressure, terminalidad, errores y orden observable.
- [ ] `NS304-T07-F04` — Implementar items borrowed/move-only y prohibir supervivencia fuera de demanda.
- [ ] `NS304-T07-F05` — Propagar cancelación y ejecutar cleanup de productor y consumidor.
- [ ] `NS304-T07-F06` — Cerrar `task scope`, spawn/join, child failure y no-escape de borrows.
- [ ] `NS304-T07-F07` — Verificar `Send`/`Sync`, cambio de thread y executor elegido por protocolo.
- [ ] `NS304-T07-F08` — Emitir metadata/debug de logical stacks, suspension points y async frames.

### T08 — extension members y operadores

- [ ] `NS304-T08-F01` — Diseñar bloques `extension` canónicos sobre tipos y constraints.
- [ ] `NS304-T08-F02` — Implementar extension properties con getter/setter/ref getter.
- [ ] `NS304-T08-F03` — Implementar métodos, propiedades y constantes estáticos de extensión.
- [ ] `NS304-T08-F04` — Implementar operadores de extensión mediante trait witnesses estáticos.
- [ ] `NS304-T08-F05` — Permitir conformance externa a traits bajo orphan/coherence rules.
- [ ] `NS304-T08-F06` — Congelar imports, visibilidad, precedencia, ambigüedad y lookup incremental.
- [ ] `NS304-T08-F07` — Versionar NSMETA y probar consumidor binario, genéricos y cero layout mutation.

### T09 — construcción, propiedades y eventos

- [ ] `NS304-T09-F01` — Implementar miembros `required` y definite initialization.
- [ ] `NS304-T09-F02` — Implementar setters `init` y congelamiento posterior a construcción.
- [ ] `NS304-T09-F03` — Implementar backing field sintetizado con layout visible.
- [ ] `NS304-T09-F04` — Extender primary constructors con orden de inicialización normativo.
- [ ] `NS304-T09-F05` — Completar collection expressions, spreads y builder protocol sin heap implícito.
- [ ] `NS304-T09-F06` — Implementar inline arrays/fixed buffers seguros con length const genérico.
- [ ] `NS304-T09-F07` — Implementar target-typed construction sin alterar overload selection.
- [ ] `NS304-T09-F08` — Definir eventos nativos sobre handler/storage explícitos.
- [ ] `NS304-T09-F09` — Integrar fallo parcial, drop order, inheritance, records, with y debug metadata.
- [ ] `NS304-T09-F10` — Implementar constructores fallibles `throws E`, propagación, cleanup parcial, ABI,
  NSMETA, MIR unwind, allocators, PDB y self-host.

### T10 — strings, null-awareness y nombres estáticos

- [ ] `NS304-T10-F01` — Implementar literales UTF-8 tipados y forma allocation-free.
- [ ] `NS304-T10-F02` — Completar raw interpolated strings y delimitadores anidados.
- [ ] `NS304-T10-F03` — Hacer productivos interpolation handlers con stack/writer/custom sink.
- [ ] `NS304-T10-F04` — Validar formato consteval e interpolación constante pura.
- [ ] `NS304-T10-F05` — Implementar `?.` y `?[]` a la izquierda con evaluación condicional del RHS.
- [ ] `NS304-T10-F06` — Integrar null-conditional compound assignment sin `++`/`--` ambiguos.
- [ ] `NS304-T10-F07` — Implementar `nameof` para símbolos, miembros, aliases y genéricos no ligados.
- [ ] `NS304-T10-F08` — Añadir `\e`, diagnósticos Unicode/UTF-8 y pruebas `noheap`/`no-culture`.

### T11 — spans, referencias y genéricos modernos

- [ ] `NS304-T11-F01` — Elevar slice/span mutable e inmutable a tipos de conversion ranking.
- [ ] `NS304-T11-F02` — Definir conversiones array/buffer/string/slice sin copia ni extensión de lifetime.
- [ ] `NS304-T11-F03` — Integrar spans en extension receiver, inferencia y conversiones compuestas.
- [ ] `NS304-T11-F04` — Implementar `params` sobre span, inline buffer y builder aprobado.
- [ ] `NS304-T11-F05` — Permitir tipos ref-like/stack-only como argumentos genéricos bajo constraints.
- [ ] `NS304-T11-F06` — Permitir tipos ref-like implementar traits sin `dyn` escapable.
- [ ] `NS304-T11-F07` — Completar ref fields, `scoped`, ref readonly y escape summaries intermodulares.
- [ ] `NS304-T11-F08` — Implementar atributos genéricos con argumentos consteval y metadata estable.

### T12 — partial members y composición de fuente

- [ ] `NS304-T12-F01` — Definir identidad y merge de tipos parciales independiente del orden de archivos.
- [ ] `NS304-T12-F02` — Implementar métodos parciales con firma, efectos, generics y body opcional/obligatorio.
- [ ] `NS304-T12-F03` — Implementar propiedades e indexadores parciales.
- [ ] `NS304-T12-F04` — Implementar constructores parciales y limitar initializer/base call a un body.
- [ ] `NS304-T12-F05` — Implementar eventos parciales con add/remove y storage declarado.
- [ ] `NS304-T12-F06` — Integrar macros/derive, colisiones, NSMETA y build incremental.

### T13 — allocators, scope e in-place construction

- [ ] `NS304-T13-F01` — Congelar protocolo allocate/resize/deallocate/alignment/failure.
- [ ] `NS304-T13-F02` — Implementar selección explícita de allocator en construcción.
- [ ] `NS304-T13-F03` — Implementar construcción `scope`/stack con escape y drop automático.
- [ ] `NS304-T13-F04` — Implementar arena/pool explícito y propagación a builders/containers.
- [ ] `NS304-T13-F05` — Completar placement/in-place con rollback ante fallo parcial.
- [ ] `NS304-T13-F06` — Implementar trailing/append storage con layout consteval y bounds.
- [ ] `NS304-T13-F07` — Verificar provenance allocator/deallocator entre módulos y DLL.
- [ ] `NS304-T13-F08` — Integrar allocation failure con `Result`, `throws`, panic y `noheap`.
- [ ] `NS304-T13-F09` — Emitir cost report que demuestre stack/arena/heap elegido.

### T14 — comptime, derive y generación higiénica

- [ ] `NS304-T14-F01` — Auditar introspección de toda declaración admitida en v3.0.4.
- [ ] `NS304-T14-F02` — Generar tipadamente miembros, impls, helpers y declaraciones auxiliares.
- [ ] `NS304-T14-F03` — Implementar mixin seguro como expansión AST higiénica y acotada.
- [ ] `NS304-T14-F04` — Garantizar source mapping, macro backtrace y navegación al origen.
- [ ] `NS304-T14-F05` — Integrar generics, associated items, partial y extension blocks generados.
- [ ] `NS304-T14-F06` — Congelar capabilities, entradas externas, cache key y reproducibilidad.
- [ ] `NS304-T14-F07` — Detectar ciclos, colisiones, crecimiento infinito y presupuestos.
- [ ] `NS304-T14-F08` — Probar serializer, formatter, binding y trait derive sin runtime reflection.

### T15 — tipo decimal nativo

- [ ] `NS304-T15-F01` — Congelar 128 bits: coeficiente 96 bits, escala 0-28, signo, sin NaN/infinito.
- [ ] `NS304-T15-F02` — Implementar lexer, literales, sufijos, parsing y round-trip exacto.
- [ ] `NS304-T15-F03` — Implementar identidad, layout, ABI, mangling, NSMETA y FFI permitido.
- [ ] `NS304-T15-F04` — Implementar aritmética, comparación y redondeo.
- [ ] `NS304-T15-F05` — Integrar overflow checked/wrapping/saturating donde esté definido.
- [ ] `NS304-T15-F06` — Implementar conversiones enteras/FP/decimal y ranking de overload.
- [ ] `NS304-T15-F07` — Unificar consteval/runtime y bajar a intrinsics o helpers versionados.
- [ ] `NS304-T15-F08` — Integrar formatting/parsing, traits numéricos, generics y bordes.

### T16 — integración ABI, metadata, debug y self-host

- [ ] `NS304-T16-F01` — Consolidar versiones definitivas de HIR, MIR, NSMETA y runtime ABI.
- [ ] `NS304-T16-F02` — Actualizar writer/reader y compatibilidad de metadata 3.0 previa.
- [ ] `NS304-T16-F03` — Validar producer/consumer sin source reference por feature pública.
- [ ] `NS304-T16-F04` — Validar EXE/DLL, exports, callbacks, unload y ownership cross-module.
- [ ] `NS304-T16-F05` — Completar CodeView/PDB de closures, patterns, async, generators y static init.
- [ ] `NS304-T16-F06` — Portar semántica a self-host y eliminar divergencias.
- [ ] `NS304-T16-F07` — Actualizar lints, fix-its y guía de migración a v3.0.4.
- [ ] `NS304-T16-F08` — Regenerar superficie sin `DeferredTo4` dentro del alcance.

### T17 — conformidad C# 14/Beef y cierre

- [ ] `NS304-T17-F01` — Ejecutar matriz C# 14 y enlazar cada capacidad con fixture NativeSharp.
- [ ] `NS304-T17-F02` — Ejecutar matriz Beef y enlazar fixture o divergencia deliberada.
- [ ] `NS304-T17-F03` — Probar async + ownership + errors + DLL + cancellation.
- [ ] `NS304-T17-F04` — Probar patterns + generics + move-only + borrowed slices.
- [ ] `NS304-T17-F05` — Probar extensions + partial + comptime + metadata separada.
- [ ] `NS304-T17-F06` — Probar allocators + generators + containers + noheap.
- [ ] `NS304-T17-F07` — Auditar costos ocultos en MIR/assembly.
- [ ] `NS304-T17-F08` — Ejecutar una vez toda la suite NSC O0 con NSLink, sin baseline.
- [ ] `NS304-T17-F09` — Corregir todos los fallos y repetir sólo fallidos y vecinos durante el barrido.
- [ ] `NS304-T17-F10` — Preparar candidato en `A:\native-sharp-3.0-baseline` sin instalar en C:.
- [ ] `NS304-T17-F11` — Tras baseline autorizado y certificado, crear milestone local `v.3.0.4`.

## B.3 Criterio de cierre

Cada ítem debe satisfacer, según aplique, especificación ES/EN, parser, AST, semántica, ownership, HIR/MIR, LLVM
Win64 O0, ABI, NSMETA intermodular, consteval, diagnósticos, PDB, self-host y pruebas positivas/negativas. La Parte B
no cierra con ningún `DeferredTo4`, `NS3049`, helper aislado, dependencia de fuentes del producer, costo oculto o
divergencia host/self-host. El cierre exige 147/147, suite O0 final y baseline autorizado.

---

# Parte C — NativeSharp 3.0.5: reingeniería del proceso de compilación

## C.1 Reasignación de versión y estado

Los IDs `NS305-*` se introducen en este maestro únicamente para dar identidad estable a los elementos granulares
del plan fuente; no pretenden demostrar implementación. La fuente los ubica bajo 3.0.4, pero la secuencia autorizada
los programa después del lenguaje, como 3.0.5. El desajuste de título permanece `DEUDA_DOCUMENTAL`.

| Tanda | Tema | Elementos | Cerrados | Estado |
|---|---|---:|---:|---|
| T01 | Telemetría profunda | 6 | 0 | `PENDIENTE` |
| T02 | Servicio, sesión y scheduler | 8 | 0 | `PENDIENTE` |
| T03 | Snapshots de proyectos y fuentes | 9 | 0 | `PENDIENTE` |
| T04 | Catálogo de parser e índices declarativos | 9 | 0 | `PENDIENTE` |
| T05 | NSMETA como índice binario | 9 | 0 | `PENDIENTE` |
| T06 | Índices de nombres, tipos y visibilidad | 7 | 0 | `PENDIENTE` |
| T07 | Genéricos incrementales | 8 | 0 | `PENDIENTE` |
| T08 | Análisis de cuerpos aislado | 11 | 0 | `EN_IMPLEMENTACION` histórica; cierre amplio pendiente |
| T09 | HIR/MIR por cuerpo | 12 | 0 | `PENDIENTE` |
| T10 | Caché Merkle y artifact store | 10 | 0 | `PENDIENTE` |
| T11 | Backend persistente | 10 | 0 | `PENDIENTE` |
| T12 | NSMETA/header incremental | 8 | 0 | `PENDIENTE` |
| T13 | Archive/link/publicación incremental | 10 | 0 | `PENDIENTE` |
| T14 | Separación física | 24 | 0 | `PENDIENTE` |
| T15 | Modo servidor opcional | 8 | 0 | `PENDIENTE` |
| **Total** |  | **149** | **0** | **0 %** |

La fuente registra T08 `PARCIAL` por seis tandas semánticas en `45dea40ec..72f7ad7cb` y correcciones
`a83e359a2`/`ad374b717`. También declara explícitamente que eso no cierra T08. Por ello no suma elementos cerrados.

## C.2 Inventario granular

### T01 — telemetría profunda

- [ ] `NS305-T01-F01` — `DriverExecutionScope` posee `CompilationMetrics`, no lista plana.
- [ ] `NS305-T01-F02` — `DriverPhaseTimer` admite fases anidadas y `parent_id`.
- [ ] `NS305-T01-F03` — `CompilationTelemetry` publica JSON schema v2.
- [ ] `NS305-T01-F04` — Instrumentar proyectos/fuentes, I/O, tokens, AST/HIR/MIR, símbolos, pools, hashing,
  cachés, NSMETA, LLVM, archive, link y publicación.
- [ ] `NS305-T01-F05` — `MetricScope` sin allocations cuando telemetría está desactivada.
- [ ] `NS305-T01-F06` — Contadores por worker fusionados al final, sin log por token/nodo.

### T02 — servicio, sesión y scheduler compartidos

- [ ] `NS305-T02-F01` — Crear `CompilationService`, `CompilationSession`, request y result.
- [ ] `NS305-T02-F02` — Mover loader, cache y pool fuera de `DriverExecutionScope`.
- [ ] `NS305-T02-F03` — Convertir `Driver::run` en adaptador request/result.
- [ ] `NS305-T02-F04` — Mantener compatibilidad para consumidores directos de `Driver`.
- [ ] `NS305-T02-F05` — Reutilizar un `CompilationGraphScheduler`.
- [ ] `NS305-T02-F06` — Scheduler con ready queue, dependency counts, completions y commit determinista.
- [ ] `NS305-T02-F07` — Inyectar pool compartido en loader, compilation, HIR, MIR y backend.
- [ ] `NS305-T02-F08` — Prohibir pools locales en GraphLoader, ProjectCompilation, NameLookup, HIR, MIR y
  object generator.

### T03 — snapshots de proyectos y fuentes

- [ ] `NS305-T03-F01` — `candidateKeys` hash set y vector separado de orden estable.
- [ ] `NS305-T03-F02` — `ProjectSnapshotStore` con descriptor, patterns, manifest, directory stamps y referencias.
- [ ] `NS305-T03-F03` — Consultar manifest antes de enumeración recursiva.
- [ ] `NS305-T03-F04` — Reenumerar sólo subárbol cuyo sello cambió.
- [ ] `NS305-T03-F05` — No usar sólo `last_write_time` como identidad.
- [ ] `NS305-T03-F06` — `SourceSnapshotStore` fast key + SHA-256 cuando cambia.
- [ ] `NS305-T03-F07` — Buffers inmutables en `shared_ptr<const SourceBuffer>`.
- [ ] `NS305-T03-F08` — Compartir snapshot del mismo archivo entre proyectos.
- [ ] `NS305-T03-F09` — Leer `NS_LIB_ROOT` una vez por request.

### T04 — catálogo de parser e índices declarativos

- [ ] `NS305-T04-F01` — `ParserTypeCatalog` inmutable e internado.
- [ ] `NS305-T04-F02` — Separar seeds, aliases y merge del catálogo.
- [ ] `NS305-T04-F03` — Fingerprint declarativo por unidad sin resolver cuerpos.
- [ ] `NS305-T04-F04` — IDs internados; ordenar sólo al publicar.
- [ ] `NS305-T04-F05` — Grafo de aliases sin producto cartesiano.
- [ ] `NS305-T04-F06` — `ProjectSyntaxUnit::parse` recibe catálogo compartido const.
- [ ] `NS305-T04-F07` — Parser conserva vista al catálogo sin copiar nombres.
- [ ] `NS305-T04-F08` — `DeclarationIndex` por unidad.
- [ ] `NS305-T04-F09` — Merge estable por módulo y diagnóstico determinista; buffer/tokens/AST comparten
  arena inmutable.

### T05 — NSMETA como índice binario

- [ ] `NS305-T05-F01` — Crear `MetadataStore` y `PrecompiledSymbolIndex`.
- [ ] `NS305-T05-F02` — Decode por header, surface, ABI, templates, specializations y deps.
- [ ] `NS305-T05-F03` — Carga lazy de secciones.
- [ ] `NS305-T05-F04` — Cache key con path, file ID, size, mtime, payload hash, schema y ABI.
- [ ] `NS305-T05-F05` — Insertar índice directo en NameLookup y modelos posteriores.
- [ ] `NS305-T05-F06` — Cuerpos genéricos en representación parseada cacheable mientras no haya formato estable.
- [ ] `NS305-T05-F07` — Eliminar lexer/parser de superficie NSMETA no genérica.
- [ ] `NS305-T05-F08` — Conservar current-compiler, ABI fingerprint y binary closure.
- [ ] `NS305-T05-F09` — No omitir validación sólo por path/timestamp y mantener lectura del schema actual.

### T06 — índices canónicos de nombres, tipos y visibilidad

- [ ] `NS305-T06-F01` — `TypeIndex` con key, AST, module/name, container y visibility.
- [ ] `NS305-T06-F02` — API O(1) `TypeIdForKey`.
- [ ] `NS305-T06-F03` — Eliminar barridos de inheritance/interface ordinal.
- [ ] `NS305-T06-F04` — Migrar mapa de member type ordinal al índice único.
- [ ] `NS305-T06-F05` — Calcular closure de visibilidad una vez con topología, bitsets y unión memoizada.
- [ ] `NS305-T06-F06` — Internar nombres y type keys.
- [ ] `NS305-T06-F07` — Strings sólo en interner, diagnósticos y formatos externos.

### T07 — genéricos incrementales

- [ ] `NS305-T07-F01` — Consumir `DeclarationIndex`, no recorrer AST completo.
- [ ] `NS305-T07-F02` — Mapas Template/Instantiation/dependencies/result/fingerprint.
- [ ] `NS305-T07-F03` — Lookup directo de template state.
- [ ] `NS305-T07-F04` — Separar syntax, declaration, substitution y specialization.
- [ ] `NS305-T07-F05` — Work queue determinista de instantiations.
- [ ] `NS305-T07-F06` — Registrar queries de types/constraints/members leídas.
- [ ] `NS305-T07-F07` — Nueva demanda agrega sólo instantiation y deps.
- [ ] `NS305-T07-F08` — Límites de profundidad/cantidad conservan diagnósticos.

### T08 — análisis de cuerpos aislado y paralelizable

- [ ] `NS305-T08-F01` — Mover datos inmutables a `BodyBindingContext`.
- [ ] `NS305-T08-F02` — Mover estado de callable a `BodyBinder`.
- [ ] `NS305-T08-F03` — `Bind(BodyId)` devuelve `BodyBindingResult` sin mutar globales.
- [ ] `NS305-T08-F04` — Scheduler ejecuta ola de cuerpos independientes.
- [ ] `NS305-T08-F05` — Ordenar por `BodyId` y reducir serial/determinísticamente.
- [ ] `NS305-T08-F06` — Demandas genéricas crean siguiente ola sin reiniciar anterior.
- [ ] `NS305-T08-F07` — Scope lookup por tabla `NameId -> ScopeName`.
- [ ] `NS305-T08-F08` — Member candidates devuelve vista estable.
- [ ] `NS305-T08-F09` — Separar escaneo declarativo de análisis de cuerpo.
- [ ] `NS305-T08-F10` — Out-parameter verification dentro del resultado de cuerpo.
- [ ] `NS305-T08-F11` — Una call graph `forceinline` y un Tarjan SCC; fachada sin estado mutable de callable.

### T09 — HIR y MIR por cuerpo con caché

- [ ] `NS305-T09-F01` — `HirLoweringContext` ligero sobre `SemanticIndex` inmutable.
- [ ] `NS305-T09-F02` — Bindings AST pasan al índice semántico.
- [ ] `NS305-T09-F03` — `LowerHir(BodyId)` produce `HirBody` inmutable y deps.
- [ ] `NS305-T09-F04` — Modelo HIR como tabla ordenada de bodies compartidos.
- [ ] `NS305-T09-F05` — No crear worker ProjectOopHirModel completo por miembro.
- [ ] `NS305-T09-F06` — `MirLoweringContext` por tarea sin copiar modelo.
- [ ] `NS305-T09-F07` — `LowerMir(BodyId)` produce `MirBody` inmutable.
- [ ] `NS305-T09-F08` — Registrar TypeIds usados durante HIR.
- [ ] `NS305-T09-F09` — Internar structural/tuple types.
- [ ] `NS305-T09-F10` — Layout table una vez por TypeId demandado.
- [ ] `NS305-T09-F11` — MIR project ensambla vistas, no clona árboles.
- [ ] `NS305-T09-F12` — Fingerprints excluyen pointers y ordinals incidentales.

### T10 — identidad Merkle y artifact store

- [ ] `NS305-T10-F01` — `FingerprintBuilder` streaming reemplaza `ostringstream`.
- [ ] `NS305-T10-F02` — Fingerprints únicos de config/common model/type/function/partition.
- [ ] `NS305-T10-F03` — Partition identity como Merkle hash versionado y ordenado.
- [ ] `NS305-T10-F04` — Cache input recibe hashes, no serialización MIR completa.
- [ ] `NS305-T10-F05` — Manifest de objeto con size/hash/dependencies.
- [ ] `NS305-T10-F06` — Verificación en store y escritura atómica.
- [ ] `NS305-T10-F07` — Restore confía en CAS; scrub explícito rehashea.
- [ ] `NS305-T10-F08` — Hardlink inmutable si es seguro; copia fallback.
- [ ] `NS305-T10-F09` — Nunca hardlink de archivos mutables posteriores.
- [ ] `NS305-T10-F10` — Deduplicar producers concurrentes por future compartido.

### T11 — backend persistente por worker

- [ ] `NS305-T11-F01` — `BackendSession` con target config inmutable.
- [ ] `NS305-T11-F02` — `BackendWorkerContext` por worker.
- [ ] `NS305-T11-F03` — Object generator obtiene contexto por worker index.
- [ ] `NS305-T11-F04` — Eliminar creación TargetMachine por módulo.
- [ ] `NS305-T11-F05` — Reutilizar data layout retenido.
- [ ] `NS305-T11-F06` — Limpiar module/scratch sin destruir máquina.
- [ ] `NS305-T11-F07` — No compartir máquina mutable entre workers.
- [ ] `NS305-T11-F08` — `forceinline`: graph, SCC y colocación caller/callee.
- [ ] `NS305-T11-F09` — Full LTO monolítico sólo cuando se solicita.
- [ ] `NS305-T11-F10` — Liberar módulo tras emitir objeto.

### T12 — NSMETA de salida, header y artefactos incrementales

- [ ] `NS305-T12-F01` — `PublicSurfaceFingerprint` desde declarations/layout/ABI/templates.
- [ ] `NS305-T12-F02` — Encode metadata sólo si cambia fingerprint.
- [ ] `NS305-T12-F03` — Fingerprint independiente para header.
- [ ] `NS305-T12-F04` — Validación estructural barata en cada producción.
- [ ] `NS305-T12-F05` — Memoizar roundtrip por payload hash.
- [ ] `NS305-T12-F06` — Verificación exhaustiva sólo en certificación explícita.
- [ ] `NS305-T12-F07` — Evitar merger/copia de modelo ya canonicalizado.
- [ ] `NS305-T12-F08` — Staging sólo cuando contenido difiere.

### T13 — archive, link y publicación incremental

- [ ] `NS305-T13-F01` — Calcular `cachedFileSetIdentity` una vez.
- [ ] `NS305-T13-F02` — `ArtifactManifest` ordenado.
- [ ] `NS305-T13-F03` — Archive key incluye objetos/runtime/mode/toolchain.
- [ ] `NS305-T13-F04` — Restore archive por key sin recalcular inputs.
- [ ] `NS305-T13-F05` — Update members sólo si NSLink lo soporta; rebuild en miss.
- [ ] `NS305-T13-F06` — Link key incluye inputs/options/entry/build ID.
- [ ] `NS305-T13-F07` — Omitir link sólo con key y output manifest coincidentes.
- [ ] `NS305-T13-F08` — `OutputPlanner` decide outputs antes del pipeline.
- [ ] `NS305-T13-F09` — `ArtifactPublisher` publica diferencias con rename atómico.
- [ ] `NS305-T13-F10` — No tocar timestamps idénticos.

### T14 — separación física de archivos gigantes

- [ ] `NS305-T14-F01` — Extraer `BodyBinder.Expressions.cpp`.
- [ ] `NS305-T14-F02` — Extraer `BodyBinder.Statements.cpp`.
- [ ] `NS305-T14-F03` — Extraer `BodyBinder.Calls.cpp`.
- [ ] `NS305-T14-F04` — Extraer `BodyBinder.Ownership.cpp`.
- [ ] `NS305-T14-F05` — Extraer `BodyBinder.Exceptions.cpp`.
- [ ] `NS305-T14-F06` — Extraer `BodyBinder.Initializers.cpp`.
- [ ] `NS305-T14-F07` — Extraer `BodyBinder.Patterns.cpp`.
- [ ] `NS305-T14-F08` — Extraer `LlvmLowering.Expressions.cpp`.
- [ ] `NS305-T14-F09` — Extraer `LlvmLowering.ControlFlow.cpp`.
- [ ] `NS305-T14-F10` — Extraer `LlvmLowering.Aggregates.cpp`.
- [ ] `NS305-T14-F11` — Extraer `LlvmLowering.Runtime.cpp`.
- [ ] `NS305-T14-F12` — Extraer `LlvmLowering.Exceptions.cpp`.
- [ ] `NS305-T14-F13` — Extraer `LlvmLowering.DebugInfo.cpp`.
- [ ] `NS305-T14-F14` — Extraer `DriverArguments.cpp`.
- [ ] `NS305-T14-F15` — Extraer `DriverProjectCommand.cpp`.
- [ ] `NS305-T14-F16` — Extraer `DriverEmitExecutable.cpp`.
- [ ] `NS305-T14-F17` — Extraer `DriverEmitLibrary.cpp`.
- [ ] `NS305-T14-F18` — Extraer `DriverMetadata.cpp`.
- [ ] `NS305-T14-F19` — Extraer `DriverPublication.cpp`.
- [ ] `NS305-T14-F20` — Extraer primero sin cambiar comportamiento.
- [ ] `NS305-T14-F21` — Interfaces estrechas; no depender del header gigante.
- [ ] `NS305-T14-F22` — No duplicar helpers estáticos.
- [ ] `NS305-T14-F23` — Medir construcción de NSC antes/después.
- [ ] `NS305-T14-F24` — No sustituir reingeniería de runtime con separación física.

### T15 — modo servidor opcional

- [ ] `NS305-T15-F01` — `nsc --server` posee services/sessions.
- [ ] `NS305-T15-F02` — Request versionada por named pipe local.
- [ ] `NS305-T15-F03` — Response con diagnostics, metrics y output manifest.
- [ ] `NS305-T15-F04` — Validar que todas las rutas pertenecen al request.
- [ ] `NS305-T15-F05` — Escrituras transaccionales.
- [ ] `NS305-T15-F06` — Fallback in-process si servidor/protocolo no coincide.
- [ ] `NS305-T15-F07` — Servidor opcional, nunca requisito.
- [ ] `NS305-T15-F08` — Misma semántica y determinismo; no ocultar repetición in-process.

## C.3 Criterio de cierre

Cada elemento requiere métricas antes/después, contrato inalterado, determinismo, prueba puntual y evidencia del
costo que elimina. T01-T15 se recorren una sola vez por ciclo; no se ejecuta batería integral entre tandas. El cierre
de Parte C exige 149/149, una batería final completa y ausencia de regresiones semánticas, ABI, outputs o toolchain.

---

# Parte D — NativeSharp 3.0.6: documentación, Allman y calidad interna

## D.1 Frontera

Esta parte es futura y no autoriza cambios funcionales. Su objeto es auditar y corregir presentación,
modularidad interna, documentación, comentarios, formato y calidad estructural sin cambiar APIs, ABI, semántica,
algoritmos observables, diagnósticos, outputs ni configuración. Cualquier cambio funcional descubierto se registra
como hallazgo fuera de D y requiere autorización separada.

Los diez frentes siguientes son agrupaciones de auditoría creadas por este maestro. Las reglas dentro de ellos
provienen literalmente de [LINEAMIENTOS-DE-TRABAJO-Y-PORTEO.md](../LINEAMIENTOS-DE-TRABAJO-Y-PORTEO.md).

| Tanda | Frente | Unidad de cierre | Estado |
|---|---|---:|---|
| D00 | Inventario, alcance y procedencia | 1 | `PENDIENTE` |
| D01 | Arquitectura y modularidad | 1 | `PENDIENTE` |
| D02 | Austeridad y proporcionalidad | 1 | `PENDIENTE` |
| D03 | Fronteras, validación y rutas calientes | 1 | `PENDIENTE` |
| D04 | Tamaño, estilo y Allman | 1 | `PENDIENTE` |
| D05 | Comentarios formativos | 1 | `PENDIENTE` |
| D06 | Algoritmos y complejidad | 1 | `PENDIENTE` |
| D07 | Eliminación de recursión no autorizada | 1 | `PENDIENTE` |
| D08 | Constitución documental y didáctica | 1 | `PENDIENTE` |
| D09 | Auditoría independiente y cierre | 1 | `PENDIENTE` |
| **Total** |  | **10** | **0/10; 0 %** |

## D.2 Inventario, auditoría y criterio por tanda

### D00 — inventario, alcance y procedencia

**Fuente:** §18.2, líneas 4631 y siguientes; §18.11, líneas 4855-4871.

- [ ] Inventariar repositorios, directorios, archivos, tipos, funciones, comentarios y documentos del alcance.
- [ ] Registrar ruta, revisión, fecha, símbolo, documento y tipo de evidencia por afirmación importante.
- [ ] Separar hecho, inferencia, contradicción, riesgo y no verificado.
- [ ] Congelar denominadores antes de editar.

**Cierre:** inventario reproducible, cero archivo o símbolo sin clasificar y procedencia localizable.

### D01 — arquitectura y modularidad

**Fuente:** §9, líneas 2790-2815.

- [ ] Cada componente tiene responsabilidad cohesionada, dependencias, contrato, límite de prueba y reemplazo.
- [ ] No hay god classes ni mezcla de parsing, semántica, generación, I/O, UI, logging y configuración.
- [ ] No hay estado global mutable que oculte dependencias.
- [ ] Hay un solo tipo por archivo.
- [ ] Puntos de composición y contratos entre subsistemas son explícitos.

**Cierre:** matriz componente-responsabilidad-dependencia-prueba completa y cero violaciones abiertas.

### D02 — austeridad y proporcionalidad

**Fuente:** §9, líneas 2817-2871.

- [ ] Cada excepción, guard, capa o abstracción responde a riesgo, contrato y prueba concretos.
- [ ] No hay `try/catch`, throw/retry, lambdas, reflection, factories, adapters o wrappers ornamentales.
- [ ] No se repiten defensas ya garantizadas por una frontera responsable.
- [ ] Se prefieren funciones directas, state machines, estructuras conocidas y early returns legibles.
- [ ] Existe pasada independiente de austeridad y se documenta cada elemento conservado.

**Cierre:** cero defensa o abstracción sin riesgo, responsable, costo y prueba identificados.

### D03 — fronteras, validación y rutas calientes

**Fuente:** §9, líneas 2873-2957.

- [ ] Clasificar fronteras no confiables, ABI/FFI/OS, API pública, núcleo normalizado y hot/cold paths.
- [ ] Validar una vez en la frontera más temprana con toda la información.
- [ ] Documentar riesgo, imposibilidad de prueba estática, owner, frecuencia, costo, fallo y prueba.
- [ ] No repetir type/null/range/state checks ni crear allocations, closures, strings o logs por operación caliente.
- [ ] `Debug` añade diagnóstico; `Release` conserva el mismo contrato funcional y validaciones necesarias.

**Cierre:** mapa de fronteras y presupuestos; cero validación redundante o costo oculto sin evidencia.

### D04 — tamaño, estilo y Allman

**Fuente:** §9, líneas 2959-2975 y 3004-3007.

- [ ] Máximo 120 **caracteres** por línea.
- [ ] Un tipo declarado por archivo.
- [ ] API NativeSharp pública/protegida/externa en PascalCase.
- [ ] Código explícito; evitar lambdas, capas y handlers innecesarios.
- [ ] No comprimir varias operaciones, condiciones, funciones o `if` en una línea.
- [ ] Preferir 20-40 líneas ejecutables por función; nunca superar 60.
- [ ] Firma, blancos y comentario documental no cuentan en el límite ejecutable.
- [ ] Reducir nesting, complejidad, responsabilidades y mutable state.
- [ ] Llaves de clase, función y control en líneas independientes, estilo Allman.
- [ ] Dividir parámetros sólo por legibilidad o límite de línea, no por regla Allman.

**Cierre:** auditoría mecánica y revisión humana con cero violaciones; no se usa el límite inexistente de 120 líneas.

### D05 — comentarios formativos

**Fuente:** §9, líneas 2977-2993.

- [ ] Cada función/método, incluidos privados y pequeños, documenta lo aplicable.
- [ ] Propósito, parámetros, unidades, precondiciones, retorno y poscondiciones.
- [ ] Efectos laterales, errores y casos límite.
- [ ] Ownership, lifetime, mutabilidad y thread safety.
- [ ] Invariantes, algoritmo y motivo, complejidad, upstream, limitaciones y workarounds.
- [ ] Ningún comentario se limita a repetir el nombre.
- [ ] Conservar comentarios útiles y no reescribir archivos no relacionados (§9, líneas 2995-3002).

**Cierre:** 100 % de funciones/métodos auditados y cero comentario nominal vacío o contrato relevante omitido.

### D06 — algoritmos y complejidad

**Fuente:** §10, líneas 3009-3038.

- [ ] Elegir la solución correcta y eficiente de menor complejidad conceptual/accidental.
- [ ] Documentar input, límites, adversariales, frecuencia, operaciones dominantes y alternativas.
- [ ] Documentar tiempo, espacio, allocations, locality, determinismo y concurrencia.
- [ ] No justificar ineficiencia con “datos pequeños”, facilidad o temporalidad.
- [ ] Eliminar quadratic sorts, repeated scans, linear lookups, copias y recomputación evitables.
- [ ] Complejidad inherente alta tiene límites y pruebas proporcionales.

**Cierre:** cada algoritmo no trivial tiene análisis y evidencia; cero ineficiencia evitable abierta.

### D07 — eliminación de recursión no autorizada

**Fuente:** §10, líneas 3040-3070.

- [ ] Inventariar recursión directa, indirecta, mutua y reentrada por callbacks.
- [ ] Eliminar recursión en AST, parsers, trees, graphs, dependencies, files o profundidad no acotada.
- [ ] Sustituir por stack, queue, deque, worklist o state machine explícita.
- [ ] No ocultar recursión en helpers ni dejarla como deuda temporal.
- [ ] Sólo conservar solicitud académica explícita o excepción productiva aprobada por escrito.
- [ ] Excepción aprobada demuestra depth, frame, stack budget, input limits, adversariales y boundary tests.

**Cierre:** cero recursión no autorizada y expediente completo por cada excepción.

### D08 — constitución documental y progresión didáctica

**Fuente:** §18, líneas 4554-5018; especialmente §18.9-18.11, líneas 4813-4871.

- [ ] Inventario previo, gate de integridad y matriz de cobertura.
- [ ] Separar contratos, arquitectura, implementación, pruebas, historia, contradicciones y propuestas.
- [ ] No resolver contradicciones silenciosamente.
- [ ] Documentar para reconstrucción, no sólo descripción.
- [ ] Dividir documentos por responsabilidad conservando navegación bidireccional.
- [ ] Orden didáctico: contexto, fundamentos, arquitectura, contratos, estructuras, algoritmos, flujo, bordes,
  pruebas, historia, implementación y reconstrucción.
- [ ] No asumir conversaciones ni conocimiento externo; declarar ruta de lectura.
- [ ] Procesar commits y Markdown completos; resúmenes no sustituyen evidencia.
- [ ] Mantener cursor/estado persistente y auditoría independiente por componente.

**Cierre:** gate de reconstructibilidad completo, procedencia por afirmación y cero recorte u omisión silenciosa.

### D09 — auditoría independiente y cierre

**Fuente:** §17, checklist de cierre, líneas 4500-4552; §18.16-18.18.

- [ ] Reauditar el inventario completo después de todas las correcciones de D00-D08.
- [ ] Confirmar un tipo por archivo, 120 caracteres, 60 líneas ejecutables, Allman y comentarios.
- [ ] Confirmar cero clases monolíticas, wrappers/defensas ornamentales y recursión no aprobada.
- [ ] Confirmar que no cambió comportamiento, API, ABI, configuración ni outputs.
- [ ] Actualizar tracking, matriz, evidencia, contradicciones y bitácora.
- [ ] Ejecutar sólo al final la validación completa autorizada de calidad/formato/documentación.

**Cierre:** 10/10 frentes, auditoría independiente sin hallazgos, batería final aplicable y commit de cierre.

---

## 4. Campaña persistente encadenada A -> B -> C -> D

### 4.1 Autoridad de rama y secuencia

- Todo trabajo activo ocurre siempre en `development`.
- Ninguna parte comienza sin cierre certificado de la parte anterior.
- Al cerrar cada parte se crea un commit exacto de cierre en `development`.
- Desde ese commit exacto se crea, sin anticiparla, la rama de frontera correspondiente.
- Después de crear la rama de frontera, el trabajo activo continúa en `development`.
- Una frontera certificada no se mueve sin instrucción explícita posterior.
- Este documento no crea ramas; sólo registra la política autorizada.

### 4.2 Puertas de transición

| Puerta | Precondición obligatoria | Commit/rama de frontera | Parte que se abre |
|---|---|---|---|
| Inicio A | Inventario causal: 52 KPD, causas y grafo | Trabajo en `development` | A / 3.0.3 |
| A -> B | 52/52; A00-A13; batería A en cero; commit | `v.3.0.3` desde commit exacto | B / 3.0.4 |
| B -> C | 147/147; T00-T17; suite/baseline; commit | `v.3.0.4` desde commit exacto | C / 3.0.5 |
| C -> D | 149/149; T01-T15; métricas; batería en cero | `v.3.0.5` desde commit exacto | D / 3.0.6 |
| Cierre D | 10/10 auditorías; cero cambio funcional | `v.3.0.6` desde commit exacto | Campaña cerrable |

Si falla una puerta, la parte no transiciona: se abre un nuevo ciclo desde su primera tanda, se auditan todas sus
tandas y sólo se repite la batería completa después de cerrar todas las causas conocidas.

### 4.3 Artefactos obligatorios por parte

1. `SCOPE LOCK` con archivos permitidos/protegidos, pruebas y acciones prohibidas.
2. Inventario congelado legible y, cuando aplique, versión machine-readable.
3. Matriz ID -> contrato -> fuente -> productor -> consumidores -> prueba -> estado.
4. Agrupación causal y grafo productor-consumidor antes de implementar.
5. Documento por ítem/corrección con propósito, causa, archivos, algoritmo, riesgos y prueba puntual.
6. Commit independiente por hipótesis causal cohesionada.
7. Evidencia puntual con ciclo, revisión, comando, resultado y artefactos.
8. Documento y commit de cierre por tanda.
9. Auditoría final de cobertura y contradicciones.
10. Reporte de batería completa final con descubiertas/ejecutadas/aprobadas/fallidas/omitidas/excluidas.
11. Commit exacto de cierre de parte.
12. Evidencia de creación de rama de frontera desde ese commit y retorno/continuidad en `development`.

---

## 5. Bitácora de actualizaciones

| Fecha | Ciclo | Cambio | Evidencia | Resultado |
|---|---:|---|---|---|
| 2026-10-11 | 0 | Creación del maestro; inventarios A-D y política encadenada | fuentes de §1 | `PENDIENTE` |
| 2026-10-11 | 0 | Reconciliar 47 outcomes + 5 laterales = 52 KPD | manifest + outcomes | 52 únicos |
| 2026-10-11 | 0 | Validación de plan lingüístico | plan 3.0.4 | 147 únicos, 0 duplicados |
| 2026-10-11 | 0 | Reasignar rendimiento a 3.0.5 | fuente intacta | 15 tandas, 149 elementos |
| 2026-10-11 | 0 | Alta futura de calidad 3.0.6 | lineamientos §9, §10, §18 | 10 frentes |
| 2026-10-11 | 1 | Cierre A00 G01-G04 | siete puntuales + consolidado A00 | 7/7 `PASS`; 45 KPD abiertos |
| 2026-10-11 | 1 | Cierre A01 G05 host | puntual + consolidado A01 | 1/1 `PASS`; 44 KPD abiertos |
| 2026-10-11 | 1 | Cierre A02 G06 | puntual KPD + Required | 1/1 `PASS`; 43 KPD abiertos |
| 2026-10-11 | 1 | Cierre A02 G07 | puntual KPD + Required | 1/1 `PASS`; 42 KPD abiertos |
| 2026-10-12 | 1 | Cierre A02 G08 y consolidado | puntual KPD + Required; A02 3/3 | `PASS`; 41 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G09 | puntual KPD + Required | 1/1 `PASS`; 40 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G10 | dos puntuales KPD + Required | 2/2 `PASS`; 38 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G11 y subbloque G09-G11 | puntual KPD + Required; subbloque 4/4 | `PASS`; 37 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G12 vertical | oráculo fortalecido + Parser-Sema-HIR KPD/Required | `PASS`; 36 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G13 | puntual KPD + Required | `PASS`; 35 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G14 | puntual KPD + Required | `PASS`; 34 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G15 | puntual KPD + Required | `PASS`; 33 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G16 | puntual KPD + Required | `PASS`; 32 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G17 | puntual KPD + Required | `PASS`; 31 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G18 | puntual KPD + Required | `PASS`; 30 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G19 | puntual KPD + Required | `PASS`; 29 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G20 | puntual KPD + Required | `PASS`; 28 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G21 | dos puntuales KPD + Required | `PASS`; 26 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G22 | puntual KPD + Required | `PASS`; 25 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G23 | puntual KPD + Required | `PASS`; 24 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G24 | puntual KPD + Required | `PASS`; 23 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G25 | puntual aislada KPD + Required | `PASS`; 22 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G26 | puntual KPD + Required | `PASS`; 21 KPD abiertos |
| 2026-10-12 | 1 | Cierre A03 G27 y tanda | puntual KPD + Required; consolidado A03 21/21 | `PASS`; 20 KPD abiertos |
| 2026-10-12 | 1 | Cierre A04 G28 | dos puntuales KPD + Required | 2/2 `PASS`; 18 KPD abiertos |
| 2026-10-12 | 1 | Cierre A04 G29 | puntual KPD + Required; guard de orden global | `PASS`; 17 KPD abiertos |
| 2026-10-12 | 1 | Cierre A04 G30 | terminal aislado + puntual KPD corregido + Required | `PASS`; 16 KPD abiertos |
| 2026-10-12 | 1 | Cierre A04 G31 y tanda | puntual KPD + Required; consolidado A04 5/5 | `PASS`; 15 KPD abiertos |
| 2026-10-12 | 1 | Cierre A05 G32 y tanda | puntual KPD + Required; consolidado A05 1/1 | `PASS`; 14 KPD abiertos |
| 2026-10-12 | 1 | Cierre A06 G33 y tanda | puntual KPD + Required; consolidado A06 1/1 | `PASS`; 13 KPD abiertos |
| 2026-10-12 | 1 | Cierre A07 G34 y tanda | puntual KPD + Required; consolidado A07 1/1 | `PASS`; 12 KPD abiertos |
| 2026-10-12 | 1 | Cierre A08 G35 | puntual KPD + Required; A08 1/2 | `PASS`; 11 KPD abiertos |

## 6. Estado al corte

- A: `41/52`; A08 G35 cerrado 1/2; siguiente acción vinculante: A08 G36 (`TSH-00214`).
- B: `0/147`; bloqueada por cierre certificado de A.
- C: `0/149`; bloqueada por cierre certificado de B.
- D: `0/10`; bloqueada por cierre certificado de C.
- Ramas `v.3.0.3` a `v.3.0.6`: no creadas; sólo se crean desde commits certificados futuros.
- Validación ejecutada: A00-A07 puntuales y consolidados por tanda; ninguna batería completa.

