# Tanda 9: ruta O0 realmente rápida

## Objetivo

Evitar ejecutar el pipeline `default<O0>` de LLVM cuando una unidad ya está verificada y no tiene ninguna transformación obligatoria pendiente.

## Cambios realizados

- El optimizador preparado recibe si existe una obligación de `forceinline`.
- Una unidad O0 sin inlining obligatorio ni funciones con personality SEH pasa directamente a emisión nativa.
- Las unidades con `forceinline` conservan el pipeline O0 completo y su `alwaysinline` obligatorio.
- Las unidades con `try`/`catch`/`finally` conservan exclusivamente la legalización `mem2reg` necesaria para el workaround Win64 SEH B-001.
- La ruta directa no crea opciones ni un pass manager de LLVM y no repite la verificación completa de un módulo que el lowerer ya verificó.
- Se mantiene la ruta pública defensiva del optimizador sin cambios de comportamiento.

## Archivos modificados

- `include/nativesharp/LLVM/LlvmOptimizer.h`
- `src/LLVM/LlvmOptimizer.cpp`
- `src/LLVM/ProjectOopObjectGenerator.cpp`

## Archivos nuevos

- Este informe. No se añadió otro optimizador: la decisión se incorporó al límite preparado que ya distingue el pipeline interno del API público.

## Mejora esperada

La compilación O0 común elimina por completo la construcción y ejecución del pass manager de optimización por unidad. Solo paga pases cuando existe una razón de corrección explícita: `forceinline` o funclets SEH.

## Verificación de la tanda

- Construcción de `nsc` en configuración Debug/O0: correcta.
- Prueba dirigida `nsc.language.forceinline-modifier`: correcta y limitada a O0.
- Emisión particionada de Stage 46 con `-O0 -j 4`: correcta.
- Emisión y ejecución de Stage 49 con excepciones y `-O0 -j 4`: correctas; proceso terminado con código 0.
- No se ejecutaron rutas O1/O2/O3 ni baseline completo.
