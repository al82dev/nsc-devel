# Tanda NSC 3.0.5 M02L — cálculo iterativo del frame awaited MIR

Fecha: 2026-10-17
Rama: `development`

## SCOPE LOCK

M02L migra exclusivamente el DFS de `sumAwaitedFrameSize`. No modifica la construcción del frame
async, el análisis de liveness, los layouts, los nodos MIR ni la aritmética histórica. Tampoco
cambia sintaxis, API pública, ABI, NSMETA, diagnósticos ni LLVM.

## Algoritmo y paridad

La ruta histórica recorría el árbol en preorden y descendía los hijos de izquierda a derecha. En
cada `AwaitFuture` recorría los layouts en orden, sumaba el `size` del primer `typeKey` coincidente
y detenía esa búsqueda. Un await sin layout aportaba cero; awaits repetidos sumaban repetidamente;
los slots nulos no aportaban nada. La suma permanecía en `std::uint64_t` sin checks adicionales.

La ruta iterativa usa un worklist LIFO de punteros no propietarios e inserta los hijos en orden
inverso. Conserva exactamente el preorden, la primera coincidencia, las repeticiones, los slots
nulos y la aritmética existente. No introduce índices, memoización, saturación ni deduplicación.

## Contrafactual e infraestructura

El focus `mir-awaited-frame-size-depth` construye una cadena de 32,768 nodos alternando
`AwaitFuture` y `Block`. Publica dos layouts con el mismo `typeKey` y tamaños 3 y 999 para exigir
la primera coincidencia, añade un await sin layout y un slot nulo, y espera exactamente 49,152
bytes. La destrucción del árbol es iterativa.

La prueba usa el target y `add_test` existentes
`nsc_project_oop_lowering_tests` / `nsc.project-oop-lowering`, tanto por selector como por ruta
default. El acceso focal pasa por el friend privado existente; no se agrega fuente, target,
catálogo, manifiesto ni caso Builder. `MirFunctionLowerer.cpp` ya está registrado una vez en CMake
raíz y una vez en compiler-suite.

## Validación ejecutada

- build compartida `nsc` y contrato estático del host: PASS;
- build `nsc_project_oop_lowering_tests`: PASS;
- focus `mir-awaited-frame-size-depth`: PASS 1/1 con 32,768 nodos;
- CTest `nsc.project-oop-lowering`: PASS 1/1 con `NSC_LINKER=nslink.exe` resuelto por el `PATH`
  nativo después de INFRA-F01;
- build `nsc_component_tests`: PASS;
- TL-LANG-370 y TL-LANG-387: PASS individualmente;
- `validate_source_registration.py`: PASS con raíz de repositorio y suite explícitas.

Los logs y temporales quedaron exclusivamente en `A:\temp`. No se ejecutó baseline ni suite
completa. La invocación focal, la ruta default y ambos casos reales conservan los contratos
existentes sin cambios de sintaxis, API, ABI, NSMETA, diagnósticos ni formatos.

La siguiente autorrecursión física, `collectPollLiveness`, se documenta como M02M descubierto. No
se convierte todavía en checkbox ni se mezcla con M02L.
