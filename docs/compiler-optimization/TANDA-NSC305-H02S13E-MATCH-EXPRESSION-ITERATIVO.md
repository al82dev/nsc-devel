# TANDA NSC 3.0.5 — H02S13E — `MatchExpression` iterativo

## Alcance

Esta hoja elimina la recursión de proceso causada por los enlaces de expresión de
`MatchExpression` dentro de `ProjectOopHirModel::lowerExpressionImpl`. El cambio
se limita al lowering HIR de la expresión completa y conserva como autoridades
existentes a `MethodMatchExpressionBinding`, `lowerStatement` y
`wrapImplicitConversion`.

No forma parte de esta hoja migrar la recursión interna de declaraciones o
statements (`H02S16A`/`H02S16B`), cambiar el binder, alterar la sintaxis, ni
modificar HIR/MIR/ABI/NSMETA o diagnósticos.

## Causa y corrección

La implementación histórica descendía mediante `lowerExpression` desde la
expresión match hacia cinco fronteras estructurales: governing, condición,
guard, declaraciones y valor del arm. Una cadena profunda podía consumir la
pila del proceso antes de producir HIR.

`HirExpressionMachine` reconoce ahora `MatchExpression` como frame explícito.
El frame conserva:

- binding semántico y nodo HIR parcial;
- índice y nodo parcial del arm;
- etapa governing/condition/guard/value;
- profundidad de cleanup activa para rollback transaccional;
- `memberOrdinal` y contexto address/value originales.

La máquina baja governing, condition, guard y value mediante su pila explícita.
Las declaraciones siguen usando `lowerStatement`, que es el contrato histórico
y pertenece a las hojas H02S16 pendientes. El primer error conserva su texto y
su orden observable; cualquier cleanup abierto se revierte antes de devolver.

## Invariantes preservadas

- Orden HIR: governing una vez; por arm condition, guard, declaraciones y valor.
- Guard ausente: `BooleanConstant(true)` en la misma posición histórica.
- `switchTagMemberOrdinal`, `armTags` y `cleanupRegionOrdinals` byte-equivalentes.
- Conversión implícita sólo para resultado por valor y sólo si no es identity.
- Match por referencia: governing y valor se bajan como dirección; el contexto
  por valor agrega exactamente un `PointerLoad` exterior.
- Diagnósticos históricos:
  - `T03-F03 match expression has no semantic binding`;
  - `a value match expression does not produce addressable storage`;
  - `T03-F03 malformed match expression arm`.

## Contrafactual focal

`tests/unit/ProjectOopLoweringTests.cpp` incorpora el focus
`hir-match-expression-depth` y lo mantiene también en la ruta default del target
ya existente `nsc_project_oop_lowering_tests`.

El focus verifica:

1. profundidad 32,768 alternando la arista recursiva entre governing,
   condition, guard y value;
2. orden somero, tags, conversión y cleanup de una declaración RAII;
3. semántica de resultado por referencia y `PointerLoad` único.

No se añadió fuente ni ejecutable de prueba, por lo que no aplica modificar
CMake, CTest, catálogo, manifiesto ni NSC Builder 3. La infraestructura existente
descubre el focus por variable `NSC_PROJECT_OOP_LOWERING_FOCUS`.

## Validación

Validación focal ejecutada el 18 de octubre de 2026, con `TEMP` y `TMP` en
`A:\temp`, herramientas de Windows resueltas por `PATH`, `nslink.exe` invocado
por nombre y runtime hermética generada en A:

- build `nsc`, `nsc_project_oop_lowering_tests` y cinco targets T03: PASS;
  `A:\temp\h02s13e-build-rerun.log`;
- focus `hir-match-expression-depth`: PASS 4/4;
  `A:\temp\h02s13e-focus.log`;
- CTest `nsc.project-oop-lowering`: PASS 1/1, 199 checks internos;
  `A:\temp\h02s13e-ctest-lowering-path.log`;
- CTest T03 exhaustive match, fundamental patterns, pattern guards, tagged
  variants y conditional patterns: PASS 5/5;
  `A:\temp\h02s13e-ctest-t03-runtime-a.log`;
- `validate_source_registration.py`: PASS;
  `A:\temp\h02s13e-source-registration.log`.

La primera invocación del CTest default no recibió el PATH refrescado y dejó
197/199 checks; la repetición canónica resolvió
`A:\nslink-devel\bin\release\nslink.exe` por nombre y pasó 199/199. Del mismo
modo, la primera invocación T03 buscó una runtime instalada fuera de A: y no
alcanzó el compilador; la corrida certificante usa
`A:\nsc-devel\build-test-infrastructure\generated\NativeSharp.Runtime.Win32.lib`
y pasó los cinco gates. Esas dos corridas se conservan sólo como diagnóstico,
no como evidencia de producto.
