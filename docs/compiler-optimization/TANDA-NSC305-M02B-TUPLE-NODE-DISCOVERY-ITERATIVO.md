# Tanda NSC 3.0.5 M02B — descubrimiento iterativo de nodos con claves tuple

Fecha: 2026-10-18
Rama: `development`

## SCOPE LOCK

M02B migra únicamente el lambda `collectNode` de `MirLayoutImporter.cpp`. Conserva el
`collectTypeKey` iterativo cerrado en M02A y no modifica la construcción ni publicación de
layouts, `ProjectOopMirModel::lowerNode`, sintaxis, diagnósticos, ABI, NSMETA, MIR ni LLVM.

## Algoritmo y paridad

La implementación histórica realizaba un DFS recursivo en preorden por cada cuerpo HIR:

1. omitía un nodo nulo;
2. recogía `typeKey`, `elementTypeKey` y `argumentTypeKeys` del nodo actual; y
3. visitaba los hijos de izquierda a derecha.

La implementación nueva mantiene un worklist LIFO de punteros no propietarios. Extrae primero el
nodo actual y empuja sus hijos en orden inverso, de modo que el siguiente nodo extraído continúa
siendo el primer hijo histórico. Los slots nulos se conservan en el worklist y se omiten al salir,
equivalente al retorno inmediato anterior. Los cuerpos HIR y sus `unique_ptr` permanecen vivos
durante todo `importLayouts`, por lo que los punteros del worklist no alteran ownership ni lifetime.

El orden observable queda idéntico: función por función, nodo padre antes que descendientes,
campos de tipo del nodo en su orden histórico y descendientes de izquierda a derecha. La
deduplicación sigue ocurriendo una sola vez mediante el `sort` + `unique` posterior.

## Límite de tamaño

El cuerpo de `collectNode` tiene menos de 20 statements ejecutables, no contiene autorrecursión y
queda debajo del máximo de 60. `ProjectOopMirModel::lowerNode` permanece intacto para M02C.

## Infraestructura y prueba focal

No se crea fuente, target, caso ni ejecutable. `MirLayoutImporter.cpp` conserva sus registros
existentes en el CMake raíz y la compiler-suite. La prueba funcional aplicable sigue siendo
`nsc_ns3_t02_native_tuple_tests` / CTest `nsc.ns3.t02.native-tuples`, porque atraviesa árboles HIR
con tuplas anidadas, genéricas y múltiples aridades hasta importación MIR, enlace y ejecución O0.

## Evidencia de validación

La validación se ejecutó en una ventana coordinada sobre el worktree compilable de R01H. Toda la
ejecución usó `TEMP` y `TMP` en `A:\temp`, el runtime generado por el build en A: y
`A:\nslink-devel\bin\release\nslink.exe`.

- build `nsc`: PASS;
- build `nsc_ns3_t02_native_tuple_tests`: PASS;
- ejecución directa: `PASS: 24 checks`;
- CTest `nsc.ns3.t02.native-tuples`: PASS 1/1;
- `git diff --check`: PASS.

Evidencia persistente:

- `A:\temp\m02b-build-nsc.log`;
- `A:\temp\m02b-build-t02.log`;
- `A:\temp\m02b-direct-t02.log`;
- `A:\temp\m02b-ctest-t02.log`.
