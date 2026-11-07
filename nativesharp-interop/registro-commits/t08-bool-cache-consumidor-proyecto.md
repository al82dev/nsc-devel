# T08: proyecto de consumidor sin referencia directa a C

El control anterior también fue rechazado porque `Consumer.nsproj` declara
`CppLibraryReference` para las bibliotecas CRT. Se prepara un proyecto de
consumidor que sólo declara `LibraryReference` al productor precompilado y usa
el mismo código fuente; no se cambió el compilador ni su protección. La nueva
ruta de enlace y su uso de caché están pendientes de ejecución.
