# NSC 3.0.5 - H02S14D: event invoke HIR iterativo

## SCOPE LOCK

- Objetivo exacto: eliminar la reentrada de pila nativa al bajar el receiver del backing y los
  argumentos de `MethodEventOperationKind::Invoke`.
- Base: `81a5c9bd0ebbeba1051e0b874e113cf0649fea2d`, con H02S14A-C presentes.
- Frontera protegida: event add/remove, delegate invoke H02S14E, core H02S14F-K, ordinary calls
  H02S14L, ABI, NSMETA, parser, MIR, LLVM, driver y tracking global.
- Prueba puntual unica: `hir-event-invoke-depth` del target existente
  `nsc_project_oop_lowering_tests`.
- Prohibido: CTest, suites, minibaseline, baseline, deploy, instalacion, `push`, `pull`, `fetch`,
  escrituras fuera de `A:` y herramientas MSYS/MinGW/Cygwin.

## Causa

La rama EventInvoke de `lowerCallExpression` llamaba a `lowerEventBackingField`; para eventos de
instancia ese helper podia bajar recursivamente un receiver explicito. Despues, cada argumento se
bajaba mediante otra llamada a `lowerExpression`. Una cadena de EventInvoke anidados por argumento
mantenia todo el estado pendiente en la pila nativa.

## Diseno implementado

1. `IterativeExpressionKind::EventInvoke` clasifica exclusivamente `Invoke` cuando existe una
   arista recursiva real: receiver explicito o argumentos.
2. `EventInvokeFrameState` guarda binding, nodo, receiver comun, etapa y siguiente argumento.
3. La inicializacion rechaza address context y backing invalido antes de visitar hijos.
4. `lowerEventBackingField` acepta opcionalmente un receiver ya bajado. El scheduler reutiliza
   `ReceiverFrameState` para implicit/base, struct addressable, struct temporal y class/interface.
5. El frame materializa primero el backing completo; despues consume argumentos uno por uno en
   orden fuente y siempre en value context.
6. Cada conversion existente publica su target type y envuelve una sola vez si no es identidad.
   Una conversion ausente no sintetiza `argumentTypeKeys`.
7. Al finalizar, `argumentCount` se toma de los hijos sintacticos y excluye el backing.
8. La rama historica conserva solamente hojas sin receiver explicito ni argumentos; ya no contiene
   descenso recursivo.

Complejidad: O(N+A), O(N) frames explicitos y O(1) pila nativa adicional por arista.

## Contratos preservados

- diagnosticos exactos de addressability, backing y receiver;
- `StaticFieldValue` o `FieldValue`, tipo y ordinal del backing;
- receiver de instancia como unico child del `FieldValue`, evaluado una sola vez;
- `EventInvoke` con `resultTypeKey`, `delegateTypeKey` y backing ordinal historicos;
- backing en `children[0]` y argumentos desde `children[1]` en orden fuente;
- argumentos siempre en value context;
- conversiones y `argumentTypeKeys` solamente cuando el binding las contiene;
- `argumentCount` igual al numero de argumentos, nunca al numero total de children;
- ausencia historica de defaults, packs, borrow y full-expression cleanup;
- kind final `EventInvoke`, preservando la validacion noheap posterior.

## Prueba focal

El selector `hir-event-invoke-depth` cubre 13 checks:

- proyecto minimo real con eventos instance, static y struct;
- 32,768 EventInvoke anidados por el primer argumento;
- recorrido y destruccion HIR iterativos;
- backing-first, ordinal, delegate type, conversion type y argumentCount;
- evento static sin argumentos;
- receiver implicito de instancia;
- receiver class explicito;
- receiver struct temporal y struct naturalmente addressable;
- conversion no identidad;
- vector de conversiones mas corto que la lista de argumentos;
- rechazo exacto de address context, backing invalido y receiver type vacio.

## Evidencia

- Compilacion exclusiva:
  `cmake --build A:\NSC-devel-paralelo\builds\agente-1-s14a --target nsc_project_oop_lowering_tests -j 16`.
- Resultado: target enlazado correctamente.
- Ejecucion exclusiva: `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-event-invoke-depth`.
- Resultado: `PASS: 13 checks`, exit 0.
- Log de compilacion: `A:\NSC-devel-paralelo\logs\agente-1\h02s14d-build.log`.
- Log focal: `A:\NSC-devel-paralelo\logs\agente-1\h02s14d-focus.log`.
- Auditoria estatica: `A:\NSC-devel-paralelo\logs\agente-1\h02s14d-static-audit.log`.

## Auditoria estatica de cierre

- El bloque EventInvoke residual contiene cero llamadas a `lowerExpression` y cero llamadas a
  `lowerReceiver`.
- `acceptEventInvokeChild` tiene un solo callsite en la maquina comun.
- Receiver ready y receiver descendido convergen en `lowerEventBackingField` con receiver
  prebajado; las ramas son mutuamente excluyentes y no reevaluan sintaxis.
- El frame no usa helpers de borrow, defaults, pack o cleanup.
- No se tocaron add/remove, delegate invoke, core/ordinary calls, ABI, NSMETA, parser, MIR, LLVM,
  driver ni tracking global.
- No se ejecuto CTest, suite, minibaseline, baseline, deploy o instalacion.
