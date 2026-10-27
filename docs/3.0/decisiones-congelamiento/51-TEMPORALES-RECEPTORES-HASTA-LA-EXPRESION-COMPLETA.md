# Congelamiento 3.0, elemento 131a — temporales owned receptores hasta el final de la expresión completa

Fecha: 2026-10-26. Revisión de partida: `942af6aba`.

## Nota de trabajo

```text
HEAD: 942af6aba
Elemento: 131a del plan de 148 (hallazgos T7, T8 y T10 de la medición de D-08, doc 50)
Contrato vigente: C-11 (dueño oculto de temporales descartados o receptores); decisión de Alberto
  (2026-10-26), abajo
Documento histórico que podría contradecirlo: ninguno
Archivos exactos que se modificarán:
  include/nativesharp/Frontend/Internal/MethodBody/ExpressionResult.h
  include/nativesharp/Frontend/ProjectMethodBodyModel.h
  include/nativesharp/Frontend/ProjectOopHirModel.h
  src/Frontend/Bodies/MemberAccessAnalyzer.cpp
  src/Frontend/Bodies/StatementAnalyzer.cpp
  src/Frontend/Bodies/AssignmentAnalyzer.cpp
  src/Frontend/HIR/HirExceptionAndCleanup.cpp
  tests/unit/Freeze30ContractTests.cpp
```

## Problema

C-11 sólo daba dueño a un temporal receptor cuando el consumidor producía un escalar o datos simples. Estas tres
formas no se destruían nunca (sondas `A:\temp\nsc-dec\d36`):

| Sonda | Forma | Destrucciones |
|---|---|---|
| T7ReferenceResult | `string s = Make(3).Name;` | 0 |
| T8Chain | `Make(3).Next().V` | 1 de 2 (`Make(3)` se fugaba) |
| T10FieldWrite | `Make(9).V = 12;` | 0 |

## Decisión de Alberto (2026-10-26)

- El compilador conserva cada temporal owned hasta el final de la expresión completa y lo destruye exactamente
  una vez, en orden inverso de creación.
- Si el resultado que sale de la expresión es un préstamo ligado al temporal, escaparlo o almacenarlo se rechaza
  en compilación.
- No se permite resolver una fuga creando una referencia colgante.
- No se amplía la búsqueda a formas no reproducidas.

## Contrato

1. **Préstamo de un temporal (análisis).**
   - Una lectura de campo de tipo referencia o string sobre el temporal de una llamada con retorno owned
     (`Make().Name`) queda marcada como préstamo de ese temporal (`borrowsOwnedTemporary`).
   - También queda marcada una lectura de campo sobre otra lectura marcada (`Make().In.Text`).
   - Inicializar un local, asignar, devolver o entregar con `yield` un valor marcado se rechaza:

     ```text
     'Name' is read from the temporary of an owned-returning call, which is destroyed at the end of the full
     expression, so the local 's' cannot keep it; store the temporary in a raii or owned local first
     ```

   - Los locales del compilador (prefijo `$`) quedan fuera.
2. **Dueño hasta la expresión completa (HIR, `ownReceiversUntilFullExpression`).** El temporal receptor recibe
   un dueño oculto en tres formas:
   - una lectura de campo que no son datos simples, cuando su valor termina en un valor simple o se descarta;
   - una llamada de instancia que devuelve un objeto o string owned nuevo;
   - el destino de una asignación de campo.

   La expresión completa, hasta su sentencia, queda envuelta en un `FullExpressionCleanup` si no hay ya uno que
   la contenga. Así el temporal se destruye después de todo otro uso, en orden inverso de creación.
3. **Sin referencias colgantes.**
   - Una lectura prestada que podría llegar a algo que la guarda (una rama de `?:`, un `??`, otra llamada que
     devuelve referencia) no recibe dueño. Conserva la conducta anterior.
   - La expresión completa termina también en un nodo que evalúa un operando sólo en algunos caminos
     (`?:`, `&&`, `||`, `??`, acceso nulo-condicional, `match`) o que suspende.
   - Si la raíz contiene un `await` o un `yield`, el temporal se cierra en su consumidor y no cruza la
     suspensión.

## SCOPE LOCK

- **Cambia.**
  - Las tres formas de arriba se destruyen.
  - Guardar, asignar, devolver o entregar con `yield` una lectura de campo prestada de un temporal es error.
- **Idéntico.**
  - Los receptores de escalares y datos simples (C-11, P4Receiver).
  - Los descartes `Make();`.
  - Los argumentos `owned` (elemento 118).
  - `(new Box(5)).Get()` (es 131b).
  - Las formas no reproducidas: `c ? Make().Name : "x"` sigue sin destruir el temporal (A10).

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol. Sondas `A:\temp\nsc-dec\d37` en `-O0` y
`-O2`. `Log` acumula el `Id` de cada `Box` destruido:

| Sonda | Forma | Resultado |
|---|---|---|
| A1BorrowedRead | `Make(1).Name.Length + Make(2).Name.Length` | 621: viven hasta el final; se destruye 2 y luego 1 |
| A2NewOwnerChain | `Make(3).Next().Id` | 443: se destruye 4, luego 3 |
| A3FieldStore | `Make(9).Id = 12;` | 12: se destruye después de la escritura |
| A7Compare | `if (Make(5).Name == "box")` | 105 |
| A8DiscardRead | `Make(6).Name;` | 6 |
| A9RaiiFromChain | `raii Box b = Make(3).Next();` | 4034: `Make(3)` tras la declaración, `b` al cerrar el bloque |
| A11ArgumentRead | `Count(Make(7).Name)` | 307 |
| A13Loop | 100 vueltas con lectura y escritura | 300 |
| A10ConditionalKeeps | `c ? Make(3).Name : "x"` | 300: sin cambio |
| A4, A5, A6, A12 | local, asignación, `return`, cadena `Make(3).In.Text` | rechazadas con el diagnóstico de arriba |

Sondas d36: T7 se rechaza, T8 da 42 (2 de 2), T10 da 1, y T1–T6, T9 y T11 no cambian.

Sin cambios en el resto (`seq.cmd f131a` contra `f121`):

- `all.cmd` idéntico;
- `asynccases` 86, `gencases` 76 y `loopcases` 10 PASS;
- los 66 programas de Freeze30 en `-O0` y `-O2`;
- `tests/cases` (incluido `nsc.stage31.receivers`, que ahora destruye el `Counter` de
  `Make(9).Value = 12`), d28 y bibliotecas como antes.

Prueba nueva: `Freeze30ContractTests::testReceiverTemporariesUntilFullExpression`. Casos:

- F131aBorrowedRead: 621;
- F131aNewOwnerChain: 443;
- F131aFieldStore: 12;
- F131aStoredBorrow, F131aAssignedBorrow y F131aReturnedBorrow: rechazadas.

Compila en `A:\temp\nsc-dec\bt`; se ejecuta en el gate final.

## No verificado

- Batería completa y unitarias: gate final.
