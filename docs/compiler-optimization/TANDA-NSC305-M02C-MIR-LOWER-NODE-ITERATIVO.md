# Tanda NSC 3.0.5 M02C — lowering iterativo del árbol HIR a MIR

Fecha: 2026-10-18
Rama: `development`

## SCOPE LOCK

M02C migra únicamente la recursión estructural de `ProjectOopMirModel::MirNodeMachine::lower` y
retira su helper recursivo `lowerChildren`. No modifica los 24 grupos de traducción de kinds, los
campos copiados, diagnósticos, sintaxis, ABI, NSMETA, HIR, forma de MIR ni LLVM.

## Orden e invariantes preservados

La ruta histórica creaba y poblaba el nodo MIR actual, ejecutaba `lowerKind` y después descendía
recursivamente por sus hijos de izquierda a derecha. El resultado de cada hijo se anexaba al padre
sólo después de terminar su subárbol. Un slot HIR nulo producía un slot MIR nulo en la misma
posición. El primer fallo de `lowerKind` abortaba el árbol completo con el diagnóstico ya escrito.

La ruta iterativa mantiene tres worklists paralelos por frame: fuente HIR, resultado MIR propietario
e índice del siguiente hijo. Un frame crea el nodo y ejecuta `copyIdentity`, `copyControlState`,
`copyAttributes` y `lowerKind` antes de apilar cualquier hijo. Cuando termina el último hijo, el
resultado se desapila y se anexa al padre; por tanto conserva tanto el preorden de efectos de
lowering como el postorden de publicación de ownership.

Los `unique_ptr` permanecen dentro del vector de resultados hasta completar cada subárbol. Los
punteros HIR son no propietarios y permanecen válidos durante la llamada. Los tres vectores crecen
y decrecen juntos, de modo que cada índice describe un único frame y no existen referencias que
sobrevivan a una realocación del vector.

La excepción histórica de `AggregateSpread` también conserva su frontera exacta: sólo al completar
un hijo `AggregateElement` sin descendientes se reemplaza `secondaryImmediate` por el offset fuente,
antes de anexarlo al padre. Si un descendiente falla, el resultado parcial se destruye sin publicar,
igual que en la ruta recursiva.

## Límite de tamaño y recursión

`MirNodeMachine::lower` permanece por debajo de 60 statements ejecutables. La declaración y el
cuerpo de `lowerChildren` se eliminan; no queda una llamada `lower` desde el recorrido de hijos ni
otra autorrecursión estructural en esta ruta.

## Infraestructura y prueba focal

No se crea fuente, target, caso ni ejecutable. `MirNodeMachine.cpp` ya está registrado una vez en el
CMake raíz y una vez en la compiler-suite. La prueba focal mínima es
`nsc_ns3_t02_native_tuple_tests` / CTest `nsc.ns3.t02.native-tuples`, complementada por el target
`nsc_project_oop_lowering_tests` y el focus nuevo `mir-node-lowering-depth`. Ese contrafactual
construye 32,768 bloques HIR anidados, comprueba identidad, orden y slot nulo, y valida por separado
la corrección postorden del descriptor `AggregateSpread`.

El focus y su llamada default se añaden al runner ya existente. El target, fuente y `add_test`
continúan registrados exactamente una vez; no aplica crear catálogo, manifiesto ni ruta Builder
adicional porque no existe un caso ni ejecutable nuevo. El friend privado de prueba reutiliza el
tipo `ProjectOopHirModelTestAccess` existente y no altera superficie pública ni ABI.

## Evidencia ejecutada

La validación se ejecutó después de integrar R01H en `2bd7cc812b48ce0d26384d93a8231dfcf228f3db`,
con `TEMP` y `TMP` en `A:\temp`. Las pruebas que invocan el compilador real recibieron además
`NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-devel\build-test-infrastructure\generated\NativeSharp.Runtime.Win32.lib`
y `NSC_LINKER=A:\nslink-devel\bin\release\nslink.exe`.

- Build `nsc`: PASS, 28 pasos. Log: `A:\temp\m02c-build-nsc.log`.
- Build `nsc_project_oop_lowering_tests`: PASS. Log:
  `A:\temp\m02c-build-lowering.log`.
- Focus `mir-node-lowering-depth`: PASS, 3 checks, exit 0, 279 ms. Log:
  `A:\temp\m02c-focus-mir-node-depth.log`.
- CTest `nsc.project-oop-lowering`: PASS, 160 checks, 1/1, 6.23 s. Log:
  `A:\temp\m02c-ctest-project-oop-lowering.log`.
- Build `nsc_ns3_t02_native_tuple_tests`: PASS. Log: `A:\temp\m02c-build-t02.log`.
- Ejecución directa T02: PASS, 24 checks, exit 0. Log: `A:\temp\m02c-direct-t02.log`.
- CTest `nsc.ns3.t02.native-tuples`: PASS, 24 checks, 1/1, 0.46 s. Log:
  `A:\temp\m02c-ctest-t02.log`.

No se ejecutó baseline ni suite completa. El gate dirigido demuestra la eliminación de recursión en
`lowerNode`, la paridad del descriptor `AggregateSpread`, el registro CTest vigente y un consumidor
externo que atraviesa el `nsc.exe` construido en esta revisión.
