# H02S13J — Suscripciones de evento HIR iterativas

## SCOPE LOCK

- Objetivo: eliminar las reentradas de stack nativo al bajar `event += handler` y
  `event -= handler`.
- Naturaleza: refactor interno, sin cambio funcional.
- Base del carril: `dd12b34c3097064827b2a06662c7f78da8cdc1d7`, que ya contiene H02S13I.
- Archivos autorizados:
  - `src/Frontend/HIR/HirExpressionMachine.cpp`;
  - `tests/unit/ProjectOopLoweringTests.cpp`;
  - este expediente.
- Archivos y contratos protegidos: parser, AST, Sema, bindings públicos, MIR, LLVM, API,
  ABI, NSMETA, sintaxis, diagnósticos y formatos.
- Archivo ajeno protegido: `tests/unit/ProjectMethodBodyModelTests.cpp`.
- Prueba autorizada: compilar sólo `nsc_project_oop_lowering_tests` y ejecutar únicamente
  `hir-event-add-remove-depth`.
- Prohibido: tracking, repositorio canónico, otras hojas HIR, pruebas adicionales, baseline,
  deploy, install, red, `fetch`, `pull`, `push`, MSYS y escrituras fuera de `A:`.
- Terminación: receiver y handler válidos pasan por la máquina iterativa, el selector alcanza
  32,768 niveles, el diferencial es exclusivo y se produce un commit y un `format-patch`.

## Causa

La rama histórica de `AssignmentExpression` reconoce `MethodEventBinding`, pero baja el receiver
mediante `lowerReceiver`/`lowerEventBackingField` y el handler mediante `lowerExpression`. Una
suscripción anidada en cualquiera de esas dos aristas vuelve a entrar a la misma máquina usando
stack nativo.

La causa local es la ausencia de una familia `EventSubscription` en
`IterativeExpressionKind`. Los nodos HIR `EventAdd` y `EventRemove` ya existían; no faltaba una
operación pública ni un contrato de Sema.

## Diseño aplicado

El clasificador admite exclusivamente asignaciones no addressable con binding `Add` o `Remove`
y handler presente. Los estados inválidos y el contexto de dirección permanecen en la ruta
histórica para conservar sus mensajes.

El frame nuevo:

1. conserva el `MethodEventBinding` original;
2. valida el backing antes de bajar operandos;
3. prepara receiver implícito/base sin reentrada;
4. baja receiver explícito por la máquina común y materializa una sola
   `TemporaryAddress` para temporales struct;
5. forma `FieldAddress`/`StaticFieldAddress` antes de programar el handler;
6. baja el handler por la misma máquina;
7. aplica únicamente la primera conversión no identidad;
8. publica `InstanceCall` para accessor custom o `EventAdd`/`EventRemove` para backing.

El orden observable continúa siendo receiver antes de handler. Cada operando se evalúa una vez.
Los custom accessors conservan ordinal, `argumentParameterOrdinals={0}` y
`argumentTypeKeys={delegateTypeKey}`. El backing conserva ordinal, tipo declarado, tipo de
elemento y dirección como primer hijo.

## Prueba puntual

El selector `hir-event-add-remove-depth` compila un fixture semántico real con:

- backing field y accessors custom;
- add y remove;
- evento de clase y evento de struct.

Después construye una sola cadena sintética de 32,768 suscripciones alternando:

- arista profunda por handler;
- arista profunda por receiver explícito;
- receiver implícito;
- backing y custom accessor;
- add y remove.

El oracle verifica preorden receiver→handler, evaluación única, kind HIR, ordinal, type keys,
dirección backing, metadata del accessor y hoja nula final. La ruta default registra el mismo
foco, pero no se ejecuta durante esta hoja.

## Infraestructura

No se crea fuente ni ejecutable nuevo. `ProjectOopLoweringTests.cpp` ya pertenece al target
`nsc_project_oop_lowering_tests`; el selector se conecta al dispatch existente y a su ruta
default. No aplican CMake adicional, CTest nuevo, manifiesto, catálogo ni NSC Builder 3.

## Evidencia

- Build exacto: `nsc_project_oop_lowering_tests` compiló y enlazó sin errores;
  evidencia en
  `A:\NSC-devel-paralelo\logs\agente-1\H02S13J-build-final.log`.
- Selector único: `hir-event-add-remove-depth` terminó `PASS: 7 checks`;
  evidencia en
  `A:\NSC-devel-paralelo\logs\agente-1\H02S13J-focus-final.log`.
- Integración estática: una definición del foco, un selector y dos llamadas totales
  (selector más registro default ya existente); no se agregó fuente ni target.
- Límite: los ocho helpers de prueba nuevos tienen entre 12 y 50 líneas ejecutables
  como cota superior; ninguno excede 60. La máquina productiva no añade otra función.
- Paridad: los casos válidos clasificados ya no alcanzan la llamada histórica
  `lowerEventBackingField`; esa llamada permanece únicamente en el fallback que conserva
  los diagnósticos de contexto addressable o binding inválido.
- `git diff --check`: PASS.
- Commit del carril: este commit; el SHA256 del `format-patch` se registra en el handoff.
