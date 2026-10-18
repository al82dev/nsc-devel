# H02S13M — assignments null-conditional field/core-array HIR iterativos

## SCOPE LOCK

- Repositorio: clon local `A:\NSC-devel-paralelo\development-agente-1-hir`.
- Rama: `codex/development-agente-1-hir`.
- Base de la hoja: H02S13L en `abdcc75372fb786991f79ae20a05bf1400fd5d91`.
- Cambio: migrar únicamente los stores null-conditional de field y core array a la máquina HIR.
- Preservado: sintaxis, API, ABI, NSMETA, diagnósticos, orden receiver→index→RHS, operación,
  ownership, volatile, element type, forma de hijos y contrato lazy del backend.
- Protegido: H02S13N y posteriores, setters de H02S13K/L, Parser, MIR, LLVM, tracking y repositorio
  integrador.
- Validación autorizada: compilar sólo `nsc_project_oop_lowering_tests` y ejecutar sólo el selector
  `hir-property-core-array-assignment-depth`.
- Prohibido: otras pruebas, CTest, suite, baseline, deploy, instalación y red.

## Causa

Las ramas históricas `NullConditionalFieldStore` y `NullConditionalArrayStore` permanecían en
`lowerExpressionImpl`. Field reentraba por receiver y RHS; array reentraba por receiver, primer
índice físico y RHS. Una cadena profunda por cualquiera de esas aristas conservaba profundidad
proporcional en el stack nativo antes de alcanzar el lowering lazy de LLVM.

## Diseño implementado

`HirExpressionMachine` clasifica únicamente assignments con binding field único válido o core
`NullConditionalArrayElement` válido. Los estados inválidos conservan la ruta histórica y sus
diagnósticos. Cada frame baja los operandos mediante la máquina común: field conserva receiver→RHS;
array conserva receiver→índice→RHS y elige `children().front()` o el fallback `second()` exactamente
como antes.

La publicación mantiene `NullConditionalFieldStore` o `NullConditionalArrayStore`, type key del
assignment, token compound de field, ownership, ordinal/volatile, element type y orden exacto de
hijos. No agrega rechazo de address context, conversiones, borrows ni setters sintéticos. LLVM no
se modifica; la forma de hijos que permite branch lazy permanece idéntica.

## Prueba puntual e infraestructura

El target existente `nsc_project_oop_lowering_tests` incorpora un único selector:

```text
NSC_PROJECT_OOP_LOWERING_FOCUS=hir-property-core-array-assignment-depth
```

El foco crea una cadena mixta de 32,768 stores cuya arista profunda rota entre receiver, índice y
RHS. Verifica shape, orden, operation, ordinal volatile, ownership, element type, selección física
del índice, address context histórico y ambos diagnósticos inválidos. No se crea target, CTest,
manifiesto, catálogo ni entrada de Builder nuevos: selector y ruta default pertenecen al ejecutable
unitario existente.

## Evidencia

- Build exacto: `nsc_project_oop_lowering_tests`, PASS.
- Selector puntual: `hir-property-core-array-assignment-depth`, PASS 6/6, exit 0.
- Profundidad: 32,768 stores mixtos con arista profunda rotada entre receiver, índice y RHS.
- Logs: `A:\NSC-devel-paralelo\logs\agente-1\H02S13M-build.log` y
  `A:\NSC-devel-paralelo\logs\agente-1\H02S13M-focus.log`.
- Otras pruebas ejecutadas: ninguna.
- Auditoría estática: las rutas válidas no llaman `lowerExpression`; los estados inválidos siguen
  llegando a las ramas históricas y LLVM permanece intacto.
