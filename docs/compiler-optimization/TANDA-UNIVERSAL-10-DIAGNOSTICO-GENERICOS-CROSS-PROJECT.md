# Tanda universal 10: diagnostico de genericos cross-project

## Scope lock

- Conservar el diagnostico real cuando una fixture Stage 52 falla antes de MIR.
- No cambiar parser, resolucion, genericos, ownership ni expectativas de la prueba.
- No ejecutar baseline, instalar, modificar NSLink ni hacer push.

## Causa confirmada

`compileAndEmitObject` reportaba solamente el parametro `error` de
`compileProjectOopMir`. Los errores semanticos viven en `DiagnosticEngine`, por lo que una falla
real aparecia como `lowers to OOP MIR` sin archivo, rango ni mensaje. La evidencia quedaba vacia y
obligaba a adivinar la fase responsable.

## Cambio

El helper concatena los diagnosticos emitidos por `ProjectCompilation` y los incluye en el fallo
de MIR. No altera el resultado booleano, el proyecto, el orden de fases ni el LLVM generado.

## Validacion prevista

- Compilar solamente `nsc_stage52_generic_collections_compiler_fixes_tests`.
- Ejecutar solamente `nsc.stage52.generic-collections-compiler-fixes`.
- Usar el nuevo diagnostico para corregir la causa del caso cross-project en un commit posterior.
