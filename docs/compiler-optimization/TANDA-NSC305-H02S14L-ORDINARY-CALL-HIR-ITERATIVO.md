# H02S14L - ordinary, virtual e interface calls HIR iterativas

## SCOPE LOCK

- Objetivo: retirar la reentrada nativa del fallback de llamadas ordinarias durante HIR lowering.
- Repositorio y rama: clon aislado `development-agente-1-hir`, rama
  `codex/development-agente-1-hir`, base `84f3b7be11e4309c5c53947e9e77c5706f329d71`.
- Archivos de producto autorizados: `HirExpressionMachine.cpp` y el fallback ordinario de
  `HirCallLowering.cpp`.
- Infraestructura autorizada: selector focal en `ProjectOopLoweringTests.cpp`.
- Contratos congelados: resolución, devirtualización, null-flow, orden receiver/argumentos,
  conversiones, parameter ordinals, packs, defaults, borrow/cleanup, ref-return y diagnósticos.
- Protegido: parser, MIR, LLVM, ABI, NSMETA, hojas H02S14A-K y tracking global.
- Validación única: target `nsc_project_oop_lowering_tests`, selector
  `hir-ordinary-call-depth`.
- Prohibido: CTest, suites, minibaseline, baseline, instalación, deploy y pruebas adicionales.

## Causa y cambio

El fallback final de `ProjectOopHirModel::lowerCallExpression` bajaba recursivamente el receiver
mediante `lowerReceiver` y cada argumento mediante `lowerExpression`. Una cadena válida que alterna
`value.Next()` con `Identity(value)` hacía crecer la pila nativa por cada nivel.

`HirExpressionMachine.cpp` incorpora un único frame `OrdinaryCall` que:

1. congela binding, target y devirtualización con la guarda histórica `!forwardedReceiver`;
2. construye el encabezado y flags null/flow una sola vez;
3. agenda receiver y argumentos uno por uno sobre el scheduler común;
4. conserva el orden receiver, argumentos, defaults, empty pack y validación tardía de ref-return;
5. guarda el índice del pack expandido, no un puntero susceptible a invalidación;
6. finaliza conversiones, borrow, cleanup y PointerLoad sin volver a bajar syntax.

El fallback de `HirCallLowering.cpp` conserva únicamente las llamadas hoja sin receiver explícito ni
argumentos. Las formas que requieren descenso deben haber sido clasificadas por el scheduler y se
rechazan si escapan; por tanto esa ruta ya no contiene una llamada recursiva a `lowerExpression` ni
`lowerReceiver`.

## Prueba focal

El selector `hir-ordinary-call-depth` construye 32,768 expresiones válidas de tipo `C`, alternando:

- llamada instance `Next()` cuyo receiver es el resultado anterior;
- llamada static `Identity(C)` cuyo argumento es el resultado anterior.

La expectativa independiente recorre el HIR iterativamente y comprueba kind, member ordinal,
posición del receiver/argumento, `argumentParameterOrdinals`, orden y literal terminal. El árbol se
destruye con el helper iterativo existente.

## Evidencia ejecutada

- Compilación exclusiva: `nsc_project_oop_lowering_tests`.
- Log: `A:\NSC-devel-paralelo\logs\agente-1\h02s14l-build-final.log`.
- Ejecución exclusiva: `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-ordinary-call-depth`.
- Resultado: `PASS: 2 checks`.
- Log: `A:\NSC-devel-paralelo\logs\agente-1\h02s14l-focus.log`.
- No se ejecutó CTest, suite, minibaseline, baseline ni otro selector.

## Auditoría de no reentrada

La ruta ejercitada es:

`lowerExpression` -> `iterativeKind(OrdinaryCall)` -> `initializeOrdinaryCallFrame` ->
`nextOrdinaryCallChild` -> scheduler común -> `acceptOrdinaryCallChild` ->
`finishOrdinaryCall`.

Ninguno de los cuatro helpers de `OrdinaryCall` llama a `lowerExpression`, `lowerReceiver` ni al
fallback `lowerCallExpression`. El finalizador sólo materializa defaults ya limitados, empty pack,
addressability, cleanup y ref-return. La profundidad de la entrada afecta el vector de frames del
heap, no la pila nativa.

## Límite de la evidencia

La evidencia certifica únicamente el target y selector focal indicados. La batería completa queda
reservada para la GUI del usuario al cierre de la tanda.
