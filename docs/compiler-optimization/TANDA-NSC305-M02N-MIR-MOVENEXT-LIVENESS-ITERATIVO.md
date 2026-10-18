# Tanda NSC 3.0.5 M02N — liveness iterativo de `$moveNext` MIR

Fecha: 2026-10-17
Rama: `development`

## SCOPE LOCK

M02N migra exclusivamente el DFS de `collectMoveNextLiveness`. No modifica la selección ni
publicación de slots, el cálculo de layouts, alineación u offsets, el liveness de `$poll`, la
clausura de borrows, la herencia del frame hacia `$drop` o constructores, ni la producción del
árbol MIR. Tampoco cambia sintaxis, API pública, ABI, NSMETA, diagnósticos, LLVM o caché.

Quedan protegidos `collectBorrowSources`, `collectLivePollLocals`, `appendPollFrameLocals` y
`appendMoveNextFrameLocals`, registrados respectivamente como M02O–M02R. Las rutas auxiliares de
restore de caché, dumper, cost explorer y destrucción propietaria quedan separadas como
M02S–M02V.

## Call graph y contrato observable

`ProjectOopMirModel::lowerFunctions` llama `lowerOneFunction`; cuando el nombre mangled termina
en `$moveNext`, éste llama `buildMoveNextFrame`. El builder ejecuta, en orden:

1. `collectMoveNextLiveness` para numerar y clasificar el árbol;
2. `appendMoveNextFrameLocals` para publicar locales supervivientes;
3. `appendLiveFrameParameters` para publicar parámetros supervivientes;
4. alineación final del frame a ocho bytes.

Después, `inheritMoveNextDropFrames` copia locales y tamaño al `$drop`, e
`inheritConstructorFrames` copia locales, parámetros y tamaño al constructor del generador. Por
ello, cambiar una posición o boundary puede alterar layout, lifetime, cleanup y ABI interno de la
máquina generadora aunque la función collector no emita diagnósticos.

El contrato histórico exacto es:

- recorrido preorden padre→hijos, de izquierda a derecha;
- un slot nulo se omite y no incrementa `order`;
- cada nodo no nulo recibe `here = order++`;
- la primera `LocalDeclaration` de un ordinal fija `localDefinition`;
- cada `LocalValue` o `LocalAddress` sobrescribe `localLastUse`;
- cada `ParameterValue` sobrescribe `parameterLastUse`;
- sólo `YieldValue` se agrega a `yieldOrders`;
- `SuspendPoint`, `AsyncYieldValue` y `AwaitFuture` no son boundaries de esta ruta;
- los contenedores recibidos no se limpian ni reordenan.

## Algoritmo

La implementación iterativa usa un worklist LIFO de punteros no propietarios. Inserta los hijos
en orden inverso para que su extracción conserve el mismo preorden izquierda→derecha del DFS
histórico. Los mapas, el vector de boundaries, las condiciones y el orden de sus mutaciones son
los mismos.

La complejidad temporal esperada permanece `O(N)` para `N` nodos, más el costo esperado de los
maps. La pila nativa `O(H)` del DFS, donde `H` es la altura del árbol, se sustituye por memoria
heap del worklist; en árbol ancho el worklist puede alcanzar `O(N)` y en cadena permanece `O(1)`.

## Contrafactual profundo

El focus `mir-move-next-liveness-depth` construye una cadena de 32,768 nodos que intercala:

- dos declaraciones repetidas del local 7;
- usos por valor y dirección del local 7;
- usos repetidos del parámetro 3;
- `YieldValue` como boundary válido;
- `AsyncYieldValue` y un `SuspendPoint` lateral como contrafactuales no válidos;
- un slot nulo en cada nivel.

La prueba exige `order`, primera definición, últimos usos y 4,096 posiciones `YieldValue`
exactas. El árbol se destruye iterativamente para que la prueba mida el collector y no la
recursión independiente del ownership, reservada para M02V.

## Infraestructura y validación

La prueba reutiliza el target y `add_test` existentes
`nsc_project_oop_lowering_tests` / `nsc.project-oop-lowering`, tanto por selector como por ruta
default. `MirFunctionLowerer.cpp` ya está registrado exactamente una vez en CMake raíz y una vez
en compiler-suite. No se agrega fuente, target, catálogo, manifiesto ni caso Builder.

La validación focal se ejecutó con `TEMP`/`TMP` y evidencia persistente en `A:\temp`:

- build `nsc`: PASS (`m02n-build-nsc.log`);
- build `nsc_project_oop_lowering_tests`: PASS (`m02n-build-lowering.log`);
- focus `mir-move-next-liveness-depth`: PASS 2/2
  (`m02n-focus-move-next-liveness-depth.log`);
- CTest `nsc.project-oop-lowering`: PASS 1/1 en 5.46 s, con
  `NSC_LINKER=nslink.exe` resuelto por el `PATH` nativo
  (`m02n-ctest-project-oop-lowering.log`);
- configuración del component runner: PASS, con el `nsc.exe` actual de
  `build-test-infrastructure` y la definición literal
  `NSC_SUITE_NSLINK_EXECUTABLE=\"nslink.exe\"`
  (`m02n-reconfigure-component-tests-path-name.log`);
- build `nsc_component_tests`: PASS (`m02n-rebuild-component-tests-path-name.log`);
- TL-LANG-381/382/383/384/385/386/387: PASS individual, exit 0, usando el runner
  anterior y `NSC_LINKER=nslink.exe` por `PATH` (`m02n-tl-lang-381.log` a
  `m02n-tl-lang-387.log`);
- `validate_source_registration.py`: PASS, exit 0
  (`m02n-source-registration.log`).

La primera reconstrucción del runner cruzó un estado intermedio de R02B4 entre la retirada de
declaraciones y la retirada de siete definiciones antiguas de `PatternParser`; no fue un fallo
M02N. Se repitió desde el punto coherente congelado y la reconstrucción completa pasó. No se
ejecutaron baseline ni batería completa.
