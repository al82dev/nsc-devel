# NSC 3.0.5 - H02S14I: atomic/interlocked call HIR iterativo

## SCOPE LOCK

- Objetivo exacto: eliminar la reentrada de pila nativa en location/receiver y values de las 18
  operaciones atomic/interlocked, preservando las dos fences como hojas de metadata.
- Base: `e5bbb75449b201bbe98dee0bc86dfbf76f79068e`, con H02S14A-H presentes.
- Operaciones incluidas: las familias int32, int64 y pointer de Atomic/Interlocked, mas
  `AtomicThreadFence` y `AtomicCompilerFence` como contrato hoja.
- Frontera protegida: monitor H02S14J, console/enum/struct H02S14K, ordinary calls H02S14L,
  CallAnalyzer, ABI, NSMETA, parser, MIR, LLVM, runtime, driver y tracking global.
- Prueba puntual unica: `hir-atomic-interlocked-call-depth` del target existente
  `nsc_project_oop_lowering_tests`.
- Prohibido: CTest, suites, minibaseline, baseline, deploy, instalacion, `push`, `pull`, `fetch`,
  escrituras fuera de `A:` y herramientas MSYS/MinGW/Cygwin.

## Causa

El bloque atomic/interlocked de `lowerCallExpression` reentraba por la ubicacion explicita o el
receiver wrapper y despues por cada value/comparand. Una cadena valida de 32,768
`AtomicInt32.Exchange` mantenia un receptor hoja por nivel, pero anidaba el Exchange siguiente como
value y consumia una llamada nativa por arista. Las fences no reentraban: su MemoryOrder ya estaba
plegado y solo debia conservarse como metadata.

## Diseno implementado

1. `HirAtomicCallPlan` contiene la unica tabla exhaustiva de 18 mappings: HIR kind y numero de
   values ejecutables. No existe kind por omision.
2. `IterativeExpressionKind::AtomicCoreCall` clasifica exclusivamente esas 18 operaciones.
3. La apertura valida, en orden historico, address context, call binding y forma de ubicacion;
   congela plan, binding, syntax de location, argument offset y modelo explicit/wrapper.
4. La ubicacion siempre se agenda primero en value context. En forma explicita se conserva tal
   cual; en wrapper se envuelve una vez en `FieldAddress` con `elementTypeKey` y ordinal `value_`.
5. Solo al reanudar la ubicacion se valida el count total, incluyendo los syntax MemoryOrder no
   ejecutables. Esto conserva la precedencia historica de errores.
6. El nodo atomic se construye con el kind/result type del plan y empaqueta success/failure order
   como `+1` en los bytes bajo y siguiente, sin bajar ni adjuntar esos syntax.
7. Los values se agendan uno por uno en orden fuente. La conversion usa el `sourceIndex` original,
   incluido el desplazamiento de la location explicita; un vector corto implica identidad.
8. El finalizador solo acepta el hijo ya bajado, avanza la etapa y devuelve el nodo. No llama a
   `lowerExpression` ni relee receiver/location.
9. Las dos fences conservan su bloque hoja dedicado, cero children y cero evaluacion de su child
   MemoryOrder.

Complejidad: O(N + V), frames explicitos O(N), children O(N + V) y O(1) pila nativa adicional por
arista.

## Contratos preservados

- los diagnosticos exactos y el orden location-antes-de-count;
- location explicita directa, `staticCall=true` y argument offset uno;
- wrapper receiver unico y `FieldAddress` sintetico con ordinal congelado;
- mapping homonimo de los 18 HIR kinds y result/element types int32, int64 y pointer;
- aridades 0/1/2 para Load/Increment/Decrement, Store/Add/Exchange y CompareExchange;
- conversiones indexadas por source index, sin convertir de nuevo la location;
- legacy order cero, success `order+1` y failure `(order+1)<<8` solo con success presente;
- MemoryOrder contado para shape pero nunca evaluado ni adjuntado;
- fences `+1`, cero children y diagnosticos historicos;
- cero argument metadata, defaults, packs, borrow, cleanup o ref-load agregado;
- cero cambios en monitor, ABI, NSMETA, parser, MIR, LLVM o runtime.

## Prueba focal

El selector `hir-atomic-interlocked-call-depth` cubre 33 checks:

- cadena valida de 32,768 `AtomicInt32.Exchange`, verificacion y destruccion iterativas;
- tabla explicita e independiente de los 18 mappings, tipos y aridades;
- Interlocked explicito sin `FieldAddress`, con conversion en indice desplazado;
- success order, success/failure compare-exchange y ausencia de children MemoryOrder;
- ambas fences como hojas metadata, mas sus dos diagnosticos exactos;
- address context, call ausente, location explicita invalida, receiver ausente y count incorrecto;
- receiver centinela que falla antes del count y demuestra la precedencia historica.

## Evidencia

- Compilacion exclusiva:
  `cmake --build A:\NSC-devel-paralelo\builds\agente-1-s14a --target nsc_project_oop_lowering_tests`.
- Resultado final: target enlazado correctamente.
- Ejecucion exclusiva: `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-atomic-interlocked-call-depth`.
- Resultado final: `PASS: 33 checks`, exit 0.
- Log de compilacion final:
  `A:\NSC-devel-paralelo\logs\agente-1\h02s14i-build-final.log`.
- Log focal: `A:\NSC-devel-paralelo\logs\agente-1\h02s14i-focus-1.log`.
- Auditoria estatica: `A:\NSC-devel-paralelo\logs\agente-1\h02s14i-static-audit.log`.

## Auditoria estatica de cierre

- la tabla de producto contiene 18 mappings y la expectativa focal 18 entradas independientes;
- el bloque atomic/interlocked residual contiene cero llamadas a `lowerExpression`;
- inicializador, aceptador y finalizador del frame contienen cero llamadas a `lowerExpression`;
- el scheduler atomic solo agenda location y `valueArgumentCount`; nunca agenda MemoryOrder;
- las fences conservan cero llamadas a `lowerExpression` y cero children;
- no se tocaron monitor, CallAnalyzer, ABI, NSMETA, parser, MIR, LLVM, runtime, driver ni tracking;
- no se ejecuto CTest, suite, minibaseline, baseline, deploy o instalacion.
