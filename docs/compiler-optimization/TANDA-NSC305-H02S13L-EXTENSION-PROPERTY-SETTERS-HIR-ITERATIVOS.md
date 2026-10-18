# H02S13L — setters de extension property HIR iterativos

## SCOPE LOCK

- Repositorio: clon local `A:\NSC-devel-paralelo\development-agente-1-hir`.
- Rama: `codex/development-agente-1-hir`.
- Base de la hoja: H02S13K en `0c8827b6013111451d6ecc2fd29e5cd40d39287d`.
- Cambio: migrar únicamente el lowering HIR de assignments cuyo binding resuelto selecciona un
  setter sintético de extension property de namespace.
- Preservado: sintaxis, API, ABI, NSMETA, diagnósticos, orden receiver→RHS, conversiones
  posicionales, metadata de argumentos, resultado asignado y fallback histórico.
- Protegido: H02S13M y posteriores, Parser, MIR, LLVM, tracking y repositorio integrador.
- Validación autorizada: compilar sólo `nsc_project_oop_lowering_tests` y ejecutar sólo el selector
  `hir-extension-property-setter-depth`.
- Prohibido: otras pruebas, CTest, suite, baseline, deploy, instalación y red.

## Causa

El getter de extension property ya descendía por la máquina común, pero el setter de namespace
seguía en `lowerExpressionImpl`. La rama bajaba recursivamente primero el receiver reenviado y luego
el RHS. Una cadena de setters anidada por RHS conservaba profundidad proporcional en el stack
nativo aun cuando el resto del assignment ya estaba migrado.

## Diseño implementado

`HirExpressionMachine` clasifica sólo `AssignmentExpression` con RHS y
`NamespaceFunctionCallBinding` resuelto cuyo ordinal existe en el modelo namespace. El frame
conserva binding y ordinal; si hay receiver reenviado, lo baja una sola vez en value context, aplica
su conversión posicional y después programa el RHS. Al regresar el RHS calcula el ordinal físico
del parámetro value, conserva su validación posterior al descenso, aplica la conversión posicional
y publica el `StaticCall`.

La llamada conserva ordinal namespace, nombre mangled, type key del binding, metadata 0/1 y
`returnsAssignedValue=true`. No agrega borrows, defaults, packs ni cleanup inexistentes. Address
context, receiver sintético perdido y parámetro value ausente mantienen sus textos y orden exactos.
Bindings inválidos continúan por el fallback histórico.

## Prueba puntual e infraestructura

El target existente `nsc_project_oop_lowering_tests` incorpora un único selector:

```text
NSC_PROJECT_OOP_LOWERING_FOCUS=hir-extension-property-setter-depth
```

El foco genera una cadena de 32,768 setters por RHS y verifica la forma `StaticCall`, receiver antes
del valor, ordinales y type keys posicionales, nombre mangled y resultado asignado. En el mismo caso
puntual cubre conversiones de receiver/value, variante sin receiver y los tres diagnósticos del
contrato. No se crea target, CTest, manifiesto, catálogo ni entrada de Builder nuevos: el selector y
la ruta default pertenecen al ejecutable unitario existente.

## Evidencia

- Build exacto: `nsc_project_oop_lowering_tests`, PASS.
- Selector puntual: `hir-extension-property-setter-depth`, PASS 8/8, exit 0.
- Profundidad: 32,768 setters sintéticos de namespace anidados por RHS.
- Logs: `A:\NSC-devel-paralelo\logs\agente-1\H02S13L-build.log` y
  `A:\NSC-devel-paralelo\logs\agente-1\H02S13L-focus.log`.
- Otras pruebas ejecutadas: ninguna.
- Auditoría estática: cero reentrada desde la ruta válida; receiver y RHS descienden por la máquina
  común y las formas no clasificadas conservan la implementación histórica.
