# NSC 3.0.5 - H02S14H: span/string core call HIR iterativo

## SCOPE LOCK

- Objetivo exacto: eliminar la reentrada de pila nativa al bajar StringView, los cuatro Span y
  los operandos de las 85 operaciones de la tabla historica compartida.
- Base: `a4a11a4e0067dc93b9a36d382b8963ce5995f9c8`, con H02S14A-G presentes.
- Operaciones incluidas: `StringViewFromString`, `SpanFromArray`, `ReadOnlySpanFromArray`,
  `SpanFromFixedArray`, `ReadOnlySpanFromFixedArray` y los 85 mappings de
  `hirCoreOperandCallPlan`.
- Frontera protegida: atomics/fences H02S14I, monitor H02S14J, console/enum/struct H02S14K,
  ordinary calls H02S14L, ABI, NSMETA, parser, MIR, LLVM, driver y tracking global.
- Prueba puntual unica: `hir-span-string-core-call-depth` del target existente
  `nsc_project_oop_lowering_tests`.
- Prohibido: CTest, suites, minibaseline, baseline, deploy, instalacion, `push`, `pull`, `fetch`,
  escrituras fuera de `A:` y herramientas MSYS/MinGW/Cygwin.

## Causa

Los bloques StringView/Span y la tabla historica bajaban receptor y operandos mediante llamadas
directas a `lowerExpression`. Una cadena nominalmente valida de `StringConcat` conservaba una
reentrada nativa por nivel y agotaba la pila antes de producir el HIR. La misma arista seguia
presente en las 85 operaciones compartidas, aunque el nombre historico del helper mencionara solo
String.

## Diseno implementado

1. `HirCoreOperandCallPlan` expone internamente la unica tabla exhaustiva de 85 mappings; la
   maquina y `lowerCallExpression` consultan exactamente la misma fuente de kind y aridad.
2. `IterativeExpressionKind::CoreOperandCall` clasifica StringView, los cuatro Span y cada mapping
   con operandos o con una precondicion invalida que debe diagnosticarse antes de descender.
3. La apertura valida en el orden historico address context, call binding/ordinal, receiver
   reenviado y aridad. No comprueba `resolution` ni inventa validaciones de shape para views.
4. El frame congela el nodo, binding, syntaxis de operandos, contextos value/address e indice
   siguiente. Agenda un solo operando por vez en orden receiver -> children.
5. Array Span baja el receptor como value; fixed-array Span lo baja como address. Los demas
   operandos usan value context.
6. Al reanudar una operacion table-driven, la conversion del mismo indice se aplica una sola vez;
   un vector corto conserva identidad en las posiciones faltantes.
7. El finalizador solo adjunta el hijo ya bajado y devuelve el nodo terminado. No llama a
   `lowerExpression` ni vuelve a resolver tabla o binding.
8. `CpuPause`, `CpuReadTimestamp` y `MutexCreate` conservan el camino directo de hoja: construyen
   el nodo sin descender y por definicion no poseen una arista recursiva. Cualquier mapping con
   operandos que escape del clasificador se rechaza con una guarda interna.

Complejidad: O(N + O), con N llamadas, O operandos totales, frames explicitos O(N + O) y O(1)
pila nativa adicional por arista.

## Contratos preservados

- los dos diagnosticos exactos de addressability para StringView/Span;
- los tres diagnosticos Stage 51 y su orden historico para la tabla compartida;
- receptor exacto `expression->first()->first()` para views;
- orden receiver reenviado -> argumentos y conversion index cero para ese receiver;
- result type, element type y los 90 mappings HIR especializados (5 views + 85 tabla);
- value context de StringView/array Span y address context de fixed-array Span;
- cero metadata de ordinary calls, defaults, packs, borrow, cleanup wrapper o ref-load nuevo;
- cero cambios de layout, lifetime, hooks runtime, ABI, NSMETA, parser, MIR o LLVM.

## Prueba focal

El selector `hir-span-string-core-call-depth` cubre 102 checks:

- cadena valida de 32,768 `StringConcat`, con llamada anidada como primer operando y literal como
  segundo; verificacion y destruccion completamente iterativas;
- forwarded receiver y conversiones por indice, incluida Identity y vector corto;
- tabla explicita e independiente de las 85 expectativas, con kind, aridad y distribucion exacta
  3/43/32/7 para aridades 0/1/2/3;
- StringView y los cuatro Span con orden, metadata y contexto del receptor;
- rechazos addressable exactos de StringView y Span;
- rechazos previos table-driven de address context, call binding ausente, ordinal ausente,
  receiver reenviado nulo y aridad incorrecta, todos con operandos centinela no visitados.

## Evidencia

- Compilacion exclusiva:
  `cmake --build A:\NSC-devel-paralelo\builds\agente-1-s14a --target nsc_project_oop_lowering_tests`.
- Resultado: target enlazado correctamente.
- Ejecucion exclusiva: `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-span-string-core-call-depth`.
- Resultado final: `PASS: 102 checks`, exit 0.
- Log de compilacion: `A:\NSC-devel-paralelo\logs\agente-1\h02s14h-build-1.log`.
- Log focal: `A:\NSC-devel-paralelo\logs\agente-1\h02s14h-focus-1.log`.
- Auditoria estatica: `A:\NSC-devel-paralelo\logs\agente-1\h02s14h-static-audit.log`.

## Auditoria estatica de cierre

- los bloques residuales StringView y Span contienen cero llamadas a `lowerExpression`;
- el bloque table-driven residual contiene cero llamadas a `lowerExpression` y solo admite sus
  tres hojas de aridad cero;
- inicializador, aceptador y finalizador del frame contienen cero llamadas a `lowerExpression`;
- la tabla de producto contiene exactamente 85 mappings y no tiene `default` ni fallback;
- la expectativa focal contiene 85 entradas explicitas y no consulta el helper de producto;
- no se tocaron atomics, monitor, console/enum/struct, ordinary calls, ABI, NSMETA, parser, MIR,
  LLVM, driver ni tracking global;
- no se ejecuto CTest, suite, minibaseline, baseline, deploy o instalacion.
