# NSC 3.0.5 — H02S13B: llamadas unary/postfix de operador de usuario HIR iterativas

## Clasificación

`IMPLEMENTADO; VALIDACIÓN DE BUILD Y PRUEBAS DIFERIDA A LA TANDA INTEGRAL`.

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la reentrada AST→HIR del operando de una llamada unary/postfix ya resuelta como operador de usuario. |
| Naturaleza | Refactor interno no funcional del lowering HIR. |
| Productor | `ProjectOopHirModel::lowerExpression`. |
| Archivos funcionales | `src/Frontend/HIR/HirExpressionMachine.cpp`. |
| Incluido | Clasificación resuelta, descenso según `passKind`, llamada estática, conversiones, borrows, cleanup, ref-return, comportamiento noheap histórico y `memberOrdinal`. |
| Excluido | Operadores binarios, resolución de overloads, parser, Sema, MIR, LLVM, ABI, NSMETA y sintaxis nueva. |
| Ejecución | Build focal PASS y `hir-user-unary-operator-depth` PASS 3/3. |

## Ruta histórica auditada

`lowerExpressionImpl` trataba `UnaryExpression` y `PostfixExpression` en el mismo caso. Cuando
`callBinding` estaba resuelto, el miembro seleccionado existía y estaba marcado
`isUserOperator`, la ruta llamaba a `lowerResolvedCall(binding, { expression->first() })`.

Ese wrapper delegaba en `lowerBoundCall`, que era y sigue siendo la única autoridad para:

- decidir el contexto address del operando a partir del `passKind` del parámetro;
- aplicar `selectedConversions` después de bajar el operando;
- insertar el borrow y el cleanup de un temporal owned cuando el binding lo exige;
- conservar `StaticCall`, `memberOrdinal` o namespace ordinal, tipos y flags de dispatch;
- sintetizar argumentos por omisión si el binding los contiene;
- marcar `referenceResult` y cargar un retorno `ref` cuando se consume como valor.

La recursión estaba en la llamada a `lowerExpression` del único argumento dentro de
`lowerBoundCall`. Por eso una cadena sintáctica profunda de operadores de usuario consumía un
frame del proceso por nivel aunque la forma de la llamada ya estuviera completamente resuelta.

## Diseño implementado

La máquina común incorpora `UserUnaryOperator`. El clasificador sólo captura exactamente la
misma condición histórica: binding resuelto, ordinal presente, miembro invocable existente y
`isUserOperator=true`. Un binding incompleto o cualquier expresión builtin permanece en su ruta
anterior y conserva diagnósticos y decisiones históricas.

Al sembrar el frame se conserva el binding y se consulta el miembro seleccionado. El operando
desciende una sola vez con address context si y sólo si su primer parámetro no es `Value`; esta es
la misma condición `implicitOperatorReference` que usa `lowerBoundCall`. Al desenrollar, el HIR ya
bajado se entrega como `preloweredArguments`, pero la llamada completa vuelve a construirse por
`lowerBoundCall`. No se duplicaron conversiones, borrows, defaults, cleanup ni ref-return.

La ruta histórica unary/postfix no ejecutaba `validateNoHeapDirectCall`: `lowerResolvedCall`
delegaba directamente a `lowerBoundCall`, y ni esa función ni la salida común de `lowerExpression`
agregaban tal validación. El frame conserva exactamente esa conducta y no introduce un diagnóstico
noheap nuevo en la raíz ni en operadores anidados. Las rutas de property/indexer que sí validaban
noheap continúan haciéndolo en sus helpers existentes y quedan fuera de esta hoja.

La llamada histórica `lowerResolvedCall(operatorCall, { expression->first() })` fue retirada del
caso unary/postfix de `lowerExpressionImpl`. Los operadores resueltos ya sólo entran en el frame;
los bindings incompletos continúan por las decisiones builtin y diagnósticos anteriores.

## Invariantes preservadas

- El operando se evalúa exactamente una vez y antes de construir su llamada.
- `ParameterPassKind::Value` baja como valor; `Ref`, `Out` e `In` bajan en address context.
- `selectedConversions` se aplican en el mismo punto y orden históricos.
- Los borrows y temporales owned conservan su binding, cleanup y orden observable.
- `memberOrdinal`, namespace ordinal, `baseReceiver`, interface dispatch, tipo y flags no cambian.
- Los argumentos por omisión y la carga de un retorno `ref` siguen perteneciendo a
  `lowerBoundCall`.
