# NSC 3.0.5 — H02S16A: declaraciones de variable HIR iterativas

## Estado

Implementación y prueba focal de `VariableDeclarationStatement` sobre la worklist HIR común.
El alcance de esta hoja no modifica sintaxis, Sema, MIR, LLVM, ABI, API ni NSMETA, y deja
explícitamente pendientes H02S16B y H02S16D–N.

## Problema cerrado

La rama histórica de `lowerStatement` bajaba de forma síncrona el initializer, el count de un VLA
unidimensional o cada extent de un VLA multidimensional. Esa llamada reconstruía pila nativa en
dos ciclos productivos:

```text
variable -> initializer loop -> LoopBlock -> variable
variable -> initializer match -> arm declaration -> variable
```

La declaración ahora es una continuación de `lowerExpressionWithStatementFrame`. Un único frame
posee el `LocalDeclaration` parcial, el símbolo semántico, el modo de child, los extents ya
completados y el `Block` parcial del VLA multidimensional. La preparación agenda como máximo un
child y el finalizador no llama a ningún lowerer.

## Costuras implementadas

- `lowerStatement(VariableDeclarationStatement)` sólo delega a
  `lowerVariableDeclarationStatement`.
- `LoopBlock` reconoce exclusivamente variables antes de su fallback atómico para otras familias.
- Match suspende el arm para cada declaration variable y conserva el fallback de statements fuera
  de H02S16A.
- Cada initializer/count/extent recibe una sola vez las postcondiciones históricas de
  `lowerExpression`, incluido `noheap`, antes de ser aceptado por el frame.
- Un error conserva el rango interior ya publicado; si el lowerer leaf no publicó rango, se usa el
  rango de ese child y nunca el de la declaración exterior.
- Cleanup se resuelve durante la preparación pero sólo se activa después de publicar la
  declaración en LoopBlock o Match. Para VLA multidimensional, la activación observa el
  `LocalDeclaration` principal, último child de su `Block`.

## Contratos preservados

- Metadata completa de local: syntax, nombre, tipo, ordinal, static/reference/pin, ownership dual,
  automatic destruction y cleanup ordinal.
- Reference initializer en address context, sin load ni conversión ordinaria.
- `placementConstructed` observa únicamente el kind superior antes de conversion y borrow.
- Conversion no identity antes de `BorrowValue`; conversión inexistente no crea diagnóstico nuevo.
- Borrow inválido o ausente no inventa wrapper.
- VLA 1D conserva un único count bajo `StackAllocation`.
- VLA multidimensional evalúa extents una vez y en orden fuente, publica hidden declarations antes
  del local principal y reutiliza `LocalValue` de esos hidden locals en la allocation.
- Diagnósticos estructurales y rollback no publican nodos ni cleanup parciales.

## Prueba puntual registrada

Target único:

```text
nsc_project_oop_lowering_tests
```

Selector único:

```text
NSC_PROJECT_OOP_LOWERING_FOCUS=hir-variable-declaration-depth
```

El selector contiene:

- contrafactual real de 32,768 declaraciones enlazadas por `variable -> loop -> block -> variable`;
- contrafactual real de 32,768 declaraciones enlazadas por
  `variable -> match -> arm declaration -> variable`;
- metadata sin initializer y static drop-bearing;
- value context y reference mutable/readonly en address context;
- conversiones identity, numérica y no existente;
- placement directo y allocator-selected envuelto en `Block`;
- borrow válido, ausente y fuera de rango;
- cleanup RAII/scope/value-drop;
- error interior con rango exacto y sin publicación parcial;
- VLA 1D y multidimensional con reads volátiles distinguibles exactamente una vez;
- errores de metadata y cardinalidad antes del primer extent;
- activación/restauración cleanup en LoopBlock y suffix exacto en Match;
- rollback de cleanup tras error interior tanto en LoopBlock como en Match;
- postcondición `noheap` aplicada en la frontera del child.

## Evidencia

La revisión estática del producto acredita:

- cero arista propia `VariableDeclarationStatement -> lowerExpression` fuera de la worklist;
- cero reentrada de VariableDeclaration por `lowerStatement` desde LoopBlock o Match;
- un solo scheduler y finalizadores sin reentrada;
- orden secuencial de extents y declaraciones de arm;
- destrucción iterativa del HIR profundo en la prueba.

La evidencia ejecutada se registra con los logs A-only del target y selector focal. No se ejecutan
CTest, suites, minibaseline, baseline ni pruebas adicionales durante esta hoja.

### Resultado ejecutado

- Target exacto `nsc_project_oop_lowering_tests`: compiló correctamente en Release.
- Selector exacto `hir-variable-declaration-depth`: `PASS: 28 checks`, exit code 0.
- Log de build válido:
  `A:\NSC-devel-paralelo\logs\agente-1\h02s16a-build-final.log`.
- Log focal válido:
  `A:\NSC-devel-paralelo\logs\agente-1\h02s16a-focus-final.log`.
- Temporales: `A:\NSC-devel-paralelo\temp\agente-1`.

La primera ejecución focal detectó una expectativa incorrecta del propio satélite allocator: el
contrato S15D publica `BlockExpression`, no `Block`. Esa corrida (`h02s16a-focus.log`, 27/28) no se
usa como evidencia. Se corrigió únicamente el oráculo, se recompiló el mismo target y se repitió el
mismo selector hasta obtener el PASS anterior. No cambió el producto por esa falla.
