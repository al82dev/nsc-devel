# NSC 3.0.5 - H02S14G: iterator core call HIR iterativo

## SCOPE LOCK

- Objetivo exacto: eliminar la reentrada de pila nativa al bajar el receptor de las cinco llamadas
  core del protocolo iterator.
- Base: `a40d844dba2ea4659c8132c55f0540720cef8e1c`, con H02S14A-F presentes.
- Operaciones incluidas: `IteratorMoveNext`, `IteratorGetEnumerator`, `IteratorDrop`,
  `IteratorMoveNextAsync` e `IteratorCancel`.
- Frontera protegida: `IteratorCurrent`, buckets core H02S14H-K, ordinary calls H02S14L, ABI,
  NSMETA, parser, MIR, LLVM, driver y tracking global.
- Prueba puntual unica: `hir-iterator-core-call-depth` del target existente
  `nsc_project_oop_lowering_tests`.
- Prohibido: CTest, suites, minibaseline, baseline, deploy, instalacion, `push`, `pull`, `fetch`,
  escrituras fuera de `A:` y herramientas MSYS/MinGW/Cygwin.

## Causa

El bloque iterator de `lowerCallExpression` bajaba recursivamente el receptor sintactico
`expression->first()->first()`. `IteratorGetEnumerator` permite una cadena enteramente valida en
value context, por lo que la pila nativa crecia con la profundidad. Las cuatro operaciones mutantes
pedian address context y una llamada iterator interior debia fallar por forma de manera controlada,
pero conservaba la misma arista recursiva.

## Diseno implementado

1. `IterativeExpressionKind::IteratorCoreCall` clasifica exclusivamente las cinco operaciones.
2. La apertura congela el binding, valida address context, cero argumentos y la cadena
   `first()->first()` antes de visitar el receptor.
3. El encabezado conserva kind especializado, `resultTypeKey` y `elementTypeKey`.
4. El frame agenda exactamente el receptor sintactico historico. `GetEnumerator` usa value context;
   MoveNext, Drop, MoveNextAsync y Cancel usan address context.
5. La normalizacion posterior del scheduler conserva explicitamente el contexto elegido por el
   frame, sin sustituirlo por el default value context.
6. Al reanudar, se adjunta el receptor una vez y se copia el ultimo exception handler activo para
   cualquiera de las cinco variantes, en el mismo punto logico historico.
7. El bloque residual ya no llama a `lowerExpression`; una fuga del clasificador produce un error
   interno controlado.

Complejidad: O(N), O(N) frames explicitos y O(1) pila nativa adicional por arista.

## Contratos preservados

- diagnostico exacto `<nombre-core>() has an invalid call shape` antes del receptor;
- receptor exacto `expression->first()->first()`, evaluado una sola vez;
- value context exclusivo de GetEnumerator y address context en las cuatro mutantes;
- mapping uno a uno de los cinco HIR kinds especializados;
- `typeKey`, `elementTypeKey` y child unico sin metadata de ordinary call;
- exception target aplicado a las cinco operaciones cuando existe handler y ausente cuando no;
- cero conversiones, borrow, defaults, packs, cleanup wrapper o ref-load sintetico;
- ningun cambio en IteratorCurrent, protocolo/generator layout, ABI, NSMETA, MIR o LLVM.

## Prueba focal

El selector `hir-iterator-core-call-depth` cubre 13 checks:

- lowering exitoso de 32,768 `IteratorGetEnumerator` anidados;
- verificacion iterativa de profundidad, kind, result type, element type, child unico y metadata
  vacia, seguida de destruccion HIR iterativa;
- cinco casos nominales, uno por operacion, con receiver value/address y exception target 77;
- caso sin handler con exception target ausente;
- rechazo exacto de address context con receptor centinela no ligado;
- rechazo exacto de argumento explicito antes de bajar receptor;
- rechazo exacto de `first()==nullptr`;
- rechazo exacto de `first()->first()==nullptr`;
- cadena mutante profunda que termina en el diagnostico address-shape controlado.

## Evidencia

- Compilacion exclusiva:
  `cmake --build A:\NSC-devel-paralelo\builds\agente-1-s14a --target nsc_project_oop_lowering_tests -j 16`.
- Resultado: target enlazado correctamente.
- Ejecucion exclusiva: `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-iterator-core-call-depth`.
- Resultado final: `PASS: 13 checks`, exit 0.
- Log de compilacion: `A:\NSC-devel-paralelo\logs\agente-1\h02s14g-build.log`.
- Log focal: `A:\NSC-devel-paralelo\logs\agente-1\h02s14g-focus.log`.
- Auditoria estatica: `A:\NSC-devel-paralelo\logs\agente-1\h02s14g-static-audit.log`.

## Auditoria estatica de cierre

- El bloque iterator core residual contiene cero llamadas a `lowerExpression`.
- El inicializador y finalizador del frame contienen cero llamadas a `lowerExpression`.
- El frame tiene una inicializacion, un descenso y una rama de reanudacion.
- `IteratorCurrent` no aparece en el clasificador de `IteratorCoreCall`.
- No se tocaron buckets H-K, ordinary calls, ABI, NSMETA, parser, MIR, LLVM, driver ni tracking
  global.
- No se ejecuto CTest, suite, minibaseline, baseline, deploy o instalacion.