- La ruta unary/postfix conserva la ausencia histórica de validación noheap directa, sin añadir
  diagnósticos a la raíz ni a operadores anidados.
- Las ramas unary y postfix comparten el mismo contrato histórico sin introducir `operator ++`
  ni `operator --`, que continúan fuera de esta superficie.
- No cambian sintaxis, API pública, ABI, NSMETA, diagnósticos ni formatos.

## Contrafactual profundo registrado

El foco `hir-user-unary-operator-depth` prepara primero un proyecto NativeSharp real con un struct
`Node`, un `public static Node operator -(Node value)` y un caller. Del modelo semántico toma el
ordinal real del operador, el ordinal del caller y el type key canónico.

Después construye una cadena interna de 32,768 llamadas y alterna los dos `AstNodeKind` que la
ruta histórica compartía: `UnaryExpression` y `PostfixExpression`. Cada nivel usa un
`MethodCallBinding` resuelto al mismo miembro real. La selección del miembro exige además que el
parámetro productivo tenga `ParameterPassKind::Value`, de modo que el contrafactual comprueba la
ruta válida por valor y no una firma de operador rechazada por el lenguaje. La hoja es `this`
tipado como el struct y, al consumirse por valor, debe permanecer como el `PointerLoad(ThisValue)`
que materializa el valor del receiver de un struct.

La verificación exige:

- terminación sin recursión de la pila del proceso;
- exactamente 32,768 `StaticCall` en el orden de anidamiento;
- alternancia exacta de provenance unary/postfix;
- un único hijo por llamada y evaluación única del operando;
- `memberOrdinal` y type key reales en todos los niveles;
- ausencia de dispatch, base, null-conditional y ref-result accidentales;
- `ParameterPassKind::Value` obtenido del miembro real y hoja productiva
  `PointerLoad(ThisValue)`.

El uso sintético de `PostfixExpression` no añade sintaxis fuente: es un contrafactual interno para
la segunda rama del caso histórico compartido. La declaración fuente continúa respetando las
restricciones vigentes del parser.

## Infraestructura

La cobertura se añadió a `tests/unit/ProjectOopLoweringTests.cpp`, que ya pertenece al ejecutable
`nsc_project_oop_lowering_tests` del CMake raíz. Ese target ya está registrado como
`nsc.project-oop-lowering` mediante `add_test`. El selector por
`NSC_PROJECT_OOP_LOWERING_FOCUS` incluye el foco nuevo y la ruta predeterminada llama al mismo
caso, por lo que no queda cobertura sólo focal.

No se creó archivo de prueba, target, fixture externo ni caso ejecutado por `nsc.exe`. Por ello no
aplican CMake de `tests/compiler-suite`, catálogo, `cases.jsonl`, inventario CSV ni alta en NSC
Builder 3: esas capas descubren pruebas externas o targets independientes, mientras que este caso
es una función adicional dentro de un ejecutable unitario ya registrado.

## Gates estáticos

- Comparación de la condición de clasificación contra el caso histórico de
  `lowerExpressionImpl`.
- Comparación del descenso por `passKind` contra `implicitOperatorReference` en
  `lowerBoundCall`.
- Reutilización de `lowerBoundCall` como autoridad única de conversiones, borrows, defaults,
  cleanup, ordinal y ref-return.
- Ausencia de la antigua reentrada `lowerResolvedCall` en el caso unary/postfix.
- Comparación de la ruta noheap contra HEAD: cero validaciones adicionales tanto en raíz como en
  operadores anidados, igual que `lowerResolvedCall` histórico.
- Auditoría de target, `add_test`, foco y ruta predeterminada.
- Revisión de diff y whitespace antes de integración.

## Validación focal

- `nsc_project_oop_lowering_tests` compiló y enlazó correctamente.
- `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-user-unary-operator-depth`: PASS 3/3.
- El fixture obtiene un operador real por valor del modelo semántico antes de construir la cadena.
- Logs persistentes: `A:\temp\h02s13b-user-unary-build.log` y
  `A:\temp\h02s13b-user-unary-focus.log`.
- No se ejecutó baseline ni la batería completa.
