# NSC 3.0.5 - H02S14B: receivers delegate/lambda HIR iterativos

## SCOPE LOCK

- Objetivo exacto: eliminar la reentrada de pila nativa al bajar el receiver explicito de
  `DelegateCreate` y reutilizar el protocolo de receiver introducido por H02S14A.
- Base correcta: `23f2ea493abc8d179ae0156800008fdb7a204dfa`.
- Archivos autorizados:
  - `include/nativesharp/Frontend/ProjectOopHirModel.h`;
  - `src/Frontend/HIR/HirExpressionMachine.cpp`;
  - `src/Frontend/HIR/HirFunctionLowerer.cpp`;
  - `tests/unit/ProjectOopLoweringTests.cpp`;
  - este expediente.
- Frontera protegida: delegate/lambda invoke H02S14E, namespace/event/core/ordinary calls
  H02S14C/D/F-L, ABI, NSMETA, parser, MIR, LLVM, driver y tracking global.
- Prueba puntual unica: selector `hir-delegate-lambda-receiver-depth` del target existente
  `nsc_project_oop_lowering_tests`.
- Prohibido: CTest, suites, minibaseline, baseline, deploy, instalacion, `push`, `pull`, `fetch`,
  escrituras fuera de `A:` y herramientas MSYS/MinGW/Cygwin.

## Causa

La creacion de delegate ya construia correctamente el nodo, flags de dispatch, ownership y
movimientos directos. Su unico residuo recursivo era el receiver no estatico que no provenia de
un local/parametro owned:

`lowerExpressionImpl(DelegateCreate) -> lowerReceiver -> lowerExpression(receiverSyntax)`.

`LambdaCreate` no bajaba cuerpos ni capturas como expresiones y no era causa de recursion. Debe
permanecer como hoja terminal valida dentro de una cadena de receivers.

## Diseno

1. Extraer del frame bound-call de H02S14A un `ReceiverFrameState` reutilizable que congele
   sintaxis, tipo, decision de materializacion y resultado HIR.
2. Conservar en un solo helper la decision historica entre receiver implicito, closure capture,
   struct addressable y struct temporal.
3. Clasificar solamente `MethodDelegateOperationKind::Create` con target valido, target no
   estatico, receiver explicito y sin transferencia owned directa.
4. Validar el target antes del descenso y agendar exactamente una evaluacion del receiver.
5. Reanudar, aplicar `TemporaryAddress` una sola vez cuando corresponda y terminar mediante
   `finishDelegateCreation`, que no llama a `lowerReceiver` ni a `lowerExpression`.
6. Mantener las creaciones estaticas, owned-local, owned-parameter y `LambdaCreate` como hojas.

El costo de una cadena de profundidad D es O(D) tiempo, O(D) frames explicitos y O(1) pila nativa
adicional por borde.

## Contratos preservados

- diagnosticos exactos Stage 42 y Stage 43;
- target seleccionado miembro/namespace validado antes de evaluar el receiver;
- `delegateTypeKey`, ordinal, virtual/interface dispatch, target type y ownership;
- target estatico sin child;
- owned local/parametro como un solo `MoveValue` sin reevaluar sintaxis;
- receiver implicito/base y captura de `this` historicos;
- struct naturalmente addressable en contexto de direccion;
- struct temporal envuelto una sola vez en `TemporaryAddress`;
- receiver ordinario evaluado una vez como unico child;
- forma, orden, modo y clear flag de captures en `LambdaCreate`.

## Prueba focal

El selector `hir-delegate-lambda-receiver-depth` entra por `lowerExpression` y por la maquina real.
Cubre:

- 32,768 `DelegateCreate` anidados y una hoja `LambdaCreate` valida;
- recorrido y destruccion iterativos;
- capture ordinal, mode, clear flag, generated name y lambda ordinal;
- receptor struct temporal materializado una vez;
- flags virtual/interface y target type;
- target estatico sin receiver;
- owned-local y owned-parameter como `MoveValue`;
- diagnosticos exactos por target invalido y capture sin source.

## Evidencia

- Compilacion exacta: `cmake --build A:\NSC-devel-paralelo\builds\agente-1-s14a
  --target nsc_project_oop_lowering_tests -j 16`.
- Resultado de compilacion: `PASS`, enlace de `nsc_project_oop_lowering_tests.exe` completado.
- Ejecucion exacta: `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-delegate-lambda-receiver-depth`
  sobre el ejecutable anterior.
- Resultado focal: `PASS: 9 checks`, `EXIT 0`.
- Log de compilacion:
  `A:\NSC-devel-paralelo\logs\agente-1\h02s14b-build-final.log`.
- Log de prueba focal: `A:\NSC-devel-paralelo\logs\agente-1\h02s14b-focus.log`.

## Auditoria estatica de cierre

- La clasificacion valida el target antes de crear el frame y no clasifica `Invoke`.
- El frame conserva un puntero const al binding semantico y un `ReceiverFrameState` propio; no
  revalida el target al reanudar.
- La ruta `DelegateCreationReceiver` agenda un solo hijo, lo acepta mediante el mismo helper de
  H02S14A y llama una vez a `finishDelegateCreation`.
- `finishDelegateCreation` solamente publica metadata y adjunta el receiver pre-bajado; no llama
  a `lowerReceiver`, `lowerExpression` ni a si mismo.
- El bloque `DelegateCreate` de `lowerExpressionImpl` ya no llama a `lowerReceiver`. Sus casos hoja
  usan `MoveValue`, target estatico sin child o `lowerImplicitReceiver`, que no baja expresiones.
- `lowerReceiver` conserva la compatibilidad de familias no migradas y delega su tramo implicito
  no recursivo al mismo `lowerImplicitReceiver`; no se duplicaron diagnosticos Stage 40/43.
- `LambdaCreate` permanece como hoja: no baja cuerpo ni capturas como expresiones.
- No se tocaron ABI, NSMETA, parser, MIR, LLVM, driver, invoke, namespace/event/core/ordinary
  calls ni tracking global.
- No se ejecuto CTest, suite, minibaseline, baseline, deploy o instalacion.
