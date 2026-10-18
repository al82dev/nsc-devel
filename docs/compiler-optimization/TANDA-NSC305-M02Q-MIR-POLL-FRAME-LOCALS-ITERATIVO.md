# Tanda NSC 3.0.5 M02Q — publicación iterativa de locales del frame `$poll`

Fecha: 2026-10-18
Rama: `development`

## SCOPE LOCK

M02Q migra exclusivamente el DFS de `appendPollFrameLocals`. El cambio es un refactor interno
sin cambio funcional: conserva preorden, selección por `liveSet`, consulta de layout, alineación,
offsets, repetición de ordinales, contenedores recibidos y slots nulos.

No modifica el cálculo de liveness, clausura de borrows, validación de fuentes no locales,
parámetros vivos, layouts de tipos, tamaño final alineado del frame, MIR producido, sintaxis,
diagnósticos, API pública, ABI, NSMETA, LLVM ni caché. Quedan protegidos R02B5B y
`tests/unit/ProjectMethodBodyModelTests.cpp`. No se ejecutará baseline ni batería completa.

## Call graph y contrato observable

`lowerFunctions` llama `lowerOneFunction`; para `$poll`, `buildPollFrame` suma primero los frames
awaited, calcula liveness, cierra fuentes de borrow y valida referencias no locales. Sólo después
`appendPollFrameLocals` recorre el cuerpo y publica los locales vivos antes de los parámetros.

El contrato histórico exacto es:

- recorrido preorden padre→hijos, de izquierda a derecha;
- nodo o slot nulo se omite;
- sólo participa `LocalDeclaration` con ordinal, `typeKey` no vacío y ordinal en `liveSet`;
- `valueLayoutForTypeKey` permanece como autoridad única de tamaño y alineación;
- un layout ausente omite el slot sin modificar el frame;
- cada slot alinea el offset vigente, conserva el `typeKey` y avanza por su tamaño;
- el vector y el tamaño recibidos no se limpian;
- repeticiones se publican en el mismo orden; esta función no deduplica.

## Algoritmo y complejidad

La implementación usa un worklist LIFO de punteros no propietarios. Inserta los hijos en orden
inverso para que la extracción conserve el preorden izquierda→derecha del DFS histórico.

La complejidad sigue siendo `O(N·T)` en el peor caso para `N` nodos y `T` tipos nominales, pues
esta hoja no cambia la búsqueda canónica de layouts. La pila nativa `O(H)` se sustituye por un
worklist en heap, `O(1)` adicional para una cadena y hasta `O(N)` para un árbol ancho.

## Contrafactual profundo

El focus `mir-poll-frame-local-depth` construye una cadena de 32,768 declaraciones con un slot
nulo por nivel. Intercala locales byte/int vivos, un local válido no vivo, tipo vacío, tipo sin
layout y declaración sin ordinal. El root agrega al final un hermano `builtin:long` que repite
el ordinal cero para probar el orden entre la cadena profunda y un hermano lateral sin dedup.

El frame comienza en byte 3 y con un slot centinela preexistente. La prueba exige 10,925 slots:
el centinela, 10,923 locales válidos de la cadena y el hermano lateral. Comprueba offsets 3 y 4
para los primeros locales, 43,688 para el último byte de la cadena, 43,696 para el hermano long
y tamaño final 43,704. La destrucción del árbol es iterativa para no medir la recursión de
ownership reservada para M02V.

## Infraestructura y validación

La prueba reutiliza `nsc_project_oop_lowering_tests`, `nsc.project-oop-lowering`, su selector por
variable de entorno y la ruta default. No agrega fuente, target, catálogo, manifiesto ni caso
Builder. `MirFunctionLowerer.cpp` permanece registrado exactamente una vez en CMake raíz y una
vez en compiler-suite.

Los gates se ejecutaron con `TEMP`/`TMP` en `A:\temp`. El proceso reconstruyó el `PATH` a
partir de sus valores nativos Machine y User; `where nslink.exe` resolvió exactamente
`A:\nslink-devel\bin\release\nslink.exe`, pero los consumidores lo invocaron exclusivamente por
el nombre `nslink.exe`.

- build `nsc`: PASS; contrato de host runtime estático: PASS;
- build `nsc_project_oop_lowering_tests`: PASS;
- build `nsc_component_tests`: PASS;
- focus `mir-poll-frame-local-depth`: PASS 2/2;
- CTest `nsc.project-oop-lowering`: PASS 1/1;
- TL-LANG-370/371/373/374/435: PASS individual, salida 0 en los cinco casos;
- `validate_source_registration.py`: PASS, salida 0.

La evidencia quedó en:

- `A:\temp\m02q-build-nsc.log`;
- `A:\temp\m02q-build-lowering.log`;
- `A:\temp\m02q-build-component-tests.log`;
- `A:\temp\m02q-focus-poll-frame-local-depth.log`;
- `A:\temp\m02q-ctest-project-oop-lowering.log`;
- `A:\temp\m02q-tl-lang-370.log` … `A:\temp\m02q-tl-lang-435.log`;
- `A:\temp\m02q-source-registration.log`.

Estos resultados verifican exclusivamente la equivalencia de la publicación de locales del
frame `$poll` y sus consumidores focales. No se ejecutó baseline ni batería completa.
