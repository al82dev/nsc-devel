# Tanda NSC 3.0.5 M02J — collector MIR de cleanup iterativo

Fecha: 2026-10-18
Rama: `development`

## SCOPE LOCK

M02J migra exclusivamente el DFS de `collectCleanupContract`. No modifica la producción del árbol
MIR, los tipos de cleanup, la clasificación de nonlocal exits, los diagnósticos, el ordenamiento
posterior ni la validación de referencias. Tampoco cambia sintaxis, API pública, ABI, NSMETA, LLVM
ni layouts.

## Algoritmo y paridad

La ruta histórica visitaba cada nodo en preorden. Antes de descender actualizaba el flujo
excepcional, publicaba el nonlocal exit, agregaba la región cleanup y se detenía en el primer
ordinal duplicado. Después recorría hijos de izquierda a derecha; los slots nulos eran válidos.

La ruta iterativa usa un worklist LIFO de punteros no propietarios. Inserta hijos en reversa para
conservar el preorden y ejecuta las mismas tres operaciones antes del descenso. Los punteros
publicados siguen apuntando a los nodos propietarios originales. El primer error y sus efectos
previos permanecen idénticos.

## Contrafactual e infraestructura

El focus `mir-cleanup-contract-depth` construye 32,768 nodos MIR en una cadena que alterna
`LocalDeclaration`, `Return`, `TryRegion` y `CatchRegion`. Verifica 8,192 regiones cleanup,
nonlocal exits, regiones protegidas y catches, además de orden, punteros y personalidad. Un segundo
árbol comprueba que el primer cleanup duplicado conserva exactamente el diagnóstico histórico.
La destrucción es iterativa.

La prueba usa el target y `add_test` existentes
`nsc_project_oop_lowering_tests` / `nsc.project-oop-lowering`, por selector y ruta default. El
acceso focal pasa por el friend privado ya existente; no se agrega fuente, target, catálogo,
manifiesto ni caso Builder. `MirFunctionLowerer.cpp` permanece registrado una vez en CMake raíz y
una vez en compiler-suite.

La auditoría también demostró que `validateCleanupReferences` conserva un DFS recursivo distinto.
Ese recorrido será M02K después de cerrar M02J; se documenta aquí como dependencia descubierta,
pero todavía no se agrega otra casilla al inventario.

## Validación ejecutada

- build compartida `nsc` y contrato estático del host: PASS;
- build `nsc_project_oop_lowering_tests`: PASS;
- focus `mir-cleanup-contract-depth`: PASS 2/2 con 32,768 nodos y diagnóstico duplicado;
- CTest `nsc.project-oop-lowering`: PASS 1/1;
- Item396 ownership/cleanup: PASS 6/6;
- Item397 exception flow: PASS 6/6;
- `validate_source_registration.py`: PASS.

El collector no conserva self-calls y cada función nueva o modificada permanece por debajo de 60
statements ejecutables. Los logs y temporales quedaron exclusivamente en `A:\temp`. No se ejecutó
baseline ni suite completa.
