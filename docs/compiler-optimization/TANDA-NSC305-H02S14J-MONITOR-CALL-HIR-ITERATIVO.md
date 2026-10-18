# Tanda NSC 3.0.5 H02S14J: llamadas Monitor HIR iterativas

## Alcance

H02S14J migra exclusivamente el lowering HIR de las cinco operaciones núcleo de
`NativeSharp.Core.Threading.Monitor`:

- `MonitorEnter`;
- `MonitorExit`;
- `MonitorWait`;
- `MonitorPulse`;
- `MonitorPulseAll`.

No modifica parser, análisis semántico, ABI, NSMETA, MIR, LLVM ni runtime. Tampoco
anticipa las hojas H02S14K/L.

## Causa

Las cinco rutas estaban clasificadas como llamadas núcleo especializadas, pero
`ProjectOopHirModel::lowerCallExpression` todavía llamaba recursivamente a
`lowerExpression`: una vez para el receptor de las cuatro operaciones simples y
dos veces en `MonitorWait` (receptor y timeout). Una cadena sintáctica profunda
podía consumir la pila nativa antes de entregar HIR.

## Diseño aplicado

`HirMonitorCallPlan` contiene el mapeo explícito y cerrado de operación núcleo a
nodo HIR y distingue `Wait`. `MonitorCoreCallFrameState` mantiene la continuación
en la máquina iterativa con tres estados:

1. `Receiver`: baja exactamente una vez el receptor en contexto de valor.
2. `Timeout`: sólo para `Wait`, baja después el argumento explícito.
3. `Ready`: entrega el nodo terminado sin reentrar a `lowerExpression`.

Las cuatro operaciones simples conservan un hijo `FieldAddress`, su tipo de
elemento y el ordinal implícito. `Wait` conserva directamente los hijos
`[receiver, timeout]`, `memberOrdinal=cv_` y
`secondaryMemberOrdinal=lock_`. La conversión del timeout continúa leyendo
exclusivamente `selectedConversions[0]`; ausencia e identidad no crean wrapper.

Todas las validaciones históricas se ejecutan antes de programar el primer hijo,
en su mismo orden y con sus diagnósticos exactos. La ruta residual de
`lowerCallExpression` es ahora un guard: ya no contiene ninguna llamada recursiva
para Monitor.

## Prueba focal

El selector `hir-monitor-call-depth` incluye:

- contrafactual estructural de 32,768 niveles alternando las cinco operaciones;
- forma nominal individual de las cinco operaciones;
- orden receptor antes de timeout;
- `FieldAddress`, tipos y ordinales de las cuatro operaciones simples;
- ambos ordinales y ambos hijos de `Wait`;
- conversión de timeout ausente, identidad y no identidad;
- precedencia y texto exacto de las validaciones, con receptores centinela que
  fallarían si se visitaran antes de tiempo;
- destrucción iterativa del producto profundo.

El contrafactual profundo es deliberadamente estructural; no afirma que la
alternancia de tipos represente un programa NativeSharp nominalmente válido. Su
propósito es probar que la arista HIR ejercitada no reentra por la pila del
proceso.

## Validación permitida

- Target único: `nsc_project_oop_lowering_tests`.
- Selector único: `hir-monitor-call-depth`.
- No se ejecutan CTest, suites, minibaseline ni baseline.

## Resultado

- `nsc_project_oop_lowering_tests`: compiló y enlazó correctamente.
- `hir-monitor-call-depth`: `PASS: 20 checks`, código de salida 0.
- La auditoría estática confirmó que el diff no agrega llamadas a
  `lowerExpression` y que el guard residual Monitor ya no contiene lowering de
  hijos.
- Evidencia: `A:\NSC-devel-paralelo\logs\agente-1\h02s14j-build-final.log`,
  `A:\NSC-devel-paralelo\logs\agente-1\h02s14j-focus-final.log` y
  `A:\NSC-devel-paralelo\logs\agente-1\h02s14j-static-audit.log`.
