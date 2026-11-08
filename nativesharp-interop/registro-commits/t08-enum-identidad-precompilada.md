# T08: conciliacion de alias de enum C precompilado

El consumidor que usa simultaneamente un encabezado C y una biblioteca NativeSharp precompilada rechazaba `typedef enum VMode ... VMode` como conflicto con un tipo nativo, aun cuando la observacion ya habia conciliado ambas identidades C.

La resolucion de nombres reconoce ahora esa identidad conciliada para enums, con el mismo criterio existente para records: ordinal del proyecto, identidad canonica, nombre, USR compatible y tipo base del enum. Los conflictos no conciliados siguen siendo errores.

La construccion incremental de NSC termino correctamente. El control `pruebas-tanda-08-volatile-campos-c` paso ejecucion directa O0-O3 y el consumidor con encabezado y biblioteca O0; continuo hasta revelar otro fallo independiente: el consumidor solo con metadata no reconoce `VMode` al analizar casts. Por ello el control completo sigue pendiente; no se declara cerrada la tanda 8.
