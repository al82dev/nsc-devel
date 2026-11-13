# T09: biblioteca C++ repetida y selección de enlazador

## Hallazgo y alternativa

Dos referencias del productor a la misma biblioteca conservan dos entradas en el catálogo precompilado. Esa repetición y su orden forman parte del comportamiento existente; no se eliminaron ni se cambió NSMeta. En la muestra, NSLink informa símbolos duplicados. La selección ya disponible `NSC_LINKER=lld-link.exe`, aplicada al entorno del proceso de compilación, enlaza y ejecuta la muestra conservando ambas entradas. No se cambió NSLink ni se impuso automáticamente otro enlazador.

## Validación

- `validar-referencias.py --repetir-biblioteca`, con `NSC_LINKER=lld-link.exe`: llamadas directas y por callback frente a Clang en estática y DLL O0–O3; 16 consumidores precompilados por ruta con y sin encabezado. Se comprobó el rechazo de un enum incompatible. Evidencia: `A:/temp/nsc-t09-sysv-enum-referencias-r6nj1lsz`.
- La misma prueba sin repetir la biblioteca pasó con `NSC_LINKER=nslink.exe`: `A:/temp/nsc-t09-sysv-enum-referencias-ihqgbkrw`.
- Control independiente `lld-link` con dos entradas idénticas: enlace y ejecución con código 0, `A:/temp/t09-duplicate-lib-control.log`. El fallo de NSLink permanece en `A:/temp/nsc-t09-cpp-variadica-sysv-enum-scoped-precompilado-x55wv9kw`.

## Límites

Esta es una alternativa explícita para las muestras probadas, no una corrección de NSLink ni una garantía para todos los archivos o configuraciones de enlace. La falla con NSLink y bibliotecas repetidas sigue abierta. La prueba SysV observa registros, no `va_arg`. T09-003 y las capacidades generales de enlace permanecen abiertos.
