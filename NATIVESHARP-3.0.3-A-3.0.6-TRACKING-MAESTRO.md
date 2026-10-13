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
| A | 3.0.3 | 14 | 52 defectos | 52 | 100 % | `TANDAS_CERRADAS`; batería final pendiente |
| B | 3.0.4 | 18 | 147 ítems | 12 | 8.16 % | `ACTIVA`; T00 cerrada; T01-F01/F02/F03/F04 cerradas, T01-F05 activa |
| C | 3.0.5 | 15 | 149 elementos granulares | 0 | 0 % | `PENDIENTE`; T8 tiene evidencia parcial no terminal |
| D | 3.0.6 | 10 | 10 frentes de auditoría | 0 | 0 % | `PENDIENTE` futuro, sin cambios funcionales |

---

# Parte A — NativeSharp 3.0.3: 52 Known Product Defects

## A.1 Regla de autoridad

El universo se calcula como unión por `case_id` de:

1. 47 filas iniciales del manifiesto con `expected_outcome=KNOWN_FAILURE`;
2. 41 filas con tag `known-product-defect` o `known-failure`;
3. cinco outcomes complementarios en `nsc-builder/metadata/expected-outcomes.json`.

La unión inicial da 52 IDs únicos. Tras cerrar G41 y A13, quedan 0 KPD abiertos y 52 casos Required. `KPD`
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
| A08 | SelfHost.Diagnostics | 2 | 2 | `CERRADA` |
| A09 | SelfHost.Lexer | 1 | 1 | `CERRADA` |
| A10 | SelfHost.Source | 1 | 1 | `CERRADA` |
| A11 | SelfHost.Types | 2 | 2 | `CERRADA` |
| A12 | BuildSystem | 2 | 2 | `CERRADA` |
| A13 | Packaging | 4 | 4 | `CERRADA` |
| **Total** |  | **52** | **52** | **100 %** |

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
  - Cierre: `CERRADO`; G36 pasó como KPD y Required para Error, Warning y Note y cerró A08 2/2;
    commit G36: este commit.
- **A09 · `TSH-00442` / `CSH-LEX-007`**
  - Contrato: triple raw literal vacío conserva token, valor decodificado, rango y cero errores.
  - Estado/evidencia: `KNOWN_FAILURE`; M:5613; T:`SelfHostLexerLiteralsModule.ns`; `5dee730d`.
  - Cierre: `CERRADO`; el productor self-host pasó como KPD y Required y cerró A09 1/1; commit G05 self-host:
    este commit.
- **A10 · `TSH-00903` / `CSH-SRC-008`**
  - Contrato: archivo ausente no se publica como buffer vacío aceptado.
  - Estado/evidencia: `KNOWN_FAILURE` lateral; M:5666; C:`SelfHostSourceCaseClassifications.jsonl`.
  - Procedencia: `dc3ee85f`. Cierre: `CERRADO`; G37 pasó como KPD y Required y cerró A10 1/1;
    commit G37: este commit.
- **A11 · `TSH-01011` / `CSH-TYP-005`**
  - Contrato: count no numérico permanece descriptor Named, no Array de count cero.
  - Estado/evidencia: `KNOWN_FAILURE` lateral; M:5557; C:`SelfHostTypesCaseClassifications.jsonl`.
  - Procedencia: `dc3ee85f`. Cierre: `CERRADO`; G38 pasó como KPD y Required sin mutación parcial;
    commit G38: este commit.
- **A11 · `TSH-01023` / `CSH-TYP-007`**
  - Contrato: `IsInteger`/`IsNumeric` dan false para IDs inválidos sin indexar fuera de tabla.
  - Estado/evidencia: `KNOWN_FAILURE` lateral; M:5566; C:`SelfHostTypesCaseClassifications.jsonl`.
  - Procedencia: `dc3ee85f`. Cierre: `CERRADO`; G39 pasó como KPD y Required y cerró A11 2/2;
    commit G39: este commit.
- **A12 · `T12B-00021` / `C12B-BLD-004`**
  - Contrato: rechazar NSLink producer vacío antes de derivar rutas self-host runtime.
  - Estado/evidencia: `KNOWN_FAILURE` lateral; M:5691; C:`BuildSystemCaseClassifications.jsonl`.
  - Procedencia: `dc3ee85f`. Cierre: `CERRADO`; el helper rechaza productor vacío antes de derivar o publicar;
    commit G40: este commit.
- **A12 · `T12B-00022` / `C12B-BLD-004`**
  - Contrato: rechazar NSLink path malformado antes de publicar rutas self-host runtime.
  - Estado/evidencia: `KNOWN_FAILURE` lateral; M:5692; C:`BuildSystemCaseClassifications.jsonl`.
  - Procedencia: `dc3ee85f`. Cierre: `CERRADO`; el helper rechaza paths no absolutos antes de derivar o publicar;
    commit G40: este commit.
- **A13 · `T12B-00457` / `C12B-PKG-001`**
  - Contrato: `deploy.bat` respeta build/prefix del caller y no usa rutas hardcoded.
  - Estado/evidencia: `KNOWN_FAILURE`; M:5707; C:`PackagingCaseClassifications.jsonl`; `5dee730d`.
  - Cierre: `CERRADO`; build y prefix explícitos llegan citados a `cmake --install`; commit G41: este commit.
- **A13 · `T12B-00458` / `C12B-PKG-001`**
  - Contrato: `deploy.bat` acepta y preserva paths con espacios o Unicode.
  - Estado/evidencia: `KNOWN_FAILURE`; M:5708; C:`PackagingCaseClassifications.jsonl`; `5dee730d`.
  - Cierre: `CERRADO`; espacios de paths representativos se preservan al invocar CMake; commit G41: este commit.
- **A13 · `T12B-00460` / `C12B-PKG-001`**
  - Contrato: destino drive-root inválido se rechaza antes de publicar.
  - Estado/evidencia: `KNOWN_FAILURE`; M:5710; C:`PackagingCaseClassifications.jsonl`; `5dee730d`.
  - Cierre: `CERRADO`; el destino raíz se rechaza antes de localizar o invocar CMake; commit G41: este commit.
- **A13 · `T12B-00461` / `C12B-PKG-001`**
  - Contrato: build y prefix iguales se rechazan antes de publicar.
  - Estado/evidencia: `KNOWN_FAILURE`; M:5711; C:`PackagingCaseClassifications.jsonl`; `5dee730d`.
  - Cierre: `CERRADO`; build y prefix iguales se rechazan antes de publicar; commit G41: este commit.

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
  - Estado: `CERRADO`; productor host cerrado en A01 y productor self-host cerrado en A09 con el mismo contrato.
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
  - Puntual: `SelfHostDiagnosticsRenderingModule.ns`; pasó como KPD y Required y cubre las tres variantes del enum.
  - Estado: `CERRADO` en A08; JSON traduce Error, Warning y Note a sus nombres canónicos.
- **G37 — error de lectura colapsa a vacío: `TSH-00903`.**
  - Productores: `File.ReadAllText` y `SourceBuffer.Load` self-host.
  - Causa: fallo devuelve `""` y Load construye buffer, indistinguible de archivo vacío válido.
  - Consumidores: Lexer, Parser, Bootstrap y Driver self-host.
  - Puntual: `SelfHostSourceLoadingModule.ns`; pasó como KPD y Required, con archivos vacío y no vacío como guards.
  - Estado: `CERRADO` en A10; `File.GetSize == -1` rechaza la carga antes de publicar y tamaño cero sigue válido.
- **G38 — array count ignora TryParse: `TSH-01011`.**
  - Productor: `TypeTable.Intern` self-host.
  - Causa: ignora el bool de `Number.TryParseInt64`; count inválido queda cero y publica Array.
  - Consumidores: Sema/CodeGen self-host y metadata de tipos.
  - Puntual: `SelfHostTypesInterningModule.ns`; pasó como KPD y Required y exige identidad/repetición estable.
  - Estado: `CERRADO` en A11; el count se valida antes de internar el elemento o publicar Array.
- **G39 — predicados indexan antes de validar ID: `TSH-01023`.**
  - Productores: `TypeTable.IsInteger` e `IsNumeric` self-host.
  - Causa: llaman `Get(id)` sin comprobar `0 <= id < Count()`.
  - Consumidores: conversiones numéricas de Sema y CodeGen self-host.
  - Puntual: `SelfHostTypesFunctionModule.ns`; pasó como KPD y Required para `-1` y `Count()`.
  - Estado: `CERRADO` en A11; ambos predicados rechazan IDs inválidos antes de indexar.
- **G40 — helper runtime publica desde path inválido: `T12B-00021`, `T12B-00022`.**
  - Productor: `cmake/NativeSharpSelfHostRuntime.cmake::nsc_resolve_selfhost_runtime`.
  - Causa: path NSLink vacío o malformado se usa para derivar library/ABI sin precondición estructural.
  - Consumidores: cuatro verificadores CMake de runtime/self-host/fixed-point.
  - Puntuales: `BuildSystemRuntimeProbe.cmake`; ambos casos pasaron como KPD y Required; resolución derivada y
    overrides explícitos preservaron precedencia.
  - Estado: `CERRADO` en A12; el helper valida productor no vacío y path absoluto antes de calcular salidas.
- **G41 — deploy no expone contrato de inputs: `T12B-00457`, `T12B-00458`, `T12B-00460`, `T12B-00461`.**
  - Productor: `deploy.bat`.
  - Causa: usa build/prefix hardcoded; no puede preservar paths ni validar destino/root/solape.
  - Consumidores: instalación manual y superficie Packaging.
  - Puntuales: `run_packaging_gate.py` con `cmake.cmd` espía y sandbox; los cuatro casos pasaron como KPD y
    Required; ausencia de CMake y repetición conservaron los guards del contrato.
  - Estado: `CERRADO` en A13; el wrapper exige build/prefix explícitos, normaliza, rechaza raíz/igualdad y sólo
    después resuelve e invoca CMake.

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

