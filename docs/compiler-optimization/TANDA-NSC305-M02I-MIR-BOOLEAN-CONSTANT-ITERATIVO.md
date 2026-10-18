# Tanda NSC 3.0.5 M02I — evaluación iterativa de constantes booleanas

Fecha: 2026-10-18
Rama: `development`

## SCOPE LOCK

M02I migra exclusivamente las aristas recursivas de
`MirConstantEvaluator::booleanValue`: Conversion, Unary `!` y Binary. No modifica los evaluadores
string, integer o floating cerrados en M02F–M02H, el MIR producido ni la publicación de campos
estáticos. Tampoco cambia sintaxis, diagnósticos, ABI, NSMETA, LLVM ni layouts.

## Algoritmo y paridad

La ruta histórica resolvía literales y campos estáticos; reenviaba Conversion; invertía Unary `!`;
y evaluaba siempre izquierda y derecha de Binary. `&&` y `||` no aplicaban short-circuit. Para
`==` y `!=`, primero intentaba ambos operandos como bool y, si cualquiera fallaba, reevaluaba ambos
mediante el comparador integer o floating seleccionado por el tipo del hijo izquierdo.

La ruta iterativa conserva el protocolo con frames `(node,state,left-result)` y una pila de
resultados opcionales. Binary visita izquierda y derecha antes de delegar al helper; el helper
recibe ambos resultados bool y conserva el fallback numérico histórico, incluida su reevaluación.
No introduce short-circuit, memoización, coerciones ni aceptación de nodos nuevos.

## Contrafactual e infraestructura

El focus `mir-boolean-constant-depth` parte de `true` y construye 32,768 niveles en bloques de
Conversion, Unary `!`, `EqualEqual false` y `AmpAmp true`. Cada bloque regresa a `true` y fuerza
las dos ramas binarias, la evaluación de ambos operandos y el orden izquierda→derecha. La
destrucción usa el helper iterativo existente.

La prueba usa el target y `add_test` existentes
`nsc_project_oop_lowering_tests` / `nsc.project-oop-lowering`, por selector y ruta default. No se
agrega fuente, target, catálogo, manifiesto ni caso Builder. `MirConstantEvaluator.cpp` permanece
registrado una vez en CMake raíz y una vez en compiler-suite.

## Validación ejecutada

- build `nsc` y contrato estático del host: PASS;
- build `nsc_project_oop_lowering_tests`: PASS;
- focus `mir-boolean-constant-depth`: PASS 1/1 con 32,768 niveles;
- CTest `nsc.project-oop-lowering`: PASS 1/1;
- build y ejecución Stage37: PASS 72/72, incluida inicialización booleana real;
- `validate_source_registration.py`: PASS.

`booleanValue` no conserva self-calls y cada función nueva o modificada permanece por debajo de 60
statements ejecutables. Los logs y temporales quedaron exclusivamente en `A:\temp`. No se ejecutó
baseline ni suite completa.
