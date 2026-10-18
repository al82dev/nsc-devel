# NSC 3.0.5 - H02S14F: panic/assert core HIR iterativo

## SCOPE LOCK

- Objetivo exacto: eliminar la reentrada de pila nativa al bajar el unico operando de `Assert`,
  `Assume` y `Panic`.
- Base: `af5a76f1be26e754562a69838cf29266e815a872`, con H02S14A-E presentes.
- Frontera protegida: iterator core H02S14G, los buckets core H02S14H-K, ordinary calls H02S14L,
  ABI, NSMETA, parser, MIR, LLVM, driver y tracking global.
- Prueba puntual unica: `hir-panic-assert-call-depth` del target existente
  `nsc_project_oop_lowering_tests`.
- Prohibido: CTest, suites, minibaseline, baseline, deploy, instalacion, `push`, `pull`, `fetch`,
  escrituras fuera de `A:` y herramientas MSYS/MinGW/Cygwin.

## Causa

El primer bloque core de `lowerCallExpression` bajaba la condicion de `Assert`/`Assume` y el
payload de `Panic` mediante una llamada directa a `lowerExpression`. Una cadena semantica invalida
pero ya ligada podia agotar la pila nativa antes de llegar al diagnostico contractual de tipo. Las
formas `Unreachable()` y `Panic()` sin payload nunca descendian y permanecen hojas.

## Diseno implementado

1. `IterativeExpressionKind::AssertionCore` clasifica las formas core con operando y tambien las
   formas invalidas con hijos que deben rechazarse antes del descenso.
2. La apertura del frame resuelve el binding una vez, valida address context, aridad y result type,
   y construye el encabezado HIR con el kind y type contractuales.
3. Para `Panic`, la apertura fija `Exception` y copia inmediatamente los cleanup ordinals en orden
   inverso, antes de visitar el payload.
4. El unico hijo se agenda en value context dentro de la worklist comun.
5. La reanudacion exige `builtin:bool` para Assert/Assume y `builtin:string` o `string-literal` para
   Panic. No aplica conversion ni inventa metadata de llamada.
6. El bloque historico conserva solamente las hojas sin operando. Si una forma con operando evade
   el clasificador, falla internamente en vez de reentrar.

Complejidad: O(N), O(N) frames explicitos y O(1) pila nativa adicional por arista.

## Contratos preservados

- diagnostico addressable `<nombre>() has no addressable result` antes de cualquier operando;
- diagnostico exacto de aridad/result type incoherente;
- diagnosticos exactos de bool perdido y payload string perdido;
- Assert y Assume producen `builtin:void`; Unreachable y Panic producen `builtin:never`;
- mapping exacto de los cuatro HIR kinds;
- cero o un child segun la forma, evaluado una sola vez en value context;
- Panic conserva continuation `Exception` y cleanup inner-to-outer congelado antes del descenso;
- ausencia de `argumentCount`, `argumentTypeKeys`, conversiones, defaults, packs, borrow, cleanup
  wrapper y ref load;
- ningun cambio en perfiles panic, reachability, MIR, LLVM, runtime ABI o NSMETA.

## Prueba focal

El selector `hir-panic-assert-call-depth` cubre 11 checks:

- 32,768 Assert/Assume anidados terminan en el diagnostico bool contractual, no stack overflow;
- 32,768 Panic(payload) anidados terminan en el diagnostico string contractual;
- Assert true y Assume true con kind, type y child exactos;
- Unreachable sin children;
- Panic sin payload con snapshot cleanup inverso;
- Panic con `string-literal`;
- Panic con payload `builtin:string` y snapshot `[inner, middle, outer]`;
- rechazo addressable antes de bajar un child deliberadamente invalido;
- rechazo de aridad antes de bajar dos children invalidos;
- rechazo de result type antes de bajar un payload invalido;
- almacenamiento AST/HIR profundo liberado sin recursion de ownership.

## Evidencia

- Compilacion exclusiva:
  `cmake --build A:\NSC-devel-paralelo\builds\agente-1-s14a --target nsc_project_oop_lowering_tests -j 16`.
- Resultado: target enlazado correctamente.
- Ejecucion exclusiva: `NSC_PROJECT_OOP_LOWERING_FOCUS=hir-panic-assert-call-depth`.
- Resultado: `PASS: 11 checks`, exit 0.
- Log de compilacion: `A:\NSC-devel-paralelo\logs\agente-1\h02s14f-build.log`.
- Log focal: `A:\NSC-devel-paralelo\logs\agente-1\h02s14f-focus.log`.
- Auditoria estatica: `A:\NSC-devel-paralelo\logs\agente-1\h02s14f-static-audit.log`.

## Auditoria estatica de cierre

- El bloque Assert/Unreachable/Assume/Panic residual contiene cero llamadas a `lowerExpression`.
- El frame AssertionCore contiene cero llamadas a `lowerExpression`; el scheduler comun realiza el
  unico descenso.
- La inicializacion tiene un callsite y la reanudacion tiene una sola rama.
- El snapshot de cleanup se construye al abrir el frame, antes de programar el payload.
- No se tocaron iterator/core G-K, ordinary calls, ABI, NSMETA, parser, MIR, LLVM, driver ni tracking
  global.
- No se ejecuto CTest, suite, minibaseline, baseline, deploy o instalacion.
