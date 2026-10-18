# Tanda NSC 3.0.5 M02G — evaluación iterativa de constantes integer

Fecha: 2026-10-18
Rama: `development`

## SCOPE LOCK

M02G migra exclusivamente la recursión estructural de
`MirConstantEvaluator::integerValue`, distribuida en los helpers de Conversion, Unary y Binary.
No modifica `stringValue` cerrado en M02F ni los evaluadores floating/boolean posteriores. Tampoco
cambia HIR, MIR, sintaxis, diagnósticos, ABI, NSMETA, LLVM ni layouts.

## Algoritmo y paridad

La implementación histórica resolvía literales y static fields inmediatamente. Conversion validaba
que destino y origen fueran integrales antes de descender; Unary evaluaba su único hijo antes de
aplicar `+`, `-` o `~`; Binary evaluaba primero izquierda y después derecha, incluso si la primera
no producía valor, y sólo entonces aplicaba normalización, shifts, división/resto y overflow mínimo.

La ruta iterativa conserva ese protocolo mediante una pila de frames `(node, state, left-result)` y
una pila de resultados opcionales. El estado cero resuelve terminales y valida la frontera de
Conversion. El estado uno recibe el primer hijo; Binary lo guarda y desciende por el segundo. El
estado final llama los mismos helpers aritméticos con los valores ya resueltos. `nullopt`, orden,
normalización y todas las tablas de tipo/bit-width permanecen sin cambio.

## Contrafactual e infraestructura

El focus `mir-integer-constant-depth` parte del literal int `1` y lo envuelve en 32,768 niveles que
alternan Conversion, Unary `+` y Binary `+ 0`. Así fuerza los tres estados productivos, la pila de
resultado izquierdo y el orden de operandos, y exige el valor exacto `1`. La destrucción del árbol
usa el helper iterativo existente.

La prueba se incorpora al target y `add_test` existentes
`nsc_project_oop_lowering_tests` / `nsc.project-oop-lowering`, por selector y ruta default. No se
agrega fuente, target, catálogo, manifiesto ni caso Builder. `MirConstantEvaluator.cpp` permanece
registrado una vez en CMake raíz y una vez en compiler-suite.

## Validación

Cada helper modificado, el loop productivo y el contrafactual permanecen por debajo de 60
statements ejecutables. Pasaron:

- build `nsc`, incluido el contrato de host estático;
- build `nsc_project_oop_lowering_tests`;
- focus `mir-integer-constant-depth`: `PASS: 1 checks`;
- CTest `nsc.project-oop-lowering`: `1/1 PASS`;
- build `nsc_stage37_static_initialization_tests`;
- Stage37 static initialization: `PASS: 72 checks`.

La primera ejecución de `validate_source_registration.py` detectó exclusivamente el módulo R02A
concurrente `host/modules/ParserTypeIterationCounterfactualModule.cpp` aún no registrado. Después
de que R02A cerró su cadena de infraestructura, la repetición global terminó con exit `0`. M02G no
agrega fuentes ni altera CMake.

La evidencia persistente está en:

- `A:\temp\m02g-build-nsc.log`;
- `A:\temp\m02g-build-lowering.log`;
- `A:\temp\m02g-focus-integer-constant-depth.log`;
- `A:\temp\m02g-ctest-project-oop-lowering.log`;
- `A:\temp\m02g-build-stage37.log`;
- `A:\temp\m02g-stage37-static-initialization.log`;
- `A:\temp\m02g-source-registration.log`.
- `A:\temp\m02g-source-registration-r02a-registered.log`.

No se ejecutó baseline ni suite completa.
