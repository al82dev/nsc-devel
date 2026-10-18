# TANDA NSC305 · H02S13A · interpolation holes HIR iterativos

## SCOPE LOCK

- **Objetivo autorizado**: retirar la recursión AST→HIR de los huecos de un
  `InterpolatedStringExpression` ligado a `InterpolationHandler`.
- **Naturaleza**: refactor interno de lowering HIR; no agrega sintaxis ni cambia contratos públicos.
- **Base**: `development`, `ccb72b66d4c0f834f39b3cbb470ad8b5d8df6670`.
- **Archivos autorizados**:
  - `src/Frontend/HIR/HirExpressionMachine.cpp`;
  - `tests/unit/ProjectOopLoweringTests.cpp`;
  - este expediente.
- **Archivos protegidos**: Driver, MIR, Parser, metadata/NSMETA, ABI, CMake y tracking externo.
- **Acciones prohibidas**: compilar, ejecutar pruebas, baseline, instalar, publicar o tocar otra unidad.
- **Criterio de cierre estático**: ningún hueco del handler vuelve a entrar recursivamente a
  `lowerExpression`; orden, forma HIR, tipos, contexto del miembro, ordinales, diagnósticos y
  política noheap permanecen equivalentes; el contrafactual profundo queda registrado en el
  target unitario existente y en su ruta focal y por defecto.

## Ruta causal auditada

La implementación histórica de `InterpolatedStringExpression` construía correctamente:

1. un `BlockExpression` del tipo handler;
2. un temporal sintético inicializado con `DefaultValue`;
3. una llamada `AppendLiteral` o `AppendFormatted` por parte, en orden de fuente;
4. una llamada final `Finish`.

El único arco recursivo de esta familia estaba dentro de la parte `Hole`:

```text
InterpolatedStringExpression
  -> for InterpolationPart in source order
     -> Hole
        -> lowerExpression(child, AppendFormattedOrdinal, false)
```

Una interpolación usada como hueco de otra repetía ese arco hasta consumir la pila nativa. El
defecto no estaba en el parser de plantillas, en la selección semántica del handler ni en LLVM.

## Diseño aplicado

`lowerExpressionWithStatementFrame` reconoce ahora `InterpolationHandler` como una clase de frame.
Al abrir el frame:

- reserva el ordinal sintético en el mismo momento lógico que la ruta histórica;
- parsea una vez las partes que el frame conservará;
- congela los hijos de hueco en orden de fuente;
- conserva el `MethodCoreBinding` y sus tres ordinales de builder;
- desciende al primer hueco sin crear una llamada C++ recursiva.

Al reanudarse, el frame acumula el HIR ya bajado del hueco y agenda el siguiente. Cuando termina,
`buildInterpolationHandlerExpression` reconstruye exactamente la forma histórica del bloque.

Cada frame conserva además el `memberOrdinal` efectivo. Un hueco se baja bajo el ordinal de
`AppendFormatted`, no bajo el miembro que contiene la interpolación. Esto preserva:

- el contexto usado por conversiones y llamadas anidadas;
- la verificación de llamadas directas `noheap`;
- la identidad del callable que recibe el operando;
- los diagnósticos que dependen del caller actual.

Los demás frames heredan y restauran ese contexto al agendar sus operandos. Para las rutas que ya
eran iterativas, el valor permanece idéntico al anterior.

## Contratos preservados

- `AppendLiteral`, `AppendFormatted` y `Finish` conservan sus ordinales semánticos.
- Los literales y huecos conservan orden izquierda→derecha.
- El receptor de cada llamada sigue siendo `LocalAddress(handlerType + "*")` al mismo temporal.
- El resultado sigue siendo `BlockExpression(handlerType)` y `Finish` sigue siendo su último hijo.
- El ordinal sintético se reserva antes del parse, igual que antes.
- La falta de un hijo conserva literalmente
  `interpolation handler hole has no expression`.
- El contexto de dirección conserva literalmente
  `an interpolation handler result is a value, not addressable storage`.
- Los errores de `parseStaticInterpolation` se propagan sin reescritura.
- `format` continúa siendo aceptado y congelado por el parser, pero el protocolo productivo
  vigente de `AppendFormatted` tiene un solo argumento y no materializa ese texto en HIR. Esta
  tanda preserva ese contrato; no inventa un overload ni una conversión nueva.
- El lenguaje vigente no expone un campo de alineación independiente en `InterpolationPart`.
  H02S13A no amplía la sintaxis ni reactiva una superficie inexistente.

## Contrafactual profundo registrado

`tests/unit/ProjectOopLoweringTests.cpp` registra
`testDeepInterpolationHandlerLowering`:

- obtiene de un proyecto mínimo real los miembros `AppendLiteral`, el overload
  `AppendFormatted(Builder)`, `Finish` y el caller;
- construye 32,768 interpolaciones handler anidadas;
- cada nivel contiene dos huecos y tres literales para comprobar orden LIFO del worklist frente a
  orden observable izquierda→derecha;
- el primer hueco enlaza al siguiente nivel y usa spelling de formato `:X4`;
- el segundo hueco comprueba que el sibling posterior no se pierde ni se adelanta;
- comprueba la forma exacta del bloque, tipos, ordinal sintético, receptores, miembros y `Finish`;
- destruye el HIR mediante la ruta iterativa existente.

El mismo foco comprueba también los diagnósticos exactos para hueco faltante, resultado solicitado
como dirección y plantilla sin cierre.

## Infraestructura

- Fuente existente: `tests/unit/ProjectOopLoweringTests.cpp`.
- Target raíz existente: `nsc_project_oop_lowering_tests`.
- Registro focal nuevo: `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-interpolation-handler-depth`.
- Registro por defecto: la misma prueba se invoca en el bloque sin foco.
- CTest: el target ya registrado por el CMake raíz conserva su caso existente.
- Builder/manifiesto/catálogo externo: no aplica; no se añadió fuente, ejecutable ni caso externo.
- Fuente o target huérfano: ninguno introducido por H02S13A.

## Validación declarada

Estado de esta tanda: **CIERRE ESTÁTICO COMPLETO; VALIDACIÓN DE BUILD PENDIENTE DEL USUARIO**.

Por instrucción expresa no se compiló ni se ejecutó ninguna prueba. Antes de integrar se exige:

- `git diff --check` sin errores;
- auditoría del diferencial limitado a los tres archivos del SCOPE LOCK;
- comprobación mecánica del target, foco y llamada por defecto;
- comprobación de que las funciones nuevas no exceden 60 líneas ejecutables;
- índice de Git vacío y exclusión de cambios concurrentes Driver/MIR/Parser/Builder.

La futura ejecución focal, fuera de esta tanda, será el caso existente
`nsc_project_oop_lowering_tests` con el foco `hir-interpolation-handler-depth`.
