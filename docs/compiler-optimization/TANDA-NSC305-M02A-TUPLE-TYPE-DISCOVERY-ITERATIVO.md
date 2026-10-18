# Tanda NSC 3.0.5 M02A — descubrimiento iterativo de claves tuple

Fecha: 2026-10-18
Rama: `development`

## SCOPE LOCK

M02A migra únicamente el lambda `collectTypeKey` de `MirLayoutImporter.cpp`. No modifica el
recorrido HIR `collectNode` —reservado para M02B—, la construcción de layouts, el orden de
publicación, los diagnósticos, la sintaxis, ABI, NSMETA, MIR ni LLVM.

## Algoritmo y paridad

La implementación histórica realizaba un DFS recursivo en preorden:

1. decodificaba la clave actual mediante `ProjectTypeConversion::tupleElementTypeKeys`;
2. si no era tuple, terminaba esa rama;
3. añadía la clave tuple al catálogo provisional; y
4. visitaba sus elementos de izquierda a derecha.

La implementación nueva usa un worklist LIFO de `std::string` propios. Inserta los elementos en
orden inverso para extraerlos después de izquierda a derecha, por lo que conserva exactamente el
preorden histórico. No añade un conjunto de visitados ni deduplica anticipadamente: repeticiones y
multiconjunto permanecen idénticos hasta el `sort` + `unique` histórico posterior. Las claves no
tuple siguen sin publicarse.

Los `std::string` del worklist son necesarios porque el decoder devuelve elementos propietarios;
se mueven entre el vector decodificado, el worklist y `tupleKeys`, evitando copias adicionales sin
introducir vistas con lifetime ambiguo.

## Límite de tamaño

El cuerpo de `collectTypeKey` tiene 14 líneas físicas útiles y menos de 15 statements ejecutables.
No contiene autorrecursión ni llamada mediante parámetro `self`; queda muy por debajo del máximo de
60. `collectNode` permanece deliberadamente recursivo y visible como la siguiente hoja M02B.

## Infraestructura y prueba focal

No se crea fuente, target, caso ni ejecutable nuevo. `MirLayoutImporter.cpp` ya está registrado una
vez en `nsc_frontend` y una vez en `nsc_component_tests`. La cobertura funcional aplicable existente
es `nsc_ns3_t02_native_tuple_tests` / CTest `nsc.ns3.t02.native-tuples`, que exige identidades tuple,
layouts anidados, aridades 0–16 y 64, genéricos, O0, enlace y ejecución nativa.

## Evidencia de validación

La validación se ejecutó sobre `development` en el HEAD `9d5c5aff0`, después del hotfix causal
que restauró `isTuple` en el clon genérico iterativo. Toda la ejecución usó `TEMP` y `TMP` en
`A:\temp`, el runtime generado por el build en A: y `A:\nslink-devel\bin\release\nslink.exe`.

- build `nsc`: PASS;
- build `nsc_ns3_t02_native_tuple_tests`: PASS;
- ejecución directa: `PASS: 24 checks`;
- CTest `nsc.ns3.t02.native-tuples`: PASS 1/1;
- `git diff --check`: PASS.

Evidencia persistente:

- `A:\temp\m02a-post-tuple-build-nsc.log`;
- `A:\temp\m02a-post-tuple-build-t02.log`;
- `A:\temp\m02a-post-tuple-direct-t02-a-only-nslink.log`;
- `A:\temp\m02a-post-tuple-ctest-t02-a-only-nslink.log`.

La primera ejecución del runner no alcanzó el caso porque no recibió explícitamente el runtime y
NSLink generados en A:. Esos dos intentos de infraestructura quedaron registrados por separado y no
se contabilizan como fallo funcional de M02A. La repetición canónica con ambas rutas A:-only es la
evidencia de cierre.
