# Tanda NSC 3.0.5 M02H — evaluación iterativa de constantes floating

Fecha: 2026-10-18
Rama: `development`

## SCOPE LOCK

M02H migra exclusivamente la recursión estructural de
`MirConstantEvaluator::floatingValue`, distribuida entre Conversion, Unary y Binary. No modifica
string/integer cerrados en M02F/M02G ni el evaluador boolean posterior. Tampoco cambia HIR, MIR,
sintaxis, diagnósticos, ABI, NSMETA, LLVM ni layouts.

## Algoritmo y paridad

La ruta histórica resolvía `FloatingConstant`, convertía `IntegerConstant` respetando signedness,
consultaba static fields y descendía por Conversion o Unary antes de normalizar al tipo destino.
Binary evaluaba izquierda y derecha en ese orden y aplicaba `+`, `-`, `*`, `/` o `fmod`; después
normalizaba a float/double.

La ruta iterativa conserva ese protocolo con frames `(node,state,left-result)` y una pila de
resultados opcionales. Terminales se resuelven en estado cero; estado uno recibe el primer hijo y
Binary lo conserva antes de visitar el segundo. Los helpers reciben valores ya resueltos y mantienen
exactamente rounding, signo, operaciones y `nullopt`. No introduce short-circuit, memoización ni
evaluación de nodos antes rechazados.

## Contrafactual e infraestructura

El focus `mir-floating-constant-depth` parte de `double 1.0` y lo envuelve en 32,768 niveles que
alternan Conversion, Unary `+` y Binary `+ 0.0`. Fuerza los tres estados, el resultado izquierdo y
el orden de operandos, y exige exactamente `1.0`. La destrucción usa el helper iterativo existente.

La prueba se incorpora al target y `add_test` existentes
`nsc_project_oop_lowering_tests` / `nsc.project-oop-lowering`, por selector y ruta default. No se
agrega fuente, target, catálogo, manifiesto ni caso Builder. `MirConstantEvaluator.cpp` permanece
registrado una vez en CMake raíz y una vez en compiler-suite.

## Validación ejecutada

Cada helper modificado, el loop productivo y el contrafactual permanecen por debajo de 60
statements ejecutables; `floatingValue` no conserva ninguna autorrecursión. La validación focal
terminó así:

- build `nsc` y contrato estático del host: PASS;
- build `nsc_project_oop_lowering_tests`: PASS;
- focus `mir-floating-constant-depth`: PASS 1/1 con 32,768 niveles;
- CTest `nsc.project-oop-lowering`: PASS 1/1;
- build y ejecución Stage37: PASS 72/72, incluida la inicialización floating real
  `double 5.0 / 2.0`;
- `validate_source_registration.py`: PASS.

Los logs y temporales quedaron exclusivamente en `A:\temp`. No se ejecutó baseline ni suite
completa.
