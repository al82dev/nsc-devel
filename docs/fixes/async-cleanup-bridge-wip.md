# Puente async con limpieza: trabajo en curso

La propuesta preparada sobre `acf568799` quedó integrada antes de la
validación, por instrucción del usuario. No se ha compilado ni ejecutado.

El bridge captura la ruta activa de limpieza en llamadas identificadas del
poll. Acepta de una a ocho regiones y limita los locales a las clases con
destrucción por la ruta revisada; delegates, arreglos, strings, value-drop,
regiones desconocidas y limpiezas suspendibles conservan el camino previo.
Los finally admitidos usan una lista cerrada de nodos. El límite acota la
construcción repetida de sufijos. No se ha confirmado que la reproducción
original cumpla esas condiciones.

El código conserva la excepción pendiente en el future mientras limpia y
permite que una excepción compatible la reemplace. Se eligió el trap 20 ya
existente si destruir el owner anterior produce otra excepción. El fallback
para excepciones ajenas conserva la cadena nativa. Estas son intenciones de
la implementación, no resultados verificados.

Pendientes: compilación incremental, verificación LLVM y depuración, pruebas
dirigidas O0/O2 de limpieza, reemplazo, excepción extranjera e incompatible,
identidad y vida del future, orden y destrucción, y consumo entre módulos.
El bridge no cubre en general finally suspendible, salidas estructuradas ni
todas las categorías RAII. El detalle previo de revisión está en
`A:/nsc-fixes-evidencia/async-worker3-bridge-20261112.txt`.

El control preparado `async_poll_replacement_trap.py` compara el reemplazo
normal con una tercera excepción durante la liberación. Espera 148 porque el
runtime Windows construye el trap como 128 más la razón 20; aún debe
confirmarse el runtime enlazado y que el caso alcance esta ruta. El código de
salida, por sí solo, no identifica el punto exacto del trap. No se ha ejecutado.

El runner recibe cuatro rutas absolutas en A:, en este orden: compilador,
directorio de evidencia nuevo, biblioteca runtime y carpeta de imports. Fija
explícitamente el runtime y los imports para el proceso compilador, conserva
sus SHA-256 y los del compilador antes y después, y guarda comandos y salidas
por caso. Los temporales y directorios de aplicación del proceso hijo quedan
en la carpeta de evidencia. La selección explícita no prueba por sí sola que
el binario tenga exactamente el comportamiento del código fuente de runtime;
debe comprobarse su procedencia junto con el punto alcanzado del trap.