- Estado de Parte A: `CERTIFICADA_CICLO_2`; A00-A13 conservan sus cierres puntuales, los 52 KPD permanecen
  reconciliados en `PASS` y la repetición final vinculante cerró limpia: run
  `20261012-0428-part-a-final-cycle2`, revisión `69dd18bd16d151224e1a12dd9f54a94a9251c3ec`, 5,714/5,714
  terminales `PASS`, cero cancelaciones y cero fallos de preparación. La puerta funcional
  `CERTIFICACION_FINAL_OK` queda cerrada.
- Inventario causal: 52 demostrados; no quedan casos por reconciliar en el inventario previo.
- Productores/consumidores: 52 trazados en 41 familias.
- Pruebas puntuales: A00-A13 cerradas; cada tanda conserva su consolidado exacto Required `PASS`.
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
- Evidencia G36: `TSH-00214` reprodujo `severity=error` (16.655 ms), pasó corregido como KPD (41.760 ms) y
  Required (17.641 ms); `TSH-00213` preservó el escape JSON (14.829 ms).
- Consolidado A08 Required: `TSH-00213` y `TSH-00214`, 2/2 `PASS`, 27.821 ms de pared.
- Evidencia G05 self-host: `TSH-00442` reprodujo el KPD con exit 44 (36.738 ms), pasó corregido como KPD
  (44.239 ms) y Required (17.087 ms); `TSH-00443` preservó las variantes (15.101 ms).
- Consolidado A09 Required: una invocación `TSH-00442`, 1/1 `PASS`, 12.463 ms de pared.
- Evidencia G37: `TSH-00903` reprodujo exit 72 (37.255 ms), pasó corregido como KPD (41.944 ms) y Required
  (17.235 ms); `TSH-00901` y `TSH-00902` preservaron archivo vacío y contenido (13.259/14.524 ms).
- Consolidado A10 Required: una invocación `TSH-00903`, 1/1 `PASS`, 12.873 ms de pared.
- Evidencia G38: `TSH-01011` reprodujo exit 31 (42.690 ms), pasó corregido como KPD (41.556 ms) y Required
  (17.715 ms); `TSH-01009` y `TSH-01012` preservaron arrays válidos (15.103/12.883 ms).
- Evidencia G39: `TSH-01023` reprodujo exit 132 (17.733 ms), pasó corregido como KPD (44.400 ms) y Required
  (19.824 ms); `TSH-01026` y `TSH-01022` preservaron predicados/firma válidos (14.917/12.764 ms).
- Consolidado A11 Required: `TSH-01011` y `TSH-01023`, 2/2 `PASS`, 29.451 ms de pared.
- Evidencia G40: `T12B-00021` y `T12B-00022` reprodujeron publicación inválida con exit 0; tras la corrección
  ambos pasaron como KPD y Required con exit 1 y sin `runtime.txt`. Los guards de ruta absoluta derivada y
  overrides explícitos terminaron exit 0 y conservaron sus rutas.
- Consolidado A12 Required: `T12B-00021` y `T12B-00022`, 2/2 `PASS`.
- Evidencia G41: los cuatro casos reprodujeron que el wrapper ignoraba al caller y publicaba el comando fijo.
  Tras la corrección, `T12B-00457` y `T12B-00458` preservaron build/prefix citados; `T12B-00460` y
  `T12B-00461` rechazaron raíz e igualdad sin crear el log espía. Los guards de CMake ausente y repetición pasaron.
- Consolidado A13 Required: `T12B-00457`, `T12B-00458`, `T12B-00460` y `T12B-00461`, 4/4 `PASS`.
- Primera certificación final: preparación completa con los tres productores requeridos en `PASS`; ejecución
  5,714/5,714; 5,713 `PASS`, un `FAIL` (`TP-PAR-478|Debug|NA`) y cero cancelaciones. Evidencia canónica:
  `_nscbuilder3/history/20261012-033650-part-a-final/` (`results.json`, `events.jsonl`, `full.log`, `summary.md`).
- Próxima acción de Parte A: recorrer A00 -> A13 contra esa evidencia, reconciliar los 52 IDs cerrados, aislar y
  corregir causalmente `TP-PAR-478`, ejecutar el consolidado dirigido de 52 + `TP-PAR-478` y sólo entonces lanzar
  una nueva batería final completa.

### A.5.6 Matriz de reapertura A00 -> A13, ciclo 2

Autoridad: `_nscbuilder3/history/20261012-033650-part-a-final/results.json`. La reconciliación usa la identidad
terminal exacta `case_id|Debug|optimization`; no sustituye una ausencia por evidencia histórica ni vuelve a ejecutar
los casos que ya tienen terminal en la batería. Los 52 IDs aparecen una sola vez, 52 tienen resultado y los 52 son
`PASS`; no hay ausencias, duplicados ni estados no `PASS` dentro del inventario KPD cerrado.

| Tanda | IDs reconciliados en orden del maestro | Resultado de batería | Auditoría ciclo 2 |
|---|---|---:|---|
| A00 | `TL-LANG-019`, `TL-LANG-035`, `TL-LANG-099`, `TL-LANG-121`, `TL-LANG-230`, `TL-LANG-250`, `TL-LANG-262` | 7/7 `PASS` | cierre preservado |
| A01 | `TL-LEX-075` | 1/1 `PASS` | cierre preservado |
| A02 | `TA-AST-014`, `TA-AST-016`, `TA-AST-215` | 3/3 `PASS` | cierre preservado |
| A03 | `TP-PAR-063`, `TP-PAR-069`, `TP-PAR-103`, `TP-PAR-306`, `TP-PAR-309`, `TP-PAR-316`, `TP-PAR-327`, `TP-PAR-334`, `TP-PAR-355`, `TP-PAR-356`, `TP-PAR-358`, `TP-PAR-400`, `TP-PAR-405`, `TP-PAR-406`, `TP-PAR-422`, `TP-PAR-431`, `TP-PAR-450`, `TP-PAR-476`, `TP-PAR-511`, `TP-PAR-514`, `TP-PAR-524` | 21/21 `PASS` | cierre preservado |
| A04 | `TS-SEMA-106`, `TS-SEMA-110`, `TS-SEMA-246`, `TS-SEMA-265`, `TS-SEMA-317` | 5/5 `PASS` | cierre preservado |
| A05 | `TF-CFG-076` | 1/1 `PASS` | cierre preservado |
| A06 | `TF-DOM-142` | 1/1 `PASS` | cierre preservado |
| A07 | `TF-LANGUAGE-SERVICE-304` | 1/1 `PASS` | cierre preservado |
| A08 | `TSH-00213`, `TSH-00214` | 2/2 `PASS` | cierre preservado |
| A09 | `TSH-00442` | 1/1 `PASS` | cierre preservado |
| A10 | `TSH-00903` | 1/1 `PASS` | cierre preservado |
| A11 | `TSH-01011`, `TSH-01023` | 2/2 `PASS` | cierre preservado |
| A12 | `T12B-00021`, `T12B-00022` | 2/2 `PASS` | cierre preservado |
| A13 | `T12B-00457`, `T12B-00458`, `T12B-00460`, `T12B-00461` | 4/4 `PASS` | cierre preservado |
| **Total** | **52 IDs únicos** | **52/52 `PASS`** | **A00-A13 reconciliadas** |

El único terminal no `PASS` de la batería es `TP-PAR-478|Debug|NA`; no pertenece a los 52 KPD iniciales, pero
sí pertenece al denominador ejecutable y bloquea la certificación global. Se abre como hallazgo de cierre del ciclo 2
sin reclasificar ni invalidar artificialmente los 52 resultados demostrados.

#### Hallazgo de cierre TP-PAR-478

- Clasificación causal: defecto de prueba/oráculo y metadata, no defecto de producto. `parseArgument()` consume una
  `AssignmentExpression` y la gramática vigente representa `..items` como `RangeExpression` con inicio omitido,
  operador `DotDot` y extremo final `items`; no existe un operador spread de llamada separado en 3.0.3. El spread de
  colección general permanece planificado en 3.0.4 (`NS304-T09-F05`) y no autoriza reinterpretar un rango válido.
- Corrección: el caso conserva named/by-reference/move y reemplaza la expectativa de diagnóstico por validación
  estructural exacta del rango abierto, incluido el extremo inicial nulo, el extremo final y la ausencia de diagnósticos.
  Manifest, plan e inventario describen de forma coincidente `named/ref/move/range args` y el mismo contrato
  `CP-PAR-039`.
- Validación puntual post-corrección: `TP-PAR-476`, `TP-PAR-477`, `TP-PAR-478` y `TP-PAR-479`, 4/4 `PASS`, contra
  `build-compiler-suite-v3/debug/nsc_component_tests.exe` después de recompilar únicamente
  `ParserPostfixExpressionModule.cpp` y relinkar el productor mediante la arista incremental existente.
- Consolidado dirigido de reapertura: run `20261012-part-a-cycle2-directed-53`, revisión
  `bc9e2441c631a08d9a1a7b943a163d3985387e22`, preparación completa, 53/53 terminales `PASS`, cero cancelaciones y
  cero fallos de preparación. Evidencia: `_nscbuilder3/history/20261012-part-a-cycle2-directed-53/`. La selección
  contiene exactamente los 52 IDs iniciales más `TP-PAR-478`, sin duplicados.
- Certificación final del ciclo 2: run `20261012-0428-part-a-final-cycle2`, 5,714/5,714 terminales `PASS`, cero
  estados alternos, cero cancelaciones y preparación de los tres productores completa. Evidencia canónica:
  `_nscbuilder3/history/20261012-0428-part-a-final-cycle2/` (`metadata.json`, `events.jsonl`, `results.json`,
  `full.log`, `summary.md`).
- Estado de puerta: `CERTIFICACION_FINAL_OK` cerrada para Parte A. La rama `v.3.0.3` preexiste y apunta a
  `7b7e36faf51fe4712940cc76c18a4598fb94facd` (`Relax C23 view exception contract`, 2026-10-08), no a la
  frontera certificada actual. La política prohíbe mover una frontera existente sin instrucción explícita; por ello
  la rama no se modifica y la transición operativa a Parte B permanece bloqueada por esta contradicción de milestone.

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
| T00 | Auditoría ejecutable y congelamiento de contratos | 8 | 8 | 100 % |
| T01 | Inicialización estática y declaraciones en condiciones | 8 | 8 | 100 % |
| T02 | Cuerpos default de traits e interfaces | 6 | 6 | 100 % |
| T03 | List, slice y property patterns | 8 | 2 | 25 % |
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
| **Total** |  | **147** | **24** | **16.33 %** |

