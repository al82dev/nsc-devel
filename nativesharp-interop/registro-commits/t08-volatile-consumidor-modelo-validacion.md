# T08-010/015: validación interna del modelo consumidor

Se construyeron incrementalmente `nsc`, `nsc_compilation_model_tests`,
`nsc_project_loader_tests` y `nsc_project_oop_lowering_tests` con build
`b5001fd349ffa761`. La construcción terminó con código 0; compiló 314 pasos
afectados por cambios en encabezados compartidos. Se observaron advertencias
de inicializadores en `CppSemanticRecord.cpp`; el resultado visible también
resumió otras dos advertencias cuyo detalle quedó truncado. No se atribuyen al
cambio sin revisión.

El llamador `A:/nsc-interop-build/validar-volatile-consumidor-modelo.cmd` se
ejecutó mediante `CALL` y terminó con código 0: el modelo pasó 78 controles,
el cargador 96 y el control HIR dirigido 4. Estos resultados incluyen los
valores ausente, Microsoft e ISO, propiedades inválidas y duplicadas, y la
consulta HIR por archivo desde un worker.

La propiedad explícita sigue bloqueada antes de la generación HIR y el acceso
por alias todavía no emite el orden requerido. Este control no es la validación
completa de la tanda 8, que permanece abierta.
