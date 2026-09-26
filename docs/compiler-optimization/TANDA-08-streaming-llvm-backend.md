# Tanda 8: backend LLVM en flujo por unidad

## Objetivo

Evitar que el compilador conserve simultáneamente todos los módulos LLVM de un proyecto durante las fases separadas de lowering, preparación, optimización y emisión.

## Cambios realizados

- Cada unidad de código ahora recorre de extremo a extremo lowering, configuración del target, pipeline LLVM y emisión de objeto.
- El módulo LLVM y su `LlvmTargetMachine` se destruyen en cuanto termina su objeto.
- Las particiones usan un único grupo de trabajos, en lugar de crear oleadas y pools separados para lowering, optimización y emisión.
- O0 también puede procesar distintas particiones concurrentemente, aunque conserva el indicador histórico de “optimización paralela” solo para niveles optimizados.
- El texto LLVM se retiene únicamente cuando lo necesitan el caché actual o ThinLTO y se combina en orden determinista al terminar.
- Los errores siguen asociados al ordinal estable de su unidad.

## Archivos modificados

- `src/LLVM/ProjectOopObjectGenerator.cpp`

## Archivos nuevos

- Este informe. No se creó una clase adicional porque la vida útil correcta de módulo, target y objeto puede expresarse dentro de la tarea existente de generación.

## Mejora esperada

Disminuye el pico de memoria del backend y se elimina la barrera entre tres fases globales. Una unidad pequeña puede terminar y liberar recursos mientras otras siguen bajando código, lo que mejora utilización de CPU y reduce presión de memoria, especialmente en O0 con `-j` mayor que uno.

## Verificación de la tanda

- Construcción de `nsc` en configuración Debug/O0: correcta.
- Emisión y archivado dirigidos de Stage 46 con `-O0 -j 4`: correctos; 3 unidades de código y 7 miembros de archivo.
- No se ejecutaron rutas O1/O2/O3 ni baseline completo.
