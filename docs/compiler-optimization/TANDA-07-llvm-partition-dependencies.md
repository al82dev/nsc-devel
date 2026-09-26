# Tanda 7: dependencias reales por partición LLVM

## Objetivo

Impedir que cada partición de código reconstruya declaraciones LLVM para todas las funciones del programa, aunque solo use una fracción de ellas.

## Cambios realizados

- Cada unidad de código conserva una lista ordenada de ordinales de funciones que necesita declarar.
- El particionador indexa funciones por nombre mangled y por miembro semántico.
- Se recorren cuerpos MIR y acciones de limpieza para registrar llamadas y referencias directas, secundarias y de destrucción.
- Cada lowerer de partición declara únicamente sus definiciones y dependencias directas.
- El módulo coordinador de globales conserva la tabla completa necesaria para dispatch y wrappers.
- El volcado de particiones informa el número de declaraciones de cada unidad.

## Archivos modificados

- `include/nativesharp/LLVM/ProjectOopCodePartition.h`
- `include/nativesharp/LLVM/ProjectOopLlvmLowerer.h`
- `src/LLVM/ProjectOopCodePartition.cpp`
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
- `src/LLVM/ProjectOopObjectGenerator.cpp`

## Archivos nuevos

- Este informe. No se creó otro grafo: las dependencias calculadas forman parte natural de cada `ProjectOopCodeUnit`.

## Mejora esperada

El costo de declarar funciones deja de multiplicar el total de funciones por el número de particiones. También disminuyen el tamaño de cada módulo LLVM, su memoria y el trabajo posterior de configuración, verificación y emisión.

## Verificación de la tanda

- Construcción de `nsc` en configuración Debug/O0: correcta.
- Emisión dirigida O0 del ejemplo Stage 30: correcta.
- Emisión y archivado dirigidos de Stage 46 con `-O0 -j 4`: correctos; se produjeron 3 unidades de código y un archivo con 7 miembros.
- No se ejecutaron rutas O1/O2/O3 ni baseline completo.