## B.2 Inventario exacto T00-T17

### T00 — auditoría ejecutable y congelamiento de contratos

- [x] `NS304-T00-F01` — Inventariar cada feature declarada `TERMINADA` y localizar su vertical real.
- [x] `NS304-T00-F02` — Clasificar evidencia por parser, semántica, MIR, LLVM, NSMETA, runtime y ejecución.
- [x] `NS304-T00-F03` — Construir matriz C# 8-14: equivalente, alternativa, divergencia o gap obligatorio.
- [x] `NS304-T00-F04` — Construir matriz Beef: memoria, extensions, generics, comptime, interop y patterns.
- [x] `NS304-T00-F05` — Añadir disposición `PlannedFor304` sin presentar features pendientes como productivas.
- [x] `NS304-T00-F06` — Congelar versiones previstas de AST, HIR, MIR, NSMETA, ABI runtime y debug metadata.
- [x] `NS304-T00-F07` — Publicar manifest de dependencias compiler/runtime/STL por feature.
- [x] `NS304-T00-F08` — Crear harness v3.0.4 sin conteos fijos, hashes de herramientas ni textos frágiles.

### T01 — inicialización estática y declaraciones en condiciones

- [x] `NS304-T01-F01` — Definir estados `uninitialized`, `initializing`, `initialized` y `failed`.
- [x] `NS304-T01-F02` — Implementar gramática y semántica de constructores estáticos por tipo.
- [x] `NS304-T01-F03` — Implementar inicializadores de módulo con orden y dependencias explícitos.
- [x] `NS304-T01-F04` — Implementar once thread-safe, espera, reentrancia y detección de ciclos.
- [x] `NS304-T01-F05` — Hacer terminal el fallo de inicialización, sin retry; panic sigue el perfil del proceso.
- [x] `NS304-T01-F06` — Implementar locales `static`, guard, lifetime y destrucción al descargar módulo.
- [x] `NS304-T01-F07` — Implementar declaraciones en `if`, `while` y `switch` con scope y cleanup por arista.
- [x] `NS304-T01-F08` — Versionar metadata, emitir debug info y probar EXE, DLL, concurrencia y recursión.

### T02 — cuerpos default de traits e interfaces

- [x] `NS304-T02-F01` — Aceptar cuerpos default en métodos, propiedades e indexadores.
- [x] `NS304-T02-F02` — Fijar prioridad: implementación explícita, miembro nominal, default más específico.
- [x] `NS304-T02-F03` — Resolver diamantes, overrides, traits sellados, coherence y conflictos intermodulares.
- [x] `NS304-T02-F04` — Integrar genéricos, associated types, constraints, `some Trait` y `dyn Trait`.
- [x] `NS304-T02-F05` — Transportar cuerpos y witnesses versionados por NSMETA sin requerir fuentes.
- [x] `NS304-T02-F06` — Cerrar vtables, ABI DLL, evolución binaria, MIR, LLVM y debug de thunks.

### T03 — list, slice y property patterns

- [x] `NS304-T03-F01` — Implementar list patterns exactos, prefijo, sufijo y elemento descartado.
- [x] `NS304-T03-F02` — Implementar rest/slice capture sin copia implícita y con lifetime del origen.
- [ ] `NS304-T03-F03` — Soportar arrays, inline arrays, slices, spans, strings UTF-8 y protocolo usuario. *(4/6 sustratos listos: slices, spans, strings→StringView y protocolo usuario. Rest sobre array/inline→Span/ReadOnlySpan pendiente: la clave cerrada es correcta pero ensureTypeSpecialization no materializa el tipo desde cuerpos de metodo (no-const + fase serial); se diagnostica error claro.)*
- [x] `NS304-T03-F04` — Completar property patterns anidados, posicionales, relacionales y lógicos. *(relacionales, lógicos y anidados; posicional nominal/unario con condición real, validación de tipo, `Deconstruct` y aridad. Evidencia puntual 2026-10-13: TP-PAR-357 PASS, 13,545 us; TL-LANG-322 PASS, 802,361 us.)*
- [x] `NS304-T03-F05` — Extender usefulness/exhaustiveness para longitud, OR-pattern y dominios abiertos. *(Unión canónica de dominios exactos/rest preservada por brazo OR; redundancia sólo cuando todas sus alternativas ya están cubiertas. Evidencia puntual 2026-10-13: TL-LANG-267 PASS, 196 us; TL-LANG-312 PASS, 468,431 us.)*
- [x] `NS304-T03-F06` — Definir bindings `copy`, `move`, `ref` y `ref mut` dentro de cada pattern. *(modos + TL-LANG-313)*
- [x] `NS304-T03-F07` — Bajar a length checks, branches y borrows sin colección ni boxing temporal. *(satisfecho por F01–F06; cubierto por TL-LANG-308/310)*
- [x] `NS304-T03-F08` — Añadir diagnósticos de casos faltantes, redundancia, escape y move inválido. *(faltantes+redundancia de list y variante; escape por TL-LANG-309)*

### T04 — lambdas, callables y funciones locales

- [x] `NS304-T04-F01` — Inferir tipo callable natural para lambda y method group cuando sea único. *(lambda natural TL-LANG-314 + method group natural único TL-LANG-319.)*
- [x] `NS304-T04-F02` — Permitir retorno explícito, atributos, calling convention y efectos en lambdas. *(retorno explícito, efectos noheap/unsafe, `extern C`, y atributos `[Attr]` desambiguados de la captura (TL-LANG-327).)*
- [x] `NS304-T04-F03` — Permitir `ref`, `ref readonly`, `in`, `out` y `scoped` sin repetir tipos inferibles. *(ref/out/in validados TL-LANG-316; ref readonly y scoped son GAP de T11-F07.)*
- [x] `NS304-T04-F04` — Implementar parámetros opcionales y defaults consteval en lambdas. *(call-site default application en delegate + TL-LANG-317.)*
- [x] `NS304-T04-F05` — Completar lambdas y funciones locales estáticas y prohibición de captura. *(2026-10-13: `KwStatic` estructurado y preservado por clones; Sema ya no busca texto auxiliar. TL-LANG-318/351/352/355 + guard TL-LANG-320 PASS; TL-LANG-355 exige fallo y diagnóstico exacto.)*
- [x] `NS304-T04-F06` — Implementar funciones locales genéricas, recursión y autoreferencia segura. *(funcion local + recursion Fact(5)=120 (TL-LANG-325/326) + generic local con retorno genérico `T Identity<T>(T x)` (TL-LANG-352; looksLikeLocalFunction acepta identificador como tipo).)*
- [x] `NS304-T04-F07` — Congelar ranking de method groups, overload priority y conversiones a `fn*`/`fn&`. *(tipo natural de method group unico + TL-LANG-319.)*
- [x] `NS304-T04-F08` — Cerrar layout, drop, escape, PDB y ejecución de closures combinadas con errors. *(layout+drop+ejecucion TL-LANG-320 y closure+throws Exception TL-LANG-321.)*

### T05 — async/await productivo

- [x] `NS304-T05-F01` — Retirar rechazo `NS3049` para `async`/`await` y completar AST/recovery. *(await→AwaitExpression; async consumido sin NS3049 + marcador en function global y en class/interface method (TL-LANG-328).)*
- [x] `NS304-T05-F02` — Integrar `Future` lazy, `Poll`, `Waker`, resultado, error y cancelación. *(modelo completo + wakerType; surface/lowering T05-F03.)*
- [ ] `NS304-T05-F03` — Implementar transformación productiva a state machine HIR/MIR.
- [ ] `NS304-T05-F04` — Calcular frame layout, discriminante, liveness y storage de locals suspendidos.
- [ ] `NS304-T05-F05` — Verificar borrows, moves, pinning, self-references y escapes entre suspensiones.
- [ ] `NS304-T05-F06` — Ejecutar drop/defer/finally una vez en completion, error o cancelación.
- [ ] `NS304-T05-F07` — Integrar `throws E`, `Result`, `?`, panic y unwind nativo con async.
- [ ] `NS304-T05-F08` — Permitir storage en stack, caller, arena o allocator explícito; nunca heap obligatorio.
- [ ] `NS304-T05-F09` — Emitir ABI/NSMETA de future opaco, poll/resume/drop y consumo intermodular.
- [ ] `NS304-T05-F10` — Emitir LLVM y debug metadata de state, locals, sequence points y logical frames.

### T06 — generadores síncronos

- [x] `NS304-T06-F01` — Retirar rechazo `NS3049` para `yield return` y `yield break`. *(AST YieldReturnStatement/YieldBreakStatement; lowering T06-F03.)*
- [ ] `NS304-T06-F02` — Implementar AST, semántica y restricciones de contexto de generator. *(yield reporta diagnostic claro de lowering T06-F03 (TL-LANG-329). Semantica/contexto completo pendiente.)*
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

- [x] `NS304-T08-F01` — Diseñar bloques `extension` canónicos sobre tipos y constraints. *(extension method en namespace + TL-LANG-323; global-namespace residual.)*
- [x] `NS304-T08-F02` — Implementar extension properties con getter/setter/ref getter. *(getter (TL-LANG-324) + setter método (TL-LANG-330) + setter asignación `21.Doubled = x` (rama de asignación→setter vía namespace call); ref getter residual.)*
- [ ] `NS304-T08-F03` — Implementar métodos, propiedades y constantes estáticos de extensión. *(parser: `static` dentro de extension no recibe receptor (ns3-extension-static) + TP-PAR-538; lookup Sema de `T.Miembro()` residual.)*
- [ ] `NS304-T08-F04` — Implementar operadores de extensión mediante trait witnesses estáticos.
- [ ] `NS304-T08-F05` — Permitir conformance externa a traits bajo orphan/coherence rules.
- [ ] `NS304-T08-F06` — Congelar imports, visibilidad, precedencia, ambigüedad y lookup incremental.
- [ ] `NS304-T08-F07` — Versionar NSMETA y probar consumidor binario, genéricos y cero layout mutation.

### T09 — construcción, propiedades y eventos

