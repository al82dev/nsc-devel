# Tanda NSC 3.0.5 M02K — validación iterativa de referencias cleanup MIR

Fecha: 2026-10-18
Rama: `development`

## SCOPE LOCK

M02K migra exclusivamente el DFS de `validateCleanupReferences`. No modifica el collector M02J,
el ordenamiento previo de contratos, las tablas de cleanup/excepciones, los diagnósticos ni la
producción del árbol MIR. Tampoco cambia sintaxis, API pública, ABI, NSMETA, LLVM ni layouts.

## Algoritmo y paridad

La ruta histórica recorría nodos en preorden. Por cada nodo validaba primero
`cleanupRegionOrdinals` en su orden mediante `any_of`; después validaba el exception target con
`binary_search`; sólo entonces descendía por hijos de izquierda a derecha. El primer faltante
detenía el recorrido y fijaba el diagnóstico causal. Los slots nulos eran válidos.

La ruta iterativa usa un worklist LIFO de punteros no propietarios e inserta hijos en reversa. Las
búsquedas y el orden cleanup→exception→children permanecen intactos, al igual que los textos y
efectos del primer error. No introduce índices, memoización ni aceptación de referencias nuevas.

## Contrafactual e infraestructura

El focus `mir-cleanup-reference-depth` valida una cadena de 32,768 nodos que referencia una región
cleanup y una región excepcional existentes. Dos árboles someros discriminan el orden de errores:
el primero contiene cleanup faltante, exception faltante e hijo inválido y exige el cleanup del
nodo raíz; el segundo conserva cleanup válido y exige el exception del nodo raíz antes del hijo.
La destrucción es iterativa.

La prueba usa el target y `add_test` existentes
`nsc_project_oop_lowering_tests` / `nsc.project-oop-lowering`, por selector y ruta default. El
acceso focal pasa por el friend privado ya existente; no se agrega fuente, target, catálogo,
manifiesto ni caso Builder. `MirFunctionLowerer.cpp` permanece registrado una vez en CMake raíz y
una vez en compiler-suite.

## Validación ejecutada

- build compartida `nsc` y contrato estático del host: PASS;
- build `nsc_project_oop_lowering_tests`, Item396 e Item397: PASS;
- focus `mir-cleanup-reference-depth`: PASS 3/3 con 32,768 nodos y dos diagnósticos discriminantes;
- CTest `nsc.project-oop-lowering`: PASS 1/1;
- Item396 ownership/cleanup: PASS 6/6;
- Item397 exception flow: PASS 6/6;
- `validate_source_registration.py`: PASS.

El validador no conserva self-calls y cada función nueva o modificada permanece por debajo de 60
statements ejecutables. Los logs y temporales quedaron exclusivamente en `A:\temp`. No se ejecutó
baseline ni suite completa.
