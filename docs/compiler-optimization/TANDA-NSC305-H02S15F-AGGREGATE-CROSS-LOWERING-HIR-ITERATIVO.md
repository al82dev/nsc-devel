# H02S15F - Aggregate/tuple/with cross-lowering HIR iterativo

## SCOPE LOCK

- Repositorio: clon aislado `A:\NSC-devel-paralelo\development-agente-1-hir`.
- Base: H02S15E `8edbdf077640b06988d356554f9f8c7bad8f3469`.
- Alcance único: retirar las tres reentradas `lowerExpression` del SCC formado por `WithExpression`, spread de aggregate y valor ordinario de aggregate.
- Producto autorizado: `HirExpressionMachine.cpp` y el guard legacy en `HirAggregateLowering.cpp`.
- Prueba autorizada: target `nsc_project_oop_lowering_tests`, selector `hir-aggregate-cross-lowering-depth`.
- Protegido: S15B/C/E, parser, sema, MIR, LLVM, ABI, NSMETA, Builder y tracking global.
- Prohibido: CTest, suites, minibaseline, baseline, instalación, deploy, `push`, `pull` y `fetch`.

## Causa y contrato preservado

`appendAggregateElements` bajaba sincrónicamente tres clases de child mediante `lowerExpression`: source de `with`, source de spread y valor ordinario. Como cualquiera de esos children puede volver a ser aggregate/tuple/with, la profundidad se acumulaba en el stack nativo a través del SCC.

La corrección introduce un solo estado `AggregateCrossLowering` dentro de la máquina iterativa común. Cada frame mantiene en heap:

- el nodo aggregate en construcción;
- la pila estructural de inicializadores exactos que sí deben aplanarse;
- el índice monotónico del entry;
- el slot y offset absoluto pendiente;
- targets y provenance del spread pendiente;
- la fase de suspensión y el único child siguiente.

El scheduler baja cada source/value una vez, reanuda el frame y publica el resultado sólo después de aceptar el child. No busca nuevamente desde el inicio, no clona el AST ni el HIR y no introduce recursión, coroutine o estado compartido.

Se preservan literalmente:

- orden `with-source -> implicit spread -> updates`;
- orden de entries y target ordinals;
- flatten estructural sólo para `AggregateInitializerExpression` exacto;
- `TupleExpression` y `WithExpression` usados como valores ordinarios como nodos aggregate anidados;
- conversiones implícitas no identity exactamente una vez;
- offsets absolutos/relativos, alignment, volatile y metadata de bitfield;
- copy/move spread, unwrap de `MoveExpression` y provenance local/parameter;
- union con sólo el primer slot y omisión de unnamed bitfields;
- texto, prioridad y range del primer diagnóstico;
- wrapper de cleanup del child, sin fabricar cleanup para el aggregate.

## Contrafactual y satélites

El selector `hir-aggregate-cross-lowering-depth` entra exclusivamente por `ProjectOopHirModel::lowerExpression` y construye 10,923 motivos reales:

```text
Wi -> source Si
Si -> spread source Vi
Vi -> ordinary value W(i+1)
```

Son 32,769 transiciones consecutivas dentro del SCC. La verificación y destrucción del HIR también son iterativas. El mismo selector contiene los satélites contractuales de tuple, flatten exacto, aggregate anidado, updates de with, copy/move spread, conversión, union, unnamed/volatile/bitfield, diagnósticos, propagación de error/range y cleanup de child.

La evidencia negativa separa binding ausente de binding presente con cardinalidad distinta a `children`, y exige en ambos casos el texto histórico, el range completo y resultado nulo. Los rechazos `With` sobre bitfield, spread sobre bitfield y spread con target fuera de rango utilizan un source válido que produce `FullExpressionCleanup`; el ordinal sintético consumido queda en `1` aun cuando el aggregate no se publica. Esto hace observable que el source se bajó antes del diagnóstico estructural y conserva la precedencia histórica sin instrumentar producto.

## Evidencia autorizada

- Build único: `cmake --build A:\NSC-devel-paralelo\builds\agente-1-s14a --target nsc_project_oop_lowering_tests`.
- Prueba única: `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-aggregate-cross-lowering-depth nsc_project_oop_lowering_tests.exe`.
- Logs finales de la revisión: `A:\NSC-devel-paralelo\logs\agente-1\h02s15f-build-amend.log` y `A:\NSC-devel-paralelo\logs\agente-1\h02s15f-focus-amend.log`.

El alcance de esta evidencia es sólo H02S15F. La batería amplia y el baseline permanecen reservados para la GUI del usuario.

Resultado focal revisado: `PASS: 21 checks`, exit `0`.

La primera compilación de esta revisión detectó que el fixture había declarado `voidType` en un bloque homónimo fuera de scope. Se corrigió exclusivamente esa declaración de prueba y se repitió el mismo target; el build final enlazó con exit `0`. No se modificó producto durante la revisión.

### Desviación operativa no usada como evidencia

Una primera invocación pasó `--focus hir-aggregate-cross-lowering-depth`, pero este runner no procesa argumentos: al no recibir `NSC_PROJECT_OOP_LOWERING_FOCUS`, ejecutó accidentalmente su ruta default de 614 checks y terminó con tres fallos ajenos a H02S15F. Esa corrida no demuestra ni invalida esta hoja, no se usa como evidencia y no se repitió. La única evidencia focal válida es el target exacto seguido del selector canónico mediante `NSC_PROJECT_OOP_LOWERING_FOCUS`, cuyo resultado fue `PASS: 18 checks`.
