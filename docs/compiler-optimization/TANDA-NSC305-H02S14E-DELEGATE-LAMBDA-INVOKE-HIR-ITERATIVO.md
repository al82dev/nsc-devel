# NSC 3.0.5 - H02S14E: delegate/lambda invoke HIR iterativo

## SCOPE LOCK

- Objetivo exacto: eliminar la reentrada de pila nativa al bajar el callee y los argumentos de
  `MethodDelegateOperationKind::Invoke`.
- Base: `5514cc34f577909ae55c979f2f174c510201ad93`, con H02S14A-D presentes.
- Frontera protegida: delegate/lambda creation H02S14B, event invoke H02S14D, core H02S14F-K,
  ordinary calls H02S14L, ABI, NSMETA, parser, MIR, LLVM, driver y tracking global.
- Prueba puntual unica: `hir-delegate-lambda-invoke-depth` del target existente
  `nsc_project_oop_lowering_tests`.
- Prohibido: CTest, suites, minibaseline, baseline, deploy, instalacion, `push`, `pull`, `fetch`,
  escrituras fuera de `A:` y herramientas MSYS/MinGW/Cygwin.

## Causa

La rama DelegateInvoke de `lowerCallExpression` bajaba recursivamente el callee para un delegate
ordinario y para una lambda directa con ambiente. Despues bajaba cada argumento explicito mediante
otra llamada a `lowerExpression`. Una cadena de invocaciones anidadas por callee o argumento
mantenia todos los nodos pendientes en la pila nativa. La lambda directa sin ambiente tenia una
excepcion contractual importante: su expresion callee no se evalua porque no existe celda delegate;
el callable generado se invoca directamente.

## Diseno implementado

1. `IterativeExpressionKind::DelegateInvoke` clasifica exclusivamente `Invoke` cuando existe una
   arista que bajar: callee requerido o argumento explicito.
2. `DelegateInvokeFrameState` conserva binding, nodo parcial, etapa y siguiente argumento.
3. La inicializacion valida address context y binding antes de visitar cualquier hijo y publica una
   sola vez `resultTypeKey`, `delegateTypeKey`, nombre directo y flag de ambiente.
4. Los modos ordinary y direct-environment programan primero el callee. El modo direct-static no lo
   programa, no crea placeholder y comienza directamente en argumentos.
5. Los argumentos se consumen uno por uno en value context y orden fuente. Cada conversion presente
   publica exactamente un target type; solo una conversion no identidad crea wrapper.
6. `finishDelegateInvoke` materializa despues los defaults con `lowerDefaultArgument`, conserva el
   type key vacio cuando el vector semantico es corto y calcula `argumentCount` sin incluir callee.
7. El finalizador no llama a `lowerExpression`; la rama historica queda limitada a la hoja
   direct-static sin argumentos explicitos.

Complejidad: O(N+A+D), O(N) frames explicitos y O(1) pila nativa adicional por arista; N es la
profundidad de invocaciones, A los argumentos explicitos y D los defaults.

## Contratos preservados

- diagnostico exacto `Stage 42 delegate invocation is not addressable`;
- callee evaluado exactamente una vez para delegate ordinario y lambda directa con ambiente;
- callee completamente omitido para lambda directa sin ambiente;
- orden callee, argumentos explicitos y defaults;
- argumentos explicitos siempre en value context;
- conversiones y `argumentTypeKeys` solamente cuando existen entradas semanticas;
- defaults literal, unary y string mediante el helper historico, incluido type key vacio;
- `argumentCount = explicit + defaults`, excluyendo el callee;
- kind final `DelegateInvoke`, result type, delegate type, nombre directo y flag de ambiente;
- ausencia historica de packs, borrow, cleanup y carga por referencia en esta ruta.

## Prueba focal

El selector `hir-delegate-lambda-invoke-depth` cubre 10 checks:

- proyecto minimo real que produce un binding semantico ordinario de Invoke;
- 32,768 invocaciones alternando ordinary, direct-environment y direct-static;
- recorrido y destruccion HIR iterativos;
- politica de callee, metadata, orden y conteos en toda la cadena;
- hojas sin argumentos para los tres modos;
- callee deliberadamente invalido que direct-static debe omitir;
- tres argumentos con conversion identidad, conversion numerica y vector corto;
- defaults integer, unary, string y type key vacio, anexados despues del callee;
- diagnostico exacto de default ausente;
- diagnostico exacto de address context.

## Evidencia

- Compilacion exclusiva:
  `cmake --build A:\NSC-devel-paralelo\builds\agente-1-s14a --target nsc_project_oop_lowering_tests -j 16`.
- Resultado: target enlazado correctamente.
- Ejecucion exclusiva: `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-delegate-lambda-invoke-depth`.
- Resultado: `PASS: 10 checks`, exit 0.
- Log de compilacion: `A:\NSC-devel-paralelo\logs\agente-1\h02s14e-build.log`.
- Log focal: `A:\NSC-devel-paralelo\logs\agente-1\h02s14e-focus.log`.
- Auditoria estatica: `A:\NSC-devel-paralelo\logs\agente-1\h02s14e-static-audit.log`.

## Auditoria estatica de cierre

- `finishDelegateInvoke` contiene cero llamadas a `lowerExpression`.
- La rama DelegateInvoke residual contiene cero llamadas a `lowerExpression` y solo acepta la hoja
  direct-static sin argumentos explicitos.
- `acceptDelegateInvokeChild` tiene un solo callsite en la maquina comun.
- Cada frame resuelve `delegateBinding` una sola vez durante inicializacion; las etapas reutilizan el
  puntero guardado.
- Los tres modos son mutuamente excluyentes y ninguna ruta reevalua callee o argumento.
- No se tocaron creation, event/core/ordinary calls, ABI, NSMETA, parser, MIR, LLVM, driver ni
  tracking global.
- No se ejecuto CTest, suite, minibaseline, baseline, deploy o instalacion.
