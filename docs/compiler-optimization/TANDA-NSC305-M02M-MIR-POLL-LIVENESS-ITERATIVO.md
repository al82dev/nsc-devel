# Tanda NSC 3.0.5 M02M — liveness iterativo del poll MIR

Fecha: 2026-10-17
Rama: `development`

## SCOPE LOCK

M02M migra exclusivamente el DFS de `collectPollLiveness`. No modifica el cálculo del tamaño de
awaits, la clausura de borrows, la selección ni publicación de slots, el liveness de `$moveNext`,
los layouts ni la producción del árbol MIR. Tampoco cambia sintaxis, API pública, ABI, NSMETA,
diagnósticos ni LLVM.

## Call graph y contratos

`lowerOneFunction` invoca `buildPollFrame` para funciones cuyo nombre termina en `$poll`.
`collectPollLiveness` asigna allí posiciones preorden a todos los nodos no nulos. Sus mapas de
definición/último uso y sus boundaries alimentan la selección de locales vivos, la validación de
borrows no locales y la publicación de parámetros del frame. Cambiar una posición puede alterar
qué valor sobrevive una suspensión o el diagnóstico derivado NS304-T05-F05.

La primera `LocalDeclaration` de un ordinal fija su definición; `LocalValue` y `LocalAddress`
sobrescriben el último uso; `ParameterValue` sobrescribe el último uso del parámetro. Sólo
`SuspendPoint` y `AsyncYieldValue` agregan boundaries. `YieldValue`, `AwaitFuture` y los slots
nulos no agregan boundaries; los nulos tampoco incrementan `order`.

## Algoritmo y contrafactual

La ruta iterativa usa un worklist LIFO de punteros no propietarios e inserta los hijos en orden
inverso. Conserva el preorden izquierda→derecha y no limpia los contenedores suministrados. No
introduce índices, ordenamientos, memoización ni diagnósticos nuevos.

El focus `mir-poll-liveness-depth` construye 32,768 nodos con declaraciones duplicadas, usos por
valor/dirección, parámetros, `SuspendPoint`, `AsyncYieldValue` y un slot nulo por nivel. Un
`YieldValue` lateral discrimina que esa suspensión síncrona no pertenece al poll. La prueba exige
`order`, primera definición, últimos usos y 8,192 boundaries con posiciones exactas, y destruye
el árbol iterativamente.

## Infraestructura y validación ejecutada

La prueba usa el target y `add_test` existentes
`nsc_project_oop_lowering_tests` / `nsc.project-oop-lowering`, por selector y ruta default. El
acceso focal pasa por el friend privado existente; no agrega fuente, target, catálogo, manifiesto
ni caso Builder. `MirFunctionLowerer.cpp` permanece registrado una vez en CMake raíz y una vez en
compiler-suite.

La validación focal sobre el HEAD posterior a R02B2 terminó correctamente:

- build `nsc`: PASS (`A:\temp\m02m-build-nsc.log`);
- build `nsc_project_oop_lowering_tests`: PASS
  (`A:\temp\m02m-build-lowering.log`);
- build `nsc_component_tests`: PASS (`A:\temp\m02m-build-component-tests.log`);
- focus `mir-poll-liveness-depth`: PASS 2/2
  (`A:\temp\m02m-focus-poll-liveness-depth.log`);
- CTest `nsc.project-oop-lowering`: PASS 1/1 con `NSC_LINKER=nslink.exe` resuelto por el `PATH`
  nativo de Windows (`A:\temp\m02m-ctest-project-oop-lowering.log`);
- TL-LANG-370/371/373/374/387/434/435: PASS individualmente con `NSC_LINKER=nslink.exe` por
  nombre (`A:\temp\m02m-tl-lang-370.log`, `A:\temp\m02m-tl-lang-371.log`,
  `A:\temp\m02m-tl-lang-373.log`, `A:\temp\m02m-tl-lang-374.log`,
  `A:\temp\m02m-tl-lang-387.log`, `A:\temp\m02m-tl-lang-434.log` y
  `A:\temp\m02m-tl-lang-435.log`);
- `validate_source_registration.py`: PASS (`A:\temp\m02m-source-registration.log`).

No se ejecutaron baseline ni batería completa. La evidencia certifica exclusivamente el cambio
iterativo de M02M y sus consumidores focales.

La autorrecursión análoga `collectMoveNextLiveness` queda documentada como siguiente hallazgo; no
se convierte todavía en checkbox ni se mezcla con M02M.
