# Tanda 8: selección automática TLS en emisión directa de DLL

Una DLL NativeSharp que enlaza el productor C con TLS se emitía correctamente por NSLink, pero no tenía directorio TLS: `Read()` devolvió `-2147483648` y el control del hilo devolvió 2. El mismo proyecto con LLD explícito produjo un directorio TLS y obtuvo lectura 3, escritura 41, control de hilo 0 y lectura final 41.

La ruta `--emit-project-oop-dll` usa ahora el mismo detector de entradas estáticas que la emisión de ejecutables, antes de calcular la caché y de enlazar. Se mantienen las selecciones explícitas. La construcción incremental y los controles dirigidos pasaron en O0–O3: directorio TLS PE, consumo mediante Python y un consumidor NativeSharp precompilado. También pasaron los controles de emisión de ejecutables y `nsc build`. Esto no modifica NSLink ni resuelve los cuatro controles distintos de DLL C que siguen abiertos.
