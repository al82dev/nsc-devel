# H02S13K — setters de property/indexer HIR iterativos

## SCOPE LOCK

- Repositorio: clon local `A:\NSC-devel-paralelo\development-agente-1-hir`.
- Rama: `codex/development-agente-1-hir`.
- Base de la hoja: H02S13J integrado en `3fdf6e6e5bbb0c77b3cf21b79bd4bee6878f8ebd`.
- Cambio: migrar únicamente el lowering HIR de assignments cuyo binding resuelto selecciona
  un setter de property o indexer.
- Preservado: sintaxis, API, ABI, NSMETA, diagnósticos, orden de evaluación, conversiones,
  borrows, defaults, cleanup, dispatch, referencia, reevaluación compound y forma HIR.
- Protegido: las hojas H02S13L y posteriores, Parser, MIR, LLVM y el repositorio integrador.
- Validación autorizada: compilar sólo `nsc_project_oop_lowering_tests` y ejecutar sólo el
  selector `hir-property-indexer-setter-depth`.
- Prohibido: otras pruebas, CTest, suite, baseline, deploy, instalación y red.

## Causa

La rama histórica bajaba cada setter mediante `lowerResolvedCall`. En assignments simples ese
helper reentraba por receiver, índices y RHS. En compound, además reentraba primero por el getter
completo y luego por receiver e índices del setter. Una cadena de RHS anidados conservaba por ello
profundidad proporcional en el stack nativo.

## Diseño implementado

`HirExpressionMachine` reconoce exclusivamente assignments con `MethodCallBinding` resuelto cuyo
miembro es un accessor setter. Un frame conserva binding, target, operandos, etapa compound y valor
combinado. Receiver, getter, índices y RHS descienden por la máquina común; `lowerBoundCall` recibe
receiver y argumentos ya bajados y continúa siendo la única autoridad de conversiones, borrows,
defaults, cleanup, dispatch y resultados por referencia.

La asignación simple conserva receiver → índices → RHS. La compuesta conserva rechazo de address,
operador, getter, RHS, conversión salvo shifts, `Binary`, nuevo receiver/índices del setter y valor
combinado. La llamada final mantiene `returnsAssignedValue=true`. Las formas no clasificadas siguen
la ruta histórica.

## Prueba puntual e infraestructura

La prueba existente `nsc_project_oop_lowering_tests` incorpora un único selector:

```text
NSC_PROJECT_OOP_LOWERING_FOCUS=hir-property-indexer-setter-depth
```

El foco genera una cadena de 32,768 setters simples alternando property e indexer y comprueba
receiver, índices, ordinal, valor y resultado asignado. En el mismo caso puntual conserva oráculos
someros compound de property/indexer y el diagnóstico byte-exacto de address context. No se crea
fuente, target, CTest, manifiesto, catálogo ni entrada de Builder nuevos: selector y ruta default ya
pertenecen al target existente.

## Evidencia

- Build exacto: `nsc_project_oop_lowering_tests`, PASS.
- Selector puntual: `hir-property-indexer-setter-depth`, PASS 6/6, exit 0.
- Cadena profunda: 32,768 assignments alternando property/indexer.
- Logs: `A:\NSC-devel-paralelo\logs\agente-1\H02S13K-build-final.log` y
  `A:\NSC-devel-paralelo\logs\agente-1\H02S13K-focus-final.log`.
- Otras pruebas ejecutadas: ninguna.
