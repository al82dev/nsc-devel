# Tanda NSC 3.0.5 M02R — publicación iterativa de locales del frame `$moveNext`

Fecha: 2026-10-18
Rama: `development`

## SCOPE LOCK

M02R migra exclusivamente el DFS de `appendMoveNextFrameLocals`. Es un refactor interno sin
cambio funcional: conserva preorden, liveness respecto de `yield`, retención por cleanup,
consulta de layout, alineación, offsets, repetición de ordinales, contenedores recibidos y slots
nulos.

No modifica la recolección de liveness, parámetros vivos, layouts, tamaño final alineado del
frame, herencia del frame hacia `$drop`, MIR producido, sintaxis, diagnósticos, API pública, ABI,
NSMETA, LLVM ni caché. Quedan protegidos R02B5C y
`tests/unit/ProjectMethodBodyModelTests.cpp`. No se ejecutará baseline ni batería completa.

## Call graph y contrato observable

`lowerFunctions` llama `lowerOneFunction`; para `$moveNext`, `buildMoveNextFrame` calcula primero
las posiciones de definición, último uso y `yield`. Después `appendMoveNextFrameLocals` publica
los locales que sobreviven, antes de añadir parámetros vivos y alinear a ocho el tamaño final.

El contrato histórico exacto es:

- recorrido preorden padre→hijos, de izquierda a derecha;
- nodo o slot nulo se omite;
- sólo participa `LocalDeclaration` con ordinal y `typeKey` no vacío;
- el local sobrevive si cruza algún `yield` o pertenece a una región de cleanup;
- `valueLayoutForTypeKey` permanece como autoridad única de tamaño y alineación;
- un layout ausente omite el slot sin modificar el frame;
- cada slot alinea el offset vigente, conserva el `typeKey` y avanza por su tamaño;
- el vector y el tamaño recibidos no se limpian;
- repeticiones se publican en el mismo orden; esta función no deduplica.

## Algoritmo y complejidad

La implementación usa un worklist LIFO de punteros no propietarios. Inserta los hijos en orden
inverso para conservar el preorden izquierda→derecha del DFS histórico. No cambia el cálculo
de liveness ni la resolución de layout.

La complejidad sigue siendo `O(N·(Y+T))` en el peor caso para `N` nodos, `Y` fronteras de
`yield` y `T` tipos nominales. La pila nativa `O(H)` se sustituye por un worklist en heap, `O(1)`
adicional para una cadena y hasta `O(N)` para un árbol ancho.

## Contrafactual profundo

El focus `mir-move-next-frame-local-depth` construye una cadena de 32,768 declaraciones y un
slot nulo por nivel. Intercala locales byte/long vivos a través de una frontera `yield`, un local
int conservado sólo por cleanup, locales no vivos, tipo vacío, tipo sin layout y declaración sin
ordinal. El root agrega al final un hermano `builtin:long` que repite el ordinal cero para probar
el orden entre la cadena profunda y un hermano lateral sin dedup.

El frame comienza en byte 3 y con un slot centinela preexistente. La prueba exige 12,290 slots:
el centinela, 12,288 locales válidos de la cadena y el hermano lateral. Comprueba offsets 3, 4 y
8 para los primeros locales; 65,528 para el último long de la cadena; 65,536 para el hermano;
y tamaño final 65,544. La destrucción del árbol es iterativa para no confundir este gate con la
recursión de ownership reservada para otra hoja.

## Infraestructura y validación

La prueba reutiliza `nsc_project_oop_lowering_tests`, `nsc.project-oop-lowering`, su selector por
variable de entorno y la ruta default. No agrega fuente, target, `add_test`, catálogo, manifiesto
ni caso Builder. `MirFunctionLowerer.cpp` ya está registrado exactamente una vez en CMake raíz y
una vez en compiler-suite.

Los gates se ejecutaron con `TEMP`/`TMP` en `A:\temp`. El proceso reconstruyó el `PATH` desde
sus valores nativos Machine y User; `where nslink.exe` resolvió exactamente
`A:\nslink-devel\bin\release\nslink.exe`, pero los consumidores lo invocaron exclusivamente
por el nombre `nslink.exe`.

- build `nsc` en el árbol productivo y en el seleccionado por el runner: PASS; contratos de host
  runtime estático: PASS;
- build `nsc_project_oop_lowering_tests` y `nsc_component_tests`: PASS;
- focus `mir-move-next-frame-local-depth`: PASS 2/2;
- CTest `nsc.project-oop-lowering`: PASS 1/1;
- TL-LANG-381…387: PASS individual, salida 0 en los siete casos;
- `validate_source_registration.py`: PASS, salida 0.

La evidencia quedó en:

- `A:\temp\m02r-build-nsc.log`;
- `A:\temp\m02r-build-selected-nsc.log`;
- `A:\temp\m02r-build-lowering.log`;
- `A:\temp\m02r-build-component-tests.log`;
- `A:\temp\m02r-focus-move-next-frame-local-depth.log`;
- `A:\temp\m02r-ctest-project-oop-lowering.log`;
- `A:\temp\m02r-tl-lang-381.log` … `A:\temp\m02r-tl-lang-387.log`;
- `A:\temp\m02r-source-registration.log`.

Estos resultados verifican exclusivamente la equivalencia de la publicación de locales del
frame `$moveNext` y sus consumidores focales. No se ejecutó baseline ni batería completa.
