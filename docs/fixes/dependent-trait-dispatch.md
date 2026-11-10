# Restricciones genéricas dependientes: verificación actual

En la base, el ejemplo `69-constraint` rechazaba `where TOp : Reducer<T>` con `unknown type name 'T'`. La decisión autorizada es admitir la relación y comprobarla después de especializar. El commit `6022fe794` implementó el tratamiento de restricciones dependientes; las ampliaciones posteriores de pruebas y sus límites se conservan en `decisiones-pendientes.md`.

Con el compilador aislado actual, `cross-generic-constraint-original-build.cmd` volvió a compilar el proyecto original sin caché y lo ejecutó con salida 0 en O0. Los archivos originales no se modificaron. El programa original no instancia el método, por lo que se comprobaron además implementaciones compatibles e incompatibles: `cross-generic-constraint-probe.log` registra lectura válida con salida 42 en O0/O2 y rechazo de dos tipos que no cumplen `Reducer<int>`.

Se añadió a la regresión existente una llamada a `op.Combine` dentro del método genérico restringido y se registró la prueba en CTest. `dependent-generic-final.cmd` pasó esta regresión y los dos controles unitarios relacionados: el despacho válido devolvió 42 en O0/O2; también pasaron las relaciones anidadas, los diagnósticos de incompatibilidad y los controles de biblioteca binaria ya incluidos. Los registros están en `A:/nsc-fixes-evidencia`.

La compilación fue incremental y el árbol contiene cambios locales provisionales de genéricos y de otros subsistemas. Esta evidencia confirma los ejemplos enumerados en ese árbol; no certifica el commit aislado ni todas las combinaciones de restricciones, alias o bibliotecas. No se ejecutó batería completa.