- [x] `NS304-T09-F01` — Implementar miembros `required` y definite initialization. *(parser `required` + flag isRequired + dataflow por camino en OwnershipFlowSnapshot + intersección de salidas normales por constructor + TL-LANG-335 PASS, 2026-10-13, 1.024 s.)*
- [x] `NS304-T09-F02` — Implementar setters `init` y congelamiento posterior a construcción. *(parser `init` + marcador `:init` + flag isInit + restricción en constructor + TL-LANG-334; congelamiento efectivo vía backing privado.)*
- [x] `NS304-T09-F03` — Implementar backing field sintetizado con layout visible. *(backing `__nsc_backing_<name>$stage38` determinista + layout de campo regular; ya satisfecho por síntesis existente.)*
- [ ] `NS304-T09-F04` — Extender primary constructors con orden de inicialización normativo. *(struct/record + class primary ctor promote-to-fields + orden textual + TL-LANG-337/344; semántica params-in-scope C# 12 residual.)*
- [ ] `NS304-T09-F05` — Completar collection expressions, spreads y builder protocol sin heap implícito. *(AST + parser + target-typed/var/tipo común; spreads ordenados HIR/MIR/LLVM con copia y evaluación única, TL-LANG-349 PASS 2026-10-13; single-spread materializa almacenamiento independiente, TL-LANG-354 PASS 2026-10-13 con mutación aislada bidireccional y una evaluación del origen; builder protocol permanece abierto.)*
- [ ] `NS304-T09-F06` — Implementar inline arrays/fixed buffers seguros con length const genérico. *(fixed array T[n] con length literal + TL-LANG-308/309/310; length const genérico residual (sistema de tipos).)*
- [x] `NS304-T09-F07` — Implementar target-typed construction sin alterar overload selection. *(new() y new(args) vía activeTargetTypeKey_ + construction model target-typed + TL-LANG-339; desambiguación de placement por lookahead.)*
- [x] `NS304-T09-F08` — Definir eventos nativos sobre handler/storage explícitos. *(handler explícito + add/remove + invoke + accessors custom add_/remove_ (parser + InstanceCall) + TL-LANG-338/341.)*
- [ ] `NS304-T09-F09` — Integrar fallo parcial, drop order, inheritance, records, with y debug metadata.
- [ ] `NS304-T09-F10` — Implementar constructores fallibles `throws E`, propagación, cleanup parcial, ABI,
  NSMETA, MIR unwind, allocators, PDB y self-host.

### T10 — strings, null-awareness y nombres estáticos

- [ ] `NS304-T10-F01` — Implementar literales UTF-8 tipados y forma allocation-free. *(escapes UTF-8 vía T10-F08 (`\u`/`\U` + noheap); tipo UTF-8 tipado `u8"..."` residual.)*
- [x] `NS304-T10-F02` — Completar raw interpolated strings y delimitadores anidados. *(lexRawInterpolatedString con delimitador dinámico 3+ comillas, contenido crudo + huecos; TL-LEX-111 + TL-LANG-332.)*
- [ ] `NS304-T10-F03` — Hacer productivos interpolation handlers con stack/writer/custom sink.
- [x] `NS304-T10-F04` — Validar formato consteval e interpolación constante pura. *(handler estático T01-F06 + TL-LANG-336 literal hole.)*
- [ ] `NS304-T10-F05` — Implementar `?.` y `?[]` a la izquierda con evaluación condicional del RHS. *(?.Field = value + compound + token `?[` + READ/WRITE de array nativo (`arr?[i]`/`arr?[i]=v`) via NullConditionalArrayLoad/Store; receptor único, índice/RHS sólo non-null y PHI del resultado, TL-LANG-340/342/345 PASS 2026-10-13; indexador de usuario residual.)*
- [ ] `NS304-T10-F06` — Integrar null-conditional compound assignment sin `++`/`--` ambiguos. *(numérico + string concat (`obj?.Name += "x"`) via operation + applyBinaryOperation/concat; devuelve valor almacenado y libera string desplazado sólo en destino owned, TL-LANG-342/346 PASS 2026-10-13; ++/-- residual.)*
- [x] `NS304-T10-F07` — Implementar `nameof` para símbolos, miembros, aliases y genéricos no ligados. *(AST conserva operando; GenericModel/NameLookup/Sema resuelven sin evaluar; HIR emite la constante sólo tras éxito. Evidencia focal 2026-10-13: TA-AST-025 y TL-LANG-331/350/353/356 PASS, con negativos de símbolo inexistente, inaccesibilidad y aridad.)*
- [ ] `NS304-T10-F08` — Añadir `\e`, diagnósticos Unicode/UTF-8 y pruebas `noheap`/`no-culture`. *(escape `\e` + `\u`/`\U` + prueba noheap TL-LANG-333; no-culture residual bloqueado en la feature.)*

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

## B.4 Reconciliación causal previa B00

`AUDITORÍA COMPLETA; 147/147 RECONCILIADOS; 0/147 CERRADOS; SIN CAMBIOS DE PRODUCTO`.

La orden del 12 de octubre de 2026 activa la Parte B en `development` pese a la
contradicción documental de la rama preexistente `v.3.0.3`. Esa rama no se
mueve ni se usa como autoridad de código. La revisión compara los 147 IDs del
plan fuente con el árbol y la evidencia actuales; no convierte diseño,
helpers, parsing aislado ni pruebas internas en implementación terminada.

Fuentes congeladas para este ciclo:

- lineamientos: 5,110 líneas, SHA-256
  `DE896530ED5D0F4DDB1D4DAA42635F921563C7C764060CEB2D781629C77C3D4F`;
- plan 3.0.4: 606 líneas, SHA-256
  `7BD3EB2EA0779F6B5EC4D8BCA0B705C6391F031E9786F7997BC6316C6A86757F`;
- tracking previo a B00: 1,661 líneas, SHA-256
  `09331F206E96435A19EEA4C1B76C4AB9808735AC307D951C9471214A32C72262`.

La extracción mecánica confirma 147 IDs únicos y la distribución exacta
`8/8/6/8/8/10/8/8/7/10/8/8/6/9/8/8/8/11` para T00-T17. El inventario del
plan y B.2 coinciden sin IDs faltantes, adicionales ni duplicados.

| Tanda / IDs reconciliados | Evidencia actual aprovechable | Brecha que impide cierre | Cerrados |
|---|---|---|---:|
| T00 / F01-F08 | Plan, tracking, `Language3Surface` y suite 3.0 existentes | faltan matrices C#/Beef ejecutables, `PlannedFor304`, versiones, manifest y harness 3.0.4 | 0/8 |
| T01 / F01-F08 | lookahead y lowering parcial de declaración condicional en `Parser.cpp`; TP-PAR-027/334 | constructores/locales static siguen `DeferredTo4`/`NS3049`; no hay once, fallo terminal, lifetime ni metadata vertical | 0/8 |
| T02 / F01-F06 | modelos de traits, witnesses y dispatch existentes | cuerpos default se rechazan con `NS3049`; faltan prioridad, diamantes, NSMETA de cuerpos y ABI de thunks | 0/6 |
| T03 / F01-F08 | match, patrones relacionales/lógicos y ownership son bases reutilizables | list patterns siguen `DeferredTo4`/`NS3049`; no hay captura slice, usefulness de longitud ni lowering productivo | 0/8 |
| T04 / F01-F08 | `LambdaExpression`, `CallableSemantics` y pruebas de lambdas normales/genéricas | falta demostrar toda la superficie ref/scoped/default/static, layout/drop/PDB y ejecución combinada | 0/8 |
| T05 / F01-F10 | `CoroutineSemantics` modela future/frame como helper | parser conserva múltiples rechazos `NS3049`; no hay state machine productiva HIR/MIR/LLVM ni ABI/debug intermodular | 0/10 |
| T06 / F01-F08 | `IterationSemantics` y contratos de foreach son bases | `yield` se rechaza con `NS3049`; faltan frame, suspensión, pinning, cleanup y ABI ejecutable | 0/8 |
| T07 / F01-F08 | modelos de concurrencia/coroutine y pruebas internas | async streams siguen diferidos; no hay máquina await+yield, backpressure, task scope ni metadata productiva | 0/8 |
| T08 / F01-F07 | extension methods y trait operators figuran `Implemented`; `ExtensionDeclaration` baja a funciones de namespace | el ítem exige bloques, properties, estáticos, operadores, coherence y NSMETA como una vertical más amplia aún no certificada | 0/7 |
| T09 / F01-F10 | primary constructors figuran `Implemented`; existen spreads, events, construcción y placement | no hay keywords `required`/`init`; faltan collection builder, inline arrays y cierre conjunto de fallo parcial/ABI/PDB/self-host | 0/10 |
| T10 / F01-F08 | raw strings figuran `Implemented`; existen interpolación y null-aware operators | faltan UTF-8 tipado/handlers completos, `nameof` productivo y certificación noheap/no-culture | 0/8 |
| T11 / F01-F08 | Span/ReadOnlySpan, params y escape analysis tienen verticales actuales | faltan todas las reglas ref-like genéricas/traits, ref fields/scoped intermodular y atributos genéricos cerrados | 0/8 |
| T12 / F01-F06 | `PartialTypeAggregator` resuelve tipos parciales | no existen verticales completas de métodos/properties/indexers/constructores/events parciales ni generación integrada | 0/6 |
| T13 / F01-F09 | placement/in-place y modelos de memoria proveen cimientos | no hay protocolo allocator congelado ni selección/arena/trailing/provenance/cost report como contrato productivo | 0/9 |
| T14 / F01-F08 | `ComptimeEngine` y `MacroExpansionEngine` son helpers reales | no existe generación tipada higiénica de declaraciones con source mapping, capabilities y derives ejecutables | 0/8 |
| T15 / F01-F08 | tipos enteros/FP y numeric contracts son reutilizables | `decimal` se rechaza con `NS3049` y sigue `DeferredTo4`; faltan los ocho estratos completos | 0/8 |
| T16 / F01-F08 | HIR, MIR, NSMETA, ABI, PDB y self-host poseen infraestructura general | integración depende de todas las verticales anteriores y no puede cerrar ninguna brecha funcional | 0/8 |
| T17 / F01-F11 | la suite O0 y NSLink de 3.0.3 quedaron certificados | no existen matrices/fixtures/candidato/baseline 3.0.4; T17 no implementará features nuevas | 0/11 |
| **Total** | **147 IDs inspeccionados** | **todo cierre requiere evidencia nueva por ítem** | **0/147** |

Autoridades de código principales: `include/nativesharp/Language/Language3Surface.h`,
`src/Language/Language3Surface.cpp`, `include/nativesharp/AST/AstNodeKind.h`,
`include/nativesharp/Lexer/TokenKind.h`, `src/Parser/Parser.cpp`,
`src/Language/CoroutineSemantics.cpp`, `src/Language/IterationSemantics.cpp`,
`src/Frontend/ProjectNamespaceFunctionModel.cpp`,
`src/Compilation/PartialTypeAggregator.cpp`, `src/Frontend/ComptimeEngine.cpp` y
`src/Frontend/MacroExpansionEngine.cpp`. La suite actual prueba varios cimientos,
pero todavía no existe `docs/3.0.4/items/` ni un harness v3.0.4 que pueda cerrar
los contratos del plan.

Orden operativo fijado:

1. B01 ejecuta T00 F01-F08 en orden y congela autoridad, disposiciones,
   versiones, dependencias y harness antes de tocar runtime o bibliotecas.
2. Después se respetan las ramas causales T01; T02; T03; T04 -> T05/T06 ->
   T07; T08 -> T09 -> T12; T10 -> T11; T13; T14; T15.
3. T16 integra únicamente verticales ya ejecutables y T17 certifica; ninguno
   se usa para ocultar un gap previo.
4. Cada ítem exige Markdown propio, corrección mínima, prueba puntual y commit;
   cada tanda recibe consolidado exacto, sin batería completa intermedia.
5. La batería completa O0 se ejecuta una vez después de T17-F11. Un fallo del
   alcance reabre la auditoría desde T00 conforme a la regla operativa global.

## B.5 Ejecución B01 — T00

### NS304-T00-F01 — inventario de features declaradas `TERMINADA`

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T00-F01-completed-feature-inventory.md`.
- Inventario canónico: 55 IDs únicos, distribuidos T00=6, T01=7, T02=6,
  T03=6, T04=7, T05=7, T06=7, T07=6 y T08=3.
- Trazabilidad: 55/55 expedientes presentes; 55/55 commits resolubles y
  ancestros de la revisión base; 55/55 filas localizadas.
- Resultado actual: 54 verticales conservan anclas vigentes para la auditoría
  por capas de F02; `NS3-T00-F01` quedó clasificada como
  `IMPLEMENTACIÓN RETIRADA` por `0bea623fb` y ya no se presenta como productiva.
- Gate: omitidos=0, desconocidos=0, contradicciones_sin_registrar=0.
- Prueba puntual: extracción determinista, unicidad, existencia de expedientes,
  resolución/ancestría de commits y comprobación de rutas; no se ejecutó build,
  batería ni prueba funcional.
- Siguiente ítem: `NS304-T00-F02`.

### NS304-T00-F02 — matriz de evidencia por capas

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T00-F02-evidence-layer-matrix.md`.
- Cobertura: 55/55 filas clasificadas en parser/AST, semántica, MIR, LLVM,
  NSMETA, runtime y ejecución.
- Separación de evidencia: `A` localiza archivo actual; `D` conserva sólo la
  afirmación histórica; `T/H` distingue artefacto vigente de resultado histórico;
  `R` representa implementación retirada; `—` conserva ausencia de evidencia.
- Resultado: ninguna ejecución histórica se promovió a certificación actual y
  `NS3-T00-F01` permanece retirada en las siete dimensiones.
- Gate: omitidos=0, desconocidos=0, resultados_actuales_inferidos_de_historia=0.
- Prueba puntual: regeneración mecánica de 55 filas desde F01, expedientes y
  commits, más comprobación exacta de distribución por símbolo de evidencia.
- Siguiente ítem: `NS304-T00-F03`.

### NS304-T00-F03 — matriz C# 8.0–14

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T00-F03-csharp-8-14-matrix.md`.
- Cobertura: 88/88 capacidades estables oficiales, distribuidas 16/16/16/15/7/10/8
  para C# 8.0/9/10/11/12/13/14.
- Resultado: 15 `EQUIVALENTE`, 22 `ALTERNATIVA`, 4 `DIVERGENCIA` y
  47 `GAP_OBLIGATORIO`, todos estos últimos ligados a dueño NS304.
- Frontera: previews de C# 12/13 y tooling de aplicaciones de archivo quedan
  separados y no se presentan como núcleo estable omitido.
- Gate: omitidas=0, duplicadas=0, disposiciones_múltiples=0,
  gaps_sin_dueño=0, previews_mezcladas=0.
- Prueba puntual: parseo de 88 filas, distribución exacta por versión,
  vocabulario cerrado de cuatro disposiciones y vínculo NS304 en cada gap.
- Siguiente ítem: `NS304-T00-F04`.

### NS304-T00-F04 — matriz Beef por capacidad

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T00-F04-beef-capability-matrix.md`.
- Cobertura: 69/69 capacidades materiales de memoria, extensions, generics,
  comptime, interop y patterns, distribuidas 14/11/10/13/11/10.
- Resultado: 19 `EQUIVALENTE`, 8 `ALTERNATIVA`, 10 `DIVERGENCIA` y
  32 `GAP_OBLIGATORIO`, todos estos últimos ligados a dueño NS304.
- Frontera: la paridad se mide por capacidad observable; no se adopta sintaxis,
  heap/boxing implícito, mutación externa de layout/vtable ni mixins textuales.
- Gate: omitidas=0, duplicadas=0, disposiciones_múltiples=0,
  gaps_sin_dueño=0.
- Prueba puntual: parseo de 69 filas, distribución exacta por área,
  vocabulario cerrado de cuatro disposiciones y vínculo NS304 en cada gap.
- Siguiente ítem: `NS304-T00-F05`.

### NS304-T00-F05 — disposición `PlannedFor304`

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T00-F05-planned-disposition.md`.
- Contrato: `Implemented`, `PlannedFor304`, `DeferredTo4` y `RejectedFor3`
  conservan semánticas distintas y labels estables.
- Catálogo: 6 implementadas, 9 planeadas para 3.0.4, 0 diferidas a 4.0 y
  1 rechazada para 3.0.
- Frontera: una entrada `planned-3.0.4` continúa rechazada por el Parser y no
  cuenta como superficie productiva.
- Prueba puntual: reconstrucción enfocada del target existente, sin
  reconfiguración; `TL-LANG-155..161` = 7/7 `PASS`.
- Gate: entradas=16, planned_como_implemented=0, labels_vacíos=0,
  disposiciones_desconocidas=0.
- Siguiente ítem: `NS304-T00-F06`.

### NS304-T00-F06 — reservas de versión 3.0.4

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T00-F06-version-reservations.md`.
- Manifest: `docs/3.0.4/contracts/version-reservations.json`.
- Baseline: AST/HIR in-memory sin wire schema; MIR `1.30`; NSMETA `1.39`;
  runtime ABI `1.2`/symbol set `4`; debug gobernado por productor LLVM.
- Reserva: seis ventanas minor, una reclamación máxima por tanda T01–T15;
  ninguna versión se emite antes del cierre del ítem propietario.
- Gate: contratos=6/6, mismatch_baseline=0, owners=6x15,
  duplicados=0, bumps_productivos_prematuros=0.
- Prueba puntual: parseo JSON, contraste de constantes numéricas y validación
  exacta de owners, ventanas y prohibiciones.
- Siguiente ítem: `NS304-T00-F07`.

### NS304-T00-F07 — manifest de dependencias por feature

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T00-F07-feature-dependencies.md`.
- Manifest: `docs/3.0.4/contracts/feature-dependencies.json`.
- Cobertura: 18/18 familias del alcance, owners T01–T15 y productores del
  compilador; dependencia runtime/STL clasificada, no inferida como presente.
- Gate: IDs únicos=18, owners faltantes=0, componentes desconocidos=0,
  rutas existing ausentes=0, planned presentado como existing=0.
- Prueba puntual: parseo JSON, whitelist de componentes/modos, cierre de owners
  y existencia física de cada ruta STL marcada `existing`.
- Siguiente ítem: `NS304-T00-F08`.

### NS304-T00-F08 — harness estructural 3.0.4

Estado: `CERRADO`; consolidado T00 `PASS`.

- Expediente: `docs/3.0.4/items/NS304-T00-F08-plan-harness.md`.
- Implementación: `tools/verify_native_sharp_304_plan.py`.
- Pruebas: `tests/unit/test_verify_native_sharp_304_plan.py`.
- Contrato: universo descubierto desde plan/tracking; salida JSON con códigos
  estables; sin denominador fijo, hashes de herramientas ni oráculos de prosa.
- Prueba puntual: 4/4 unitarias `PASS`; corrida real `PASS`, 147/147 IDs,
  8 cerrados y 2 contratos descubiertos.
- Gate: missing=0, extra=0, order_mismatch=0, contract_issues=0.
- Consolidado T00: F01=55, F02=55, F03=88, F04=69, F05=7/7,
  F06=6, F07=18, F08=4/4 + gate real; `PASS`, sin batería completa.
- Siguiente acción: T01 habilitada; `NS304-T01-F01` cerrada y
  `NS304-T01-F02` activa.

## B.6 Ejecución B02 — T01

### NS304-T01-F01 — estados de inicialización estática

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T01-F01-static-initialization-states.md`.
- Contrato: `StaticInitializationState` es un dominio cerrado con los estados
  `uninitialized`, `initializing`, `initialized` y `failed`; F01 no adelanta
  transiciones, espera, reentrancia, ciclos ni política de fallo.
- Implementación: `include/nativesharp/Language/Language3Surface.h`.
- Prueba puntual: `TL-LANG-294` (`CL-LANG-012`, `Host.Language`) `PASS` en
  Debug/O0; valida ordinales, nombres exactos y rechazo nominal de valores
  externos mediante `unknown`.
- Integración canónica: una fila en manifest, diseño, allocation y plan
  Host.Language; productor `nsc_component_tests`.
- Gate: modelo y prueba dirigidos `PASS`; T01 queda 1/8 y Parte B 9/147, sin
  batería completa.
- Siguiente ítem: `NS304-T01-F02`, cerrado a continuación.

### NS304-T01-F02 — constructores estáticos por tipo

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T01-F02-static-constructors.md`.
- Contrato: `static Tipo()` sin accesibilidad, parámetros ni initializer;
  único por tipo, sin `this` y separado de constructores invocables.
- Implementación: Parser conserva `ConstructorDeclaration + KwStatic`;
  member/construction separan el overload set; HIR/MIR usan función `void`
  estática `__nsc_cctor_*` sin prólogo de instancia.
- Prueba puntual: `TL-LANG-295` `PASS` en 102329 microsegundos, positivo hasta
  MIR y cinco negativos exactos.
- Guard de frontera: `TL-LANG-160` `PASS`; el manifest conserva
  `static-constructors=planned-3.0.4` hasta cerrar triggers/once/runtime/F08.
- Gate: T01 queda 2/8 y Parte B 10/147, sin batería completa.
- Siguiente ítem: `NS304-T01-F03`.

### NS304-T01-F03 — inicializadores de módulo ordenados

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T01-F03-module-initialization-plan.md`.
- Contrato: un proyecto es el módulo de inicialización; `ProjectGraph` conserva
  el orden dependency-first y el plan publica sólo aristas directas explícitas.
- Implementación: `ProjectMemberModel` agrupa los constructores estáticos de
  cada módulo sin ejecutarlos durante el import; `ProjectCompilation` construye
  el plan después de miembros.
- Prueba puntual: `TL-LANG-296` `PASS` en 40707 microsegundos; Producer y
  Consumer conservan orden, dependencia y ownership exactos.
- Guards: `TL-LANG-295` `PASS` en 141701 microsegundos y `TL-LANG-160` `PASS`
  en 4306 microsegundos.
- Gate: T01 queda 3/8 y Parte B 11/147, sin batería completa.
- Siguiente ítem: `NS304-T01-F04`.

### NS304-T01-F04 — once thread-safe, espera, reentrancia y ciclos

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T01-F04-static-initialization-once.md`.
- Contrato: un coordinador privado por tipo reclama `0 -> 1`, ejecuta
  inicializadores y cuerpo explícito una sola vez, y publica `2`; otros hilos
  esperan y la reentrada del propietario evita auto-bloqueo.
- Implementación: HIR distingue `__nsc_cctor_body_*`; LLVM conserva
  `__nsc_cctor_*`, CAS/loads/stores `seq_cst`, owner thread, spin y triggers en
  accesos estáticos y construcción de instancia. `ProjectGraphLoader` rechaza
  ciclos de módulos antes del plan.
- Prueba puntual: `TL-LANG-297` `PASS` en 68613 microsegundos.
- Guards: `TL-LANG-296` `PASS` en 11406 microsegundos, `TL-LANG-295` `PASS` en
  45586 microsegundos y `TL-LANG-160` `PASS` en 1268 microsegundos.
- Guard LLVM: `nsc_stage37_static_initialization_tests` `PASS`, 61 checks.
- Gate: T01 queda 4/8 y Parte B 12/147, sin batería completa.
- Siguiente ítem: `NS304-T01-F05`.

### NS304-T01-F05 — fallo terminal sin retry

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T01-F05-static-initialization-failure.md`.
- Contrato: una salida excepcional publica `failed=3` antes de conservar el
  mismo unwind; `failed` es terminal y no vuelve al CAS. Una reconsulta termina
  por el `PanicProfile` del proyecto.
- Implementación: `Language3Surface` define terminalidad/retry;
  `ProjectOopLlvmLowerer` conecta llamadas potencialmente divergentes a un
  `cleanuppad` Win64, publica estado 3 atómicamente y separa estado 2/3 en fast
  path, resultado de CAS y espera.
- Prueba puntual: `TL-LANG-298` `PASS` en 427 microsegundos.
- Guards: `TL-LANG-297` `PASS` en 71138 microsegundos, `TL-LANG-294` `PASS` en
  7 microsegundos y `TL-LANG-160` `PASS` en 4239 microsegundos.
- Guard LLVM: `nsc_stage37_static_initialization_tests` `PASS`, 67 checks.
- Gate: T01 queda 5/8 y Parte B 13/147, sin batería completa.
- Siguiente ítem: `NS304-T01-F06`.

### NS304-T01-F06 — locales `static` y lifetime de módulo

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T01-F06-static-locals.md`.
- Contrato: storage por módulo, inicialización al primer alcance con guard
  0/1/2/3 y destrucción una vez al descargar el módulo; nunca al salir del
  scope. Referencias locales y VLA estáticos se rechazan causalmente.
- Implementación: `staticLocal` viaja Parser -> MethodLocal -> HIR -> MIR ->
  cache/dump; LLVM emite storage/state/owner privados, CAS/espera/reentrancia,
  fallo terminal y thunks registrados en `llvm.global_dtors`.
- Prueba puntual: `TL-LANG-299` `PASS` en 90358 microsegundos.
- Guards: `TL-LANG-297` `PASS` en 22235 microsegundos, `TL-LANG-298` `PASS` en
  7 microsegundos, `TL-LANG-160` `PASS` en 3885 microsegundos y `TL-LANG-294`
  `PASS` en 7 microsegundos.
- Guard LLVM: `nsc_stage37_static_initialization_tests` `PASS`, 72 checks.
- Catálogo: 5720 casos, 5720 ejecutables y 5720 filas de manifest.
- Gate: T01 queda 6/8 y Parte B 14/147, sin batería completa.
- Siguiente ítem: `NS304-T01-F07`.

### NS304-T01-F07 — declaraciones en condiciones

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T01-F07-condition-declarations.md`.
- Contrato: exactamente un local inicializado; `if`/`while` conservan condición
  `bool`, `switch` integral/enum, y el nombre no escapa de la sentencia controlada.
- Implementación: `if`/`switch` bajan a bloque declaración-control; `while` baja
  a `while(true)` con declaración por iteración, salida `!nombre` y cuerpo
  original. Los bloques existentes concentran scope y cleanup por arista.
- Prueba puntual: `TL-LANG-300` `PASS` en 90353 microsegundos.
- Guards: `TP-PAR-027` `PASS` en 4676 microsegundos, `TP-PAR-334` `PASS` en
  690 microsegundos, `TP-PAR-336` `PASS` en 300 microsegundos, `TP-PAR-340`
  `PASS` en 261 microsegundos, `TP-PAR-341` `PASS` en 349 microsegundos y
  `TL-LANG-160` `PASS` en 4196 microsegundos.
- Build focal: tres objetos y un relink con LLVM/Clang 23, sin regenerar CMake.
- Gate: T01 queda 7/8 y Parte B 15/147, sin batería completa.
- Siguiente ítem: `NS304-T01-F08`.

### NS304-T01-F08 — integración, metadata y debug

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T01-F08-integration.md`.
- Metadata: NSMETA 1.39 publica un catálogo aditivo de capacidades T01; el
  reader 1.39 valida bits desconocidos y la emisión deliberada 1.38 omite el
  catálogo sin inventar capacidades.
- Debug: NSDEBUG 1.1 versiona el contrato NativeSharp sobre CodeView/PDB; las
  emisiones `-g` de EXE y DLL produjeron PDB no vacíos.
- Producto: la partición coordinadora es la única que resuelve el cuerpo del
  constructor estático; las unidades ordinarias conservan sólo la declaración
  externa del coordinador y ya no dependen de funciones podadas.
- Prueba puntual: `TL-LANG-301` `PASS` en 805071 microsegundos; repetición del
  consolidado en 786280 microsegundos.
- Consolidado T01: `TL-LANG-294`..`TL-LANG-301`, 8/8 `PASS`.
- Guards: `TL-LANG-155`..`TL-LANG-161`, 7/7 `PASS`; `TF-INTEROP-273`
  `PASS` en 26440 microsegundos.
- Builds focales: compilador 56/56 y 6/6; suite 18/18, 3/3 y 2/2, todos con
  LLVM/Clang 23, sin regenerar CMake.
- Gate: T01 queda 8/8 y Parte B 16/147, sin batería completa.
- Siguiente ítem: `NS304-T02-F01`.

### NS304-T02-F01 — cuerpos default básicos

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T02-F01-default-bodies.md`.
- Contrato: métodos y accessors de propiedad/indexador con bloque son defaults
  productivos en `trait` e `interface`; el mismo miembro terminado en `;` es
  requisito abstracto, y `abstract` explícito con cuerpo se rechaza.
- Implementación: Parser asigna abstracción por accessor real; el modelo de
  interfaces aplica el mismo invariante cuerpo/requisito a ambos spellings y
  el plan de tabla selecciona cualquier cuerpo default declarado.
- Prueba puntual: `TL-LANG-302` `PASS` en 381914 microsegundos; repetición
  dirigida `PASS` en 413062 microsegundos.
- Guards: `TP-PAR-183`..`TP-PAR-196`, 14/14 `PASS`; `TL-LANG-271`..
  `TL-LANG-278`, 8/8 `PASS`.
- Unitaria histórica de declaraciones trait: 14/14 checks `PASS` con las rutas
  canónicas de NSLink/runtime del runner activo.
- Builds focales: compilador 7/7 y runner 5/5 con LLVM/Clang 23, sin regenerar
  CMake.
- Gate: T02 queda 1/6 y Parte B 17/147, sin batería completa.
- Siguiente ítem: `NS304-T02-F02`.

### NS304-T02-F02 — precedencia de implementaciones default

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T02-F02-default-precedence.md`.
- Contrato: implementación explícita antes que miembro nominal y miembro nominal
  antes que el default más específico; la selección por derivación es idéntica
  para `trait` e `interface` ordinaria.
- Implementación: el plan existente conserva primero las implementaciones de la
  clase; los dos reemplazos de default dejaron de depender del spelling `trait`
  y ahora siguen cuerpo y relación de derivación.
- Prueba puntual: `TL-LANG-303` `PASS` en 392739 microsegundos.
- Guards: `TL-LANG-302` `PASS` en 419338 microsegundos;
  `TL-LANG-271`..`TL-LANG-278`, 8/8 `PASS`.
- Builds focales: compilador 6/6 y runner 3/3 con LLVM/Clang 23, sin regenerar
  CMake.
- Gate: T02 queda 2/6 y Parte B 18/147, sin batería completa.
- Siguiente ítem: `NS304-T02-F03`.

### NS304-T02-F03 — diamantes y coherencia sellada

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T02-F03-diamonds-and-coherence.md`.
- Contrato: defaults no relacionados son ambiguos; una redeclaración derivada
  resuelve el conflicto; coherence sellada permite trait importado sobre tipo
  local y prohíbe duplicados o impls sobre un tipo extranjero.
- Implementación: el análisis de dominancia/redeclaración dejó de estar limitado
  al spelling `trait`; el pipeline productivo previo conserva ownership único y
  autoridad intermodular mediante la clase propietaria.
- Prueba puntual: `TL-LANG-304` `PASS` en 933173 microsegundos; repetición
  dirigida `PASS` en 913717 microsegundos.
- Consolidado parcial T02: `TL-LANG-302`..`TL-LANG-304`, 3/3 `PASS`.
- Guards: `TL-LANG-271`..`TL-LANG-278`, 8/8 `PASS`.
- Builds focales: compilador 6/6 y runner 4/4 con LLVM/Clang 23, sin regenerar
  CMake.
- Gate: T02 queda 3/6 y Parte B 19/147, sin batería completa.
- Siguiente ítem: `NS304-T02-F04`.

### NS304-T02-F04 — genéricos, asociados, `some` y `dyn`

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T02-F04-generic-associated-opaque-dyn.md`.
- Contrato: un mismo default conserva identidad y resultado bajo constraint
  genérico, associated defaults, `some Trait` y `dyn Trait`; object safety sigue
  rechazando métodos genéricos e identidades asociadas sin default.
- Implementación: el wrapper opaco clona cuerpos default y associated defaults
  aplicando las sustituciones ya establecidas; los requisitos abstractos siguen
  usando el forwarder existente.
- Prueba puntual: `TL-LANG-305` `PASS`; repetición dirigida en 452122
  microsegundos.
- Consolidado parcial T02: `TL-LANG-302`..`TL-LANG-305`, 4/4 `PASS`.
- Guards activos: `TL-LANG-271`..`TL-LANG-278`, 8/8 `PASS`; guards verticales
  reconstruidos: associated items 16/16, opaque results 14/14 y trait objects
  12/12 `PASS`.
- Builds focales: compilador 6/6, runner 3/3 y guards 12/12 pasos con
  LLVM/Clang 23, sin regenerar CMake.
- Gate: T02 queda 4/6 y Parte B 20/147, sin batería completa.
- Siguiente ítem: `NS304-T02-F05`.

### NS304-T02-F05 — defaults y witnesses versionados por NSMETA

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T02-F05-nsmeta-default-witnesses.md`.
- Contrato: el productor posee el cuerpo default; el consumidor reconstruye la
  declaración, rebindea su identidad física desde NSMETA y no necesita fuentes.
- Implementación: los accessors importados no abstractos conservan un cuerpo
  marcador, igual que los métodos, y el binding precompilado los conecta al
  callee publicado; accessors abstractos siguen emitiendo `;`.
- Prueba puntual: `TL-LANG-306` `PASS` en 518878 microsegundos; repetición
  dirigida `PASS` en 521615 microsegundos.
- Consolidado parcial T02: `TL-LANG-302`..`TL-LANG-306`, 5/5 `PASS`.
- Guards: `TL-LANG-271`..`TL-LANG-278`, 8/8 `PASS`.
- Builds focales: compilador 6/6 y runner 2/2 con LLVM/Clang 23, sin regenerar
  CMake.
- Gate: T02 queda 5/6 y Parte B 21/147, sin batería completa.
- Siguiente ítem: `NS304-T02-F06`.

### NS304-T02-F06 — vtables, ABI DLL, evolución y backend

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T02-F06-default-dispatch-abi.md`.
- Contrato: lookup estático y dispatch dinámico comparten el witness seleccionado;
  el DLL lo exporta, NSMETA fija orden, MIR/LLVM lo conservan y debug produce PDB.
- Implementación: lookup concreto reutiliza como fallback el plan coherente de
  interfaz; publicación/validación DLL admite métodos default no abstractos de
  interfaz sin ampliar requisitos abstractos.
- Prueba puntual: `TL-LANG-307` `PASS` en 782796 microsegundos; repetición
  dirigida `PASS` en 760726 microsegundos.
- Consolidado T02: `TL-LANG-302`..`TL-LANG-307`, 6/6 `PASS`; guards
  `TL-LANG-271`..`TL-LANG-278`, 8/8 `PASS`.
- Builds focales: compilador 6/6; runner 4/4 y 2/2 con LLVM/Clang 23, sin
  regenerar CMake.
- Gate: T02 queda 6/6 y Parte B 22/147, sin batería completa.
- Siguiente tanda habilitada: `T03`, empezando por `NS304-T03-F01`.

### NS304-T03-F01 — list patterns exactos, prefijo, sufijo y descarte

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T03-F01-list-pattern-shapes.md`.
- Contrato: longitud exacta sin rest; longitud mínima con rest; el prefijo y
  sufijo conservan orden y `_` no crea binding.
- Implementación: `PatternKind::List` se reduce tempranamente a `Length`,
  índices y branches ordinarios con la comprobación de longitud primero.
- Prueba puntual: `TL-LANG-308` `PASS` en 361243 microsegundos; el producto
  O0/debug terminó con código 42.
- Guards: `TL-LANG-261`..`TL-LANG-270`, 10/10 `PASS`.
- Build focal: compilador 11/11 y runner 70/70 con LLVM/Clang 23, sin
  regenerar CMake en la reconstrucción certificante.
- Gate: T03 queda 1/8 y Parte B 23/147, sin batería completa.
- Siguiente ítem: `NS304-T03-F02`.

### NS304-T03-F02 — captura rest/slice sin copia y con lifetime del origen

Estado: `CERRADO`.

- Expediente: `docs/3.0.4/items/NS304-T03-F02-rest-slice-capture.md`.
- Contrato: `.. <pattern>` captura una vista `Slice(prefix, Length-fixed)` del
  origen; no materializa ni copia elementos y conserva su lifetime prestado.
- Implementación: el parser preserva el subpattern rest y el lowering entrega
  una llamada `Slice` al binding ordinario después de los checks fijos.
- Prueba puntual: `TL-LANG-309` `PASS` en 388498 microsegundos; el positivo
  terminó 52 y el negativo rechazó el escape desde `stackalloc` con el
  diagnóstico de préstamo local exacto.
- Guards: `TL-LANG-308` y `TL-LANG-261`..`TL-LANG-270`, 11/11 `PASS`.
- Build focal: `Parser.cpp`, archivo frontend de 142 objetos y enlace nsc de
  98 inputs; runner 69+2+2 aristas, LLVM/Clang 23 y sin regenerar CMake.
- Gate: T03 queda 2/8 y Parte B 24/147, sin batería completa.
- Siguiente ítem: `NS304-T03-F03`.
- Corte de handover: Parte B queda `PAUSADA POR INSTRUCCIÓN DEL USUARIO` en
  24/147. `NS304-T03-F03` no fue iniciado ni marcado; la reanudación exacta
  comienza por su auditoría causal, sin cambiar configuración, toolchain o
  Builder y sin ejecutar batería completa entre tandas.

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
| 2026-10-12 | 1 | Cierre A08 G36 y tanda | puntual KPD + Required; consolidado A08 2/2 | `PASS`; 10 KPD abiertos |
| 2026-10-12 | 1 | Cierre A09 G05 self-host y tanda | puntual KPD + Required; consolidado A09 1/1 | `PASS`; 9 KPD abiertos |
| 2026-10-12 | 1 | Cierre A10 G37 y tanda | puntual KPD + Required; consolidado A10 1/1 | `PASS`; 8 KPD abiertos |
| 2026-10-12 | 1 | Cierre A11 G38 | puntual KPD + Required; A11 1/2 | `PASS`; 7 KPD abiertos |
| 2026-10-12 | 1 | Cierre A11 G39 y tanda | puntual KPD + Required; consolidado A11 2/2 | `PASS`; 6 KPD abiertos |
| 2026-10-12 | 1 | Cierre A12 G40 y tanda | dos puntuales KPD + Required; consolidado A12 2/2 | `PASS`; 4 KPD abiertos |
| 2026-10-12 | 1 | Cierre A13 G41 y tandas A00-A13 | cuatro puntuales KPD + Required; consolidado A13 4/4 | `PASS`; 0 KPD abiertos; batería final pendiente |
| 2026-10-12 | 2 | Primera certificación final de Parte A | run `20261012-033650-part-a-final`; 5,714 terminales | 5,713 `PASS`; `TP-PAR-478` `FAIL`; puerta no cerrada y auditoría reabierta desde A00 |
| 2026-10-12 | 2 | Corrección y consolidado dirigido de reapertura | TP-PAR-478 + guards 4/4; run `20261012-part-a-cycle2-directed-53` | 53/53 `PASS`; causa de prueba/oráculo cerrada; repetición final completa pendiente |
| 2026-10-12 | 2 | Certificación final de Parte A | run `20261012-0428-part-a-final-cycle2`; revisión `69dd18bd1`; 5,714 terminales | 5,714 `PASS`; `CERTIFICACION_FINAL_OK`; rama `v.3.0.3` preexistente en `7b7e36faf` no movida |
| 2026-10-12 | B00 | Reconciliación causal previa de Parte B | 147 IDs/18 tandas contra plan, código y pruebas actuales | 147/147 reconciliados; 0 cerrados; B01/T00 habilitada sin mover ramas |
| 2026-10-12 | B01/T00-F01 | Inventario de features 3.0 declaradas terminadas | 55 filas históricas contra expedientes, commits y rutas actuales | 55/55 trazadas; 54 ancladas; 1 implementación retirada registrada; 1/147 cerrado |
| 2026-10-12 | B01/T00-F02 | Clasificación de evidencia por capas | 55 verticales contra parser/Sema/MIR/LLVM/NSMETA/runtime/ejecución | 55/55 clasificadas; historia separada de evidencia actual; 2/147 cerrados |
| 2026-10-12 | B01/T00-F03 | Matriz C# 8.0–14 | 88 capacidades estables oficiales con disposición única | 88/88 clasificadas; 47 gaps con dueño; 3/147 cerrados |
| 2026-10-12 | B01/T00-F04 | Matriz Beef por capacidad | 69 capacidades en seis áreas oficiales con disposición única | 69/69 clasificadas; 32 gaps con dueño; 4/147 cerrados |
| 2026-10-12 | B01/T00-F05 | Disposición PlannedFor304 | catálogo, manifest, Parser y pruebas Host.Language | 9 planes no productivos; 7/7 casos puntuales PASS; 5/147 cerrados |
| 2026-10-12 | B01/T00-F06 | Reservas de versión 3.0.4 | seis contratos contra constantes y modelos actuales | 6/6 baselines trazados; ventanas T01–T15 sin bump prematuro; 6/147 cerrados |
| 2026-10-12 | B01/T00-F07 | Manifest de dependencias por feature | 18 familias contra plan, compiler, runtime y STL | 18/18 trazadas; existentes verificados y futuras explícitas; 7/147 cerrados |
| 2026-10-12 | B01/T00-F08 | Harness estructural 3.0.4 | unitarias y ejecución sobre fuentes canónicas | 4/4 unitarias PASS; gate real PASS; 8/147 cerrados |
| 2026-10-12 | B01/T00 | Consolidado puntual de tanda | F01–F08, sólo gates documentales y TL-LANG-155..161 | `PASS`; T00 8/8; T01-F01 habilitada sin batería completa |
| 2026-10-12 | B02/T01-F01 | Estados de inicialización estática | dominio cerrado, nombre estable y `TL-LANG-294` | `PASS`; T01 1/8; 9/147 cerrados; T01-F02 activa |
| 2026-10-12 | B02/T01-F02 | Constructor estático por tipo | Parser, miembro, construction y HIR/MIR; `TL-LANG-295` + guard `TL-LANG-160` | `PASS`; T01 2/8; 10/147 cerrados; T01-F03 activa |
| 2026-10-12 | B02/T01-F03 | Plan de inicialización por módulo | ProjectGraph dependency-first, aristas directas y ownership de cctors; `TL-LANG-296` + guards 295/160 | `PASS`; T01 3/8; 11/147 cerrados; T01-F04 activa |
| 2026-10-12 | B02/T01-F04 | Once thread-safe, espera, reentrancia y ciclos | guard por tipo CAS/owner/spin, cuerpo explícito separado, rechazo de ciclo; `TL-LANG-297` + Stage37 | `PASS`; T01 4/8; 12/147 cerrados; T01-F05 activa |
| 2026-10-12 | B02/T01-F05 | Fallo terminal sin retry | state3, cleanuppad Win64, propagación y panic por perfil; `TL-LANG-298` + Stage37 | `PASS`; T01 5/8; 13/147 cerrados; T01-F06 activa |
| 2026-10-12 | B02/T01-F06 | Locales static y lifetime de módulo | storage/state/owner privados, guard 0/1/2/3, sin cleanup léxico, global dtors; `TL-LANG-299` + Stage37 | `PASS`; T01 6/8; 14/147 cerrados; T01-F07 activa |
| 2026-10-12 | B02/T01-F07 | Declaraciones en condiciones | bloques if/switch y declaración por iteración while; `TL-LANG-300` + seis guardas | `PASS`; T01 7/8; 15/147 cerrados; T01-F08 activa |
| 2026-10-12 | B02/T01-F08 | Integración T01, metadata y debug | NSMETA 1.39, NSDEBUG 1.1, EXE/DLL, concurrencia, reentrancia y partición owner-only; `TL-LANG-301` + guards | `PASS`; T01 8/8; 16/147 cerrados; T02-F01 habilitada |
| 2026-10-12 | B03/T02-F01 | Cuerpos default básicos | métodos y accessors de propiedad/indexador en trait e interface; `TL-LANG-302` + 22 guards | `PASS`; T02 1/6; 17/147 cerrados; T02-F02 activa |
| 2026-10-12 | B03/T02-F02 | Precedencia de defaults | implementación explícita, miembro nominal y default más específico en trait/interface; `TL-LANG-303` + 9 guards | `PASS`; T02 2/6; 18/147 cerrados; T02-F03 activa |
| 2026-10-12 | B03/T02-F03 | Diamantes y coherence | conflicto/redeclaración y ownership intermodular sellado; `TL-LANG-304` + consolidado T02/guards | `PASS`; T02 3/6; 19/147 cerrados; T02-F04 activa |
| 2026-10-12 | B03/T02-F04 | Integración genérica de defaults | associated defaults, constraints, `some` y `dyn`; `TL-LANG-305` + guards verticales | `PASS`; T02 4/6; 20/147 cerrados; T02-F05 activa |
| 2026-10-12 | B03/T02-F05 | Witnesses default por NSMETA | método, propiedad e indexador consumidos sin fuentes del productor; `TL-LANG-306` | `PASS`; T02 5/6; 21/147 cerrados; T02-F06 activa |
| 2026-10-12 | B03/T02-F06 | Cierre ABI de defaults | lookup estático/dyn, DLL exports, slots, MIR, LLVM y PDB; `TL-LANG-307` | `PASS`; T02 6/6; 22/147 cerrados; T03-F01 habilitada |
| 2026-10-12 | B04/T03-F01 | Formas base de list patterns | exacto, prefijo, sufijo y descarte bajados a Length+índices; `TL-LANG-308` | `PASS`; T03 1/8; 23/147 cerrados; T03-F02 activa |
| 2026-10-12 | B04/T03-F02 | Captura rest/slice prestada | `.. var` baja a `Slice` sin copia y conserva lifetime; `TL-LANG-309` positivo+negativo | `PASS`; T03 2/8; 24/147 cerrados; T03-F03 activa |
| 2026-10-13 | mantenimiento focal | Tipo función nullable con lifetime | `TP-PAR-053`; `QuestionBracket` se integra al carrier sin perder lifetime; oráculo separado en siete campos | `PASS` en 3,370 us; no altera 24/147 ni certifica una tanda de Parte B |
| 2026-10-13 | mantenimiento focal | Placement construction | `TP-PAR-470`; fixture con `Item` nominal conocido y oráculo separado en siete campos | `PASS` en 5,504 us; corrección sólo de prueba; no altera 24/147 ni certifica una tanda de Parte B |

## 6. Estado al corte

- A: `52/52` cierres puntuales preservados; `TP-PAR-478` corregido; consolidado dirigido 53/53 y batería final
  5,714/5,714 `PASS`; Parte A certificada en ciclo 2.
- B: `24/147`; B00 reconcilió los 147 IDs, T00-F01/F02 separaron inventario y
  evidencia histórica, T00-F03 clasificó 88/88 capacidades estables C# 8.0–14
  T00-F04 clasificó 69/69 capacidades Beef de las seis áreas comprometidas y
  T00-F05 separó nueve planes 3.0.4 de la superficie productiva y T00-F06
  congeló seis ventanas de versión sin adelantar writers ni ABI, y T00-F07
  publicó las dependencias de 18 familias sin convertir planes en artefactos,
  y T00-F08 añadió el harness estructural; consolidado T00 `PASS`. T01-F01
  fijó los cuatro estados de inicialización y `TL-LANG-294` pasó; T01-F02
  cerró la declaración estática hasta MIR con `TL-LANG-295`; T01-F03 añadió el
  plan dependency-first por módulo con `TL-LANG-296`; T01-F04 cerró el guard
  CAS/owner/spin y ciclos con `TL-LANG-297` y Stage37; T01-F05 hizo terminal el
  estado `failed`, preservó unwind y seleccionó panic por perfil con
  `TL-LANG-298`; T01-F06 cerró locales static con storage/guard por declaración
  y destrucción al unload mediante `TL-LANG-299`; T01-F07 cerró declaraciones
  en `if`/`while`/`switch` con `TL-LANG-300`; T01-F08 promovió la superficie,
  reclamó NSMETA 1.39/NSDEBUG 1.1 y cerró EXE, DLL, concurrencia y reentrancia
  con `TL-LANG-301`. T01 queda 8/8; T02-F01 habilitó cuerpos default en
  métodos, propiedades e indexadores de traits e interfaces mediante
  `TL-LANG-302`; T02-F02 fijó la precedencia explícita, nominal y default más
  específico mediante `TL-LANG-303`; T02-F03 unificó diamantes y coherence
  sellada para trait/interface mediante `TL-LANG-304`; T02-F04 integró defaults
  con genéricos, associated items, constraints, `some` y `dyn` mediante
  `TL-LANG-305`; T02-F05 preservó witnesses default de método, propiedad e
  indexador por NSMETA mediante `TL-LANG-306`; T02-F06 cerró lookup estático,
  vtables, DLL exports, evolución, MIR/LLVM y PDB mediante `TL-LANG-307`. T02
  queda 6/6. T03-F01 añadió list patterns exactos, prefijo, sufijo y descarte
  con `TL-LANG-308`; T03-F02 añadió captura rest como `Slice` prestado sin
  copia y cierre de escape con `TL-LANG-309`; T03 queda 2/8 y T03-F03 queda
  habilitada.
  La contradicción de `v.3.0.3`
  permanece como deuda y la rama no se mueve.
- C: `0/149`; bloqueada por cierre certificado de B.
- D: `0/10`; bloqueada por cierre certificado de C.
- Ramas: `v.3.0.3` ya existía antes del cierre y apunta a `7b7e36faf51fe4712940cc76c18a4598fb94facd`; no se
  movió. `v.3.0.4` a `v.3.0.6` no se crean anticipadamente.
- Validación ejecutada: A00-A13 puntuales y consolidados por tanda; primera batería completa ejecutada con un
  fallo terminal en `TP-PAR-478`, seguida por corrección puntual, guards y consolidado dirigido 53/53 `PASS`;
  repetición final 5,714/5,714 `PASS`. Evidencias en `_nscbuilder3/history/20261012-033650-part-a-final/`,
  `_nscbuilder3/history/20261012-part-a-cycle2-directed-53/` y
  `_nscbuilder3/history/20261012-0428-part-a-final-cycle2/`.

